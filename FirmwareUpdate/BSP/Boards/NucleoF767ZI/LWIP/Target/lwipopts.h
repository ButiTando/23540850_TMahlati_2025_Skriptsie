/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : Target/lwipopts.h
  * Description        : This file overrides LwIP stack default configuration
  *                      done in opt.h file.
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

/* Define to prevent recursive inclusion --------------------------------------*/
#ifndef __LWIPOPTS__H__
#define __LWIPOPTS__H__

#include "main.h"

/*-----------------------------------------------------------------------------*/
/* Current version of LwIP supported by CubeMx: 2.1.2 -*/
/*-----------------------------------------------------------------------------*/

/* Within 'USER CODE' section, code will be kept by default at each generation */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

#ifdef __cplusplus
 extern "C" {
#endif

/* STM32CubeMX Specific Parameters (not defined in opt.h) ---------------------*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- WITH_RTOS disabled (Since FREERTOS is not set) -----*/
#define WITH_RTOS 0
/*----- CHECKSUM_BY_HARDWARE enabled -----*/
#define CHECKSUM_BY_HARDWARE 1
/*-----------------------------------------------------------------------------*/

/* LwIP Stack Parameters (modified compared to initialization value in opt.h) -*/
/* Parameters set in STM32CubeMX LwIP Configuration GUI -*/
/*----- Value in opt.h for LWIP_DHCP: 0 -----*/
#define LWIP_DHCP 1
/*----- Value in opt.h for NO_SYS: 0 -----*/
#define NO_SYS 1
/*----- Value in opt.h for SYS_LIGHTWEIGHT_PROT: 1 -----*/
#define SYS_LIGHTWEIGHT_PROT 0
/*----- Value in opt.h for MEM_ALIGNMENT: 1 -----*/
#define MEM_ALIGNMENT 4
/*----- Default Value for F7 devices: 0x20048000 -----*/
#define LWIP_RAM_HEAP_POINTER 0x20048000
/*----- Value in opt.h for MEMP_NUM_SYS_TIMEOUT: (LWIP_TCP + IP_REASSEMBLY + LWIP_ARP + (2*LWIP_DHCP) + LWIP_AUTOIP + LWIP_IGMP + LWIP_DNS + (PPP_SUPPORT*6*MEMP_NUM_PPP_PCB) + (LWIP_IPV6 ? (1 + LWIP_IPV6_REASS + LWIP_IPV6_MLD) : 0)) -*/
#define MEMP_NUM_SYS_TIMEOUT 5
/*----- Value in opt.h for LWIP_ETHERNET: LWIP_ARP || PPPOE_SUPPORT -*/
#define LWIP_ETHERNET 1
/*----- Value in opt.h for LWIP_DNS_SECURE: (LWIP_DNS_SECURE_RAND_XID | LWIP_DNS_SECURE_NO_MULTIPLE_OUTSTANDING | LWIP_DNS_SECURE_RAND_SRC_PORT) -*/
#define LWIP_DNS_SECURE 7
/*----- Value in opt.h for TCP_SND_QUEUELEN: (4*TCP_SND_BUF + (TCP_MSS - 1))/TCP_MSS -----*/
#define TCP_SND_QUEUELEN 9
/*----- Value in opt.h for TCP_SNDLOWAT: LWIP_MIN(LWIP_MAX(((TCP_SND_BUF)/2), (2 * TCP_MSS) + 1), (TCP_SND_BUF) - 1) -*/
#define TCP_SNDLOWAT 1071
/*----- Value in opt.h for TCP_SNDQUEUELOWAT: LWIP_MAX(TCP_SND_QUEUELEN)/2, 5) -*/
#define TCP_SNDQUEUELOWAT 5
/*----- Value in opt.h for TCP_WND_UPDATE_THRESHOLD: LWIP_MIN(TCP_WND/4, TCP_MSS*4) -----*/
#define TCP_WND_UPDATE_THRESHOLD 536
/*----- Default Value for LWIP_NETIF_HOSTNAME: 0 ---*/
#define LWIP_NETIF_HOSTNAME 1
/*----- Default Value for LWIP_NETIF_STATUS_CALLBACK: 0 ---*/
#define LWIP_NETIF_STATUS_CALLBACK 1
/*----- Value in opt.h for LWIP_NETIF_LINK_CALLBACK: 0 -----*/
#define LWIP_NETIF_LINK_CALLBACK 1
/*----- Value in opt.h for LWIP_NETCONN: 1 -----*/
#define LWIP_NETCONN 0
/*----- Value in opt.h for LWIP_SOCKET: 1 -----*/
#define LWIP_SOCKET 0
/*----- Value in opt.h for RECV_BUFSIZE_DEFAULT: INT_MAX -----*/
#define RECV_BUFSIZE_DEFAULT 2000000000
/*----- Value in opt.h for LWIP_STATS: 1 -----*/
#define LWIP_STATS 0
/*----- Value in opt.h for CHECKSUM_GEN_IP: 1 -----*/
#define CHECKSUM_GEN_IP 0
/*----- Value in opt.h for CHECKSUM_GEN_UDP: 1 -----*/
#define CHECKSUM_GEN_UDP 0
/*----- Value in opt.h for CHECKSUM_GEN_TCP: 1 -----*/
#define CHECKSUM_GEN_TCP 0
/*----- Value in opt.h for CHECKSUM_GEN_ICMP: 1 -----*/
#define CHECKSUM_GEN_ICMP 0
/*----- Value in opt.h for CHECKSUM_GEN_ICMP6: 1 -----*/
#define CHECKSUM_GEN_ICMP6 0
/*----- Value in opt.h for CHECKSUM_CHECK_IP: 1 -----*/
#define CHECKSUM_CHECK_IP 0
/*----- Value in opt.h for CHECKSUM_CHECK_UDP: 1 -----*/
#define CHECKSUM_CHECK_UDP 0
/*----- Value in opt.h for CHECKSUM_CHECK_TCP: 1 -----*/
#define CHECKSUM_CHECK_TCP 0
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP: 1 -----*/
#define CHECKSUM_CHECK_ICMP 0
/*----- Value in opt.h for CHECKSUM_CHECK_ICMP6: 1 -----*/
#define CHECKSUM_CHECK_ICMP6 0
/*-----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */
/*
 * OS-mode overrides.
 *
 * Everything above this point is exactly what CubeMX generates from
 * FirmwareUpdate.ioc, so a regeneration produces no diff there. Everything the
 * .ioc cannot express is here instead, because CubeMX preserves USER CODE
 * sections across regeneration -- which is the only reason this file survives.
 *
 * Why the .ioc cannot express it: CubeMX gates WITH_RTOS on
 *     !NO_SYS & !(DIE451 & (FREERTOS$context:FreeRTOS_API = 1))
 * (db/mcu/IP/LWIP-v2.1.2_Cube_Modes.xml). DIE451 is the STM32F76x/F77x die and
 * FreeRTOS_API=1 is the CMSIS-RTOS v2 interface, so on this exact part with
 * CMSIS v2 the option is unavailable and CubeMX always emits NO_SYS 1. The
 * only configuration it would accept is CMSIS v1, which ILT_OS/Lib/Thread.h is
 * not written against and which ARM has deprecated.
 *
 * Verified by regenerating from the .ioc and diffing, not assumed.
 */

/* --- threading ------------------------------------------------------------ */
/* lwIP owns a tcpip_thread; sys_arch.c on CMSIS-RTOS v2 provides the sys_*
   layer. Note ethernetif.c must match: see the note in that file. */
#undef NO_SYS
#define NO_SYS 0
#undef WITH_RTOS
#define WITH_RTOS 1
#undef SYS_LIGHTWEIGHT_PROT
#define SYS_LIGHTWEIGHT_PROT 1

/* sys_thread_new() passes stacksize straight to osThreadAttr_t.stack_size,
   which CMSIS-RTOS v2 measures in BYTES -- not the FreeRTOS words ST's older
   examples pass here. A word count would give tcpip_thread a quarter of the
   stack it needs and fault the first time httpd formats a response. */
#define TCPIP_THREAD_NAME "tcpip"
#define TCPIP_THREAD_STACKSIZE 4096
#define TCPIP_THREAD_PRIO osPriorityHigh
#define DEFAULT_THREAD_STACKSIZE 2048
#define DEFAULT_THREAD_PRIO osPriorityNormal

/* Message counts, not bytes. */
#define TCPIP_MBOX_SIZE 8
#define DEFAULT_UDP_RECVMBOX_SIZE 8
#define DEFAULT_TCP_RECVMBOX_SIZE 8
#define DEFAULT_ACCEPTMBOX_SIZE 8

/* --- memory --------------------------------------------------------------- */
/* CubeMX emits LWIP_RAM_HEAP_POINTER 0x20048000 for every F7 (its own template
   says the parameter is meant for the H7 only). A fixed address in the middle
   of SRAM1 collides with .bss as the firmware grows, and the collision is
   silent. Undefining it puts the heap in .bss, which the linker script keeps
   in SRAM1 where the Ethernet DMA can reach it. */
#undef LWIP_RAM_HEAP_POINTER

#define MEM_SIZE (16 * 1024)
#define MEMP_NUM_PBUF 16
#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_PCB_LISTEN 4
#define MEMP_NUM_TCP_SEG 24
#define PBUF_POOL_SIZE 12

/* lwIP's own timers plus the two NetworkStack keeps running: the 100 ms PHY
   poll and the DHCP fallback deadline. Running out is a silent runtime failure
   rather than a build error, so this is deliberately generous. */
#undef MEMP_NUM_SYS_TIMEOUT
#define MEMP_NUM_SYS_TIMEOUT 16

/* --- TCP ------------------------------------------------------------------ */
/* A full-size MSS is worth having on Ethernet. The four derived constants
   CubeMX emits were computed for the default 536-byte MSS, so they are undefined
   here and left for opt.h to recompute from these values. */
#define TCP_MSS 1460
#define TCP_SND_BUF (4 * TCP_MSS)
#define TCP_WND (4 * TCP_MSS)
#undef TCP_SND_QUEUELEN
#undef TCP_SNDLOWAT
#undef TCP_SNDQUEUELOWAT
#undef TCP_WND_UPDATE_THRESHOLD

/* --- APIs ----------------------------------------------------------------- */
/* httpd uses the raw API, so netconn is not needed for the web server. It is
   on because it is the API any later service would be written against, and
   api_lib.c is compiled either way. */
#undef LWIP_NETCONN
#define LWIP_NETCONN 1

/* --- HTTP server ---------------------------------------------------------- */
/* Content comes from ILT_OS/Lib/Net/HttpServer.cpp, which implements the
   fs_open/fs_close/fs_bytes_left API in lwip/apps/fs.h over a C++ route table.
   lwIP's own fs.c and the makefsdata-generated fsdata.c are therefore NOT
   compiled -- see Middlewares/CMakeLists.txt. */

/* Let httpd emit the status line and headers itself, choosing Content-Type
   from the extension of the requested path. Without this every route would
   have to include its own raw HTTP header bytes. */
#define LWIP_HTTPD_DYNAMIC_HEADERS 1

/* The whole body is handed over at fs_open() time, so httpd never calls
   fs_read() and our backend does not implement it. */
#define LWIP_HTTPD_DYNAMIC_FILE_READ 0
#define LWIP_HTTPD_FS_ASYNC_READ 0
#define LWIP_HTTPD_CUSTOM_FILES 0

/* SSI and CGI are lwIP's mechanisms for dynamic content; we use a render
   callback per route instead, which needs neither. */
#define LWIP_HTTPD_SSI 0
#define LWIP_HTTPD_CGI 0

/* The one hook we do use. httpd cuts the URI at '?' before calling fs_open(),
   so a route can only see its query parameters through httpd_cgi_handler(),
   which httpd calls afterwards but before it reads the file length. */
#define LWIP_HTTPD_CGI_SSI 1
#define HTTPD_USE_CUSTOM_FSDATA 0

/* Make httpd COPY what it sends, instead of referencing it.
 *
 * Upstream defaults both of these to "reference, do not copy", which is right
 * for its own fsdata: a const array in flash that never changes and never goes
 * away. Neither assumption holds here, and each one breaks something:
 *
 *   Correctness. Our dynamic routes render into a pooled RAM buffer that
 *   fs_close() hands straight back for the next request. Sending it by
 *   reference leaves lwIP transmitting out of a buffer we have already
 *   recycled -- a use-after-free that would show up as one client seeing
 *   another's response.
 *
 *   Memory. A referenced write allocates a PBUF_ROM from MEMP_PBUF and holds
 *   it until the data is ACKed. That pool is 16 entries; a handful of
 *   responses exhausts it and every later tcp_write returns ERR_MEM, which on
 *   hardware looked like httpd accepting connections and never answering.
 *
 * Copying puts the bytes in a PBUF_RAM from the lwIP heap, which is sized for
 * it, and lets fs_close() reclaim the render buffer immediately. */
#define HTTP_IS_HDR_VOLATILE(hs, ptr) TCP_WRITE_FLAG_COPY
#define HTTP_IS_DATA_VOLATILE(hs)     TCP_WRITE_FLAG_COPY

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /*__LWIPOPTS__H__ */
