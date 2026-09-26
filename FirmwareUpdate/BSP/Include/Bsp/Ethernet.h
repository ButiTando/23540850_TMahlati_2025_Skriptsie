/**
  ******************************************************************************
  * @file    Bsp/Ethernet.h
  * @brief   The board's Ethernet interface, expressed as lwIP wants it.
  *
  * The MAC, the PHY and the pin muxing are board business; everything above
  * this header only needs the netif init callback to hand to netif_add() and a
  * way to ask whether the board has Ethernet at all.
  *
  * This is the one BSP header that speaks lwIP types. Making it board-neutral
  * in some stack-agnostic way would buy nothing: a netif *is* the portable
  * interface, and every board that has Ethernet drives it through one.
  ******************************************************************************
  */

#ifndef ILT_BSP_ETHERNET_H
#define ILT_BSP_ETHERNET_H

#include "lwip/netif.h"

#include <cstdint>

namespace bsp::eth {

/**
 * @brief Whether this board populates an Ethernet MAC and PHY.
 *
 * A board without Ethernet still links, and netifInit() returns nullptr, so
 * ilt::net::NetworkStack can fail cleanly instead of the image failing to
 * build.
 */
bool available() noexcept;

/**
 * @brief The lwIP init callback for this board's interface.
 * @return The callback to pass to netif_add(), or nullptr if !available().
 */
netif_init_fn netifInit() noexcept;

/**
 * @brief Re-read the PHY and reconcile the netif and MAC with what it says.
 *
 * Called periodically from lwIP's tcpip_thread. Brings the interface up when a
 * cable appears (configuring the MAC for the negotiated speed and duplex) and
 * down when it goes away, so portable code never touches an MDIO register.
 *
 * @param netif The interface returned by netifInit(); never null.
 */
void pollLink(struct netif *netif) noexcept;

/**
 * @brief The MAC address this board should use.
 *
 * Derived from the MCU's 96-bit unique device ID so two boards from the same
 * build can share a network segment without a hand-edited constant. The result
 * is stable across resets, locally administered and never multicast.
 *
 * @param out Receives the six address bytes, most significant first.
 */
void macAddress(std::uint8_t out[6]) noexcept;

} // namespace bsp::eth

#endif /* ILT_BSP_ETHERNET_H */
