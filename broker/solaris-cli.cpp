#include "BrokerClient.h"

#include <cstdio>
#include <cstring>
#include <strings.h>   // strcasecmp
#include <cctype>     // isdigit
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <map>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <csignal>

static volatile sig_atomic_t gInterrupted = 0;

static void SigIntHandler(int /*sig*/) {
  gInterrupted = 1;
}

static void PrintHelp() {
  printf("Commands:\n");
  printf("  connect [host] [cmd_port] [pub_port]  Connect to broker (default: localhost:5555/5556)\n");
  printf("  disconnect                             Disconnect from broker\n");
  printf("  ping                                   Check broker is alive\n");
  printf("  list                                   List connected digitizers\n");
  printf("  info <digi>                            Show digitizer info\n");
  printf("  open <url>                             Open digitizer (e.g. dig2://192.168.0.254)\n");
  printf("  close <digi>                           Close digitizer\n");
  printf("  read <digi> <param>                    Read parameter\n");
  printf("  param <digi> <name|substr>             Describe / search parameters (range, values, setinrun)\n");
  printf("  write <digi> <param> <value>           Write parameter\n");
  printf("  cmd <digi> <command>                   Send command\n");
  printf("  format <digi> <fmt>                    Set data format (0=ALL,1=OneTrace,2=NoTrace,...)\n");
  printf("  start <digi|all> <fmt> [save <path>]   Start ACQ\n");
  printf("  stop <digi|all>                        Stop ACQ\n");
  printf("  status                                 Show ACQ status\n");
  printf("  file-status <digi>                     Show file sizes\n");
  printf("  save-settings <digi> [file]            Save settings to file\n");
  printf("  load-settings <digi> [file]            Load settings from file\n");
  printf("  hist <digi> <ch|all> [sec] [nbin] [xmax] [long|short]  Energy histogram over N s from the hit stream (ACQ must be ON; defaults 5 s, 200 bins, 5000, long gate)\n");
  printf("  trace <digi> <ch> [samples]             Capture and display a waveform trace (default 100 samples)\n");
  printf("  subscribe [seconds]                    Monitor live rates and file sizes for N seconds (default 10)\n");
  printf("  load <script>                          Execute commands from a script file\n");
  printf("  sleep <ms>                             Sleep for N milliseconds\n");
  printf("  ! <command>                            Run a shell command (e.g. ! mkdir -p /data/run001)\n");
  printf("  shutdown                               Shutdown broker\n");
  printf("  help                                   Show this help\n");
  printf("  quit / exit                            Exit CLI\n");
  printf("\nScript-only flow control (inside .cli files):\n");
  printf("  if read <digi> <param> == <value>       Branch based on parameter value\n");
  printf("  if read <digi> <param> != <value>       Branch if not equal\n");
  printf("  if num_digi == <N>                      Branch based on digitizer count\n");
  printf("  elif ...                                Else-if branch\n");
  printf("  else                                    Else branch\n");
  printf("  endif                                   End if block\n");
  printf("  for <var> in <val1> <val2> ...           Loop with explicit values\n");
  printf("  for <var> in <start>..<end>              Loop over integer range (inclusive)\n");
  printf("  for <var> in <start>..<step>..<end>      Loop with step\n");
  printf("  endfor                                   End for block\n");
  printf("  set <var> num_digi                       Set variable to number of digitizers\n");
  printf("  set <var> read <digi> <param>            Set variable from a parameter read\n");
  printf("  set <var> <A> + <B>                      Set variable to A + B (integer math)\n");
  printf("  set <var> <A> - <B>                      Set variable to A - B\n");
  printf("  # comment                               Lines starting with # are ignored\n");
  printf("  Use $var or ${var} for variable substitution in any command\n");
}

static std::vector<std::string> Tokenize(const std::string& line) {
  std::vector<std::string> tokens;
  std::istringstream iss(line);
  std::string token;
  while (iss >> token) tokens.push_back(token);
  return tokens;
}

//^============================================ Variable substitution

// Replace all $var and ${var} occurrences in a string with values from the map
static std::string SubstituteVars(const std::string& line, const std::map<std::string, std::string>& vars) {
  if (vars.empty()) return line;

  std::string result;
  result.reserve(line.size());

  for (size_t i = 0; i < line.size(); i++) {
    if (line[i] == '$' && i + 1 < line.size()) {
      std::string varName;
      size_t j;

      if (line[i + 1] == '{') {
        // ${varName}
        j = line.find('}', i + 2);
        if (j != std::string::npos) {
          varName = line.substr(i + 2, j - i - 2);
          i = j; // skip past '}'
        } else {
          result += line[i];
          continue;
        }
      } else {
        // $varName -- read until non-alphanumeric/non-underscore
        j = i + 1;
        while (j < line.size() && (isalnum(line[j]) || line[j] == '_')) j++;
        varName = line.substr(i + 1, j - i - 1);
        i = j - 1; // -1 because the for loop increments
      }

      auto it = vars.find(varName);
      if (it != vars.end()) {
        result += it->second;
      } else {
        result += "$" + varName; // leave unresolved
      }
    } else {
      result += line[i];
    }
  }
  return result;
}

//^============================================ Range expansion for "for" loops

// Parse "A..B" or "A..S..B" and return list of integer strings
static std::vector<std::string> ExpandRange(const std::string& token) {
  std::vector<std::string> result;

  // Count ".." separators
  std::vector<size_t> dotPositions;
  size_t pos = 0;
  while ((pos = token.find("..", pos)) != std::string::npos) {
    dotPositions.push_back(pos);
    pos += 2;
  }

  if (dotPositions.size() == 1) {
    // A..B
    int a = atoi(token.substr(0, dotPositions[0]).c_str());
    int b = atoi(token.substr(dotPositions[0] + 2).c_str());
    int step = (a <= b) ? 1 : -1;
    for (int i = a; (step > 0 ? i <= b : i >= b); i += step) {
      result.push_back(std::to_string(i));
    }
  } else if (dotPositions.size() == 2) {
    // A..S..B
    int a = atoi(token.substr(0, dotPositions[0]).c_str());
    int s = atoi(token.substr(dotPositions[0] + 2, dotPositions[1] - dotPositions[0] - 2).c_str());
    int b = atoi(token.substr(dotPositions[1] + 2).c_str());
    if (s == 0) { result.push_back(std::to_string(a)); return result; }
    for (int i = a; (s > 0 ? i <= b : i >= b); i += s) {
      result.push_back(std::to_string(i));
    }
  } else {
    // Not a range, return as-is
    result.push_back(token);
  }
  return result;
}

//^============================================ Script line structure

struct ScriptLine {
  std::string text;
  int lineNum;
};

// Forward declarations
static int ExecuteCommand(const std::string& line, BrokerClient& client, bool echo);
static int ProcessLines(const std::vector<ScriptLine>& lines, size_t start, size_t end,
                        BrokerClient& client, std::map<std::string, std::string>& vars);

//^============================================ Find matching block end

// Find matching endfor for a for at position 'start', handling nesting
static size_t FindEndFor(const std::vector<ScriptLine>& lines, size_t start, size_t end) {
  int depth = 1;
  for (size_t i = start + 1; i < end; i++) {
    auto tok = Tokenize(lines[i].text);
    if (tok.empty()) continue;
    if (tok[0] == "for") depth++;
    if (tok[0] == "endfor") { depth--; if (depth == 0) return i; }
  }
  return end; // not found
}

// Find matching endif for an if at position 'start', handling nesting.
// Also returns positions of elif/else at the same level.
struct IfBlock {
  size_t endifPos;
  std::vector<size_t> branchPositions; // positions of if, elif, else (in order)
};

static IfBlock FindIfBlock(const std::vector<ScriptLine>& lines, size_t start, size_t end) {
  IfBlock block;
  block.branchPositions.push_back(start); // the "if" line itself
  int depth = 1;
  for (size_t i = start + 1; i < end; i++) {
    auto tok = Tokenize(lines[i].text);
    if (tok.empty()) continue;
    if (tok[0] == "if") depth++;
    if (tok[0] == "endif") {
      depth--;
      if (depth == 0) { block.endifPos = i; return block; }
    }
    if (depth == 1 && (tok[0] == "elif" || tok[0] == "else")) {
      block.branchPositions.push_back(i);
    }
  }
  block.endifPos = end;
  return block;
}

