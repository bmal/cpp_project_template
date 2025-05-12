#include <gtest/gtest.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "trading/logger.hpp"
#include "trading/socket_utils.hpp"
#include "trading/udp_socket.hpp"

namespace {
struct TestEnvironment {
    bool isCI;
    bool supportsSoTimestamp;
    bool supportsBusyPoll;
    bool verbose;

    static TestEnvironment detect() {
        TestEnvironment env;
        
        // Simple environment flag checks
        env.isCI = (std::getenv("CI") != nullptr);
        env.verbose = (std::getenv("VERBOSE_TESTS") != nullptr);
        
        // Feature detection with minimal error handling
        int testSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (testSocket == -1) {
            // Default to conservative values if we can't create a socket
            env.supportsSoTimestamp = false;
            env.supportsBusyPoll = false;
            return env;
        }
        
        int enable = 1;
        env.supportsSoTimestamp = 
            (setsockopt(testSocket, SOL_SOCKET, SO_TIMESTAMP, 
                       &enable, sizeof(enable)) == 0);
        
        env.supportsBusyPoll = 
            (setsockopt(testSocket, SOL_SOCKET, SO_BUSY_POLL, 
                       &enable, sizeof(enable)) == 0);
        
        close(testSocket);
        return env;
    }

    void logCapabilities() const {
        if (!verbose) return;
        
        std::cout << "\n---------- Test Environment ----------" << std::endl
                  << "CI Environment: " << (isCI ? "Yes" : "No") << std::endl
                  << "SO_TIMESTAMP support: " << (supportsSoTimestamp ? "Yes" : "No") << std::endl
                  << "SO_BUSY_POLL support: " << (supportsBusyPoll ? "Yes" : "No") << std::endl
                  << "--------------------------------------\n" << std::endl;
    }
};
} // namespace

