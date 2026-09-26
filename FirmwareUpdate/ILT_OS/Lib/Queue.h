/**
  ******************************************************************************
  * @file    Queue.h
  * @brief   A fixed-capacity, type-safe message queue between threads.
  *
  *     struct Command { std::uint8_t plate; bool insert; };
  *     ilt::Queue<Command, 8> g_commands;
  *
  *     // producer (thread or ISR)
  *     g_commands.tryPut(Command{3, true});
  *
  *     // consumer
  *     Command c;
  *     while (g_commands.get(c)) { execute(c); }
  *
  * Both the control block and the storage live inside the object, so a Queue
  * costs no FreeRTOS heap and may be a member or a file-scope static. An
  * overcommit is a link-time .bss overflow rather than a start-up failure.
  *
  * Messages are COPIED in and out by value, byte for byte. That is what makes
  * the queue safe across threads -- no ownership question, no lifetime question
  * -- and it is why T is restricted to trivially copyable types. To move
  * something large, queue a pointer to it and agree separately on who frees it.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_QUEUE_H
#define ILT_OS_LIB_QUEUE_H

#include "Kernel.h"

#include "cmsis_os2.h"

#include "FreeRTOS.h"
#include "queue.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ilt {

/**
 * @brief Non-template part of Queue, so the code is emitted once.
 *
 * Every Queue<T, N> shares this: the template only adds the typed veneer and
 * the storage, which keeps flash cost flat as more queue types are used.
 */
class QueueBase
{
public:
    QueueBase(const QueueBase &)            = delete;
    QueueBase &operator=(const QueueBase &) = delete;
    QueueBase(QueueBase &&)                 = delete;
    QueueBase &operator=(QueueBase &&)      = delete;

    bool isValid() const noexcept { return handle_ != nullptr; }

    /** @brief Messages waiting to be read. */
    std::uint32_t count() const noexcept;

    /** @brief Free slots remaining. */
    std::uint32_t space() const noexcept;

    /** @brief Total slots, i.e. the Capacity template argument. */
    std::uint32_t capacity() const noexcept;

    bool isEmpty() const noexcept { return count() == 0U; }
    bool isFull() const noexcept { return space() == 0U; }

    /** @brief Discard every queued message. */
    bool reset() noexcept;

    osMessageQueueId_t handle() const noexcept { return handle_; }

protected:
    QueueBase() noexcept = default;
    ~QueueBase();

    void create(std::uint32_t messageCount, std::uint32_t messageSize,
                void *controlBlock, std::uint32_t controlBlockSize,
                void *storage, std::uint32_t storageSize,
                const char *name) noexcept;

    bool putRaw(const void *message, std::uint32_t timeoutMs) noexcept;
    bool getRaw(void *message, std::uint32_t timeoutMs) noexcept;

private:
    osMessageQueueAttr_t attr_{};
    osMessageQueueId_t   handle_{nullptr};
};

/**
 * @brief A queue of @p Capacity values of type @p T.
 *
 * @tparam T        Message type; must be trivially copyable.
 * @tparam Capacity Slots. Storage is Capacity * sizeof(T) bytes in .bss.
 */
template <typename T, std::size_t Capacity>
class Queue : public QueueBase
{
    static_assert(Capacity > 0U, "a queue needs at least one slot");
    static_assert(std::is_trivially_copyable<T>::value,
                  "queue messages are copied bytewise; T must be trivially "
                  "copyable. Queue a pointer instead for anything else.");

public:
    using value_type = T;

    explicit Queue(const char *name = nullptr) noexcept
    {
        create(static_cast<std::uint32_t>(Capacity),
               static_cast<std::uint32_t>(sizeof(T)),
               &controlBlock_, sizeof(controlBlock_),
               storage_, sizeof(storage_), name);
    }

    /**
     * @brief Append a message, waiting up to @p timeoutMs for room.
     *
     * Only legal from an ISR with kNoWait.
     *
     * @return true if the message was queued; false if it timed out, in which
     *         case the message was NOT queued and the caller still owns it.
     */
    bool put(const T &message, std::uint32_t timeoutMs = kWaitForever) noexcept
    {
        return putRaw(&message, timeoutMs);
    }

    /** @brief Append only if there is room right now. Safe from an ISR. */
    bool tryPut(const T &message) noexcept { return putRaw(&message, kNoWait); }

    /**
     * @brief Take the oldest message, waiting up to @p timeoutMs for one.
     *
     * Only legal from an ISR with kNoWait.
     *
     * @param[out] message Written only when this returns true.
     */
    bool get(T &message, std::uint32_t timeoutMs = kWaitForever) noexcept
    {
        return getRaw(&message, timeoutMs);
    }

    /** @brief Take a message only if one is waiting. Safe from an ISR. */
    bool tryGet(T &message) noexcept { return getRaw(&message, kNoWait); }

private:
    StaticQueue_t controlBlock_{};
    /* Aligned for T: FreeRTOS memcpy's messages in and out of this, and an
       under-aligned buffer would make those copies unaligned accesses. */
    alignas(T) std::uint8_t storage_[Capacity * sizeof(T)]{};
};

} // namespace ilt

#endif /* ILT_OS_LIB_QUEUE_H */
