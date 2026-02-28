#pragma once

#include "TcpServer.hpp"
#include <iostream>
#include <deque>
#include <unordered_map>
#include <chrono>

using namespace std;

// =============================================================================
// TcpEchoServer
//   Echoes every received message back to the sender at 1 character per second.
//   Pacing is owned here (not in TcpServer) so TcpServer stays generic.
//   Sending "shutdown" (as a full line) stops the server.
// =============================================================================
class TcpEchoServer: public TcpServer {
public:
    using TcpServer::TcpServer;
    virtual ~TcpEchoServer() {}

protected:
    void onServerStart(uint16_t port) override {
        cout << "[EchoServer] Listening on port " << port << "\n";
    }

    void onServerStop() override {
        cout << "[EchoServer] Stopped.\n";
    }

    void onClientConnect(int fd, const string& addr) override {
        cout << "[+] Client " << fd << " connected from " << addr << "\n";
        echoQueues[fd] = EchoState{};
    }

    void onClientMessage(int fd, const string& msg) override {
        cout << "[" << fd << "] received: \"" << msg << "\"\n";

        if (msg == "shutdown") {
            cout << "[EchoServer] Shutdown command received.\n";
            // Send "shutdown\n" immediately at full speed so the client unblocks
            sendToClient(fd, "shutdown\n");
            stop();
            return;
        }

        // Enqueue msg + newline into this client's drip queue
        auto& q = echoQueues[fd].pending;
        for (char c : msg) q.push_back(c);
        q.push_back('\n');
    }

    // onTick fires every ~N ms — emit at most one character per second
    // per client from their drip queue.
    void onTick() override {
        auto now = chrono::steady_clock::now();
        for (auto& [fd, state] : echoQueues) {
            if (state.pending.empty()) continue;
            auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                               now - state.lastSend).count();
            if (elapsed < 100) continue;
            char c = state.pending.front();
            state.pending.pop_front();
            state.lastSend = now;
            sendToClient(fd, string(1, c));
        }
    }

    void onClientDisconnect(int fd) override {
        cout << "[-] Client " << fd << " disconnected.\n";
        echoQueues.erase(fd);
    }

    void onClientError(int fd, const string& err) override {
        cerr << "[!] Client " << fd << " error: " << err << "\n";
        echoQueues.erase(fd);
    }

private:
    struct EchoState {
        deque<char> pending;
        chrono::steady_clock::time_point lastSend = chrono::steady_clock::now();
    };

    unordered_map<int, EchoState> echoQueues;
};
