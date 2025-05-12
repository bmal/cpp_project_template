#include <gtest/gtest.h>
#include "trading/socket_utils.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>

namespace Trading::Core::Test {

/**
 * @brief Test environment detection
 * 
 * Detects available platform capabilities to allow tests to adapt.
 * Tests will skip gracefully when required features aren't available.
 */
class TestEnvironment {
public:
    static TestEnvironment detect() {
        TestEnvironment env;
        
        // Detect CI environment
        env.isCI = std::getenv("CI") != nullptr;
        env.verbose = std::getenv("VERBOSE_TESTS") != nullptr;
        
        // Detect SO_TIMESTAMP support
        int testSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (testSocket != -1) {
            int optval = 1;
            env.supportsSoTimestamp = (setsockopt(testSocket, SOL_SOCKET, SO_TIMESTAMP, 
                                                &optval, sizeof(optval)) == 0);
            
            // Detect SO_BUSY_POLL support (common on newer Linux kernels)
            env.supportsBusyPoll = (setsockopt(testSocket, SOL_SOCKET, SO_BUSY_POLL, 
                                             &optval, sizeof(optval)) == 0);
            
            // Detect SO_REUSEPORT support (not available on all systems)
            env.supportsReusePort = (setsockopt(testSocket, SOL_SOCKET, SO_REUSEPORT, 
                                              &optval, sizeof(optval)) == 0);
            
            close(testSocket);
        }
        
        return env;
    }

    void logCapabilities() const {
        if (!verbose) return;
        
        std::cout
            << "\n══════════ Test Environment ══════════" << std::endl
            << "CI Environment: " << (isCI ? "Yes" : "No") << std::endl
            << "SO_TIMESTAMP support: " << (supportsSoTimestamp ? "Yes" : "No") << std::endl
            << "SO_BUSY_POLL support: " << (supportsBusyPoll ? "Yes" : "No") << std::endl
            << "SO_REUSEPORT support: " << (supportsReusePort ? "Yes" : "No") << std::endl
            << "══════════════════════════════════════\n"
            << std::endl;
    }

    bool isCI = false;
    bool verbose = false;
    bool supportsSoTimestamp = false;
    bool supportsBusyPoll = false;
    bool supportsReusePort = false;
};

/**
 * @brief RAII wrapper for socket file descriptors in tests
 * 
 * Manages cleanup of socket file descriptors to prevent resource leaks
 * even if tests fail.
 */
class ScopedSocket {
public:
    explicit ScopedSocket(int fd = SOCKET_ERROR) : _fd(fd) {}
    
    ~ScopedSocket() {
        close();
    }
    
    void close() {
        if (_fd != SOCKET_ERROR) {
            ::close(_fd);
            _fd = SOCKET_ERROR;
        }
    }
    
    [[nodiscard]] int get() const { 
        return _fd; 
    }
    
    void reset(int fd = SOCKET_ERROR) {
        close();
        _fd = fd;
    }
    
    int release() {
        int fd = _fd;
        _fd = SOCKET_ERROR;
        return fd;
    }
    
    // Move support
    ScopedSocket(ScopedSocket&& other) noexcept : _fd(other._fd) {
        other._fd = SOCKET_ERROR;
    }
    
    ScopedSocket& operator=(ScopedSocket&& other) noexcept {
        if (this != &other) {
            close();
            _fd = other._fd;
            other._fd = SOCKET_ERROR;
        }
        return *this;
    }
    
    // Prevent copying
    ScopedSocket(const ScopedSocket&) = delete;
    ScopedSocket& operator=(const ScopedSocket&) = delete;
    
private:
    int _fd;
};

/**
 * @brief Base test fixture with common socket test functionality
 * 
 * Provides shared setup/teardown and utility methods for all socket tests.
 */
class SocketTestBase : public ::testing::Test {
protected:
    void SetUp() override {
        _logger = std::make_shared<Logger>("socket_tests.log");
    }
    
    void TearDown() override {
        _logger.reset();
    }
    
    static void SetUpTestSuite() {
        _testEnv = TestEnvironment::detect();
        _testEnv.logCapabilities();
    }
    
    int getTimeoutMs() const {
        // Longer timeouts in CI environments to reduce flakiness
        return _testEnv.isCI ? 5000 : 1000;
    }
    
    /**
     * @brief Create a pair of connected sockets
     * 
     * @return std::pair<ScopedSocket, ScopedSocket> Two connected sockets
     */
    std::pair<ScopedSocket, ScopedSocket> createSocketPair() {
        int fds[2];
        
        // Try to use socketpair if available (faster and more reliable)
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0) {
            logVerbose("Created socket pair using socketpair(): " + 
                     std::to_string(fds[0]) + ", " + std::to_string(fds[1]));
            return {ScopedSocket(fds[0]), ScopedSocket(fds[1])};
        }
        
