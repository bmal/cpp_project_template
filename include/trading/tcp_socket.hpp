/**
 * @file tcp_socket.hpp
 * @brief High-performance TCP socket implementation for trading applications
 *
 * This header provides optimized TCP socket creation and configuration utilities
 * specifically designed for ultra-low latency trading applications. It builds on
 * the core socket utilities and applies TCP-specific optimizations.
 *
 * Key features:
 * - Non-blocking TCP socket operations
 * - Linux-specific TCP performance optimizations
 * - TCP client and server socket support
 * - Optimized for minimal latency in HFT environments
 *
 * Performance optimization details:
 * - TCP_NODELAY: Disables Nagle's algorithm for lower latency
 * - TCP_QUICKACK: Immediate ACK for reduced latency
 * - TCP_FASTOPEN: Reduces connection setup latency
 * - TCP_SLOW_START_AFTER_IDLE: Disabled to maintain consistent performance
 * - SO_LINGER: Zero timeout for quick socket closure
 * - SO_BUSY_POLL: Reduces latency by busy-polling
 * - SO_PRIORITY: Sets socket priority for better scheduling
 */

#pragma once

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <asm/socket.h>
#include <linux/sockios.h>
#include <sys/epoll.h>

#include "trading/socket_utils.hpp"

#ifndef TCP_SLOW_START_AFTER_IDLE
#define TCP_SLOW_START_AFTER_IDLE 31
#endif

namespace Trading::Core {

/**
 * @brief TCP Socket Configuration Options
 *
 * Configuration parameters for TCP socket optimization
 */
struct TcpSocketOptions {
    bool disableNagle{true};                ///< Disable Nagle's algorithm (TCP_NODELAY)
    bool enableQuickAck{true};              ///< Enable immediate ACKs
    bool enableFastOpen{true};              ///< Enable TCP Fast Open (reduced connection latency)
    bool enableKeepalive{false};            ///< Enable TCP keepalive (default: disabled for HFT)
    int rcvBufferSize{4 * 1024 * 1024};     ///< Socket receive buffer size (4MB default for market data)
    int sndBufferSize{1 * 1024 * 1024};     ///< Socket send buffer size (1MB default for orders)
    int lingerTimeout{0};                   ///< SO_LINGER timeout (0 for immediate close)
    int keepaliveTime{5};                   ///< Seconds before sending keepalive (if enabled)
    int keepaliveInterval{1};               ///< Seconds between keepalive probes (if enabled)
    int keepaliveProbeCount{3};             ///< Failed probes before declaring connection dead
    SocketPurpose purpose{SocketPurpose::MARKET_DATA}; ///< Socket purpose for priority optimization
    PollingMode pollingMode{PollingMode::BALANCED};    ///< Busy-polling configuration

    /**
     * @brief Get appropriate busy poll timeout based on polling mode
     * @return Timeout in microseconds
     */
    int getBusyPollTimeout() const noexcept {
        switch (pollingMode) {
            case PollingMode::NONE: return 0;
            case PollingMode::BALANCED: return 50;
            case PollingMode::AGGRESSIVE: return 100;
            case PollingMode::ULTRA_AGGRESSIVE: return 200;
            default: return 50;
        }
    }
    
    /**
     * @brief Get appropriate socket priority based on purpose
     * @return Socket priority value
     */
    int getSocketPriority() const noexcept {
        switch (purpose) {
            case SocketPurpose::EXECUTION: return 7;     // Highest priority
            case SocketPurpose::MARKET_DATA: return 6;   // Medium priority
            case SocketPurpose::ADMINISTRATIVE: return 4; // Lowest priority
            default: return 6;
        }
    }
    
    /**
     * @brief Create options optimized for market data connections
     * @return Pre-configured TcpSocketOptions
     */
    static TcpSocketOptions forMarketData() noexcept {
        TcpSocketOptions opts;
        opts.purpose = SocketPurpose::MARKET_DATA;
        opts.rcvBufferSize = 4 * 1024 * 1024;  // 4MB for market data
        opts.sndBufferSize = 256 * 1024;       // 256KB for control messages
        opts.pollingMode = PollingMode::BALANCED;
        return opts;
    }
    
