/**
  ******************************************************************************
  * @file    StreamServer.h
  * @brief   A TCP port that clients connect to and stay connected to, receiving
  *          every record the firmware produces.
  *
  * The push half of the interface, next to HttpServer's request/response half.
  * A control station opens one socket, leaves it open, and reads newline-
  * delimited records as they happen -- no polling, no reconnect per sample.
  *
  *     nc 192.168.1.200 5000
  *     {"t":1234,"ch":[10,0,3,0,0,0]}
  *     {"t":2234,"ch":[12,0,4,0,0,0]}
  *
  * Any thread may call broadcast(). lwIP itself is only ever touched from
  * tcpip_thread: broadcast() copies the record into a small pool and hands it
  * over, so the calling thread never blocks on the network and a stalled client
  * cannot slow the measurement that produced the data.
  *
  * The pool is finite. When it is full, or when a client's send buffer is, the
  * record is DROPPED and counted rather than queued indefinitely -- on a rig
  * producing data on a fixed period, unbounded buffering would only convert a
  * slow reader into an out-of-memory failure later. droppedRecords() is the
  * signal that a consumer is not keeping up.
  ******************************************************************************
  */

#ifndef ILT_OS_LIB_NET_STREAMSERVER_H
#define ILT_OS_LIB_NET_STREAMSERVER_H

#include <cstddef>
#include <cstdint>

namespace ilt::net {

/**
 * @brief A broadcast TCP listener.
 *
 * A singleton for the same reason HttpServer is: it owns a bound port, and a
 * second instance on the same port could not be created anyway.
 */
class StreamServer
{
public:
    static StreamServer &instance() noexcept;

    StreamServer(const StreamServer &)            = delete;
    StreamServer &operator=(const StreamServer &) = delete;

    /**
     * @brief Bind @p port and begin accepting clients.
     *
     * Call after NetworkStack::start(). Safe from any thread: the lwIP work is
     * marshalled onto tcpip_thread and this returns once it has run.
     */
    bool start(std::uint16_t port) noexcept;

    bool isRunning() const noexcept;

    /**
     * @brief Send one record to every connected client.
     *
     * A newline is appended if @p record does not end with one, so consumers
     * can read line by line.
     *
     * @return false if the record was dropped, i.e. no buffer was free. Not an
     *         error to handle so much as a fact to count; see droppedRecords().
     */
    bool broadcast(const char *record, std::size_t length) noexcept;

    /** @brief As above, for a NUL-terminated string. */
    bool broadcast(const char *record) noexcept;

    /** @brief Clients currently connected. */
    std::size_t clientCount() const noexcept;

    /** @brief Records handed to at least one client since boot. */
    std::uint32_t sentRecords() const noexcept;

    /** @brief Records dropped because no buffer or no send space was free. */
    std::uint32_t droppedRecords() const noexcept;

    /** @brief Longest record the pool can carry, ILT_STREAM_RECORD_BYTES. */
    static std::size_t recordCapacity() noexcept;

    /** @brief Simultaneous clients supported, ILT_STREAM_MAX_CLIENTS. */
    static std::size_t clientCapacity() noexcept;

private:
    StreamServer() noexcept = default;
};

} // namespace ilt::net

#endif /* ILT_OS_LIB_NET_STREAMSERVER_H */