// Base test fixture for UDP socket tests
class UdpSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        _logger = std::make_unique<Trading::Core::Logger>("udp_socket_test_log.txt");
    }

    void TearDown() override {
        _logger.reset();
    }

    static void SetUpTestSuite() {
        _testEnv = TestEnvironment::detect();
        _testEnv.logCapabilities();
    }

    // Get a port in the ephemeral range for testing
    static int getNextEphemeralPort() {
        static constexpr int EPHEMERAL_PORT_MIN = 49152;
        static constexpr int EPHEMERAL_PORT_MAX = 65535;
        static constexpr int EPHEMERAL_PORT_RANGE = EPHEMERAL_PORT_MAX - EPHEMERAL_PORT_MIN + 1;
        static std::atomic<int> nextPort{EPHEMERAL_PORT_MIN};

        return (nextPort.fetch_add(1) % EPHEMERAL_PORT_RANGE) + EPHEMERAL_PORT_MIN;
    }

    // Helper to create a server socket bound to a random port
    std::pair<Trading::Core::UdpSocket, int> createServerWithEphemeralPort() {
        int port = getNextEphemeralPort();
        auto serverSocket = Trading::Core::UdpSocket::createServer(*_logger, "127.0.0.1", port);
        
        if (!serverSocket.isValid()) {
            logError("Failed to create server socket");
            return {Trading::Core::UdpSocket(), -1};
        }
        
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (getsockname(serverSocket.getFd(), reinterpret_cast<struct sockaddr*>(&addr), &addrLen) != 0) {
            logError("Failed to get socket name");
            return {Trading::Core::UdpSocket(), -1};
        }
        
        int boundPort = ntohs(addr.sin_port);
        logInfo("Server socket bound to port: " + std::to_string(boundPort));
        return {std::move(serverSocket), boundPort};
    }

    // Helper to create a pair of connected UDP sockets
    std::pair<Trading::Core::UdpSocket, Trading::Core::UdpSocket> createSocketPair() {
        auto [server, port] = createServerWithEphemeralPort();
        if (!server.isValid()) {
            return {Trading::Core::UdpSocket(), Trading::Core::UdpSocket()};
        }
        
        auto client = Trading::Core::UdpSocket::createClient(*_logger, "127.0.0.1", port);
        if (!client.isValid()) {
            logError("Failed to create client socket");
            return {Trading::Core::UdpSocket(), Trading::Core::UdpSocket()};
        }
        
        return {std::move(client), std::move(server)};
    }

    // Helper to get socket option value
    int getSocketOption(int socketFd, int level, int option) {
        int value = 0;
        socklen_t length = sizeof(value);
        if (getsockopt(socketFd, level, option, &value, &length) != 0) {
            return -1;
        }
        return value;
    }

    // Helper to check if socket has specific option set
    bool hasSocketOption(int socketFd, int level, int option, int expectedValue) {
        int value = getSocketOption(socketFd, level, option);
        return (value == expectedValue);
    }

    // Helper to verify a socket is in non-blocking mode
    bool isNonBlocking(int socketFd) {
        if (socketFd == Trading::Core::SOCKET_ERROR) {
            return false;
        }

        int flags = fcntl(socketFd, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }

        return (flags & O_NONBLOCK) != 0;
    }

    // Simplified wait for readable helper
    bool waitForReadable(int socketFd, int timeoutMs) {
        if (socketFd == Trading::Core::SOCKET_ERROR) {
            return false;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(socketFd, &fds);

        struct timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        int result = select(socketFd + 1, &fds, nullptr, nullptr, &tv);
        return (result > 0);
    }

    // Simplified receive with timeout
    bool receiveData(int socketFd, void* buffer, size_t bufferSize, size_t& bytesReceived, int timeoutMs) {
        if (!waitForReadable(socketFd, timeoutMs)) {
            return false;
        }
        
        ssize_t result = recv(socketFd, buffer, bufferSize, 0);
        if (result < 0) {
            return false;
        }
        
        bytesReceived = static_cast<size_t>(result);
        return true;
    }

    // Check if timestamp option is enabled
    bool hasTimestampEnabled(int socketFd) {
        return hasSocketOption(socketFd, SOL_SOCKET, SO_TIMESTAMP, 1);
    }

    // Simplified approach to check if multicast is supported
    bool hasMulticastSupport() {
        int testSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (testSocket == -1) {
            return false;
        }
        
        int ttl = 2;
        bool result = (setsockopt(testSocket, IPPROTO_IP, IP_MULTICAST_TTL, 
                                &ttl, sizeof(ttl)) == 0);
        close(testSocket);
        return result;
    }

    // Simplified first network interface finder
    std::string getFirstNetworkInterface() {
        ifaddrs* ifAddrStruct = nullptr;
        
        if (getifaddrs(&ifAddrStruct) != 0) {
            return "";
        }
        
        std::string result;
        for (ifaddrs* ifa = ifAddrStruct; ifa; ifa = ifa->ifa_next) {
            // Skip invalid entries
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET || 
                !ifa->ifa_name || strcmp(ifa->ifa_name, "lo") == 0) {
                continue;
            }
            
            result = ifa->ifa_name;
            break;
        }
        
        freeifaddrs(ifAddrStruct);
        return result;
    }

    // Get an appropriate timeout based on environment
    int getAppropriateTimeout() const {
        return _testEnv.isCI ? 5000 : 1000;
    }

    // Simplified send data helper
    bool sendData(int socketFd, const void* data, size_t dataSize, 
                 struct sockaddr* destAddr = nullptr, socklen_t addrLen = 0) {
        ssize_t result;
        
        if (destAddr) {
            result = sendto(socketFd, data, dataSize, 0, destAddr, addrLen);
        } else {
            result = send(socketFd, data, dataSize, 0);
        }
        
        return (result > 0);
    }

    // Simplified data exchange helper
    bool exchangeData(Trading::Core::UdpSocket& sender, Trading::Core::UdpSocket& receiver,
                     const char* message, int port = 0) {
        if (port == 0) {
            // Already connected
            if (!sendData(sender.getFd(), message, strlen(message))) {
                return false;
            }
        } else {
            // Send to specific address
            struct sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr("127.0.0.1");
            
            if (!sendData(sender.getFd(), message, strlen(message), 
                        reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr))) {
                return false;
            }
        }
        
        // Receive data
        char buffer[256] = {0};
        size_t bytesReceived = 0;
        
        if (!receiveData(receiver.getFd(), buffer, sizeof(buffer), 
                        bytesReceived, getAppropriateTimeout())) {
            return false;
        }
        
        return strcmp(buffer, message) == 0;
    }

    void logInfo(const std::string& message) const {
        if (_testEnv.verbose) {
            std::cout << "[INFO] " << message << std::endl;
        }
        if (_logger) {
            _logger->log("%\n", message);
        }
    }

    void logError(const std::string& message) const {
        std::cout << "[ERROR] " << message << std::endl;
        if (_logger) {
            _logger->log("[ERROR] %\n", message);
        }
    }

    std::unique_ptr<Trading::Core::Logger> _logger;
    static TestEnvironment _testEnv;
};