//^============================================ Condition evaluation

static bool EvalCondition(const std::vector<std::string>& tok, size_t condStart,
                          BrokerClient& client, int lineNum) {
  if (condStart >= tok.size()) {
    printf("ERROR (line %d): empty condition\n", lineNum);
    return false;
  }

  if (tok[condStart] == "read") {
    if (condStart + 4 >= tok.size()) {
      printf("ERROR (line %d): usage: if read <digi> <param> ==|!= <value>\n", lineNum);
      return false;
    }
    int digiIdx = atoi(tok[condStart + 1].c_str());
    std::string param = tok[condStart + 2];
    std::string op = tok[condStart + 3];
    std::string expected = tok[condStart + 4];
    std::string actual = client.ReadValue(digiIdx, param);

    if (op == "==") return actual == expected;
    if (op == "!=") return actual != expected;
    printf("ERROR (line %d): unknown operator '%s', use == or !=\n", lineNum, op.c_str());
    return false;
  }

  if (tok[condStart] == "num_digi") {
    if (condStart + 2 >= tok.size()) {
      printf("ERROR (line %d): usage: if num_digi ==|>|< <N>\n", lineNum);
      return false;
    }
    int actual = client.GetNumDigitizers();
    std::string op = tok[condStart + 1];
    int expected = atoi(tok[condStart + 2].c_str());

    if (op == "==") return actual == expected;
    if (op == "!=") return actual != expected;
    if (op == ">")  return actual > expected;
    if (op == "<")  return actual < expected;
    if (op == ">=") return actual >= expected;
    if (op == "<=") return actual <= expected;
    printf("ERROR (line %d): unknown operator '%s'\n", lineNum, op.c_str());
    return false;
  }

  printf("ERROR (line %d): unknown condition '%s'. Use 'read' or 'num_digi'\n",
         lineNum, tok[condStart].c_str());
  return false;
}

//^============================================ Recursive line processor

// Process lines[start..end) with variable substitution.
// Returns: 0 = continue, 1 = quit
static int ProcessLines(const std::vector<ScriptLine>& lines, size_t start, size_t end,
                        BrokerClient& client, std::map<std::string, std::string>& vars) {
  size_t i = start;
  while (i < end) {
    // Apply variable substitution
    std::string line = SubstituteVars(lines[i].text, vars);
    int lineNum = lines[i].lineNum;
    auto tok = Tokenize(line);
    if (tok.empty()) { i++; continue; }

    //--- for loop ---
    if (tok[0] == "for") {
      // for <var> in <values...>
      if (tok.size() < 4 || tok[2] != "in") {
        printf("ERROR (line %d): usage: for <var> in <values...>\n", lineNum);
        i++; continue;
      }
      std::string varName = tok[1];
      size_t bodyEnd = FindEndFor(lines, i, end);
      if (bodyEnd >= end) {
        printf("ERROR (line %d): for without matching endfor\n", lineNum);
        i++; continue;
      }

      // Collect iteration values (expand ranges)
      std::vector<std::string> values;
      for (size_t t = 3; t < tok.size(); t++) {
        // Apply variable substitution to the value tokens too
        std::string val = SubstituteVars(tok[t], vars);
        if (val.find("..") != std::string::npos) {
          auto expanded = ExpandRange(val);
          values.insert(values.end(), expanded.begin(), expanded.end());
        } else {
          values.push_back(val);
        }
      }

      printf("> for %s in ", varName.c_str());
      if (values.size() <= 10) {
        for (size_t v = 0; v < values.size(); v++) printf("%s%s", values[v].c_str(), v + 1 < values.size() ? " " : "");
      } else {
        printf("%s %s ... %s (%zu values)", values[0].c_str(), values[1].c_str(),
               values.back().c_str(), values.size());
      }
      printf("\n");

      // Execute body for each value
      std::string oldVal;
      bool hadOld = vars.count(varName);
      if (hadOld) oldVal = vars[varName];

      for (const auto& val : values) {
        vars[varName] = val;
        int ret = ProcessLines(lines, i + 1, bodyEnd, client, vars);
        if (ret == 1) return 1;
      }

      // Restore previous variable value
      if (hadOld) vars[varName] = oldVal;
      else vars.erase(varName);

      printf("> endfor  (%s)\n", varName.c_str());
      i = bodyEnd + 1; // skip past endfor
      continue;
    }

    if (tok[0] == "endfor") {
      // Should not reach here (handled by FindEndFor), but just in case
      i++; continue;
    }

    //--- if/elif/else/endif ---
    if (tok[0] == "if") {
      IfBlock block = FindIfBlock(lines, i, end);
      if (block.endifPos >= end) {
        printf("ERROR (line %d): if without matching endif\n", lineNum);
        i++; continue;
      }

      bool anyTaken = false;

      for (size_t b = 0; b < block.branchPositions.size(); b++) {
        size_t branchLine = block.branchPositions[b];
        std::string branchText = SubstituteVars(lines[branchLine].text, vars);
        auto branchTok = Tokenize(branchText);

        // Determine body range: from branchLine+1 to next branch or endif
        size_t bodyStart = branchLine + 1;
        size_t bodyEnd = (b + 1 < block.branchPositions.size())
                         ? block.branchPositions[b + 1]
                         : block.endifPos;

        if (anyTaken) {
          // Skip -- a previous branch was taken
          if (branchTok[0] == "if" || branchTok[0] == "elif") {
            printf("> %s  -> false (previous branch taken)\n", branchText.c_str());
          } else {
            printf("> else  -> false (previous branch taken)\n");
          }
          continue;
        }

        bool result;
        if (branchTok[0] == "else") {
          result = true;
          printf("> else  -> true\n");
        } else {
          // "if" or "elif" -- evaluate condition (condition starts at tok[1])
          result = EvalCondition(branchTok, 1, client, lines[branchLine].lineNum);
          printf("> %s  -> %s\n", branchText.c_str(), result ? "true" : "false");
        }

        if (result) {
          anyTaken = true;
          int ret = ProcessLines(lines, bodyStart, bodyEnd, client, vars);
          if (ret == 1) return 1;
        }
      }

      printf("> endif\n");
      i = block.endifPos + 1;
      continue;
    }

    // Skip stray elif/else/endif (shouldn't happen with correct scripts)
    if (tok[0] == "elif" || tok[0] == "else" || tok[0] == "endif") {
      printf("ERROR (line %d): unexpected '%s'\n", lineNum, tok[0].c_str());
      i++; continue;
    }

    //--- set command: assign variables ---
    if (tok[0] == "set") {
      if (tok.size() < 3) {
        printf("ERROR (line %d): usage: set <var> <value|num_digi|read ...>\n", lineNum);
        i++; continue;
      }
      std::string varName = tok[1];
      std::string value;

      if (tok[2] == "num_digi") {
        value = std::to_string(client.GetNumDigitizers());
      } else if (tok[2] == "read" && tok.size() >= 5) {
        int digiIdx = atoi(tok[3].c_str());
        value = client.ReadValue(digiIdx, tok[4]);
      } else if (tok.size() == 5 && (tok[3] == "+" || tok[3] == "-" || tok[3] == "*" || tok[3] == "/")) {
        // Arithmetic: set var A op B
        int a = atoi(tok[2].c_str());
        int b = atoi(tok[4].c_str());
        if (tok[3] == "+") value = std::to_string(a + b);
        else if (tok[3] == "-") value = std::to_string(a - b);
        else if (tok[3] == "*") value = std::to_string(a * b);
        else if (tok[3] == "/" && b != 0) value = std::to_string(a / b);
        else value = "0";
      } else {
        // Literal value
        value = tok[2];
      }

      vars[varName] = value;
      printf("> set %s = %s\n", varName.c_str(), value.c_str());
      i++; continue;
    }

    //--- Regular command ---
    printf("> %s\n", line.c_str());
    int ret = ExecuteCommand(line, client, false);
    if (ret == 1) return 1;

    i++;
  }
  return 0;
}

//^============================================ Script loader

