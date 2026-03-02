#pragma once

#include "TcpProxy.hpp"
#include "wol.hpp"

class WolProxy: public TcpProxy {
public:
    WolProxy(const string& backendMac): backendMac(backendMac) {}
    virtual ~WolProxy() {}

    void onClientConnect(int fd, const string& addr) override {
        wol(backendMac, backendHost);
        TcpProxy::onClientConnect(fd, addr);
    }

protected:
    string backendMac;
};