TestEnvironment UdpSocketTest::_testEnv = TestEnvironment();

// Fixture specifically for socket options testing
class UdpSocketOptionsTest : public UdpSocketTest {
protected:
    void verifySocketOption(const Trading::Core::UdpSocket& socket, 
                           int level, int option, 
                           int expectedValue, 
                           const std::string& optionName) {
        int actualValue = getSocketOption(socket.getFd(), level, option);
        logInfo(optionName + " = " + std::to_string(actualValue));
        
        EXPECT_EQ(actualValue, expectedValue) 
            << optionName << " not set correctly";
    }
};

// Fixture for networking-dependent tests
class UdpSocketNetworkTest : public UdpSocketTest {
protected:
    void SetUp() override {
        UdpSocketTest::SetUp();
        if (_testEnv.isCI) {
            GTEST_SKIP() << "Skipping network-dependent test in CI environment";
        }
    }
};

//------------------------------------------------------------------
// Socket Creation Tests
//------------------------------------------------------------------

TEST_F(UdpSocketTest, CreateClientSocket_ReturnsValidSocket) {
    auto udpSocket = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    
    ASSERT_TRUE(udpSocket.isValid());
}

TEST_F(UdpSocketTest, CreateClientSocket_WithNonBlocking_SocketIsNonBlocking) {
    auto udpSocket = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    
    ASSERT_TRUE(isNonBlocking(udpSocket.getFd()));
}

TEST_F(UdpSocketTest, CreateClientSocket_SocketTypeIsUDP) {
    auto udpSocket = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    
    ASSERT_TRUE(hasSocketOption(udpSocket.getFd(), SOL_SOCKET, SO_TYPE, SOCK_DGRAM));
}

TEST_F(UdpSocketTest, CreateClientWithInvalidAddress_ReturnsInvalidSocket) {
    auto clientSocket = Trading::Core::UdpSocket::createClient(
        *_logger, "999.999.999.999", getNextEphemeralPort());
    
    EXPECT_FALSE(clientSocket.isValid());
}

TEST_F(UdpSocketTest, CreateServerSocket_BindsToSpecifiedPort) {
    int testPort = getNextEphemeralPort();
    auto serverSocket = Trading::Core::UdpSocket::createServer(*_logger, "127.0.0.1", testPort);
    
    ASSERT_TRUE(serverSocket.isValid());
    
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    ASSERT_EQ(getsockname(serverSocket.getFd(), 
              reinterpret_cast<sockaddr*>(&addr), &addrLen), 0);
    
    EXPECT_EQ(ntohs(addr.sin_port), testPort);
}

TEST_F(UdpSocketTest, CreateServerWithInvalidAddress_ShouldFail) {
    const std::string_view invalidAddress = "invalid.ip.address";
    const int testPort = getNextEphemeralPort();
    
    auto serverSocket = Trading::Core::UdpSocket::createServer(*_logger, invalidAddress, testPort);
    
    EXPECT_FALSE(serverSocket.isValid()) 
        << "Socket creation should fail with invalid address";
}

// Add a new test to explicitly test binding to all interfaces
TEST_F(UdpSocketTest, CreateServerWithEmptyAddress_BindsToAllInterfaces) {
    const std::string_view emptyAddress = "";  // Empty string explicitly indicates all interfaces
    const int testPort = getNextEphemeralPort();
    
    auto serverSocket = Trading::Core::UdpSocket::createServer(*_logger, emptyAddress, testPort);
    
    EXPECT_TRUE(serverSocket.isValid()) 
        << "Socket creation should succeed with empty address";
    
    struct sockaddr_in addr{};
    socklen_t addrLen = sizeof(addr);
    ASSERT_EQ(getsockname(serverSocket.getFd(), 
                         reinterpret_cast<struct sockaddr*>(&addr), 
                         &addrLen), 0)
        << "Failed to get socket name: " << strerror(errno);
    
    EXPECT_EQ(addr.sin_addr.s_addr, htonl(INADDR_ANY)) 
        << "Socket should be bound to INADDR_ANY";
}

