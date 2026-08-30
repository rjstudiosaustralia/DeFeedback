#include "../Source/Core/RingGuardCore.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <vector>

namespace
{
void run (int instanceCount, int blockSize)
{
    constexpr double sampleRate = 48000.0;
    constexpr int seconds = 10;
    const auto blocks = static_cast<int> (sampleRate * seconds / blockSize);

    std::vector<ringguard::RealtimeProcessor> processors (static_cast<std::size_t> (instanceCount));
    std::vector<std::vector<float>> buffers (
        static_cast<std::size_t> (instanceCount), std::vector<float> (static_cast<std::size_t> (blockSize)));

    for (auto& processor : processors)
    {
        processor.prepare (sampleRate, static_cast<std::size_t> (blockSize), 1);
        processor.setSettings ({ 1.0f, 0.65f, 4 });
    }

    double phase = 0.0;
    const auto increment = 2.0 * 3.14159265358979323846 * 997.0 / sampleRate;
    auto worstBlock = std::chrono::nanoseconds::zero();
    const auto start = std::chrono::steady_clock::now();

    for (int block = 0; block < blocks; ++block)
    {
        for (auto& buffer : buffers)
        {
            for (auto& sample : buffer)
            {
                sample = static_cast<float> (0.06 * std::sin (phase));
                phase += increment;
                if (phase > 2.0 * 3.14159265358979323846)
                    phase -= 2.0 * 3.14159265358979323846;
            }
        }

        const auto blockStart = std::chrono::steady_clock::now();
        for (int instance = 0; instance < instanceCount; ++instance)
        {
            float* channel[] { buffers[static_cast<std::size_t> (instance)].data() };
            processors[static_cast<std::size_t> (instance)].process (
                channel, 1, static_cast<std::size_t> (blockSize));
        }
        worstBlock = std::max (worstBlock,
                               std::chrono::steady_clock::now() - blockStart);
    }

    const auto elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const auto renderedAudioSeconds = static_cast<double> (seconds);
    const auto deadlineMicroseconds = 1.0e6 * blockSize / sampleRate;
    const auto worstMicroseconds = std::chrono::duration<double, std::micro> (worstBlock).count();

    std::cout << std::setw (2) << instanceCount << " instances, "
              << std::setw (3) << blockSize << " samples: "
              << std::fixed << std::setprecision (3)
              << elapsed << " s wall / " << renderedAudioSeconds << " s audio, "
              << "RT load " << (100.0 * elapsed / renderedAudioSeconds) << "%, "
              << "worst block " << worstMicroseconds << " us / "
              << deadlineMicroseconds << " us\n";
}
}

int main()
{
    for (const auto blockSize : { 16, 32, 64, 128 })
    {
        run (1, blockSize);
        run (10, blockSize);
    }

    std::cout << "Benchmark numbers are diagnostic, not CI pass/fail criteria.\n";
    return 0;
}
