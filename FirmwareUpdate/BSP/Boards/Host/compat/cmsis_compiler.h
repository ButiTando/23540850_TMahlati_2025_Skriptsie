/**
  ******************************************************************************
  * @file    cmsis_compiler.h
  * @brief   Host stand-in for the CMSIS-Core compiler header.
  *
  * cmsis_os2.c includes this for two Cortex-M intrinsics it uses to decide
  * whether it is running in an interrupt:
  *
  *     IS_IRQ_MODE()   -> __get_IPSR()   != 0
  *     IS_IRQ_MASKED() -> __get_PRIMASK() != 0
  *
  * Neither concept exists in a Linux process. The host has no interrupt
  * context, so both answer zero and every CMSIS call takes its thread-mode
  * path -- which is the truthful answer, not a convenient one.
  *
  * This file is on the include path for the Host board only. The real CMSIS
  * header is used on the MCU boards.
  ******************************************************************************
  */

#ifndef ILT_HOST_CMSIS_COMPILER_H
#define ILT_HOST_CMSIS_COMPILER_H

#include <stdint.h>

#ifndef __STATIC_INLINE
#define __STATIC_INLINE static inline
#endif
#ifndef __STATIC_FORCEINLINE
#define __STATIC_FORCEINLINE static inline
#endif
#ifndef __NO_RETURN
#define __NO_RETURN __attribute__((__noreturn__))
#endif
#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif
#ifndef __USED
#define __USED __attribute__((used))
#endif
#ifndef __ALIGNED
#define __ALIGNED(x) __attribute__((aligned(x)))
#endif

/** @brief Interrupt Program Status Register: always 0, i.e. thread mode. */
__STATIC_INLINE uint32_t __get_IPSR(void) { return 0U; }

/** @brief Priority mask: always 0, i.e. interrupts are never masked here. */
__STATIC_INLINE uint32_t __get_PRIMASK(void) { return 0U; }

/**
 * @brief Global interrupt enable/disable.
 *
 * cmsis_os2.c brackets its system-timer read with these. There is nothing to
 * mask in a Linux process, and the FreeRTOS POSIX port has its own signal-based
 * critical sections, so these are genuinely empty rather than unimplemented.
 */
__STATIC_INLINE void __disable_irq(void) {}
__STATIC_INLINE void __enable_irq(void) {}

#endif /* ILT_HOST_CMSIS_COMPILER_H */
