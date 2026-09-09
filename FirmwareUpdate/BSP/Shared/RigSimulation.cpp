/**
  ******************************************************************************
  * @file    RigSimulation.cpp
  * @brief   A degrader and dosimeter that behave like the real ones, in software.
  *
  * Compiled in place of a board's real BoardRig.cpp when ILT_SIMULATE_RIG is on.
  * It implements the same Bsp/Degrader.h and Bsp/PulseCounter.h, so nothing
  * above the BSP can tell the difference -- the lens state machine really runs,
  * really waits on switches, and really takes time to move.
  *
  * It exists so the web UI can be developed against a board on a desk with no
  * mechanism attached. It is NOT the default on any hardware board: a fake that
  * invents switch transitions lets the state machine appear to work on a rig
  * that cannot move, and the first honest failure would then be on the real
  * one. Turning it on is a deliberate `-DILT_SIMULATE_RIG=ON`, and the
  * application says "simulated" on the console and in /api/status.json.
  *
  * What it models, and why each part matters:
  *
  *   The carousel really rotates. Steps accrue at the real 200 Hz, a SELECT
  *   switch closes only when the carousel has actually reached that station, so
  *   a lens further round genuinely takes longer -- which is what makes the
  *   position readout worth watching and the nearest-first ordering testable.
  *
  *   The beam really attenuates. Counts fall by about 3% per millimetre of
  *   absorber in the beam, so inserting a lens visibly changes the channel
  *   data. The two halves of the rig are connected, as they are in the physics.
  ******************************************************************************
  */

#include "Bsp/Degrader.h"
#include "Bsp/PulseCounter.h"

#include "Kernel.h"

#include <cstring>

