/**
  ******************************************************************************
  * @file    BoardRig.cpp
  * @brief   Degrader and pulse-counter implementation for the Host build.
  *
  * Two modes, and which one you get is an explicit choice:
  *
  *   default            the mechanism reports itself ABSENT and does nothing.
  *   ILT_SIMULATE=1     a plausible mechanism responds to the motors and the
  *                      counters produce traffic.
  *
  * The default is absence, not simulation, for the reason argued in the Nucleo
  * stub: a fake that invents switch transitions lets the lens state machine
  * appear to work on a board that cannot move anything, and the first honest
  * failure then happens on the real rig. Simulation is for exercising the UI
  * and the data path, so it is opt-in and says so on stdout when it engages.
  ******************************************************************************
  */

#include "Bsp/Degrader.h"
#include "Bsp/PulseCounter.h"

#include "Kernel.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

bool simulateEnabled() noexcept
{
    static const bool enabled = [] {
        const char *v = std::getenv("ILT_SIMULATE");
        const bool on = (v != nullptr && v[0] == '1');
        std::printf("rig: hardware simulation %s\n", on ? "ENABLED (ILT_SIMULATE=1)"
                                                        : "off");
        return on;
    }();
    return enabled;
}

/* --- degrader model ------------------------------------------------------- */
/* The rig's real numbers: TIM12 on the old degrader ran at
   108 MHz / 1080 / 500 = 200 Hz. Steps per station is a mechanical constant of
   the carousel that the schematic does not record -- 200 is a placeholder, so
   one station takes one second. Correct it once the gear ratio is known; every
   position estimate scales with it. */
constexpr std::uint32_t kStepRateHz      = 200U;
constexpr std::uint32_t kStepsPerStation = 200U;

/* How long the DC motor takes to drive a lens fully in or out. */
constexpr std::uint32_t kTravelMs = 300U;

bool g_stepperRunning = false;
bool g_dcRunning      = false;
bsp::degrader::BeamDirection g_dcDirection = bsp::degrader::BeamDirection::OutOfBeam;
std::uint32_t g_stepperSince = 0U;
std::uint32_t g_dcSince      = 0U;

/* Steps commanded, and the fractional carousel position they imply. The
   carousel is continuous: it wraps back to station 0 after the last lens. */
std::uint32_t g_steps = 0U;

/** Bring g_steps up to date for however long the stepper has been running. */
void accrueSteps() noexcept;

/* Which lenses the simulated mechanism actually has in the beam. */
bool g_inBeam[bsp::degrader::kLensCount] = {false};

std::uint32_t elapsedSince(std::uint32_t mark) noexcept
{
    return ilt::kernel::tickCount() - mark;
}

void accrueSteps() noexcept
{
    if (!g_stepperRunning)
    {
        return;
    }

    const std::uint32_t now     = ilt::kernel::tickCount();
    const std::uint32_t elapsed = now - g_stepperSince;
    const std::uint32_t due     = (elapsed * kStepRateHz) / 1000U;

    /* Advance the mark by exactly the steps taken, so the remainder is not lost
       and the average rate stays right over a long move. */
    if (due > 0U)
    {
        g_steps += due;
        g_stepperSince += (due * 1000U) / kStepRateHz;
    }
}

/** Which station the commanded step count puts the carousel on. */
std::size_t stationFromSteps() noexcept
{
    return static_cast<std::size_t>((g_steps / kStepsPerStation)
                                    % bsp::degrader::kLensCount);
}

/* --- pulse model ---------------------------------------------------------- */
/* Each channel counts at its own rate with a bit of scatter, so the six series
   are visibly different rather than six copies of one line. */
constexpr std::uint32_t kBaseRate[bsp::pulse::kChannelCount] =
    {1400U, 950U, 2300U, 420U, 1750U, 120U};

std::uint32_t g_counter[bsp::pulse::kChannelCount] = {0};
std::uint32_t g_lastSample = 0U;
std::uint32_t g_rng = 0x1234567U;

std::uint32_t nextRandom() noexcept
{
    /* xorshift32: cheap, and good enough to make a chart look alive. */
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

/** Advance the simulated counters to now. */
void accrue() noexcept
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

    /* More absorber in the beam means fewer particles reaching the detectors:
       a crude 3% attenuation per millimetre, so moving a lens in visibly
       changes the counts and the two halves of the rig are connected. */
    std::uint32_t thickness = 0U;
    for (std::size_t i = 0; i < bsp::degrader::kLensCount; ++i)
    {
        static const std::uint32_t mm[] = {2U, 3U, 6U, 8U, 10U, 12U, 30U};
        if (g_inBeam[i]) { thickness += mm[i]; }
    }

    double transmission = 1.0;
    for (std::uint32_t i = 0; i < thickness; ++i) { transmission *= 0.97; }

    for (std::size_t c = 0; c < bsp::pulse::kChannelCount; ++c)
    {
        const double expected =
            (static_cast<double>(kBaseRate[c]) * deltaMs / 1000.0) * transmission;
        const std::uint32_t jitter = nextRandom() % 21U;           /* 0..20 */
        const double scaled = expected * (0.90 + jitter / 100.0);  /* +/-10% */
        g_counter[c] += static_cast<std::uint32_t>(scaled);
    }
}

} // namespace

namespace bsp::degrader {

bool available() noexcept { return simulateEnabled(); }

void init() noexcept
{
    if (!simulateEnabled()) { return; }
    std::memset(g_inBeam, 0, sizeof(g_inBeam));
}

bool readSwitch(Lens lens, LimitSwitch which) noexcept
{
    if (!simulateEnabled()) { return false; }

    const std::size_t index = static_cast<std::size_t>(lens);
    if (index >= kLensCount) { return false; }

    switch (which)
    {
    case LimitSwitch::Select:
        /* Closes when the carousel has actually rotated to this station, so a
           lens further round genuinely takes longer to reach -- which is what
           makes the position readout worth looking at. */
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
    if (!g_stepperRunning) { g_stepperSince = ilt::kernel::tickCount(); }
    g_stepperRunning = true;
}

void stepperStop() noexcept
{
    accrueSteps();
    g_stepperRunning = false;
}

std::uint32_t stepsIssued() noexcept
{
    accrueSteps();
    return g_steps;
}

std::uint32_t stepRateHz() noexcept { return kStepRateHz; }
std::uint32_t stepsPerStation() noexcept { return kStepsPerStation; }

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
    g_dcRunning = false;
}

} // namespace bsp::degrader

namespace bsp::pulse {

bool available() noexcept { return simulateEnabled(); }

void init() noexcept
{
    if (!simulateEnabled()) { return; }
    std::memset(g_counter, 0, sizeof(g_counter));
    g_lastSample = ilt::kernel::tickCount();
}

std::uint32_t read(std::size_t channel) noexcept
{
    if (!simulateEnabled() || channel >= kChannelCount) { return 0U; }
    accrue();
    return g_counter[channel];
}

void reset(std::size_t channel) noexcept
{
    if (simulateEnabled() && channel < kChannelCount) { g_counter[channel] = 0U; }
}

void readAndReset(std::uint32_t *counts) noexcept
{
    if (counts == nullptr) { return; }

    if (!simulateEnabled())
    {
        std::memset(counts, 0, kChannelCount * sizeof(counts[0]));
        return;
    }

    accrue();
    for (std::size_t c = 0; c < kChannelCount; ++c)
    {
        counts[c]    = g_counter[c];
        g_counter[c] = 0U;
    }
}

} // namespace bsp::pulse
