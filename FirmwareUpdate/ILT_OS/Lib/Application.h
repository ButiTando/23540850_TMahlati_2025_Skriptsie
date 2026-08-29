/**
  ******************************************************************************
  * @file    Application.h
  * @brief   Contract between main() and whichever application is built.
  *
  * Exactly one application is compiled into the image, selected at configure
  * time with -DILT_APP=<name> (a directory under ILT_OS/Applications). Each one
  * provides ILT_ApplicationStart(); main() calls it without knowing which.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_APPLICATION_H
#define ILT_OS_LIB_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the application's threads.
 *
 * Called from main() after osKernelInitialize() and before osKernelStart(),
 * so osThreadNew() is usable but nothing is scheduled yet.
 */
void ILT_ApplicationStart(void);

#ifdef __cplusplus
}
#endif

#endif /* ILT_OS_LIB_APPLICATION_H */
