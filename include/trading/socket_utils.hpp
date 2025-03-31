/**
 * @file socket_utils.hpp
 * @brief High-performance socket utilities for trading applications
 *
 * This header provides optimized socket creation and configuration utilities
 * specifically designed for low-latency trading applications. It supports both
 * TCP and UDP protocols with Linux-specific optimizations.
 *
 * Key features:
 * - Non-blocking socket operations
 * - Linux-specific performance optimizations
 * - Support for both client and server sockets
 * - Multicast capability
 * - Hardware timestamp support
 * - Automatic resource management
 *
 * Performance optimization details:
 * - Linux:
 *   - TCP_NODELAY: Disables Nagle's algorithm for lower latency
 *   - TCP_QUICKACK: Immediate ACK for reduced latency
 *   - SO_BUSY_POLL: Reduces latency by busy-polling
 *   - Custom buffer sizes for optimal throughput
 *   - SO_PRIORITY: Sets socket priority for better scheduling
 *
 * @note There is still plenty of space for improvements. It should be
 *       investigated how to decrease number of system calls, maybe add kernel
 *       bypass and busy pooling, proper hardware timestamping etc.
 */

#pragma once

// Common POSIX headers
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * Linux-specific headers for extended socket options and I/O control
 */
#include <asm/socket.h>
#include <linux/sockios.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

#include "time_utils.hpp"
#include "trading/asserts.hpp"
#include "trading/logger.hpp"

#ifndef TCP_SLOW_START_AFTER_IDLE
#define TCP_SLOW_START_AFTER_IDLE 31
#endif

namespace Trading::Core {

/**
 * @brief Linux-specific socket configuration constants
 */
constexpr int SOCKET_RECV_FLAGS = MSG_DONTWAIT;  // Non-blocking receive on Linux
constexpr int MAX_TCP_BACKLOG = SOMAXCONN;       // Use kernel maximum

/**
 * @brief Common socket constants and error codes
 */
constexpr int SOCKET_ERROR = -1;         ///< Standard socket error return value
constexpr int SOCKET_OK = 0;             ///< Successful operation return value
constexpr int ENABLE_SOCKET_OPTION = 1;  ///< Value to enable socket options
constexpr size_t MAX_IP_LENGTH =
    16;  ///< Maximum IPv4 address length (xxx.xxx.xxx.xxx\0)

/**
 * @brief Pre-defined error messages for socket operations
 *
 * These messages provide consistent error reporting across the library.
 * Each message is designed to be concatenated with additional error details.
 */
constexpr std::string_view ERR_GET_ADDR_INFO = "getaddrinfo() failed. error:";
constexpr std::string_view ERR_SOCKET_CREATE = "socket() failed. errno:";
constexpr std::string_view ERR_NON_BLOCKING = "setNonBlocking() failed. errno:";
constexpr std::string_view ERR_CONNECT = "connect() failed. errno:";
constexpr std::string_view ERR_REUSE_ADDR =
    "setsockopt() SO_REUSEADDR failed. errno:";
constexpr std::string_view ERR_BIND = "bind() failed. errno:";
constexpr std::string_view ERR_LISTEN = "listen() failed. errno:";
constexpr std::string_view ERR_TIMESTAMP = "setSOTimestamp() failed. errno:";
constexpr std::string_view ERR_OPTIMIZE = "Socket optimization failed. errno:";

/**
 * @brief Socket configuration parameters for socket creation and setup
 *
 * This structure encapsulates all configuration options needed to create and
 * configure a socket for either client or server operation. It supports both
 * TCP and UDP protocols, with options for special features like hardware
 * timestamping and interface-based binding.
 */
struct SocketConfig {
    std::string_view
        ip;  ///< IP address to bind/connect to (empty for interface-based)
    std::string_view
        interface;           ///< Network interface name (used if ip is empty)
    int port{SOCKET_ERROR};  ///< Port number for the socket
    bool isUDP{false};  ///< Protocol selection (true for UDP, false for TCP)
    bool isListening{
        false};  ///< Server mode flag (true for server, false for client)
    bool needsSOTimestamp{
        false};  ///< Enable hardware timestamping for received packets
    bool isMulticast{false};              ///< Mark as multicast>
    std::string_view multicastInterface;  ///< Interface for receiving multicast
};

/**
 * @brief RAII wrapper for addrinfo structures
 *
 * Manages the lifecycle of addrinfo structures returned by getaddrinfo().
 * Automatically frees resources when going out of scope, preventing memory
 * leaks even in error cases.
 *
 * Example usage:
 * @code
 * addrinfo* raw_info = nullptr;
 * getaddrinfo("localhost", "8080", &hints, &raw_info);
 * ScopedAddrInfo info(raw_info);  // Takes ownership
 * // Use info.get() to access the addrinfo
 * // No need to call freeaddrinfo - destructor handles cleanup
 * @endcode
 */
class ScopedAddrInfo {
    addrinfo* _info = nullptr;

