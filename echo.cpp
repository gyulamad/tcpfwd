#include "cpptools/misc/App.hpp"
#include "cpptools/misc/Arguments.hpp"
#include "cpptools/misc/ConsoleLogger.hpp"

#include "EchoServer.hpp"

using namespace std;

class EchoApplication: public App<ConsoleLogger, Arguments> {
public:
    using App::App;
    virtual ~EchoApplication() {}

    virtual int process() override {
        args.addHelp(1, "port", "Listening port number");
        int port = args.get<int>(1);
        EchoServer s;
        s.listen(port);
        return 0;
    }
};

int main(int argc, char* argv[]) {
    return EchoApplication(argc, argv);
}