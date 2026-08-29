/**
  ******************************************************************************
  * @file    Thread.cpp
  * @brief   C++ thread abstraction over CMSIS-RTOS v2 / FreeRTOS.
  ******************************************************************************
  */

#include "Thread.h"

namespace ilt {

Thread::Thread(const char *name, uint32_t stackBytes,
               osPriority_t priority) noexcept
{
    attr_.name       = name;
    attr_.stack_size = stackBytes;
    attr_.priority   = priority;
    /* cb_mem/stack_mem stay null => osThreadNew() allocates from the FreeRTOS
       heap. useStaticStorage() fills them in to opt out of that. */
}

Thread::~Thread()
{
    /* The running task dereferences `this` on every loop of run(), so it must
       not outlive the object. This only helps for an orderly teardown; a thread
       destroyed while holding a lock still leaves that lock held. */
    terminate();
}

void Thread::useStaticStorage(void *controlBlock, uint32_t controlBlockSize,
                              void *stack, uint32_t stackBytes) noexcept
{
    attr_.cb_mem     = controlBlock;
    attr_.cb_size    = controlBlockSize;
    attr_.stack_mem  = stack;
    attr_.stack_size = stackBytes;
}

bool Thread::start() noexcept
{
    if (handle_ != nullptr)
    {
        return false; /* already running */
    }

    handle_ = osThreadNew(&Thread::trampoline, this, &attr_);
    return handle_ != nullptr;
}

bool Thread::terminate() noexcept
{
    if (handle_ == nullptr)
    {
        return false;
    }

    const osThreadId_t id = handle_;
    handle_ = nullptr;
    return osThreadTerminate(id) == osOK;
}

uint32_t Thread::stackHighWaterMarkWords() const noexcept
{
    if (handle_ == nullptr)
    {
        return 0U;
    }

    /* Despite what the CMSIS-RTOS v2 docs say about bytes, this port returns
       uxTaskGetStackHighWaterMark() unscaled, i.e. in words. */
    return osThreadGetStackSpace(handle_);
}

void Thread::trampoline(void *arg)
{
    Thread *const self = static_cast<Thread *>(arg);

    self->run();

    /* osThreadNew() hands the function straight to xTaskCreate() with no
       wrapper, so simply returning would drop into FreeRTOS's
       prvTaskExitError(): a configASSERT() followed by an infinite loop with
       interrupts disabled. Delete the task instead. */
    self->handle_ = nullptr;
    osThreadExit();
}

} // namespace ilt
