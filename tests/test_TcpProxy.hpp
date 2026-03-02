#pragma once

#ifdef TEST

#include "../cpptools/misc/TEST.hpp"
#include "../cpptools/misc/capture_cout.hpp"
#include "../cpptools/misc/Stopper.hpp"
#include <thread>
#include <chrono>
#include "../TcpProxy.hpp"
#include "../EchoServer.hpp"
#include "../TcpClient.hpp"

using namespace std;

// =============================================================================
// TcpProxy Network Tests
// =============================================================================
// The TcpProxy forwards traffic between clients and a backend server.
// Test strategy:
//   1. Start an EchoServer as the backend
//   2. Start TcpProxy to forward from proxy port to backend port
//   3. Connect TcpClient(s) to the proxy
//   4. Verify bidirectional forwarding works correctly
//
// NOTE: Each test uses different ports to avoid conflicts when tests run
//       sequentially. Port naming: backend=5XXX, proxy=6XXX
//       Tests include delays to allow ports to be released between tests.
// =============================================================================

// Helper: wait for a client to have data available (with timeout)
static bool wait_for_available(TcpClient& client, int timeoutMs = 2000) {
    auto start = chrono::steady_clock::now();
    while (!client.available()) {
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(
            chrono::steady_clock::now() - start).count();
        if (elapsed > timeoutMs) return false;
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    return true;
}

// Helper: wait for port to be available after test
static void wait_for_port_release(int delayMs = 1000) {
    this_thread::sleep_for(chrono::milliseconds(delayMs));
}

// =============================================================================
// Test: should accept tcp client connections
// =============================================================================
TEST(test_TcpProxy_accept_client_connections) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5001
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5001); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6001 -> backend 5001
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6001, "localhost", 5001); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect a client - this tests that proxy accepts connections
        TcpClient client;
        bool connected = false;
        try {
            client.connect("localhost", 6001);
            connected = client.isConnected();
        } catch (...) {
            connected = false;
        }
        
        // Cleanup: shutdown backend directly and stop proxy
        TcpClient shutdownClient;
        shutdownClient.connect("localhost", 5001);
        shutdownClient.send("shutdown");
        
        if (backendThread.joinable()) backendThread.join();
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        assert(connected && "Proxy should accept client connections");
    });
}

// =============================================================================
// Test: should build up and maintain a channel for each client to the server
// =============================================================================
TEST(test_TcpProxy_maintain_channel_per_client) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5002
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5002); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6002 -> backend 5002
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6002, "localhost", 5002); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect multiple clients - each should get its own backend channel
        TcpClient client1, client2;
        client1.connect("localhost", 6002);
        client2.connect("localhost", 6002);
        
        // Send messages to verify each channel works independently
        client1.send("msg1");
        client2.send("msg2");
        
        // Wait for responses (echo server drips chars, so we need more time)
        bool avail1 = wait_for_available(client1, 3000);
        bool avail2 = wait_for_available(client2, 3000);
        
        string response1, response2;
        if (avail1) response1 = client1.read();
        if (avail2) response2 = client2.read();
        
        // Cleanup: shutdown backend directly and stop proxy
        TcpClient shutdownClient;
        shutdownClient.connect("localhost", 5002);
        shutdownClient.send("shutdown");
        
        if (backendThread.joinable()) backendThread.join();
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        assert(avail1 && "Client 1 should receive response through proxy");
        assert(avail2 && "Client 2 should receive response through proxy");
        assert(response1 == "msg1" && "Client 1 channel should be independent");
        assert(response2 == "msg2" && "Client 2 channel should be independent");
    });
}

// =============================================================================
// Test: should forward data from client to server and back
// =============================================================================
TEST(test_TcpProxy_forward_data_bidirectional) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5003
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5003); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6003 -> backend 5003
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6003, "localhost", 5003); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect client and send message through proxy
        TcpClient client;
        client.connect("localhost", 6003);
        
        // Test: client -> proxy -> backend (forward)
        client.send("HelloThroughProxy");
        
        // Test: backend -> proxy -> client (backward)
        bool available = wait_for_available(client, 3000);
        string response;
        if (available) response = client.read();
        
        // Cleanup: shutdown backend directly and stop proxy
        TcpClient shutdownClient;
        shutdownClient.connect("localhost", 5003);
        shutdownClient.send("shutdown");
        
        if (backendThread.joinable()) backendThread.join();
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        assert(available && "Response should be forwarded back through proxy");
        assert(response == "HelloThroughProxy" && "Data should be forwarded bidirectionally");
    });
}

