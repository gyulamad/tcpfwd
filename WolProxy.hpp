#pragma once

#include "TcpProxy.hpp"
#include "cpptools/misc/Executor.hpp"
#include "wol.hpp"

int ssh(const string& user, const string& host, const string& cmd) {

}

class WolProxy: public TcpProxy {
public:
    WolProxy(): TcpProxy() {}
    virtual ~WolProxy() {}

    void forward(int port, const string& host, int hport, const string& mac, const string& user, const string& cmd) {
        this->mac = mac;
        this->user = user;
        this->cmd = cmd;
        TcpProxy::forward(port, host, hport);
    }

    void onClientConnect(int fd, const string& addr) override {
        if (wol(mac, backendHost) && !cmd.empty())
            Executor::execute("ssh " + user + "@" + backendHost + " 'setsid " + cmd + " >/dev/null 2>&1 < /dev/null &'");
        TcpProxy::onClientConnect(fd, addr);
    }

protected:
    string mac;
    string user;
    string cmd;
};