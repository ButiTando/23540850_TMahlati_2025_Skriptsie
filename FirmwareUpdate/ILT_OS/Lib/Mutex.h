/**
  ******************************************************************************
  * @file    Mutex.h
  * @brief   Mutual exclusion with priority inheritance, plus RAII locking.
  *
  *     class Store {
  *         mutable ilt::Mutex lock_;
  *         Reading latest_;
  *     public:
  *         void publish(const Reading &r) {
  *             ilt::LockGuard guard(lock_);
  *             latest_ = r;
  *         }
  *     };
  *
  * The control block lives inside the object, so a Mutex costs no FreeRTOS heap
  * and can be a member or a file-scope static. It is created by the constructor
  * and so may be constructed before main() -- FreeRTOS allows a statically
  * allocated mutex to be created before the scheduler starts.
  *
  * A mutex may NOT be used from an interrupt: priority inheritance only means
  * something for a thread, and an ISR cannot block to wait for one. Use a
  * Semaphore, an EventFlags, or ilt::InterruptLock to talk to an ISR.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_MUTEX_H
#define ILT_OS_LIB_MUTEX_H

#include "Kernel.h"

#include "cmsis_os2.h"

#include "FreeRTOS.h"
#include "semphr.h"

namespace ilt {

/**
 * @brief A non-recursive mutex with priority inheritance.
 *
 * Priority inheritance is FreeRTOS's default for a mutex and is why this should
 * be preferred over a binary semaphore for locking: while a low-priority thread
 * holds the lock, it is temporarily raised to the priority of the highest
 * thread waiting, so a medium-priority thread cannot keep the holder off the
 * CPU indefinitely.
 *
 * Locking the same mutex twice from one thread deadlocks it against itself --
 * use RecursiveMutex if that is genuinely needed, though it usually means the
 * locking is in the wrong place.
 */
class Mutex
{
public:
    /**
     * @param name Optional; appears in the FreeRTOS queue registry, which is
     *             what a debugger reads to label the object. Stored by pointer.
     */
    explicit Mutex(const char *name = nullptr) noexcept;
    ~Mutex();

    Mutex(const Mutex &)            = delete;
    Mutex &operator=(const Mutex &) = delete;
    Mutex(Mutex &&)                 = delete;
    Mutex &operator=(Mutex &&)      = delete;

    /** @brief Whether the mutex was created. False only if the RTOS refused. */
    bool isValid() const noexcept { return handle_ != nullptr; }

    /**
     * @brief Take the lock, waiting up to @p timeoutMs.
     * @return true if the lock is now held by the calling thread.
     */
    bool lock(std::uint32_t timeoutMs = kWaitForever) noexcept;

    /** @brief Take the lock only if it is free right now. */
    bool tryLock() noexcept { return lock(kNoWait); }

    /**
     * @brief Release the lock.
     * @return false if the caller was not the owner, which is a bug in the
     *         caller rather than a condition to handle.
     */
    bool unlock() noexcept;

    osMutexId_t handle() const noexcept { return handle_; }

protected:
    /** Used by RecursiveMutex to pass osMutexRecursive. */
    Mutex(const char *name, std::uint32_t attributeBits) noexcept;

private:
    osMutexAttr_t   attr_{};
    StaticSemaphore_t controlBlock_{};
    osMutexId_t     handle_{nullptr};
};

/**
 * @brief A mutex the owning thread may lock more than once.
 *
 * It must be unlocked as many times as it was locked. Reach for this only when
 * a public method that locks has to call another public method that locks; the
 * tidier fix is usually a private unlocked helper that both call.
 */
class RecursiveMutex : public Mutex
{
public:
    explicit RecursiveMutex(const char *name = nullptr) noexcept;
};

/**
 * @brief Holds a Mutex for the enclosing scope.
 *
 * Waits without a deadline, so the lock is always held on return and there is
 * no failure case for the caller to forget to check. When a deadline matters,
 * call Mutex::lock(timeout) directly and act on the result.
 */
class LockGuard
{
public:
    explicit LockGuard(Mutex &mutex) noexcept
        : mutex_(mutex)
    {
        mutex_.lock();
    }

    ~LockGuard() { mutex_.unlock(); }

    LockGuard(const LockGuard &)            = delete;
    LockGuard &operator=(const LockGuard &) = delete;

private:
    Mutex &mutex_;
};

/**
 * @brief Tries to hold a Mutex for the enclosing scope, and may fail.
 *
 *     ilt::TryLockGuard guard(lock_, 10);
 *     if (!guard) { return false; }   // busy -- caller decides what to do
 *
 * Unlocks on destruction only if it actually acquired the lock.
 */
class TryLockGuard
{
public:
    TryLockGuard(Mutex &mutex, std::uint32_t timeoutMs = kNoWait) noexcept
        : mutex_(mutex), held_(mutex.lock(timeoutMs))
    {
    }

    ~TryLockGuard()
    {
        if (held_)
        {
            mutex_.unlock();
        }
    }

    TryLockGuard(const TryLockGuard &)            = delete;
    TryLockGuard &operator=(const TryLockGuard &) = delete;

    /** @brief Whether the lock is held. */
    bool owns() const noexcept { return held_; }
    explicit operator bool() const noexcept { return held_; }

private:
    Mutex &mutex_;
    bool   held_;
};

} // namespace ilt

#endif /* ILT_OS_LIB_MUTEX_H */
