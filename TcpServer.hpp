#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <cstring>
#include <cerrno>
#include <atomic>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "cpptools/misc/ERROR.hpp"

using namespace std;

// =============================================================================
// TcpServer
//   - Non-blocking, select()-based, single-threaded event loop
//   - Subclass and override the virtual hooks to implement your server logic
// =============================================================================
class TcpServer {
public:
    TcpServer(): listenFd(-1), running(false) {}
    virtual ~TcpServer() { stop(); }

    // Start the server: bind, listen, enter event loop (blocks until stop())
    void listen(uint16_t port) {
        setupListenSocket(port);
        running = true;
        onServerStart(port);
        eventLoop();
        onServerStop();
    }

    void stop() { running = false; }

protected:
    // == Override these hooks in your subclass =================================

    virtual void onServerStart(uint16_t /*port*/) {}
    virtual void onServerStop() {}

    // fd         : file descriptor identifying this client (unique per connection)
    // remoteAddr : "ip:port" string
    virtual void onClientConnect(int /*fd*/, const string& /*remoteAddr*/) {}

    // message : one complete line (newline stripped)
    virtual void onClientMessage(int /*fd*/, const string& /*message*/) {}

    virtual void onClientDisconnect(int /*fd*/) {}
    virtual void onClientError(int /*fd*/, const string& /*error*/) {}

    // Called every ~100 ms regardless of activity (useful for timers, etc.)
    virtual void onTick() {}

    // == Utilities available to subclasses =====================================

    // Queue data to be sent to a client (rate-limited by the server's tick)
    void sendToClient(int fd, const string& data) {
        auto it = clients.find(fd);
        if (it != clients.end())
            it->second.sendQueue.insert(
                it->second.sendQueue.end(), data.begin(), data.end()
            );
    }

    void disconnectClient(int fd) {
        auto it = clients.find(fd);
        if (it == clients.end()) return;
        onClientDisconnect(fd);
        ::close(fd);
        clients.erase(it);
    }

private:
    struct ClientState {
        string remoteAddr;
        string recvBuf;
        deque<char> sendQueue;
    };

    int listenFd;
    atomic<bool> running;
    unordered_map<int, ClientState> clients;

    // == Socket setup ==========================================================
    void setupListenSocket(uint16_t port) {
        listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) throw ERROR("socket(): " + errStr());

        int opt = 1;
        ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setNonBlocking(listenFd);

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port        = htons(port);

        if (::bind(listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            throw ERROR("bind(): " + errStr());

        if (::listen(listenFd, SOMAXCONN) < 0)
            throw ERROR("listen(): " + errStr());
    }

    // == Main event loop =======================================================
    void eventLoop() {
        while (running) {
            fd_set readSet, writeSet;
            FD_ZERO(&readSet);
            FD_ZERO(&writeSet);

            FD_SET(listenFd, &readSet);
            int maxFd = listenFd;

            for (auto& [fd, st] : clients) {
                FD_SET(fd, &readSet);
                if (!st.sendQueue.empty())
                    FD_SET(fd, &writeSet);
                if (fd > maxFd) maxFd = fd;
            }

            // Wake up every N ms so rate-limiter and onTick() fire regularly
            timeval tv{0, 100'000};
            int ready = ::select(maxFd + 1, &readSet, &writeSet, nullptr, &tv);
            if (ready < 0) {
                if (errno == EINTR) continue;
                throw ERROR("select(): " + errStr());
            }

            onTick();

            if (FD_ISSET(listenFd, &readSet))
                acceptClients();

            // Snapshot fd list — callbacks may add/remove clients
            vector<int> fds;
            fds.reserve(clients.size());
            for (auto& [fd, _] : clients) fds.push_back(fd);

            for (int fd : fds) {
                if (!clients.count(fd)) continue;
                if (FD_ISSET(fd, &readSet))  handleRead(fd);
                if (!clients.count(fd)) continue;
                if (FD_ISSET(fd, &writeSet)) handleWrite(fd);
            }
        }

        for (auto& [fd, _] : clients) ::close(fd);
        clients.clear();
        if (listenFd >= 0) { ::close(listenFd); listenFd = -1; }
    }

    void acceptClients() {
        while (true) {
            sockaddr_in addr{};
            socklen_t len = sizeof(addr);
            int cfd = ::accept(listenFd, reinterpret_cast<sockaddr*>(&addr), &len);
            if (cfd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                onClientError(-1, "accept(): " + errStr());
                break;
            }
            setNonBlocking(cfd);

            char ipBuf[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &addr.sin_addr, ipBuf, sizeof(ipBuf));
            string remote = string(ipBuf) + ":" +
                                 to_string(ntohs(addr.sin_port));

            clients[cfd] = ClientState{remote, {}, {}};
            onClientConnect(cfd, remote);
        }
    }

    void handleRead(int fd) {
        char buf[4096];
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            onClientError(fd, "recv(): " + errStr());
            disconnectClient(fd);
            return;
        }
        if (n == 0) { disconnectClient(fd); return; }

        auto& st = clients[fd];
        st.recvBuf.append(buf, n);

        size_t pos;
        while ((pos = st.recvBuf.find('\n')) != string::npos) {
            string msg = st.recvBuf.substr(0, pos);
            if (!msg.empty() && msg.back() == '\r') msg.pop_back();
            st.recvBuf.erase(0, pos + 1);
            onClientMessage(fd, msg);
        }
    }

    void handleWrite(int fd) {
        auto& st = clients[fd];
        if (st.sendQueue.empty()) return;

        // Send as much as the socket will accept in one go
        // (callers control pacing by how much they enqueue via sendToClient)
        string buf(st.sendQueue.begin(), st.sendQueue.end());
        ssize_t n = ::send(fd, buf.data(), buf.size(), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            onClientError(fd, "send(): " + errStr());
            disconnectClient(fd);
            return;
        }
        st.sendQueue.erase(st.sendQueue.begin(), st.sendQueue.begin() + n);
    }

    static void setNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    static string errStr() { return strerror(errno); }
};
