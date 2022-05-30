module;
#include <string>
export module Logging;

export namespace awesome::logging
{
	enum class DebugLevel
	{
		Info,
		Error
	};

	void DebugLog(DebugLevel level, std::wstring const &);
	void DebugLog(DebugLevel level, wchar_t const *);

	std::wstring ToWString(std::string const &);

} // namespace awesome::logging
