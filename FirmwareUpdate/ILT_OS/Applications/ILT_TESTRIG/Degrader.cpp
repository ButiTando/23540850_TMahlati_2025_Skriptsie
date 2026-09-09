/**
  ******************************************************************************
  * @file    Degrader.cpp
  * @brief   Lens carousel sequencing.
  ******************************************************************************
  */

#include "Degrader.h"

#include "Kernel.h"

namespace rig {
namespace {

using bsp::degrader::BeamDirection;
using bsp::degrader::Lens;
using bsp::degrader::LimitSwitch;
using bsp::degrader::StepperDirection;

/* The original settled the mechanism with `while (HAL_GetTick() - t < 5) {}`
   after a switch closed and after starting the DC motor. Same 5 ms, but as a
   sleep, so the CPU is available to the network threads meanwhile. */
constexpr std::uint32_t kSettleMs = 5U;

/* How often to look at a limit switch while waiting for it. Fast enough not to
   overshoot a moving mechanism, slow enough to leave the CPU alone. */
constexpr std::uint32_t kSwitchPollMs = 2U;

/* The original waited on a switch forever. A jammed carousel or a failed switch
   would therefore hang the whole superloop with a motor still energised. Here a
   move gives up, stops both motors and reports Fault. */
constexpr std::uint32_t kMoveTimeoutMs = 30000U;

/* How long to wait for a request before looking at the mechanism again. */
constexpr std::uint32_t kIdlePollMs = 100U;

bool bitSet(std::uint8_t mask, std::size_t index) noexcept
{
    return (mask & static_cast<std::uint8_t>(1U << index)) != 0U;
}

} // namespace

Degrader::Degrader() noexcept
    : StaticThread("degrader", osPriorityNormal)
{
    for (auto &state : lensStates_)
    {
        state = LensState::Unknown;
    }
}

bool Degrader::request(std::uint8_t lensMask) noexcept
{
    /* Only the seven lens bits are meaningful; the original's eighth bit was a
       "probe" flag that asked for a reply without changing anything, which an
       HTTP GET expresses better. */
    return requests_.tryPut(static_cast<std::uint8_t>(lensMask & 0x7FU));
}

LensState Degrader::lensState(std::size_t lens) const noexcept
{
    return lens < bsp::degrader::kLensCount ? lensStates_[lens]
                                            : LensState::Unknown;
}

std::uint16_t Degrader::responseWord() const noexcept
{
    std::uint16_t word = 0U;

    for (std::size_t i = 0U; i < bsp::degrader::kLensCount; ++i)
    {
        word |= static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(lensStates_[i]) & 0x3U) << (2U * i));
    }

    word |= static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(status_) & 0x3U) << 14U);

    return word;
}

bool Degrader::awaitSwitch(Lens lens, LimitSwitch which,
                           std::uint32_t timeoutMs) noexcept
{
    const std::uint32_t deadline = ilt::kernel::tickCount() + timeoutMs;

    while (!bsp::degrader::readSwitch(lens, which))
    {
        /* Nothing to do but wait; the loop below yields between polls. */
        /* Unsigned subtraction, so this stays correct across the 49-day tick
           wrap that a plain `now < deadline` comparison would get wrong. */
        if ((deadline - ilt::kernel::tickCount()) > timeoutMs)
        {
            return false;
        }

        sleep(kSwitchPollMs);
    }

    /* A SELECT switch closing is the only sensor confirmation of where the
       carousel actually is. Record it: everything else about position is
       commanded rather than measured. */
    if (which == LimitSwitch::Select)
    {
        confirmedStation_ = static_cast<int>(lens);
    }

    return true;
}

std::uint32_t Degrader::positionSteps() const noexcept
{
    return bsp::degrader::stepsIssued();
}

std::uint32_t Degrader::positionMilliStations() const noexcept
{
    const std::uint32_t perStation = bsp::degrader::stepsPerStation();
    if (perStation == 0U)
    {
        return 0U;
    }

    /* Fractional position within one full turn of the carousel, x1000. Done in
       64 bits: steps climbs without bound over a long run and a 32-bit multiply
       by 1000 would wrap after about seven hours of continuous stepping. */
    const std::uint64_t span = static_cast<std::uint64_t>(perStation)
                             * bsp::degrader::kLensCount;
    const std::uint64_t within = bsp::degrader::stepsIssued() % span;

    return static_cast<std::uint32_t>((within * 1000ULL) / perStation);
}

