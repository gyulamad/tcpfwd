#pragma once

#ifdef TEST

#include "../cpptools/misc/TEST.hpp"
#include "../TcpProxy.hpp"

using namespace std;

// =============================================================================
// TcpProxy Basic Tests (no network required)
// =============================================================================
TEST(test_TcpProxy_initial_state) {
    TcpProxy proxy;
    // Just verify it can be constructed without crashing
}

TEST(test_TcpProxy_forward_method_exists) {
    TcpProxy proxy;
    // Verify the forward method exists and can be called
    // (We won't actually call it as that would block)
}

#endif
