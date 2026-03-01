#include "cpptools/misc/App.hpp"
#include "cpptools/misc/Arguments.hpp"
#include "cpptools/misc/ConsoleLogger.hpp"

using namespace std;

class HelloApplication: public App<ConsoleLogger, Arguments> {
public:
    using App::App;
    virtual ~HelloApplication() {}

    virtual int process() override {
        return 0;
    }
};

int main(int argc, char* argv[]) {
    return HelloApplication(argc, argv);
}