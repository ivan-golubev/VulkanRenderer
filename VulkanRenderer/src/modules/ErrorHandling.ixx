module;
#include <exception>
#include <string>
#include <windows.h>
export module ErrorHandling;

namespace gg
{
	export class ComException : public std::exception
	{
	public:
		ComException(HRESULT hr);
		std::wstring whatString() const;
		char const* what() const override;
	private:
		HRESULT returnCode;
	};

	export void ThrowIfFailed(HRESULT hr);
} // namespace gg
