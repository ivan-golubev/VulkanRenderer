module;
#include <cstdint>
export module TimeManager;

namespace gg 
{
	export class TimeManager {
	public:
		TimeManager();
		uint64_t Tick();
		uint64_t GetCurrentTimeTicks() const;
		uint64_t GetCurrentTimeMs() const;
		float GetCurrentTimeSec() const;
	private:
		uint64_t ticksPerSec{ 0 };
		uint64_t currentTimeTicks{ 0 };
		uint64_t startTimeTicks{ 0 };
	};
} // namespace gg
