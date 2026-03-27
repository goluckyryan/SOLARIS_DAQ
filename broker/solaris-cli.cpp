#include "BrokerClient.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>

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
  printf("  subscribe [seconds]                    Print scalar updates for N seconds (default 10)\n");
  printf("  shutdown                               Shutdown broker\n");
  printf("  help                                   Show this help\n");
  printf("  quit / exit                            Exit CLI\n");
}

static std::vector<std::string> Tokenize(const std::string& line) {
  std::vector<std::string> tokens;
  std::istringstream iss(line);
  std::string token;
  while (iss >> token) tokens.push_back(token);
  return tokens;
}

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

  std::string line;
  while (true) {
    printf("> ");
    fflush(stdout);
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;

    auto tok = Tokenize(line);
    if (tok.empty()) continue;
    const std::string& cmd = tok[0];

    if (cmd == "quit" || cmd == "exit") {
      break;
    }

    if (cmd == "help") {
      PrintHelp();
      continue;
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
      continue;
    }

    if (cmd == "disconnect") {
      client.Disconnect();
      printf("Disconnected.\n");
      continue;
    }

    if (!client.IsConnected()) {
      printf("Not connected. Use 'connect' first.\n");
      continue;
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
      if (tok.size() < 2) { printf("Usage: info <digi>\n"); continue; }
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
      if (tok.size() < 2) { printf("Usage: open <url>\n"); continue; }
      int idx = client.OpenDigitizer(tok[1]);
      if (idx >= 0) {
        printf("Opened as digitizer %d\n", idx);
      } else {
        printf("Failed: %s\n", client.GetLastError().c_str());
      }
    }
    else if (cmd == "close") {
      if (tok.size() < 2) { printf("Usage: close <digi>\n"); continue; }
      client.CloseDigitizer(atoi(tok[1].c_str()));
      printf("OK\n");
    }
    else if (cmd == "read") {
      if (tok.size() < 3) { printf("Usage: read <digi> <param>\n"); continue; }
      std::string val = client.ReadValue(atoi(tok[1].c_str()), tok[2]);
      if (!val.empty()) {
        printf("%s\n", val.c_str());
      } else {
        printf("Error: %s\n", client.GetLastError().c_str());
      }
    }
    else if (cmd == "write") {
      if (tok.size() < 4) { printf("Usage: write <digi> <param> <value>\n"); continue; }
      if (client.WriteValue(atoi(tok[1].c_str()), tok[2], tok[3])) {
        printf("OK\n");
      } else {
        printf("Failed: %s\n", client.GetLastError().c_str());
      }
    }
    else if (cmd == "cmd") {
      if (tok.size() < 3) { printf("Usage: cmd <digi> <command>\n"); continue; }
      client.SendCommand(atoi(tok[1].c_str()), tok[2]);
      printf("OK\n");
    }
    else if (cmd == "format") {
      if (tok.size() < 3) { printf("Usage: format <digi> <fmt>\n"); continue; }
      client.SetDataFormat(atoi(tok[1].c_str()), atoi(tok[2].c_str()));
      printf("OK\n");
    }
    else if (cmd == "start") {
      if (tok.size() < 3) { printf("Usage: start <digi|all> <fmt> [save <path>]\n"); continue; }
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
      if (tok.size() < 2) { printf("Usage: stop <digi|all>\n"); continue; }
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
      if (tok.size() < 2) { printf("Usage: file-status <digi>\n"); continue; }
      auto fs = client.GetFileStatus(atoi(tok[1].c_str()));
      printf("Total: %.2f MB, Current file: %.2f MB\n",
             fs.totalFileSize / 1048576.0, fs.currentFileSize / 1048576.0);
    }
    else if (cmd == "save-settings") {
      if (tok.size() < 2) { printf("Usage: save-settings <digi> [file]\n"); continue; }
      std::string fn = (tok.size() > 2) ? tok[2] : "";
      client.SaveSettingsFile(atoi(tok[1].c_str()), fn);
      printf("OK\n");
    }
    else if (cmd == "load-settings") {
      if (tok.size() < 2) { printf("Usage: load-settings <digi> [file]\n"); continue; }
      std::string fn = (tok.size() > 2) ? tok[2] : "";
      client.LoadSettingsFile(atoi(tok[1].c_str()), fn);
      printf("OK\n");
    }
    else if (cmd == "subscribe") {
      int seconds = (tok.size() > 1) ? atoi(tok[1].c_str()) : 10;
      printf("Listening for scalar updates for %d seconds...\n", seconds);

      // Set up callback to print scalars
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

      std::this_thread::sleep_for(std::chrono::seconds(seconds));

      client.onScalarUpdate = nullptr;
      client.onLogMessage = nullptr;
      client.onStatusChange = nullptr;
      printf("\nSubscription ended.\n");
    }
    else if (cmd == "shutdown") {
      printf("Sending shutdown to broker...\n");
      client.Shutdown();
      printf("Done.\n");
    }
    else {
      printf("Unknown command: %s (type 'help')\n", cmd.c_str());
    }
  }

  client.Disconnect();
  printf("Goodbye.\n");
  return 0;
}