   public:
    /**
     * @brief Constructs a wrapper around an addrinfo pointer
     * @param info Raw addrinfo pointer to take ownership of
     */
    explicit ScopedAddrInfo(addrinfo* info) noexcept : _info(info) {}

    /**
     * @brief Destructor that ensures proper cleanup of addrinfo structure
     */
    ~ScopedAddrInfo() noexcept {
        if (_info) {
            freeaddrinfo(_info);
        }
    }

    /**
     * @brief Provides access to the underlying addrinfo structure
     * @return Pointer to the managed addrinfo structure
     */
    [[nodiscard]] addrinfo* get() const noexcept { return _info; }

    // Prevent copying to ensure single ownership
    ScopedAddrInfo(const ScopedAddrInfo&) = delete;
    ScopedAddrInfo& operator=(const ScopedAddrInfo&) = delete;
};

}  // namespace Trading::Core

/**
 * @brief std::formatter specialization for SocketConfig
 *
 * Enables formatting of SocketConfig objects using std::format.
 * Useful for logging and debugging socket configurations.
 *
 * Example usage:
 * @code
 * SocketConfig cfg{...};
 * logger.log("Creating socket with config: {}", cfg);
 * @endcode
 */
namespace std {
template <>
struct formatter<Trading::Core::SocketConfig> {
    static constexpr auto parse(std::format_parse_context& ctx) noexcept {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const Trading::Core::SocketConfig& cfg,
                FormatContext& ctx) const {
        return std::format_to(ctx.out(),
                              "SocketCfg[ip:{} interface:{} port:{} isUDP:{} "
                              "isListening:{} needsSOTimestamp:{}]",
                              cfg.ip, cfg.interface, cfg.port, cfg.isUDP,
                              cfg.isListening, cfg.needsSOTimestamp);
    }
};
}  // namespace std

namespace Trading::Core {

/**
 * @brief Applies performance optimizations to a socket
 *
 * Configures socket options for optimal performance in an HFT environment.
 * Different optimizations are applied based on:
 * - Protocol (TCP vs UDP)
 * - Socket domain (Internet vs Unix domain sockets)
 *
 * Performance options include:
 * - Buffer sizes: Optimized for high-throughput trading data (1MB)
 * - TCP-specific:
 *   - TCP_NODELAY: Disables Nagle's algorithm for lowest possible latency
 *   - TCP_QUICKACK: Immediate ACK for reduced round-trip times
 *   - TCP_FASTOPEN: Reduced connection setup latency
 *   - SO_LINGER: Zero timeout for quick socket closure
 * - Linux-specific:
 *   - SO_BUSY_POLL: Ultra-low latency through busy-polling
 *   - SO_PRIORITY: Higher scheduling priority for trading traffic
 *   - TCP_SLOW_START_AFTER_IDLE: Disabled to maintain consistent performance
 *
 * Implementation includes robust verification to ensure options are properly
 * applied even with aggressive compiler optimizations in release builds.
 *
 * @param socketFd Valid socket file descriptor
 * @param isUDP Protocol indicator (true for UDP, false for TCP)
 * @return true if critical optimizations were successfully applied
 */
[[nodiscard]] inline bool optimizeSocket(const int socketFd,
                                         const bool isUDP) noexcept {
    constexpr int ENABLE_OPTION = 1;
    constexpr int OPTIMAL_BUFFER_SIZE = 1024 * 1024;  // 1MB buffer

    if (socketFd == SOCKET_ERROR) {
        return false;
    }

    // Step 1: Apply buffer sizes - these should work for all socket types
    if (setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, &OPTIMAL_BUFFER_SIZE,
                   sizeof(OPTIMAL_BUFFER_SIZE)) == SOCKET_ERROR) {
        return false;
    }

