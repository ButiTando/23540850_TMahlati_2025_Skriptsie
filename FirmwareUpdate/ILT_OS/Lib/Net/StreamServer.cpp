/**
  ******************************************************************************
  * @file    StreamServer.cpp
  * @brief   A broadcast TCP listener on lwIP's raw API.
  *
  * Threading rule, as in NetworkStack.cpp:
  *   [tcpip] runs in tcpip_thread -- may call lwIP freely.
  *   [any]   runs in a caller's thread -- may only publish scalars or hand work
  *           to tcpip_thread.
  ******************************************************************************
  */

#include "Net/StreamServer.h"

#include "Semaphore.h"

#include "lwip/err.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"

#include <cstring>

#ifndef ILT_STREAM_MAX_CLIENTS
#define ILT_STREAM_MAX_CLIENTS 4
#endif
#ifndef ILT_STREAM_RECORD_BYTES
#define ILT_STREAM_RECORD_BYTES 192
#endif
#ifndef ILT_STREAM_POOL_DEPTH
#define ILT_STREAM_POOL_DEPTH 4
#endif

namespace {

/* --- client table: touched only by tcpip_thread --------------------------- */
struct tcp_pcb *g_clients[ILT_STREAM_MAX_CLIENTS];
struct tcp_pcb *g_listener = nullptr;

/* --- record pool ---------------------------------------------------------- */
/* Producers claim a slot from an application thread and tcpip_thread releases
   it, so `busy` is the one field both touch. It is a single aligned word and
   is only ever moved between two values, so a reader never sees a torn state;
   the claim itself is made safe by the InterruptLock in claimSlot(). */
struct Record
{
    char          data[ILT_STREAM_RECORD_BYTES];
    std::uint16_t length;
    volatile bool busy;
};

Record g_pool[ILT_STREAM_POOL_DEPTH];

volatile bool          g_running  = false;
volatile std::uint32_t g_sent     = 0U;
volatile std::uint32_t g_dropped  = 0U;
volatile std::uint32_t g_clientCount = 0U;

std::uint16_t g_port = 0U;

/** [any] Take a free pool slot, or nullptr if every one is in flight. */
Record *claimSlot() noexcept
{
    /* Brief: just the search and the flag set. Long enough to matter only if
       the pool were large, which it is not. */
    ilt::InterruptLock lock;

    for (auto &record : g_pool)
    {
        if (!record.busy)
        {
            record.busy = true;
            return &record;
        }
    }

    return nullptr;
}

/** [tcpip] Drop a client and forget it. */
void removeClient(struct tcp_pcb *pcb, bool abort) noexcept
{
    for (auto &client : g_clients)
    {
        if (client == pcb)
        {
            client = nullptr;
            if (g_clientCount > 0U)
            {
                --g_clientCount;
            }
            break;
        }
    }

    tcp_arg(pcb, nullptr);
    tcp_recv(pcb, nullptr);
    tcp_err(pcb, nullptr);
    tcp_sent(pcb, nullptr);

    if (abort)
    {
        tcp_abort(pcb);
    }
    else if (tcp_close(pcb) != ERR_OK)
    {
        /* close() only fails for want of memory to send the FIN; abort frees
           the pcb unconditionally, which matters more than a clean shutdown
           when the alternative is leaking it. */
        tcp_abort(pcb);
    }
}

/** [tcpip] lwIP reports a connection died. The pcb is already freed. */
void onError(void *arg, err_t) noexcept
{
    auto *const pcb = static_cast<struct tcp_pcb *>(arg);

    for (auto &client : g_clients)
    {
        if (client == pcb)
        {
            client = nullptr;
            if (g_clientCount > 0U)
            {
                --g_clientCount;
            }
            break;
        }
    }
}

/** [tcpip] Data from a client. This is a one-way stream, so it is discarded. */
err_t onReceive(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) noexcept
{
    (void)arg;

    if (p == nullptr)
    {
        /* Remote closed. */
        removeClient(pcb, false);
        return ERR_OK;
    }

    if (err != ERR_OK)
    {
        pbuf_free(p);
        return err;
    }

    /* Nothing here reads client input, but the window still has to be reopened
       or the peer stalls after one buffer's worth. */
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

/** [tcpip] A new client. */
err_t onAccept(void *, struct tcp_pcb *pcb, err_t err) noexcept
{
    if (err != ERR_OK || pcb == nullptr)
    {
        return ERR_VAL;
    }

    for (auto &client : g_clients)
    {
        if (client == nullptr)
        {
            client = pcb;
            ++g_clientCount;

            tcp_arg(pcb, pcb);
            tcp_recv(pcb, onReceive);
            tcp_err(pcb, onError);

            /* Records are small and latency matters more than packing them, so
               do not sit on a partial segment waiting for more. */
            tcp_nagle_disable(pcb);
            return ERR_OK;
        }
    }

    /* Full. Refusing outright is clearer to the operator than accepting a
       connection that would silently receive nothing. */
    tcp_abort(pcb);
    return ERR_ABRT;
}

/** [tcpip] Write one record to every client, then release the slot. */
void deliver(void *arg) noexcept
{
    auto *const record = static_cast<Record *>(arg);
    bool delivered = false;

    for (auto &client : g_clients)
    {
        if (client == nullptr)
        {
            continue;
        }

        if (tcp_sndbuf(client) < record->length)
        {
            /* This client is behind. Skip the record for it rather than block
               the others or buffer without bound. */
            ++g_dropped;
            continue;
        }

        if (tcp_write(client, record->data, record->length, TCP_WRITE_FLAG_COPY) == ERR_OK)
        {
            tcp_output(client);
            delivered = true;
        }
        else
        {
            ++g_dropped;
        }
    }

    if (delivered)
    {
        ++g_sent;
    }

    record->busy = false;
}

/** [tcpip] Bind the listening pcb. */
void startListener(void *arg) noexcept
{
    struct tcp_pcb *pcb = tcp_new();

    if (pcb != nullptr && tcp_bind(pcb, IP_ANY_TYPE, g_port) == ERR_OK)
    {
        g_listener = tcp_listen_with_backlog(pcb, ILT_STREAM_MAX_CLIENTS);
        if (g_listener != nullptr)
        {
            tcp_accept(g_listener, onAccept);
            g_running = true;
        }
    }
    else if (pcb != nullptr)
    {
        tcp_close(pcb);
    }

    static_cast<ilt::BinarySemaphore *>(arg)->release();
}

} // namespace

namespace ilt::net {

StreamServer &StreamServer::instance() noexcept
{
    static StreamServer server;
    return server;
}

bool StreamServer::start(std::uint16_t port) noexcept
{
    if (g_running)
    {
        return false;
    }

    g_port = port;

    BinarySemaphore done;
    if (!done.isValid())
    {
        return false;
    }

    const bool posted = tcpip_callback(startListener, &done) == ERR_OK;
    return posted && done.acquire(5000U) && g_running;
}

bool StreamServer::isRunning() const noexcept
{
    return g_running;
}

bool StreamServer::broadcast(const char *record, std::size_t length) noexcept
{
    if (!g_running || record == nullptr || length == 0U)
    {
        return false;
    }

    /* Room for the newline this appends if the caller did not. */
    if (length > ILT_STREAM_RECORD_BYTES - 1U)
    {
        ++g_dropped;
        return false;
    }

    /* No clients: not a drop, just nobody listening. Skipping the copy keeps an
       unattended rig from doing work no one asked for. */
    if (g_clientCount == 0U)
    {
        return true;
    }

    Record *const slot = claimSlot();
    if (slot == nullptr)
    {
        ++g_dropped;
        return false;
    }

    std::memcpy(slot->data, record, length);
    if (slot->data[length - 1U] != '\n')
    {
        slot->data[length++] = '\n';
    }
    slot->length = static_cast<std::uint16_t>(length);

    if (tcpip_callback(deliver, slot) != ERR_OK)
    {
        slot->busy = false;
        ++g_dropped;
        return false;
    }

    return true;
}

bool StreamServer::broadcast(const char *record) noexcept
{
    return record != nullptr && broadcast(record, std::strlen(record));
}

std::size_t StreamServer::clientCount() const noexcept { return g_clientCount; }
std::uint32_t StreamServer::sentRecords() const noexcept { return g_sent; }
std::uint32_t StreamServer::droppedRecords() const noexcept { return g_dropped; }
std::size_t StreamServer::recordCapacity() noexcept { return ILT_STREAM_RECORD_BYTES; }
std::size_t StreamServer::clientCapacity() noexcept { return ILT_STREAM_MAX_CLIENTS; }

} // namespace ilt::net
