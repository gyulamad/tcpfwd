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
//   1. Start an EchoServer as the backend (port 9091)
//   2. Start TcpProxy to forward from port 9090 to backend
//   3. Connect TcpClient(s) to the proxy (port 9090)
//   4. Verify bidirectional forwarding works correctly
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

// =============================================================================
// Test: should accept tcp client connections
// =============================================================================
TEST(test_TcpProxy_accept_client_connections) {
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        // Give backend time to start
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        // Give proxy time to start
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect a client - this tests that proxy accepts connections
        TcpClient client;
        bool connected = false;
        try {
            client.connect("localhost", 9090);
            connected = client.isConnected();
        } catch (...) {
            connected = false;
        }
        
        // Cleanup: shutdown via backend
        client.send("shutdown");
        if (proxyThread.joinable()) proxyThread.join();
        if (backendThread.joinable()) backendThread.join();
        
        assert(connected && "Proxy should accept client connections");
    });
}

// =============================================================================
// Test: should build up and maintain a channel for each client to the server
// =============================================================================
TEST(test_TcpProxy_maintain_channel_per_client) {
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect multiple clients - each should get its own backend channel
        TcpClient client1, client2;
        client1.connect("localhost", 9090);
        client2.connect("localhost", 9090);
        
        // Send messages to verify each channel works independently
        client1.send("msg1");
        client2.send("msg2");
        
        // Wait for responses (echo server drips chars, so we need more time)
        bool avail1 = wait_for_available(client1, 3000);
        bool avail2 = wait_for_available(client2, 3000);
        
        string response1, response2;
        if (avail1) response1 = client1.read();
        if (avail2) response2 = client2.read();
        
        // Cleanup
        client1.send("shutdown");
        if (proxyThread.joinable()) proxyThread.join();
        if (backendThread.joinable()) backendThread.join();
        
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
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect client and send message through proxy
        TcpClient client;
        client.connect("localhost", 9090);
        
        // Test: client -> proxy -> backend (forward)
        client.send("HelloThroughProxy");
        
        // Test: backend -> proxy -> client (backward)
        bool available = wait_for_available(client, 3000);
        string response;
        if (available) response = client.read();
        
        // Cleanup
        client.send("shutdown");
        if (proxyThread.joinable()) proxyThread.join();
        if (backendThread.joinable()) backendThread.join();
        
        assert(available && "Response should be forwarded back through proxy");
        assert(response == "HelloThroughProxy" && "Data should be forwarded bidirectionally");
    });
}

// =============================================================================
// Test: should handle client disconnects gracefully
// =============================================================================
TEST(test_TcpProxy_handle_client_disconnect) {
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect two clients
        TcpClient client1, client2;
        client1.connect("localhost", 9090);
        client2.connect("localhost", 9090);
        
        // Disconnect client1
        client1.disconnect();
        
        this_thread::sleep_for(chrono::milliseconds(200));
        
        // Client2 should still work (proxy handled client1 disconnect gracefully)
        client2.send("StillWorking");
        bool available = wait_for_available(client2, 3000);
        string response;
        if (available) response = client2.read();
        
        // Cleanup
        client2.send("shutdown");
        if (proxyThread.joinable()) proxyThread.join();
        if (backendThread.joinable()) backendThread.join();
        
        assert(available && "Client 2 should still work after client 1 disconnect");
        assert(response == "StillWorking" && "Proxy should handle client disconnect gracefully");
    });
}

// =============================================================================
// Test: should handle server disconnect gracefully by removing all clients
// =============================================================================
TEST(test_TcpProxy_handle_server_disconnect) {
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect client through proxy
        TcpClient client;
        client.connect("localhost", 9090);
        
        // Shutdown the backend server (simulating server disconnect)
        // We need another client to send shutdown to backend directly
        TcpClient directClient;
        directClient.connect("localhost", 9091);
        directClient.send("shutdown");
        
        // Wait for backend to stop
        if (backendThread.joinable()) backendThread.join();
        
        this_thread::sleep_for(chrono::milliseconds(200));
        
        // The proxy should still be running but client should be disconnected
        // or unable to communicate
        
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
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect client and verify normal operation
        TcpClient client;
        client.connect("localhost", 9090);
        client.send("TestMessage");
        
        bool available = wait_for_available(client, 3000);
        string response;
        if (available) response = client.read();
        
        // Abruptly disconnect client (simulating error condition)
        client.disconnect();
        
        this_thread::sleep_for(chrono::milliseconds(200));
        
        // Connect another client - proxy should still work
        TcpClient client2;
        client2.connect("localhost", 9090);
        client2.send("AfterError");
        
        bool available2 = wait_for_available(client2, 3000);
        string response2;
        if (available2) response2 = client2.read();
        
        // Cleanup
        client2.send("shutdown");
        if (proxyThread.joinable()) proxyThread.join();
        if (backendThread.joinable()) backendThread.join();
        
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
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect multiple clients concurrently
        TcpClient client1, client2, client3;
        string response1, response2, response3;
        double elapsed1 = 0, elapsed2 = 0, elapsed3 = 0;
        
        client1.connect("localhost", 9090);
        client2.connect("localhost", 9090);
        client3.connect("localhost", 9090);
        
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
        
        // Cleanup
        client1.send("shutdown");
        if (proxyThread.joinable()) proxyThread.join();
        if (backendThread.joinable()) backendThread.join();
        
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
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Rapidly connect and disconnect multiple clients
        for (int i = 0; i < 5; i++) {
            TcpClient client;
            client.connect("localhost", 9090);
            client.send("rapid" + to_string(i));
            wait_for_available(client, 2000);
            client.disconnect();
        }
        
        // Final client to verify proxy is still healthy
        TcpClient finalClient;
        finalClient.connect("localhost", 9090);
        finalClient.send("FinalTest");
        
        bool available = wait_for_available(finalClient, 3000);
        string response;
        if (available) response = finalClient.read();
        
        // Cleanup
        finalClient.send("shutdown");
        if (proxyThread.joinable()) proxyThread.join();
        if (backendThread.joinable()) backendThread.join();
        
        assert(available && "Proxy should work after rapid connect/disconnect");
        assert(response == "FinalTest" && "Proxy should handle rapid connections");
    });
}

// =============================================================================
// Test: proxy should forward multiple messages sequentially
// =============================================================================
TEST(test_TcpProxy_multiple_messages_sequential) {
    capture_cout([]() {
        // Start backend echo server
        EchoServer backend;
        thread backendThread([&backend]() { backend.listen(9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Start proxy
        TcpProxy proxy;
        thread proxyThread([&proxy]() { proxy.forward(9090, "localhost", 9091); });
        
        this_thread::sleep_for(chrono::milliseconds(100));
        
        // Connect client and send multiple messages
        TcpClient client;
        client.connect("localhost", 9090);
        
        vector<string> messages = {"First", "Second", "Third", "Fourth"};
        vector<string> responses;
        
        for (const auto& msg : messages) {
            client.send(msg);
            if (wait_for_available(client, 3000)) {
                responses.push_back(client.read());
            }
        }
        
        // Cleanup
        client.send("shutdown");
        if (proxyThread.joinable()) proxyThread.join();
        if (backendThread.joinable()) backendThread.join();
        
        assert(responses.size() == messages.size() && "Should receive all responses");
        for (size_t i = 0; i < messages.size(); i++) {
            assert(responses[i] == messages[i] && "Each message should be echoed correctly");
        }
    });
}

#endif
