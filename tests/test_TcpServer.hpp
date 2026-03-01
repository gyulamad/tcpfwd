#pragma once

#ifdef TEST

#include "../cpptools/misc/TEST.hpp"
#include "../cpptools/misc/capture_cout.hpp"
#include <thread>
#include "../EchoServer.hpp"
#include "../TcpClient.hpp"

using namespace std;

TEST(test_TcpServer_echo_single_client) {
    capture_cout([]() {
        EchoServer s;
        thread st([&s]() { s.listen(9090); });
        TcpClient c;
        c.connect("localhost", 9090);
        c.send("Hello!");
        while (!c.available()); // waiting for response...
        const string echo = c.read();
        c.send("shutdown"); // test tcp echo server must implement a shutdown command so that tests won't stuck...
        if (st.joinable()) st.join();
        assert(echo == "Hello!" && "Echo must be the same as original message");
    });
}

TEST(test_TcpServer_echo_multi_client) {
    capture_cout([]() {
        EchoServer s;
        thread st([&s]() { s.listen(9090); });

        TcpClient c1, c2, c3;
        string echo1, echo2, echo3;
        double elapsed1, elapsed2, elapsed3;

        thread c1t([&]() {
            Stopper stopper;
            c1.connect("localhost", 9090);
            c1.send("Hello1");
            while (!c1.available()); // waiting for response...
            echo1 = c1.read();
            elapsed1 = stopper.stop();            
        });
        thread c2t([&]() {
            Stopper stopper;
            c2.connect("localhost", 9090);
            c2.send("Hello2");
            while (!c2.available()); // waiting for response...
            echo2 = c2.read();
            elapsed2 = stopper.stop();
        });
        thread c3t([&]() {
            Stopper stopper;
            c3.connect("localhost", 9090);
            c3.send("Hello3");
            while (!c3.available()); // waiting for response...
            echo3 = c3.read();
            elapsed3 = stopper.stop();
        });
        
        if (c1t.joinable()) c1t.join();
        if (c2t.joinable()) c2t.join();
        if (c3t.joinable()) c3t.join();
        
        c1.send("shutdown"); // test tcp echo server must implement a shutdown command so that tests won't stuck...
        if (st.joinable()) st.join();
        
        assert(echo1 == "Hello1" && "Echo must be the same as original message");
        assert(echo2 == "Hello2" && "Echo must be the same as original message");
        assert(echo3 == "Hello3" && "Echo must be the same as original message");
        assert(abs(elapsed1 - elapsed2) < 100 && "Everything must happend in the same time");
        assert(abs(elapsed1 - elapsed3) < 100 && "Everything must happend in the same time");
    });
}

#endif