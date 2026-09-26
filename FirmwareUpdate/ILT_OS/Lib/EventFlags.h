/**
  ******************************************************************************
  * @file    EventFlags.h
  * @brief   A set of bits threads can wait on, individually or in combination.
  *
  * What a semaphore cannot do: wait for several different things at once, and
  * find out which of them happened.
  *
  *     enum : std::uint32_t {
  *         kLinkUp    = 1U << 0,
  *         kAddressUp = 1U << 1,
  *         kShutdown  = 1U << 2,
  *     };
  *     ilt::EventFlags g_net;
  *
  *     const std::uint32_t got = g_net.waitAny(kAddressUp | kShutdown);
  *     if (got & kShutdown) { return; }
  *
  * Only bits 0..23 are usable: FreeRTOS event groups reserve the top eight bits
  * of the word for their own bookkeeping, and CMSIS reports an error rather
  * than silently truncating if a caller strays into them.
  *
  * Setting flags is safe from an interrupt; waiting is not.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_EVENTFLAGS_H
#define ILT_OS_LIB_EVENTFLAGS_H

#include "Kernel.h"

#include "cmsis_os2.h"

#include "FreeRTOS.h"
#include "event_groups.h"

namespace ilt {

class EventFlags
{
public:
    /** @brief Highest usable flag bit; bits 24..31 belong to the kernel. */
    static constexpr std::uint32_t kMaxFlags = 0x00FFFFFFU;

    explicit EventFlags(const char *name = nullptr) noexcept;
    ~EventFlags();

    EventFlags(const EventFlags &)            = delete;
    EventFlags &operator=(const EventFlags &) = delete;
    EventFlags(EventFlags &&)                 = delete;
    EventFlags &operator=(EventFlags &&)      = delete;

    bool isValid() const noexcept { return handle_ != nullptr; }

    /**
     * @brief Raise the given bits, waking anything whose condition is now met.
     *
     * Safe from an interrupt.
     *
     * @return The flag set after the change, or 0 if the call was rejected.
     */
    std::uint32_t set(std::uint32_t flags) noexcept;

    /** @brief Lower the given bits. Returns the set before the change. */
    std::uint32_t clear(std::uint32_t flags) noexcept;

    /** @brief The bits currently raised. */
    std::uint32_t get() const noexcept;

    /**
     * @brief Wait until ANY of @p flags is raised.
     *
     * @param flags     Bits of interest.
     * @param timeoutMs How long to wait.
     * @param autoClear Clear the bits that satisfied the wait before returning.
     *                  On by default, which is what makes each event consumed
     *                  once; turn it off for a latched condition such as "the
     *                  link is up", which every thread should be able to see.
     * @return The flags that satisfied the wait, or 0 on timeout.
     */
    std::uint32_t waitAny(std::uint32_t flags,
                          std::uint32_t timeoutMs = kWaitForever,
                          bool autoClear = true) noexcept;

    /**
     * @brief Wait until ALL of @p flags are raised simultaneously.
     * @return The flags that satisfied the wait, or 0 on timeout.
     */
    std::uint32_t waitAll(std::uint32_t flags,
                          std::uint32_t timeoutMs = kWaitForever,
                          bool autoClear = true) noexcept;

    osEventFlagsId_t handle() const noexcept { return handle_; }

private:
    std::uint32_t wait(std::uint32_t flags, std::uint32_t options,
                       std::uint32_t timeoutMs) noexcept;

    osEventFlagsAttr_t attr_{};
    StaticEventGroup_t controlBlock_{};
    osEventFlagsId_t   handle_{nullptr};
};

} // namespace ilt

#endif /* ILT_OS_LIB_EVENTFLAGS_H */
