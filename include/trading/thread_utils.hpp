#pragma once

#include <pthread.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

namespace Trading::Core {

#ifdef __linux__
[[nodiscard]] inline bool pinThreadToCore(int coreId) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);

    return (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),
                                   &cpuset) == 0);
}
#else
[[nodiscard]] inline bool pinThreadToCore(int /*coreId*/) noexcept {
    return true;
}
#endif

template <typename F, typename... Args>
[[nodiscard]] inline auto createPinnedThread(int coreId,
                                             std::string_view name,
                                             F&& func,
                                             Args&&... args) {
    return std::thread([func = std::forward<F>(func),
                        ... args = std::forward<Args>(args), coreId,
                        name]() mutable {
        if (coreId >= 0 && !pinThreadToCore(coreId)) {
            throw std::runtime_error("Failed to pin thread: " +
                                     std::string{name});
        }

        func(std::forward<Args>(args)...);
    });
}

}  // namespace Trading::Core
