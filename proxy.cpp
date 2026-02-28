#include "cpptools/misc/App.hpp"
#include "cpptools/misc/Arguments.hpp"
#include "cpptools/misc/ConsoleLogger.hpp"

using namespace std;

class TcpProxyApplication: public App<ConsoleLogger, Arguments> {
public:
    using App::App;
    virtual ~TcpProxyApplication() {}

    virtual int process() override {
        return 0;
    }
};

int main(int argc, char* argv[]) {
    return TcpProxyApplication(argc, argv);
}