        // Fall back to manual socket setup
        logVerbose("socketpair() failed, creating manual socket pair");
        int serverFd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd == -1) {
            logError("Failed to create server socket");
            return {ScopedSocket(), ScopedSocket()};
        }
        
        // Set up server socket
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;  // Let system choose port
        
        if (bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            logError("Failed to bind server socket");
            close(serverFd);
            return {ScopedSocket(), ScopedSocket()};
        }
        
        if (listen(serverFd, 1) != 0) {
            logError("Failed to listen on server socket");
            close(serverFd);
            return {ScopedSocket(), ScopedSocket()};
        }
        
        // Get the assigned port
        socklen_t addrLen = sizeof(addr);
        if (getsockname(serverFd, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0) {
            logError("Failed to get socket name");
            close(serverFd);
            return {ScopedSocket(), ScopedSocket()};
        }
        
        // Create client socket
        int clientFd = socket(AF_INET, SOCK_STREAM, 0);
        if (clientFd == -1) {
            logError("Failed to create client socket");
            close(serverFd);
            return {ScopedSocket(), ScopedSocket()};
        }
        
        // Connect client to server
        if (connect(clientFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            logError("Failed to connect client socket");
            close(serverFd);
            close(clientFd);
            return {ScopedSocket(), ScopedSocket()};
        }
        
        // Accept connection
        int acceptedFd = accept(serverFd, nullptr, nullptr);
        if (acceptedFd == -1) {
            logError("Failed to accept connection");
            close(serverFd);
            close(clientFd);
            return {ScopedSocket(), ScopedSocket()};
        }
        
        // Close server socket - not needed anymore
        close(serverFd);
        
        logVerbose("Created manual socket pair: " + 
                 std::to_string(clientFd) + ", " + std::to_string(acceptedFd));
        return {ScopedSocket(clientFd), ScopedSocket(acceptedFd)};
    }
    
    /**
     * @brief Get the name of a valid network interface
     * 
     * @return std::string Interface name or empty string if none found
     */
    std::string getFirstNetworkInterface() {
        ifaddrs* ifAddrStruct = nullptr;
        std::string result;
        
        if (getifaddrs(&ifAddrStruct) == 0) {
            // First try to find a non-loopback interface
            for (ifaddrs* ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && 
                    ifa->ifa_name && strcmp(ifa->ifa_name, "lo") != 0) {
                    result = ifa->ifa_name;
                    break;
                }
            }
            
            // Fall back to loopback if no other interface found
            if (result.empty()) {
                for (ifaddrs* ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
                    if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && 
                        ifa->ifa_name && strcmp(ifa->ifa_name, "lo") == 0) {
                        result = ifa->ifa_name;
                        break;
                    }
                }
            }
            
            freeifaddrs(ifAddrStruct);
        }
        
        if (!result.empty()) {
            logVerbose("Found network interface: " + result);
        } else {
            logVerbose("No suitable network interface found");
        }
        
        return result;
    }
    
    /**
     * @brief Wait for a socket to be readable with timeout
     * 
     * @param fd Socket file descriptor
     * @param timeoutMs Timeout in milliseconds
     * @return true if socket became readable, false if timeout
     */
    bool waitForReadable(int fd, int timeoutMs) {
        return waitForSocketState(fd, true, timeoutMs);
    }
    
    /**
     * @brief Wait for a socket to be writable with timeout
     * 
     * @param fd Socket file descriptor
     * @param timeoutMs Timeout in milliseconds
     * @return true if socket became writable, false if timeout
     */
    bool waitForWritable(int fd, int timeoutMs) {
        return waitForSocketState(fd, false, timeoutMs);
    }
    
    /**
     * @brief Get an unused ephemeral port
     * 
     * @return int Port number in host byte order
     */
    static int getEphemeralPort() {
        // Create a socket and bind to port 0 to let the system assign a port
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            return 49152; // Fallback to start of ephemeral range
        }
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        
        if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            close(fd);
            return 49152; // Fallback to start of ephemeral range
        }
        
        socklen_t addrLen = sizeof(addr);
        if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0) {
            close(fd);
            return 49152; // Fallback to start of ephemeral range
        }
        
        int port = ntohs(addr.sin_port);
        close(fd);
        
        return port;
    }
    
    // Logging helpers
    void logVerbose(const std::string& message) const {
        if (_testEnv.verbose) {
            std::cout << "[INFO] " << message << std::endl;
        }
        if (_logger) {
            _logger->log("%\n", message);
        }
    }
    
    void logError(const std::string& message) const {
        std::string errMsg = message + ": [" + std::to_string(errno) + "] " + strerror(errno);
        std::cerr << "[ERROR] " << errMsg << std::endl;
        if (_logger) {
            _logger->log("%\n", errMsg);
        }
    }
    
