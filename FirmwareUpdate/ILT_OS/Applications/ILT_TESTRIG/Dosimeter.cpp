/**
  ******************************************************************************
  * @file    Dosimeter.cpp
  * @brief   Pulse channel sampling and record publication.
  ******************************************************************************
  */

#include "Dosimeter.h"

#include "Kernel.h"
#include "Net/StreamServer.h"

#include <cstdio>
#include <cstring>

namespace rig {

Dosimeter::Dosimeter() noexcept
    : StaticThread("dosimeter", osPriorityNormal)
{
}

bool Dosimeter::setPeriod(std::uint32_t seconds) noexcept
{
    if (seconds < kMinPeriodS || seconds > kMaxPeriodS)
    {
        return false;
    }

    periodS_ = seconds;
    return true;
}

void Dosimeter::latest(Sample &out) const noexcept
{
    ilt::LockGuard guard(latestLock_);
    out = latest_;
}

void Dosimeter::run()
{
    if (bsp::pulse::available())
    {
        bsp::pulse::init();
    }

    auto &stream = ilt::net::StreamServer::instance();

    /* Absolute deadline rather than sleep(period): the time spent reading,
       formatting and handing the record to the network does not then push the
       next sample later, so the period stays honest over hours of running. */
    std::uint32_t next = ilt::kernel::tickCount();

    for (;;)
    {
        next += periodS_ * 1000U;
        ilt::kernel::delayUntil(next);

        std::uint32_t counts[bsp::pulse::kChannelCount] = {0};
        bsp::pulse::readAndReset(counts);

        const std::uint32_t seq = sequence_ + 1U;
        {
            ilt::LockGuard guard(latestLock_);
            latest_.sequence = seq;
            latest_.uptimeMs = ilt::kernel::tickCount();
            std::memcpy(latest_.counts, counts, sizeof(counts));
        }
        sequence_ = seq;

        char record[160];
        int written = std::snprintf(
            record, sizeof(record),
            "{\"type\":\"dosimeter\",\"seq\":%lu,\"t_ms\":%lu,\"period_s\":%lu,"
            "\"ch\":[%lu,%lu,%lu,%lu,%lu,%lu]}",
            static_cast<unsigned long>(seq),
            static_cast<unsigned long>(latest_.uptimeMs),
            static_cast<unsigned long>(periodS_),
            static_cast<unsigned long>(counts[0]),
            static_cast<unsigned long>(counts[1]),
            static_cast<unsigned long>(counts[2]),
            static_cast<unsigned long>(counts[3]),
            static_cast<unsigned long>(counts[4]),
            static_cast<unsigned long>(counts[5]));

        if (written > 0 && static_cast<std::size_t>(written) < sizeof(record))
        {
            stream.broadcast(record, static_cast<std::size_t>(written));
        }
    }
}

} // namespace rig
