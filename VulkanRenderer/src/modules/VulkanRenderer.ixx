module;
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>
#include <SDL2/SDL_video.h>
#include <vulkan/vulkan.h>
export module VulkanRenderer;

import Camera;
import Input;
import Vertex;
import TimeManager;

namespace gg 
{

	export class VulkanRenderer {
	public:
		VulkanRenderer(uint32_t width, uint32_t height, SDL_Window*);
		~VulkanRenderer();
		void OnWindowResized(uint32_t width, uint32_t height);
		void Render(uint64_t deltaTimeMs);
	private:
		//void PopulateCommandList(XMMATRIX const & mvpMatrix);
		void WaitForPreviousFrame();
		void UploadGeometry();
		void ResizeRenderTargets();
		void ResizeDepthBuffer();
		void ResizeWindow();

		VkShaderModule createShaderModule(std::vector<char> const& shaderBlob);
		//void CreateBuffer(
		//	ComPtr<ID3D12GraphicsCommandList> const & commandList, 
		//	ComPtr<ID3D12Resource> & gpuResource, 
		//	ComPtr<ID3D12Resource> & cpuResource, 
		//	void const * data, 
		//	uint64_t sizeBytes,
		//	std::wstring const & resourceName
		//);

		static constexpr int8_t mFrameCount{ 2 };
		uint32_t mWidth;
		uint32_t mHeight;
		SDL_Window* mWindowHandle;
		bool mWindowResized{ true };

		//D3D12_VIEWPORT mViewport;
		//D3D12_RECT mScissorRect;

		//ComPtr<ID3D12Device4> mDevice;
		//ComPtr<ID3D12CommandQueue> mCommandQueue;
		//ComPtr<ID3D12CommandAllocator> mCommandAllocator;
		//ComPtr<ID3D12GraphicsCommandList> mCommandList;
		//ComPtr<IDXGISwapChain3> mSwapChain;
		//ComPtr<ID3D12PipelineState> mPipelineState;
		//ComPtr<ID3D12RootSignature> mRootSignature;

		///* Render Targets */
		//ComPtr<ID3D12Resource> mRenderTargets[mFrameCount];
		//CD3DX12_CPU_DESCRIPTOR_HANDLE mRtvHandles[mFrameCount];
		//ComPtr<ID3D12DescriptorHeap> mRenderTargetViewHeap;
		//uint32_t mRtvDescriptorSize;

		///* Depth */
		//ComPtr<ID3D12Resource> mDepthBuffer;
		//ComPtr<ID3D12DescriptorHeap> mDepthStencilHeap;
		//CD3DX12_CPU_DESCRIPTOR_HANDLE mDsvHandle;
		//uint32_t mDsvDescriptorSize;

		///* Vertex and Index Buffers for the cube. TODO: There is a better place for them. */
		//ComPtr<ID3D12Resource> mVB_GPU_Resource;
		//ComPtr<ID3D12Resource> mVB_CPU_Resource;
		//D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

		//ComPtr<ID3D12Resource> mIB_GPU_Resource;
		//ComPtr<ID3D12Resource> mIB_CPU_Resource;
		//D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

		/* Shaders */
		VkShaderModule mVertexShader;
		VkShaderModule mPixelShader;

		/* TODO: move this to a "game_object" class */
		uint32_t mIndexCount;

		std::unique_ptr<Camera> mCamera;

		VkDevice mVkDevice;
		VkInstance mVkInstance;
		VkSurfaceKHR mVkSurface;
		/* Synchronization objects */
		//ComPtr<ID3D12Fence> mFence;
		//uint32_t mFrameIndex{0};
		//uint64_t mFenceValue{0};
		//HANDLE mFenceEvent{nullptr};
	};

} // namespace gg
