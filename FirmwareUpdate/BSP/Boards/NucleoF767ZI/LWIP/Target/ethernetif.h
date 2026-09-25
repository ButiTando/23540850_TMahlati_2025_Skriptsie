/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * File Name          : ethernetif.h
  * Description        : This file provides initialization code for LWIP
  *                      middleWare.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__
#include "lwip/err.h"
#include "lwip/netif.h"

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */
/* BoardEthernet.cpp publishes these through Bsp/Ethernet.h, so the header has
   to be callable from C++. CubeMX does not emit the guard for this file. */
#ifdef __cplusplus
extern "C" {
#endif
/* USER CODE END 0 */

/* Exported functions ------------------------------------------------------- */
err_t ethernetif_init(struct netif *netif);

/* An RTOS thread body, not a poll hook: lwIP is built with NO_SYS=0, so this
   runs forever on its own task and takes the netif as its argument. */
void ethernetif_input(void *argument);
void ethernet_link_check_state(struct netif *netif);

void Error_Handler(void);
u32_t sys_jiffies(void);
u32_t sys_now(void);

/* USER CODE BEGIN 1 */
#ifdef __cplusplus
}
#endif
/* USER CODE END 1 */
#endif
