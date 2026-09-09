/**
  ******************************************************************************
  * @file    BoardRig.cpp
  * @brief   Degrader and pulse-counter stubs for the NUCLEO-F767ZI.
  *
  * The Nucleo is the network and application test target; none of the rig
  * hardware is attached to it. These implementations report the mechanism as
  * absent and do nothing, which lets ILT_TESTRIG build, run its threads and
  * serve its API here while the real drivers wait for the DegraderDosimeter
  * board's CubeMX tree.
  *
  * They are deliberately NOT a simulation. A stub that invented plausible
  * switch transitions would let the lens state machine appear to work on a
  * board that cannot move anything, and the first honest failure would then be
  * on the real rig. available() returning false is the useful answer: the
  * application reports "no mechanism" and every other part of it -- threads,
  * HTTP API, the data stream -- is still exercised for real.
  ******************************************************************************
  */

#include "Bsp/Degrader.h"
#include "Bsp/PulseCounter.h"

#include <cstring>

namespace bsp::degrader {

bool available() noexcept { return false; }

void init() noexcept {}

bool readSwitch(Lens, LimitSwitch) noexcept
{
    /* No switch is ever met, so the state machine's guarded waits time out
       rather than advancing on a lie. */
    return false;
}

void stepperStart(StepperDirection) noexcept {}
void stepperStop() noexcept {}

/* No stepper to command, so nothing has been issued. The rate and the station
   pitch are the rig's real constants, reported so the UI can show the scale it
   would be working in. */
std::uint32_t stepsIssued() noexcept { return 0U; }
std::uint32_t stepRateHz() noexcept { return 200U; }
std::uint32_t stepsPerStation() noexcept { return 200U; }
void dcMotorStart(BeamDirection) noexcept {}
void dcMotorStop() noexcept {}
void stopAll() noexcept {}

} // namespace bsp::degrader

namespace bsp::pulse {

bool available() noexcept { return false; }

void init() noexcept {}

std::uint32_t read(std::size_t) noexcept { return 0U; }

void reset(std::size_t) noexcept {}

void readAndReset(std::uint32_t *counts) noexcept
{
    if (counts != nullptr)
    {
        std::memset(counts, 0, kChannelCount * sizeof(counts[0]));
    }
}

} // namespace bsp::pulse