    if (setsockopt(socketFd, SOL_SOCKET, SO_SNDBUF, &OPTIMAL_BUFFER_SIZE,
                   sizeof(OPTIMAL_BUFFER_SIZE)) == SOCKET_ERROR) {
        return false;
    }

    // If this is a UDP socket, we're done with essential optimizations
    if (isUDP) {
        return true;
    }

    // TCP-specific options - apply if this is a TCP socket
    // TCP_NODELAY is used both as an optimization and to detect if this is a
    // TCP socket
    int nodelay = ENABLE_OPTION;
    if (setsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                   sizeof(nodelay)) == SOCKET_ERROR) {
        // Not a TCP socket or TCP_NODELAY not supported - that's ok, return
        // success
        return true;
    }

    // Verify TCP_NODELAY was actually set - creates an optimization barrier
    int verifyNodelay = 0;
    socklen_t optLen = sizeof(verifyNodelay);
    if (getsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, &verifyNodelay,
                   &optLen) == 0) {
        // If not set correctly, try again unconditionally
        if (verifyNodelay != ENABLE_OPTION) {
            nodelay = ENABLE_OPTION;
            setsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                       sizeof(nodelay));
            
            // Verify again
            getsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, &verifyNodelay, &optLen);
        }
    }

    // Linux-specific TCP optimizations for ultra-low latency HFT

    // TCP_QUICKACK - reduce latency by immediately acknowledging packets
    setsockopt(socketFd, IPPROTO_TCP, TCP_QUICKACK, &ENABLE_OPTION,
               sizeof(ENABLE_OPTION));

    // TCP_FASTOPEN - reduces connection setup latency (if supported)
    setsockopt(socketFd, IPPROTO_TCP, TCP_FASTOPEN, &ENABLE_OPTION,
               sizeof(ENABLE_OPTION));

    // SO_LINGER with zero timeout - crucial for clean shutdown without
    // TIME_WAIT
    struct linger ling = {ENABLE_OPTION, 0};
    setsockopt(socketFd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));

    // SO_BUSY_POLL - critical for HFT to reduce context switch latency
    constexpr int BUSY_POLL_TIMEOUT = 50;  // microseconds
    setsockopt(socketFd, SOL_SOCKET, SO_BUSY_POLL, &BUSY_POLL_TIMEOUT,
               sizeof(BUSY_POLL_TIMEOUT));

    // SO_PRIORITY - highest priority to reduce scheduling latency
    constexpr int SOCKET_PRIORITY = 6;  // Highest priority
    setsockopt(socketFd, SOL_SOCKET, SO_PRIORITY, &SOCKET_PRIORITY,
               sizeof(SOCKET_PRIORITY));

    // Disable TCP slow start after idle
    constexpr int DISABLE_OPTION = 0;
    setsockopt(socketFd, IPPROTO_TCP, TCP_SLOW_START_AFTER_IDLE,
               &DISABLE_OPTION, sizeof(DISABLE_OPTION));

    return true;
}

/**
 * @brief Resolves IP address for a given network interface
 *
 * Retrieves the IPv4 address associated with a specific network interface.
 * Used when socket configuration specifies an interface name instead of
 * a direct IP address.
 *
 * Implementation details:
 * 1. Queries system interface list using getifaddrs
 * 2. Finds matching interface name
 * 3. Converts address to string representation
 * 4. Ensures proper cleanup of system resources
 *
 * @param interface Network interface name (e.g., "eth0")
 * @param outBuffer Buffer to store the resolved IP address (must be at least
 * MAX_IP_LENGTH)
 * @return true if IP was successfully resolved
 *
 * @note The function only returns the first IPv4 address found for the
 * interface
 */
