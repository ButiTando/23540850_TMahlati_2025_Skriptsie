/**
  ******************************************************************************
  * @file    Mutex.cpp
  * @brief   Mutual exclusion with priority inheritance.
  ******************************************************************************
  */

#include "Mutex.h"

namespace ilt {

Mutex::Mutex(const char *name, std::uint32_t attributeBits) noexcept
{
    attr_.name      = name;
    attr_.attr_bits = attributeBits;
    attr_.cb_mem    = &controlBlock_;
    attr_.cb_size   = sizeof(controlBlock_);

    /* osMutexNew() only refuses static memory if cb_size is too small or the
       call is made from an ISR, so this cannot fail for a correctly sized
       control block in thread context. isValid() reports it either way rather
       than asserting, because a constructor running before main() has nowhere
       useful to report a failure to. */
    handle_ = osMutexNew(&attr_);
}

Mutex::Mutex(const char *name) noexcept
    : Mutex(name, 0U)
{
}

Mutex::~Mutex()
{
    if (handle_ != nullptr)
    {
        osMutexDelete(handle_);
        handle_ = nullptr;
    }
}

bool Mutex::lock(std::uint32_t timeoutMs) noexcept
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return osMutexAcquire(handle_, kernel::msToTicks(timeoutMs)) == osOK;
}

bool Mutex::unlock() noexcept
{
    if (handle_ == nullptr)
    {
        return false;
    }

    return osMutexRelease(handle_) == osOK;
}

RecursiveMutex::RecursiveMutex(const char *name) noexcept
    : Mutex(name, osMutexRecursive)
{
}

} // namespace ilt