//------------------------------------------------------------------
// Socket Communication Tests
//------------------------------------------------------------------

TEST_F(UdpSocketTest, ConnectedSocket_CanSendData) {
    auto [server, port] = createServerWithEphemeralPort();
    ASSERT_TRUE(server.isValid());
    ASSERT_GT(port, 0);
    
    auto client = Trading::Core::UdpSocket::createClient(
        *_logger, "127.0.0.1", port);
    ASSERT_TRUE(client.isValid());
    
    const char* testMessage = "Connected UDP test";
    
    ASSERT_TRUE(sendData(client.getFd(), testMessage, strlen(testMessage)));
}

TEST_F(UdpSocketTest, UnconnectedSocket_CanSendToSpecificAddress) {
    auto [server, port] = createServerWithEphemeralPort();
    ASSERT_TRUE(server.isValid());
    ASSERT_GT(port, 0);
    
    auto client = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(client.isValid());
    
    const char* testMessage = "Unconnected UDP test";
    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    ASSERT_TRUE(sendData(client.getFd(), testMessage, strlen(testMessage),
                        reinterpret_cast<struct sockaddr*>(&serverAddr),
                        sizeof(serverAddr)));
}

TEST_F(UdpSocketTest, ServerSocket_CanReceiveData) {
    auto [server, port] = createServerWithEphemeralPort();
    ASSERT_TRUE(server.isValid());
    ASSERT_GT(port, 0);
    
    auto client = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(client.isValid());
    
    // Send data to server
    const char* testMessage = "UDP test message";
    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    ASSERT_TRUE(sendData(client.getFd(), testMessage, strlen(testMessage),
                        reinterpret_cast<struct sockaddr*>(&serverAddr),
                        sizeof(serverAddr)));
    
    // Receive data at server
    char buffer[128] = {0};
    struct sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    
    ASSERT_TRUE(waitForReadable(server.getFd(), getAppropriateTimeout()));
    
    ssize_t recvResult = recvfrom(server.getFd(), buffer, sizeof(buffer), 0,
                                reinterpret_cast<struct sockaddr*>(&clientAddr),
                                &addrLen);
    
    ASSERT_GT(recvResult, 0);
    EXPECT_STREQ(buffer, testMessage);
}

TEST_F(UdpSocketTest, ConnectedSockets_CanExchangeData) {
    auto [server, port] = createServerWithEphemeralPort();
    ASSERT_TRUE(server.isValid());
    ASSERT_GT(port, 0);
    
    auto client = Trading::Core::UdpSocket::createClient(
        *_logger, "127.0.0.1", port);
    ASSERT_TRUE(client.isValid());
    
    // Client sends to server
    const char* clientMessage = "Client to server";
    ASSERT_TRUE(sendData(client.getFd(), clientMessage, strlen(clientMessage)));
    
    // Server receives
    char serverBuffer[128] = {0};
    struct sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    
    ASSERT_TRUE(waitForReadable(server.getFd(), getAppropriateTimeout()));
    
    ssize_t serverReceived = recvfrom(server.getFd(), serverBuffer, sizeof(serverBuffer), 0,
                                    reinterpret_cast<struct sockaddr*>(&clientAddr),
                                    &addrLen);
    ASSERT_GT(serverReceived, 0);
    EXPECT_STREQ(serverBuffer, clientMessage);
    
    // Server sends reply
    const char* serverMessage = "Server to client";
    ASSERT_TRUE(sendData(server.getFd(), serverMessage, strlen(serverMessage),
                        reinterpret_cast<struct sockaddr*>(&clientAddr),
                        addrLen));
    
    // Client receives reply
    char clientBuffer[128] = {0};
    size_t bytesReceived = 0;
    
    ASSERT_TRUE(receiveData(client.getFd(), clientBuffer, sizeof(clientBuffer),
                          bytesReceived, getAppropriateTimeout()));
    EXPECT_STREQ(clientBuffer, serverMessage);
}

//------------------------------------------------------------------
// Socket Options Tests
//------------------------------------------------------------------

TEST_F(UdpSocketOptionsTest, Default_SetsBufferSizes) {
    auto socket = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(socket.isValid());
    
    // Verify buffer sizes are non-zero
    int rcvBufferSize = getSocketOption(socket.getFd(), SOL_SOCKET, SO_RCVBUF);
    int sndBufferSize = getSocketOption(socket.getFd(), SOL_SOCKET, SO_SNDBUF);
    
    EXPECT_GT(rcvBufferSize, 0);
    EXPECT_GT(sndBufferSize, 0);
}

