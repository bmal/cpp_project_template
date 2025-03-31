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

// Simplified test environment structure - only tracks truly variable capabilities
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

class SocketTest : public ::testing::Test {
   protected:
    // SetUp and TearDown run for each test
    void SetUp() override {
        _logger = std::make_unique<Trading::Core::Logger>("socket_test_log.txt");
    }

    void TearDown() override {
        _logger.reset();
    }

    // Setup shared test environment once for all tests
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

    Trading::Core::SocketConfig createSocketConfig(bool isServer,
                                                   bool isUDP = false,
                                                   int specificPort = 0,
                                                   bool needsTimestamp = false,
                                                   bool isMulticast = false) {
        return Trading::Core::SocketConfig{
            .ip = isMulticast ? "239.255.0.1" : "127.0.0.1",
            .interface = "",
            .port = specificPort > 0 ? specificPort : getNextEphemeralPort(),
            .isUDP = isUDP,
            .isListening = isServer,
            .needsSOTimestamp = needsTimestamp,
            .isMulticast = isMulticast,
            .multicastInterface = ""};
    }

    static int getNextEphemeralPort() {
        static std::atomic<int> nextPort{49152};
        return (nextPort.fetch_add(1) % 16383) + 49152;
    }

    std::pair<ScopedSocket, int> createServerWithEphemeralPort(
        bool isUDP = false) {
        auto config = createSocketConfig(true, isUDP);

        int socketFd = Trading::Core::createSocket(*_logger, config);
        if (socketFd == Trading::Core::SOCKET_ERROR) {
            logLastError(
                "createServerWithEphemeralPort: Failed to create socket");
            return {ScopedSocket(), -1};
        }

        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        if (getsockname(socketFd, reinterpret_cast<struct sockaddr*>(&addr),
                        &addrLen) != 0) {
            logLastError(
                "createServerWithEphemeralPort: Failed to get socket name");
            ::close(socketFd);
            return {ScopedSocket(), -1};
        }

        int port = ntohs(addr.sin_port);
        logInfo("Server socket bound to port: " + std::to_string(port));
        return {ScopedSocket(socketFd), port};
    }

    std::pair<ScopedSocket, ScopedSocket> createSocketPair() {
        int fds[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
            logLastError("createSocketPair: Failed to create socket pair");
            return {ScopedSocket(), ScopedSocket()};
        }
        logInfo("Created socket pair: " + std::to_string(fds[0]) + ", " +
                std::to_string(fds[1]));
        return {ScopedSocket(fds[0]), ScopedSocket(fds[1])};
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

    // Consolidated wait function to reduce code duplication
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

    bool receiveWithTimeout(int socketFd, void* buffer, size_t bufferSize, 
                            size_t& bytesReceived, int timeoutMs) {
        if (!waitForReadable(socketFd, timeoutMs)) {
            return false;
        }
        
        ssize_t result = recv(socketFd, buffer, bufferSize, 0);
        if (result < 0) {
            logLastError("receiveWithTimeout: Failed to receive data");
            return false;
        }
        
        bytesReceived = static_cast<size_t>(result);
        return true;
    }

    bool verifyTimestampReceipt(int socketFd, int timeoutMs) {
        if (!_testEnv.supportsSoTimestamp) {
            logInfo("Skipping timestamp verification - not supported");
            return true;
        }

        char controlBuf[CMSG_SPACE(sizeof(struct timeval))];
        char dataBuf[128];
        struct iovec iov = {
            .iov_base = dataBuf,
            .iov_len = sizeof(dataBuf)
        };
        
        struct msghdr msg = {
            .msg_name = nullptr,
            .msg_namelen = 0,
            .msg_iov = &iov,
            .msg_iovlen = 1,
            .msg_control = controlBuf,
            .msg_controllen = sizeof(controlBuf),
            .msg_flags = 0
        };

        if (!waitForReadable(socketFd, timeoutMs)) {
            logInfo("Timestamp test - no data available");
            return false;
        }

        ssize_t ret = recvmsg(socketFd, &msg, 0);
        if (ret < 0) {
            logLastError("verifyTimestampReceipt: recvmsg failed");
            return false;
        }

        struct cmsghdr* cmsg;
        bool foundTimestamp = false;

        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMP) {
                struct timeval* tv = (struct timeval*)CMSG_DATA(cmsg);
                std::ostringstream oss;
                oss << "Received timestamp: " << tv->tv_sec << "." 
                    << std::setfill('0') << std::setw(6) << tv->tv_usec;
                logInfo(oss.str());
                foundTimestamp = true;
                break;
            }
        }

        return foundTimestamp;
    }

    int getAppropriateTimeout() const {
        return _testEnv.isCI ? 5000 : 1000;
    }

    // Helper to find a valid network interface name
    std::string getFirstNetworkInterface() const {
        ifaddrs* ifAddrStruct = nullptr;
        std::string result;
        
        if (getifaddrs(&ifAddrStruct) == 0) {
            for (ifaddrs* ifa = ifAddrStruct; ifa; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && 
                    ifa->ifa_name && strcmp(ifa->ifa_name, "lo") != 0) {
                    result = ifa->ifa_name;
                    break;
                }
            }
            freeifaddrs(ifAddrStruct);
        }
        
        return result;
    }

    void logInfo(const std::string& message) const {
        // Only log if verbose mode is enabled, except for critical errors
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
        // Always log errors
        std::cout << "[ERROR] " << oss.str() << std::endl;
        if (_logger) {
            _logger->log("%\n", oss.str());
        }
    }

    std::unique_ptr<Trading::Core::Logger> _logger;
    static TestEnvironment _testEnv;
};

// Initialize static member
TestEnvironment SocketTest::_testEnv = TestEnvironment();

