#pragma once

#include <string>
#include "cpptools/misc/tpl_replace.hpp"
#include "cpptools/misc/Executor.hpp"
#include "ping.hpp"

using namespace std;

// returns true when woke up, false if already was awaik
inline bool wol(
    const string& mac, const string& addr, 
    const string& cmd_ping = "ping -c 1 -W 1 {{addr}}", 
    const string& cmd_wol = "wakeonlan -i {{addr}} {{mac}}",
    int retry = 100,
    function<void(int)> callback = [](int retry) {
        cout << "[WOL] retry " << retry << "..." << endl;
    }
) {
    if (!ping(addr, cmd_ping)) {
        Executor::execute(tpl_replace({
            { "{{mac}}", mac },
            { "{{addr}}", addr },
        }, cmd_wol));
        while (!ping(addr, cmd_ping)) {
            sleep(1);
            retry--;
            callback(retry);
        }
        return true;
    }
    return false;
}