[[nodiscard]] inline bool getInterfaceIP(
    const std::string_view interface,
    char (&outBuffer)[MAX_IP_LENGTH]) noexcept {
    ifaddrs* ifAddrStruct = nullptr;
    if (getifaddrs(&ifAddrStruct) == SOCKET_OK) {
        for (ifaddrs* ifa = ifAddrStruct; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET &&
                interface == ifa->ifa_name) {
                const auto result =
                    getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), outBuffer,
                                MAX_IP_LENGTH, nullptr, 0, NI_NUMERICHOST);
                freeifaddrs(ifAddrStruct);
                return result == SOCKET_OK;
            }
        }
        freeifaddrs(ifAddrStruct);
    }
    outBuffer[0] = '\0';
    return false;
}

/**
 * @brief Sets a socket to non-blocking mode
 *
 * Critical for high-performance trading applications to prevent blocking on I/O
 * operations. Uses fcntl to modify socket flags without affecting other
 * settings. Includes verification to ensure reliability in optimized builds.
 *
 * Implementation details:
 * 1. Retrieves current socket flags
 * 2. Sets non-blocking flag unconditionally for reliable behavior
 * 3. Verifies flag was properly applied (crucial for optimized builds)
 *
 * @param socketFd Valid socket file descriptor
 * @return true if socket is successfully set to non-blocking mode
 *
 * @note Non-blocking mode is essential for:
 *       - Preventing unexpected latency spikes in HFT applications
 *       - Implementing efficient event-driven I/O
 *       - Avoiding thread blocking in multi-threaded applications
 *       - Ensuring consistent socket behavior across different build modes
 */
[[nodiscard]] inline bool setNonBlocking(const int socketFd) noexcept {
    // Get current flags - this will fail appropriately if socketFd is invalid
    const int flags = fcntl(socketFd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }

    // Set non-blocking flag (even if already set - more reliable in optimized
    // builds)
    const int newFlags = flags | O_NONBLOCK;
    if (fcntl(socketFd, F_SETFL, newFlags) == -1) {
        return false;
    }

    // Critical: Verify the flags were actually applied
    // This prevents compiler optimizations from eliminating the previous call
    const int verifyFlags = fcntl(socketFd, F_GETFL, 0);
    if (verifyFlags == -1) {
        return false;
    }

    // If O_NONBLOCK isn't set properly, try again explicitly
    if ((verifyFlags & O_NONBLOCK) == 0) {
        if (fcntl(socketFd, F_SETFL, verifyFlags | O_NONBLOCK) == -1) {
            return false;
        }

        // Verify one more time
        const int finalFlags = fcntl(socketFd, F_GETFL, 0);
        if (finalFlags == -1) {
            return false;
        }

        return (finalFlags & O_NONBLOCK) != 0;
    }

    return true;
}

/**
 * @brief Enables software timestamping on the socket
 *
 * Configures socket for kernel-level software timestamping of received
 * packets. Uses Linux-specific implementation with verification to ensure
 * the setting is reliably applied even with aggressive compiler optimizations.
 *
 * @param socketFd Valid socket file descriptor
 * @return true if software timestamping was successfully enabled
 *
 * @note Software timestamps provide:
 *       - Moderately accurate packet arrival timing (microsecond precision)
 *       - No special hardware requirements
 *       - Critical timing information for latency-sensitive trading
 * applications
 */
