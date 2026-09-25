/**
  ******************************************************************************
  * @file    WebServerApp.cpp
  * @brief   Serves a status page over Ethernet from a single application thread.
  *
  * One thread does the whole job: it brings the interface up, registers the
  * routes, starts lwIP's httpd and then supervises -- reporting address and
  * link changes on the console and driving the board LEDs. Serving itself
  * happens in lwIP's tcpip_thread, which is where httpd and the render
  * callbacks below run.
  *
  * There is nothing board-specific here. The LEDs are named by role, the
  * console is whatever the board calls a console, and the MAC and PHY are
  * reached only through NetworkStack.
  ******************************************************************************
  */

#include "Application.h"
#include "Kernel.h"
#include "Thread.h"

#include "Bsp/Board.h"
#include "Bsp/Console.h"
#include "Bsp/Led.h"

#include "Net/HttpServer.h"
#include "Net/NetworkStack.h"

#include "IndexPage.h"

#include <cstdio>
#include <cstring>

/* Defaults, so the translation unit still compiles if built without CMake. */
#ifndef WEBSERVER_STACK_BYTES
#define WEBSERVER_STACK_BYTES 1536
#endif
#ifndef WEBSERVER_HOSTNAME
#define WEBSERVER_HOSTNAME "ilt-degrader"
#endif
#ifndef WEBSERVER_USE_DHCP
#define WEBSERVER_USE_DHCP 1
#endif
#ifndef WEBSERVER_DHCP_TIMEOUT_MS
#define WEBSERVER_DHCP_TIMEOUT_MS 15000
#endif
#ifndef WEBSERVER_FALLBACK_IP
#define WEBSERVER_FALLBACK_IP 192, 168, 1, 200
#endif
#ifndef WEBSERVER_FALLBACK_NETMASK
#define WEBSERVER_FALLBACK_NETMASK 255, 255, 255, 0
#endif
#ifndef WEBSERVER_FALLBACK_GATEWAY
#define WEBSERVER_FALLBACK_GATEWAY 192, 168, 1, 1
#endif

namespace {

using ilt::net::HttpServer;
using ilt::net::NetworkStack;
using ilt::net::NetworkState;

/* How often the supervisor loop wakes to blink and re-check state. Also the
   heartbeat half-period, so the Status LED blinks at 2 Hz. */
constexpr std::uint32_t kSuperviseIntervalMs = 250U;

/**
 * @brief Render /status.json.
 *
 * Runs in tcpip_thread (see ilt::net::HttpRenderFn), so it only reads the
 * scalars NetworkStack and HttpServer publish and formats them. No blocking,
 * no lwIP calls, no waiting on another thread.
 */
std::size_t renderStatus(char *buffer, std::size_t capacity,
                         const ilt::net::HttpQuery &) noexcept
{
    const auto &net  = NetworkStack::instance();
    const auto &http = HttpServer::instance();

    char address[16];
    net.formatAddress(address, sizeof(address));

    const int written = std::snprintf(
        buffer, capacity,
        "{\"board\":\"%s\",\"ip\":\"%s\",\"uptime_s\":%lu,"
        "\"requests\":%lu,\"link\":%s,\"dhcp\":%s}",
        bsp::boardName(), address,
        static_cast<unsigned long>(ilt::kernel::tickCount() / 1000U),
        static_cast<unsigned long>(http.requestsServed()),
        net.isLinkUp() ? "true" : "false",
        net.usingFallbackAddress() ? "false" : "true");

    /* snprintf returns what it *would* have written; a truncated body would be
       invalid JSON, so answer 404 rather than serving half an object. */
    if (written < 0 || static_cast<std::size_t>(written) >= capacity)
    {
        return 0U;
    }

    return static_cast<std::size_t>(written);
}

class WebServerThread : public ilt::StaticThread<WEBSERVER_STACK_BYTES>
{
public:
    WebServerThread() noexcept
        : StaticThread("web", osPriorityNormal)
    {
    }

protected:
    void run() override
    {
        bsp::ledAllOff();

        bsp::consoleWriteLine("");
        bsp::consoleWrite("ILT firmware on ");
        bsp::consoleWriteLine(bsp::boardName());

        if (!bringUp())
        {
            /* Nothing left to serve with. Hold the fault LED on and stop --
               returning is safe, ilt::Thread deletes the task cleanly. */
            bsp::ledSet(bsp::Led::Fault, true);
            return;
        }

        supervise();
    }

private:
    /** @return true once httpd is listening. */
    bool bringUp() noexcept
    {
        NetworkStack::Config config;
        config.hostname       = WEBSERVER_HOSTNAME;
        config.useDhcp        = (WEBSERVER_USE_DHCP != 0);
        config.dhcpTimeoutMs  = WEBSERVER_DHCP_TIMEOUT_MS;
        const std::uint8_t address[4] = {WEBSERVER_FALLBACK_IP};
        const std::uint8_t netmask[4] = {WEBSERVER_FALLBACK_NETMASK};
        const std::uint8_t gateway[4] = {WEBSERVER_FALLBACK_GATEWAY};
        std::memcpy(config.address, address, sizeof(config.address));
        std::memcpy(config.netmask, netmask, sizeof(config.netmask));
        std::memcpy(config.gateway, gateway, sizeof(config.gateway));

        if (!NetworkStack::instance().start(config))
        {
            bsp::consoleWriteLine("net: interface did not start");
            return false;
        }

        /* Registered before start() so a client that connects the instant the
           listener opens cannot be told 404. */
        auto &http = HttpServer::instance();
        const bool routed = http.addPage("/index.html", app::kIndexHtml) &&
                            http.addPage("/status.json", &renderStatus);
        if (!routed)
        {
            bsp::consoleWriteLine("http: route table full");
            return false;
        }

        if (!http.start())
        {
            bsp::consoleWriteLine("http: listener did not start");
            return false;
        }

        bsp::consoleWriteLine("http: listening on port 80");
        return true;
    }