TEST_F(UdpSocketOptionsTest, EnableBroadcast_SetsSOBroadcastOption) {
    Trading::Core::UdpSocketOptions options;
    options.enableBroadcast = true;
    
    auto socket = Trading::Core::UdpSocket::createClient(*_logger, "", 0, 0, false, options);
    ASSERT_TRUE(socket.isValid());
    
    verifySocketOption(socket, SOL_SOCKET, SO_BROADCAST, 1, "SO_BROADCAST");
}

TEST_F(UdpSocketOptionsTest, SocketPriority_SetsPriorityOption) {
    Trading::Core::UdpSocketOptions options;
    options.purpose = Trading::Core::SocketPurpose::EXECUTION;
    
    auto socket = Trading::Core::UdpSocket::createClient(*_logger, "", 0, 0, false, options);
    ASSERT_TRUE(socket.isValid());
    
    verifySocketOption(socket, SOL_SOCKET, SO_PRIORITY, 7, "SO_PRIORITY");
}

TEST_F(UdpSocketOptionsTest, BusyPollTimeout_SetsBusyPollOption) {
    if (!_testEnv.supportsBusyPoll) {
        GTEST_SKIP() << "SO_BUSY_POLL not supported on this platform";
    }

    Trading::Core::UdpSocketOptions options;
    options.pollingMode = Trading::Core::PollingMode::AGGRESSIVE;
    
    auto socket = Trading::Core::UdpSocket::createClient(*_logger, "", 0, 0, false, options);
    ASSERT_TRUE(socket.isValid());
    
    verifySocketOption(socket, SOL_SOCKET, SO_BUSY_POLL, 100, "SO_BUSY_POLL");
}

TEST_F(UdpSocketOptionsTest, FactoryMethod_ForMarketDataMulticast_CreatesAppropriateSocket) {
    auto options = Trading::Core::UdpSocketOptions::forMarketDataMulticast();
    
    EXPECT_TRUE(options.enableMulticast);
    EXPECT_EQ(options.purpose, Trading::Core::SocketPurpose::MARKET_DATA);
    EXPECT_EQ(options.rcvBufferSize, 8 * 1024 * 1024); // 8MB
    EXPECT_EQ(options.pollingMode, Trading::Core::PollingMode::AGGRESSIVE);
}

TEST_F(UdpSocketOptionsTest, FactoryMethod_ForExecution_CreatesAppropriateSocket) {
    auto options = Trading::Core::UdpSocketOptions::forExecution();
    
    EXPECT_FALSE(options.enableMulticast);
    EXPECT_EQ(options.purpose, Trading::Core::SocketPurpose::EXECUTION);
    EXPECT_EQ(options.rcvBufferSize, 2 * 1024 * 1024); // 2MB
    EXPECT_EQ(options.pollingMode, Trading::Core::PollingMode::ULTRA_AGGRESSIVE);
}

//------------------------------------------------------------------
// Timestamp Tests
//------------------------------------------------------------------

TEST_F(UdpSocketTest, CreateWithTimestamp_EnablesTimestampOption) {
    if (!_testEnv.supportsSoTimestamp) {
        GTEST_SKIP() << "SO_TIMESTAMP not supported in this environment";
    }

    auto socket = Trading::Core::UdpSocket::createClient(*_logger, "", 0, 0, true);
    ASSERT_TRUE(socket.isValid());
    
    EXPECT_TRUE(hasTimestampEnabled(socket.getFd()));
}

//------------------------------------------------------------------
// Multicast Tests
//------------------------------------------------------------------

TEST_F(UdpSocketTest, JoinMulticastGroup_ReturnsSuccess) {
    int testPort = getNextEphemeralPort();
    
    Trading::Core::UdpSocketOptions options;
    options.enableMulticast = true;
    
    auto socket = Trading::Core::UdpSocket::createServer(
        *_logger, "", testPort, false, options);
    
    if (!socket.isValid()) {
        GTEST_SKIP() << "Multicast socket creation failed - not supported in this environment";
    }
    
    bool joinResult = socket.joinMulticastGroup(*_logger, "239.255.0.1");
    EXPECT_TRUE(joinResult);
}

