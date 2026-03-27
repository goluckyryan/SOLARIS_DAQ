#include "BrokerClient.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
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
  printf("  write <digi> <param> <value>           Write parameter\n");
  printf("  cmd <digi> <command>                   Send command\n");
  printf("  format <digi> <fmt>                    Set data format (0=ALL,1=OneTrace,2=NoTrace,...)\n");
  printf("  start <digi|all> <fmt> [save <path>]   Start ACQ\n");
  printf("  stop <digi|all>                        Stop ACQ\n");
  printf("  status                                 Show ACQ status\n");
  printf("  file-status <digi>                     Show file sizes\n");
  printf("  save-settings <digi> [file]            Save settings to file\n");
  printf("  load-settings <digi> [file]            Load settings from file\n");
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
  else if (cmd == "write") {
    if (tok.size() < 4) { printf("Usage: write <digi> <param> <value>\n"); return 0; }
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
  "read", "write", "cmd", "format", "start", "stop", "status",
  "file-status", "save-settings", "load-settings", "subscribe",
  "load", "sleep", "shutdown", "help", "quit", "exit", nullptr
};

// Common CAEN parameter paths for read/write completion
static const char* paramPaths[] = {
  "/par/SerialNum", "/par/ModelName", "/par/FwType", "/par/NumCh",
  "/par/ADC_SamplRate", "/par/ClockSource", "/par/StartSource",
  "/par/GlobalTriggerSource", "/par/TrgOutMode", "/par/GPIOMode",
  "/par/BusyInSource", "/par/SyncOutMode", "/par/BoardVetoSource",
  "/par/RunDelay", "/par/IOlevel", "/par/EnAutoDisarmAcq",
  "/par/EnStatEvents", "/par/BoardVetoWidth",
  "/par/DACoutMode", "/par/DACoutChSelect",
  "/par/TestPulsePeriod", "/par/TestPulseWidth",
  "/par/TestPulseLowLevel", "/par/TestPulseHighLevel",
  "/par/EnClockOutFP", "/par/VolatileClockOutDelay",
  "/par/PermanentClockOutDelay",
  "/par/ITLAMainLogic", "/par/ITLAMajorityLev", "/par/ITLAPairLogic",
  "/par/ITLAPolarity", "/par/ITLAGateWidth",
  "/par/ITLBMainLogic", "/par/ITLBMajorityLev", "/par/ITLBPairLogic",
  "/par/ITLBPolarity", "/par/ITLBGateWidth",
  "/ch/0/par/ChEnable", "/ch/0/par/DCOffset", "/ch/0/par/TriggerThr",
  "/ch/0/par/SelfTrgRate", "/ch/0/par/ChannelRealtime",
  "/ch/0/par/ChannelSavedCount", "/ch/0/par/PulsePolarity",
  "/ch/0/par/WaveDataSource", "/ch/0/par/ChRecordLengthT",
  "/ch/0/par/ChPreTriggerT", "/ch/0/par/WaveSaving",
  "/ch/0/par/WaveResolution", "/ch/0/par/EventTriggerSource",
  "/ch/0/par/WaveTriggerSource",
  "/ch/0/par/TimeFilterRiseTimeT", "/ch/0/par/TimeFilterRetriggerGuardT",
  "/ch/0/par/EnergyFilterRiseTimeT", "/ch/0/par/EnergyFilterFlatTopT",
  "/ch/0/par/EnergyFilterPoleZeroT", "/ch/0/par/EnergyFilterPeakingPosition",
  "/ch/0/par/EnergyFilterBaselineAvg", "/ch/0/par/EnergyFilterFineGain",
  "/ch/0/par/GateLongLengthT", "/ch/0/par/GateShortLengthT",
  "/ch/0/par/GateOffsetT", "/ch/0/par/CFDDelayT", "/ch/0/par/CFDFraction",
  "/ch/0/par/TriggerFilterSelection", "/ch/0/par/ADCInputBaselineAvg",
  "/cmd/Reset", "/cmd/armacquisition", "/cmd/swstartacquisition",
  "/cmd/SwStopAcquisition", "/cmd/disarmacquisition",
  nullptr
};

static char* CommandGenerator(const char* text, int state) {
  static int index;
  if (state == 0) index = 0;
  while (commands[index]) {
    const char* cmd = commands[index++];
    if (strncmp(cmd, text, strlen(text)) == 0) {
      return strdup(cmd);
    }
  }
  return nullptr;
}

static char* ParamGenerator(const char* text, int state) {
  static int index;
  if (state == 0) index = 0;
  while (paramPaths[index]) {
    const char* p = paramPaths[index++];
    if (strncmp(p, text, strlen(text)) == 0) {
      return strdup(p);
    }
  }
  return nullptr;
}

static char** CliCompletion(const char* text, int start, int /*end*/) {
  // Figure out which word we're completing
  std::string lineSoFar(rl_line_buffer, start);
  auto tok = Tokenize(lineSoFar);

  if (tok.empty()) {
    // First word: complete command names
    return rl_completion_matches(text, CommandGenerator);
  }

  // Second or third word after read/write/cmd: complete parameter paths
  if (tok[0] == "read" || tok[0] == "write" || tok[0] == "cmd") {
    if (text[0] == '/' || tok.size() >= 2) {
      return rl_completion_matches(text, ParamGenerator);
    }
  }

  // After "load": fall through to default filename completion
  if (tok[0] == "load") {
    return nullptr; // use readline's default filename completion
  }

  // After "!": fall through to default filename completion for shell commands
  if (tok[0] == "!") {
    return nullptr;
  }

  // No matches -- suppress default filename completion for other commands
  rl_attempted_completion_over = 1;
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
