module;
#include <cstdint>
#include <DirectXMath.h>
export module Camera;

using DirectX::XMMATRIX;
using DirectX::XMVECTOR;

export namespace gg {

	class Camera
	{
	public:
		void UpdateCamera(uint64_t deltaTimeMs);
		void UpdateProjectionMatrix(float windowAspectRatio);
		XMMATRIX const & GetViewMatrix() const;
		XMMATRIX const & GetProjectionMatrix() const;
	private:
		XMMATRIX mProjectionMatrix{};
		XMMATRIX mViewMatrix{};

		XMVECTOR mCameraPos{ 0.f, 0.f, -3.f, 1.f };
		XMVECTOR mFocusPoint{ 0.f, 0.f, 0.f, 1.f };
	};

} // namespace gg