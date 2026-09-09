/**
  ******************************************************************************
  * @file    HttpServer.cpp
  * @brief   Route table for lwIP's httpd, plus the fs_* backend it reads through.
  *
  * Two halves:
  *   * the HttpServer class, called from application threads to register routes
  *     and to start the server;
  *   * the fs_open/fs_close/fs_bytes_left implementation at the bottom, called
  *     by httpd from lwIP's tcpip_thread.
  *
  * The route table is written only before start() and only read afterwards, so
  * the two halves never contend. The render-buffer pool is touched exclusively
  * from tcpip_thread, which is why it needs no lock of its own.
  ******************************************************************************
  */

#include "Net/HttpServer.h"

#include "Semaphore.h"

#include "lwip/apps/fs.h"
#include "lwip/apps/httpd.h"
#include "lwip/err.h"
#include "lwip/tcpip.h"

#include <cstdlib>
#include <cstring>

/* Build-time sizing. The CMake cache variables of the same names set these; the
   defaults keep the translation unit compilable on its own. */
#ifndef ILT_HTTP_MAX_ROUTES
#define ILT_HTTP_MAX_ROUTES 8
#endif
#ifndef ILT_HTTP_RENDER_BUFFERS
#define ILT_HTTP_RENDER_BUFFERS 2
#endif
#ifndef ILT_HTTP_RENDER_BYTES
#define ILT_HTTP_RENDER_BYTES 1024
#endif

namespace {

struct Route
{
    const char           *path;
    const char           *body;   /**< Static content, or null for dynamic. */
    std::size_t           length;
    ilt::net::HttpRenderFn render; /**< Dynamic content, or null for static. */
    /** The body already contains its own status line and headers. */
    bool                  headerIncluded;
};

Route       g_routes[ILT_HTTP_MAX_ROUTES];
std::size_t g_routeCount = 0U;

/* Pool of scratch buffers for dynamic routes. in-use flags and buffers are
   only ever touched from tcpip_thread (fs_open/fs_close), so no lock. */
char g_renderBuffers[ILT_HTTP_RENDER_BUFFERS][ILT_HTTP_RENDER_BYTES];
bool g_renderBufferBusy[ILT_HTTP_RENDER_BUFFERS];

bool g_running = false;

volatile std::uint32_t g_requestsServed   = 0U;
volatile std::uint32_t g_requestsRejected = 0U;

/**
 * @brief Compare a registered path against a request URI.
 *
 * The URI may carry a query string ("/status.json?t=3"); everything from the
 * '?' is ignored here, which is what makes a route reachable from a browser
 * that adds a cache-buster.
 */
bool pathMatches(const char *routePath, const char *uri) noexcept
{
    std::size_t i = 0U;
    for (; routePath[i] != '\0'; ++i)
    {
        if (uri[i] != routePath[i])
        {
            return false;
        }
    }

    return uri[i] == '\0' || uri[i] == '?';
}

const Route *findRoute(const char *uri) noexcept
{
    for (std::size_t i = 0U; i < g_routeCount; ++i)
    {
        if (pathMatches(g_routes[i].path, uri))
        {
            return &g_routes[i];
        }
    }

    return nullptr;
}

char *acquireRenderBuffer() noexcept
{
    for (std::size_t i = 0U; i < ILT_HTTP_RENDER_BUFFERS; ++i)
    {
        if (!g_renderBufferBusy[i])
        {
            g_renderBufferBusy[i] = true;
            return g_renderBuffers[i];
        }
    }

    return nullptr;
}

void releaseRenderBuffer(const char *buffer) noexcept
{
    for (std::size_t i = 0U; i < ILT_HTTP_RENDER_BUFFERS; ++i)
    {
        if (g_renderBuffers[i] == buffer)
        {
            g_renderBufferBusy[i] = false;
            return;
        }
    }
}

bool addRoute(const Route &route) noexcept
{
    if (route.path == nullptr || route.path[0] != '/')
    {
        return false;
    }

    if (g_routeCount >= ILT_HTTP_MAX_ROUTES)
    {
        return false;
    }

    g_routes[g_routeCount++] = route;
    return true;
}

/** Runs in tcpip_thread: bind port 80. */
void startHttpd(void *arg) noexcept
{
    httpd_init();
    g_running = true;
    static_cast<ilt::BinarySemaphore *>(arg)->release();
}

} // namespace

