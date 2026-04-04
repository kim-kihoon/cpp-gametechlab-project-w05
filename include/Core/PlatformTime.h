#pragma once
#include <windows.h>
#include <cstdint>

namespace Core
{
    typedef uint64_t uint64;

    /**
     * [Windows 전용] 하드웨어 카운터를 이용한 정밀 시간 측정 클래스.
     */
    class FPlatformTime
    {
    public:
        static double GSecondsPerCycle;
        static bool bInitialized;

        static void InitTiming()
        {
            if (!bInitialized)
            {
                LARGE_INTEGER Frequency;
                QueryPerformanceFrequency(&Frequency);
                GSecondsPerCycle = 1.0 / static_cast<double>(Frequency.QuadPart);
                bInitialized = true;
            }
        }

        static uint64 Cycles64()
        {
            LARGE_INTEGER CycleCount;
            QueryPerformanceCounter(&CycleCount);
            return static_cast<uint64>(CycleCount.QuadPart);
        }

        static double ToMilliseconds(uint64 CycleDiff)
        {
            if (!bInitialized) InitTiming();
            return (static_cast<double>(CycleDiff) * GSecondsPerCycle) * 1000.0;
        }
    };
}
