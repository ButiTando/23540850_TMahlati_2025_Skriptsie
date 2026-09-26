/**
  ******************************************************************************
  * @file    Semaphore.cpp
  * @brief   Counting and binary semaphores.
  ******************************************************************************
  */

#include "Semaphore.h"

namespace ilt {

Semaphore::Semaphore(std::uint32_t maxCount, std::uint32_t initialCount,
                     const char *name) noexcept
{
    attr_.name    = name;
    attr_.cb_mem  = &controlBlock_;
    attr_.cb_size = sizeof(controlBlock_);

    /* osSemaphoreNew() rejects maxCount 0 or initialCount > maxCount; both are
       caller errors that leave handle_ null rather than half-working. */
    handle_ = osSemaphoreNew(maxCount, initialCount, &attr_);
}

Semaphore::~Semaphore()
{
    if (handle_ != nullptr)
    {
        osSemaphoreDelete(handle_);
        handle_ = nullptr;
    }
}

bool Semaphore::acquire(std::uint32_t timeoutMs) noexcept
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return osSemaphoreAcquire(handle_, kernel::msToTicks(timeoutMs)) == osOK;
}

bool Semaphore::release() noexcept
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return osSemaphoreRelease(handle_) == osOK;
}

std::uint32_t Semaphore::count() const noexcept
{
    if (handle_ == nullptr)
    {
        return 0U;
    }

    return osSemaphoreGetCount(handle_);
}

} // namespace ilt