bool Degrader::moveLens(Lens lens, bool intoBeam) noexcept
{
    const std::size_t index = static_cast<std::size_t>(lens);
    lensStates_[index] = LensState::Moving;
    targetLens_ = static_cast<int>(index);

    /* 1. Rotate the carousel until this lens is aligned. Direction is fixed:
          the carousel is continuous, so one way round always arrives. */
    motion_ = Motion::Selecting;
    bsp::degrader::stepperStart(StepperDirection::Forward);

    if (!awaitSwitch(lens, LimitSwitch::Select, kMoveTimeoutMs))
    {
        bsp::degrader::stopAll();
        lensStates_[index] = LensState::Unknown;
        motion_ = Motion::Idle;
        targetLens_ = -1;
        return false;
    }

    sleep(kSettleMs);
    bsp::degrader::stepperStop();

    /* 2. Drive the aligned lens in or out until the matching switch closes. */
    motion_ = intoBeam ? Motion::Inserting : Motion::Retracting;
    bsp::degrader::dcMotorStart(intoBeam ? BeamDirection::IntoBeam
                                         : BeamDirection::OutOfBeam);
    sleep(kSettleMs);

    const LimitSwitch target = intoBeam ? LimitSwitch::In : LimitSwitch::Out;
    if (!awaitSwitch(lens, target, kMoveTimeoutMs))
    {
        bsp::degrader::stopAll();
        lensStates_[index] = LensState::Unknown;
        motion_ = Motion::Idle;
        targetLens_ = -1;
        return false;
    }

    bsp::degrader::dcMotorStop();
    lensStates_[index] = intoBeam ? LensState::In : LensState::Out;
    motion_ = Motion::Idle;
    targetLens_ = -1;
    return true;
}

std::size_t Degrader::currentStation() const noexcept
{
    const std::uint32_t perStation = bsp::degrader::stepsPerStation();
    if (perStation == 0U)
    {
        return 0U;
    }

    return static_cast<std::size_t>(
        (bsp::degrader::stepsIssued() / perStation) % bsp::degrader::kLensCount);
}

void Degrader::reconcile() noexcept
{
    const std::uint8_t desired = desiredMask_;
    std::uint8_t       current = currentMask_;

    if (desired == current)
    {
        status_ = ProcessStatus::Ready;
        return;
    }

    status_ = ProcessStatus::Processing;

    /* Serve whichever pending lens the carousel reaches first.
     *
     * The original walked the lenses in index order, which ignores where the
     * mechanism actually is: asked for 2 mm and 30 mm while parked at 30 mm, it
     * would drive most of the way round to 2 mm and then most of the way round
     * again to come back. The stepper only turns one way, so the cheapest order
     * is simply the order the stations come past -- pick the pending lens with
     * the smallest forward distance each time, and the whole set is served in
     * one sweep instead of several laps.
     *
     * Correctness does not depend on this: any order reaches the same end state.
     * It is travel, and therefore time and wear, that it saves.
     */
    for (;;)
    {
        const std::size_t station = currentStation();

        std::size_t next = bsp::degrader::kLensCount; /* none pending */
        std::size_t bestDistance = bsp::degrader::kLensCount;

        for (std::size_t i = 0U; i < bsp::degrader::kLensCount; ++i)
        {
            if (bitSet(desired, i) == bitSet(current, i))
            {
                continue; /* already where it should be */
            }

            /* Forward-only rotation, so distance wraps rather than going back. */
            const std::size_t distance =
                (i + bsp::degrader::kLensCount - station) % bsp::degrader::kLensCount;

            if (distance < bestDistance)
            {
                bestDistance = distance;
                next = i;
            }
        }

        if (next == bsp::degrader::kLensCount)
        {
            break; /* nothing left to move */
        }

        const bool want = bitSet(desired, next);

        if (moveLens(static_cast<Lens>(next), want))
        {
            /* Commit one lens at a time: if a later lens fails, the mask still
               reflects what the mechanism actually did. */
            current = static_cast<std::uint8_t>(
                want ? (current | (1U << next)) : (current & ~(1U << next)));
            currentMask_ = current;
            ++movesCompleted_;
        }
        else
        {
            ++moveFailures_;
            status_ = ProcessStatus::Fault;
            return;
        }
    }

    status_ = ProcessStatus::Ready;
}

void Degrader::run()
{
    if (!bsp::degrader::available())
    {
        /* No mechanism on this board. Stay alive and answerable -- the API and
           the data stream are still worth testing -- but say so rather than
           pretend to home. */
        status_ = ProcessStatus::Fault;

        for (;;)
        {
            std::uint8_t requested = 0U;
            if (requests_.get(requested, ilt::kWaitForever))
            {
                desiredMask_ = requested;
            }
        }
    }

    bsp::degrader::init();
    bsp::degrader::stopAll();
    status_ = ProcessStatus::Ready;

    for (;;)
    {
        std::uint8_t requested = 0U;
        if (requests_.get(requested, kIdlePollMs))
        {
            desiredMask_ = requested;
        }

        reconcile();
    }
}

} // namespace rig