    /**
     * @brief Create options optimized for order execution connections
     * @return Pre-configured TcpSocketOptions
     */
    static TcpSocketOptions forExecution() noexcept {
        TcpSocketOptions opts;
        opts.purpose = SocketPurpose::EXECUTION;
        opts.rcvBufferSize = 1 * 1024 * 1024;  // 1MB for execution responses
        opts.sndBufferSize = 1 * 1024 * 1024;  // 1MB for orders
        opts.pollingMode = PollingMode::AGGRESSIVE;
        return opts;
    }
    
    /**
     * @brief Create options optimized for administrative connections
     * @return Pre-configured TcpSocketOptions
     */
    static TcpSocketOptions forAdministrative() noexcept {
        TcpSocketOptions opts;
        opts.purpose = SocketPurpose::ADMINISTRATIVE;
        opts.rcvBufferSize = 1 * 1024 * 1024;   // 1MB is sufficient
        opts.sndBufferSize = 1 * 1024 * 1024;   // 1MB is sufficient
        opts.pollingMode = PollingMode::NONE;   // No busy polling needed
        opts.enableKeepalive = true;            // Enable keepalive for reliability
        return opts;
    }
};

/**
 * @brief High-performance TCP socket class
 *
 * Zero-overhead wrapper around TCP socket file descriptor with optimizations
 * specifically designed for high-frequency trading applications.
 */
class TcpSocket : public Socket {
public:
    /**
     * @brief Create an empty TCP socket object
     */
    TcpSocket() noexcept : Socket() {}

    /**
     * @brief Create a TCP client socket connected to a remote endpoint
     *
     * Thread-safety: This method is thread-safe, but each socket must be used
     * by only one thread at a time once created, unless external synchronization is provided.
     *
     * @param logger Logger for operation tracking
     * @param ip IP address to connect to
     * @param port TCP port to connect to
     * @param needsTimestamp Enable hardware timestamping
     * @param options TCP socket optimization options
     * @return Initialized TCP socket or empty socket on failure
     */
    [[nodiscard]] static TcpSocket createClient(
        Logger& logger, 
        const std::string_view ip, 
        int port, 
        bool needsTimestamp = false,
        const TcpSocketOptions& options = {}) noexcept {
        
        return TcpSocket(createClientSocket(logger, ip, port, needsTimestamp, options));
    }
    
    /**
     * @brief Create a TCP server socket listening for connections
     *
     * Thread-safety: This method is thread-safe, but each socket must be used
     * by only one thread at a time once created, unless external synchronization is provided.
     *
     * @param logger Logger for operation tracking
     * @param ip IP address to bind to (empty for all interfaces)
     * @param port TCP port to listen on
     * @param needsTimestamp Enable hardware timestamping
     * @param options TCP socket optimization options
     * @return Initialized TCP socket or empty socket on failure
     */
    [[nodiscard]] static TcpSocket createServer(
        Logger& logger, 
        const std::string_view ip, 
        int port, 
        bool needsTimestamp = false,
        const TcpSocketOptions& options = {}) noexcept {
        
        return TcpSocket(createServerSocket(logger, ip, port, needsTimestamp, options));
    }
    