private:
    /**
     * @brief Wait for a socket to be readable or writable with timeout
     * 
     * @param fd Socket file descriptor
     * @param forReadable true to wait for readable, false for writable
     * @param timeoutMs Timeout in milliseconds
     * @return true if socket reached desired state, false if timeout
     */
    bool waitForSocketState(int fd, bool forReadable, int timeoutMs) {
        if (fd == SOCKET_ERROR) {
            logVerbose("waitForSocketState: Invalid socket");
            return false;
        }
        
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        
        struct timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        
        std::string stateStr = forReadable ? "readable" : "writable";
        logVerbose("Waiting for socket " + std::to_string(fd) + " to become " 
                 + stateStr + " (timeout: " + std::to_string(timeoutMs) + "ms)");
        
        int result = select(fd + 1, 
                         forReadable ? &fds : nullptr, 
                         forReadable ? nullptr : &fds, 
                         nullptr, &tv);
        
        if (result > 0) {
            logVerbose("Socket " + std::to_string(fd) + " is now " + stateStr);
            return true;
        } else if (result == 0) {
            logVerbose("Timeout waiting for socket " + std::to_string(fd) + " to be " + stateStr);
            return false;
        } else {
            logError("Error in select() waiting for " + stateStr);
            return false;
        }
    }
    
protected:
    std::shared_ptr<Logger> _logger;
    static TestEnvironment _testEnv;
};

TestEnvironment SocketTestBase::_testEnv{};

/*****************************************************************
 * Socket Operation Tests
 * 
 * Tests for basic socket operations like setting non-blocking mode
 *****************************************************************/
class SocketOperationsTest : public SocketTestBase {};

TEST_F(SocketOperationsTest, WhenSettingNonBlocking_ShouldSetFlagCorrectly) {
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(socketFd, SOCKET_ERROR) << "Failed to create socket";
    ScopedSocket testSocket(socketFd);
    
    // Get original flags to verify we only add O_NONBLOCK
    int originalFlags = fcntl(socketFd, F_GETFL, 0);
    ASSERT_NE(originalFlags, -1) << "Failed to get socket flags: " << strerror(errno);
    
    ASSERT_TRUE(setNonBlocking(socketFd)) << "Failed to set non-blocking mode: " << strerror(errno);
    
    // Verify O_NONBLOCK flag was set
    int newFlags = fcntl(socketFd, F_GETFL, 0);
    ASSERT_NE(newFlags, -1) << "Failed to get socket flags after setting non-blocking: " << strerror(errno);
    EXPECT_EQ(newFlags, originalFlags | O_NONBLOCK) << "O_NONBLOCK flag not set correctly";
}

TEST_F(SocketOperationsTest, WhenSettingNonBlockingOnInvalidFd_ShouldReturnFalse) {
    const int invalidFd = -1;
    
    errno = 0; // Reset errno to detect changes
    bool result = setNonBlocking(invalidFd);
    
    EXPECT_FALSE(result) << "setNonBlocking should fail with invalid fd";
    EXPECT_NE(errno, 0) << "errno should be set after failure";
}