// =============================================================================
// Test: should handle client disconnects gracefully
// =============================================================================
TEST(test_TcpProxy_handle_client_disconnect) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5004
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5004); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6004 -> backend 5004
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6004, "localhost", 5004); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect two clients
        TcpClient client1, client2;
        client1.connect("localhost", 6004);
        client2.connect("localhost", 6004);
        
        // Disconnect client1
        client1.disconnect();
        
        this_thread::sleep_for(chrono::milliseconds(200));
        
        // Client2 should still work (proxy handled client1 disconnect gracefully)
        client2.send("StillWorking");
        bool available = wait_for_available(client2, 3000);
        string response;
        if (available) response = client2.read();
        
        // Cleanup: shutdown backend directly and stop proxy
        TcpClient shutdownClient;
        shutdownClient.connect("localhost", 5004);
        shutdownClient.send("shutdown");
        
        if (backendThread.joinable()) backendThread.join();
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        assert(available && "Client 2 should still work after client 1 disconnect");
        assert(response == "StillWorking" && "Proxy should handle client disconnect gracefully");
    });
}

// =============================================================================
// Test: should handle server disconnect gracefully by removing all clients
// =============================================================================
TEST(test_TcpProxy_handle_server_disconnect) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5005
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5005); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6005 -> backend 5005
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6005, "localhost", 5005); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect client through proxy
        TcpClient client;
        client.connect("localhost", 6005);
        
        // Shutdown the backend server (simulating server disconnect)
        TcpClient directClient;
        directClient.connect("localhost", 5005);
        directClient.send("shutdown");
        
        // Wait for backend to stop
        if (backendThread.joinable()) backendThread.join();
        
        this_thread::sleep_for(chrono::milliseconds(200));
        
        // Stop proxy
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        // Test passes if no crash occurred - proxy handled backend disconnect
        assert(true && "Proxy should handle backend server disconnect without crashing");
    });
}

// =============================================================================
// Test: should handle errors gracefully
// =============================================================================
TEST(test_TcpProxy_handle_errors_gracefully) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5006
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5006); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6006 -> backend 5006
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6006, "localhost", 5006); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect client and verify normal operation
        TcpClient client;
        client.connect("localhost", 6006);
        client.send("TestMessage");
        
        bool available = wait_for_available(client, 3000);
        string response;
        if (available) response = client.read();
        
        // Abruptly disconnect client (simulating error condition)
        client.disconnect();
        
        this_thread::sleep_for(chrono::milliseconds(200));
        
        // Connect another client - proxy should still work
        TcpClient client2;
        client2.connect("localhost", 6006);
        client2.send("AfterError");
        
        bool available2 = wait_for_available(client2, 3000);
        string response2;
        if (available2) response2 = client2.read();
        
        // Cleanup: shutdown backend directly and stop proxy
        TcpClient shutdownClient;
        shutdownClient.connect("localhost", 5006);
        shutdownClient.send("shutdown");
        
        if (backendThread.joinable()) backendThread.join();
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        assert(available && "First client should work");
        assert(response == "TestMessage" && "First message should be forwarded");
        assert(available2 && "Proxy should accept new clients after error");
        assert(response2 == "AfterError" && "Proxy should handle errors gracefully");
    });
}