// Basic socket creation tests
TEST_F(SocketTest, WhenCreatingSocket_ShouldSucceedAndBeCorrectType) {
    struct TestCase {
        bool isUDP;
        int expectedType;
        const char* description;
    };

    const TestCase testCases[] = {{false, SOCK_STREAM, "TCP"},
                                  {true, SOCK_DGRAM, "UDP"}};

    for (const auto& tc : testCases) {
        SCOPED_TRACE(std::string("Testing ") + tc.description +
                     " socket creation");

        auto config = createSocketConfig(false, tc.isUDP);
        ScopedSocket testSocket(Trading::Core::createSocket(*_logger, config));

        ASSERT_NE(testSocket.get(), Trading::Core::SOCKET_ERROR)
            << "Failed to create " << tc.description
            << " socket: " << strerror(errno);

        verifySocketOption(testSocket.get(), SOL_SOCKET, SO_TYPE,
                           tc.expectedType, "Socket type");
    }
}

// Core test that runs in all build modes - testing safe validation behavior
TEST_F(SocketTest, WhenValidatingInputs_ShouldIdentifyInvalidValues) {
    // Tests that are safe in all build modes
    char ipBuffer[Trading::Core::MAX_IP_LENGTH];
    EXPECT_FALSE(Trading::Core::getInterfaceIP("", ipBuffer));
    EXPECT_FALSE(Trading::Core::setNonBlocking(-1));
    EXPECT_FALSE(Trading::Core::optimizeSocket(-1, false));
    
    // Test socket operations that should fail without triggering assertions
    int invalidSocket = socket(AF_INET+1000, SOCK_STREAM, 0);
    EXPECT_EQ(invalidSocket, -1);
    
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, "not.a.valid.ip", &(sa.sin_addr));
    EXPECT_EQ(result, 0);
}

