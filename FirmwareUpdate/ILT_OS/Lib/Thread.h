/**
  ******************************************************************************
  * @file    Thread.h
  * @brief   C++ thread abstraction over CMSIS-RTOS v2 / FreeRTOS.
  *
  * Derive from Thread, override run(), then call start():
  *
  *     class Blinker : public ilt::StaticThread<512> {
  *     public:
  *       Blinker() : StaticThread("blink", osPriorityNormal) {}
  *     protected:
  *       void run() override {
  *         for (;;) {
  *           HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
  *           sleep(500);
  *         }
  *       }
  *     };
  *
  *     static Blinker blinker;      // constructed by __libc_init_array
  *     ...
  *     blinker.start();             // after osKernelInitialize()
  *
  * Threads must be started *after* osKernelInitialize() and may be started
  * either before or after osKernelStart().
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_THREAD_H
#define ILT_OS_LIB_THREAD_H

#include "cmsis_os2.h"

/* StaticTask_t / StackType_t, needed by StaticThread below. */
#include "FreeRTOS.h"
#include "task.h"

#include <cstddef>
#include <cstdint>

namespace ilt {

/**
 * @brief Base class for a thread of execution.
 *
 * The task is created by start(), never by the constructor: until the derived
 * constructor has finished running, the vtable still points at Thread, and a
 * task that began executing early would dispatch run() as a pure virtual call.
 *
 * Not copyable or movable -- the running task holds the `this` pointer that was
 * handed to osThreadNew(), so the object must stay put.
 */
class Thread
{
public:
    /**
     * @param name        Thread name. FreeRTOS copies it into the TCB (truncated
     *                    to configMAX_TASK_NAME_LEN), so a temporary is safe,
     *                    though a string literal is the usual choice.
     * @param stackBytes  Stack size in BYTES, and a multiple of 4. CMSIS divides
     *                    this by sizeof(StackType_t) to get FreeRTOS words, so
     *                    passing a word count here yields a stack 4x too small.
     * @param priority    osPriorityIdle .. osPriorityISR.
     */
    Thread(const char *name, uint32_t stackBytes,
           osPriority_t priority = osPriorityNormal) noexcept;

    virtual ~Thread();

    Thread(const Thread &)            = delete;
    Thread &operator=(const Thread &) = delete;
    Thread(Thread &&)                 = delete;
    Thread &operator=(Thread &&)      = delete;

    /**
     * @brief Create and schedule the underlying task.
     * @return true on success; false if already started or the RTOS refused
     *         (out of heap, or a stack/priority the port rejects).
     */
    bool start() noexcept;

    /**
     * @brief Terminate the thread.
     *
     * Terminating a thread that holds a mutex or is mid-transaction leaves that
     * state orphaned; prefer having run() return instead.
     */
    bool terminate() noexcept;

    bool isRunning() const noexcept { return handle_ != nullptr; }
    osThreadId_t handle() const noexcept { return handle_; }
    const char *name() const noexcept { return attr_.name; }

    /** Lowest number of stack WORDS ever left unused (0 if not running). */
    uint32_t stackHighWaterMarkWords() const noexcept;
    uint32_t stackHighWaterMarkBytes() const noexcept
    {
        return stackHighWaterMarkWords() * sizeof(StackType_t);
    }

    /* --- helpers acting on the *calling* thread --- */
    static void sleep(uint32_t ms) noexcept { osDelay(ms); }
    static void yield() noexcept { osThreadYield(); }

protected:
    /**
     * @brief Thread body, supplied by the derived class.
     *
     * Normally loops forever. Returning is safe -- the trampoline calls
     * osThreadExit() so the task is deleted rather than falling off the end
     * into FreeRTOS's prvTaskExitError() trap.
     */
    virtual void run() = 0;

    /**
     * @brief Use caller-supplied memory instead of the FreeRTOS heap.
     *
     * Call before start(). Used by StaticThread; also available directly if you
     * want to place a stack in a specific section.
     */
    void useStaticStorage(void *controlBlock, uint32_t controlBlockSize,
                          void *stack, uint32_t stackBytes) noexcept;

private:
    /**
     * osThreadNew() takes a plain function pointer, which a non-static member
     * function cannot provide, so this static shim recovers the object from the
     * argument and dispatches to run().
     */
    static void trampoline(void *arg);

    osThreadAttr_t attr_{};
    osThreadId_t   handle_{nullptr};
};

/**
 * @brief A Thread whose control block and stack are members, not heap.
 *
 * configTOTAL_HEAP_SIZE is only 15360 bytes in this project, so allocating
 * thread stacks dynamically exhausts it quickly. StaticThread moves them into
 * .bss, where an overcommit is a link-time failure rather than a start()
 * returning false at runtime.
 *
 * @tparam StackBytes Stack size in bytes; multiple of sizeof(StackType_t).
 */
template <std::size_t StackBytes>
class StaticThread : public Thread
{
    static_assert(StackBytes % sizeof(StackType_t) == 0,
                  "StackBytes must be a multiple of sizeof(StackType_t)");
    static_assert(StackBytes >= configMINIMAL_STACK_SIZE * sizeof(StackType_t),
                  "StackBytes is below configMINIMAL_STACK_SIZE");

public:
    explicit StaticThread(const char *name,
                          osPriority_t priority = osPriorityNormal) noexcept
        : Thread(name, static_cast<uint32_t>(StackBytes), priority)
    {
        useStaticStorage(&controlBlock_, sizeof(controlBlock_),
                         stack_, static_cast<uint32_t>(sizeof(stack_)));
    }

private:
    StaticTask_t controlBlock_{};
    /* The Cortex-M AAPCS wants an 8-byte-aligned stack. */
    alignas(8) StackType_t stack_[StackBytes / sizeof(StackType_t)]{};
};

} // namespace ilt

#endif /* ILT_OS_LIB_THREAD_H */
