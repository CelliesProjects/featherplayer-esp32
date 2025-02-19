#ifndef SCOPEDMUTEX_H
#define SCOPEDMUTEX_H

#include <Arduino.h>
#include <freertos/semphr.h>

/**
 * @brief Constructs a ScopedMutex object and attempts to acquire the mutex.
 *
 * Guarantees mutex release when the `ScopedMutex` object goes out of scope.
 *
 * This class prevents deadlocks by ensuring that the mutex is always released,
 * even if exceptions are thrown or the code exits early.
 *
 * @param m The FreeRTOS semaphore handle.
 * @param timeout The timeout in FreeRTOS ticks. Defaults to portMAX_DELAY (indefinite blocking).
 * @note -  If no timeout is specified, the mutex acquisition will block indefinitely and is guaranteed to return an acquired mutex.
 *
 *       -  If a timeout is provided, the `acquired()` method should be used to check if the mutex was successfully acquired.
 *
 * Example Usage:
 * ```cpp
 * {
 *     ScopedMutex lock(myMutex);
 *
 *     // Access shared resource here
 *
 * } // Mutex automatically released
 *
 * ```
 */
class ScopedMutex
{
private:
    SemaphoreHandle_t &mutex;
    bool locked;

public:
    explicit ScopedMutex(SemaphoreHandle_t &m, TickType_t timeout = portMAX_DELAY)
        : mutex(m), locked(xSemaphoreTake(mutex, timeout)) {}

    ScopedMutex(const ScopedMutex &) = delete;
    ScopedMutex &operator=(const ScopedMutex &) = delete;

    /**
     * @brief Destroys the ScopedMutex object and releases the mutex if it was acquired.
     */
    ~ScopedMutex()
    {
        if (locked)
        {
            xSemaphoreGive(mutex);
        }
    }

    /**
     * @brief Returns true if the mutex was successfully acquired.
     * @return True if the mutex was acquired, false otherwise (e.g., due to timeout).
     */
    bool acquired() const { return locked; }
};

#endif // SCOPEDMUTEX_H