static int LoadScript(const std::string& filename, BrokerClient& client) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    printf("ERROR: cannot open script file: %s\n", filename.c_str());
    return 0;
  }

  // Read all lines
  std::vector<ScriptLine> lines;
  std::string rawLine;
  int lineNum = 0;
  while (std::getline(file, rawLine)) {
    lineNum++;

    // Strip whitespace
    size_t start = rawLine.find_first_not_of(" \t");
    if (start == std::string::npos) continue;
    size_t end = rawLine.find_last_not_of(" \t\r\n");
    std::string trimmed = rawLine.substr(start, end - start + 1);

    if (trimmed.empty() || trimmed[0] == '#') continue;
    lines.push_back({trimmed, lineNum});
  }

  printf("--- loading script: %s ---\n", filename.c_str());
  std::map<std::string, std::string> vars;
  int ret = ProcessLines(lines, 0, lines.size(), client, vars);
  printf("--- end of script: %s (%d lines) ---\n", filename.c_str(), lineNum);
  return ret;
}

struct ParamInfo {
  std::string name;      // "SelfTriggerWidth"
  std::string scope;     // "ch" | "dig" | "cmd" | "lvds" | "vga" | "group"
  std::string unit;
  RW          rw;
  ANSTYPE     ansType;
  std::vector<std::pair<std::string,std::string>> answers;
};

// Completion needs to ASK THE BOARD for a parameter's allowed values, and it must do so
// without a broker restart. The existing REQ_READ_VALUE path already reaches device-tree
// subnodes (verified live: read 0 /ch/0/par/SelfTriggerWidth/setinrun -> "false"), so no new
// opcode is needed. The Reg tables in DAQ:core/DigiParameters.h are NOT trustworthy for enum
// values -- measured 2026-10-21, 14 of the board's 48 enumerated parameters disagree with them:
//   WaveTriggerSource  header "TrgIn"    vs board canonical "TRGIN"      (case differs; board
//                                                                              is case-insensitive)
//   StartSource        header lacks "P0"
//   GlobalTriggerSource header lacks "P0", "EncodedClkIn"
//   DACoutMode         header lacks "MemOccupancy"
//   GPIOMode           header lacks "UserGPO"
//   SyncOutMode        header lacks "User"
//   EnergyFilterPeakingAvg header lacks "HighAVG"
//   WaveDigitalProbe0..3 header "EnergyFilterPileUpGuard" vs board "...PileupGuard"
// So the local answers are a FALLBACK and the board is authoritative; the union is offered.
static BrokerClient* g_client = nullptr;
static std::map<std::string, std::vector<std::string>> g_valCache;

// One device-tree subnode, cached. Returns "" when not connected or the node is absent.
static std::map<std::string, std::string> g_nodeCache;
static std::string SubNode(const std::string& path, const std::string& node, int digi) {
  std::string key = std::to_string(digi) + " " + path + "/" + node;
  auto it = g_nodeCache.find(key);
  if (it != g_nodeCache.end()) return it->second;
  std::string v = (g_client && g_client->IsConnected()) ? g_client->ReadValue(digi, path + "/" + node) : "";
  g_nodeCache[key] = v;
  return v;
}

static std::vector<std::string> BoardValues(const std::string& path, int digi) {
  std::string ckey = std::to_string(digi) + " " + path;
  auto it = g_valCache.find(ckey);
  if (it != g_valCache.end()) return it->second;
  std::vector<std::string> out;
  if (!g_client || !g_client->IsConnected()) { g_valCache[ckey] = out; return out; }
  // "<path>/allowedvalues" returns the COUNT; each value is "<path>/allowedvalues/<i>".
  std::string cntS = g_client->ReadValue(digi, path + "/allowedvalues");
  int n = cntS.empty() ? 0 : atoi(cntS.c_str());
  if (n <= 0 || n > 64) { g_valCache[path] = out; return out; }
  for (int i = 0; i < n; i++) {
    std::string v = g_client->ReadValue(digi, path + "/allowedvalues/" + std::to_string(i));
    if (!v.empty()) out.push_back(v);
  }
  g_valCache[ckey] = out;
  return out;
}
static std::vector<ParamInfo> g_params;   // one entry per distinct parameter NAME
static bool g_paramsBuilt = false;

static void AddReg(const Reg& r, const char* scope) {
  ParamInfo p;
  p.name = r.GetPara();
  p.scope = scope;
  p.unit = r.GetUnit();
  p.rw = r.ReadWrite();
  p.ansType = r.GetAnswerType();
  p.answers = r.GetAnswers();
  g_params.push_back(p);
}

template <typename V>
static void AddTable(const V& v, const char* scope) {
  for (const Reg& r : v) AddReg(r, scope);
}

// The AllSettings vectors omit these 7 real parameters; they are completed from this list,
// name-only -- type/range/description are read from the board on demand, so nothing here can
// go stale the way the enum answer-vectors did (AGENT.md §27).
//
// NOT included: the 10 *_S (sample-unit) twins that AllSettings also omits -- ChPreTriggerS,
// ChRecordLengthS, CoincidenceLengthS, TimeFilter{RiseTime,RetriggerGuard}S,
// EnergyFilter{RiseTime,FlatTop,PoleZero,BaselineGuard,PileUpGuard}S. Each is ONE REGISTER with
// its *_T twin, which IS in AllSettings: measured 2026-10-21 by writing each *_T and reading
// both names, T/(8*S) = 1.000 for all ten. Same range, same setinrun, so the *_T form reaches
// the identical value and completing both would just double every entry in the list. Trade-off
// accepted: 'param 0 ChRecordLengthS' no longer resolves -- use the T form.
//
// These 7 are NOT added to AllSettings either: that vector drives ReadAllSettings() and
// DumpSettingsCache(), so touching it changes GUI and settings-file behaviour -- a far bigger
// blast radius than command completion deserves.
struct ExtraName { const char* scope; const char* name; };
static const ExtraName extraNames[] = {
  { "ch",  "SignalOffset" },
  { "ch",  "WaveDownSamplingFactor" },
  { "dig", "LVDSIOReg" },
  { "dig", "LVDSTrgMask" },   // Reg exists but is COMMENTED OUT in DigiParameters.h:300
  { "dig", "SFPLinkActive" },
  { "dig", "SFPLinkPresence" },
  { "dig", "SFPLinkProtocol" },
  { nullptr, nullptr }
};

static void BuildParamTable() {
  if (g_paramsBuilt) return;
  g_paramsBuilt = true;
  AddTable(PHA::DIG::AllSettings, "dig");
  AddTable(PHA::CH::AllSettings,  "ch");
  AddTable(PHA::LVDS::AllSettings,"lvds");
  // /cmd/ are not in any AllSettings vector; declared individually at DigiParameters.h:304
  AddReg(PHA::DIG::Reset,                "cmd");
  AddReg(PHA::DIG::ClearData,            "cmd");
  AddReg(PHA::DIG::ArmACQ,               "cmd");
  AddReg(PHA::DIG::DisarmACQ,            "cmd");
  AddReg(PHA::DIG::SoftwareStartACQ,     "cmd");
  AddReg(PHA::DIG::SoftwareStopACQ,      "cmd");
  AddReg(PHA::DIG::SendSoftwareTrigger,  "cmd");
  AddReg(PHA::DIG::ReloadCalibration,    "cmd");
  AddReg(PHA::DIG::Reboot,               "cmd");
  AddReg(PHA::GROUP::InputDelay,         "group");
  AddReg(PHA::VGA::VGAGain,              "vga");
  for (int i = 0; extraNames[i].scope; i++) {
    ParamInfo p; p.name = extraNames[i].name; p.scope = extraNames[i].scope;
    p.rw = RW::ReadWrite; p.ansType = ANSTYPE::NONE;
    g_params.push_back(p);
  }
}

static const ParamInfo* FindParam(const std::string& name) {
  for (const ParamInfo& p : g_params)
    if (strcasecmp(p.name.c_str(), name.c_str()) == 0) return &p;
  return nullptr;
}

// Is this parameter's answer vector a value list (vs min/max/step)?
static bool IsEnum(const ParamInfo& p) {
  if (p.answers.empty()) return false;
  return !(p.ansType == ANSTYPE::INTEGER || p.ansType == ANSTYPE::FLOAT);
}

//^---------------- shared: canonical path + write-time warnings

// Canonical full path for a parameter name, in the form the board accepts.
static std::string CanonPath(const ParamInfo& p) {
  return (p.scope=="ch")   ? "/ch/0/par/"  + p.name :
         (p.scope=="dig")  ? "/par/"       + p.name :
         (p.scope=="cmd")  ? "/cmd/"       + p.name :
         (p.scope=="lvds") ? "/lvds/0/par/"+ p.name :
         (p.scope=="vga")  ? "/vga/0/par/" + p.name : "/group/0/par/" + p.name;
}

