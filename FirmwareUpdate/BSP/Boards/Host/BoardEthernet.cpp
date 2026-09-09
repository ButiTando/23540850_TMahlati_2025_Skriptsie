/**
  ******************************************************************************
  * @file    BoardEthernet.cpp
  * @brief   Host implementation of Bsp/Ethernet.h, over a Linux TAP device.
  *
  * lwIP's own contrib tapif is the "PHY" here: it opens /dev/net/tun, and the
  * kernel presents the other end as a normal network interface. From the
  * firmware's side nothing changes -- netifInit() still returns a
  * netif_init_fn, so NetworkStack brings the interface up exactly as it does on
  * the Nucleo.
  ******************************************************************************
  */

#include "Bsp/Ethernet.h"

/* lwIP's contrib headers carry no extern "C" guard of their own. */
extern "C" {
#include "netif/tapif.h"
}

#include <cstdlib>
#include <unistd.h>

namespace bsp::eth {

bool available() noexcept { return true; }

netif_init_fn netifInit() noexcept
{
    return &tapif_init;
}

void pollLink(struct netif *) noexcept
{
    /* A TAP device has no PHY to interrogate and no cable to unplug: the link
       is up for as long as the process is running, and tapif_init() has already
       marked it so. Nothing to poll. */
}

void macAddress(std::uint8_t out[6]) noexcept
{
    /* Locally administered and unicast, as on the real boards. The low bytes
       come from the PID so two instances on one bridge do not collide. */
    const unsigned pid = static_cast<unsigned>(::getpid());

    out[0] = 0x02U;
    out[1] = 0x00U;
    out[2] = 0x00U;
    out[3] = 0x00U;
    out[4] = static_cast<std::uint8_t>((pid >> 8) & 0xFFU);
    out[5] = static_cast<std::uint8_t>(pid & 0xFFU);
}

} // namespace bsp::eth
