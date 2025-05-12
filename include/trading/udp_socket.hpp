/**
 * @file udp_socket.hpp
 * @brief High-performance UDP socket implementation for trading applications
 *
 * This header provides optimized UDP socket creation and configuration utilities
 * specifically designed for ultra-low latency trading applications. It builds on
 * the core socket utilities and applies UDP-specific optimizations.
 *
 * Key features:
 * - Non-blocking UDP socket operations
 * - Linux-specific UDP performance optimizations
 * - Multicast support for market data feeds
 * - Optimized for minimal latency in HFT environments
 *
 * Performance optimization details:
 * - SO_TIMESTAMP: Hardware timestamping for accurate latency measurement
 * - SO_BUSY_POLL: Reduces latency by busy-polling
 * - SO_PRIORITY: Sets socket priority for better scheduling
 * - Buffer sizing: Optimized buffer sizes for different traffic patterns
 * - SO_SNDBUF/SO_RCVBUF: Tuned to prevent packet drops
 */

#pragma once

#include <netinet/udp.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <asm/socket.h>
#include <linux/sockios.h>
#include <sys/epoll.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "trading/socket_utils.hpp"

namespace Trading::Core {

/**
 * @brief UDP Socket Configuration Options
 *
 * Configuration parameters for UDP socket optimization
 */
struct UdpSocketOptions {
    bool enableBroadcast{false};          ///< Allow broadcast packets
    bool enableMulticast{false};          ///< Configure for multicast
    bool enableLoopback{false};           ///< Loopback multicast packets
    int rcvBufferSize{8 * 1024 * 1024};   ///< Socket receive buffer size (8MB default for market data)
    int sndBufferSize{1 * 1024 * 1024};   ///< Socket send buffer size (1MB default for orders)
    int ttl{1};                           ///< Time to live (for multicast: 1=local network)
    int multicastTtl{1};                  ///< Multicast TTL (when enableMulticast=true)
    int multicastInterface{0};            ///< Interface for multicast (0 for system default)
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
     * @brief Create options optimized for market data multicast
     * @return Pre-configured UdpSocketOptions
     */
    static UdpSocketOptions forMarketDataMulticast() noexcept {
        UdpSocketOptions opts;
        opts.purpose = SocketPurpose::MARKET_DATA;
        opts.rcvBufferSize = 8 * 1024 * 1024;   // 8MB for high-volume market data
        opts.sndBufferSize = 256 * 1024;        // 256KB for control messages
        opts.enableMulticast = true;
        opts.multicastTtl = 1;                  // Local network only
        opts.pollingMode = PollingMode::AGGRESSIVE;
        return opts;
    }
    
    /**
     * @brief Create options optimized for order execution
     * @return Pre-configured UdpSocketOptions
     */
    static UdpSocketOptions forExecution() noexcept {
        UdpSocketOptions opts;
        opts.purpose = SocketPurpose::EXECUTION;
        opts.rcvBufferSize = 2 * 1024 * 1024;   // 2MB for execution responses
        opts.sndBufferSize = 2 * 1024 * 1024;   // 2MB for orders
        opts.pollingMode = PollingMode::ULTRA_AGGRESSIVE;
        return opts;
    }
    
    /**
     * @brief Create options optimized for administrative traffic
     * @return Pre-configured UdpSocketOptions
     */
    static UdpSocketOptions forAdministrative() noexcept {
        UdpSocketOptions opts;
        opts.purpose = SocketPurpose::ADMINISTRATIVE;
        opts.rcvBufferSize = 1 * 1024 * 1024;   // 1MB is sufficient
        opts.sndBufferSize = 1 * 1024 * 1024;   // 1MB is sufficient
        opts.pollingMode = PollingMode::NONE;   // No busy polling needed
        return opts;
    }
};

/**
 * @brief High-performance UDP socket class
 *
 * Zero-overhead wrapper around UDP socket file descriptor with optimizations
 * specifically designed for high-frequency trading applications.
 */
class UdpSocket : public Socket {
public:
    /**
     * @brief Create an empty UDP socket object
     */
    UdpSocket() noexcept : Socket() {}

