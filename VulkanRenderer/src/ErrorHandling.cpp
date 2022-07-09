module;
#include <cstdint>
#include <format>
#include <string>
#include <windows.h>
module ErrorHandling;

import GlobalSettings;

namespace gg
{
	ComException::ComException(HRESULT hr) : returnCode{ hr }
	{
	}

	std::wstring ComException::whatString() const
	{
		return std::format(L"Failed with HRESULT={}", static_cast<uint32_t>(returnCode));
	}

	char const* ComException::what() const
	{
		static char s_str[64]{};
		sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(returnCode));
		return s_str;
	}

	void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
			throw ComException(hr);
	}

	void BreakIfFalse(bool condition)
	{
		if constexpr (IsDebug())
		{
			if (!condition)
				__debugbreak();
		}
	}

} // namespace gg