// Split a typed path into (dir-through-par, leaf name). "/ch/52/par/TriggerThr" ->
// ("/ch/52/par/", "TriggerThr").
static void SplitPath(const std::string& path, std::string& dir, std::string& leaf) {
  size_t s = path.rfind('/');
  dir  = (s == std::string::npos) ? std::string() : path.substr(0, s + 1);
  leaf = (s == std::string::npos) ? path : path.substr(s + 1);
}

// Warn before a write that cannot take effect, for the two parameter classes that invite it.
// Both are REFUSED by FELib with an explicit "WriteValue failed" -- measured on this board
// 2026-10-21 with ACQ running: SelfTriggerWidth, ITLConnect, TrgOutMode and ChRecordLengthS
// (all setinrun=false) and SelfTrgRate / ChWaveCnt (READ_ONLY) each failed the write.
// So neither case is silent, and the earlier belief recorded in DAQ:agent/README.md that a
// setinrun=false write is "accepted and silently ignored" is WRONG for this firmware -- the
// VALUE is unchanged either way, which is what that note was reaching for.
// The warning still earns its place: it names the CAUSE before the generic failure line,
// which is the thing that actually costs time here.
// setinrun is NOT in the Reg tables at all -- it exists only in the board's device tree,
// hence the read below rather than a table lookup.
static void WarnBeforeWrite(BrokerClient& client, const std::string& path, int digi) {
  BuildParamTable();          // NOT lazily built elsewhere in piped/script mode: CliCompletion
                              // is the only other caller and it never runs without a tty.
  if (!client.IsConnected()) return;
  std::string dir, leaf;
  SplitPath(path, dir, leaf);
  const ParamInfo* p = FindParam(leaf);

  if (p && p->rw == RW::ReadOnly) {
    // FELib refuses these outright, so the failure is visible; flagged because
    // SelfTrgRate / ChTriggerCnt / ChWaveCnt are named like settings and invite the attempt
    // (AGENT.md §19: they are readouts, and they read 0 outside a run).
    printf("WARNING: %s is READ_ONLY -- this is a readout, not a setting.\n", leaf.c_str());
  }

  std::string sir = SubNode(path, "setinrun", digi);
  if (sir == "false") {
    // Only worth saying something if acquisition is actually running. GetACQStatus() is the
    // real per-digitizer flag; /par/AcquisitionStatus is NOT usable (reads 8 while OFF).
    auto st = client.GetACQStatus();
    bool running = (digi >= 0 && digi < st.nDigi) ? st.acqOn[digi] : false;
    if (running)
      printf("WARNING: %s has setinrun=false and ACQUISITION IS RUNNING -- stop ACQ first.\n",
             leaf.c_str());
    else
      printf("note: %s has setinrun=false (needs ACQ stopped to take effect)\n", leaf.c_str());
  }
}

//^---------------- 'param' : search + describe parameters

static void PrintParam(const ParamInfo& p, BrokerClient& client, int digi, bool detail) {
  std::string path = CanonPath(p);
  const char* rw = (p.rw == RW::ReadOnly)  ? "RO" :
                   (p.rw == RW::WriteOnly) ? "WO" : "RW";
  if (!detail) {
    printf("  %-32s %-6s %s\n", p.name.c_str(), path.c_str(), rw);
    return;
  }
  printf("%s\n", path.c_str());
  printf("  scope      : %s\n", p.scope.c_str());
  printf("  access     : %s", rw);
  std::string acc = SubNode(path, "accessmode", digi);
  if (!acc.empty()) printf("  (board: %s)", acc.c_str());
  printf("\n");

  std::string sir = SubNode(path, "setinrun", digi);
  if (!sir.empty())
    printf("  setinrun   : %s%s\n", sir.c_str(),
           sir == "false" ? "   <-- needs ACQ STOPPED to take effect" : "");

  std::string desc = SubNode(path, "description", digi);
  if (!desc.empty()) printf("  description: %s\n", desc.c_str());

  std::string mn = SubNode(path, "minvalue", digi);
  std::string mx = SubNode(path, "maxvalue", digi);
  std::string st = SubNode(path, "increment", digi);
  std::string un = SubNode(path, "uom", digi);
  if (!mn.empty() || !mx.empty())
    printf("  range      : %s .. %s  step %s  %s\n",
           mn.empty() ? "-" : mn.c_str(), mx.empty() ? "-" : mx.c_str(),
           st.empty() ? "-" : st.c_str(), un.c_str());
  else if (!p.answers.empty() && (p.ansType == ANSTYPE::INTEGER || p.ansType == ANSTYPE::FLOAT))
    printf("  range      : %s .. %s  step %s  %s  (from firmware table; not confirmed by board)\n",
           p.answers[0].first.c_str(),
           p.answers.size() > 1 ? p.answers[1].first.c_str() : "?",
           p.answers.size() > 2 ? p.answers[2].first.c_str() : "?",
           p.unit.c_str());

  std::vector<std::string> vals = BoardValues(path, digi);
  if (!vals.empty()) {
    printf("  values     :");
    for (size_t i = 0; i < vals.size(); i++) printf(" %s%s", vals[i].c_str(),
        (i + 1) % 5 == 0 && i + 1 < vals.size() ? "\n               :" : "");
    printf("\n");
  } else if (!p.answers.empty() && !IsEnum(p)) {
    printf("  values     : (none -- numeric)\n");
  }
  // Flag a firmware table that is INCOMPLETE relative to the board. Measured 2026-10-21: the
  // Reg answer vectors are a strict subset of the board's allowedvalues modulo case -- they
  // never offer a value the board rejects, but they omit 6 real ones (StartSource/
  // GlobalTriggerSource lack "P0", GlobalTriggerSource lacks "EncodedClkIn", DACoutMode lacks
  // "MemOccupancy", GPIOMode lacks "UserGPO", SyncOutMode lacks "User", EnergyFilterPeakingAvg
  // lacks "HighAVG"). Matters beyond this CLI: DAQ:GUI/digiSettingsPanel.cpp SetupComboBox()
  // builds its dropdown from these same answers, so those 6 values cannot be selected in the
  // GUI at all. Completion unions both lists, so it is not affected while connected.
  if (!vals.empty() && !p.answers.empty() && IsEnum(p)) {
    std::string missing;
    for (const std::string& v : vals) {
      bool found = false;
      for (const auto& a : p.answers) if (!strcasecmp(v.c_str(), a.first.c_str())) found = true;
      if (!found) { if (!missing.empty()) missing += " "; missing += v; }
    }
    if (!missing.empty())
      printf("  NOTE       : firmware table omits: %s -- TRUST THE BOARD'S LIST ABOVE\n",
             missing.c_str());
  }
  std::string def = SubNode(path, "defaultvalue", digi);
  std::string cur = client.IsConnected() ? client.ReadValue(digi, path) : "";
  if (!def.empty()) printf("  default    : %s\n", def.c_str());
  if (!cur.empty()) printf("  current    : %s\n", cur.c_str());
}

static int CmdParam(const std::vector<std::string>& tok, BrokerClient& client) {
  BuildParamTable();
  int digi = 0;
  size_t ai = 1;
  if (tok.size() > 2 && tok[1].size() && isdigit((unsigned char)tok[1][0])) {
    digi = atoi(tok[1].c_str()); ai = 2;
  }
  if (tok.size() <= ai) {
    printf("Usage: param [digi] <substring>        search parameter names\n");
    printf("       param [digi] <Name|/full/path>  describe one parameter\n");
    printf("Range/description/values are read from the board's device tree through the\n");
    printf("existing read path -- no broker restart is needed to use this.\n");
    return 0;
  }
  std::string q = tok[ai];
  std::string dir, leaf;
  SplitPath(q, dir, leaf);

  // exact hit?
  for (const ParamInfo& p : g_params) {
    if (!strcasecmp(p.name.c_str(), leaf.c_str()) &&
        (dir.empty() || CanonPath(p).rfind(dir, 0) == 0)) {
      PrintParam(p, client, digi, true);
      return 0;
    }
  }
  // substring search over name AND description
  std::string lq = q; std::transform(lq.begin(), lq.end(), lq.begin(), ::tolower);
  int n = 0;
  printf("matching '%s':\n", q.c_str());
  for (const ParamInfo& p : g_params) {
    std::string ln = p.name; std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
    bool hit = ln.find(lq) != std::string::npos;
    if (!hit) {
      std::string d = SubNode(CanonPath(p), "description", digi);
      std::transform(d.begin(), d.end(), d.begin(), ::tolower);
      hit = !d.empty() && d.find(lq) != std::string::npos;
    }
    if (hit) { PrintParam(p, client, digi, false); n++; }
  }
  if (!n) printf("  (none)\n");
  else printf("  %d match(es). 'param %s <Name>' for full detail.\n", n, tok[1].c_str());
  return 0;
}