    /**
     * @brief Create a UDP client socket for sending datagrams
     *
     * Thread-safety: This method is thread-safe, but each socket must be used
     * by only one thread at a time once created, unless external synchronization is provided.
     *
     * @param logger Logger for operation tracking
     * @param destIp IP address to send to (may be empty for bind-only)
     * @param destPort UDP port to send to
     * @param sourcePort Optional source port to bind to (0 for any)
     * @param needsTimestamp Enable hardware timestamping
     * @param options UDP socket optimization options
     * @return Initialized UDP socket or empty socket on failure
     */
    [[nodiscard]] static UdpSocket createClient(
        Logger& logger, 
        const std::string_view destIp, 
        int destPort, 
        int sourcePort = 0,
        bool needsTimestamp = false,
        const UdpSocketOptions& options = {}) noexcept {
        
        return UdpSocket(createClientSocket(logger, destIp, destPort, sourcePort, needsTimestamp, options));
    }
    
    /**
     * @brief Create a UDP server socket for receiving datagrams
     *
     * Thread-safety: This method is thread-safe, but each socket must be used
     * by only one thread at a time once created, unless external synchronization is provided.
     *
     * @param logger Logger for operation tracking
     * @param ip IP address to bind to (empty for all interfaces)
     * @param port UDP port to listen on
     * @param needsTimestamp Enable hardware timestamping
     * @param options UDP socket optimization options
     * @return Initialized UDP socket or empty socket on failure
     */
    [[nodiscard]] static UdpSocket createServer(
        Logger& logger, 
        const std::string_view ip, 
        int port, 
        bool needsTimestamp = false,
        const UdpSocketOptions& options = {}) noexcept {
        
        return UdpSocket(createServerSocket(logger, ip, port, needsTimestamp, options));
    }
    