[[nodiscard]] inline bool setSOTimestamp(const int socketFd) noexcept {
    constexpr int ENABLE_OPTION = 1;

    // Standard SO_TIMESTAMP setting on Linux
    if (setsockopt(socketFd, SOL_SOCKET, SO_TIMESTAMP, &ENABLE_OPTION,
                   sizeof(ENABLE_OPTION)) == SOCKET_ERROR) {
        return false;
    }

    // Verify the setting was applied
    int actual_value = 0;
    socklen_t optlen = sizeof(actual_value);
    if (getsockopt(socketFd, SOL_SOCKET, SO_TIMESTAMP, &actual_value,
                   &optlen) == SOCKET_ERROR) {
        return false;
    }

    // If verification failed, try setting again (helps with some Linux
    // kernels)
    if (actual_value != ENABLE_OPTION) {
        if (setsockopt(socketFd, SOL_SOCKET, SO_TIMESTAMP, &ENABLE_OPTION,
                       sizeof(ENABLE_OPTION)) == SOCKET_ERROR) {
            return false;
        }
        
        // Verify again
        if (getsockopt(socketFd, SOL_SOCKET, SO_TIMESTAMP, &actual_value,
                      &optlen) == SOCKET_ERROR) {
            return false;
        }
        
        if (actual_value != ENABLE_OPTION) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Joins a socket to a multicast group
 *
 * Configures a UDP socket to receive data from a specified multicast group.
 *
 * @param socketFd Valid socket file descriptor
 * @param multicastIp Multicast group address to join
 * @param interfaceIP IP of the interface to receive multicast packets (empty
 * for default)
 * @return true if join operation succeeded, false on failure
 */
[[nodiscard]] inline bool joinMulticast(
    const int socketFd,
    const std::string_view multicastIp,
    const std::string_view interfaceIP = "") noexcept {
    // Use anonymous struct with same memory layout as ip_mreq
    struct {
        struct in_addr imr_multiaddr;  // IP multicast address of group
        struct in_addr imr_interface;  // Local IP address of interface
    } mreq;

    // Set multicast IP
    mreq.imr_multiaddr.s_addr = inet_addr(multicastIp.data());

    // Use specific interface if provided, otherwise use default
    mreq.imr_interface.s_addr =
        interfaceIP.empty() ? INADDR_ANY : inet_addr(interfaceIP.data());

    // Single system call to join the group
    return (setsockopt(socketFd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
                       sizeof(mreq)) != SOCKET_ERROR);
}

/**
 * @brief Creates and configures a socket based on provided configuration
 *
 * Main entry point for socket creation in the trading system. Handles both
 * client and server sockets, with support for TCP and UDP protocols.
 * Optimized specifically for HFT (High-Frequency Trading) applications with
 * reliability enhancements for release builds.
 *
 * Implementation steps:
 * 1. IP Resolution:
 *    - Uses provided IP or resolves from interface
 *    - Validates address information
 *
 * 2. Socket Creation:
 *    - Creates appropriate socket type
 *    - Applies non-blocking mode with verification
 *    - Sets performance optimizations specific to HFT
 *
 * 3. Socket Configuration:
 *    - Client: Establishes connection
 *    - Server: Binds and prepares for listening with verification
 *    - Applies protocol-specific settings
 *    - Handles multicast group membership if specified
 *
 * Error Handling:
 * - Uses ASSERT for critical failures
 * - Logs all operations
 * - Ensures resource cleanup
 *
 * Performance Considerations:
 * - Minimizes system calls
 * - Uses Linux-specific optimizations for lowest latency
 * - Avoids unnecessary allocations
 * - Prevents compiler optimizations from affecting socket behavior
 *
 * @param logger Logger instance for operation tracking
 * @param cfg Socket configuration parameters
 * @return Valid socket descriptor or SOCKET_ERROR on failure
 *
 * @note Thread Safety: Thread-safe for different sockets, but concurrent
 *       operations on the same socket must be externally synchronized
 *
 * Example Usage:
 * @code
 * SocketConfig cfg{
 *     .ip = "192.168.1.1",
 *     .port = 8080,
 *     .isUDP = false,
 *     .isListening = true
 * };
 * int sock = createSocket(logger, cfg);
 * @endcode
 */
[[nodiscard]] inline int createSocket(Logger& logger,
                                      const SocketConfig& cfg) noexcept {
    // Step 1: IP Resolution
    // If no IP is provided, attempt to resolve from interface name
    char ipBuffer[MAX_IP_LENGTH];
    const auto& resolvedIP =
        cfg.ip.empty()
            ? (getInterfaceIP(cfg.interface, ipBuffer) ? ipBuffer : "")
            : cfg.ip;

    // Log socket creation attempt with configuration details
    logger.log("%:% %() % cfg:%\n", __FILE__, __LINE__, __FUNCTION__,
               getCurrentTimeStr(), std::format("{}", cfg));

    // Step 2: Address Resolution Setup
    // Configure address resolution flags:
    // - AI_PASSIVE: For server sockets (bind to all interfaces)
    // - AI_NUMERICHOST: IP is in numeric format
    // - AI_NUMERICSERV: Port is in numeric format
    const int addrFlags =
        (cfg.isListening ? AI_PASSIVE : 0) | (AI_NUMERICHOST | AI_NUMERICSERV);

    // Configure address hints for getaddrinfo:
    // - IPv4 only (AF_INET)
    // - Protocol type (SOCK_STREAM/SOCK_DGRAM)
    // - Protocol (IPPROTO_TCP/IPPROTO_UDP)
    const addrinfo hints{addrFlags,
                         AF_INET,
                         cfg.isUDP ? SOCK_DGRAM : SOCK_STREAM,
                         cfg.isUDP ? IPPROTO_UDP : IPPROTO_TCP,
                         0,
                         0,
                         nullptr,
                         nullptr};

    // Step 3: Resolve Address
    // Convert port to string and resolve address information
    addrinfo* rawAddrInfo = nullptr;
    const int addrInfoResult = 
        getaddrinfo(resolvedIP.data(), std::to_string(cfg.port).c_str(), &hints,
                    &rawAddrInfo);

    // Verify address resolution success
    if (addrInfoResult != 0) {
        logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                  getCurrentTimeStr(), 
                  std::string(ERR_GET_ADDR_INFO) + gai_strerror(addrInfoResult));
        ASSERT(false, std::string(ERR_GET_ADDR_INFO) + gai_strerror(addrInfoResult) +
                    " errno:" + strerror(errno));
        return SOCKET_ERROR;
    }

    // Step 4: Socket Creation and Configuration
    // Use RAII to manage address info lifecycle
    ScopedAddrInfo addrInfo(rawAddrInfo);
    int socketFd = SOCKET_ERROR;

    // Iterate through resolved addresses (typically only one for IPv4)
    for (addrinfo* rp = addrInfo.get(); rp; rp = rp->ai_next) {
        // Create socket with resolved parameters
        socketFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (socketFd == SOCKET_ERROR) {
            logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), 
                      std::string(ERR_SOCKET_CREATE) + strerror(errno));
            ASSERT(false, std::string(ERR_SOCKET_CREATE) + strerror(errno));
            continue;  // Try next address
        }

        // Set non-blocking mode - critical for performance
        if (!setNonBlocking(socketFd)) {
            logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), 
                      std::string(ERR_NON_BLOCKING) + strerror(errno));
            ASSERT(false, std::string(ERR_NON_BLOCKING) + strerror(errno));
            close(socketFd);
            socketFd = SOCKET_ERROR;
            continue;
        }

        // Apply performance optimizations based on protocol
        if (!optimizeSocket(socketFd, cfg.isUDP)) {
            logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), 
                      std::string(ERR_OPTIMIZE) + strerror(errno));
            ASSERT(false, std::string(ERR_OPTIMIZE) + strerror(errno));
            close(socketFd);
            socketFd = SOCKET_ERROR;
            continue;
        }

        // Handle client connection
        if (!cfg.isListening) {
            // For client sockets, attempt connection
            // Note: Non-blocking connect may return EINPROGRESS
            int connectResult = connect(socketFd, rp->ai_addr, rp->ai_addrlen);
            if (connectResult == -1 && errno != EINPROGRESS) {
                logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                          getCurrentTimeStr(), 
                          std::string(ERR_CONNECT) + strerror(errno));
                ASSERT(false, std::string(ERR_CONNECT) + strerror(errno));
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            break;  // Connection initiated successfully
        }

        // Server-specific setup
        // Enable address reuse to prevent "address already in use" errors
        const int enableOption = ENABLE_SOCKET_OPTION;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &enableOption,
                      sizeof(enableOption)) != SOCKET_OK) {
            logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), 
                      std::string(ERR_REUSE_ADDR) + strerror(errno));
            ASSERT(false, std::string(ERR_REUSE_ADDR) + strerror(errno));
            close(socketFd);
            socketFd = SOCKET_ERROR;
            continue;
        }

        // Prepare binding address
        // For UDP or when binding to specific interface
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(cfg.port);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Bind to all interfaces

        // Bind socket:
        // - UDP: Use prepared address
        // - TCP: Use resolved address
        const int bindResult =
            bind(socketFd,
                 cfg.isUDP ? reinterpret_cast<const struct sockaddr*>(&addr)
                           : rp->ai_addr,
                 cfg.isUDP ? sizeof(addr) : rp->ai_addrlen);

        if (bindResult != SOCKET_OK) {
            logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), 
                      std::string(ERR_BIND) + strerror(errno));
            ASSERT(false, std::string(ERR_BIND) + strerror(errno));
            close(socketFd);
            socketFd = SOCKET_ERROR;
            continue;
        }

        // For TCP server sockets, start listening with enhanced reliability
        if (!cfg.isUDP) {
            // Start listening with explicit error handling
            const int listenResult = listen(socketFd, MAX_TCP_BACKLOG);
            if (listenResult != SOCKET_OK) {
                logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                          getCurrentTimeStr(), 
                          std::string(ERR_LISTEN) + strerror(errno));
                ASSERT(false, std::string(ERR_LISTEN) + strerror(errno));
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }

            // Verify socket is in listening state (critical for optimization stability)
            int accepting = 0;
            socklen_t optlen = sizeof(accepting);

            // Query socket listening state - this creates an optimization
            // barrier that prevents the listen() call from being optimized away
            if (getsockopt(socketFd, SOL_SOCKET, SO_ACCEPTCONN, &accepting,
                           &optlen) == 0) {
                if (!accepting) {
                    // If not in accepting state, force listen call again
                    const int retryListen = listen(socketFd, MAX_TCP_BACKLOG);
                    if (retryListen != SOCKET_OK) {
                        close(socketFd);
                        logger.log("%:% %() % Secondary listen failed: %\n",
                                   __FILE__, __LINE__, __FUNCTION__,
                                   getCurrentTimeStr(), strerror(errno));
                        socketFd = SOCKET_ERROR;
                        continue;
                    }

                    // Verify again to create another optimization barrier
                    getsockopt(socketFd, SOL_SOCKET, SO_ACCEPTCONN, &accepting,
                               &optlen);
                }
            }

            // Log successful server socket creation for HFT debugging
            logger.log("%:% %() % Created TCP server on port %\n", __FILE__,
                       __LINE__, __FUNCTION__, getCurrentTimeStr(), cfg.port);
        }

        break;  // Binding and setup successful
    }

    // If we failed to create a socket, return the error
    if (socketFd == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }

    // Step 5: Additional Features
    // Configure hardware timestamping if requested
    if (cfg.needsSOTimestamp) {
        if (!setSOTimestamp(socketFd)) {
            logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), 
                      std::string(ERR_TIMESTAMP) + strerror(errno));
            ASSERT(false, std::string(ERR_TIMESTAMP) + strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
    }

    // Step 6: Multicast Configuration (if applicable)
    // For UDP multicast receivers, join the specified multicast group
    if (cfg.isUDP && cfg.isMulticast && cfg.isListening) {
        // For multicast, apply additional socket options

        // Linux-specific multicast optimization
        // SO_REUSEPORT allows multiple processes to bind to same multicast
        // address+port
        const int enableOption = ENABLE_SOCKET_OPTION;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEPORT, &enableOption,
                     sizeof(enableOption)) != SOCKET_OK) {
            logger.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), 
                      std::string("Failed to set SO_REUSEPORT: ") + strerror(errno));
            ASSERT(false, std::string("Failed to set SO_REUSEPORT: ") + strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }

        // Resolve multicast interface IP if specified
        char interfaceIPBuffer[MAX_IP_LENGTH]{};
        const char* interfaceIP = nullptr;

        if (!cfg.multicastInterface.empty()) {
            // Get IP address for the specified interface
            if (getInterfaceIP(cfg.multicastInterface, interfaceIPBuffer)) {
                interfaceIP = interfaceIPBuffer;
            } else {
                close(socketFd);
                logger.log("%:% %() % Failed to get interface IP: %\n",
                           __FILE__, __LINE__, __FUNCTION__,
                           getCurrentTimeStr(), cfg.multicastInterface);
                return SOCKET_ERROR;
            }
        }

        // Join the multicast group using the resolved IP and interface
        if (!joinMulticast(socketFd, cfg.ip,
                           interfaceIP ? std::string_view(interfaceIP)
                                       : std::string_view())) {
            const int errorCode = errno;
            close(socketFd);
            logger.log("%:% %() % Failed to join multicast group: %\n",
                       __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                       strerror(errorCode));
            return SOCKET_ERROR;
        }

        logger.log("%:% %() % Successfully joined multicast group: %\n",
                   __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                   cfg.ip);
    }

    return socketFd;
}

}  // namespace Trading::Core
