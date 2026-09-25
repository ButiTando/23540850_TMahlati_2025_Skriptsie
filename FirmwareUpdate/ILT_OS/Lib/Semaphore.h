/**
  ******************************************************************************
  * @file    Semaphore.h
  * @brief   Counting and binary semaphores.
  *
  * The primitive for signalling, as opposed to Mutex, which is the primitive
  * for locking. A semaphore has no owner, so unlike a mutex it may be given
  * from an interrupt -- which is what makes it the way an ISR wakes a thread:
  *
  *     ilt::BinarySemaphore g_dataReady;         // file scope
  *
  *     void DMA1_Stream0_IRQHandler(void) { g_dataReady.release(); }
  *
  *     void Worker::run() {
  *         for (;;) {
  *             if (g_dataReady.acquire(1000)) { process(); }
  *             else { reportStall(); }
  *         }
  *     }
  *
  * The control block lives inside the object, so a Semaphore costs no FreeRTOS
  * heap and may be a member or a file-scope static.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_SEMAPHORE_H
#define ILT_OS_LIB_SEMAPHORE_H

#include "Kernel.h"

#include "cmsis_os2.h"

#include "FreeRTOS.h"
#include "semphr.h"

namespace ilt {

/**
 * @brief A counting semaphore.
 *
 * Holds a count between 0 and maxCount. acquire() decrements it, blocking while
 * it is zero; release() increments it, waking one waiter.
 *
 * Use it to hand out a fixed number of somethings (slots in a pool, permits to
 * use a bus), or to count events an ISR has produced. Do NOT use it as a lock:
 * it has no owner and therefore no priority inheritance, which is exactly the
 * gap that lets an unrelated medium-priority thread starve the holder.
 */
class Semaphore
{
public:
    /**
     * @param maxCount     Highest the count may reach; must be at least 1.
     * @param initialCount Starting count; must not exceed @p maxCount.
     * @param name         Optional debugger label, stored by pointer.
     */
    Semaphore(std::uint32_t maxCount, std::uint32_t initialCount,
              const char *name = nullptr) noexcept;
    ~Semaphore();

    Semaphore(const Semaphore &)            = delete;
    Semaphore &operator=(const Semaphore &) = delete;
    Semaphore(Semaphore &&)                 = delete;
    Semaphore &operator=(Semaphore &&)      = delete;

    bool isValid() const noexcept { return handle_ != nullptr; }

    /**
     * @brief Take one count, waiting up to @p timeoutMs.
     *
     * Only legal from an ISR with kNoWait; blocking in an interrupt is not.
     *
     * @return true if a count was taken.
     */
    bool acquire(std::uint32_t timeoutMs = kWaitForever) noexcept;

    /** @brief Take one count only if one is available right now. */
    bool tryAcquire() noexcept { return acquire(kNoWait); }

    /**
     * @brief Give one count back, waking a waiter.
     *
     * Safe from an interrupt.
     *
     * @return false if the count is already at maxCount, i.e. the signal was
     *         dropped. For a BinarySemaphore that simply means "already
     *         pending" and is usually not an error.
     */
    bool release() noexcept;

    /** @brief Current count. A sample; it can change the instant it returns. */
    std::uint32_t count() const noexcept;

    osSemaphoreId_t handle() const noexcept { return handle_; }

private:
    osSemaphoreAttr_t attr_{};
    StaticSemaphore_t controlBlock_{};
    osSemaphoreId_t   handle_{nullptr};
};

/**
 * @brief A semaphore that counts no higher than one.
 *
 * The usual ISR-to-thread signal: "something happened, go look". Because the
 * count saturates at one, two events arriving before the thread runs wake it
 * once -- so the thread must drain whatever is pending rather than assume one
 * wake means one item. That is a feature; it is what stops a slow consumer
 * building an unbounded backlog of wakeups.
 */
class BinarySemaphore : public Semaphore
{
public:
    /**
     * @param initiallyAvailable Whether the semaphore starts already signalled.
     */
    explicit BinarySemaphore(bool initiallyAvailable = false,
                             const char *name = nullptr) noexcept
        : Semaphore(1U, initiallyAvailable ? 1U : 0U, name)
    {
    }
};

} // namespace ilt

#endif /* ILT_OS_LIB_SEMAPHORE_H */