namespace {

/* The rig's real numbers: TIM12 on the original degrader ran at
   108 MHz / 1080 / 500 = 200 Hz. Steps per station is a mechanical constant the
   schematic does not record -- 200 is a placeholder giving one second per
   station. Correct it once the gear ratio is known; every position estimate
   scales with it. */
constexpr std::uint32_t kStepRateHz      = 200U;
constexpr std::uint32_t kStepsPerStation = 200U;

/** How long the DC motor takes to drive a lens fully in or out. */
constexpr std::uint32_t kTravelMs = 300U;

/** Lens thicknesses in carousel order, for the attenuation model. */
constexpr std::uint32_t kThicknessMm[bsp::degrader::kLensCount] =
    {2U, 3U, 6U, 8U, 10U, 12U, 30U};

/* --- mechanism state ------------------------------------------------------ */
bool g_stepperRunning = false;
bool g_dcRunning      = false;
bsp::degrader::BeamDirection g_dcDirection = bsp::degrader::BeamDirection::OutOfBeam;
std::uint32_t g_stepperSince = 0U;
std::uint32_t g_dcSince      = 0U;
std::uint32_t g_steps        = 0U;
bool g_inBeam[bsp::degrader::kLensCount] = {false};

/* --- detector state ------------------------------------------------------- */
/* Each channel counts at its own rate, so the six series are visibly different
   rather than six copies of one line. */
constexpr std::uint32_t kBaseRate[bsp::pulse::kChannelCount] =
    {1400U, 950U, 2300U, 420U, 1750U, 120U};

std::uint32_t g_counter[bsp::pulse::kChannelCount] = {0};
std::uint32_t g_lastSample = 0U;
std::uint32_t g_rng = 0x1234567U;

std::uint32_t elapsedSince(std::uint32_t mark) noexcept
{
    return ilt::kernel::tickCount() - mark;
}

/** Bring the step count up to date for however long the stepper has run. */
void accrueSteps() noexcept
{
    if (!g_stepperRunning)
    {
        return;
    }

    const std::uint32_t elapsed = elapsedSince(g_stepperSince);
    const std::uint32_t due     = (elapsed * kStepRateHz) / 1000U;

    /* Advance the mark by exactly the steps taken, so the remainder is not lost
       and the average rate stays right over a long move. */
    if (due > 0U)
    {
        g_steps += due;
        g_stepperSince += (due * 1000U) / kStepRateHz;
    }
}

std::size_t stationFromSteps() noexcept
{
    return static_cast<std::size_t>((g_steps / kStepsPerStation)
                                    % bsp::degrader::kLensCount);
}

std::uint32_t nextRandom() noexcept
{
    /* xorshift32: cheap, and good enough to make a chart look alive. */
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

/** Advance the simulated counters to now, attenuated by what is in the beam. */
void accrueCounts() noexcept
{
    const std::uint32_t now = ilt::kernel::tickCount();
    if (g_lastSample == 0U)
    {
        g_lastSample = now;
        return;
    }

    const std::uint32_t deltaMs = now - g_lastSample;
    if (deltaMs == 0U)
    {
        return;
    }
    g_lastSample = now;

    std::uint32_t thickness = 0U;
    for (std::size_t i = 0U; i < bsp::degrader::kLensCount; ++i)
    {
        if (g_inBeam[i])
        {
            thickness += kThicknessMm[i];
        }
    }

    /* ~3% of the beam absorbed per millimetre. Integer maths throughout: this
       runs on a Cortex-M and there is no reason to pull in soft-float for a
       number that only has to look plausible. Scale is per-mille. */
    std::uint32_t transmission = 1000U;
    for (std::uint32_t i = 0U; i < thickness; ++i)
    {
        transmission = (transmission * 97U) / 100U;
    }

    for (std::size_t c = 0U; c < bsp::pulse::kChannelCount; ++c)
    {
        const std::uint32_t expected =
            (kBaseRate[c] * deltaMs / 1000U) * transmission / 1000U;
        const std::uint32_t jitter = nextRandom() % 21U;      /* 0..20 */
        g_counter[c] += (expected * (90U + jitter)) / 100U;   /* +/-10% */
    }
}

} // namespace

namespace bsp::degrader {

bool available() noexcept { return true; }

void init() noexcept
{
    std::memset(g_inBeam, 0, sizeof(g_inBeam));
    g_steps = 0U;
}

bool readSwitch(Lens lens, LimitSwitch which) noexcept
{
    const std::size_t index = static_cast<std::size_t>(lens);
    if (index >= kLensCount)
    {
        return false;
    }

    switch (which)
    {
    case LimitSwitch::Select:
        accrueSteps();
        return stationFromSteps() == index;

    case LimitSwitch::In:
        if (g_dcRunning && g_dcDirection == BeamDirection::IntoBeam &&
            elapsedSince(g_dcSince) >= kTravelMs)
        {
            g_inBeam[index] = true;
            return true;
        }
        return false;

    case LimitSwitch::Out:
        if (g_dcRunning && g_dcDirection == BeamDirection::OutOfBeam &&
            elapsedSince(g_dcSince) >= kTravelMs)
        {
            g_inBeam[index] = false;
            return true;
        }
        return false;
    }

    return false;
}

void stepperStart(StepperDirection) noexcept
{
    if (!g_stepperRunning)
    {
        g_stepperSince = ilt::kernel::tickCount();
    }
    g_stepperRunning = true;
}

void stepperStop() noexcept
{
    accrueSteps();
    g_stepperRunning = false;
}

void dcMotorStart(BeamDirection direction) noexcept
{
    if (!g_dcRunning || g_dcDirection != direction)
    {
        g_dcSince = ilt::kernel::tickCount();
    }
    g_dcDirection = direction;
    g_dcRunning   = true;
}

void dcMotorStop() noexcept { g_dcRunning = false; }

void stopAll() noexcept
{
    accrueSteps();
    g_stepperRunning = false;
    g_dcRunning      = false;
}

std::uint32_t stepsIssued() noexcept
{
    accrueSteps();
    return g_steps;
}

std::uint32_t stepRateHz() noexcept { return kStepRateHz; }
std::uint32_t stepsPerStation() noexcept { return kStepsPerStation; }

} // namespace bsp::degrader

namespace bsp::pulse {

bool available() noexcept { return true; }

void init() noexcept
{
    std::memset(g_counter, 0, sizeof(g_counter));
    g_lastSample = ilt::kernel::tickCount();
}

std::uint32_t read(std::size_t channel) noexcept
{
    if (channel >= kChannelCount)
    {
        return 0U;
    }

    accrueCounts();
    return g_counter[channel];
}

void reset(std::size_t channel) noexcept
{
    if (channel < kChannelCount)
    {
        g_counter[channel] = 0U;
    }
}

void readAndReset(std::uint32_t *counts) noexcept
{
    if (counts == nullptr)
    {
        return;
    }

    accrueCounts();
    for (std::size_t c = 0U; c < kChannelCount; ++c)
    {
        counts[c]    = g_counter[c];
        g_counter[c] = 0U;
    }
}

} // namespace bsp::pulse