    /**
     * @brief Accept a new client connection
     *
     * For server sockets, accepts a new incoming client connection.
     * Only valid for listening sockets.
     *
     * Thread-safety: Not thread-safe. Only one thread should call accept()
     * on a particular socket at a time.
     *
     * @param logger Logger for operation tracking
     * @param clientAddress Optional pointer to store client address
     * @return New TcpSocket representing the client connection
     */
    [[nodiscard]] TcpSocket accept(
        Logger& logger,
        sockaddr_in* clientAddress = nullptr) const noexcept {
        
        sockaddr_in addr{};
        socklen_t addrLen = sizeof(addr);
        
        // Use accept4() with SOCK_NONBLOCK to save a syscall
        const int clientFd = ::accept4(getFd(), 
                                 clientAddress ? 
                                   reinterpret_cast<sockaddr*>(clientAddress) : 
                                   reinterpret_cast<sockaddr*>(&addr), 
                                 &addrLen,
                                 SOCK_NONBLOCK);
        
        if (clientFd == SOCKET_ERROR) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                logger.log("%:% %() % accept() failed: %\n", 
                           __FILE__, __LINE__, __FUNCTION__,
                           getCurrentTimeStr(), strerror(errno));
            }
            return TcpSocket();
        }
        
        // Only apply TCP_NODELAY - the single most critical option for HFT
        const int enable = 1;
        if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) == SOCKET_ERROR) {
            close(clientFd);
            return TcpSocket();
        }
        
        return TcpSocket(clientFd);
    }
    
    /**
     * @brief Reset TCP_QUICKACK for critical response paths
     * 
     * TCP_QUICKACK is automatically cleared after receiving a packet.
     * Call this after receiving data when immediate ACK is critical.
     * Note: Involves a syscall, so use strategically only when needed.
     */
    void resetQuickAck() const noexcept {
        if (isValid()) {
            const int enable = 1;
            setsockopt(getFd(), IPPROTO_TCP, TCP_QUICKACK, &enable, sizeof(enable));
        }
    }
    
    /**
     * @brief Add socket to an epoll instance
     * 
     * Configures this socket for monitoring with epoll, which is more efficient
     * than select/poll for HFT applications.
     * 
     * @param epollFd File descriptor of the epoll instance
     * @param events Event types to monitor (EPOLLIN, EPOLLOUT, etc.)
     * @param userData Optional user data to associate with this socket
     * @return 0 on success, -1 on failure
     */
    int addToEpoll(int epollFd, uint32_t events, void* userData = nullptr) const noexcept {
        if (!isValid()) return -1;
        
        epoll_event event{};
        event.events = events;
        event.data.ptr = userData ? userData : const_cast<TcpSocket*>(this);
        
        return epoll_ctl(epollFd, EPOLL_CTL_ADD, getFd(), &event);
    }
    
    /**
     * @brief Modify this socket's epoll registration
     * 
     * Updates the events or user data for an existing epoll registration.
     * 
     * @param epollFd File descriptor of the epoll instance
     * @param events New event types to monitor
     * @param userData Optional new user data to associate with this socket
     * @return 0 on success, -1 on failure
     */
    int modifyEpoll(int epollFd, uint32_t events, void* userData = nullptr) const noexcept {
        if (!isValid()) return -1;
        
        epoll_event event{};
        event.events = events;
        event.data.ptr = userData ? userData : const_cast<TcpSocket*>(this);
        
        return epoll_ctl(epollFd, EPOLL_CTL_MOD, getFd(), &event);
    }
    
    /**
     * @brief Remove socket from an epoll instance
     * 
     * @param epollFd File descriptor of the epoll instance
     * @return 0 on success, -1 on failure
     */
    int removeFromEpoll(int epollFd) const noexcept {
        if (!isValid()) return -1;
        
        return epoll_ctl(epollFd, EPOLL_CTL_DEL, getFd(), nullptr);
    }