TEST_F(SocketOperationsTest, WhenConnectingWithNonBlockingSocket_ShouldReturnImmediately) {
    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(socketFd, SOCKET_ERROR) << "Failed to create socket";
    ScopedSocket testSocket(socketFd);
    
    ASSERT_TRUE(setNonBlocking(socketFd)) << "Failed to set non-blocking mode";
    
    // Use a non-routable address that would normally cause connect to block
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("240.0.0.1"); // Reserved address space
    addr.sin_port = htons(1); // Reserved port
    
    auto startTime = std::chrono::steady_clock::now();
    
    int result = connect(socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    EXPECT_EQ(result, -1) << "Non-blocking connect should return -1 immediately";
    EXPECT_TRUE(errno == EINPROGRESS || errno == ENETUNREACH) 
        << "Expected EINPROGRESS or ENETUNREACH, got: " << strerror(errno);
    
    // Connection attempt should return very quickly (well under 100ms)
    EXPECT_LT(duration.count(), 100) 
        << "Non-blocking connect took too long: " << duration.count() << "ms";
}

/*****************************************************************
 * Socket Data Transmission Tests
 * 
 * Tests for sending and receiving data through sockets
 *****************************************************************/
class SocketDataTest : public SocketTestBase {};

TEST_F(SocketDataTest, WhenSendingData_ShouldTransmitEntireMessage) {
    auto [sendSocket, recvSocket] = createSocketPair();
    ASSERT_NE(sendSocket.get(), SOCKET_ERROR) << "Failed to create socket pair";
    ASSERT_NE(recvSocket.get(), SOCKET_ERROR) << "Failed to create socket pair";
    
    const char* testMessage = "Hello, socket world!";
    const size_t messageLen = strlen(testMessage);
    
    ssize_t bytesSent = write(sendSocket.get(), testMessage, messageLen);
    ASSERT_EQ(bytesSent, static_cast<ssize_t>(messageLen)) 
        << "Failed to send entire message: " << strerror(errno);
    
    char buffer[64] = {0};
    ssize_t bytesRead = read(recvSocket.get(), buffer, sizeof(buffer));
    
    ASSERT_EQ(bytesRead, static_cast<ssize_t>(messageLen)) 
        << "Failed to receive entire message: " << strerror(errno);
    EXPECT_STREQ(buffer, testMessage) << "Received message doesn't match sent message";
}

TEST_F(SocketDataTest, WhenReadingFromEmptyNonBlockingSocket_ShouldReturnEAGAIN) {
    auto [sendSocket, recvSocket] = createSocketPair();
    ASSERT_NE(sendSocket.get(), SOCKET_ERROR) << "Failed to create socket pair";
    ASSERT_NE(recvSocket.get(), SOCKET_ERROR) << "Failed to create socket pair";
    
    ASSERT_TRUE(setNonBlocking(recvSocket.get())) << "Failed to set non-blocking mode";
    
    char buffer[64] = {0};
    ssize_t bytesRead = read(recvSocket.get(), buffer, sizeof(buffer));
    
    EXPECT_EQ(bytesRead, -1) << "Non-blocking read from empty socket should return -1";
    EXPECT_TRUE(errno == EAGAIN || errno == EWOULDBLOCK) 
        << "Expected EAGAIN or EWOULDBLOCK, got: " << strerror(errno);
}

TEST_F(SocketDataTest, WhenWritingToNonBlockingSocket_ShouldSucceedWithAvailableBuffer) {
    auto [sendSocket, recvSocket] = createSocketPair();
    ASSERT_NE(sendSocket.get(), SOCKET_ERROR) << "Failed to create socket pair";
    ASSERT_NE(recvSocket.get(), SOCKET_ERROR) << "Failed to create socket pair";
    
    ASSERT_TRUE(setNonBlocking(sendSocket.get())) << "Failed to set non-blocking mode";
    
    const char* testMessage = "Test non-blocking write";
    const size_t messageLen = strlen(testMessage);
    
    ssize_t bytesSent = write(sendSocket.get(), testMessage, messageLen);
    EXPECT_EQ(bytesSent, static_cast<ssize_t>(messageLen)) 
        << "Failed to send data on non-blocking socket: " << strerror(errno);
}

/*****************************************************************
 * Socket Option Tests
 * 
 * Tests for setting and getting socket options
 *****************************************************************/
class SocketOptionTest : public SocketTestBase {};

TEST_F(SocketOptionTest, WhenSettingSOTimestamp_ShouldEnableOption) {
    if (!_testEnv.supportsSoTimestamp) {
        GTEST_SKIP() << "SO_TIMESTAMP not supported in this environment";
    }
    
    int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_NE(socketFd, SOCKET_ERROR) << "Failed to create socket";
    ScopedSocket testSocket(socketFd);
    
    ASSERT_TRUE(setSOTimestamp(socketFd)) << "Failed to set SO_TIMESTAMP option";
    
    // Verify the option was set
    int enabled = 0;
    socklen_t optLen = sizeof(enabled);
    int result = getsockopt(socketFd, SOL_SOCKET, SO_TIMESTAMP, &enabled, &optLen);
    
    ASSERT_EQ(result, 0) << "Failed to get SO_TIMESTAMP option: " << strerror(errno);
    EXPECT_NE(enabled, 0) << "SO_TIMESTAMP was not enabled";
}

TEST_F(SocketOptionTest, WhenSettingSOTimestampOnInvalidFd_ShouldReturnFalse) {
    const int invalidFd = -1;
    
    errno = 0; // Reset errno to detect changes
    bool result = setSOTimestamp(invalidFd);
    
    EXPECT_FALSE(result) << "setSOTimestamp should fail with invalid fd";
    EXPECT_NE(errno, 0) << "errno should be set after failure";
}

/*****************************************************************
 * Network Interface Tests
 * 
 * Tests for retrieving network interface information
 *****************************************************************/
class NetworkInterfaceTest : public SocketTestBase {};

TEST_F(NetworkInterfaceTest, WhenGettingValidInterfaceIP_ShouldReturnCorrectAddress) {
    std::string interfaceName = getFirstNetworkInterface();
    if (interfaceName.empty()) {
        GTEST_SKIP() << "No suitable network interface found for testing";
    }
    
    char ipBuffer[MAX_IP_LENGTH];
    bool success = getInterfaceIP(interfaceName, ipBuffer);
    
    ASSERT_TRUE(success) << "Failed to get IP for interface " << interfaceName;
    ASSERT_STRNE(ipBuffer, "") << "Returned IP address is empty";
    
    // Verify it's a valid IPv4 address
    struct sockaddr_in sa{};
    int result = inet_pton(AF_INET, ipBuffer, &(sa.sin_addr));
    EXPECT_EQ(result, 1) << "Not a valid IPv4 address: " << ipBuffer;
}

TEST_F(NetworkInterfaceTest, WhenGettingInvalidInterfaceIP_ShouldReturnFalse) {
    char ipBuffer[MAX_IP_LENGTH] = "original value";
    
    bool success = getInterfaceIP("nonexistent_iface", ipBuffer);
    
    EXPECT_FALSE(success) << "getInterfaceIP should fail for non-existent interface";
    EXPECT_STREQ(ipBuffer, "") << "Buffer should be cleared on failure";
}

TEST_F(NetworkInterfaceTest, WhenGettingEmptyInterfaceIP_ShouldReturnFalse) {
    char ipBuffer[MAX_IP_LENGTH] = "original value";
    
    bool success = getInterfaceIP("", ipBuffer);
    
    EXPECT_FALSE(success) << "getInterfaceIP should fail for empty interface name";
    EXPECT_STREQ(ipBuffer, "") << "Buffer should be cleared on failure";
}

TEST_F(NetworkInterfaceTest, WhenCalledMultipleTimes_ShouldReturnConsistentResults) {
    std::string interfaceName = getFirstNetworkInterface();
    if (interfaceName.empty()) {
        GTEST_SKIP() << "No suitable network interface found for testing";
    }
    
    char firstResult[MAX_IP_LENGTH];
    bool firstSuccess = getInterfaceIP(interfaceName, firstResult);
    ASSERT_TRUE(firstSuccess) << "Failed to get IP on first call";
    
    // Call again and verify consistent results
    for (int i = 0; i < 3; i++) {
        char nextResult[MAX_IP_LENGTH];
        bool nextSuccess = getInterfaceIP(interfaceName, nextResult);
        
        EXPECT_TRUE(nextSuccess) << "Failed to get IP on call " << (i + 2);
        EXPECT_STREQ(nextResult, firstResult) << "Inconsistent result on call " << (i + 2);
    }
}

/*****************************************************************
 * Socket Class Construction and Destruction Tests
 *
 * Tests for Socket class construction, destruction and state
 *****************************************************************/
class SocketClassBasicTest : public SocketTestBase {};

TEST_F(SocketClassBasicTest, WhenDefaultConstructed_SocketShouldBeInvalid) {
    Socket socket;
    
    EXPECT_FALSE(socket.isValid()) << "Default-constructed Socket should be invalid";
    EXPECT_EQ(socket.getFd(), SOCKET_ERROR) << "Default-constructed Socket should have SOCKET_ERROR fd";
}

TEST_F(SocketClassBasicTest, WhenConstructedWithValidFd_SocketShouldBeValid) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, SOCKET_ERROR) << "Failed to create raw socket";
    
    Socket socket(fd);
    
    EXPECT_TRUE(socket.isValid()) << "Socket should be valid with valid fd";
    EXPECT_EQ(socket.getFd(), fd) << "Socket should store the provided fd";
}

