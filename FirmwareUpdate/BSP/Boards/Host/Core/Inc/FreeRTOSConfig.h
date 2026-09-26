/**
  ******************************************************************************
  * @file    FreeRTOSConfig.h
  * @brief   FreeRTOS configuration for the Host (POSIX) build.
  *
  * Deliberately different from the MCU boards in one respect that matters: the
  * POSIX port hands each task's stack straight to pthread_attr_setstack(), so
  * every stack -- including the idle and timer tasks' -- must be at least
  * glibc's PTHREAD_STACK_MIN or pthread_create() fails and the port aborts.
  * The sizes below are therefore in a completely different range from the
  * Cortex-M ones, and the heap is sized to match. RAM is free here; correctness
  * is not.
  ******************************************************************************
  */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* cmsis_os2.c reports it through osKernelGetSysTimerFreq(). Nothing on the
   host is actually clocked by it; a plausible number keeps the API honest. */
#define configCPU_CLOCK_HZ                       ((unsigned long)1000000000)

#define configUSE_PREEMPTION                     1
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0
#define configTICK_RATE_HZ                       ((TickType_t)1000)
#define configMAX_PRIORITIES                     (56)
/* In WORDS. StackType_t is 8 bytes here, so this is 64 KB -- comfortably past
   PTHREAD_STACK_MIN (16 KB on x86-64 glibc). */
#define configMINIMAL_STACK_SIZE                 ((uint16_t)8192)
#define configTOTAL_HEAP_SIZE                    ((size_t)(4 * 1024 * 1024))
#define configMAX_TASK_NAME_LEN                  (16)
#define configUSE_16_BIT_TICKS                   0
#define configIDLE_SHOULD_YIELD                  1
#define configUSE_MUTEXES                        1
#define configUSE_RECURSIVE_MUTEXES              1
#define configUSE_COUNTING_SEMAPHORES            1
#define configQUEUE_REGISTRY_SIZE                8
#define configUSE_APPLICATION_TASK_TAG           0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0
#define configSUPPORT_STATIC_ALLOCATION          1
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configUSE_TIME_SLICING                   1
#define configUSE_NEWLIB_REENTRANT               0
/* The POSIX port is written against the pre-v8 type names (pdTASK_CODE,
   portTickType), which only exist when this is on. The MCU boards keep it off. */
#define configENABLE_BACKWARD_COMPATIBILITY      1
#define configSTACK_DEPTH_TYPE                   uint32_t

/* Hook functions */
#define configCHECK_FOR_STACK_OVERFLOW           0
#define configUSE_MALLOC_FAILED_HOOK             0
#define configUSE_DAEMON_TASK_STARTUP_HOOK       0

/* Run time and task stats gathering */
#define configGENERATE_RUN_TIME_STATS            0
#define configUSE_TRACE_FACILITY                 1
#define configUSE_STATS_FORMATTING_FUNCTIONS     0

/* Co-routines */
#define configUSE_CO_ROUTINES                    0
#define configMAX_CO_ROUTINE_PRIORITIES          (2)

/* Software timers -- the timer daemon's stack is subject to the same floor. */
#define configUSE_TIMERS                         1
#define configTIMER_TASK_PRIORITY                (2)
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH             (configMINIMAL_STACK_SIZE)

/* Optional functions. The POSIX port itself needs the three task handle
   getters and the timer daemon handle; omitting any of them fails to link. */
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_vTaskCleanUpResources            0
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelayUntil                  1
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTimerPendFunctionCall           1
#define INCLUDE_xQueueGetMutexHolder             1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_eTaskGetState                    1
#define INCLUDE_xTaskGetIdleTaskHandle           1
#define INCLUDE_xTimerGetTimerDaemonTaskHandle   1

/* The Cortex-M portmacro.h defines portNOP; the POSIX one does not, and
   FreeRTOS.h provides no default. lwIP's sys_arch.c calls it. Defined here
   because FreeRTOSConfig.h is included before portmacro.h. */
#ifndef portNOP
#define portNOP()
#endif

#define configASSERT(x) \
    if ((x) == 0) { ILT_HostAssert(__FILE__, __LINE__, #x); }

#ifdef __cplusplus
extern "C" {
#endif
/** Implemented in main.cpp: prints and aborts, so a failed assert is visible. */
void ILT_HostAssert(const char *file, int line, const char *expression);
#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_CONFIG_H */