private:
    /**
     * @brief Construct from an existing socket file descriptor
     * @param fd Socket file descriptor to take ownership of
     */
    explicit TcpSocket(int fd) noexcept : Socket(fd) {}
    
    /**
    * @brief Apply TCP-specific performance optimizations to a socket with error reporting
    *
    * Configures socket options for optimal TCP performance in an HFT environment.
    * This function applies TCP-specific optimizations with targeted error checking
    * that balances operational visibility with performance.
    *
    * Error handling approach:
    * - Critical options (buffer sizes, TCP_NODELAY): Return false on failure
    * - Non-critical options: Log warnings but continue execution
    * - All error conditions are logged for operational visibility
    *
    * @param socketFd Valid socket file descriptor
    * @param options TCP socket optimization options
    * @param logger Logger for reporting option application errors
    * @return true if critical optimizations were successfully applied
    */
    [[nodiscard]] static bool optimizeTcpSocket(
        const int socketFd, 
        const TcpSocketOptions& options,
        Logger& logger) noexcept {
        
        constexpr int ENABLE_OPTION = 1;
        constexpr int DISABLE_OPTION = 0;

        if (socketFd == SOCKET_ERROR) {
            return false;
        }

        // Apply buffer sizes for high throughput - critical for preventing drops
        if (setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, &options.rcvBufferSize,
                    sizeof(options.rcvBufferSize)) == SOCKET_ERROR) {
            logger.log("Error: Failed to set SO_RCVBUF to %: %\n", 
                    options.rcvBufferSize, strerror(errno));
            return false;
        }

        if (setsockopt(socketFd, SOL_SOCKET, SO_SNDBUF, &options.sndBufferSize,
                    sizeof(options.sndBufferSize)) == SOCKET_ERROR) {
            logger.log("Error: Failed to set SO_SNDBUF to %: %\n", 
                    options.sndBufferSize, strerror(errno));
            return false;
        }

        // TCP_NODELAY - Critical for HFT to disable Nagle's algorithm
        int nodelay = options.disableNagle ? ENABLE_OPTION : DISABLE_OPTION;
        if (setsockopt(socketFd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
                    sizeof(nodelay)) == SOCKET_ERROR) {
            logger.log("Error: Failed to set TCP_NODELAY to %: %\n", 
                    nodelay, strerror(errno));
            return false;
        }

        // TCP_QUICKACK - reduce latency by immediately acknowledging packets
        if (options.enableQuickAck) {
            if (setsockopt(socketFd, IPPROTO_TCP, TCP_QUICKACK, &ENABLE_OPTION,
                        sizeof(ENABLE_OPTION)) == SOCKET_ERROR) {
                // Log but continue - not critical enough to fail
                logger.log("Warning: Failed to set TCP_QUICKACK: %\n", strerror(errno));
            }
        }

        // TCP_FASTOPEN - reduces connection setup latency (if supported)
        if (options.enableFastOpen) {
            if (setsockopt(socketFd, IPPROTO_TCP, TCP_FASTOPEN, &ENABLE_OPTION,
                        sizeof(ENABLE_OPTION)) == SOCKET_ERROR) {
                // Log but continue - not all kernels support this
                logger.log("Warning: Failed to set TCP_FASTOPEN: %\n", strerror(errno));
            }
        }

        // SO_LINGER with zero timeout - crucial for clean shutdown without TIME_WAIT
        struct linger ling = {ENABLE_OPTION, options.lingerTimeout};
        if (setsockopt(socketFd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) == SOCKET_ERROR) {
            // Log but continue - not critical enough to fail
            logger.log("Warning: Failed to set SO_LINGER: %\n", strerror(errno));
        }

        // SO_BUSY_POLL - apply based on polling mode
        const int busyPollTimeout = options.getBusyPollTimeout();
        if (busyPollTimeout > 0) {
            if (setsockopt(socketFd, SOL_SOCKET, SO_BUSY_POLL, &busyPollTimeout,
                        sizeof(busyPollTimeout)) == SOCKET_ERROR) {
                // For HFT, busy polling is quite important
                logger.log("Warning: Failed to set SO_BUSY_POLL to %us: %\n", 
                        busyPollTimeout, strerror(errno));
            }
        }

        // SO_PRIORITY - set based on socket purpose
        const int socketPriority = options.getSocketPriority();
        if (setsockopt(socketFd, SOL_SOCKET, SO_PRIORITY, &socketPriority,
                    sizeof(socketPriority)) == SOCKET_ERROR) {
            // Priority is important but shouldn't fail socket creation
            logger.log("Warning: Failed to set SO_PRIORITY to %: %\n", 
                    socketPriority, strerror(errno));
        }

        // Disable TCP slow start after idle
        if (setsockopt(socketFd, IPPROTO_TCP, TCP_SLOW_START_AFTER_IDLE,
                    &DISABLE_OPTION, sizeof(DISABLE_OPTION)) == SOCKET_ERROR) {
            // Log but continue
            logger.log("Warning: Failed to disable TCP_SLOW_START_AFTER_IDLE: %\n", strerror(errno));
        }
        
        // Apply keepalive if enabled
        if (options.enableKeepalive) {
            if (setsockopt(socketFd, SOL_SOCKET, SO_KEEPALIVE, &ENABLE_OPTION, 
                    sizeof(ENABLE_OPTION)) == SOCKET_ERROR) {
                logger.log("Warning: Failed to set SO_KEEPALIVE: %\n", strerror(errno));
            } else {
                // Only try to set these if enabling keepalive succeeded
                setsockopt(socketFd, IPPROTO_TCP, TCP_KEEPIDLE, &options.keepaliveTime, 
                        sizeof(options.keepaliveTime));
                setsockopt(socketFd, IPPROTO_TCP, TCP_KEEPINTVL, &options.keepaliveInterval,
                        sizeof(options.keepaliveInterval));
                setsockopt(socketFd, IPPROTO_TCP, TCP_KEEPCNT, &options.keepaliveProbeCount,
                        sizeof(options.keepaliveProbeCount));
            }
        }

        return true;
    }
    
    /**
     * @brief Implementation for creating a TCP client socket
     *
     * @param logger Logger for operation tracking
     * @param ip IP address to connect to
     * @param port TCP port to connect to
     * @param needsTimestamp Enable hardware timestamping
     * @param options TCP socket optimization options
     * @return Socket file descriptor or SOCKET_ERROR on failure
     */
    [[nodiscard]] static int createClientSocket(
        Logger& logger,
        const std::string_view ip,
        int port,
        bool needsTimestamp,
        const TcpSocketOptions& options) noexcept {
        
        // Save initial errno to avoid interference from other threads
        const int savedErrno = errno;
        errno = 0;
        
        // Log socket creation attempt
        logger.log("%:% %() % Creating TCP client socket to %:%\n", 
                  __FILE__, __LINE__, __FUNCTION__,
                  getCurrentTimeStr(), ip, port);
        
        // Configure address resolution hints for TCP client
        const addrinfo hints{
            AI_NUMERICHOST,  // IP and port are numeric, but allow service name resolution
            AF_INET,         // IPv4 only
            SOCK_STREAM,     // TCP socket
            IPPROTO_TCP,     // TCP protocol
            0, 0, nullptr, nullptr
        };
        
        // Convert IP to null-terminated string if needed
        std::string resolvedIPStr = !ip.empty() ? std::string(ip) : "";
        
        // Convert port to string and resolve address
        std::string portStr = std::to_string(port);
        auto addrInfo = resolveAddressInfo(
            resolvedIPStr.empty() ? nullptr : resolvedIPStr.c_str(), 
            portStr.c_str(), 
            &hints,
            logger);
        
        if (!addrInfo.get()) {
            if (errno == 0) errno = savedErrno;
            return SOCKET_ERROR;
        }
        
        // Try to create and connect socket
        int socketFd = SOCKET_ERROR;
        int lastError = 0;
        
        for (addrinfo* rp = addrInfo.get(); rp; rp = rp->ai_next) {
            // Create socket with resolved parameters
            socketFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socketFd == SOCKET_ERROR) {
                lastError = errno;
                continue;  // Try next address
            }
            
            // Set non-blocking mode
            if (!setNonBlocking(socketFd)) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Apply TCP optimizations
            if (!optimizeTcpSocket(socketFd, options, logger)) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Apply timestamping if requested
            if (needsTimestamp && !setSOTimestamp(socketFd)) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Initiate non-blocking connection
            int connectResult = connect(socketFd, rp->ai_addr, rp->ai_addrlen);
            if (connectResult == -1 && errno != EINPROGRESS) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Connection initiated successfully
            logger.log("%:% %() % Successfully initiated TCP connection to %:%\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                      ip, port);
            break;
        }
        
        // Log error only once if all addresses failed
        if (socketFd == SOCKET_ERROR) {
            logger.log("%:% %() % Failed to create TCP client socket: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(lastError));
            errno = lastError;
        }
        
        return socketFd;
    }
    
    /**
     * @brief Implementation for creating a TCP server socket
     *
     * @param logger Logger for operation tracking
     * @param ip IP address to bind to (empty for all interfaces)
     * @param port TCP port to listen on
     * @param needsTimestamp Enable hardware timestamping
     * @param options TCP socket optimization options
     * @return Socket file descriptor or SOCKET_ERROR on failure
     */
    [[nodiscard]] static int createServerSocket(
        Logger& logger,
        const std::string_view ip,
        int port,
        bool needsTimestamp,
        const TcpSocketOptions& options) noexcept {
        
        // Save initial errno to avoid interference from other threads
        const int savedErrno = errno;
        errno = 0;
        
        // Log socket creation attempt
        logger.log("%:% %() % Creating TCP server socket on %:%\n", 
                  __FILE__, __LINE__, __FUNCTION__,
                  getCurrentTimeStr(), 
                  ip.empty() ? std::string_view("*") : ip, port);
        
        // Configure address resolution hints for TCP server
        const addrinfo hints{
            AI_PASSIVE | AI_NUMERICHOST,  // Server socket, numeric IP but allow service
            AF_INET,                      // IPv4 only
            SOCK_STREAM,                  // TCP socket
            IPPROTO_TCP,                  // TCP protocol
            0, 0, nullptr, nullptr
        };
        
        // Convert IP to null-terminated string if needed
        std::string resolvedIPStr = !ip.empty() ? std::string(ip) : "";
        
        // Convert port to string and resolve address
        std::string portStr = std::to_string(port);
        auto addrInfo = resolveAddressInfo(
            resolvedIPStr.empty() ? nullptr : resolvedIPStr.c_str(), 
            portStr.c_str(), 
            &hints,
            logger);
        
        if (!addrInfo.get()) {
            if (errno == 0) errno = savedErrno;
            return SOCKET_ERROR;
        }
        
        // Try to create and bind socket
        int socketFd = SOCKET_ERROR;
        int lastError = 0;
        
        for (addrinfo* rp = addrInfo.get(); rp; rp = rp->ai_next) {
            // Create socket with resolved parameters
            socketFd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socketFd == SOCKET_ERROR) {
                lastError = errno;
                continue;  // Try next address
            }
            
            // Set non-blocking mode
            if (!setNonBlocking(socketFd)) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Apply TCP optimizations
            if (!optimizeTcpSocket(socketFd, options, logger)) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Enable address reuse to prevent "address already in use" errors
            const int enableOption = ENABLE_SOCKET_OPTION;
            if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &enableOption,
                          sizeof(enableOption)) != SOCKET_OK) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Bind socket to address
            const int bindResult = bind(socketFd, rp->ai_addr, rp->ai_addrlen);
            if (bindResult != SOCKET_OK) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Apply timestamping if requested
            if (needsTimestamp && !setSOTimestamp(socketFd)) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            // Start listening for connections
            const int listenResult = listen(socketFd, MAX_TCP_BACKLOG);
            if (listenResult != SOCKET_OK) {
                lastError = errno;
                close(socketFd);
                socketFd = SOCKET_ERROR;
                continue;
            }
            
            logger.log("%:% %() % Created TCP server on port %\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(), port);
            break;
        }
        
        // Log error only once if all addresses failed
        if (socketFd == SOCKET_ERROR) {
            logger.log("%:% %() % Failed to create TCP server socket: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(lastError));
            errno = lastError;
        }
        
        return socketFd;
    }
};

}  // namespace Trading::Core