TEST_F(SocketClassBasicTest, WhenDestructed_SocketShouldCloseFd) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, SOCKET_ERROR) << "Failed to create raw socket";
    
    {
        Socket socket(fd);
    } // Socket goes out of scope here
    
    // Verify fd is closed by checking fcntl fails with EBADF
    int result = fcntl(fd, F_GETFL);
    EXPECT_EQ(result, -1) << "fd should be closed after Socket destruction";
    EXPECT_EQ(errno, EBADF) << "Expected EBADF for closed fd, got: " << strerror(errno);
}

/*****************************************************************
 * Socket Class Reset and Release Tests
 *
 * Tests for Socket reset and release functionality
 *****************************************************************/
class SocketClassResetTest : public SocketTestBase {};

TEST_F(SocketClassResetTest, WhenReset_ShouldCloseOldFdAndAdoptNew) {
    int firstFd = socket(AF_INET, SOCK_STREAM, 0);
    int secondFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(firstFd, SOCKET_ERROR) << "Failed to create first socket";
    ASSERT_NE(secondFd, SOCKET_ERROR) << "Failed to create second socket";
    
    Socket socket(firstFd);
    ASSERT_EQ(socket.getFd(), firstFd) << "Socket should initially hold first fd";
    
    socket.reset(secondFd);
    
    // Verify first fd was closed
    int result = fcntl(firstFd, F_GETFL);
    EXPECT_EQ(result, -1) << "Original fd should be closed after reset";
    EXPECT_EQ(errno, EBADF) << "Expected EBADF for closed fd";
    
    // Verify second fd is now owned by socket
    EXPECT_EQ(socket.getFd(), secondFd) << "Socket should now hold second fd";
    result = fcntl(secondFd, F_GETFL);
    EXPECT_NE(result, -1) << "New fd should be valid after reset";
}

