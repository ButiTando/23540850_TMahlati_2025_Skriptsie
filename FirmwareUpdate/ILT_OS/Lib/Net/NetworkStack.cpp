/**
  ******************************************************************************
  * @file    NetworkStack.cpp
  * @brief   Interface bring-up and address management for lwIP in OS mode.
  *
  * Threading rule for everything below: lwIP is not thread safe, and in OS mode
  * the only thread allowed to touch it is tcpip_thread. Every function here is
  * therefore in one of two groups, and each is labelled:
  *
  *   [tcpip] runs in tcpip_thread -- may call lwIP freely.
  *   [any]   runs in a caller's thread -- may only read the published scalars,
  *           or hand work to tcpip_thread.
  ******************************************************************************
  */

#include "Net/NetworkStack.h"

#include "Bsp/Ethernet.h"

#include "Semaphore.h"

#include "lwip/dhcp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"
#include "lwip/timeouts.h"

#include <cstring>

namespace ilt::net {
namespace {

/* How often to ask the PHY whether the cable is still there. 100 ms is what
   CubeMX's generated bare-metal loop used, and it is fast enough that a plug
   event is noticed before a person gets impatient. */
constexpr std::uint32_t kLinkPollMs = 100U;

/* Bound on how long start() waits for tcpip_thread to finish adding the netif.
   Only exceeded if the RTOS could not create the thread at all. */
constexpr std::uint32_t kInitTimeoutMs = 5000U;

/* --- state shared between tcpip_thread and everyone else ------------------ */
/* Written only by tcpip_thread, read by anybody. Each is a single naturally
   aligned word, so a reader sees either the old or the new value, never a torn
   one; there is no invariant spanning two of them that a reader could catch
   half-updated. */
volatile std::uint32_t g_addressNetworkOrder = 0U;
volatile std::uint8_t  g_state               = static_cast<std::uint8_t>(NetworkState::Stopped);
volatile bool          g_linkUp              = false;
volatile bool          g_usingFallback       = false;

/* Touched only by tcpip_thread after start() returns. */
struct netif g_netif;
NetworkStack::Config g_config;
bool g_dhcpTimerArmed = false;

/* Set by start() before tcpip_init(), read by everyone afterwards. */
bool g_started = false;

void setState(NetworkState state) noexcept
{
    g_state = static_cast<std::uint8_t>(state);
}

/** [tcpip] Recompute the published state from what lwIP currently believes. */
void publishState() noexcept
{
    g_addressNetworkOrder = netif_ip4_addr(&g_netif)->addr;

    if (g_addressNetworkOrder != 0U)
    {
        setState(NetworkState::Ready);
    }
    else if (g_linkUp)
    {
        setState(NetworkState::Configuring);
    }
    else
    {
        setState(NetworkState::LinkDown);
    }
}

/** [tcpip] Apply the Config fallback address to the interface. */
void applyFallbackAddress() noexcept
{
    ip4_addr_t address;
    ip4_addr_t netmask;
    ip4_addr_t gateway;

    IP4_ADDR(&address, g_config.address[0], g_config.address[1],
             g_config.address[2], g_config.address[3]);
    IP4_ADDR(&netmask, g_config.netmask[0], g_config.netmask[1],
             g_config.netmask[2], g_config.netmask[3]);
    IP4_ADDR(&gateway, g_config.gateway[0], g_config.gateway[1],
             g_config.gateway[2], g_config.gateway[3]);

    netif_set_addr(&g_netif, &address, &netmask, &gateway);
    g_usingFallback = true;
    publishState();
}

/** [tcpip] DHCP did not answer in time; take the static address instead. */
void onDhcpTimeout(void *) noexcept
{
    g_dhcpTimerArmed = false;

    /* A lease may have landed between the timer firing and this running. */
    if (netif_ip4_addr(&g_netif)->addr != 0U)
    {
        return;
    }

    dhcp_stop(&g_netif);
    applyFallbackAddress();
}

/** [tcpip] lwIP tells us the address or the up/down flag changed. */
void onStatusChanged(struct netif *) noexcept
{
    publishState();
}

/** [tcpip] lwIP tells us the PHY link came or went. */
void onLinkChanged(struct netif *netif) noexcept
{
    g_linkUp = netif_is_link_up(netif) != 0;

    if (g_linkUp && g_config.useDhcp && g_config.dhcpTimeoutMs != 0U &&
        !g_dhcpTimerArmed && netif_ip4_addr(netif)->addr == 0U)
    {
        /* Start counting only once there is a cable: arming this at bring-up
           would burn the whole timeout against an unplugged port and drop us
           onto the fallback address before DHCP ever got a chance. */
        sys_timeout(g_config.dhcpTimeoutMs, onDhcpTimeout, nullptr);
        g_dhcpTimerArmed = true;
    }

    publishState();
}

/** [tcpip] Periodic PHY poll, re-armed on every expiry. */
void onLinkPoll(void *) noexcept
{
    bsp::eth::pollLink(&g_netif);
    sys_timeout(kLinkPollMs, onLinkPoll, nullptr);
}

/**
 * [tcpip] Runs once, inside tcpip_thread, as soon as the thread is up.
 *
 * Everything that touches lwIP state at bring-up happens here rather than in
 * start(), which is what makes start() callable from an ordinary thread.
 */
void onTcpipReady(void *arg) noexcept
{
    ip4_addr_t any;
    ip4_addr_set_zero(&any);

    /* Added with a zero address even in the static case: netif_add publishes
       the interface before DHCP has run, and applyFallbackAddress() below sets
       the real one through the same path a lease would take, so there is one
       code path that changes the address rather than two. */
    netif_add(&g_netif, &any, &any, &any, nullptr, bsp::eth::netifInit(),
              &tcpip_input);
    netif_set_default(&g_netif);

#if LWIP_NETIF_HOSTNAME
    netif_set_hostname(&g_netif, g_config.hostname);
#endif

    netif_set_status_callback(&g_netif, onStatusChanged);
    netif_set_link_callback(&g_netif, onLinkChanged);

    netif_set_up(&g_netif);

    /* Seed the link state; the PHY was already interrogated by netifInit(). */
    g_linkUp = netif_is_link_up(&g_netif) != 0;

    if (g_config.useDhcp)
    {
        dhcp_start(&g_netif);

        if (g_linkUp && g_config.dhcpTimeoutMs != 0U)
        {
            sys_timeout(g_config.dhcpTimeoutMs, onDhcpTimeout, nullptr);
            g_dhcpTimerArmed = true;
        }
    }
    else
    {
        applyFallbackAddress();
    }

    sys_timeout(kLinkPollMs, onLinkPoll, nullptr);
    publishState();

    static_cast<BinarySemaphore *>(arg)->release();
}

} // namespace

NetworkStack &NetworkStack::instance() noexcept
{
    static NetworkStack stack;
    return stack;
}

bool NetworkStack::start(const Config &config) noexcept
{
    if (g_started)
    {
        return false;
    }

    if (!bsp::eth::available() || bsp::eth::netifInit() == nullptr)
    {
        return false;
    }

    g_config = config;

    /* tcpip_init() returns as soon as the thread is created, not when it has
       run, so we wait for the callback to signal that the netif exists. Without
       this, a caller could register HTTP routes and start httpd against a
       half-built interface. */
    BinarySemaphore ready;
    if (!ready.isValid())
    {
        return false;
    }

    tcpip_init(onTcpipReady, &ready);

    const bool ok = ready.acquire(kInitTimeoutMs);

    g_started = ok;
    return ok;
}

bool NetworkStack::isStarted() const noexcept
{
    return g_started;
}

NetworkState NetworkStack::state() const noexcept
{
    return static_cast<NetworkState>(g_state);
}

bool NetworkStack::isLinkUp() const noexcept
{
    return g_linkUp;
}

bool NetworkStack::usingFallbackAddress() const noexcept
{
    return g_usingFallback;
}

std::uint32_t NetworkStack::address() const noexcept
{
    return lwip_ntohl(g_addressNetworkOrder);
}

const char *NetworkStack::formatAddress(char *buffer, std::size_t capacity) const noexcept
{
    if (buffer == nullptr || capacity == 0U)
    {
        return "";
    }

    ip4_addr_t address;
    address.addr = g_addressNetworkOrder;

    if (ip4addr_ntoa_r(&address, buffer, static_cast<int>(capacity)) == nullptr)
    {
        /* Only happens if capacity is under 16; leave something printable. */
        buffer[0] = '\0';
    }

    return buffer;
}

} // namespace ilt::net
