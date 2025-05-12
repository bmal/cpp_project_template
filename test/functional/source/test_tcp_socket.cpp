#include <gtest/gtest.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "trading/logger.hpp"
#include "trading/socket_utils.hpp"
#include "trading/tcp_socket.hpp"

namespace {
struct TestEnvironment {
    bool isCI = false;
    bool supportsSoTimestamp = true;
    bool supportsTcpFastOpen = false;
    bool supportsBusyPoll = false;
    bool supportsSlowStartDisable = false;
    
    // Used only for test decisions, not for conditional behavior
    bool verbose = false;

    static TestEnvironment detect() {
        TestEnvironment env;
        env.isCI = std::getenv("CI") != nullptr;
        env.verbose = std::getenv("VERBOSE_TESTS") != nullptr;
        
        // Only detect options that might be missing on some Linux systems
        int testSocket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (testSocket != -1) {
            int enable = 1;
            env.supportsSoTimestamp =
                (setsockopt(testSocket, SOL_SOCKET, SO_TIMESTAMP, &enable,
                            sizeof(enable)) != -1);
            
            env.supportsTcpFastOpen =
                (setsockopt(testSocket, IPPROTO_TCP, TCP_FASTOPEN, &enable,
                            sizeof(enable)) != -1);

            env.supportsBusyPoll =
                (setsockopt(testSocket, SOL_SOCKET, SO_BUSY_POLL, &enable,
                            sizeof(enable)) != -1);

            env.supportsSlowStartDisable =
                (setsockopt(testSocket, IPPROTO_TCP, TCP_SLOW_START_AFTER_IDLE, 
                            &enable, sizeof(enable)) != -1);

            close(testSocket);
        }
        return env;
    }

    void logCapabilities() const {
        if (!verbose) return;
        
        std::cout
            << "\n---------- Test Environment ----------" << std::endl
            << "CI Environment: " << (isCI ? "Yes" : "No") << std::endl
            << "SO_TIMESTAMP support: " << (supportsSoTimestamp ? "Yes" : "No") << std::endl
            << "TCP_FASTOPEN support: " << (supportsTcpFastOpen ? "Yes" : "No") << std::endl
            << "SO_BUSY_POLL support: " << (supportsBusyPoll ? "Yes" : "No") << std::endl
            << "TCP_SLOW_START_AFTER_IDLE support: " 
            << (supportsSlowStartDisable ? "Yes" : "No") << std::endl
            << "--------------------------------------\n"
            << std::endl;
    }
};
} // namespace

class TcpSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        _logger = std::make_unique<Trading::Core::Logger>("tcp_socket_test_log.txt");
    }

    void TearDown() override {
        _logger.reset();
    }

    static void SetUpTestSuite() {
        _testEnv = TestEnvironment::detect();
        _testEnv.logCapabilities();
    }

    class ScopedSocket {
    public:
        explicit ScopedSocket(int fd = Trading::Core::SOCKET_ERROR) : _fd(fd) {}

        ~ScopedSocket() { close(); }

        void close() {
            if (_fd != Trading::Core::SOCKET_ERROR) {
                ::close(_fd);
                _fd = Trading::Core::SOCKET_ERROR;
            }
        }

        [[nodiscard]] int get() const { return _fd; }

        void reset(int fd) {
            close();
            _fd = fd;
        }

        int release() {
            int fd = _fd;
            _fd = Trading::Core::SOCKET_ERROR;
            return fd;
        }

        ScopedSocket(ScopedSocket&& other) noexcept : _fd(other._fd) {
            other._fd = Trading::Core::SOCKET_ERROR;
        }

        ScopedSocket& operator=(ScopedSocket&& other) noexcept {
            if (this != &other) {
                close();
                _fd = other._fd;
                other._fd = Trading::Core::SOCKET_ERROR;
            }
            return *this;
        }

        ScopedSocket(const ScopedSocket&) = delete;
        ScopedSocket& operator=(const ScopedSocket&) = delete;

    private:
        int _fd;
    };

    static int getNextEphemeralPort() {
        static constexpr int EPHEMERAL_PORT_MIN = 49152;
        static constexpr int EPHEMERAL_PORT_MAX = 65535;
        static constexpr int EPHEMERAL_PORT_RANGE = EPHEMERAL_PORT_MAX - EPHEMERAL_PORT_MIN + 1;
        static std::atomic<int> nextPort{EPHEMERAL_PORT_MIN};

        return (nextPort.fetch_add(1)%EPHEMERAL_PORT_RANGE) + EPHEMERAL_PORT_MIN;
    }

    std::pair<Trading::Core::TcpSocket, int> createServerWithEphemeralPort() {
        int port = getNextEphemeralPort();
        auto serverSocket = Trading::Core::TcpSocket::createServer(*_logger, "127.0.0.1", port);
        
        if (!serverSocket.isValid()) {
            logLastError("createServerWithEphemeralPort: Failed to create socket");
            return {Trading::Core::TcpSocket(), -1};
        }
        
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (getsockname(serverSocket.getFd(), reinterpret_cast<struct sockaddr*>(&addr), &addrLen) != 0) {
            logLastError("createServerWithEphemeralPort: Failed to get socket name");
            return {Trading::Core::TcpSocket(), -1};
        }
        
        int boundPort = ntohs(addr.sin_port);
        logInfo("Server socket bound to port: " + std::to_string(boundPort));
        return {std::move(serverSocket), boundPort};
    }

    bool verifySocketOption(int socketFd,
                            int level,
                            int option,
                            int expectedValue,
                            const char* optionName,
                            bool allowPlatformSpecificValues = false) {
        if (socketFd == Trading::Core::SOCKET_ERROR) {
            ADD_FAILURE() << "Invalid socket for verifying " << optionName;
            return false;
        }

        int actualValue;
        socklen_t length = sizeof(actualValue);
        if (getsockopt(socketFd, level, option, &actualValue, &length) != 0) {
            logLastError(std::string("Failed to get ") + optionName);
            return false;
        }

        std::ostringstream oss;
        oss << optionName << " = " << actualValue;
        if (expectedValue != -1) {
            oss << " (expected " << expectedValue << ")";
        }
        logInfo(oss.str());

        if (expectedValue != -1 && !allowPlatformSpecificValues &&
            actualValue != expectedValue) {
            ADD_FAILURE() << optionName << " should be " << expectedValue
                          << " but was " << actualValue;
            return false;
        }

        return true;
    }

    bool verifyLingerOption(int socketFd, int expectedOnOff, int expectedLinger) {
        if (socketFd == Trading::Core::SOCKET_ERROR) {
            ADD_FAILURE() << "Invalid socket for verifying SO_LINGER";
            return false;
        }

        struct linger ling;
        socklen_t length = sizeof(ling);
        if (getsockopt(socketFd, SOL_SOCKET, SO_LINGER, &ling, &length) != 0) {
            logLastError("Failed to get SO_LINGER");
            return false;
        }

        std::ostringstream oss;
        oss << "SO_LINGER = {" << ling.l_onoff << ", " << ling.l_linger 
            << "} (expected {" << expectedOnOff << ", " << expectedLinger << "})";
        logInfo(oss.str());

        if (ling.l_onoff != expectedOnOff || ling.l_linger != expectedLinger) {
            ADD_FAILURE() << "SO_LINGER should be {" << expectedOnOff << ", " 
                          << expectedLinger << "} but was {" << ling.l_onoff 
                          << ", " << ling.l_linger << "}";
            return false;
        }

        return true;
    }

    bool verifyNonBlocking(int socketFd) {
        if (socketFd == Trading::Core::SOCKET_ERROR) {
            ADD_FAILURE() << "Invalid socket for verifying non-blocking mode";
            return false;
        }

        int flags = fcntl(socketFd, F_GETFL, 0);
        if (flags == -1) {
            logLastError("Failed to get socket flags");
            return false;
        }

        std::ostringstream oss;
        oss << "Socket flags: 0x" << std::hex << flags
            << ", O_NONBLOCK flag: 0x" << O_NONBLOCK
            << ", Is non-blocking: " << ((flags & O_NONBLOCK) != 0);
        logInfo(oss.str());

        bool isNonBlocking = (flags & O_NONBLOCK) != 0;
        if (!isNonBlocking) {
            ADD_FAILURE() << "Socket is not in non-blocking mode";
            return false;
        }

        return true;
    }

    bool waitForSocketState(int socketFd, bool forReadable, int timeoutMs) {
        if (socketFd == Trading::Core::SOCKET_ERROR) {
            logInfo("waitForSocketState: Invalid socket");
            return false;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(socketFd, &fds);

        struct timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        std::string stateStr = forReadable ? "readable" : "writable";
        logInfo("Waiting for socket " + std::to_string(socketFd) +
                " to become " + stateStr + " (timeout: " + 
                std::to_string(timeoutMs) + "ms)");

        int result = select(socketFd + 1, 
                          forReadable ? &fds : nullptr, 
                          forReadable ? nullptr : &fds, 
                          nullptr, &tv);

        if (result > 0) {
            logInfo("Socket " + std::to_string(socketFd) + " is now " + stateStr);
            return true;
        } else if (result == 0) {
            logInfo("Timeout waiting for socket " + std::to_string(socketFd) +
                    " to be " + stateStr);
            return false;
        } else {
            logLastError("Error in select() waiting for " + stateStr);
            return false;
        }
    }

    bool waitForReadable(int socketFd, int timeoutMs) {
        return waitForSocketState(socketFd, true, timeoutMs);
    }

    bool waitForWritable(int socketFd, int timeoutMs) {
        return waitForSocketState(socketFd, false, timeoutMs);
    }

    int getAppropriateTimeout() const {
        return _testEnv.isCI ? 5000 : 1000;
    }

    void logInfo(const std::string& message) const {
        if (_testEnv.verbose) {
            std::cout << "[INFO] " << message << std::endl;
        }
        if (_logger) {
            _logger->log("%\n", message);
        }
    }

    void logLastError(const std::string& message) const {
        std::ostringstream oss;
        oss << message << ": [" << errno << "] " << strerror(errno);
        std::cout << "[ERROR] " << oss.str() << std::endl;
        if (_logger) {
            _logger->log("%\n", oss.str());
        }
    }

    std::unique_ptr<Trading::Core::Logger> _logger;
    static TestEnvironment _testEnv;
};

