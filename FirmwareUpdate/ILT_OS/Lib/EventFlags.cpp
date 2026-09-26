/**
  ******************************************************************************
  * @file    EventFlags.cpp
  * @brief   A set of bits threads can wait on.
  ******************************************************************************
  */

#include "EventFlags.h"

namespace ilt {
namespace {

/**
 * CMSIS returns the resulting flag set and errors in the same word,
 * distinguishing them by the top bit. Anything with it set is an error code
 * (osFlagsErrorTimeout, osFlagsErrorParameter, ...), not a flag set.
 */
bool isError(std::uint32_t result) noexcept
{
    return (result & 0x80000000U) != 0U;
}

} // namespace

EventFlags::EventFlags(const char *name) noexcept
{
    attr_.name    = name;
    attr_.cb_mem  = &controlBlock_;
    attr_.cb_size = sizeof(controlBlock_);

    handle_ = osEventFlagsNew(&attr_);
}

EventFlags::~EventFlags()
{
    if (handle_ != nullptr)
    {
        osEventFlagsDelete(handle_);
        handle_ = nullptr;
    }
}

std::uint32_t EventFlags::set(std::uint32_t flags) noexcept
{
    if (handle_ == nullptr)
    {
        return 0U;
    }

    const std::uint32_t result = osEventFlagsSet(handle_, flags);
    return isError(result) ? 0U : result;
}

std::uint32_t EventFlags::clear(std::uint32_t flags) noexcept
{
    if (handle_ == nullptr)
    {
        return 0U;
    }

    const std::uint32_t result = osEventFlagsClear(handle_, flags);
    return isError(result) ? 0U : result;
}

std::uint32_t EventFlags::get() const noexcept
{
    return handle_ != nullptr ? osEventFlagsGet(handle_) : 0U;
}

std::uint32_t EventFlags::wait(std::uint32_t flags, std::uint32_t options,
                               std::uint32_t timeoutMs) noexcept
{
    if (handle_ == nullptr)
    {
        return 0U;
    }

    const std::uint32_t result =
        osEventFlagsWait(handle_, flags, options, kernel::msToTicks(timeoutMs));

    /* A timeout is the ordinary outcome of a bounded wait, so it is reported as
       "nothing happened" rather than as a distinct failure the caller has to
       decode. */
    return isError(result) ? 0U : result;
}

std::uint32_t EventFlags::waitAny(std::uint32_t flags, std::uint32_t timeoutMs,
                                  bool autoClear) noexcept
{
    return wait(flags,
                osFlagsWaitAny | (autoClear ? 0U : osFlagsNoClear),
                timeoutMs);
}

std::uint32_t EventFlags::waitAll(std::uint32_t flags, std::uint32_t timeoutMs,
                                  bool autoClear) noexcept
{
    return wait(flags,
                osFlagsWaitAll | (autoClear ? 0U : osFlagsNoClear),
                timeoutMs);
}

} // namespace ilt
