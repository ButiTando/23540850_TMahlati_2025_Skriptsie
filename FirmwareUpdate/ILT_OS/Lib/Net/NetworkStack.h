/**
  ******************************************************************************
  * @file    NetworkStack.h
  * @brief   Brings the board's network interface up and keeps it up.
  *
  * lwIP is built in OS mode (NO_SYS=0), so the stack runs in its own
  * tcpip_thread and every lwIP call must be made from that thread. This class
  * is the seam: it does all of its lwIP work inside tcpip_thread callbacks and
  * publishes the result as plain scalars that any thread may read.
  *
  *     auto &net = ilt::net::NetworkStack::instance();
  *     net.start(ilt::net::NetworkStack::Config{});   // DHCP, static fallback
  *     while (!net.isReady()) { ilt::Thread::sleep(100); }
  *
  *     char ip[16];
  *     net.formatAddress(ip, sizeof(ip));
  *
  * Which MAC and PHY are involved is the board's business -- this class asks
  * BSP/Include/Bsp/Ethernet.h for the netif init callback and nothing more, so
  * it is unchanged when the H723 board is added.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_NET_NETWORKSTACK_H
#define ILT_OS_LIB_NET_NETWORKSTACK_H

#include <cstddef>
#include <cstdint>

namespace ilt::net {

/** @brief How far the interface has got. */
enum class NetworkState : std::uint8_t
{
    Stopped,     /**< start() has not run, or it failed. */
    LinkDown,    /**< No cable, or the peer is not talking. */
    Configuring, /**< Link is up; waiting for DHCP to answer. */
    Ready        /**< An address is assigned and the stack is usable. */
};

/**
 * @brief The board's single network interface.
 *
 * A singleton because lwIP's netif and tcpip_thread are themselves process-wide
 * and there is exactly one MAC on these boards. Constructed before main() with
 * no side effects; nothing touches hardware until start().
 */
class NetworkStack
{
public:
    /** @brief Addressing policy. Defaults are DHCP with a static fallback. */
    struct Config
    {
        /** DHCP client identifier; also what a router shows in its lease list. */
        const char *hostname = "ilt-degrader";

        /** Try DHCP first. When false the static address below is used at once. */
        bool useDhcp = true;

        /**
         * How long to wait for a DHCP lease before falling back to the static
         * address. Zero waits forever, which is rarely what you want on a
         * bench: with no DHCP server the board would simply never answer.
         */
        std::uint32_t dhcpTimeoutMs = 15000U;

        /** Fallback address, used when DHCP is off or does not answer in time. */
        std::uint8_t address[4] = {192, 168, 1, 200};
        std::uint8_t netmask[4] = {255, 255, 255, 0};
        std::uint8_t gateway[4] = {192, 168, 1, 1};
    };

    static NetworkStack &instance() noexcept;

    NetworkStack(const NetworkStack &)            = delete;
    NetworkStack &operator=(const NetworkStack &) = delete;

    /**
     * @brief Start tcpip_thread, add the interface and begin addressing.
     *
     * Call once, from an application thread, after osKernelStart(). Returns
     * when the interface exists and DHCP (if any) has been kicked off -- not
     * when an address has arrived; poll isReady() for that.
     *
     * @return false if the board has no Ethernet, or if start() already ran.
     */
    bool start(const Config &config) noexcept;

    /** @brief Whether start() has completed successfully. */
    bool isStarted() const noexcept;

    /** @brief Current state. Safe to call from any thread. */
    NetworkState state() const noexcept;

    /** @brief Shorthand for state() == NetworkState::Ready. */
    bool isReady() const noexcept { return state() == NetworkState::Ready; }

    /** @brief Whether the PHY reports a live link. */
    bool isLinkUp() const noexcept;

    /**
     * @brief Whether the address in use came from the Config fallback rather
     *        than from a DHCP server.
     */
    bool usingFallbackAddress() const noexcept;

    /** @brief The current IPv4 address in host byte order, or 0 if none. */
    std::uint32_t address() const noexcept;

    /**
     * @brief Render the current address as a dotted quad into @p buffer.
     *
     * Takes a caller-supplied buffer rather than returning a pointer to shared
     * storage, so two threads asking at once cannot see a half-written string.
     *
     * @param buffer   Destination; 16 bytes is always enough.
     * @param capacity sizeof(buffer).
     * @return @p buffer, always NUL-terminated ("0.0.0.0" when unaddressed).
     */
    const char *formatAddress(char *buffer, std::size_t capacity) const noexcept;

private:
    NetworkStack() noexcept = default;
};

} // namespace ilt::net

#endif /* ILT_OS_LIB_NET_NETWORKSTACK_H */
