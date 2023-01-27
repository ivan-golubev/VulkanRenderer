module;
#include <algorithm>
#include <array>
#include <cstdint>
#include <DirectXMath.h>
#include <filesystem>
#include <format>
#include <glm/glm.hpp>
#include <limits>
#include <optional>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <set>
#include <stb_image.h>
#include <vector>
#include <vulkan/vulkan.h>

#include <windows.h>
#include <pix3.h> // has to be the last - depends on types in windows.h

module VulkanRenderer;

import Application;
import Camera;
import ErrorHandling;
import GlobalSettings;
import Input;
import Vertex;
import ModelLoader;

using namespace DirectX;

namespace gg
{
	VulkanRenderer::VulkanRenderer(uint32_t width, uint32_t height, SDL_Window* windowHandle)
		: mWidth{ width }
		, mHeight{ height }
		, mWindowHandle{ windowHandle }
		, mCamera{ std::make_unique<Camera>() }
	{
		uint32_t extension_count;
		if (!SDL_Vulkan_GetInstanceExtensions(windowHandle, &extension_count, nullptr))
		{
			throw std::exception("Could not get the number of required instance extensions from SDL.");
		}
		std::vector<char const*> extensions(extension_count);
		if (!SDL_Vulkan_GetInstanceExtensions(windowHandle, &extension_count, extensions.data()))
		{
			throw std::exception("Could not get the names of required instance extensions from SDL.");
		}

		std::vector<char const*> layers;
		if constexpr (IsDebug())
		{ /* Enable the debug layer */
			layers.emplace_back("VK_LAYER_KHRONOS_validation");
		}

		CreateVkInstance(layers, extensions);

		/* Create a surface for rendering */
		if (!SDL_Vulkan_CreateSurface(windowHandle, mInstance, &mSurface))
		{
			throw std::exception("Could not create a Vulkan surface.");
		}

		SelectPhysicalDevice();
		CreateLogicalDevice();
		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();

		CreateUniformBuffers();
		CreateDescriptorSetLayout();

		CreateFrameBuffers();
		CreateCommandPool();

		CreateTextureImage();
		CreateTextureImageView();
		CreateTextureSampler();

		CreateDescriptorPool();
		CreateDescriptorSets();

		CreateCommandBuffers();
		CreateSyncObjects();
	}

	void VulkanRenderer::CreateVkInstance(std::vector<char const*> const& layers, std::vector<char const*> const& extensions)
	{
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "gg Vulkan Renderer";
		appInfo.applicationVersion = VK_HEADER_VERSION_COMPLETE;
		appInfo.pEngineName = "gg Engine";
		appInfo.engineVersion = VK_HEADER_VERSION_COMPLETE;
		appInfo.apiVersion = VK_HEADER_VERSION_COMPLETE;

		VkInstanceCreateInfo instInfo{};
		instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instInfo.flags = 0;
		instInfo.pApplicationInfo = &appInfo;
		instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instInfo.ppEnabledExtensionNames = extensions.data();
		instInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
		instInfo.ppEnabledLayerNames = layers.data();

		VkResult const result = vkCreateInstance(&instInfo, nullptr, &mInstance);
		if (VK_ERROR_INCOMPATIBLE_DRIVER == result)
		{
			throw std::exception("Unable to find a compatible Vulkan Driver.");
		}
		else if (result)
		{
			throw std::exception("Could not create a Vulkan instance (for unknown reasons).");
		}
	}