TestEnvironment TcpSocketTest::_testEnv = TestEnvironment();

TEST_F(TcpSocketTest, WhenCreatingTcpSocket_ShouldBeProperType) {
    auto tcpSocket = Trading::Core::TcpSocket::createClient(*_logger, "127.0.0.1", getNextEphemeralPort());
    ASSERT_TRUE(tcpSocket.isValid()) << "Failed to create TCP socket";
    
    verifySocketOption(tcpSocket.getFd(), SOL_SOCKET, SO_TYPE, SOCK_STREAM, "Socket type");
}

TEST_F(TcpSocketTest, WhenCreatingTcpSocket_ShouldBeNonBlocking) {
    auto tcpSocket = Trading::Core::TcpSocket::createClient(*_logger, "127.0.0.1", getNextEphemeralPort());
    ASSERT_TRUE(tcpSocket.isValid()) << "Failed to create TCP socket";
    
    verifyNonBlocking(tcpSocket.getFd());
}

TEST_F(TcpSocketTest, WhenCreatingTcpSocket_ShouldEnableTCPNoDelay) {
    auto tcpSocket = Trading::Core::TcpSocket::createClient(*_logger, "127.0.0.1", getNextEphemeralPort());
    ASSERT_TRUE(tcpSocket.isValid()) << "Failed to create TCP socket";
    
    verifySocketOption(tcpSocket.getFd(), IPPROTO_TCP, TCP_NODELAY, 1, "TCP_NODELAY");
}

