#pragma once

#include <pthread.h>
#include <sys/_pthread/_pthread_t.h>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

namespace Common {

#ifdef __linux__
inline auto tryToSetThreadAffinity(int coreId) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);

    return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),
                                   &cpuset) == 0);
}
#else
inline auto tryToSetThreadAffinity(int /*coreId*/) noexcept {
    // CPU affinity not supported on this platform
    return true;
}
#endif

template <typename T, typename... Args>
inline auto createAndStartThread(int coreId,
                                 const std::string& name,
                                 T&& func,
                                 Args&&... args) {
    return std::thread([&]() {
        if (coreId > 0 && !tryToSetThreadAffinity(coreId)) {
            throw std::runtime_error("Failed to set core affinity for: " +
                                     name);
        }

        std::forward<T>(func)((std::forward<Args>(args))...);
    });
}
}  // namespace Common