//^============================================ Single command executor

static int ExecuteCommand(const std::string& line, BrokerClient& client, bool echo) {
  auto tok = Tokenize(line);
  if (tok.empty()) return 0;
  const std::string& cmd = tok[0];

  if (echo) {
    printf("> %s\n", line.c_str());
  }

  if (cmd == "quit" || cmd == "exit") {
    return 1;
  }

  if (cmd == "!") {
    // Shell command: everything after "!" is passed to system()
    size_t pos = line.find('!');
    if (pos != std::string::npos && pos + 1 < line.size()) {
      std::string shellCmd = line.substr(pos + 1);
      // Trim leading whitespace
      size_t start = shellCmd.find_first_not_of(" \t");
      if (start != std::string::npos) {
        shellCmd = shellCmd.substr(start);
        int rc = system(shellCmd.c_str());
        if (rc != 0) printf("(exit code %d)\n", WEXITSTATUS(rc));
      }
    }
    return 0;
  }

  if (cmd == "help") {
    PrintHelp();
    return 0;
  }

  if (cmd == "sleep") {
    int ms = (tok.size() > 1) ? atoi(tok[1].c_str()) : 1000;
    gInterrupted = 0;
    for (int t = 0; t < ms && !gInterrupted; t += 100) {
      std::this_thread::sleep_for(std::chrono::milliseconds(std::min(100, ms - t)));
    }
    if (gInterrupted) { printf("\nSleep interrupted.\n"); gInterrupted = 0; }
    return 0;
  }

  if (cmd == "load") {
    if (tok.size() < 2) { printf("Usage: load <script>\n"); return 0; }
    return LoadScript(tok[1], client);
  }

  if (cmd == "connect") {
    std::string host = (tok.size() > 1) ? tok[1] : "localhost";
    int cmdPort = (tok.size() > 2) ? atoi(tok[2].c_str()) : 5555;
    int pubPort = (tok.size() > 3) ? atoi(tok[3].c_str()) : 5556;
    std::string cmdEp = "tcp://" + host + ":" + std::to_string(cmdPort);
    std::string pubEp = "tcp://" + host + ":" + std::to_string(pubPort);
    if (client.Connect(cmdEp, pubEp) == 0) {
      printf("Connected to %s\n", cmdEp.c_str());
    } else {
      printf("Failed: %s\n", client.GetLastError().c_str());
    }
    return 0;
  }

  if (cmd == "disconnect") {
    client.Disconnect();
    printf("Disconnected.\n");
    return 0;
  }

  if (!client.IsConnected()) {
    printf("Not connected. Use 'connect' first.\n");
    return 0;
  }

  if (cmd == "ping") {
    if (client.Ping()) {
      printf("PONG - broker is alive\n");
    } else {
      printf("No response: %s\n", client.GetLastError().c_str());
    }
  }
  else if (cmd == "list") {
    auto digis = client.ListDigitizers();
    if (digis.empty()) {
      printf("No digitizers.\n");
    } else {
      printf("%-5s %-8s %-10s %-10s %-6s\n", "Index", "SN", "Model", "FPGA", "NCh");
      for (size_t i = 0; i < digis.size(); i++) {
        if (digis[i].isConnected) {
          printf("%-5zu %-8d %-10s %-10s %-6d\n", i,
                 digis[i].serialNumber,
                 digis[i].modelName.c_str(),
                 digis[i].fpgaType.c_str(),
                 digis[i].nChannels);
        } else {
          printf("%-5zu (disconnected)\n", i);
        }
      }
    }
  }
  else if (cmd == "info") {
    if (tok.size() < 2) { printf("Usage: info <digi>\n"); return 0; }
    int idx = atoi(tok[1].c_str());
    auto info = client.GetDigiInfo(idx);
    if (info.isConnected) {
      printf("Digitizer %d:\n", idx);
      printf("  Serial Number: %d\n", info.serialNumber);
      printf("  Model: %s\n", info.modelName.c_str());
      printf("  FPGA Type: %s\n", info.fpgaType.c_str());
      printf("  Channels: %d\n", info.nChannels);
      printf("  Tick2ns: %d\n", info.tick2ns);
      printf("  FPGA Version: %u\n", info.fpgaVersion);
      printf("  CupVersion: %u\n", info.cupVersion);
    } else {
      printf("Error: %s\n", client.GetLastError().c_str());
    }
  }
  else if (cmd == "open") {
    if (tok.size() < 2) { printf("Usage: open <url>\n"); return 0; }
    int idx = client.OpenDigitizer(tok[1]);
    if (idx >= 0) {
      printf("Opened as digitizer %d\n", idx);
    } else {
      printf("Failed: %s\n", client.GetLastError().c_str());
    }
  }
  else if (cmd == "close") {
    if (tok.size() < 2) { printf("Usage: close <digi>\n"); return 0; }
    client.CloseDigitizer(atoi(tok[1].c_str()));
    printf("OK\n");
  }
  else if (cmd == "read") {
    if (tok.size() < 3) { printf("Usage: read <digi> <param>\n"); return 0; }
    std::string val = client.ReadValue(atoi(tok[1].c_str()), tok[2]);
    if (!val.empty()) {
      printf("%s\n", val.c_str());
    } else {
      printf("Error: %s\n", client.GetLastError().c_str());
    }
  }
  else if (cmd == "param") {
    return CmdParam(tok, client);
  }
  else if (cmd == "write") {
    if (tok.size() < 4) { printf("Usage: write <digi> <param> <value>\n"); return 0; }
    WarnBeforeWrite(client, tok[2], atoi(tok[1].c_str()));
    if (client.WriteValue(atoi(tok[1].c_str()), tok[2], tok[3])) {
      printf("OK\n");
    } else {
      printf("Failed: %s\n", client.GetLastError().c_str());
    }
  }
  else if (cmd == "cmd") {
    if (tok.size() < 3) { printf("Usage: cmd <digi> <command>\n"); return 0; }
    client.SendCommand(atoi(tok[1].c_str()), tok[2]);
    printf("OK\n");
  }
  else if (cmd == "format") {
    if (tok.size() < 3) { printf("Usage: format <digi> <fmt>\n"); return 0; }
    client.SetDataFormat(atoi(tok[1].c_str()), atoi(tok[2].c_str()));
    printf("OK\n");
  }
  else if (cmd == "start") {
    if (tok.size() < 3) { printf("Usage: start <digi|all> <fmt> [save <path>]\n"); return 0; }
    int digiIdx = (tok[1] == "all") ? 0xFF : atoi(tok[1].c_str());
    int fmt = atoi(tok[2].c_str());
    bool save = false;
    std::string path;
    if (tok.size() >= 5 && tok[3] == "save") {
      save = true;
      path = tok[4];
    }
    client.StartACQ(digiIdx, fmt, save, path);
    printf("ACQ started.\n");
  }
  else if (cmd == "stop") {
    if (tok.size() < 2) { printf("Usage: stop <digi|all>\n"); return 0; }
    int digiIdx = (tok[1] == "all") ? 0xFF : atoi(tok[1].c_str());
    client.StopACQ(digiIdx);
    printf("ACQ stopped.\n");
  }
  else if (cmd == "status") {
    auto st = client.GetACQStatus();
    printf("%d digitizer(s):\n", st.nDigi);
    for (int i = 0; i < st.nDigi; i++) {
      printf("  Digi %d: ACQ %s\n", i, st.acqOn[i] ? "ON" : "OFF");
    }
  }
  else if (cmd == "file-status") {
    if (tok.size() < 2) { printf("Usage: file-status <digi>\n"); return 0; }
    auto fs = client.GetFileStatus(atoi(tok[1].c_str()));
    printf("Total: %.2f MB, Current file: %.2f MB\n",
           fs.totalFileSize / 1048576.0, fs.currentFileSize / 1048576.0);
  }
  else if (cmd == "save-settings") {
    if (tok.size() < 2) { printf("Usage: save-settings <digi> [file]\n"); return 0; }
    std::string fn = (tok.size() > 2) ? tok[2] : "";
    client.SaveSettingsFile(atoi(tok[1].c_str()), fn);
    printf("OK\n");
  }
  else if (cmd == "load-settings") {
    if (tok.size() < 2) { printf("Usage: load-settings <digi> [file]\n"); return 0; }
    std::string fn = (tok.size() > 2) ? tok[2] : "";
    client.LoadSettingsFile(atoi(tok[1].c_str()), fn);
    printf("OK\n");
  }
  else if (cmd == "subscribe") {
    int seconds = (tok.size() > 1) ? atoi(tok[1].c_str()) : 10;
    printf("Listening for scalar updates for %d seconds...\n", seconds);

    client.onScalarUpdate = [&client](int digiIdx) {
      std::lock_guard<std::mutex> lock(client.scalarMutex);
      auto& sd = client.scalarData[digiIdx];
      printf("\n--- Digi %d (SN=%d) ACQ=%s FileSize=%.2f MB ---\n",
             digiIdx, sd.serialNumber, sd.acqOn ? "ON" : "OFF",
             sd.totalFileSize / 1048576.0);
      printf("%-4s %10s %10s %10s\n", "Ch", "TrgRate", "Accepted", "AccpRate");
      for (int ch = 0; ch < sd.nChannels; ch++) {
        if (sd.trgRate[ch] > 0 || sd.savedCount[ch] > 0) {
          printf("%-4d %10u %10lu %10.1f\n", ch,
                 sd.trgRate[ch], (unsigned long)sd.savedCount[ch], sd.acceptRate[ch]);
        }
      }
    };

    client.onLogMessage = [](const std::string& msg) {
      printf("[BROKER] %s\n", msg.c_str());
    };

    client.onStatusChange = [](StatusEvent event, int digiIdx) {
      const char* names[] = {"?", "ACQ_STARTED", "ACQ_STOPPED", "FILE_OPENED",
                             "FILE_CLOSED", "DIGI_OPENED", "DIGI_CLOSED"};
      int idx = static_cast<int>(event);
      printf("[STATUS] Digi %d: %s\n", digiIdx,
             (idx >= 0 && idx < 7) ? names[idx] : "unknown");
    };

    gInterrupted = 0;
    for (int t = 0; t < seconds * 10 && !gInterrupted; t++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    client.onScalarUpdate = nullptr;
    client.onLogMessage = nullptr;
    client.onStatusChange = nullptr;
    if (gInterrupted) {
      printf("\nSubscription interrupted.\n");
      gInterrupted = 0;
    } else {
      printf("\nSubscription ended.\n");
    }
  }
  else if (cmd == "hist") {
    /// Energy histogram from the hit-summary stream the subscription thread already pushes into
    /// the client ring (PUB_HIT_SUMMARY). The server only publishes hits while ACQ is ON.
    /// Drain semantics and the fresh-start cursor mirror SingleSpectra::FillHistograms(): the
    /// cursor starts at the live ring end, so this measures the NEXT N seconds, not a backlog.
    if (tok.size() < 3) { printf("Usage: hist <digi> <ch|all> [seconds] [nbin] [xmax] [long|short]\n"); return 0; }
    if (!client.IsConnected()) { printf("Not connected to broker.\n"); return 0; }

    int digiIdx = atoi(tok[1].c_str());
    bool allCh  = (tok[2] == "all");
    int ch0     = allCh ? -1 : atoi(tok[2].c_str());
    double seconds = (tok.size() > 3) ? atof(tok[3].c_str()) : 5.0;
    int nbin       = (tok.size() > 4) ? atoi(tok[4].c_str()) : 200;
    double xmax    = (tok.size() > 5) ? atof(tok[5].c_str()) : 5000.0;
    bool useShort  = (tok.size() > 6) && (tok[6] == "short" || tok[6] == "s");
    if (seconds <= 0 || nbin < 1 || nbin > 16384 || xmax <= 0) {
      printf("Bad arguments (need seconds>0, 1<=nbin<=16384, xmax>0).\n"); return 0;
    }
    BrokerClient::DigiInfo dinfo = client.GetDigiInfo(digiIdx);
    if (!dinfo.isConnected) { printf("Digi %d not connected on broker.\n", digiIdx); return 0; }
    int nCh = dinfo.nChannels;
    if (!allCh && (ch0 < 0 || ch0 >= nCh)) { printf("ch must be 0..%d or 'all'.\n", nCh-1); return 0; }

    struct ChHist {
      std::vector<uint32_t> bins;
      unsigned long under = 0, over = 0, total = 0;
      double eSum = 0;                 // for the mean
      unsigned long cursor = 0;
    };
    std::vector<ChHist> hists(nCh);
    for (auto& h : hists) h.bins.assign(nbin, 0);

    int hi = allCh ? nCh : ch0 + 1;
    int lo = allCh ? 0 : ch0;

    // Fresh-start: sync every cursor to the live end BEFORE collecting (same reasoning as the
    // GUI checkbox fix -- anything already sitting in the ring is history, not this measurement).
    for (int ch = lo; ch < hi; ch++) hists[ch].cursor = client.GetRingBuffer(digiIdx, ch).index();

    printf("Collecting %s-gate hits, digi %d, ch %s, %.1f s, %d bins / %.0f ...\n",
           useShort ? "short" : "long", digiIdx, allCh ? "all" : std::to_string(ch0).c_str(),
           seconds, nbin, xmax);

    gInterrupted = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(seconds*1000));
    while (std::chrono::steady_clock::now() < deadline && !gInterrupted) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      for (int ch = lo; ch < hi; ch++) {
        auto& rb = client.GetRingBuffer(digiIdx, ch);
        auto& h  = hists[ch];
        unsigned long now = rb.index();
        if (now - h.cursor > RingBufferSize) h.cursor = now - RingBufferSize;  // fell behind: skip oldest
        while (h.cursor < now) {
          HitSummary hs = rb.at(h.cursor++);
          uint16_t e = useShort ? hs.energy_short : hs.energy;
          h.total++; h.eSum += e;
          int b = (int)((double)e * nbin / xmax);
          if (b < 0) h.under++;
          else if (b >= nbin) h.over++;
          else h.bins[b]++;
        }
      }
    }
    if (gInterrupted) { printf("\nInterrupted.\n"); gInterrupted = 0; }

    const char* gate = useShort ? "short" : "long";
    if (allCh) {
      // One summary line per channel that saw anything -- compact for scripts and agents.
      printf("# hist digi=%d ch=all gate=%s bins=%d xmax=%.0f\n", digiIdx, gate, nbin, xmax);
      printf("%-4s %10s %10s %10s %10s %10s\n", "Ch", "Counts", "Mean", "Over", "Under", "PeakBin");
      for (int ch = lo; ch < hi; ch++) {
        auto& h = hists[ch];
        if (!h.total) continue;
        int peak = (int)(std::max_element(h.bins.begin(), h.bins.end()) - h.bins.begin());
        printf("%-4d %10lu %10.1f %10lu %10lu %10d\n", ch, h.total, h.eSum/h.total, h.over, h.under, peak);
      }
    } else {
      auto& h = hists[ch0];
      uint32_t peakCnt = *std::max_element(h.bins.begin(), h.bins.end());
      printf("# hist digi=%d ch=%d gate=%s bins=%d xmax=%.0f total=%lu mean=%.1f under=%lu over=%lu\n",
             digiIdx, ch0, gate, nbin, xmax, h.total, h.total ? h.eSum/h.total : 0.0, h.under, h.over);
      printf("%6s %9s %9s %10s  %s\n", "Bin", "Lo", "Hi", "Count", "Bar");
      double dx = xmax / nbin;
      for (int b = 0; b < nbin; b++) {
        int bar = peakCnt ? (int)((unsigned long long)h.bins[b] * 40 / peakCnt) : 0;
        printf("%6d %9.1f %9.1f %10u  %.*s\n", b, b*dx, (b+1)*dx, h.bins[b], bar, bar ? "#" : "");
      }
    }
  }
  else if (cmd == "trace") {
    if (tok.size() < 3) { printf("Usage: trace <digi> <ch> [samples]\n"); return 0; }
    int digiIdx = atoi(tok[1].c_str());
    int ch = atoi(tok[2].c_str());
    int maxSamples = (tok.size() > 3) ? atoi(tok[3].c_str()) : 100;

    // Save current settings
    std::string oldWaveSaving = client.ReadValue(digiIdx, "/ch/" + std::to_string(ch) + "/par/WaveSaving");
    std::string oldWaveTrigSrc = client.ReadValue(digiIdx, "/ch/" + std::to_string(ch) + "/par/WaveTriggerSource");

    // Configure for trace capture
    int nCh = client.GetDigiInfo(digiIdx).nChannels;
    std::string chRange = "/ch/0.." + std::to_string(nCh - 1);
    client.WriteValue(digiIdx, chRange + "/par/ChEnable", "False");
    client.WriteValue(digiIdx, "/ch/" + std::to_string(ch) + "/par/ChEnable", "True");
    client.WriteValue(digiIdx, "/ch/" + std::to_string(ch) + "/par/WaveSaving", "Always");
    client.WriteValue(digiIdx, "/ch/" + std::to_string(ch) + "/par/WaveTriggerSource", "ChSelfTrigger");

    printf("Capturing trace from digi %d, ch %d (Ctrl+C to abort)...\n", digiIdx, ch);

    // Set up trace callback
    std::atomic<bool> gotTrace(false);
    TraceSnapshot capturedTrace = {};

    client.onTraceSnapshot = [&](int dIdx) {
      if (dIdx != digiIdx || gotTrace) return;
      auto& rb = client.GetTraceRingBuffer(dIdx);
      unsigned long idx = rb.index();
      if (idx == 0) return;
      capturedTrace = rb.at(idx - 1);
      if (capturedTrace.traceLenght > 0) gotTrace = true;
    };

    // Start ACQ with ALL format
    client.StartACQ(digiIdx, DataFormat::ALL, false);

    // Wait for trace (up to 10 seconds)
    gInterrupted = 0;
    for (int t = 0; t < 100 && !gotTrace && !gInterrupted; t++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop ACQ
    client.StopACQ(digiIdx);
    client.onTraceSnapshot = nullptr;

    // Restore settings
    client.WriteValue(digiIdx, chRange + "/par/ChEnable", "True");
    client.WriteValue(digiIdx, "/ch/" + std::to_string(ch) + "/par/WaveSaving", oldWaveSaving);
    client.WriteValue(digiIdx, "/ch/" + std::to_string(ch) + "/par/WaveTriggerSource", oldWaveTrigSrc);

    if (!gotTrace) {
      printf("No trace captured (timeout or no triggers).\n");
    } else {
      int len = std::min((int)capturedTrace.traceLenght, maxSamples);
      printf("Trace: %zu samples (showing %d)\n", capturedTrace.traceLenght, len);
      printf("%6s  %10s  %10s  %4s %4s %4s %4s\n", "Sample", "Analog0", "Analog1", "D0", "D1", "D2", "D3");
      for (int i = 0; i < len; i++) {
        printf("%6d  %10d  %10d  %4d %4d %4d %4d\n", i,
               capturedTrace.analog_probes[0][i],
               capturedTrace.analog_probes[1][i],
               capturedTrace.digital_probes[0][i],
               capturedTrace.digital_probes[1][i],
               capturedTrace.digital_probes[2][i],
               capturedTrace.digital_probes[3][i]);
      }
    }
  }
  else if (cmd == "shutdown") {
    printf("Sending shutdown to broker...\n");
    client.Shutdown();
    printf("Done.\n");
  }
  else {
    printf("Unknown command: %s (type 'help')\n", cmd.c_str());
  }

  return 0;
}

