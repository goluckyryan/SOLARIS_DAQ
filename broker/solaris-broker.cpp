#include "BrokerServer.h"

#include <cstdio>
#include <cstring>
#include <csignal>
#include <string>
#include <vector>
#include <sstream>

static BrokerServer* gServer = nullptr;

static void SignalHandler(int sig) {
  (void)sig;
  printf("\nReceived signal %d, shutting down...\n", sig);
  if (gServer) gServer->Stop();
}

static void PrintUsage(const char* prog) {
  printf("Usage: %s [options]\n", prog);
  printf("Options:\n");
  printf("  --ip <ip1,ip2,...>     Comma-separated digitizer IPs to open on startup\n");
  printf("  --cmd-port <port>      Command (REQ/REP) port [default: 5555]\n");
  printf("  --pub-port <port>      Publish (PUB/SUB) port [default: 5556]\n");
  printf("  --scalar-interval <s>  Scalar broadcast interval in seconds [default: 2.0]\n");
  printf("  --help                 Show this help\n");
  printf("\nExample:\n");
  printf("  %s --ip 192.168.0.254,192.168.0.253\n", prog);
  printf("  %s --ip 192.168.0.254 --cmd-port 6000 --pub-port 6001\n", prog);
}

static std::vector<std::string> SplitComma(const std::string& s) {
  std::vector<std::string> tokens;
  std::istringstream iss(s);
  std::string token;
  while (std::getline(iss, token, ',')) {
    // Trim whitespace
    size_t start = token.find_first_not_of(" \t");
    size_t end = token.find_last_not_of(" \t");
    if (start != std::string::npos) {
      tokens.push_back(token.substr(start, end - start + 1));
    }
  }
  return tokens;
}

int main(int argc, char** argv) {
  std::vector<std::string> ips;
  int cmdPort = 5555;
  int pubPort = 5556;
  float scalarInterval = 2.0f;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--ip") == 0 && i + 1 < argc) {
      ips = SplitComma(argv[++i]);
    } else if (strcmp(argv[i], "--cmd-port") == 0 && i + 1 < argc) {
      cmdPort = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--pub-port") == 0 && i + 1 < argc) {
      pubPort = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--scalar-interval") == 0 && i + 1 < argc) {
      scalarInterval = atof(argv[++i]);
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage(argv[0]);
      return 0;
    } else {
      printf("Unknown option: %s\n", argv[i]);
      PrintUsage(argv[0]);
      return 1;
    }
  }

  printf("=== SOLARIS Digitizer Broker ===\n");
  printf("Command port: %d\n", cmdPort);
  printf("Publish port: %d\n", pubPort);
  printf("Scalar interval: %.1f s\n", scalarInterval);
  if (!ips.empty()) {
    printf("Digitizer IPs: ");
    for (size_t i = 0; i < ips.size(); i++) {
      printf("%s%s", ips[i].c_str(), (i + 1 < ips.size()) ? ", " : "\n");
    }
  }
  printf("\n");

  BrokerServer server;
  gServer = &server;

  server.SetCommandEndpoint("tcp://*:" + std::to_string(cmdPort));
  server.SetPublishEndpoint("tcp://*:" + std::to_string(pubPort));
  server.SetScalarInterval(scalarInterval);

  if (server.Start() != 0) {
    printf("ERROR: failed to start broker\n");
    return 1;
  }

  // Open digitizers from command line
  for (const auto& ip : ips) {
    std::string url = "dig2://" + ip;
    printf("Opening digitizer at %s ...\n", url.c_str());
    int idx = server.OpenDigitizer(url);
    if (idx < 0) {
      printf("WARNING: failed to open digitizer at %s, continuing...\n", ip.c_str());
    }
  }

  // Install signal handlers
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  // Run main loop (blocks until Stop() is called)
  server.Run();

  gServer = nullptr;
  printf("Broker exited cleanly.\n");
  return 0;
}