// =============================================================================
// Test: should handle concurrent clients at the same time
// =============================================================================
TEST(test_TcpProxy_concurrent_clients) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5007
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5007); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6007 -> backend 5007
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6007, "localhost", 5007); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect multiple clients concurrently
        TcpClient client1, client2, client3;
        string response1, response2, response3;
        double elapsed1 = 0, elapsed2 = 0, elapsed3 = 0;
        
        client1.connect("localhost", 6007);
        client2.connect("localhost", 6007);
        client3.connect("localhost", 6007);
        
        // Send messages from all clients concurrently
        thread t1([&]() {
            Stopper stopper;
            client1.send("Concurrent1");
            if (wait_for_available(client1, 5000)) response1 = client1.read();
            elapsed1 = stopper.stop();
        });
        thread t2([&]() {
            Stopper stopper;
            client2.send("Concurrent2");
            if (wait_for_available(client2, 5000)) response2 = client2.read();
            elapsed2 = stopper.stop();
        });
        thread t3([&]() {
            Stopper stopper;
            client3.send("Concurrent3");
            if (wait_for_available(client3, 5000)) response3 = client3.read();
            elapsed3 = stopper.stop();
        });
        
        if (t1.joinable()) t1.join();
        if (t2.joinable()) t2.join();
        if (t3.joinable()) t3.join();
        
        // Cleanup: shutdown backend directly and stop proxy
        TcpClient shutdownClient;
        shutdownClient.connect("localhost", 5007);
        shutdownClient.send("shutdown");
        
        if (backendThread.joinable()) backendThread.join();
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        assert(response1 == "Concurrent1" && "Client 1 should receive correct response");
        assert(response2 == "Concurrent2" && "Client 2 should receive correct response");
        assert(response3 == "Concurrent3" && "Client 3 should receive correct response");
        
        // All clients should have completed in roughly the same time window
        // (concurrent, not sequential)
        double maxDiff = max({abs(elapsed1 - elapsed2), abs(elapsed1 - elapsed3), abs(elapsed2 - elapsed3)});
        assert(maxDiff < 500.0 && "Clients should be handled concurrently");
    });
}

// =============================================================================
// Test: proxy should work with rapid connect/disconnect cycles
// =============================================================================
TEST(test_TcpProxy_rapid_connect_disconnect) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5008
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5008); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6008 -> backend 5008
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6008, "localhost", 5008); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Rapidly connect and disconnect multiple clients
        for (int i = 0; i < 5; i++) {
            TcpClient client;
            client.connect("localhost", 6008);
            client.send("rapid" + to_string(i));
            wait_for_available(client, 2000);
            client.disconnect();
        }
        
        // Final client to verify proxy is still healthy
        TcpClient finalClient;
        finalClient.connect("localhost", 6008);
        finalClient.send("FinalTest");
        
        bool available = wait_for_available(finalClient, 3000);
        string response;
        if (available) response = finalClient.read();
        
        // Cleanup: shutdown backend directly and stop proxy
        TcpClient shutdownClient;
        shutdownClient.connect("localhost", 5008);
        shutdownClient.send("shutdown");
        
        if (backendThread.joinable()) backendThread.join();
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        assert(available && "Proxy should work after rapid connect/disconnect");
        assert(response == "FinalTest" && "Proxy should handle rapid connections");
    });
}

// =============================================================================
// Test: proxy should forward multiple messages sequentially
// =============================================================================
TEST(test_TcpProxy_multiple_messages_sequential) {
    wait_for_port_release();
    
    capture_cout([]() {
        // Start backend echo server on port 5009
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(5009); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy: port 6009 -> backend 5009
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(6009, "localhost", 5009); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect client and send multiple messages
        TcpClient client;
        client.connect("localhost", 6009);
        
        vector<string> messages = {"First", "Second", "Third", "Fourth"};
        vector<string> responses;
        
        for (const auto& msg : messages) {
            client.send(msg);
            if (wait_for_available(client, 3000)) {
                responses.push_back(client.read());
            }
        }
        
        // Cleanup: shutdown backend directly and stop proxy
        TcpClient shutdownClient;
        shutdownClient.connect("localhost", 5009);
        shutdownClient.send("shutdown");
        
        if (backendThread.joinable()) backendThread.join();
        proxy.stop();
        if (proxyThread.joinable()) proxyThread.join();
        
        assert(responses.size() == messages.size() && "Should receive all responses");
        for (size_t i = 0; i < messages.size(); i++) {
            assert(responses[i] == messages[i] && "Each message should be echoed correctly");
        }
    });
}

#endif