//^============================================ Tab completion

static const char* commands[] = {
  "connect", "disconnect", "ping", "list", "info", "open", "close",
  "read", "write", "param", "cmd", "format", "start", "stop", "status",
  "file-status", "save-settings", "load-settings", "hist", "trace", "subscribe",
  "load", "sleep", "shutdown", "help", "quit", "exit", nullptr
};

//^---------------- Parameter metadata
//
// Built at startup from the Reg tables in DAQ:core/DigiParameters.h, which are ALREADY
// linked into this binary (broker/Makefile:33 pulls ClassDigitizer2Gen.o into solaris-cli).
// The previous implementation hand-maintained a list of 71 path literals; 10 of them do not
// exist on this board (CFDDelayT, CFDFraction, Gate*LengthT, GateOffsetT, WaveResolution,
// ChannelRealtime, ChannelSavedCount, TriggerFilterSelection, ADCInputBaselineAvg -- PSD-only
// names), so tab-completing them produced a write that could only fail, and only 61 of the
// board's 165 parameters were reachable at all. Deriving the list removes both failure modes
// and keeps it in step with the firmware tables by construction.
//
// Deliberately CLI-only: no broker opcode, so no broker restart. A restart costs a fresh
// open, which resets WaveTriggerSource on all 64 channels -- see AGENT.md §19/§21.
// Which scope directory a typed path prefix belongs to ("" if not a parameter path)
static std::string ScopeOfDir(const std::string& dir) {
  if (dir == "/par/")  return "dig";
  if (dir == "/cmd/")  return "cmd";
  if (dir == "/ch/0/par/" || dir == "/ch/0..63/par/") return "ch";
  if (dir.rfind("/ch/",0) == 0 && dir.size() > 4 && dir.compare(dir.size()-5,5,"/par/")==0) {
    // /ch/<digits>/par/  or /ch/<d>..<d>/par/
    std::string mid = dir.substr(4, dir.size()-9);
    bool ok = !mid.empty();
    for (char c : mid) if (!isdigit((unsigned char)c) && c != '.' && c != '-') ok = false;
    return ok ? "ch" : "";
  }
  if (dir.rfind("/lvds/",0)==0 && dir.compare(dir.size()-5,5,"/par/")==0) return "lvds";
  if (dir.rfind("/vga/",0)  ==0 && dir.compare(dir.size()-5,5,"/par/")==0) return "vga";
  if (dir.rfind("/group/",0)==0 && dir.compare(dir.size()-5,5,"/par/")==0) return "group";
  return "";
}

