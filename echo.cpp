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
        EchoServer s;
        s.listen(9090);
        return 0;
    }
};

int main(int argc, char* argv[]) {
    return EchoApplication(argc, argv);
}