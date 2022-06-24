module;
#include <cstdint>
#include <DirectXMath.h>
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

using DirectX::XMMATRIX;

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
		void SelectPhysicalDevice();
		void CreateLogicalDevice();
		void CreateSwapChain();
		void CreateImageViews();
		void CreateRenderPass();
		void CreateGraphicsPipeline();
		void CreateFrameBuffers();
		void CreateCommandPool();
		void CreateCommandBuffers();
		void CreateSyncObjects();
		void RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex, XMMATRIX const & mvpMatrix);
		void Present(uint32_t imageIndex);
		void SubmitCommands();
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
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags) const;
		bool IsDeviceSuitable(VkPhysicalDevice const) const;

		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice const) const;
		bool SwapChainRequirementsSatisfied(VkPhysicalDevice const) const;

		VkExtent2D ChooseSwapExtent(VkSurfaceCapabilitiesKHR const&) const;

		VkShaderModule createShaderModule(std::vector<char> const& shaderBlob);
		void CreateBuffer(
			VkBuffer& outBuffer,
			VkDeviceMemory& outMemory,
			uint64_t sizeBytes,
			VkBufferUsageFlagBits usage
		);

		void UploadBufferData(VkDeviceMemory&, void const* data, uint64_t sizeBytes);

		//void CreateBuffer(
		//	ComPtr<ID3D12GraphicsCommandList> const & commandList, 
		//	ComPtr<ID3D12Resource> & gpuResource, 
		//	ComPtr<ID3D12Resource> & cpuResource, 
		//	void const * data, 
		//	uint64_t sizeBytes,
		//	std::wstring const & resourceName
		//);

		static constexpr int8_t MAX_FRAMES_IN_FLIGHT{ 2 };
		uint32_t mWidth;
		uint32_t mHeight;
		SDL_Window* mWindowHandle;
		bool mWindowResized{ true };

		VkCommandPool mCommandPool;
		std::vector<VkCommandBuffer> mCommandBuffers;
		//ComPtr<ID3D12RootSignature> mRootSignature;
		VkRenderPass mRenderPass;
		VkPipelineLayout mPipelineLayout;
		VkPipeline mGraphicsPipeline;

		/* Render Targets */
		std::vector<VkImage> mSwapChainImages;
		std::vector<VkImageView> mSwapChainImageViews;
		std::vector<VkFramebuffer> mFrameBuffers;
		VkFormat mSwapChainImageFormat;
		VkExtent2D mSwapChainExtent;

		uint32_t mCurrentFrame{ 0 };

		///* Depth */
		//ComPtr<ID3D12Resource> mDepthBuffer;
		//ComPtr<ID3D12DescriptorHeap> mDepthStencilHeap;
		//CD3DX12_CPU_DESCRIPTOR_HANDLE mDsvHandle;
		//uint32_t mDsvDescriptorSize;

		/* Vertex and Index Buffers for the cube. TODO: There is a better place for them. */
		VkBuffer mVB;
		VkBuffer mIB;
		VkDeviceMemory mVertexBufferMemory;
		VkDeviceMemory mIndexBufferMemory;
		//ComPtr<ID3D12Resource> mVB_GPU_Resource;
		//ComPtr<ID3D12Resource> mVB_CPU_Resource;
		//D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

		//ComPtr<ID3D12Resource> mIB_GPU_Resource;
		//ComPtr<ID3D12Resource> mIB_CPU_Resource;
		//D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

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
		std::vector<VkSemaphore> mImageAvailableSemaphores;
		std::vector<VkSemaphore> mRenderFinishedSemaphores;
		std::vector<VkFence> mInFlightFences;
	};

} // namespace gg