static std::string DirOf(const std::string& text) {
  size_t s = text.rfind('/');
  return (s == std::string::npos) ? std::string() : text.substr(0, s+1);
}

static bool NameMatches(const std::string& name, const std::string& leaf) {
  if (leaf.empty()) return true;
  if (name.size() < leaf.size()) return false;
  return strncasecmp(name.c_str(), leaf.c_str(), leaf.size()) == 0;
}

// stateful cursor for rl_completion_matches
static size_t g_iter = 0;
static std::vector<std::string>* g_list = nullptr;

static char* ListGenerator(const char* /*text*/, int state) {
  if (state == 0) g_iter = 0;
  if (!g_list) return nullptr;
  while (g_iter < g_list->size()) {
    // skip empties, hand out each match exactly once
    return strdup((*g_list)[g_iter++].c_str());
  }
  return nullptr;
}

static char** MatchList(std::vector<std::string>& list) {
  if (list.empty()) return nullptr;
  g_list = &list;
  char** r = rl_completion_matches("", ListGenerator);
  g_list = nullptr;
  return r;
}

// The board's own subnode names, readable through the SAME read path the completion offers --
// verified live: read 0 /ch/0/par/SelfTriggerWidth/setinrun -> "false".
static const char* subNodes[] = {
  "value","defaultvalue","datatype","accessmode","description","setinrun",
  "minvalue","maxvalue","increment","uom","expuom","allowedvalues","multiplevalue",
  nullptr
};

//^---------------- value completion

//^---------------- completion entry point