    /**
     * @brief Join a multicast group for receiving multicast data
     *
     * @param logger Logger for operation tracking
     * @param multicastGroup Multicast group IP address
     * @param interfaceIP Optional interface IP to join on (empty for default)
     * @return true if successfully joined the multicast group
     */
    bool joinMulticastGroup(
        Logger& logger,
        const std::string_view multicastGroup,
        const std::string_view interfaceIP = {}) const noexcept {
        
        if (!isValid()) {
            logger.log("%:% %() % Cannot join multicast group: invalid socket\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr());
            return false;
        }
        
        struct ip_mreq mreq{};
        
        // Convert multicast group to in_addr
        if (inet_pton(AF_INET, multicastGroup.data(), &mreq.imr_multiaddr) != 1) {
            logger.log("%:% %() % Failed to convert multicast group '%': %\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                      multicastGroup, strerror(errno));
            return false;
        }
        
        // Set interface address
        if (!interfaceIP.empty()) {
            // Use specified interface
            if (inet_pton(AF_INET, interfaceIP.data(), &mreq.imr_interface) != 1) {
                logger.log("%:% %() % Failed to convert interface IP '%': %\n",
                          __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                          interfaceIP, strerror(errno));
                return false;
            }
        } else {
            // Use default interface (INADDR_ANY)
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        }
        
        // Join multicast group
        if (setsockopt(getFd(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            logger.log("%:% %() % Failed to join multicast group '%': %\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                      multicastGroup, strerror(errno));
            return false;
        }
        
        logger.log("%:% %() % Successfully joined multicast group % on interface %\n",
                  __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                  multicastGroup, interfaceIP.empty() ? std::string_view("default") : interfaceIP);
        
        return true;
    }
    
    /**
     * @brief Leave a multicast group
     *
     * @param logger Logger for operation tracking
     * @param multicastGroup Multicast group IP address
     * @param interfaceIP Optional interface IP to leave on (empty for default)
     * @return true if successfully left the multicast group
     */
    bool leaveMulticastGroup(
        Logger& logger,
        const std::string_view multicastGroup,
        const std::string_view interfaceIP = {}) const noexcept {
        
        if (!isValid()) {
            logger.log("%:% %() % Cannot leave multicast group: invalid socket\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr());
            return false;
        }
        
        struct ip_mreq mreq{};
        
        // Convert multicast group to in_addr
        if (inet_pton(AF_INET, multicastGroup.data(), &mreq.imr_multiaddr) != 1) {
            logger.log("%:% %() % Failed to convert multicast group '%': %\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                      multicastGroup, strerror(errno));
            return false;
        }
        
        // Set interface address
        if (!interfaceIP.empty()) {
            // Use specified interface
            if (inet_pton(AF_INET, interfaceIP.data(), &mreq.imr_interface) != 1) {
                logger.log("%:% %() % Failed to convert interface IP '%': %\n",
                          __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                          interfaceIP, strerror(errno));
                return false;
            }
        } else {
            // Use default interface (INADDR_ANY)
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        }
        
        // Leave multicast group
        if (setsockopt(getFd(), IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            logger.log("%:% %() % Failed to leave multicast group '%': %\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                      multicastGroup, strerror(errno));
            return false;
        }
        
        logger.log("%:% %() % Successfully left multicast group % on interface %\n",
                  __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                  multicastGroup, interfaceIP.empty() ? std::string_view("default") : interfaceIP);
        
        return true;
    }

    /**
     * @brief Set multicast interface for sending multicast packets
     *
     * @param logger Logger for operation tracking
     * @param interfaceIP Interface IP address to use for multicast
     * @return true if successfully set the multicast interface
     */
    bool setMulticastInterface(
        Logger& logger,
        const std::string_view interfaceIP) const noexcept {
        
        if (!isValid()) {
            logger.log("%:% %() % Cannot set multicast interface: invalid socket\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr());
            return false;
        }
        
        struct in_addr addr{};
        
        // Convert interface IP to in_addr
        if (inet_pton(AF_INET, interfaceIP.data(), &addr) != 1) {
            logger.log("%:% %() % Failed to convert interface IP '%': %\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                      interfaceIP, strerror(errno));
            return false;
        }
        
        // Set multicast interface
        if (setsockopt(getFd(), IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) < 0) {
            logger.log("%:% %() % Failed to set multicast interface '%': %\n",
                      __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                      interfaceIP, strerror(errno));
            return false;
        }
        
        logger.log("%:% %() % Set multicast interface to %\n",
                  __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(),
                  interfaceIP);
        
        return true;
    }
    
    /**
     * @brief Add socket to an epoll instance
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
        event.data.ptr = userData ? userData : const_cast<UdpSocket*>(this);
        
        return epoll_ctl(epollFd, EPOLL_CTL_ADD, getFd(), &event);
    }
    
    /**
     * @brief Modify this socket's epoll registration
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
        event.data.ptr = userData ? userData : const_cast<UdpSocket*>(this);
        
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
    
    /**
     * @brief Get socket address for a multicast group
     *
     * Helper function to create a sockaddr_in structure for multicast groups
     *
     * @param multicastIP Multicast group IP address
     * @param port UDP port for the multicast group
     * @return Configured sockaddr_in structure
     */
    static sockaddr_in getMulticastSockAddr(
        const std::string_view multicastIP,
        int port) noexcept {
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        inet_pton(AF_INET, multicastIP.data(), &addr.sin_addr);
        
        return addr;
    }

private:
    /**
     * @brief Construct from an existing socket file descriptor
     * @param fd Socket file descriptor to take ownership of
     */
    explicit UdpSocket(int fd) noexcept : Socket(fd) {}
    
    /**
    * @brief Apply UDP-specific performance optimizations to a socket with error reporting
    *
    * Configures socket options for optimal UDP performance in an HFT environment.
    * This function applies UDP-specific optimizations with targeted error checking
    * that balances operational visibility with performance.
    *
    * Error handling approach:
    * - Critical options (buffer sizes, broadcast): Return false on failure
    * - Non-critical options: Log warnings but continue execution
    * - All error conditions are logged for operational visibility
    * - Multicast options are logged but failures don't prevent socket creation
    *
    * @param socketFd Valid socket file descriptor
    * @param options UDP socket optimization options
    * @param logger Logger for reporting option application errors
    * @return true if critical optimizations were successfully applied
    */
    [[nodiscard]] static bool optimizeUdpSocket(
        const int socketFd, 
        const UdpSocketOptions& options,
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

        // Enable broadcast if requested - critical for broadcast applications
        if (options.enableBroadcast) {
            if (setsockopt(socketFd, SOL_SOCKET, SO_BROADCAST, &ENABLE_OPTION,
                        sizeof(ENABLE_OPTION)) == SOCKET_ERROR) {
                logger.log("Error: Failed to set SO_BROADCAST: %\n", strerror(errno));
                return false;
            }
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
        
        // Configure for multicast if requested
        if (options.enableMulticast) {
            // Set multicast TTL
            const unsigned char ttl = static_cast<unsigned char>(options.multicastTtl);
            if (setsockopt(socketFd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) == SOCKET_ERROR) {
                logger.log("Warning: Failed to set IP_MULTICAST_TTL to %: %\n", 
                        ttl, strerror(errno));
            }
            
            // Configure multicast loopback
            const int loopbackValue = options.enableLoopback ? ENABLE_OPTION : DISABLE_OPTION;
            if (setsockopt(socketFd, IPPROTO_IP, IP_MULTICAST_LOOP, &loopbackValue,
                        sizeof(loopbackValue)) == SOCKET_ERROR) {
                logger.log("Warning: Failed to set IP_MULTICAST_LOOP to %: %\n", 
                        loopbackValue, strerror(errno));
            }
            
            // Set interface if specified
            if (options.multicastInterface != 0) {
                const struct in_addr addr{
                    .s_addr = htonl(static_cast<uint32_t>(options.multicastInterface))
                };
                if (setsockopt(socketFd, IPPROTO_IP, IP_MULTICAST_IF, &addr, sizeof(addr)) == SOCKET_ERROR) {
                    logger.log("Warning: Failed to set IP_MULTICAST_IF to interface 0x%X: %\n", 
                            options.multicastInterface, strerror(errno));
                }
            }
        } else {
            // Set standard TTL for non-multicast
            const int ttl = options.ttl;
            if (setsockopt(socketFd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == SOCKET_ERROR) {
                logger.log("Warning: Failed to set IP_TTL to %: %\n", ttl, strerror(errno));
            }
        }

        return true;
    }
    
    /**
     * @brief Implementation for creating a UDP client socket
     *
     * @param logger Logger for operation tracking
     * @param destIp IP address to send to
     * @param destPort UDP port to send to
     * @param sourcePort Source port to bind to (0 for any)
     * @param needsTimestamp Enable hardware timestamping
     * @param options UDP socket optimization options
     * @return Socket file descriptor or SOCKET_ERROR on failure
     */
    [[nodiscard]] static int createClientSocket(
        Logger& logger,
        const std::string_view destIp,
        int destPort,
        int sourcePort,
        bool needsTimestamp,
        const UdpSocketOptions& options) noexcept {
        
        // Log socket creation attempt
        logger.log("%:% %() % Creating UDP client socket to %:%\n", 
                  __FILE__, __LINE__, __FUNCTION__,
                  getCurrentTimeStr(), destIp, destPort);
        
        // Create socket
        const int socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketFd == SOCKET_ERROR) {
            logger.log("%:% %() % Failed to create UDP socket: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            return SOCKET_ERROR;
        }
        
        // Set non-blocking mode
        if (!setNonBlocking(socketFd)) {
            logger.log("%:% %() % Failed to set UDP socket to non-blocking: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
        
        // Apply UDP optimizations
        if (!optimizeUdpSocket(socketFd, options, logger)) {
            logger.log("%:% %() % Failed to apply UDP optimizations: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
        
        // Apply timestamping if requested
        if (needsTimestamp && !setSOTimestamp(socketFd)) {
            logger.log("%:% %() % Failed to set timestamp option: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
        
        // Bind to source port if specified
        if (sourcePort > 0) {
            // Configure bind address
            sockaddr_in bindAddr{};
            bindAddr.sin_family = AF_INET;
            bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            bindAddr.sin_port = htons(static_cast<uint16_t>(sourcePort));
            
            // Bind socket
            if (bind(socketFd, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
                logger.log("%:% %() % Failed to bind UDP socket to port %: %\n", 
                          __FILE__, __LINE__, __FUNCTION__,
                          getCurrentTimeStr(), sourcePort, strerror(errno));
                close(socketFd);
                return SOCKET_ERROR;
            }
            
            logger.log("%:% %() % Bound UDP client to source port %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), sourcePort);
        }
        
        // Connect to destination if specified
        if (!destIp.empty() && destPort > 0) {
            // Configure destination address
            sockaddr_in destAddr{};
            destAddr.sin_family = AF_INET;
            
            // Convert IP to binary form
            if (inet_pton(AF_INET, destIp.data(), &destAddr.sin_addr) != 1) {
                logger.log("%:% %() % Invalid destination IP '%': %\n", 
                          __FILE__, __LINE__, __FUNCTION__,
                          getCurrentTimeStr(), destIp, strerror(errno));
                close(socketFd);
                return SOCKET_ERROR;
            }
            
            destAddr.sin_port = htons(static_cast<uint16_t>(destPort));
            
            // Connect socket (for UDP this just sets the default destination)
            if (connect(socketFd, reinterpret_cast<struct sockaddr*>(&destAddr), sizeof(destAddr)) != 0) {
                logger.log("%:% %() % Failed to connect UDP socket to %:%: %\n", 
                          __FILE__, __LINE__, __FUNCTION__,
                          getCurrentTimeStr(), destIp, destPort, strerror(errno));
                close(socketFd);
                return SOCKET_ERROR;
            }
            
            logger.log("%:% %() % Connected UDP socket to %:%\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), destIp, destPort);
        }
        
        return socketFd;
    }
    
    /**
     * @brief Implementation for creating a UDP server socket
     *
     * @param logger Logger for operation tracking
     * @param ip IP address to bind to (empty for all interfaces)
     * @param port UDP port to listen on
     * @param needsTimestamp Enable hardware timestamping
     * @param options UDP socket optimization options
     * @return Socket file descriptor or SOCKET_ERROR on failure
     */
    [[nodiscard]] static int createServerSocket(
        Logger& logger,
        const std::string_view ip,
        int port,
        bool needsTimestamp,
        const UdpSocketOptions& options) noexcept {
        
        // Log socket creation attempt
        logger.log("%:% %() % Creating UDP server socket on %:%\n", 
                  __FILE__, __LINE__, __FUNCTION__,
                  getCurrentTimeStr(), 
                  ip.empty() ? std::string_view("*") : ip, port);
        
        // Create socket
        const int socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketFd == SOCKET_ERROR) {
            logger.log("%:% %() % Failed to create UDP socket: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            return SOCKET_ERROR;
        }
        
        // Set non-blocking mode
        if (!setNonBlocking(socketFd)) {
            logger.log("%:% %() % Failed to set UDP socket to non-blocking: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
        
        // Apply UDP optimizations
        if (!optimizeUdpSocket(socketFd, options, logger)) {
            logger.log("%:% %() % Failed to apply UDP optimizations: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
        
        // Enable address reuse to prevent "address already in use" errors
        const int enableReuse = ENABLE_SOCKET_OPTION;
        if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &enableReuse,
                      sizeof(enableReuse)) != SOCKET_OK) {
            logger.log("%:% %() % Failed to set SO_REUSEADDR: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
        
        // Apply timestamping if requested
        if (needsTimestamp && !setSOTimestamp(socketFd)) {
            logger.log("%:% %() % Failed to set timestamp option: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(), strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
        
        // Configure bind address
        sockaddr_in bindAddr{};
        bindAddr.sin_family = AF_INET;
        
        if (ip.empty()) {
            // Bind to all interfaces
            bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        } else {
            // Bind to specific interface
            if (inet_pton(AF_INET, ip.data(), &bindAddr.sin_addr) != 1) {
                logger.log("%:% %() % Invalid bind IP '%': %\n", 
                          __FILE__, __LINE__, __FUNCTION__,
                          getCurrentTimeStr(), ip, strerror(errno));
                close(socketFd);
                return SOCKET_ERROR;
            }
        }
        
        bindAddr.sin_port = htons(static_cast<uint16_t>(port));
        
        // Bind socket
        if (bind(socketFd, reinterpret_cast<struct sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
            logger.log("%:% %() % Failed to bind UDP socket to %:%: %\n", 
                      __FILE__, __LINE__, __FUNCTION__,
                      getCurrentTimeStr(),
                      ip.empty() ? std::string_view("*") : ip, port, strerror(errno));
            close(socketFd);
            return SOCKET_ERROR;
        }
        
        logger.log("%:% %() % Successfully created UDP server on %:%\n", 
                  __FILE__, __LINE__, __FUNCTION__,
                  getCurrentTimeStr(),
                  ip.empty() ? std::string_view("*") : ip, port);
        
        return socketFd;
    }
};

}  // namespace Trading::Core
