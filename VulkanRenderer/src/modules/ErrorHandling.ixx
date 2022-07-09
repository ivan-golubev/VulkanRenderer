module;
#include <exception>
#include <string>
#include <windows.h>
export module ErrorHandling;

export namespace gg
{
	class ComException : public std::exception
	{
	public:
		ComException(HRESULT hr);
		std::wstring whatString() const;
		char const* what() const override;
	private:
		HRESULT returnCode;
	};

	void ThrowIfFailed(HRESULT hr);
	void BreakIfFalse(bool);
} // namespace gg
