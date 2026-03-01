#include "cpptools/misc/App.hpp"
#include "cpptools/misc/Arguments.hpp"
#include "cpptools/misc/ConsoleLogger.hpp"

#include "TcpProxy.hpp"
#include "cpptools/misc/explode.hpp"

using namespace std;

class TcpProxyApplication: public App<ConsoleLogger, Arguments> {
public:
    using App::App;
    virtual ~TcpProxyApplication() {}

    virtual int process() override {
        args.addHelp(1, "port", "Listening port number");
        args.addHelp(2, "address", "Server addres:port for forwarding");
        int port = args.get<int>(1);
        vector<string> addr = trim(explode(":", args.get<string>(2)));
        if (addr.size() != 2)
            throw ERROR("Invalid address format. Use <host>:<port>");
        TcpProxy s;
        s.forward(port, addr[0], parse<int>(addr[1]));
        return 0;
    }
};

int main(int argc, char* argv[]) {
    return TcpProxyApplication(argc, argv);
}