	static VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> const& availableFormats)
	{
		for (auto const& availableFormat : availableFormats)
			if (VK_FORMAT_B8G8R8A8_UNORM == availableFormat.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == availableFormat.colorSpace)
				return availableFormat;
		return availableFormats[0];
	}

	static VkPresentModeKHR chooseSwapPresentMode(std::vector<VkPresentModeKHR> const& availablePresentModes)
	{
		for (auto const& m : availablePresentModes)
			if (VK_PRESENT_MODE_MAILBOX_KHR == m)
				return m;
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D VulkanRenderer::ChooseSwapExtent(VkSurfaceCapabilitiesKHR const& capabilities) const
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;
		else
		{
			int width, height;
			SDL_Vulkan_GetDrawableSize(mWindowHandle, &width, &height);

			return
			{
				std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
				std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
			};
		}
	}

	void VulkanRenderer::CreateSwapChain()
	{
		SwapChainSupportDetails swapChainSupport{ QuerySwapChainSupport(mPhysicalDevice) };
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = mSurface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices{ FindQueueFamilies(mPhysicalDevice) };
		uint32_t queueFamilyIndices[]{ indices.graphicsFamily.value(), indices.presentFamily.value() };
		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		if (VK_SUCCESS != vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapChain))
		{
			throw std::runtime_error("Failed to create swap chain!");
		}
		/* Retrieve the swap chain images to render to */
		vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, nullptr);
		mSwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(mDevice, mSwapChain, &imageCount, mSwapChainImages.data());

		mSwapChainImageFormat = surfaceFormat.format;
		mSwapChainExtent = extent;
	}

	void VulkanRenderer::CreateImageViews()
	{
		mSwapChainImageViews.resize(mSwapChainImages.size());
		for (size_t i = 0; i < mSwapChainImages.size(); ++i)
			mSwapChainImageViews[i] = CreateImageView(mSwapChainImages[i], mSwapChainImageFormat);
	}

	void VulkanRenderer::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = mSwapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (VK_SUCCESS != vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass))
		{
			throw std::runtime_error("failed to create render pass!");
		}

	}

	void VulkanRenderer::CreateDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings { uboLayoutBinding, samplerLayoutBinding };
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (VK_SUCCESS != vkCreateDescriptorSetLayout(mDevice, &layoutInfo, nullptr, &mDescriptorSetLayout))
			throw std::runtime_error("failed to create descriptor set layout!");
	}

	void VulkanRenderer::CreateGraphicsPipeline()
	{
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = mModel->shaderProgram->GetVertexShader();
		vertShaderStageInfo.pName = VS_ENTRY_POINT;

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = mModel->shaderProgram->GetFragmentShader();
		fragShaderStageInfo.pName = FS_ENTRY_POINT;

		VkPipelineShaderStageCreateInfo const shaderStages[]{ vertShaderStageInfo, fragShaderStageInfo };

		auto bindingDescription = Vertex::GetBindingDescription();
		auto attributeDescriptions = Vertex::GetAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(mSwapChainExtent.width);
		viewport.height = static_cast<float>(mSwapChainExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = mSwapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &mDescriptorSetLayout;

		if (VK_SUCCESS != vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout))
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.layout = mPipelineLayout;
		pipelineInfo.renderPass = mRenderPass;
		pipelineInfo.subpass = 0;

		if (VK_SUCCESS != vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mGraphicsPipeline))
		{
			throw std::runtime_error("failed to create graphics pipeline!");
		}

		/* cleanup */
	}

	void VulkanRenderer::CreateFrameBuffers()
	{
		mFrameBuffers.resize(mSwapChainImageViews.size());
		for (size_t i{ 0 }; i < mSwapChainImageViews.size(); ++i)
		{
			VkImageView attachments[]{ mSwapChainImageViews[i] };
			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = mRenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = mSwapChainExtent.width;
			framebufferInfo.height = mSwapChainExtent.height;
			framebufferInfo.layers = 1;
			if (VK_SUCCESS != vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mFrameBuffers[i]))
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	void VulkanRenderer::CreateCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(mPhysicalDevice);
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
		if (VK_SUCCESS != vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool))
		{
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void VulkanRenderer::CreateTextureImage()
	{
		// TODO: move this outside of the renderer
		int texWidth, texHeight, texChannels;
		std::string textureFileAbsPath{ std::filesystem::absolute("../../textures/CubeColor.tga").generic_string() };
		stbi_uc* pixels = stbi_load(textureFileAbsPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSizeBytes = texWidth * texHeight * 4;

		if (!pixels)
			throw std::runtime_error("failed to load texture image!");

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(stagingBuffer
			, stagingBufferMemory
			, imageSizeBytes
			, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
			, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		void* mappedData;
		vkMapMemory(mDevice, stagingBufferMemory, 0, imageSizeBytes, 0, &mappedData);
		memcpy(mappedData, pixels, static_cast<size_t>(imageSizeBytes));
		vkUnmapMemory(mDevice, stagingBufferMemory);

		stbi_image_free(pixels);

		CreateImage(texWidth
			, texHeight
			, VK_FORMAT_R8G8B8A8_SRGB
			, VK_IMAGE_TILING_OPTIMAL
			, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
			, mTextureImage
			, mTextureImageMemory);

		TransitionImageLayout(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(stagingBuffer, mTextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		TransitionImageLayout(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
		vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
	}

	void VulkanRenderer::CreateImage(
		  uint32_t width
		, uint32_t height
		, VkFormat format
		, VkImageTiling tiling
		, VkImageUsageFlags usage
		, VkMemoryPropertyFlags properties
		, VkImage& image
		, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = static_cast<uint32_t>(width);
		imageInfo.extent.height = static_cast<uint32_t>(height);
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

		if (vkCreateImage(mDevice, &imageInfo, nullptr, &image) != VK_SUCCESS)
			throw std::runtime_error("failed to create image!");

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(mDevice, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate image memory!");

		vkBindImageMemory(mDevice, image, imageMemory, 0);
	}

	void VulkanRenderer::CreateCommandBuffers()
	{
		mCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = mCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());
		if (VK_SUCCESS != vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()))
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void VulkanRenderer::CreateSyncObjects()
	{
		mImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		mRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			if (vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS
				|| vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS
				|| vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create semaphores!");
			}
		}
	}

	VulkanRenderer::QueueFamilyIndices VulkanRenderer::FindQueueFamilies(VkPhysicalDevice const device) const
	{
		uint32_t queueFamilyCount{ 0 };
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		QueueFamilyIndices indices{};
		int i = 0;
		for (auto const& q : queueFamilies)
		{
			if (q.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphicsFamily = i;

			VkBool32 presentSupport{ false };
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface, &presentSupport);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.IsComplete())
				break;
			i++;
		}
		return indices;
	}

	uint32_t VulkanRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("failed to find suitable memory type!");
	}

	static std::vector<char const*> const deviceExtensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	static bool checkDeviceExtensionSupport(VkPhysicalDevice const device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> availableExtensionNames{};
		for (auto& ext : availableExtensions)
			availableExtensionNames.insert(ext.extensionName);

		for (auto& ext : deviceExtensions)
			if (!availableExtensionNames.contains(ext))
				return false;
		return true;
	}

	VulkanRenderer::SwapChainSupportDetails VulkanRenderer::QuerySwapChainSupport(VkPhysicalDevice device) const
	{
		SwapChainSupportDetails details{};

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, nullptr);

		if (0 != formatCount)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, nullptr);

		if (0 != presentModeCount)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	bool VulkanRenderer::SwapChainRequirementsSatisfied(VkPhysicalDevice const device) const
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	bool VulkanRenderer::IsDeviceSuitable(VkPhysicalDevice const device) const
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == deviceProperties.deviceType
			&& checkDeviceExtensionSupport(device)
			&& SwapChainRequirementsSatisfied(device)
			&& FindQueueFamilies(device).IsComplete()
			&& supportedFeatures.samplerAnisotropy;
	}

	void VulkanRenderer::SelectPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
		if (0 == deviceCount)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
		for (auto const& d : devices)
		{
			if (IsDeviceSuitable(d))
			{
				mPhysicalDevice = d;
				break;
			}
		}
		if (VK_NULL_HANDLE == mPhysicalDevice)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	void VulkanRenderer::CreateLogicalDevice()
	{
		QueueFamilyIndices indices{ FindQueueFamilies(mPhysicalDevice) };
		std::set<uint32_t> uniqueQueueFamilies{ indices.graphicsFamily.value(), indices.presentFamily.value() };
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

		float queuePriority{ 1.0f };
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}
		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		if (VK_SUCCESS != vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice))
		{
			throw std::runtime_error("failed to create logical device!");
		}
		vkGetDeviceQueue(mDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
	}

	void VulkanRenderer::UploadGeometry(std::unique_ptr<Model> model)
	{
		mModel = std::move(model);
		for (auto& m : mModel->meshes)
			CreateVertexBuffer(m);
		CreateGraphicsPipeline();
	}

	void VulkanRenderer::CreateVertexBuffer(Mesh const& mesh)
	{
		uint32_t const VB_sizeBytes{ mesh.VerticesSizeBytes() };

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(stagingBuffer, stagingBufferMemory, VB_sizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* mappedData;
		vkMapMemory(mDevice, stagingBufferMemory, 0, VB_sizeBytes, 0, &mappedData);
		memcpy(mappedData, mesh.Vertices.data(), static_cast<size_t>(VB_sizeBytes));
		vkUnmapMemory(mDevice, stagingBufferMemory);

		VkBufferUsageFlagBits const usage = static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		CreateBuffer(mVB, mVertexBufferMemory, VB_sizeBytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CopyBuffer(stagingBuffer, mVB, VB_sizeBytes);

		vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
		vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
	}

	void VulkanRenderer::CreateIndexBuffer(Mesh const& mesh)
	{
		VkBuffer IB{};
		VkDeviceMemory IndexBufferMemory{};

		uint32_t const IB_sizeBytes{ mesh.IndicesSizeBytes() };

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		CreateBuffer(stagingBuffer, stagingBufferMemory, IB_sizeBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		void* mappedData;
		vkMapMemory(mDevice, stagingBufferMemory, 0, IB_sizeBytes, 0, &mappedData);
		memcpy(mappedData, mesh.Indices.data(), static_cast<size_t>(IB_sizeBytes));
		vkUnmapMemory(mDevice, stagingBufferMemory);

		VkBufferUsageFlagBits const usage = static_cast<VkBufferUsageFlagBits>(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		CreateBuffer(IB, IndexBufferMemory, IB_sizeBytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CopyBuffer(stagingBuffer, IB, IB_sizeBytes);

		vkDestroyBuffer(mDevice, stagingBuffer, nullptr);
		vkFreeMemory(mDevice, stagingBufferMemory, nullptr);
	}

	void VulkanRenderer::CreateUniformBuffers()
	{
		VkDeviceSize const bufferSize = sizeof(XMMATRIX);
		mUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		mUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i{ 0 }; i < MAX_FRAMES_IN_FLIGHT; ++i)
			CreateBuffer(mUniformBuffers[i], mUniformBuffersMemory[i], bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	void VulkanRenderer::CreateDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		if (VK_SUCCESS != vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool))
			throw std::runtime_error("failed to create descriptor pool!");
	}

	void VulkanRenderer::CreateDescriptorSets()
	{
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, mDescriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = mDescriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		allocInfo.pSetLayouts = layouts.data();

		mDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		if (VK_SUCCESS != vkAllocateDescriptorSets(mDevice, &allocInfo, mDescriptorSets.data()))
		{
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = mUniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(XMMATRIX);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = mTextureImageView;
			imageInfo.sampler = mTextureSampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = mDescriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo;

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = mDescriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		}
	}

	void VulkanRenderer::ResizeWindow()
	{
		RecreateSwapChain();
		float windowAspectRatio{ mWidth / static_cast<float>(mHeight) };
		mCamera->UpdateProjectionMatrix(windowAspectRatio);
		mWindowResized = false;
	}

	void VulkanRenderer::CleanupSwapChain()
	{
		for (auto framebuffer : mFrameBuffers)
			vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
		for (auto imageView : mSwapChainImageViews)
			vkDestroyImageView(mDevice, imageView, nullptr);

		vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
		vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
		vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
		vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
	}

	void VulkanRenderer::RecreateSwapChain()
	{
		vkDeviceWaitIdle(mDevice);
		CleanupSwapChain();

		CreateSwapChain();
		CreateImageViews();
		CreateRenderPass();
		CreateGraphicsPipeline();
		CreateFrameBuffers();
	}

	void VulkanRenderer::CreateBuffer(
		VkBuffer& outBuffer,
		VkDeviceMemory& outBufferMemory,
		uint64_t sizeBytes,
		VkBufferUsageFlagBits usage,
		VkMemoryPropertyFlags properties
	)
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = sizeBytes;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (VK_SUCCESS != vkCreateBuffer(mDevice, &bufferInfo, nullptr, &outBuffer))
		{
			throw std::runtime_error("failed to create vertex buffer!");
		}

		/* allocate and bind memory to this buffer */
		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(mDevice, outBuffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);
		if (VK_SUCCESS != vkAllocateMemory(mDevice, &allocInfo, nullptr, &outBufferMemory))
		{
			throw std::runtime_error("failed to allocate vertex buffer memory!");
		}
		vkBindBufferMemory(mDevice, outBuffer, outBufferMemory, 0);
	}

	void VulkanRenderer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
		
		EndSingleTimeCommands(commandBuffer);
	}

	VulkanRenderer::~VulkanRenderer()
	{
		/* Ensure that the GPU is no longer referencing resources that are about to be
		 cleaned up by the destructor. */
		vkDeviceWaitIdle(mDevice);

		CleanupSwapChain();
		
		vkDestroySampler(mDevice, mTextureSampler, nullptr);
		vkDestroyImageView(mDevice, mTextureImageView, nullptr);
		vkDestroyImage(mDevice, mTextureImage, nullptr);
		vkFreeMemory(mDevice, mTextureImageMemory, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
			vkDestroyFence(mDevice, mInFlightFences[i], nullptr);
		}
		vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

		for (size_t i{ 0 }; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			vkDestroyBuffer(mDevice, mUniformBuffers[i], nullptr);
			vkFreeMemory(mDevice, mUniformBuffersMemory[i], nullptr);
		}

		vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
		vkDestroyBuffer(mDevice, mVB, nullptr);
		vkFreeMemory(mDevice, mVertexBufferMemory, nullptr);

		/* destroys the associated shaders */
		mModel.reset();

		vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
		vkDestroyDevice(mDevice, nullptr);
		vkDestroyInstance(mInstance, nullptr);

		SDL_DestroyWindow(mWindowHandle);
		SDL_Quit();
	}

	void VulkanRenderer::OnWindowResized(uint32_t width, uint32_t height)
	{
		mWindowResized = true;
		mWidth = std::max(8u, width);
		mHeight = std::max(8u, height);
	}

	VkDevice VulkanRenderer::GetDevice() { return mDevice; }

	void VulkanRenderer::Render(uint64_t deltaTimeMs)
	{
		vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(mDevice, mSwapChain, UINT64_MAX, mImageAvailableSemaphores[mCurrentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			ResizeWindow();
			return;
		}
		else if (result != VK_SUCCESS)
			throw std::runtime_error("failed to acquire swap chain image!");

		/* Rotate the model */
		auto const elapsedTimeMs = Application::Get()->GetTimeManager()->GetCurrentTimeMs();
		auto const rotation = 0.0002f * DirectX::XM_PI * elapsedTimeMs;
		XMMATRIX const modelMatrix = XMMatrixMultiply(XMMatrixRotationY(rotation), XMMatrixRotationZ(rotation));

		mCamera->UpdateCamera(deltaTimeMs);
		XMMATRIX const& viewMatrix = mCamera->GetViewMatrix();
		XMMATRIX const& mProjectionMatrix = mCamera->GetProjectionMatrix();

		XMMATRIX mvpMatrix = XMMatrixMultiply(modelMatrix, viewMatrix);
		mvpMatrix = XMMatrixMultiply(mvpMatrix, mProjectionMatrix);

		/* submit the UBO data */
		void* data;
		vkMapMemory(mDevice, mUniformBuffersMemory[mCurrentFrame], 0, sizeof(XMMATRIX), 0, &data);
		memcpy(data, &mvpMatrix, sizeof(XMMATRIX));
		vkUnmapMemory(mDevice, mUniformBuffersMemory[mCurrentFrame]);
		/***********************/

		vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrame]);

		/* Record all the commands we need to render the scene into the command list. */
		RecordCommandBuffer(mCommandBuffers[mCurrentFrame], imageIndex, mvpMatrix);
		/* Execute the commands */
		SubmitCommands();
		/* Present the frame and inefficiently wait for the frame to render. */
		result = Present(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mWindowResized)
			ResizeWindow();
		else if (result != VK_SUCCESS)
			throw std::runtime_error("failed to present swap chain image!");
		mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	VkResult VulkanRenderer::Present(uint32_t imageIndex)
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphores[mCurrentFrame] };
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { mSwapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		return vkQueuePresentKHR(mGraphicsQueue, &presentInfo);
	}

	void VulkanRenderer::SubmitCommands()
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		VkSemaphore waitSemaphores[]{ mImageAvailableSemaphores[mCurrentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mCommandBuffers[mCurrentFrame];
		VkSemaphore signalSemaphores[] = { mRenderFinishedSemaphores[mCurrentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;
		if (VK_SUCCESS != vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrame]))
		{
			throw std::runtime_error("failed to submit a command buffer!");
		}
	}

	void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, XMMATRIX const& mvpMatrix)
	{
		vkResetCommandBuffer(commandBuffer, 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (VK_SUCCESS != vkBeginCommandBuffer(commandBuffer, &beginInfo))
		{
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = mRenderPass;
		renderPassInfo.framebuffer = mFrameBuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = mSwapChainExtent;

		VkClearValue clearColor = { {{0.0f, 0.2f, 0.4f, 1.0f}} };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

		VkBuffer vertexBuffers[]{ mVB };
		VkDeviceSize offsets[]{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		/* bind a desciptor for the UBO */
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSets[mCurrentFrame], 0, nullptr);
		for (auto& m : mModel->meshes)
			vkCmdDraw(commandBuffer, m.GetVertexCount(), 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);
		if (VK_SUCCESS != vkEndCommandBuffer(commandBuffer))
		{
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	VkCommandBuffer VulkanRenderer::BeginSingleTimeCommands()
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = mCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		return commandBuffer;
	}

	void VulkanRenderer::EndSingleTimeCommands(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(mGraphicsQueue);

		vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
	}

	void VulkanRenderer::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else
			throw std::invalid_argument("unsupported layout transition!");

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands(commandBuffer);
	}

	void VulkanRenderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			width,
			height,
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		EndSingleTimeCommands(commandBuffer);
	}

	VkImageView VulkanRenderer::CreateImageView(VkImage image, VkFormat format)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(mDevice, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
			throw std::runtime_error("failed to create texture image view!");
		return imageView;
	}

	void VulkanRenderer::CreateTextureImageView()
	{
		mTextureImageView = CreateImageView(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB);
	}

	void VulkanRenderer::CreateTextureSampler()
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		{
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
			samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		}
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(mDevice, &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS)
			throw std::runtime_error("failed to create texture sampler!");
	}

} // namespace gg
