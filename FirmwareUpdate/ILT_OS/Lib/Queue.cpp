/**
  ******************************************************************************
  * @file    Queue.cpp
  * @brief   The type-erased half of Queue<T, Capacity>.
  ******************************************************************************
  */

#include "Queue.h"

namespace ilt {

QueueBase::~QueueBase()
{
    if (handle_ != nullptr)
    {
        osMessageQueueDelete(handle_);
        handle_ = nullptr;
    }
}

void QueueBase::create(std::uint32_t messageCount, std::uint32_t messageSize,
                       void *controlBlock, std::uint32_t controlBlockSize,
                       void *storage, std::uint32_t storageSize,
                       const char *name) noexcept
{
    attr_.name    = name;
    attr_.cb_mem  = controlBlock;
    attr_.cb_size = controlBlockSize;
    attr_.mq_mem  = storage;
    attr_.mq_size = storageSize;

    /* osMessageQueueNew() falls back to the FreeRTOS heap if it does not like
       the static memory it was handed -- so a mistake here would still appear
       to work while quietly spending heap the project does not have. The
       template supplies both sizes from sizeof, so the checks it makes
       (cb_size >= sizeof(StaticQueue_t), mq_size >= count * size) hold by
       construction. */
    handle_ = osMessageQueueNew(messageCount, messageSize, &attr_);
}

bool QueueBase::putRaw(const void *message, std::uint32_t timeoutMs) noexcept
{
    if (handle_ == nullptr)
    {
        return false;
    }

    /* Priority 0 throughout: FreeRTOS queues are strictly FIFO and ignore the
       CMSIS priority argument, so exposing it would promise an ordering the
       kernel underneath does not implement. */
    return osMessageQueuePut(handle_, message, 0U,
                             kernel::msToTicks(timeoutMs)) == osOK;
}

bool QueueBase::getRaw(void *message, std::uint32_t timeoutMs) noexcept
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return osMessageQueueGet(handle_, message, nullptr,
                             kernel::msToTicks(timeoutMs)) == osOK;
}

std::uint32_t QueueBase::count() const noexcept
{
    return handle_ != nullptr ? osMessageQueueGetCount(handle_) : 0U;
}

std::uint32_t QueueBase::space() const noexcept
{
    return handle_ != nullptr ? osMessageQueueGetSpace(handle_) : 0U;
}

std::uint32_t QueueBase::capacity() const noexcept
{
    return handle_ != nullptr ? osMessageQueueGetCapacity(handle_) : 0U;
}

bool QueueBase::reset() noexcept
{
    return handle_ != nullptr && osMessageQueueReset(handle_) == osOK;
}

} // namespace ilt
