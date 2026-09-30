// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <chrono>
#include <thread>

namespace aare {

/**
 * @brief Hint the CPU that this is a spin-wait. Prefer this over yield() when
 * the wait is expected to be short.
 */
inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

/**
 * @brief Escalating wait for producer/consumer idle and backpressure loops.
 *
 * Starts with a pause instruction (sub-microsecond, no syscall), then yield,
 * then a short sleep. Call reset() whenever work arrives so a busy pipeline
 * never leaves the pause tier.
 */
class Backoff {
    int m_count{0};

  public:
    void reset() noexcept { m_count = 0; }

    void pause() noexcept {
        if (m_count < 64) {
            cpu_relax();
        } else if (m_count < 256) {
            std::this_thread::yield();
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        ++m_count;
    }
};

} // namespace aare