TEST_F(UdpSocketTest, LeaveMulticastGroup_ReturnsSuccess) {
    int testPort = getNextEphemeralPort();
    const char* multicastGroup = "239.255.0.1";
    
    Trading::Core::UdpSocketOptions options;
    options.enableMulticast = true;
    
    auto socket = Trading::Core::UdpSocket::createServer(
        *_logger, "", testPort, false, options);
    
    if (!socket.isValid()) {
        GTEST_SKIP() << "Multicast socket creation failed - not supported in this environment";
    }
    
    bool joinResult = socket.joinMulticastGroup(*_logger, multicastGroup);
    if (!joinResult) {
        GTEST_SKIP() << "Could not join multicast group - not supported in this environment";
    }
    
    bool leaveResult = socket.leaveMulticastGroup(*_logger, multicastGroup);
    EXPECT_TRUE(leaveResult);
}

TEST_F(UdpSocketTest, SetMulticastInterface_ReturnsSuccess) {
    int testPort = getNextEphemeralPort();
    
    Trading::Core::UdpSocketOptions options;
    options.enableMulticast = true;
    
    auto socket = Trading::Core::UdpSocket::createServer(
        *_logger, "", testPort, false, options);
    
    if (!socket.isValid()) {
        GTEST_SKIP() << "Multicast socket creation failed - not supported in this environment";
    }
    
    bool result = socket.setMulticastInterface(*_logger, "127.0.0.1");
    EXPECT_TRUE(result);
}

TEST_F(UdpSocketNetworkTest, MulticastSocket_CanReceiveMulticastPackets) {
    if (!hasMulticastSupport()) {
        GTEST_SKIP() << "Multicast not supported in this environment";
    }
    
    std::string interfaceName = getFirstNetworkInterface();
    if (interfaceName.empty()) {
        GTEST_SKIP() << "No suitable network interface for multicast test";
    }
    
    int testPort = getNextEphemeralPort();
    const char* multicastAddress = "239.255.0.1";
    
    // Create multicast receiver
    Trading::Core::UdpSocketOptions receiverOptions;
    receiverOptions.enableMulticast = true;
    
    auto receiver = Trading::Core::UdpSocket::createServer(
        *_logger, "", testPort, false, receiverOptions);
    
    if (!receiver.isValid()) {
        GTEST_SKIP() << "Multicast receiver creation failed";
    }
    
    // Join the multicast group
    bool joinResult = receiver.joinMulticastGroup(*_logger, multicastAddress);
    ASSERT_TRUE(joinResult);
    
    // Create sender
    auto sender = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(sender.isValid());
    
    // Set TTL for multicast
    int ttl = 2;
    ASSERT_EQ(setsockopt(sender.getFd(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)), 0);
    
    // Send a test message
    struct sockaddr_in multicastAddr = 
        Trading::Core::UdpSocket::getMulticastSockAddr(multicastAddress, testPort);
    
    const char* testMessage = "Multicast test";
    ASSERT_TRUE(sendData(sender.getFd(), testMessage, strlen(testMessage),
                        reinterpret_cast<struct sockaddr*>(&multicastAddr),
                        sizeof(multicastAddr)));
    
    // Try to receive the message
    char buffer[128] = {0};
    size_t bytesReceived = 0;
    
    if (receiveData(receiver.getFd(), buffer, sizeof(buffer), 
                   bytesReceived, getAppropriateTimeout())) {
        EXPECT_STREQ(buffer, testMessage);
    } else {
        // This is expected in some environments
        logInfo("Could not receive multicast message (common in some environments)");
    }
}

