module;
#include <cstdint>
#include <DirectXMath.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>
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
		void CreateDescriptorSetLayout();
		void CreateGraphicsPipeline();
		void CreateFrameBuffers();
		void CreateCommandPool();
		void CreateCommandBuffers();
		void CreateSyncObjects();
		void CreateVertexBuffer();
		void CreateIndexBuffer();
		void CreateUniformBuffers();
		void CreateDescriptorPool();
		void CreateDescriptorSets();
		void RecordCommandBuffer(VkCommandBuffer, uint32_t imageIndex, XMMATRIX const & mvpMatrix);
		VkResult Present(uint32_t imageIndex);
		void SubmitCommands();
		void UploadGeometry();

		void CleanupSwapChain();
		void RecreateSwapChain();
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
			VkBufferUsageFlagBits usage,
			VkMemoryPropertyFlags properties
		);

		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

		static constexpr int8_t MAX_FRAMES_IN_FLIGHT{ 2 };
		uint32_t mWidth;
		uint32_t mHeight;
		SDL_Window* mWindowHandle;
		bool mWindowResized{ true };

		VkCommandPool mCommandPool;
		std::vector<VkCommandBuffer> mCommandBuffers;
		VkRenderPass mRenderPass;
		VkDescriptorSetLayout mDescriptorSetLayout;
		VkPipelineLayout mPipelineLayout;
		VkPipeline mGraphicsPipeline;

		/* Render Targets */
		std::vector<VkImage> mSwapChainImages;
		std::vector<VkImageView> mSwapChainImageViews;
		std::vector<VkFramebuffer> mFrameBuffers;
		VkFormat mSwapChainImageFormat;
		VkExtent2D mSwapChainExtent;

		uint32_t mCurrentFrame{ 0 };

		/* Vertex and Index Buffers for the cube. TODO: There is a better place for them. */
		VkBuffer mVB;
		VkBuffer mIB;
		VkDeviceMemory mVertexBufferMemory;
		VkDeviceMemory mIndexBufferMemory;

		std::vector<VkBuffer> mUniformBuffers;
		std::vector<VkDeviceMemory> mUniformBuffersMemory;
		VkDescriptorPool mDescriptorPool;
		std::vector<VkDescriptorSet> mDescriptorSets;

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