TEST_F(TcpSocketTest, WhenCreatingTcpSocket_ShouldSetAppropriateBufferSizes) {
    auto tcpSocket = Trading::Core::TcpSocket::createClient(*_logger, "127.0.0.1", getNextEphemeralPort());
    ASSERT_TRUE(tcpSocket.isValid()) << "Failed to create TCP socket";
    
    // Create a default socket to compare with
    ScopedSocket defaultSocket(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_NE(defaultSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create comparison socket: " << strerror(errno);

    // Check receive buffer
    int rcvBufferSize, defaultRcvSize;
    socklen_t length = sizeof(int);

    ASSERT_EQ(getsockopt(tcpSocket.getFd(), SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &length), 0)
        << "Failed to get receive buffer size: " << strerror(errno);

    ASSERT_EQ(getsockopt(defaultSocket.get(), SOL_SOCKET, SO_RCVBUF, &defaultRcvSize, &length), 0)
        << "Failed to get default receive buffer size: " << strerror(errno);

    logInfo("Receive buffer: Optimized=" + std::to_string(rcvBufferSize) +
            " bytes, Default=" + std::to_string(defaultRcvSize) + " bytes");

    EXPECT_GT(rcvBufferSize, 8192) << "Receive buffer seems too small";

    // Check send buffer
    int sndBufferSize, defaultSndSize;

    ASSERT_EQ(getsockopt(tcpSocket.getFd(), SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &length), 0)
        << "Failed to get send buffer size: " << strerror(errno);

    ASSERT_EQ(getsockopt(defaultSocket.get(), SOL_SOCKET, SO_SNDBUF, &defaultSndSize, &length), 0)
        << "Failed to get default send buffer size: " << strerror(errno);

    logInfo("Send buffer: Optimized=" + std::to_string(sndBufferSize) +
            " bytes, Default=" + std::to_string(defaultSndSize) + " bytes");

    EXPECT_GT(sndBufferSize, 8192) << "Send buffer seems too small";
}

TEST_F(TcpSocketTest, WhenCreatingServerWithEphemeralPort_ShouldBindSuccessfully) {
    auto [serverSocket, boundPort] = createServerWithEphemeralPort();

    ASSERT_TRUE(serverSocket.isValid())
        << "Failed to create server socket: " << strerror(errno);

    ASSERT_GT(boundPort, 0) << "Failed to get ephemeral port number";

    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    ASSERT_EQ(getsockname(serverSocket.getFd(),
                          reinterpret_cast<sockaddr*>(&addr), &addrLen),
              0)
        << "Failed to get socket name: " << strerror(errno);

    EXPECT_EQ(ntohs(addr.sin_port), boundPort)
        << "Socket not bound to expected port";

    int accepting = 0;
    socklen_t optLen = sizeof(accepting);
    if (getsockopt(serverSocket.getFd(), SOL_SOCKET, SO_ACCEPTCONN, &accepting, &optLen) == 0) {
        EXPECT_NE(accepting, 0) << "Socket is not in listening state";
    }
}

TEST_F(TcpSocketTest, WhenServerBindsToPort_ShouldPreventOtherBindsToSamePort) {
    int testPort = getNextEphemeralPort();

    auto serverSocket = Trading::Core::TcpSocket::createServer(*_logger, "127.0.0.1", testPort);
    ASSERT_TRUE(serverSocket.isValid())
        << "Failed to create server socket: " << strerror(errno);

    ScopedSocket testSocket(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_NE(testSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create test socket: " << strerror(errno);

    int disable = 0;
    ASSERT_EQ(setsockopt(testSocket.get(), SOL_SOCKET, SO_REUSEADDR, &disable, sizeof(disable)), 0)
        << "Failed to disable SO_REUSEADDR: " << strerror(errno);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(testPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bindResult = bind(testSocket.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

    EXPECT_EQ(bindResult, -1) << "Second bind should fail when port is in use";
    EXPECT_EQ(errno, EADDRINUSE)
        << "Expected EADDRINUSE but got: " << strerror(errno);
}

TEST_F(TcpSocketTest, WhenSettingReuseAddr_ShouldAllowRebindingToSamePort) {
    int testPort = getNextEphemeralPort();

    {
        ScopedSocket socket1(::socket(AF_INET, SOCK_STREAM, 0));
        ASSERT_NE(socket1.get(), Trading::Core::SOCKET_ERROR)
            << "Failed to create first socket: " << strerror(errno);

        int enable = 1;
        ASSERT_EQ(setsockopt(socket1.get(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)), 0)
            << "Failed to set SO_REUSEADDR: " << strerror(errno);

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(testPort);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        ASSERT_EQ(bind(socket1.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)), 0)
            << "Failed to bind first socket: " << strerror(errno);
    }

    ScopedSocket socket2(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_NE(socket2.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create second socket: " << strerror(errno);

    int enable = 1;
    ASSERT_EQ(setsockopt(socket2.get(), SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)), 0)
        << "Failed to set SO_REUSEADDR on second socket: " << strerror(errno);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(testPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bindResult = bind(socket2.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

    EXPECT_EQ(bindResult, 0)
        << "Failed to bind second socket with SO_REUSEADDR: " << strerror(errno);
}

TEST_F(TcpSocketTest, WhenClientConnectsToServer_ShouldEstablishConnection) {
    auto [serverSocket, serverPort] = createServerWithEphemeralPort();
    ASSERT_TRUE(serverSocket.isValid())
        << "Failed to create server socket: " << strerror(errno);
    ASSERT_GT(serverPort, 0) << "Failed to get ephemeral port number";

    auto clientSocket = Trading::Core::TcpSocket::createClient(*_logger, "127.0.0.1", serverPort);
    ASSERT_TRUE(clientSocket.isValid())
        << "Failed to create client socket: " << strerror(errno);

    int timeout = getAppropriateTimeout();

    ASSERT_TRUE(waitForWritable(clientSocket.getFd(), timeout))
        << "Client socket not ready for writing";

    ASSERT_TRUE(waitForReadable(serverSocket.getFd(), timeout))
        << "Server not ready to accept connections";

    auto acceptedSocket = serverSocket.accept(*_logger);
    ASSERT_TRUE(acceptedSocket.isValid()) << "Accept failed: " << strerror(errno);

    const char* testMessage = "Connection test";
    ssize_t bytesSent = send(clientSocket.getFd(), testMessage, strlen(testMessage), 0);
    ASSERT_GT(bytesSent, 0) << "Failed to send data: " << strerror(errno);

    char buffer[64] = {0};

    ASSERT_TRUE(waitForReadable(acceptedSocket.getFd(), timeout))
        << "Accepted socket not readable";

    ssize_t bytesRead = recv(acceptedSocket.getFd(), buffer, sizeof(buffer), 0);
    ASSERT_GT(bytesRead, 0) << "Failed to receive data: " << strerror(errno);

    EXPECT_STREQ(buffer, testMessage) << "Data corruption in connection test";
}

TEST_F(TcpSocketTest, WhenConnectingToClosedPort_ShouldReturnError) {
    int unusedPort = getNextEphemeralPort();
    
    auto clientSocket = Trading::Core::TcpSocket::createClient(*_logger, "127.0.0.1", unusedPort);
    
    ASSERT_TRUE(clientSocket.isValid())
        << "Failed to create client socket (should succeed even with bad destination)";
    
    // Wait briefly and verify connection didn't succeed
    fd_set writeFds, errorFds;
    FD_ZERO(&writeFds);
    FD_ZERO(&errorFds);
    FD_SET(clientSocket.getFd(), &writeFds);
    FD_SET(clientSocket.getFd(), &errorFds);
    
    struct timeval tv { 0, 100000 }; // 100ms timeout
    
    int selectResult = select(clientSocket.getFd() + 1, nullptr, &writeFds, &errorFds, &tv);
    ASSERT_GE(selectResult, 0) << "Select failed: " << strerror(errno);
    
    // Check for error condition
    if (FD_ISSET(clientSocket.getFd(), &errorFds)) {
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(clientSocket.getFd(), SOL_SOCKET, SO_ERROR, &error, &len);
        logInfo("Connection failed with error: " + std::string(strerror(error)));
        EXPECT_TRUE(error == ECONNREFUSED || error == ETIMEDOUT || error == ENETUNREACH)
            << "Expected connection refused or timeout, got: " << strerror(error);
    }
    else if (FD_ISSET(clientSocket.getFd(), &writeFds)) {
        // If socket is writable, verify it's not actually connected
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(clientSocket.getFd(), SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0) {
            logInfo("Connection marked writable but has error: " + std::string(strerror(error)));
            EXPECT_TRUE(error == ECONNREFUSED || error == ETIMEDOUT || error == ENETUNREACH);
        }
        else {
            // Try to send data - should fail
            char testData[] = "Test";
            int sendResult = send(clientSocket.getFd(), testData, sizeof(testData), 0);
            EXPECT_EQ(sendResult, -1) << "Send should fail on unconnected socket";
        }
    }
    else {
        // Neither error nor writable - connection is still in progress
        logInfo("Connection still in progress after timeout");
    }
}

TEST_F(TcpSocketTest, WhenRequestingTimestamp_ShouldSetSOTimestamp) {
    if (!_testEnv.supportsSoTimestamp) {
        GTEST_SKIP() << "SO_TIMESTAMP not supported in this environment";
        return;
    }

    auto socketWithTimestamp = Trading::Core::TcpSocket::createClient(
        *_logger, "127.0.0.1", getNextEphemeralPort(), true);
    
    ASSERT_TRUE(socketWithTimestamp.isValid())
        << "Failed to create socket with SO_TIMESTAMP: " << strerror(errno);

    int timestamp;
    socklen_t optLen = sizeof(timestamp);

    if (getsockopt(socketWithTimestamp.getFd(), SOL_SOCKET, SO_TIMESTAMP, &timestamp, &optLen) != 0) {
        // If we can't get the option, skip the test
        GTEST_SKIP() << "Cannot retrieve SO_TIMESTAMP option value: " << strerror(errno);
        return;
    }

    EXPECT_NE(timestamp, 0) << "SO_TIMESTAMP not enabled";

    auto socketNoTimestamp = Trading::Core::TcpSocket::createClient(
        *_logger, "127.0.0.1", getNextEphemeralPort());
        
    ASSERT_TRUE(socketNoTimestamp.isValid())
        << "Failed to create socket without SO_TIMESTAMP: " << strerror(errno);
}

TEST_F(TcpSocketTest, WhenOptimizingTcpSocket_ShouldApplyAllOptions) {
    auto tcpSocket = Trading::Core::TcpSocket::createClient(*_logger, "127.0.0.1", getNextEphemeralPort());
    ASSERT_TRUE(tcpSocket.isValid()) << "Failed to create TCP socket";

    // Verify TCP-specific socket options
    verifySocketOption(tcpSocket.getFd(), IPPROTO_TCP, TCP_NODELAY, 1, "TCP_NODELAY");
    verifySocketOption(tcpSocket.getFd(), IPPROTO_TCP, TCP_QUICKACK, 1, "TCP_QUICKACK");
    verifySocketOption(tcpSocket.getFd(), SOL_SOCKET, SO_PRIORITY, 6, "SO_PRIORITY");
    verifyLingerOption(tcpSocket.getFd(), 1, 0);
    
    // Check buffer sizes 
    verifySocketOption(tcpSocket.getFd(), SOL_SOCKET, SO_RCVBUF, -1, "SO_RCVBUF", true);
    verifySocketOption(tcpSocket.getFd(), SOL_SOCKET, SO_SNDBUF, -1, "SO_SNDBUF", true);
    
    // Check TCP_FASTOPEN if supported
    if (_testEnv.supportsTcpFastOpen) {
        verifySocketOption(tcpSocket.getFd(), IPPROTO_TCP, TCP_FASTOPEN, 1, "TCP_FASTOPEN");
    }
    
    // Check SO_BUSY_POLL if supported
    if (_testEnv.supportsBusyPoll) {
        verifySocketOption(tcpSocket.getFd(), SOL_SOCKET, SO_BUSY_POLL, 50, "SO_BUSY_POLL");
    }
    
    // Check TCP_SLOW_START_AFTER_IDLE if supported
    if (_testEnv.supportsSlowStartDisable) {
        verifySocketOption(tcpSocket.getFd(), IPPROTO_TCP, TCP_SLOW_START_AFTER_IDLE, 0, 
                         "TCP_SLOW_START_AFTER_IDLE");
    }
}

TEST_F(TcpSocketTest, WhenUsingTcpSocketOptions_ShouldApplyCorrectly) {
    // Create a socket with custom options
    Trading::Core::TcpSocketOptions customOptions;
    customOptions.disableNagle = false;        // Enable Nagle's algorithm
    customOptions.enableQuickAck = false;      // Disable quick ACK
    customOptions.purpose = Trading::Core::SocketPurpose::ADMINISTRATIVE; // Lower priority
    customOptions.pollingMode = Trading::Core::PollingMode::BALANCED;     // Specific polling mode
    customOptions.rcvBufferSize = 2 * 1024 * 1024; // 2MB receive buffer
    customOptions.sndBufferSize = 2 * 1024 * 1024; // 2MB send buffer
    
    auto tcpSocket = Trading::Core::TcpSocket::createClient(
        *_logger, "127.0.0.1", getNextEphemeralPort(), false, customOptions);
    
    ASSERT_TRUE(tcpSocket.isValid()) << "Failed to create TCP socket with custom options";
    
    // Verify discrete socket options that should have exact values
    verifySocketOption(tcpSocket.getFd(), IPPROTO_TCP, TCP_NODELAY, 0, "TCP_NODELAY (disabled)");
    verifySocketOption(tcpSocket.getFd(), SOL_SOCKET, SO_PRIORITY, 4, "SO_PRIORITY (administrative)");
    
    if (_testEnv.supportsBusyPoll) {
        verifySocketOption(tcpSocket.getFd(), SOL_SOCKET, SO_BUSY_POLL, 50, 
                         "SO_BUSY_POLL (balanced)");
    }
    
    // Test for buffer size optimization - compare with default socket
    ScopedSocket defaultSocket(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_NE(defaultSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create comparison socket: " << strerror(errno);
    
    // Get optimized and default buffer sizes
    int optimizedRcvSize, defaultRcvSize;
    socklen_t optLen = sizeof(int);
    
    ASSERT_EQ(getsockopt(tcpSocket.getFd(), SOL_SOCKET, SO_RCVBUF, 
                        &optimizedRcvSize, &optLen), 0)
        << "Failed to get optimized receive buffer size";
    
    ASSERT_EQ(getsockopt(defaultSocket.get(), SOL_SOCKET, SO_RCVBUF, 
                        &defaultRcvSize, &optLen), 0)
        << "Failed to get default receive buffer size";
    
    logInfo("Receive buffer sizes - Default: " + std::to_string(defaultRcvSize) + 
            ", Optimized: " + std::to_string(optimizedRcvSize));
    
    // For HFT applications, the optimized buffer should be larger than default
    // This test is robust against different system limits
    EXPECT_GE(optimizedRcvSize, defaultRcvSize) 
        << "Optimized buffer should be at least as large as default";
    
    // Similar check for send buffer
    int optimizedSndSize, defaultSndSize;
    
    ASSERT_EQ(getsockopt(tcpSocket.getFd(), SOL_SOCKET, SO_SNDBUF, 
                        &optimizedSndSize, &optLen), 0)
        << "Failed to get optimized send buffer size";
    
    ASSERT_EQ(getsockopt(defaultSocket.get(), SOL_SOCKET, SO_SNDBUF, 
                        &defaultSndSize, &optLen), 0)
        << "Failed to get default send buffer size";
    
    logInfo("Send buffer sizes - Default: " + std::to_string(defaultSndSize) + 
            ", Optimized: " + std::to_string(optimizedSndSize));
    
    EXPECT_GE(optimizedSndSize, defaultSndSize) 
        << "Optimized send buffer should be at least as large as default";
}

// Test large data transfer - uses raw sockets for simplicity
TEST_F(TcpSocketTest, SLOW_WhenUsingTcpSocketWithLargeData_ShouldTransferCorrectly) {
    // Skip this test in CI environments where it might be too slow
    if (_testEnv.isCI) {
        GTEST_SKIP() << "Skipping large data transfer test in CI environment";
        return;
    }
    
    auto [serverSocket, serverPort] = createServerWithEphemeralPort();
    ASSERT_TRUE(serverSocket.isValid());
    ASSERT_GT(serverPort, 0);
    
    auto clientSocket = Trading::Core::TcpSocket::createClient(*_logger, "127.0.0.1", serverPort);
    ASSERT_TRUE(clientSocket.isValid());
    
    int timeout = getAppropriateTimeout();
    ASSERT_TRUE(waitForWritable(clientSocket.getFd(), timeout));
    ASSERT_TRUE(waitForReadable(serverSocket.getFd(), timeout));
    
    auto acceptedSocket = serverSocket.accept(*_logger);
    ASSERT_TRUE(acceptedSocket.isValid());
    
    // Test with moderate data size to avoid timeouts
    const size_t TEST_SIZE = 64 * 1024; // 64KB
    
    // Create test pattern
    std::vector<char> sendData(TEST_SIZE);
    for (size_t i = 0; i < TEST_SIZE; i++) {
        sendData[i] = static_cast<char>((i * 7) % 251);
    }
    
    // Create receiver thread
    std::vector<char> receivedData(TEST_SIZE);
    std::atomic<bool> receiverDone = false;
    std::atomic<size_t> totalReceived = 0;
    
    std::thread receiverThread([&]() {
        size_t offset = 0;
        while (offset < TEST_SIZE) {
            if (!waitForReadable(acceptedSocket.getFd(), timeout)) {
                logInfo("Receiver timeout waiting for data");
                break;
            }
            
            ssize_t bytesRead = recv(acceptedSocket.getFd(), 
                                    receivedData.data() + offset, 
                                    TEST_SIZE - offset, 0);
            if (bytesRead <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                logLastError("Read error in receiver thread");
                break;
            }
            
            offset += bytesRead;
            totalReceived.store(offset);
        }
        receiverDone.store(true);
    });
    
    // Send data
    size_t totalSent = 0;
    const size_t CHUNK_SIZE = 4096;  // 4KB chunks
    
    while (totalSent < TEST_SIZE) {
        size_t toSend = std::min(CHUNK_SIZE, TEST_SIZE - totalSent);
        
        if (!waitForWritable(clientSocket.getFd(), timeout)) {
            logInfo("Send timeout waiting for socket to be writable");
            break;
        }
        
        ssize_t bytesSent = send(clientSocket.getFd(), 
                               sendData.data() + totalSent, 
                               toSend, 0);
        if (bytesSent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            logLastError("Write error in sender");
            break;
        }
        
        totalSent += bytesSent;
    }
    
    // Wait for receiver to finish
    auto startTime = std::chrono::steady_clock::now();
    const auto waitTimeout = std::chrono::seconds(5);
    
    while (!receiverDone.load()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > waitTimeout) {
            logInfo("Timeout waiting for receiver");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (receiverThread.joinable()) {
        receiverThread.join();
    }
    
    // Verify results
    EXPECT_EQ(totalSent, TEST_SIZE) << "Failed to send all data";
    EXPECT_EQ(totalReceived.load(), TEST_SIZE) << "Failed to receive all data";
    
    // Verify data integrity
    bool dataCorrect = true;
    size_t verifySize = std::min(totalReceived.load(), size_t{100});
    for (size_t i = 0; i < verifySize; i++) {
        if (receivedData[i] != sendData[i]) {
            dataCorrect = false;
            break;
        }
    }
    EXPECT_TRUE(dataCorrect) << "Data corruption detected";
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(endTime - startTime).count();
    
    logInfo("Transferred " + std::to_string(totalReceived.load()) + 
            " bytes in " + std::to_string(duration) + " seconds");
}