static char** CliCompletion(const char* text, int start, int /*end*/) {
  BuildParamTable();
  std::string lineSoFar(rl_line_buffer, start);
  auto tok = Tokenize(lineSoFar);
  bool freshWord = (start == 0) || (rl_line_buffer[start-1] == ' ');
  std::string w = text ? text : "";

  rl_attempted_completion_over = 1;   // never fall through to filename completion

  // ---- word 0: command names ----
  if (tok.empty()) {
    std::vector<std::string> out;
    for (int i = 0; commands[i]; i++)
      if (w.empty() || strncasecmp(commands[i], w.c_str(), w.size()) == 0) out.push_back(commands[i]);
    return MatchList(out);
  }

  const std::string c0 = tok[0];
  bool isRW = (c0 == "read" || c0 == "write" || c0 == "cmd" || c0 == "param");

  // ---- word 1: digitizer index ----
  if (isRW && tok.size() == 1 && freshWord) {
    std::vector<std::string> out;
    char buf[8];
    for (int i = 0; i < 4; i++) { snprintf(buf,sizeof buf,"%d",i); out.push_back(buf); }
    return MatchList(out);
  }

  // ---- value position: write <digi> <path> <value> ----
  if (c0 == "write" && tok.size() >= 3 && freshWord) {
    std::string dir  = DirOf(tok[2]);
    std::string leaf = tok[2].substr(dir.size());
    const ParamInfo* p = FindParam(leaf);
    std::vector<std::string> out;
    int digi = atoi(tok[1].c_str());
    // Authoritative source first: the board's own allowedvalues. Falls out empty when not
    // connected or when the parameter has no enum (the read fails), which is the point.
    for (const std::string& v : BoardValues(tok[2], digi))
      if (w.empty() || strncasecmp(v.c_str(), w.c_str(), w.size()) == 0) out.push_back(v);
    // Local answers as a fallback/offline hint, de-duplicated against the board's list.
    if (p && IsEnum(*p)) {
      for (const auto& a : p->answers) {
        if (w.size() && strncasecmp(a.first.c_str(), w.c_str(), w.size()) != 0) continue;
        bool seen = false;
        for (const std::string& v : out) if (!strcasecmp(v.c_str(), a.first.c_str())) seen = true;
        if (!seen) out.push_back(a.first);
      }
    }
    std::sort(out.begin(), out.end());
    return MatchList(out);   // empty -> nothing offered (numeric params)
  }

  // ---- parameter path position ----
  if (isRW) {
    // Split the typed text into (scope, dir-through-/par/, remainder-after-dir).
    // The remainder is "Name" when completing a name, or "Name/sub" when completing a
    // device-tree subnode. DirOf() alone cannot tell these apart -- "/ch/0/par/Name/" ends
    // in "Name/" and looks like an unknown scope, which is why the first cut of this
    // function silently offered nothing for subnode completion.
    std::string scope, dir, rest;
    static const char* flatDirs[] = { "/par/", "/cmd/", nullptr };
    for (int i = 0; flatDirs[i]; i++) {
      if (w.rfind(flatDirs[i], 0) == 0) { scope = ScopeOfDir(flatDirs[i]); dir = flatDirs[i]; rest = w.substr(dir.size()); break; }
    }
    if (scope.empty() && w.rfind("/", 0) == 0) {
      // /<scope>/<index>/par/<rest>   (index may be the 0..63 wildcard form)
      size_t p1 = w.find('/', 1);
      size_t p2 = (p1 == std::string::npos) ? std::string::npos : w.find('/', p1 + 1);
      size_t p3 = (p2 == std::string::npos) ? std::string::npos : w.find('/', p2 + 1);
      if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos &&
          w.compare(p2 + 1, p3 - p2 - 1, "par") == 0) {
        std::string sc = w.substr(1, p1 - 1);
        std::string idx = w.substr(p1 + 1, p2 - p1 - 1);
        bool idxOk = !idx.empty();
        for (char c : idx) if (!isdigit((unsigned char)c) && c != '.' && c != '-') idxOk = false;
        if (idxOk) {
          scope = (sc == "ch" ? "ch" : sc == "lvds" ? "lvds" : sc == "vga" ? "vga" :
                  sc == "group" ? "group" : "");
          if (!scope.empty()) { dir = w.substr(0, p3 + 1); rest = w.substr(p3 + 1); }
        }
      }
    }
    if (!w.empty() && w[0] != '/') { scope = "*"; dir = ""; rest = w; }   // bare word
    if (w.empty())                { scope = "*"; dir = ""; rest = ""; }
    if (w[0] == '/' && scope.empty()) return nullptr;                     // unknown path shape

    // subnode position: rest already contains a complete name, then "/", then a prefix
    size_t slash = rest.find('/');
    if (slash != std::string::npos) {
      const ParamInfo* p = FindParam(rest.substr(0, slash));
      if (!p) return nullptr;
      std::string pre = rest.substr(slash + 1);
      std::vector<std::string> out;
      for (int i = 0; subNodes[i]; i++)
        if (strncasecmp(subNodes[i], pre.c_str(), pre.size()) == 0)
          out.push_back(dir + rest.substr(0, slash) + "/" + subNodes[i]);
      return MatchList(out);
    }

    std::vector<std::string> out;
    for (const ParamInfo& p : g_params) {
      if (scope != "*" && p.scope != scope) continue;
      if (!NameMatches(p.name, rest)) continue;
      std::string pre = dir;
      if (pre.empty())
        pre = (p.scope=="ch")   ? "/ch/0/par/"  :
              (p.scope=="dig")  ? "/par/"       :
              (p.scope=="cmd")  ? "/cmd/"       :
              (p.scope=="lvds") ? "/lvds/0/par/":
              (p.scope=="vga")  ? "/vga/0/par/" : "/group/0/par/";
      out.push_back(pre + p.name);
    }
    if (dir.empty() && rest.empty()) {
      for (const char* d : {"/par/","/ch/0/par/","/ch/0..63/par/","/cmd/","/lvds/0/par/","/vga/0/par/","/group/0/par/"})
        out.push_back(d);
    } else if (!dir.empty() && rest.empty()) {
      out.insert(out.begin(), dir);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return MatchList(out);
  }

  // ---- "load" / "!" / settings files: filenames ----
  if (c0 == "load" || c0 == "!" || c0 == "save-settings" || c0 == "load-settings") {
    rl_attempted_completion_over = 0;
    return nullptr;
  }

  return nullptr;
}

//^============================================ Main

int main(int argc, char** argv) {
  printf("SOLARIS Broker CLI\n");
  printf("Type 'help' for commands, 'quit' to exit.\n\n");

  BrokerClient client;

  // Auto-connect if args provided
  if (argc > 1 && strcmp(argv[1], "--connect") == 0) {
    std::string host = (argc > 2) ? argv[2] : "localhost";
    int cmdPort = (argc > 3) ? atoi(argv[3]) : 5555;
    int pubPort = (argc > 4) ? atoi(argv[4]) : 5556;
    std::string cmdEp = "tcp://" + host + ":" + std::to_string(cmdPort);
    std::string pubEp = "tcp://" + host + ":" + std::to_string(pubPort);
    if (client.Connect(cmdEp, pubEp) == 0) {
      printf("Connected to %s\n", cmdEp.c_str());
    } else {
      printf("Failed to connect: %s\n", client.GetLastError().c_str());
    }
  }

  // Auto-load script if --script provided
  if (argc > 1 && strcmp(argv[1], "--script") == 0 && argc > 2) {
    LoadScript(argv[2], client);
  }

  // Ctrl+C handler: interrupts subscribe/sleep, clears line at prompt
  signal(SIGINT, SigIntHandler);

  // Setup readline tab completion
  g_client = &client;
  rl_attempted_completion_function = CliCompletion;

  // Load command history from file
  std::string historyFile = std::string(getenv("HOME") ? getenv("HOME") : ".") + "/.solaris_cli_history";
  read_history(historyFile.c_str());

  // Interactive loop with readline
  char* input;
  while (true) {
    gInterrupted = 0;
    input = readline("> ");
    if (!input) break; // EOF (Ctrl+D)
    if (gInterrupted) { printf("\n"); gInterrupted = 0; continue; } // Ctrl+C at prompt

    std::string line(input);
    free(input);

    if (line.empty()) continue;

    // Add to history (skip duplicates of the last entry)
    HIST_ENTRY* last = history_get(history_length);
    if (!last || line != last->line) {
      add_history(line.c_str());
    }

    int ret = ExecuteCommand(line, client, false);
    if (ret == 1) break;
  }

  // Append new commands to history file, then trim to 1000 lines
  append_history(history_length, historyFile.c_str());
  history_truncate_file(historyFile.c_str(), 1000);

  client.Disconnect();
  printf("Goodbye.\n");
  return 0;
}
