#include "cpptools/misc/App.hpp"
#include "cpptools/misc/Arguments.hpp"
#include "cpptools/misc/ConsoleLogger.hpp"

#include "TcpEchoServer.hpp"

using namespace std;

class TcpEchoApplication: public App<ConsoleLogger, Arguments> {
public:
    using App::App;
    virtual ~TcpEchoApplication() {}

    virtual int process() override {
        TcpEchoServer s;
        s.listen(9090);
        return 0;
    }
};

int main(int argc, char* argv[]) {
    return TcpEchoApplication(argc, argv);
}