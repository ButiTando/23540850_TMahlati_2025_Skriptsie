/**
  ******************************************************************************
  * @file    main.cpp
  * @brief   Host entry point -- the counterpart of the CubeMX main() on a board.
  *
  * Same shape as Core/Src/main.c on the MCU boards, minus the hardware: bring
  * the kernel up, let the selected application create its threads, start the
  * scheduler. osKernelStart() does not return.
  ******************************************************************************
  */

#include "Application.h"

#include "cmsis_os2.h"

#include <cstdio>
#include <cstdlib>

extern "C" void ILT_HostAssert(const char *file, int line, const char *expression)
{
    std::fprintf(stderr, "\nconfigASSERT failed: %s\n  at %s:%d\n",
                 expression, file, line);
    std::fflush(stderr);
    std::abort();
}

int main(void)
{
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    std::printf("ILT firmware, host build\n");

    if (osKernelInitialize() != osOK)
    {
        std::fprintf(stderr, "osKernelInitialize failed\n");
        return 1;
    }

    ILT_ApplicationStart();

    if (osKernelStart() != osOK)
    {
        std::fprintf(stderr, "osKernelStart failed\n");
        return 1;
    }

    return 0; /* not reached */
}