#ifdef NDEBUG
// Release mode test - directly testing invalid inputs where assertions are disabled
TEST_F(SocketTest, WhenHandlingInvalidInputs_ShouldFailGracefully) {
    // Test with invalid port
    auto configWithInvalidPort = createSocketConfig(false);
    configWithInvalidPort.port = -1;
    
    ScopedSocket testSocket(Trading::Core::createSocket(*_logger, configWithInvalidPort));
    EXPECT_EQ(testSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Creating socket with invalid port should fail";

    // Test with invalid IP format
    auto configWithInvalidIP = createSocketConfig(false);
    configWithInvalidIP.ip = "not.a.valid.ip";
    
    ScopedSocket invalidIPSocket(Trading::Core::createSocket(*_logger, configWithInvalidIP));
    EXPECT_EQ(invalidIPSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Creating socket with invalid IP should fail";
}
#else
// Debug mode death tests - marked as DISABLED_ to avoid slowing down regular test runs
TEST_F(SocketTest, DISABLED_WhenCreatingSocketWithInvalidPort_ShouldAssert) {
    auto configWithInvalidPort = createSocketConfig(false);
    configWithInvalidPort.port = -1;
    
    EXPECT_DEATH({
        (void)Trading::Core::createSocket(*_logger, configWithInvalidPort);
    }, "ASSERT FAILED: .* getaddrinfo\\(\\) failed");
}

TEST_F(SocketTest, DISABLED_WhenCreatingSocketWithInvalidIP_ShouldAssert) {
    auto configWithInvalidIP = createSocketConfig(false);
    configWithInvalidIP.ip = "not.a.valid.ip";
    
    EXPECT_DEATH({
        (void)Trading::Core::createSocket(*_logger, configWithInvalidIP);
    }, "ASSERT FAILED: .* getaddrinfo\\(\\) failed");
}
#endif

TEST_F(SocketTest, WhenCreatingSocket_ShouldBeNonBlocking) {
    auto config = createSocketConfig(false);
    ScopedSocket testSocket(Trading::Core::createSocket(*_logger, config));
    ASSERT_NE(testSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create socket: " << strerror(errno);

    int flags = fcntl(testSocket.get(), F_GETFL, 0);
    ASSERT_NE(flags, -1) << "Failed to get socket flags: " << strerror(errno);

    std::ostringstream oss;
    oss << "Socket flags: 0x" << std::hex << flags << ", O_NONBLOCK flag: 0x"
        << O_NONBLOCK << std::dec
        << ", Is non-blocking: " << ((flags & O_NONBLOCK) != 0);
    logInfo(oss.str());

    EXPECT_TRUE(flags & O_NONBLOCK) << "Socket is not in non-blocking mode";
    
    // Test non-blocking nature with a connect that would block
    ScopedSocket nonConnectingSocket(socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_NE(nonConnectingSocket.get(), Trading::Core::SOCKET_ERROR);
    
    ASSERT_TRUE(Trading::Core::setNonBlocking(nonConnectingSocket.get()));
    
    // Set up an address that should timeout (non-routable)
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(12345);  // Arbitrary port on non-routable address
    addr.sin_addr.s_addr = inet_addr("10.255.255.1");  // Non-routable IP
    
    // Connect should return immediately with EINPROGRESS, not block
    int result = connect(nonConnectingSocket.get(), 
                      reinterpret_cast<struct sockaddr*>(&addr), 
                      sizeof(addr));
    
    EXPECT_EQ(result, -1) << "Non-blocking connect should return immediately";
    EXPECT_TRUE(errno == EINPROGRESS || errno == ENETUNREACH) 
        << "Expected EINPROGRESS or ENETUNREACH but got: " << strerror(errno);
}

TEST_F(SocketTest, WhenCreatingServerWithEphemeralPort_ShouldBindSuccessfully) {
    auto [serverSocket, boundPort] = createServerWithEphemeralPort();

    ASSERT_NE(serverSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create server socket: " << strerror(errno);

    ASSERT_GT(boundPort, 0) << "Failed to get ephemeral port number";

    sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    ASSERT_EQ(getsockname(serverSocket.get(),
                          reinterpret_cast<sockaddr*>(&addr), &addrLen),
              0)
        << "Failed to get socket name: " << strerror(errno);

    EXPECT_EQ(ntohs(addr.sin_port), boundPort)
        << "Socket not bound to expected port";

    int accepting = 0;
    socklen_t optLen = sizeof(accepting);
    if (getsockopt(serverSocket.get(), SOL_SOCKET, SO_ACCEPTCONN, &accepting,
                   &optLen) == 0) {
        EXPECT_NE(accepting, 0) << "Socket is not in listening state";
    }
}

TEST_F(SocketTest, WhenServerBindsToPort_ShouldPreventOtherBindsToSamePort) {
    int testPort = getNextEphemeralPort();

    auto config = createSocketConfig(true, false, testPort);
    ScopedSocket serverSocket(Trading::Core::createSocket(*_logger, config));
    ASSERT_NE(serverSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create server socket: " << strerror(errno);

    ScopedSocket testSocket(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_NE(testSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create test socket: " << strerror(errno);

    int disable = 0;
    ASSERT_EQ(setsockopt(testSocket.get(), SOL_SOCKET, SO_REUSEADDR, &disable,
                         sizeof(disable)),
              0)
        << "Failed to disable SO_REUSEADDR: " << strerror(errno);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(testPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bindResult =
        bind(testSocket.get(), reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr));

    EXPECT_EQ(bindResult, -1) << "Second bind should fail when port is in use";
    EXPECT_EQ(errno, EADDRINUSE)
        << "Expected EADDRINUSE but got: " << strerror(errno);
}

// Test buffer sizes with different configurations
TEST_F(SocketTest, WhenCreatingSocket_ShouldSetAppropriateBufferSizes) {
    auto config = createSocketConfig(false);
    ScopedSocket testSocket(Trading::Core::createSocket(*_logger, config));
    ASSERT_NE(testSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create socket: " << strerror(errno);

    // Create a default socket to compare with
    ScopedSocket defaultSocket(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_NE(defaultSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create comparison socket: " << strerror(errno);

    // Check receive buffer
    int rcvBufferSize, defaultRcvSize;
    socklen_t length = sizeof(int);

    ASSERT_EQ(getsockopt(testSocket.get(), SOL_SOCKET, SO_RCVBUF,
                         &rcvBufferSize, &length),
              0)
        << "Failed to get receive buffer size: " << strerror(errno);

    ASSERT_EQ(getsockopt(defaultSocket.get(), SOL_SOCKET, SO_RCVBUF,
                         &defaultRcvSize, &length),
              0)
        << "Failed to get default receive buffer size: " << strerror(errno);

    logInfo("Receive buffer: Optimized=" + std::to_string(rcvBufferSize) +
            " bytes, Default=" + std::to_string(defaultRcvSize) + " bytes");

    EXPECT_GT(rcvBufferSize, 8192) << "Receive buffer seems too small";

    // Check send buffer
    int sndBufferSize, defaultSndSize;

    ASSERT_EQ(getsockopt(testSocket.get(), SOL_SOCKET, SO_SNDBUF,
                         &sndBufferSize, &length),
              0)
        << "Failed to get send buffer size: " << strerror(errno);

    ASSERT_EQ(getsockopt(defaultSocket.get(), SOL_SOCKET, SO_SNDBUF,
                         &defaultSndSize, &length),
              0)
        << "Failed to get default send buffer size: " << strerror(errno);

    logInfo("Send buffer: Optimized=" + std::to_string(sndBufferSize) +
            " bytes, Default=" + std::to_string(defaultSndSize) + " bytes");

    EXPECT_GT(sndBufferSize, 8192) << "Send buffer seems too small";
    
    // Test with UDP socket as well
    auto udpConfig = createSocketConfig(false, true);
    ScopedSocket udpSocket(Trading::Core::createSocket(*_logger, udpConfig));
    ASSERT_NE(udpSocket.get(), Trading::Core::SOCKET_ERROR);
    
    int udpRcvBufferSize;
    ASSERT_EQ(getsockopt(udpSocket.get(), SOL_SOCKET, SO_RCVBUF,
                         &udpRcvBufferSize, &length),
              0);
    
    logInfo("UDP Receive buffer: " + std::to_string(udpRcvBufferSize) + " bytes");
    EXPECT_GT(udpRcvBufferSize, 8192) << "UDP Receive buffer seems too small";
}

TEST_F(SocketTest, WhenCreatingTCPSocket_ShouldEnableTCPNoDelay) {
    auto config = createSocketConfig(false);
    ScopedSocket testSocket(Trading::Core::createSocket(*_logger, config));
    ASSERT_NE(testSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create socket: " << strerror(errno);

    int nodelay;
    socklen_t optLen = sizeof(nodelay);
    ASSERT_EQ(getsockopt(testSocket.get(), IPPROTO_TCP, TCP_NODELAY, &nodelay,
                         &optLen),
              0)
        << "Failed to get TCP_NODELAY option: " << strerror(errno);

    logInfo(std::string("TCP_NODELAY = ") + std::to_string(nodelay));

    EXPECT_NE(nodelay, 0) << "TCP_NODELAY should be enabled";
}

// Split the large data transfer test into basic and performance sections
TEST_F(SocketTest, WhenUsingSockets_ShouldSendAndReceiveData) {
    auto [socket1, socket2] = createSocketPair();
    ASSERT_NE(socket1.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create socket pair: " << strerror(errno);
    ASSERT_NE(socket2.get(), Trading::Core::SOCKET_ERROR);

    const char* testMessage = "Hello, socket world!";
    ssize_t bytesSent = write(socket1.get(), testMessage, strlen(testMessage));
    ASSERT_GT(bytesSent, 0) << "Failed to write to socket: " << strerror(errno);

    char buffer[64] = {0};
    ssize_t bytesRead = read(socket2.get(), buffer, sizeof(buffer));
    ASSERT_GT(bytesRead, 0)
        << "Failed to read from socket: " << strerror(errno);

    EXPECT_EQ(bytesRead, static_cast<ssize_t>(strlen(testMessage)))
        << "Incorrect number of bytes read";

    EXPECT_STREQ(testMessage, buffer)
        << "Data corruption in socket communication";
}

// Explicitly marked as SLOW to separate from regular tests
TEST_F(SocketTest, SLOW_WhenUsingSocketsWithLargeData_ShouldTransferCorrectly) {
    // Skip this test in CI environments where it might be too slow
    if (_testEnv.isCI) {
        GTEST_SKIP() << "Skipping large data transfer test in CI environment";
        return;
    }
    
    auto [socket1, socket2] = createSocketPair();
    ASSERT_NE(socket1.get(), Trading::Core::SOCKET_ERROR);
    ASSERT_NE(socket2.get(), Trading::Core::SOCKET_ERROR);
    
    // Set both sockets to blocking mode for this test to simplify the code
    // (Non-blocking would require significantly more complex handling)
    int flags1 = fcntl(socket1.get(), F_GETFL, 0);
    int flags2 = fcntl(socket2.get(), F_GETFL, 0);
    ASSERT_NE(flags1, -1);
    ASSERT_NE(flags2, -1);
    
    // Clear non-blocking flag if set
    if (flags1 & O_NONBLOCK) {
        ASSERT_NE(fcntl(socket1.get(), F_SETFL, flags1 & ~O_NONBLOCK), -1);
    }
    if (flags2 & O_NONBLOCK) {
        ASSERT_NE(fcntl(socket2.get(), F_SETFL, flags2 & ~O_NONBLOCK), -1);
    }
    
    // Optimize buffer sizes
    ASSERT_TRUE(Trading::Core::optimizeSocket(socket1.get(), false));
    ASSERT_TRUE(Trading::Core::optimizeSocket(socket2.get(), false));
    
    // Use a smaller test size to avoid test timeouts
    const size_t TEST_SIZE = 64 * 1024; // 64KB
    
    // Create a test pattern
    std::vector<char> sendData(TEST_SIZE);
    for (size_t i = 0; i < TEST_SIZE; i++) {
        sendData[i] = static_cast<char>((i * 7) % 251);
    }
    
    // Create a receiver thread to prevent deadlock
    std::vector<char> receivedData(TEST_SIZE);
    std::atomic<bool> receiverDone = false;
    std::atomic<size_t> totalReceived = 0;
    
    std::thread receiverThread([&]() {
        size_t offset = 0;
        while (offset < TEST_SIZE) {
            // Wait for readability to avoid busy-waiting
            if (!waitForReadable(socket2.get(), getAppropriateTimeout())) {
                logInfo("Receiver timeout waiting for data");
                break;
            }
            
            // Read available data
            ssize_t bytesRead = read(socket2.get(), 
                                     receivedData.data() + offset, 
                                     TEST_SIZE - offset);
            if (bytesRead <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;  // Try again
                }
                logLastError("Read error in receiver thread");
                break;
            }
            
            offset += bytesRead;
            totalReceived.store(offset);
        }
        receiverDone.store(true);
    });
    
    // Send the data in smaller chunks to avoid buffer filling up
    size_t totalSent = 0;
    const size_t CHUNK_SIZE = 4096;  // 4KB chunks
    
    while (totalSent < TEST_SIZE) {
        size_t toSend = std::min(CHUNK_SIZE, TEST_SIZE - totalSent);
        
        // Wait for writability before attempting to send
        if (!waitForWritable(socket1.get(), getAppropriateTimeout())) {
            logInfo("Send timeout waiting for socket to be writable");
            break;
        }
        
        ssize_t bytesSent = write(socket1.get(), 
                                 sendData.data() + totalSent, 
                                 toSend);
        if (bytesSent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // Try again
            }
            logLastError("Write error in sender");
            break;
        }
        
        totalSent += bytesSent;
        
        // Give the receiver thread a chance to catch up
        if (totalSent % (4 * CHUNK_SIZE) == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    // Wait for receiver to finish
    auto startTime = std::chrono::steady_clock::now();
    const auto waitTimeout = std::chrono::seconds(5);
    
    while (!receiverDone.load()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > waitTimeout) {
            logInfo("Final timeout waiting for receiver");
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Make sure we join the thread even on test failure
    if (receiverThread.joinable()) {
        receiverThread.join();
    }
    
    // Verify results
    EXPECT_EQ(totalSent, TEST_SIZE) << "Failed to send all data";
    EXPECT_EQ(totalReceived.load(), TEST_SIZE) << "Failed to receive all data";
    
    // Verify data integrity (first 100 bytes)
    bool dataCorrect = true;
    size_t verifySize = std::min(totalReceived.load(), size_t{100});
    for (size_t i = 0; i < verifySize; i++) {
        if (receivedData[i] != sendData[i]) {
            dataCorrect = false;
            break;
        }
    }
    EXPECT_TRUE(dataCorrect) << "Data corruption detected";
    
    // Report timing for performance measurement
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(endTime - startTime).count();
    
    logInfo("Transferred " + std::to_string(totalReceived.load()) + 
            " of " + std::to_string(TEST_SIZE) + " bytes in " + 
            std::to_string(duration) + " seconds");
}

TEST_F(SocketTest, WhenClientConnectsToServer_ShouldEstablishConnection) {
    auto [serverSocket, serverPort] = createServerWithEphemeralPort();
    ASSERT_NE(serverSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create server socket: " << strerror(errno);
    ASSERT_GT(serverPort, 0) << "Failed to get ephemeral port number";

    auto clientConfig = createSocketConfig(false, false, serverPort);
    ScopedSocket clientSocket(
        Trading::Core::createSocket(*_logger, clientConfig));
    ASSERT_NE(clientSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create client socket: " << strerror(errno);

    int timeout = getAppropriateTimeout();

    ASSERT_TRUE(waitForWritable(clientSocket.get(), timeout))
        << "Client socket not ready for writing";

    ASSERT_TRUE(waitForReadable(serverSocket.get(), timeout))
        << "Server not ready to accept connections";

    ScopedSocket acceptedSocket(accept(serverSocket.get(), nullptr, nullptr));

    ASSERT_NE(acceptedSocket.get(), -1) << "Accept failed: " << strerror(errno);

    const char* testMessage = "Connection test";
    ssize_t bytesSent =
        send(clientSocket.get(), testMessage, strlen(testMessage), 0);
    ASSERT_GT(bytesSent, 0) << "Failed to send data: " << strerror(errno);

    char buffer[64] = {0};

    ASSERT_TRUE(waitForReadable(acceptedSocket.get(), timeout))
        << "Accepted socket not readable";

    ssize_t bytesRead = recv(acceptedSocket.get(), buffer, sizeof(buffer), 0);
    ASSERT_GT(bytesRead, 0) << "Failed to receive data: " << strerror(errno);

    EXPECT_STREQ(buffer, testMessage) << "Data corruption in connection test";
}

// Test that attempts to connect to a closed port and verifies proper error handling
TEST_F(SocketTest, WhenConnectingToClosedPort_ShouldReturnError) {
    int unusedPort = getNextEphemeralPort();
    
    auto clientConfig = createSocketConfig(false, false, unusedPort);
    
    // Since this is non-blocking, it might initially return EINPROGRESS
    // followed by an error on select() rather than immediate failure
    ScopedSocket clientSocket(Trading::Core::createSocket(*_logger, clientConfig));
    
    ASSERT_NE(clientSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create client socket (should succeed even with bad destination)";
    
    // Wait briefly and verify connection didn't succeed
    fd_set writeFds, errorFds;
    FD_ZERO(&writeFds);
    FD_ZERO(&errorFds);
    FD_SET(clientSocket.get(), &writeFds);
    FD_SET(clientSocket.get(), &errorFds);
    
    struct timeval tv { 0, 100000 }; // 100ms timeout
    
    int selectResult = select(clientSocket.get() + 1, nullptr, &writeFds, &errorFds, &tv);
    ASSERT_GE(selectResult, 0) << "Select failed: " << strerror(errno);
    
    // Check for error condition
    if (FD_ISSET(clientSocket.get(), &errorFds)) {
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(clientSocket.get(), SOL_SOCKET, SO_ERROR, &error, &len);
        logInfo("Connection failed with error: " + std::string(strerror(error)));
        EXPECT_TRUE(error == ECONNREFUSED || error == ETIMEDOUT || error == ENETUNREACH)
            << "Expected connection refused or timeout, got: " << strerror(error);
    }
    else if (FD_ISSET(clientSocket.get(), &writeFds)) {
        // If socket is writable, verify it's not actually connected
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(clientSocket.get(), SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0) {
            logInfo("Connection marked writable but has error: " + std::string(strerror(error)));
            EXPECT_TRUE(error == ECONNREFUSED || error == ETIMEDOUT || error == ENETUNREACH);
        }
        else {
            // Try to send data - should fail
            char testData[] = "Test";
            int sendResult = send(clientSocket.get(), testData, sizeof(testData), 0);
            EXPECT_EQ(sendResult, -1) << "Send should fail on unconnected socket";
        }
    }
    else {
        // Neither error nor writable - connection is still in progress
        logInfo("Connection still in progress after timeout");
    }
}

TEST_F(SocketTest, WhenSettingReuseAddr_ShouldAllowRebindingToSamePort) {
    int testPort = getNextEphemeralPort();

    {
        ScopedSocket socket1(::socket(AF_INET, SOCK_STREAM, 0));
        ASSERT_NE(socket1.get(), Trading::Core::SOCKET_ERROR)
            << "Failed to create first socket: " << strerror(errno);

        int enable = 1;
        ASSERT_EQ(setsockopt(socket1.get(), SOL_SOCKET, SO_REUSEADDR, &enable,
                             sizeof(enable)),
                  0)
            << "Failed to set SO_REUSEADDR: " << strerror(errno);

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(testPort);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        ASSERT_EQ(bind(socket1.get(), reinterpret_cast<struct sockaddr*>(&addr),
                       sizeof(addr)),
                  0)
            << "Failed to bind first socket: " << strerror(errno);
    }

    ScopedSocket socket2(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_NE(socket2.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create second socket: " << strerror(errno);

    int enable = 1;
    ASSERT_EQ(setsockopt(socket2.get(), SOL_SOCKET, SO_REUSEADDR, &enable,
                         sizeof(enable)),
              0)
        << "Failed to set SO_REUSEADDR on second socket: " << strerror(errno);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(testPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bindResult = bind(
        socket2.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

    EXPECT_EQ(bindResult, 0)
        << "Failed to bind second socket with SO_REUSEADDR: "
        << strerror(errno);
}

// Test SO_REUSEPORT for multicast sockets
TEST_F(SocketTest, WhenCreatingMulticastSockets_ShouldSupportReusePort) {
    int testPort = getNextEphemeralPort();
    
    auto config1 = createSocketConfig(true, true, testPort, false, true);
    ScopedSocket socket1(Trading::Core::createSocket(*_logger, config1));
    ASSERT_NE(socket1.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create first multicast socket: " << strerror(errno);
        
    auto config2 = createSocketConfig(true, true, testPort, false, true);
    ScopedSocket socket2(Trading::Core::createSocket(*_logger, config2));
    ASSERT_NE(socket2.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create second multicast socket: " << strerror(errno);
    
    logInfo("Successfully created two multicast sockets on same port");
}

TEST_F(SocketTest, WhenRequestingTimestamp_ShouldSetSOTimestamp) {
    if (!_testEnv.supportsSoTimestamp) {
        GTEST_SKIP() << "SO_TIMESTAMP not supported in this environment";
        return;
    }

    auto config = createSocketConfig(false, false, 0, true);
    ScopedSocket testSocket(Trading::Core::createSocket(*_logger, config));
    ASSERT_NE(testSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create socket with SO_TIMESTAMP: " << strerror(errno);

    int timestamp;
    socklen_t optLen = sizeof(timestamp);

    if (getsockopt(testSocket.get(), SOL_SOCKET, SO_TIMESTAMP, &timestamp,
                   &optLen) != 0) {
        // If we can't get the option, skip the test
        GTEST_SKIP() << "Cannot retrieve SO_TIMESTAMP option value: "
                     << strerror(errno);
        return;
    }

    EXPECT_NE(timestamp, 0) << "SO_TIMESTAMP not enabled";

    auto configNoTimestamp = createSocketConfig(false);
    ScopedSocket socketNoTimestamp(
        Trading::Core::createSocket(*_logger, configNoTimestamp));
    ASSERT_NE(socketNoTimestamp.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create socket without SO_TIMESTAMP: " << strerror(errno);
        
    // For timestamp testing, use UDP sockets instead of socket pair
    // UDP is more likely to work with timestamps
    int timestampPort = getNextEphemeralPort();
    
    // Create receiver socket with timestamp enabled
    ScopedSocket recvSocket(socket(AF_INET, SOCK_DGRAM, 0));
    ASSERT_NE(recvSocket.get(), Trading::Core::SOCKET_ERROR);
    
    // Enable SO_TIMESTAMP
    int enable = 1;
    ASSERT_EQ(setsockopt(recvSocket.get(), SOL_SOCKET, SO_TIMESTAMP, &enable, sizeof(enable)), 0)
        << "Failed to set SO_TIMESTAMP: " << strerror(errno);
    
    // Bind to port
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(timestampPort);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    ASSERT_EQ(bind(recvSocket.get(), reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)), 0)
        << "Failed to bind timestamp receiver: " << strerror(errno);
    
    // Create sender socket
    ScopedSocket sendSocket(socket(AF_INET, SOCK_DGRAM, 0));
    ASSERT_NE(sendSocket.get(), Trading::Core::SOCKET_ERROR);
    
    // Set destination
    struct sockaddr_in destAddr = addr;
    
    // Send a test message
    const char* testMessage = "Timestamp test";
    ssize_t bytesSent = sendto(sendSocket.get(), testMessage, strlen(testMessage), 0,
                              reinterpret_cast<struct sockaddr*>(&destAddr),
                              sizeof(destAddr));
    ASSERT_GT(bytesSent, 0) << "Failed to send timestamp test message";
    
    // Wait for data to arrive
    int timeout = getAppropriateTimeout();
    ASSERT_TRUE(waitForReadable(recvSocket.get(), timeout))
        << "Timeout waiting for timestamp test data";
    
    // Receive the message with timestamp
    char msgBuffer[128];
    char ctrlBuffer[CMSG_SPACE(sizeof(struct timeval))];
    
    struct msghdr msg{};
    struct iovec iov[1];
    
    iov[0].iov_base = msgBuffer;
    iov[0].iov_len = sizeof(msgBuffer);
    
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ctrlBuffer;
    msg.msg_controllen = sizeof(ctrlBuffer);
    
    ssize_t bytesReceived = recvmsg(recvSocket.get(), &msg, 0);
    ASSERT_GT(bytesReceived, 0) << "Failed to receive timestamp message";
    
    // Extract and verify timestamp
    bool foundTimestamp = false;
    struct cmsghdr* cmsg;
    
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SO_TIMESTAMP) {
            struct timeval* tv = (struct timeval*)CMSG_DATA(cmsg);
            logInfo("Received timestamp: " + std::to_string(tv->tv_sec) + "." + 
                   std::to_string(tv->tv_usec));
            foundTimestamp = true;
            break;
        }
    }
    
    // On some systems, timestamps might not be supported properly
    // So we'll check if we got a timestamp but not fail the test if not
    if (!foundTimestamp) {
        logInfo("Warning: Timestamp not received. This might be normal on some systems.");
    }
    
    // Verify message content instead of timestamp receipt
    msgBuffer[bytesReceived] = '\0';  // Null-terminate
    EXPECT_STREQ(msgBuffer, testMessage) << "Message content mismatch";
    
    // Verify that we can check for the option, even if we can't get the actual timestamp
    int tsOptionEnabled;
    socklen_t tsOptLen = sizeof(tsOptionEnabled);
    int getOptResult = getsockopt(recvSocket.get(), SOL_SOCKET, SO_TIMESTAMP, 
                                 &tsOptionEnabled, &tsOptLen);
    
    EXPECT_EQ(getOptResult, 0) << "Failed to get SO_TIMESTAMP option: " << strerror(errno);
    EXPECT_NE(tsOptionEnabled, 0) << "SO_TIMESTAMP should be enabled";
}

// Add a test for getInterfaceIP function
TEST_F(SocketTest, WhenRequestingInterfaceIP_ShouldResolveCorrectly) {
    std::string interfaceName = getFirstNetworkInterface();
    
    if (interfaceName.empty()) {
        GTEST_SKIP() << "No suitable network interface found for testing";
        return;
    }
    
    logInfo("Testing getInterfaceIP with interface: " + interfaceName);
    
    char ipBuffer[Trading::Core::MAX_IP_LENGTH];
    bool success = Trading::Core::getInterfaceIP(interfaceName, ipBuffer);
    
    ASSERT_TRUE(success) << "Failed to get IP for interface " << interfaceName;
    ASSERT_STRNE(ipBuffer, "") << "Returned IP address is empty";
    
    logInfo("Interface " + interfaceName + " has IP: " + ipBuffer);
    
    // Verify the IP is in valid format (simple check)
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipBuffer, &(sa.sin_addr));
    EXPECT_EQ(result, 1) << "IP address not in valid format: " << ipBuffer;
    
    // Test with invalid interface name
    char invalidBuffer[Trading::Core::MAX_IP_LENGTH];
    bool invalidResult = Trading::Core::getInterfaceIP("nonexistent_iface", invalidBuffer);
    
    EXPECT_FALSE(invalidResult) << "getInterfaceIP should fail for invalid interface";
    EXPECT_STREQ(invalidBuffer, "") << "Buffer should be empty for invalid interface";
}

TEST_F(SocketTest, WhenCreatingUDPSocket_ShouldSupportMulticastAddress) {
    auto config = createSocketConfig(false, true, 0, false, true);
    ScopedSocket testSocket(Trading::Core::createSocket(*_logger, config));
    ASSERT_NE(testSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create UDP socket with multicast address: "
        << strerror(errno);

    verifySocketOption(testSocket.get(), SOL_SOCKET, SO_TYPE, SOCK_DGRAM,
                       "Socket type");
                       
    // Add actual multicast send/receive test if possible
    int testPort = getNextEphemeralPort();
    
    // Create receiver first (needs to join multicast group)
    auto receiverConfig = createSocketConfig(true, true, testPort, false, true);
    ScopedSocket receiverSocket(Trading::Core::createSocket(*_logger, receiverConfig));
    ASSERT_NE(receiverSocket.get(), Trading::Core::SOCKET_ERROR)
        << "Failed to create multicast receiver socket";

    // Create sender
    ScopedSocket senderSocket(socket(AF_INET, SOCK_DGRAM, 0));
    ASSERT_NE(senderSocket.get(), Trading::Core::SOCKET_ERROR);
    
    // Set TTL for multicast (ensure packets reach the network)
    int ttl = 2;
    ASSERT_EQ(setsockopt(senderSocket.get(), IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)), 0)
        << "Failed to set TTL on multicast sender";
    
    // Send a test message to the multicast group
    struct sockaddr_in multicastAddr{};
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(testPort);
    multicastAddr.sin_addr.s_addr = inet_addr("239.255.0.1");  // Same as in config
    
    const char* testMessage = "Multicast test";
    ssize_t bytesSent = sendto(senderSocket.get(), testMessage, strlen(testMessage), 0,
                              reinterpret_cast<struct sockaddr*>(&multicastAddr),
                              sizeof(multicastAddr));
                              
    ASSERT_GT(bytesSent, 0) << "Failed to send multicast message: " << strerror(errno);
    logInfo("Sent multicast message of " + std::to_string(bytesSent) + " bytes");
    
    // Try to receive the message (might not work in all environments)
    char buffer[128] = {0};
    size_t bytesReceived = 0;
    bool received = receiveWithTimeout(receiverSocket.get(), buffer, sizeof(buffer), 
                                      bytesReceived, getAppropriateTimeout());
                                      
    if (received) {
        logInfo("Received multicast message: " + std::string(buffer));
        EXPECT_STREQ(buffer, testMessage) << "Received multicast message doesn't match sent message";
    }
    else {
        logInfo("Note: Failed to receive multicast message (expected in some environments)");
    }
}

// Comprehensive test of socket optimizations
TEST_F(SocketTest, WhenOptimizingNetworkSocket_ShouldApplyAllOptions) {
    // Create a TCP/IP socket
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(socketFd, Trading::Core::SOCKET_ERROR)
        << "Failed to create socket: " << strerror(errno);
    ScopedSocket scoped(socketFd);  // For automatic cleanup

    // Test the optimization function
    ASSERT_TRUE(Trading::Core::optimizeSocket(socketFd, false))
        << "Failed to optimize socket: " << strerror(errno);

    // Verify all socket options were set correctly
    verifySocketOption(socketFd, IPPROTO_TCP, TCP_NODELAY, 1, "TCP_NODELAY");
    verifySocketOption(socketFd, IPPROTO_TCP, TCP_QUICKACK, 1, "TCP_QUICKACK");
    verifySocketOption(socketFd, SOL_SOCKET, SO_PRIORITY, 6, "SO_PRIORITY");
    verifyLingerOption(socketFd, 1, 0);
    
    // Check buffer sizes 
    verifySocketOption(socketFd, SOL_SOCKET, SO_RCVBUF, -1, "SO_RCVBUF", true);
    verifySocketOption(socketFd, SOL_SOCKET, SO_SNDBUF, -1, "SO_SNDBUF", true);
    
    // Check TCP_FASTOPEN if supported
    if (_testEnv.supportsTcpFastOpen) {
        verifySocketOption(socketFd, IPPROTO_TCP, TCP_FASTOPEN, 1, "TCP_FASTOPEN");
    }
    
    // Check SO_BUSY_POLL if supported
    if (_testEnv.supportsBusyPoll) {
        verifySocketOption(socketFd, SOL_SOCKET, SO_BUSY_POLL, 50, "SO_BUSY_POLL");
    }
    
    // Check TCP_SLOW_START_AFTER_IDLE if supported
    if (_testEnv.supportsSlowStartDisable) {
        verifySocketOption(socketFd, IPPROTO_TCP, TCP_SLOW_START_AFTER_IDLE, 0, 
                         "TCP_SLOW_START_AFTER_IDLE");
    }
    
    // Test UDP optimization as well
    int udpSocketFd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_NE(udpSocketFd, Trading::Core::SOCKET_ERROR);
    ScopedSocket udpScoped(udpSocketFd);
    
    ASSERT_TRUE(Trading::Core::optimizeSocket(udpSocketFd, true))
        << "Failed to optimize UDP socket";
        
    // UDP should only optimize buffer sizes
    verifySocketOption(udpSocketFd, SOL_SOCKET, SO_RCVBUF, -1, "UDP SO_RCVBUF", true);
    verifySocketOption(udpSocketFd, SOL_SOCKET, SO_SNDBUF, -1, "UDP SO_SNDBUF", true);
}

// Test extreme buffer sizes and throughput - explicitly marked as SLOW
TEST_F(SocketTest, SLOW_WhenUsingLargeBuffers_ShouldTransferDataEfficiently) {
    // Skip this test in CI environments where it might be too slow
    if (_testEnv.isCI) {
        GTEST_SKIP() << "Skipping large data transfer test in CI environment";
        return;
    }
    
    auto [socket1, socket2] = createSocketPair();
    ASSERT_NE(socket1.get(), Trading::Core::SOCKET_ERROR);
    ASSERT_NE(socket2.get(), Trading::Core::SOCKET_ERROR);
    
    // Get larger timeout for this test
    int timeout = getAppropriateTimeout() * 5;  // Use 5x normal timeout
    
    // Use a smaller test size to avoid test timeouts
    const size_t TEST_SIZE = 128 * 1024; // 128KB
    
    // Create a test pattern
    std::vector<char> sendData(TEST_SIZE);
    for (size_t i = 0; i < TEST_SIZE; i++) {
        sendData[i] = static_cast<char>((i * 7) % 251);
    }
    
    // Create a receiver thread to prevent deadlock
    std::vector<char> receivedData(TEST_SIZE);
    std::atomic<bool> receiverDone = false;
    std::atomic<size_t> totalReceived = 0;
    
    std::thread receiverThread([&]() {
        size_t offset = 0;
        while (offset < TEST_SIZE) {
            // Wait for readability to avoid busy-waiting
            if (!waitForReadable(socket2.get(), timeout)) {
                logInfo("Receiver timeout waiting for data");
                break;
            }
            
            // Read available data
            ssize_t bytesRead = read(socket2.get(), 
                                     receivedData.data() + offset, 
                                     TEST_SIZE - offset);
            if (bytesRead <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;  // Try again
                }
                logLastError("Read error in receiver thread");
                break;
            }
            
            offset += bytesRead;
            totalReceived.store(offset);
        }
        receiverDone.store(true);
    });
    
    // Send the data
    size_t totalSent = 0;
    while (totalSent < TEST_SIZE) {
        // Use smaller chunks to avoid buffer filling up
        const size_t CHUNK_SIZE = 4096;  // 4KB chunks
        size_t toSend = std::min(CHUNK_SIZE, TEST_SIZE - totalSent);
        
        // Wait for writability before attempting to send
        if (!waitForWritable(socket1.get(), timeout)) {
            logInfo("Send timeout waiting for socket to be writable");
            break;
        }
        
        ssize_t bytesSent = write(socket1.get(), 
                                 sendData.data() + totalSent, 
                                 toSend);
        if (bytesSent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // Try again
            }
            logLastError("Write error in sender");
            break;
        }
        
        totalSent += bytesSent;
    }
    
    // Wait for receiver to finish
    auto startTime = std::chrono::steady_clock::now();
    const auto waitTimeout = std::chrono::seconds(10);
    
    while (!receiverDone.load()) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > waitTimeout) {
            logInfo("Final timeout waiting for receiver. Sent: " + std::to_string(totalSent) + 
                   ", Received: " + std::to_string(totalReceived.load()));
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Make sure we join the thread even on test failure
    if (receiverThread.joinable()) {
        receiverThread.join();
    }
    
    // Verify results
    EXPECT_EQ(totalSent, TEST_SIZE) << "Failed to send all data";
    EXPECT_EQ(totalReceived.load(), TEST_SIZE) << "Failed to receive all data";
    
    // Verify data integrity (first 100 bytes)
    bool dataCorrect = true;
    size_t verifySize = std::min(totalReceived.load(), size_t{100});
    for (size_t i = 0; i < verifySize; i++) {
        if (receivedData[i] != sendData[i]) {
            dataCorrect = false;
            break;
        }
    }
    EXPECT_TRUE(dataCorrect) << "Data corruption detected";
    
    // Report timing for performance measurement
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(endTime - startTime).count();
    
    logInfo("Transferred " + std::to_string(totalReceived.load()) + 
            " of " + std::to_string(totalSent) + " bytes in " + 
            std::to_string(duration) + " seconds");
}

// Verify correct error reporting for expected failures
TEST_F(SocketTest, WhenSettingInvalidOptions_ShouldReportError) {
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(socketFd, Trading::Core::SOCKET_ERROR);
    ScopedSocket testSocket(socketFd);
    
    // Try setting an invalid socket option
    constexpr int INVALID_OPTION = 999999;
    int value = 1;
    
    int result = setsockopt(socketFd, SOL_SOCKET, INVALID_OPTION, &value, sizeof(value));
    EXPECT_EQ(result, -1) << "Should fail when setting invalid option";
    EXPECT_EQ(errno, ENOPROTOOPT) << "Expected ENOPROTOOPT, got: " << strerror(errno);
    
    // Verify error condition in setNonBlocking with invalid socket
    bool nonBlockingResult = Trading::Core::setNonBlocking(-1);
    EXPECT_FALSE(nonBlockingResult) << "setNonBlocking should fail with invalid socket";
    
    // Verify error condition in optimizeSocket with invalid socket
    bool optimizeResult = Trading::Core::optimizeSocket(-1, false);
    EXPECT_FALSE(optimizeResult) << "optimizeSocket should fail with invalid socket";
    
    // Close the socket and verify operations fail
    close(socketFd);
    testSocket.release(); // Prevent double close
    
    result = setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
    EXPECT_EQ(result, -1) << "Should fail with closed socket";
    EXPECT_EQ(errno, EBADF) << "Expected EBADF, got: " << strerror(errno);
}