TEST_F(SocketClassResetTest, WhenResetWithInvalidFd_ShouldCloseCurrentAndBecomeInvalid) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, SOCKET_ERROR) << "Failed to create socket";
    
    Socket socket(fd);
    ASSERT_TRUE(socket.isValid()) << "Socket should initially be valid";
    
    socket.reset(SOCKET_ERROR);
    
    // Verify original fd was closed
    int result = fcntl(fd, F_GETFL);
    EXPECT_EQ(result, -1) << "Original fd should be closed after reset";
    EXPECT_EQ(errno, EBADF) << "Expected EBADF for closed fd";
    
    // Verify socket is now invalid
    EXPECT_FALSE(socket.isValid()) << "Socket should be invalid after reset with SOCKET_ERROR";
    EXPECT_EQ(socket.getFd(), SOCKET_ERROR) << "Socket should have SOCKET_ERROR fd";
}

TEST_F(SocketClassResetTest, WhenReleased_ShouldBecomeInvalidWithoutClosingFd) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, SOCKET_ERROR) << "Failed to create socket";
    
    Socket socket(fd);
    ASSERT_TRUE(socket.isValid()) << "Socket should initially be valid";
    
    int releasedFd = socket.release();
    
    // Verify socket state
    EXPECT_FALSE(socket.isValid()) << "Socket should be invalid after release";
    EXPECT_EQ(socket.getFd(), SOCKET_ERROR) << "Socket should have SOCKET_ERROR fd after release";
    
    // Verify fd was not closed
    EXPECT_EQ(releasedFd, fd) << "Released fd should match original fd";
    int result = fcntl(fd, F_GETFL);
    EXPECT_NE(result, -1) << "Released fd should still be valid";
    
    // Clean up
    close(fd);
}

/*****************************************************************
 * Socket Class Move Semantics Tests
 *
 * Tests for Socket class move construction and assignment
 *****************************************************************/
class SocketClassMoveTest : public SocketTestBase {};

TEST_F(SocketClassMoveTest, WhenMoveConstructed_ShouldTransferOwnership) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(fd, SOCKET_ERROR) << "Failed to create socket";
    
    Socket original(fd);
    ASSERT_TRUE(original.isValid()) << "Original socket should be valid";
    
    Socket moved(std::move(original));
    
    // Verify ownership transfer
    EXPECT_FALSE(original.isValid()) << "Original socket should be invalid after move";
    EXPECT_EQ(original.getFd(), SOCKET_ERROR) << "Original socket should have SOCKET_ERROR fd after move";
    
    EXPECT_TRUE(moved.isValid()) << "Moved-to socket should be valid";
    EXPECT_EQ(moved.getFd(), fd) << "Moved-to socket should have the original fd";
}

TEST_F(SocketClassMoveTest, WhenMoveAssigned_ShouldTransferOwnershipAndCloseOldFd) {
    int sourceFd = socket(AF_INET, SOCK_STREAM, 0);
    int targetFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(sourceFd, SOCKET_ERROR) << "Failed to create source socket";
    ASSERT_NE(targetFd, SOCKET_ERROR) << "Failed to create target socket";
    
    Socket source(sourceFd);
    Socket target(targetFd);
    
    ASSERT_TRUE(source.isValid()) << "Source socket should be valid";
    ASSERT_TRUE(target.isValid()) << "Target socket should be valid";
    
    target = std::move(source);
    
    // Verify source state
    EXPECT_FALSE(source.isValid()) << "Source socket should be invalid after move";
    EXPECT_EQ(source.getFd(), SOCKET_ERROR) << "Source socket should have SOCKET_ERROR fd after move";
    
    // Verify target now owns source fd
    EXPECT_TRUE(target.isValid()) << "Target socket should be valid after move";
    EXPECT_EQ(target.getFd(), sourceFd) << "Target socket should have the source fd";
    
    // Verify original target fd was closed
    int result = fcntl(targetFd, F_GETFL);
    EXPECT_EQ(result, -1) << "Target's original fd should be closed";
    EXPECT_EQ(errno, EBADF) << "Expected EBADF for closed fd";
}

/*****************************************************************
 * ScopedAddrInfo Tests
 *
 * Tests for ScopedAddrInfo RAII wrapper
 *****************************************************************/