    /** Blink, and report state transitions on the console as they happen. */
    void supervise() noexcept
    {
        const auto &net = NetworkStack::instance();

        auto lastState = NetworkState::Stopped;
        std::uint32_t lastRequests = 0U;

        for (;;)
        {
            const NetworkState state = net.state();

            if (state != lastState)
            {
                reportState(state);
                lastState = state;
            }

            /* Status: slow blink while configuring, solid once addressed, so
               the board's state is readable without a serial terminal. */
            if (state == NetworkState::Ready)
            {
                bsp::ledSet(bsp::Led::Status, true);
            }
            else
            {
                bsp::ledToggle(bsp::Led::Status);
            }

            /* Activity: on for one interval whenever the count moved. */
            const std::uint32_t requests = HttpServer::instance().requestsServed();
            bsp::ledSet(bsp::Led::Activity, requests != lastRequests);
            lastRequests = requests;

            sleep(kSuperviseIntervalMs);
        }
    }

    void reportState(NetworkState state) noexcept
    {
        const auto &net = NetworkStack::instance();

        switch (state)
        {
        case NetworkState::LinkDown:
            bsp::consoleWriteLine("net: link down");
            break;

        case NetworkState::Configuring:
            bsp::consoleWriteLine("net: link up, requesting an address");
            break;

        case NetworkState::Ready:
        {
            char address[16];
            net.formatAddress(address, sizeof(address));

            bsp::consoleWrite("net: ready -- http://");
            bsp::consoleWrite(address);
            bsp::consoleWriteLine(net.usingFallbackAddress()
                                      ? "/  (static fallback, DHCP did not answer)"
                                      : "/  (DHCP)");
            break;
        }

        case NetworkState::Stopped:
        default:
            bsp::consoleWriteLine("net: stopped");
            break;
        }
    }
};

/* Constructed by __libc_init_array before main(); only the stack and control
   block are reserved at this point, no RTOS call happens yet. */
WebServerThread g_webServer;

} // namespace

extern "C" void ILT_ApplicationStart(void)
{
    bsp::boardInit();
    g_webServer.start();
}
