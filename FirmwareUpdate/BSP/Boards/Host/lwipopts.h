/**
  ******************************************************************************
  * @file    lwipopts.h
  * @brief   lwIP configuration for the Host (POSIX/TAP) build.
  *
  * Same OS-mode stack as the boards, with two differences that are not
  * cosmetic:
  *
  *  1. CHECKSUMS ARE COMPUTED IN SOFTWARE. On the F767 they are switched off
  *     because the Ethernet MAC inserts and verifies them. A TAP interface has
  *     no MAC; leaving them off here would put packets on the wire with zero
  *     checksums and the host kernel would silently drop every one.
  *
  *  2. THREAD STACKS ARE IN A DIFFERENT RANGE. The FreeRTOS POSIX port hands
  *     each stack to pthread_attr_setstack(), so anything under glibc's
  *     PTHREAD_STACK_MIN aborts at thread creation. tcpip_thread and tapif's
  *     own RX thread are both created through sys_thread_new() with the sizes
  *     below, so both must clear that floor.
  ******************************************************************************
  */

#ifndef __LWIPOPTS__H__
#define __LWIPOPTS__H__

#ifdef __cplusplus
extern "C" {
#endif

/* --- threading ------------------------------------------------------------ */
#define WITH_RTOS                       1
#define NO_SYS                          0
#define SYS_LIGHTWEIGHT_PROT            1

/* Bytes, and generous: see note 2 above. */
#define TCPIP_THREAD_NAME               "tcpip"
#define TCPIP_THREAD_STACKSIZE          (256 * 1024)
#define TCPIP_THREAD_PRIO               osPriorityHigh
#define DEFAULT_THREAD_STACKSIZE        (256 * 1024)
#define DEFAULT_THREAD_PRIO             osPriorityNormal

#define TCPIP_MBOX_SIZE                 16
#define DEFAULT_UDP_RECVMBOX_SIZE       16
#define DEFAULT_TCP_RECVMBOX_SIZE       16
#define DEFAULT_ACCEPTMBOX_SIZE         16

/* --- memory --------------------------------------------------------------- */
/* 8 on x86-64: pointers in the heap must land on their natural alignment. */
#define MEM_ALIGNMENT                   8
#define MEM_SIZE                        (64 * 1024)
#define MEMP_NUM_PBUF                   32
#define MEMP_NUM_TCP_PCB                16
#define MEMP_NUM_TCP_PCB_LISTEN         8
#define MEMP_NUM_TCP_SEG                64
#define MEMP_NUM_SYS_TIMEOUT            16
#define PBUF_POOL_SIZE                  32

/* --- protocols ------------------------------------------------------------ */
#define LWIP_ETHERNET                   1
#define LWIP_ARP                        1
#define LWIP_IPV4                       1
#define LWIP_ICMP                       1
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_DHCP                       1
#define LWIP_DNS                        1

#define TCP_MSS                         1460
#define TCP_SND_BUF                     (8 * TCP_MSS)
#define TCP_WND                         (8 * TCP_MSS)

/* --- netif callbacks ------------------------------------------------------ */
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1

/* --- APIs ----------------------------------------------------------------- */
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     0

/* --- HTTP server ---------------------------------------------------------- */
/* Identical to the boards: the same HttpServer.cpp backs fs_open here. */
#define LWIP_HTTPD_DYNAMIC_HEADERS      1
#define LWIP_HTTPD_DYNAMIC_FILE_READ    0
#define LWIP_HTTPD_FS_ASYNC_READ        0
#define LWIP_HTTPD_CUSTOM_FILES         0
#define LWIP_HTTPD_SSI                  0
#define LWIP_HTTPD_CGI                  0

/* The one hook we do use. httpd cuts the URI at '?' before calling fs_open(),
   so a route can only see its query parameters through httpd_cgi_handler(),
   which httpd calls afterwards but before it reads the file length. */
#define LWIP_HTTPD_CGI_SSI              1
#define HTTPD_USE_CUSTOM_FSDATA         0

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

/* --- checksums ------------------------------------------------------------ */
/* All on: there is no MAC to offload to. See note 1 above. */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_CHECK_ICMP             1

/* --- diagnostics ---------------------------------------------------------- */
#define LWIP_STATS                      1
#define LWIP_STATS_DISPLAY              1
#define LWIP_DEBUG                      0

#ifdef __cplusplus
}
#endif
#endif /* __LWIPOPTS__H__ */