class ScopedAddrInfoTest : public SocketTestBase {};

TEST_F(ScopedAddrInfoTest, WhenDefaultConstructed_ShouldBeInvalid) {
    ScopedAddrInfo emptyInfo;
    
    EXPECT_EQ(emptyInfo.get(), nullptr) << "Default-constructed ScopedAddrInfo should have nullptr";
    EXPECT_FALSE(bool(emptyInfo)) << "Default-constructed ScopedAddrInfo should convert to false";
}

TEST_F(ScopedAddrInfoTest, WhenConstructedWithValidPtr_ShouldBeValid) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    
    addrinfo* rawInfo = nullptr;
    int result = getaddrinfo("localhost", "80", &hints, &rawInfo);
    ASSERT_EQ(result, 0) << "getaddrinfo failed: " << gai_strerror(result);
    ASSERT_NE(rawInfo, nullptr) << "getaddrinfo returned nullptr";
    
    ScopedAddrInfo scopedInfo(rawInfo);
    
    EXPECT_EQ(scopedInfo.get(), rawInfo) << "ScopedAddrInfo should store the provided pointer";
    EXPECT_TRUE(bool(scopedInfo)) << "ScopedAddrInfo should convert to true with valid pointer";
}

TEST_F(ScopedAddrInfoTest, WhenMoveConstructed_ShouldTransferOwnership) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    
    addrinfo* rawInfo = nullptr;
    ASSERT_EQ(getaddrinfo("localhost", "80", &hints, &rawInfo), 0) << "getaddrinfo failed";
    ASSERT_NE(rawInfo, nullptr) << "getaddrinfo returned nullptr";
    
    ScopedAddrInfo original(rawInfo);
    ScopedAddrInfo moved(std::move(original));
    
    // Verify ownership transfer
    EXPECT_EQ(original.get(), nullptr) << "Original should be nullptr after move";
    EXPECT_FALSE(bool(original)) << "Original should convert to false after move";
    
    EXPECT_EQ(moved.get(), rawInfo) << "Moved-to object should have the original pointer";
    EXPECT_TRUE(bool(moved)) << "Moved-to object should convert to true";
}

TEST_F(ScopedAddrInfoTest, WhenMoveAssigned_ShouldTransferOwnership) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    
    addrinfo* firstInfo = nullptr;
    addrinfo* secondInfo = nullptr;
    
    ASSERT_EQ(getaddrinfo("localhost", "80", &hints, &firstInfo), 0) << "First getaddrinfo failed";
    ASSERT_EQ(getaddrinfo("127.0.0.1", "443", &hints, &secondInfo), 0) << "Second getaddrinfo failed";
    ASSERT_NE(firstInfo, nullptr) << "First getaddrinfo returned nullptr";
    ASSERT_NE(secondInfo, nullptr) << "Second getaddrinfo returned nullptr";
    
    ScopedAddrInfo first(firstInfo);
    ScopedAddrInfo second(secondInfo);
    
    // Track the second pointer for verification
    addrinfo* secondPtr = second.get();
    
    first = std::move(second);
    
    // Verify second is now empty
    EXPECT_EQ(second.get(), nullptr) << "Source should be nullptr after move";
    EXPECT_FALSE(bool(second)) << "Source should convert to false after move";
    
    // Verify first now has second's data
    EXPECT_EQ(first.get(), secondPtr) << "Target should have source's pointer after move";
    EXPECT_TRUE(bool(first)) << "Target should convert to true after move";
    
    // Note: Can't verify firstInfo was freed without risking UB
}

/*****************************************************************
 * Address Resolution Tests
 *
 * Tests for address resolution functions
 *****************************************************************/
class AddressResolutionTest : public SocketTestBase {};

TEST_F(AddressResolutionTest, WhenResolvingLocalhost_ShouldReturnValidInfo) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    auto addrInfo = resolveAddressInfo("localhost", "80", &hints, *_logger);
    
    ASSERT_TRUE(bool(addrInfo)) << "resolveAddressInfo should succeed for localhost";
    ASSERT_NE(addrInfo.get(), nullptr) << "resolveAddressInfo should return non-null addrinfo";
    
    EXPECT_EQ(addrInfo.get()->ai_family, AF_INET) << "Should respect requested address family";
    EXPECT_EQ(addrInfo.get()->ai_socktype, SOCK_STREAM) << "Should respect requested socket type";
}

