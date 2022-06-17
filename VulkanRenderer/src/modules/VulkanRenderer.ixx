module;
#include <cstdint>
#include <memory>
#include <optional>
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
		void CreateVkInstance(std::vector<char const*> const & layers, std::vector<char const*> const & extensions);
		void CreateSwapChain();
		void SelectPhysicalDevice();
		void CreateLogicalDevice();
		//void PopulateCommandList(XMMATRIX const & mvpMatrix);
		void WaitForPreviousFrame();
		void UploadGeometry();
		void ResizeRenderTargets();
		void ResizeDepthBuffer();
		void ResizeWindow();

		struct QueueFamilyIndices
		{
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
			bool IsComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
		};

		struct SwapChainSupportDetails
		{
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice const) const;
		bool IsDeviceSuitable(VkPhysicalDevice const) const;

		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice const) const;
		bool SwapChainRequirementsSatisfied(VkPhysicalDevice const) const;

		VkExtent2D ChooseSwapExtent(VkSurfaceCapabilitiesKHR const&) const;

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

		/* Render Targets */
		std::vector<VkImage> mSwapChainImages;
		VkFormat mSwapChainImageFormat;
		VkExtent2D mSwapChainExtent;
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
		VkShaderModule mFragmentShader;

		/* TODO: move this to a "game_object" class */
		uint32_t mIndexCount;

		std::unique_ptr<Camera> mCamera;

		VkDevice mDevice;
		VkInstance mInstance;
		VkPhysicalDevice mPhysicalDevice;
		VkQueue mGraphicsQueue;
		VkSurfaceKHR mSurface;
		VkSwapchainKHR mSwapChain;
		/* Synchronization objects */
		//ComPtr<ID3D12Fence> mFence;
		//uint32_t mFrameIndex{0};
		//uint64_t mFenceValue{0};
		//HANDLE mFenceEvent{nullptr};
	};

} // namespace gg