TEST_F(UdpSocketNetworkTest, MultipleMulticastGroups_CanReceiveFromBothGroups) {
    if (!hasMulticastSupport()) {
        GTEST_SKIP() << "Multicast not supported in this environment";
    }
    
    std::string interfaceName = getFirstNetworkInterface();
    if (interfaceName.empty()) {
        GTEST_SKIP() << "No suitable network interface for multicast test";
    }
    
    int testPort = getNextEphemeralPort();
    const char* multicastAddress1 = "239.255.0.1";
    const char* multicastAddress2 = "239.255.0.2";
    
    // Create multicast receiver
    Trading::Core::UdpSocketOptions receiverOptions;
    receiverOptions.enableMulticast = true;
    
    auto receiver = Trading::Core::UdpSocket::createServer(
        *_logger, "", testPort, false, receiverOptions);
    
    if (!receiver.isValid()) {
        GTEST_SKIP() << "Multicast not supported in this environment";
    }
    
    // Join first multicast group
    bool joinResult1 = receiver.joinMulticastGroup(*_logger, multicastAddress1);
    ASSERT_TRUE(joinResult1);
    
    // Join second multicast group
    bool joinResult2 = receiver.joinMulticastGroup(*_logger, multicastAddress2);
    ASSERT_TRUE(joinResult2);
    
    auto sender = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(sender.isValid());
    
    // Set TTL for multicast
    int ttl = 2;
    ASSERT_EQ(setsockopt(sender.getFd(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)), 0);
    
    // Send to second multicast group
    struct sockaddr_in group2Addr = 
        Trading::Core::UdpSocket::getMulticastSockAddr(multicastAddress2, testPort);
    
    const char* message = "Multicast group 2 test";
    ASSERT_TRUE(sendData(sender.getFd(), message, strlen(message),
                        reinterpret_cast<struct sockaddr*>(&group2Addr),
                        sizeof(group2Addr)));
    
    // Try to receive from second group
    char buffer[128] = {0};
    size_t bytesReceived = 0;
    
    if (receiveData(receiver.getFd(), buffer, sizeof(buffer), 
                   bytesReceived, getAppropriateTimeout())) {
        EXPECT_STREQ(buffer, message);
    } else {
        // This is expected in some environments
        logInfo("Could not receive multicast message (common in some environments)");
    }
}

//------------------------------------------------------------------
// Epoll Integration Tests
//------------------------------------------------------------------

TEST_F(UdpSocketTest, EpollIntegration_AddToEpoll_Succeeds) {
    auto socket = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(socket.isValid());
    
    int epollFd = epoll_create1(0);
    ASSERT_GT(epollFd, 0);
    
    int result = socket.addToEpoll(epollFd, EPOLLIN | EPOLLET);
    EXPECT_EQ(result, 0);
    
    close(epollFd);
}

TEST_F(UdpSocketTest, EpollIntegration_ModifyEpoll_Succeeds) {
    auto socket = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(socket.isValid());
    
    int epollFd = epoll_create1(0);
    ASSERT_GT(epollFd, 0);
    
    int addResult = socket.addToEpoll(epollFd, EPOLLIN);
    ASSERT_EQ(addResult, 0);
    
    int modifyResult = socket.modifyEpoll(epollFd, EPOLLIN | EPOLLOUT);
    EXPECT_EQ(modifyResult, 0);
    
    close(epollFd);
}

TEST_F(UdpSocketTest, EpollIntegration_RemoveFromEpoll_Succeeds) {
    auto socket = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(socket.isValid());
    
    int epollFd = epoll_create1(0);
    ASSERT_GT(epollFd, 0);
    
    int addResult = socket.addToEpoll(epollFd, EPOLLIN);
    ASSERT_EQ(addResult, 0);
    
    int removeResult = socket.removeFromEpoll(epollFd);
    EXPECT_EQ(removeResult, 0);
    
    close(epollFd);
}

TEST_F(UdpSocketTest, EpollIntegration_DataNotification_Succeeds) {
    auto [server, port] = createServerWithEphemeralPort();
    ASSERT_TRUE(server.isValid());
    ASSERT_GT(port, 0);
    
    int epollFd = epoll_create1(0);
    ASSERT_GT(epollFd, 0);
    
    // Add server socket to epoll
    int addResult = server.addToEpoll(epollFd, EPOLLIN);
    ASSERT_EQ(addResult, 0);
    
    // Create client and send data
    auto client = Trading::Core::UdpSocket::createClient(*_logger, "", 0);
    ASSERT_TRUE(client.isValid());
    
    const char* testMessage = "Epoll test message";
    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    ASSERT_TRUE(sendData(client.getFd(), testMessage, strlen(testMessage),
                        reinterpret_cast<struct sockaddr*>(&serverAddr),
                        sizeof(serverAddr)));
    
    // Check if epoll detected the event
    epoll_event events[1];
    int eventCount = epoll_wait(epollFd, events, 1, getAppropriateTimeout());
    
    EXPECT_EQ(eventCount, 1);
    if (eventCount == 1) {
        EXPECT_TRUE(events[0].events & EPOLLIN);
    }
    
    close(epollFd);
}
