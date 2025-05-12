/**
 * @file socket_utils.hpp
 * @brief Core socket utilities for high-performance trading applications
 *
 * This header provides foundational socket creation and configuration utilities
 * for low-latency trading applications. It contains protocol-agnostic functions
 * that serve as the basis for both TCP and UDP implementations.
 *
 * Key features:
 * - Non-blocking socket operations
 * - Linux-specific performance optimizations
 * - Address resolution utilities
 * - Network interface handling
 * - Socket resource management
 *
 * Performance design principles:
 * - Minimal system calls
 * - Verification of critical operations
 * - Optimized error handling for latency-sensitive paths
 * - Consistent resource cleanup
 */

#pragma once

// Common POSIX headers
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <format>
#include <string>
#include <string_view>

#include "time_utils.hpp"
#include "trading/asserts.hpp"
#include "trading/logger.hpp"

namespace Trading::Core {

/**
 * @brief Common socket constants and error codes
 */
constexpr int SOCKET_ERROR = -1;         ///< Standard socket error return value
constexpr int SOCKET_OK = 0;             ///< Successful operation return value
constexpr int ENABLE_SOCKET_OPTION = 1;  ///< Value to enable socket options
constexpr size_t MAX_IP_LENGTH =
    16;  ///< Maximum IPv4 address length (xxx.xxx.xxx.xxx\0)

/**
 * @brief Socket config flags
 */
constexpr int SOCKET_RECV_FLAGS = MSG_DONTWAIT;  // Non-blocking receive on Linux
constexpr int MAX_TCP_BACKLOG = SOMAXCONN;       // Use kernel maximum

/**
 * @brief Pre-defined error messages for socket operations
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
constexpr std::string_view ERR_MULTICAST = "Multicast operation failed. errno:";

/**
 * @brief Connection purpose for socket optimization
 *
 * Defines the primary purpose of a socket to apply appropriate optimizations
 */
enum class SocketPurpose {
    MARKET_DATA = 0,        ///< Market data feeds (medium priority)
    EXECUTION = 1,          ///< Order execution (highest priority)
    ADMINISTRATIVE = 2      ///< Reporting, position updates (lowest priority)
};

/**
 * @brief Polling intensity for busy-poll optimization
 *
 * Controls the trade-off between CPU usage and latency
 */
enum class PollingMode {
    NONE = 0,               ///< No busy polling (lowest CPU usage)
    BALANCED = 1,           ///< Moderate polling (50μs timeout)
    AGGRESSIVE = 2,         ///< Aggressive polling (100μs timeout)
    ULTRA_AGGRESSIVE = 3    ///< Maximum polling (200μs timeout, highest CPU usage)
};

/**
 * @brief RAII wrapper for addrinfo structures
 *
 * Manages the lifecycle of addrinfo structures returned by getaddrinfo().
 * Automatically frees resources when going out of scope, preventing memory leaks.
 * Provides validity checking via explicit boolean conversion.
 */
class ScopedAddrInfo {
    addrinfo* _info = nullptr;

public:
    /**
     * @brief Constructs a wrapper around an addrinfo pointer
     * @param info Raw addrinfo pointer to take ownership of (nullptr creates an invalid object)
     */
    explicit ScopedAddrInfo(addrinfo* info = nullptr) noexcept : _info(info) {}

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
     * @return Pointer to the managed addrinfo structure (may be nullptr)
     */
    [[nodiscard]] addrinfo* get() const noexcept { return _info; }
    
    /**
     * @brief Check if this object contains a valid addrinfo structure
     * @return true if the object holds a non-null addrinfo pointer
     */
    [[nodiscard]] explicit operator bool() const noexcept { return _info != nullptr; }

    // Prevent copying to ensure single ownership (addrinfo must be freed exactly once)
    ScopedAddrInfo(const ScopedAddrInfo&) = delete;
    ScopedAddrInfo& operator=(const ScopedAddrInfo&) = delete;
    
    /**
     * @brief Move constructor that transfers ownership
     * @param other ScopedAddrInfo to take ownership from (will be invalidated)
     */
    ScopedAddrInfo(ScopedAddrInfo&& other) noexcept : _info(other._info) {
        other._info = nullptr;
    }
    
    /**
     * @brief Move assignment operator that transfers ownership
     * @param other ScopedAddrInfo to take ownership from (will be invalidated)
     * @return Reference to this object
     */
    ScopedAddrInfo& operator=(ScopedAddrInfo&& other) noexcept {
        if (this != &other) {
            if (_info) {
                freeaddrinfo(_info);
            }
            _info = other._info;
            other._info = nullptr;
        }
        return *this;
    }
};

/**
 * @brief Base socket class providing RAII socket management
 *
 * This class manages a socket file descriptor with RAII semantics,
 * ensuring the socket is properly closed when the object is destroyed.
 * Provides move semantics for efficient ownership transfer, which is
 * essential for connection handling in HFT applications.
 */
class Socket {
private:
    int _fd{SOCKET_ERROR};
    
