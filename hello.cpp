#include "cpptools/misc/App.hpp"
#include "cpptools/misc/Arguments.hpp"
#include "cpptools/misc/ConsoleLogger.hpp"

#include "HelloServer.hpp"

using namespace std;

class HelloApplication: public App<ConsoleLogger, Arguments> {
public:
    using App::App;
    virtual ~HelloApplication() {}

    virtual int process() override {
        args.addHelp(1, "port", "Listening port number");
        int port = args.get<int>(1);
        HelloServer server;        
        server.listen(port);
        return 0;
    }
};

int main(int argc, char* argv[]) {
    return HelloApplication(argc, argv);
}
