#include "../cpptools/misc/TEST.hpp"
#include "../cpptools/misc/ConsoleLogger.hpp"

#ifdef TEST
#include "test_TcpServer.hpp"
#endif // TEST

int main(int argc, char** argv) {
    createLogger<ConsoleLogger>();
    Arguments args(argc, argv);
    tester.run(args);
}