    /**
     * @brief Safely close a file descriptor with EINTR retry
     * @param fd File descriptor to close
     */
    static void closeDescriptor(int& fd) noexcept {
        if (fd != SOCKET_ERROR) {
            int result;
            do {
                result = close(fd);
            } while (result == -1 && errno == EINTR);  // Retry if interrupted
            
            fd = SOCKET_ERROR;
        }
    }

public:
    /**
     * @brief Creates an empty socket object or wraps an existing descriptor
     * @param fd Existing socket file descriptor (or SOCKET_ERROR for empty)
     */
    explicit Socket(int fd = SOCKET_ERROR) noexcept : _fd(fd) {}
    
    /**
     * @brief Destructor automatically closes the socket if owned
     * 
     * Uses interrupt-safe close operation to ensure proper resource cleanup
     * even if interrupted by signals, which is essential for long-running
     * HFT applications.
     */
    ~Socket() noexcept {
        closeDescriptor(_fd);
    }
    
    /**
     * @brief Get the socket file descriptor
     * @return The socket file descriptor
     */
    [[nodiscard]] int getFd() const noexcept { return _fd; }
    
    /**
     * @brief Check if socket is valid
     * @return true if socket holds a valid file descriptor
     */
    [[nodiscard]] bool isValid() const noexcept { return _fd != SOCKET_ERROR; }
    
    /**
     * @brief Take ownership of fd (closing any previously owned fd)
     * @param fd New socket file descriptor to own
     * 
     * Safely closes any existing descriptor before taking ownership of the new one.
     */
    void reset(int fd) noexcept {
        closeDescriptor(_fd);
        _fd = fd;
    }
    
    /**
     * @brief Release ownership without closing
     * @return The socket file descriptor (caller takes ownership)
     * 
     * After calling this method, the Socket object becomes invalid and
     * the caller is responsible for closing the returned descriptor.
     */
    [[nodiscard]] int release() noexcept {
        int tmp = _fd;
        _fd = SOCKET_ERROR;
        return tmp;
    }
    
    // Prevent copying to ensure single ownership of socket resources
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    
    /**
     * @brief Move constructor that transfers socket ownership
     * @param other Socket to take ownership from (will be invalidated)
     */
    Socket(Socket&& other) noexcept : _fd(other._fd) {
        other._fd = SOCKET_ERROR;
    }
    
    /**
     * @brief Move assignment operator that transfers socket ownership
     * @param other Socket to take ownership from (will be invalidated)
     * @return Reference to this object
     * 
     * Safely closes any existing descriptor before taking ownership of the other socket.
     */
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            reset(other._fd);
            other._fd = SOCKET_ERROR;
        }
        return *this;
    }
};

/**
 * @brief Resolves IP address for a given network interface
 *
 * Retrieves the IPv4 address associated with a specific network interface.
 *
 * @param interface Network interface name (e.g., "eth0")
 * @param outBuffer Buffer to store the resolved IP address
 * @return true if IP was successfully resolved
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
 * Critical for high-performance trading applications to prevent blocking on I/O operations.
 * Optimized implementation uses a single system call for minimal latency.
 *
 * @param socketFd Valid socket file descriptor
 * @return true if socket is successfully set to non-blocking mode
 */
[[nodiscard]] inline bool setNonBlocking(const int socketFd) noexcept {
    // Single system call implementation for maximum performance
    return fcntl(socketFd, F_SETFL, O_NONBLOCK) != -1;
}

/**
 * @brief Enables software timestamping on the socket
 *
 * Configures socket for kernel-level software timestamping of received packets.
 * Uses minimal system calls for optimal performance in latency-sensitive paths.
 *
 * @param socketFd Valid socket file descriptor
 * @return true if software timestamping was successfully enabled
 */
[[nodiscard]] inline bool setSOTimestamp(const int socketFd) noexcept {
    constexpr int ENABLE_OPTION = 1;
    
    // Direct system call implementation for minimal latency
    return setsockopt(socketFd, SOL_SOCKET, SO_TIMESTAMP, &ENABLE_OPTION,
                    sizeof(ENABLE_OPTION)) != SOCKET_ERROR;
}

/**
 * @brief Resolve address information for socket operations
 * 
 * Optimized helper function to resolve address information using getaddrinfo.
 * Takes hints by pointer to avoid unnecessary copying and align with getaddrinfo API.
 * 
 * @param ip IP address string (null-terminated)
 * @param port Port number as string (null-terminated)
 * @param hints Pointer to address hints structure (matches getaddrinfo API)
 * @param logger Logger for error reporting
 * @return ScopedAddrInfo with resolved address or nullptr on failure
 */
[[nodiscard]] inline ScopedAddrInfo resolveAddressInfo(
    const char* ip, 
    const char* port,
    const addrinfo* hints, 
    Logger& logger) noexcept {
    
    addrinfo* rawAddrInfo = nullptr;
    const int addrInfoResult = getaddrinfo(ip, port, hints, &rawAddrInfo);

    if (addrInfoResult != 0) {
        logger.log("%:% %() % % %\n", __FILE__, __LINE__, __FUNCTION__,
                  getCurrentTimeStr(), ERR_GET_ADDR_INFO, 
                  gai_strerror(addrInfoResult));
        return ScopedAddrInfo(nullptr);
    }
    
    return ScopedAddrInfo(rawAddrInfo);
}

}  // namespace Trading::Core