TEST_F(AddressResolutionTest, WhenResolvingIPLiteral_ShouldReturnValidInfo) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    auto addrInfo = resolveAddressInfo("127.0.0.1", "8080", &hints, *_logger);
    
    ASSERT_TRUE(bool(addrInfo)) << "resolveAddressInfo should succeed for IP literals";
    ASSERT_NE(addrInfo.get(), nullptr) << "resolveAddressInfo should return non-null addrinfo";
    
    const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(addrInfo.get()->ai_addr);
    
    // Convert numeric address back to string for comparison
    char addrStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), addrStr, INET_ADDRSTRLEN);
    
    EXPECT_STREQ(addrStr, "127.0.0.1") << "Should correctly parse IP literal";
    EXPECT_EQ(ntohs(addr->sin_port), 8080) << "Should correctly parse numeric port";
}

TEST_F(AddressResolutionTest, WhenResolvingNonexistentHost_ShouldReturnNull) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    auto addrInfo = resolveAddressInfo("nonexistent.example.invalid", "80", &hints, *_logger);
    
    EXPECT_FALSE(bool(addrInfo)) << "Should fail for non-existent hostname";
    EXPECT_EQ(addrInfo.get(), nullptr) << "Should return nullptr for non-existent hostname";
}

TEST_F(AddressResolutionTest, WhenResolvingWithServiceName_ShouldSetCorrectPort) {
    struct servent* service = getservbyname("http", "tcp");
    if (!service) {
        GTEST_SKIP() << "Service name resolution not available on this system";
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    auto addrInfo = resolveAddressInfo("localhost", "http", &hints, *_logger);
    
    ASSERT_TRUE(bool(addrInfo)) << "resolveAddressInfo should succeed with service name";
    ASSERT_NE(addrInfo.get(), nullptr) << "resolveAddressInfo should return non-null addrinfo";
    
    const sockaddr_in* addr = reinterpret_cast<const sockaddr_in*>(addrInfo.get()->ai_addr);
    EXPECT_EQ(ntohs(addr->sin_port), 80) << "Should map 'http' to port 80";
}

TEST_F(AddressResolutionTest, WhenResolvingWithInvalidService_ShouldReturnNull) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    auto addrInfo = resolveAddressInfo("localhost", "invalid-service", &hints, *_logger);
    
    EXPECT_FALSE(bool(addrInfo)) << "Should fail for invalid service name";
    EXPECT_EQ(addrInfo.get(), nullptr) << "Should return nullptr for invalid service name";
}

/*****************************************************************
 * EINTR Handling Tests
 *
 * Tests for proper handling of EINTR in socket operations
 *****************************************************************/
class EINTRHandlingTest : public SocketTestBase {};

#ifdef ENABLE_SIGNAL_TESTS
TEST_F(EINTRHandlingTest, WhenSocketDestructedDuringSignal_ShouldCloseDescriptorProperly) {
    // This test verifies the Socket class can handle EINTR during close()
    
    int pipefd[2];
    ASSERT_NE(pipe(pipefd), -1) << "Failed to create pipe";
    
    pid_t pid = fork();
    ASSERT_NE(pid, -1) << "Failed to fork";
    
    if (pid == 0) {
        // Child process
        close(pipefd[0]);  // Close read end
        
        // Wait for parent readiness signal
        char ready;
        ASSERT_EQ(read(pipefd[1], &ready, 1), 1) << "Failed to read readiness signal";
        
        // Send SIGUSR1 to parent
        kill(getppid(), SIGUSR1);
        close(pipefd[1]);
        exit(0);
    } else {
        // Parent process
        close(pipefd[1]);  // Close write end
        
        // Set up signal handler that doesn't have SA_RESTART
        struct sigaction sa{};
        sa.sa_handler = [](int) {}; // Empty handler
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;  // No SA_RESTART to ensure EINTR occurs
        
        struct sigaction oldAction;
        ASSERT_NE(sigaction(SIGUSR1, &sa, &oldAction), -1) << "Failed to set up signal handler";
        
        // Create socket
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        ASSERT_NE(fd, -1) << "Failed to create socket";
        
        // Signal child that we're ready
        char ready = 1;
        ASSERT_EQ(write(pipefd[0], &ready, 1), 1) << "Failed to signal readiness";
        
        // Socket destruction during signal handler
        {
            Socket socketObj(fd);
        } // Socket destroyed here, close() might be interrupted by SIGUSR1
        
        // Verify fd was closed despite potential EINTR
        int result = fcntl(fd, F_GETFL);
        EXPECT_EQ(result, -1) << "fd should be closed even if interrupted by signal";
        EXPECT_EQ(errno, EBADF) << "Expected EBADF for closed fd";
        
        // Cleanup
        close(pipefd[0]);
        int status;
        waitpid(pid, &status, 0);
        
        // Restore original signal handler
        ASSERT_NE(sigaction(SIGUSR1, &oldAction, nullptr), -1) << "Failed to restore signal handler";
    }
}
#endif

} // namespace Trading::Core::Test
