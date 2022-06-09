module;
#include <cstdint>
#include <cstdlib>
#include <windows.h>
module TimeManager;

namespace gg
{
	TimeManager::TimeManager() 
	{
		LARGE_INTEGER Frequency;
		if (!QueryPerformanceFrequency(&Frequency))
			exit(-1); // Hardware does not support high-res counter
		ticksPerSec = Frequency.QuadPart;

		LARGE_INTEGER perfCount;
		QueryPerformanceCounter(&perfCount);
		startTimeTicks = currentTimeTicks = perfCount.QuadPart;
	}

	uint64_t TimeManager::Tick() 
	{
		uint64_t prevTimeTicks = currentTimeTicks;
		LARGE_INTEGER perfCount;
		QueryPerformanceCounter(&perfCount);
		currentTimeTicks = perfCount.QuadPart;
		uint64_t deltaMs = (currentTimeTicks - prevTimeTicks) * 1000ULL / ticksPerSec;
		//deltaMs = std::max(deltaMs, 7ULL); /* make sure the first frame is at least 7 ms (144 FPS) */
		return deltaMs;
	}

	uint64_t TimeManager::GetCurrentTimeTicks() const 
	{
		return currentTimeTicks - startTimeTicks;
	}

	uint64_t TimeManager::GetCurrentTimeMs() const 
	{
		return GetCurrentTimeTicks() * 1000ULL / ticksPerSec;
	}

	float TimeManager::GetCurrentTimeSec() const 
	{
		return static_cast<float> (GetCurrentTimeTicks() / ticksPerSec);
	}
} // namespace gg