namespace ilt::net {

HttpServer &HttpServer::instance() noexcept
{
    static HttpServer server;
    return server;
}

bool HttpServer::addPage(const char *path, const char *body) noexcept
{
    if (body == nullptr)
    {
        return false;
    }

    return addPage(path, body, std::strlen(body));
}

bool HttpServer::addPage(const char *path, const char *body,
                         std::size_t length) noexcept
{
    if (body == nullptr)
    {
        return false;
    }

    return addRoute(Route{path, body, length, nullptr, false});
}

bool HttpServer::addRawResponse(const char *path, const void *response,
                                std::size_t length) noexcept
{
    if (response == nullptr || length == 0U)
    {
        return false;
    }

    return addRoute(Route{path, static_cast<const char *>(response), length,
                          nullptr, true});
}

bool HttpServer::addPage(const char *path, HttpRenderFn render) noexcept
{
    if (render == nullptr)
    {
        return false;
    }

    return addRoute(Route{path, nullptr, 0U, render, false});
}

bool HttpServer::start() noexcept
{
    if (g_running)
    {
        return false;
    }

    /* httpd_init() allocates a PCB and binds it, both of which are lwIP core
       operations, so it has to happen on tcpip_thread rather than here. */
    BinarySemaphore done;
    if (!done.isValid())
    {
        return false;
    }

    const bool posted = tcpip_callback(startHttpd, &done) == ERR_OK;
    return posted && done.acquire(5000U);
}

bool HttpServer::isRunning() const noexcept
{
    return g_running;
}

std::size_t HttpServer::routeCount() const noexcept
{
    return g_routeCount;
}

std::size_t HttpServer::routeCapacity() noexcept
{
    return ILT_HTTP_MAX_ROUTES;
}

std::size_t HttpServer::renderCapacity() noexcept
{
    return ILT_HTTP_RENDER_BYTES;
}

std::size_t HttpServer::renderBufferCount() noexcept
{
    return ILT_HTTP_RENDER_BUFFERS;
}

std::uint32_t HttpServer::requestsServed() const noexcept
{
    return g_requestsServed;
}

std::uint32_t HttpServer::requestsRejected() const noexcept
{
    return g_requestsRejected;
}

const char *HttpQuery::get(const char *name) const noexcept
{
    if (name == nullptr || names_ == nullptr || values_ == nullptr)
    {
        return nullptr;
    }

    for (int i = 0; i < count_; ++i)
    {
        if (names_[i] != nullptr && std::strcmp(names_[i], name) == 0)
        {
            return values_[i] != nullptr ? values_[i] : "";
        }
    }

    return nullptr;
}

bool HttpQuery::getUint(const char *name, std::uint32_t &out) const noexcept
{
    const char *const text = get(name);
    if (text == nullptr || *text == '\0')
    {
        return false;
    }

    char             *end    = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 0);

    if (end == text || *end != '\0')
    {
        return false; /* trailing rubbish means the caller mistyped it */
    }

    out = static_cast<std::uint32_t>(parsed);
    return true;
}

} // namespace ilt::net

/*==============================================================================
  lwIP filesystem backend

  httpd reaches content only through these three functions (fs_read is not
  needed: LWIP_HTTPD_DYNAMIC_FILE_READ is 0, so httpd sends straight from
  fs_file::data). All three are called from tcpip_thread.
==============================================================================*/

extern "C" err_t fs_open(struct fs_file *file, const char *name)
{
    if (file == nullptr || name == nullptr)
    {
        return ERR_ARG;
    }

    const Route *const route = findRoute(name);
    if (route == nullptr)
    {
        ++g_requestsRejected;
        return ERR_VAL;
    }

    std::memset(file, 0, sizeof(*file));

    if (route->body != nullptr)
    {
        /* Static: point straight at flash. Nothing to allocate, and any number
           of connections can share it. */
        file->data = route->body;
        file->len  = static_cast<int>(route->length);
    }
    else
    {
        char *const buffer = acquireRenderBuffer();
        if (buffer == nullptr)
        {
            ++g_requestsRejected;
            return ERR_MEM;
        }

        /* httpd has already cut the URI at '?', so no parameters are available
           yet; they arrive in httpd_cgi_handler() below, which re-renders. */
        const std::size_t produced =
            route->render(buffer, ILT_HTTP_RENDER_BYTES, ilt::net::HttpQuery{});
        if (produced == 0U || produced > ILT_HTTP_RENDER_BYTES)
        {
            releaseRenderBuffer(buffer);
            ++g_requestsRejected;
            return ERR_VAL;
        }

        file->data = buffer;
        file->len  = static_cast<int>(produced);

        /* fs_close() gives the buffer back; pextension is the per-file slot
           lwIP reserves for exactly this. */
        file->pextension = buffer;
    }

    file->index = file->len;

    /* Telling httpd the headers are already in the body makes it send these
       bytes untouched -- no generated status line, no Content-Type guessed from
       the extension. get_http_headers() is skipped entirely (httpd.c:2395). */
    file->flags = route->headerIncluded ? FS_FILE_FLAGS_HEADER_INCLUDED : 0U;

    ++g_requestsServed;
    return ERR_OK;
}

extern "C" void fs_close(struct fs_file *file)
{
    if (file != nullptr && file->pextension != nullptr)
    {
        releaseRenderBuffer(static_cast<const char *>(file->pextension));
        file->pextension = nullptr;
    }
}

/**
 * @brief Called by httpd once per request that carries query parameters.
 *
 * This is the only point at which the parameters and the file exist together:
 * httpd cuts the URI at '?' before fs_open(), and calls this afterwards but
 * before it reads file->len to decide how much to send. So a dynamic route is
 * rendered again here, with its parameters, over the same buffer.
 *
 * Enabled by LWIP_HTTPD_CGI_SSI in lwipopts.h. Runs in tcpip_thread.
 */
extern "C" void httpd_cgi_handler(struct fs_file *file, const char *uri,
                                  int iNumParams, char **pcParam, char **pcValue)
{
    if (file == nullptr || file->pextension == nullptr || uri == nullptr)
    {
        return; /* static route, or no render buffer: nothing to redo */
    }

    const Route *const route = findRoute(uri);
    if (route == nullptr || route->render == nullptr)
    {
        return;
    }

    char *const buffer = static_cast<char *>(file->pextension);
    const ilt::net::HttpQuery query{iNumParams, pcParam, pcValue};

    const std::size_t produced =
        route->render(buffer, ILT_HTTP_RENDER_BYTES, query);

    /* A render that fails now would leave the first, parameter-less body in
       place; that is a better answer than a truncated or empty one. */
    if (produced > 0U && produced <= ILT_HTTP_RENDER_BYTES)
    {
        file->data  = buffer;
        file->len   = static_cast<int>(produced);
        file->index = file->len;
    }
}

extern "C" int fs_bytes_left(struct fs_file *file)
{
    if (file == nullptr)
    {
        return 0;
    }

    return file->len - file->index;
}
