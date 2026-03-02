#include "cpptools/misc/App.hpp"
#include "cpptools/misc/Arguments.hpp"
#include "cpptools/misc/ConsoleLogger.hpp"

#include "WolProxy.hpp"
#include "cpptools/misc/explode.hpp"

using namespace std;

class WolProxyApplication: public App<ConsoleLogger, Arguments> {
public:
    using App::App;
    virtual ~WolProxyApplication() {}

    virtual int process() override {
        args.addHelp(1, "port", "Listening port number");
        args.addHelp(2, "address", "Server addres:port for forwarding");
        args.addHelp(3, "mac", "Server MAC addres");
        int port = args.get<int>(1);
        vector<string> addr = trim(explode(":", args.get<string>(2)));
        if (addr.size() != 2)
            throw ERROR("Invalid address format. Use <host>:<port>");
        string mac = args.get<string>(3);
        WolProxy s(mac);
        s.forward(port, addr[0], parse<int>(addr[1]));
        return 0;
    }
};

int main(int argc, char* argv[]) {
    return WolProxyApplication(argc, argv);
}