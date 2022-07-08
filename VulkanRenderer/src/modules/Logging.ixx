module;
#include <string>
export module Logging;

export namespace gg
{
	enum class DebugLevel
	{
		Info,
		Error
	};

	void DebugLog(DebugLevel level, std::string const&);

} // namespace gg
