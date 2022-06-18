module;
#include <algorithm>
#include <cstdint>
#include <DirectXMath.h>
#include <format>
#include <fstream>
#include <glm/glm.hpp>
#include <limits>
#include <optional>
#include <set>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <vulkan/vulkan.h>
#include <windows.h>
#include <wrl.h>

#include <pix3.h> // has to be the last - depends on types in windows.h

module VulkanRenderer;

import Application;
import Camera;
import ErrorHandling;
import GlobalSettings;
import Input;
import Vertex;

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace gg {

    static std::vector<char> readFile(std::string const & filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) 
        {
            throw std::runtime_error(std::format("failed to open file: {}", filename));
        }

        size_t const fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    VkShaderModule VulkanRenderer::createShaderModule(std::vector<char> const & shaderBlob)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderBlob.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderBlob.data());
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

    VulkanRenderer::VulkanRenderer(uint32_t width, uint32_t height, SDL_Window* windowHandle)
        : mWidth{ width }
        , mHeight{ height }
        , mWindowHandle{ windowHandle }
        , mPhysicalDevice{ VK_NULL_HANDLE }
        //, mScissorRect{ D3D12_DEFAULT_SCISSOR_STARTX, D3D12_DEFAULT_SCISSOR_STARTY, D3D12_VIEWPORT_BOUNDS_MAX, D3D12_VIEWPORT_BOUNDS_MAX }
        //, mViewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f }
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
        CreateGraphicsPipeline();
        CreateFrameBuffers();

        /* Create render targets */
        ResizeRenderTargets();
        /* Create depth buffer */
        ResizeDepthBuffer();

        UploadGeometry();
    }

    void VulkanRenderer::CreateVkInstance(std::vector<char const*> const& layers, std::vector<char const*> const& extensions)
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "gg Vulkan Renderer";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName = "gg Engine";
        appInfo.engineVersion = VK_API_VERSION_1_0;
        appInfo.apiVersion = VK_API_VERSION_1_0;

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

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<VkSurfaceFormatKHR> const & availableFormats) 
    {
        for (auto const& availableFormat : availableFormats) 
            if (VK_FORMAT_B8G8R8A8_SRGB == availableFormat.format && VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == availableFormat.colorSpace)
                return availableFormat;
        return availableFormats[0];
    }

    static VkPresentModeKHR chooseSwapPresentMode(std::vector<VkPresentModeKHR> const & availablePresentModes) 
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
        uint32_t queueFamilyIndices[] { indices.graphicsFamily.value(), indices.presentFamily.value() };
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
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = mSwapChainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = mSwapChainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            if (VK_SUCCESS != vkCreateImageView(mDevice, &createInfo, nullptr, &mSwapChainImageViews[i])) 
            {
                throw std::runtime_error("failed to create image views!");
            }
        }
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

        if (VK_SUCCESS != vkCreateRenderPass(mDevice, &renderPassInfo, nullptr, &mRenderPass)) 
        {
            throw std::runtime_error("failed to create render pass!");
        }

    }

    void VulkanRenderer::CreateGraphicsPipeline()
    {
        VkShaderModule vertexShader;
        VkShaderModule fragmentShader;
        /* Read shaders */
        {
            std::vector<char> vertexShaderBlob = readFile("shaders//colored_surface_VS.spv");
            std::vector<char> fragmentShaderBlob = readFile("shaders//colored_surface_PS.spv");
            vertexShader = createShaderModule(vertexShaderBlob);
            fragmentShader = createShaderModule(fragmentShaderBlob);
        }

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertexShader;
        vertShaderStageInfo.pName = "vs_main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragmentShader;
        fragShaderStageInfo.pName = "ps_main";

        VkPipelineShaderStageCreateInfo const shaderStages[] { vertShaderStageInfo, fragShaderStageInfo };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        // TODO: fill

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
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
#if 0
        std::vector<VkDynamicState> dynamicStates
        {
            VK_DYNAMIC_STATE_VIEWPORT
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();
#endif
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

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
        vkDestroyShaderModule(mDevice, vertexShader, nullptr);
        vkDestroyShaderModule(mDevice, fragmentShader, nullptr);
    }

    void VulkanRenderer::CreateFrameBuffers()
    {
        mFramebuffers.resize(mSwapChainImageViews.size());
        for (size_t i{ 0 }; i < mSwapChainImageViews.size(); ++i)
        {
            VkImageView attachments[] { mSwapChainImageViews[i] };
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = mRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = mSwapChainExtent.width;
            framebufferInfo.height = mSwapChainExtent.height;
            framebufferInfo.layers = 1;
            if (VK_SUCCESS != vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &mFramebuffers[i])) 
            {
                throw std::runtime_error("failed to create framebuffer!");
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

            if (presentSupport) {
                indices.presentFamily = i;
            }

            if (indices.IsComplete())
                break;
            i++;
        }
        return indices;
    }

    static std::vector<char const *> const deviceExtensions
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

        if (0 != presentModeCount) {
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

        return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == deviceProperties.deviceType
            && checkDeviceExtensionSupport(device)
            && SwapChainRequirementsSatisfied(device)
            && FindQueueFamilies(device).IsComplete();
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
        for (auto const & d : devices)
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

    void VulkanRenderer::UploadGeometry()
    {
        /* Initialize the vertices. TODO: move to a separate class */
        // TODO: in fact, cubes are not fun, read data from an .fbx
        std::vector<Vertex> const mVertices{
            /*  x      y      z     w     r      g    b     a */
            {-1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 0
            {-1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f}, // 1
            { 1.0f,  1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f}, // 2
            { 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f}, // 3
            {-1.0f, -1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f}, // 4
            {-1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f}, // 5
            { 1.0f,  1.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f}, // 6
            { 1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f}, // 7
        };
        std::vector<uint32_t> const mIndices{
            0, 1, 2, 0, 2, 3,
            4, 6, 5, 4, 7, 6,
            4, 5, 1, 4, 1, 0,
            3, 2, 6, 3, 6, 7,
            1, 5, 6, 1, 6, 2,
            4, 0, 3, 4, 3, 7
        };
        mIndexCount = static_cast<uint32_t>(mIndices.size());

        uint32_t const VB_sizeBytes = static_cast<uint32_t>(mVertices.size() * sizeof(Vertex));
        uint32_t const IB_sizeBytes = static_cast<uint32_t>(mIndices.size() * sizeof(uint32_t));

        /*CreateBuffer(mCommandList, mVB_GPU_Resource, mVB_CPU_Resource, mVertices.data(), VB_sizeBytes, L"VertexBuffer");
        CreateBuffer(mCommandList, mIB_GPU_Resource, mIB_CPU_Resource, mIndices.data(), IB_sizeBytes, L"IndexBuffer");*/

        WaitForPreviousFrame();
    }

    void VulkanRenderer::ResizeWindow()
    {
        //mViewport = CD3DX12_VIEWPORT(0.0f, 0.0f,static_cast<float>(mWidth), static_cast<float>(mHeight), 0.0f, 1.0f);
        ResizeRenderTargets();
        ResizeDepthBuffer();
        mWindowResized = false;
    }

    void VulkanRenderer::ResizeRenderTargets()
    {
        /*for (uint32_t i{ 0 }; i < mFrameCount; ++i)
            mRenderTargets[i].Reset();

        ThrowIfFailed(mSwapChain->ResizeBuffers(mFrameCount, mWidth, mHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0));

        for (uint8_t n = 0; n < mFrameCount; n++)
        {
            ThrowIfFailed(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));
            mRtvHandles[n] = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), n, mRtvDescriptorSize);
            mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, mRtvHandles[n]);
            SetName(mRenderTargets[n].Get(), std::format(L"SwapChainBuffer[{}]", n));
        }*/
    }

    void VulkanRenderer::ResizeDepthBuffer()
    {
        //mDepthBuffer.Reset();

        //D3D12_CLEAR_VALUE optimizedClearValue{};
        //optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        //optimizedClearValue.DepthStencil = { 1.0f, 0 };

        //uint32_t const width{ std::max(mWidth, 0U) };
        //uint32_t const height{ std::max(mHeight, 0U) };

        //CD3DX12_HEAP_PROPERTIES const defaultHeapProps{ D3D12_HEAP_TYPE_DEFAULT };
        //D3D12_RESOURCE_DESC const resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        //    DXGI_FORMAT_D32_FLOAT, width, height,
        //    1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        //);
        //ThrowIfFailed(mDevice->CreateCommittedResource(
        //    &defaultHeapProps,
        //    D3D12_HEAP_FLAG_NONE,
        //    &resourceDesc,
        //    D3D12_RESOURCE_STATE_DEPTH_WRITE,
        //    &optimizedClearValue,
        //    IID_PPV_ARGS(&mDepthBuffer)
        //));
        //SetName(mDepthBuffer.Get(), std::format(L"{}_GPU", L"DepthStencilTexture"));

        ///* create the DSV */
        //D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        //dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        //dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        //dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        //mDsvHandle = { mDepthStencilHeap->GetCPUDescriptorHandleForHeapStart() };
        //mDevice->CreateDepthStencilView(mDepthBuffer.Get(), &dsvDesc, mDsvHandle);
    }

    //void VulkanRenderer::CreateBuffer(
    //    ComPtr<ID3D12GraphicsCommandList> const & commandList,
    //    ComPtr<ID3D12Resource> & gpuResource,
    //    ComPtr<ID3D12Resource> & cpuResource,
    //    void const * data,
    //    uint64_t sizeBytes,
    //    std::wstring const & resourceName
    //)
    //{
    //    /* create an intermediate resource */
    //    CD3DX12_HEAP_PROPERTIES uploadHeapProps{ D3D12_HEAP_TYPE_UPLOAD };
    //    CD3DX12_RESOURCE_DESC uploadResourceProps{ CD3DX12_RESOURCE_DESC::Buffer(sizeBytes) };
    //    ThrowIfFailed(mDevice->CreateCommittedResource(
    //        &uploadHeapProps,
    //        D3D12_HEAP_FLAG_NONE,
    //        &uploadResourceProps,
    //        D3D12_RESOURCE_STATE_GENERIC_READ,
    //        nullptr,
    //        IID_PPV_ARGS(&cpuResource))
    //    );
    //    SetName(cpuResource.Get(), std::format(L"{}_CPU", resourceName));

    //    /* create the target resource on the GPU */
    //    CD3DX12_HEAP_PROPERTIES const defaultHeapProps{ D3D12_HEAP_TYPE_DEFAULT };
    //    CD3DX12_RESOURCE_DESC const gpuResourceProps{ CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, D3D12_RESOURCE_FLAG_NONE) };
    //    ThrowIfFailed(mDevice->CreateCommittedResource(
    //        &defaultHeapProps,
    //        D3D12_HEAP_FLAG_NONE,
    //        &gpuResourceProps,
    //        D3D12_RESOURCE_STATE_COPY_DEST,
    //        nullptr,
    //        IID_PPV_ARGS(&gpuResource))
    //    );
    //    SetName(gpuResource.Get(), std::format(L"{}_GPU", resourceName));

    //    /* transfer the data */
    //    D3D12_SUBRESOURCE_DATA subresourceData = {};
    //    subresourceData.pData = data;
    //    subresourceData.RowPitch = subresourceData.SlicePitch = sizeBytes;

    //    UpdateSubresources(
    //        commandList.Get(),
    //        gpuResource.Get(),
    //        cpuResource.Get(),
    //        0, 0, 1,
    //        &subresourceData
    //    );
    //}

    VulkanRenderer::~VulkanRenderer()
    {
        /* Ensure that the GPU is no longer referencing resources that are about to be
         cleaned up by the destructor. */
        WaitForPreviousFrame();
        //CloseHandle(mFenceEvent);

        for (auto framebuffer : mFramebuffers) 
        {
            vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
        }

        for (auto imageView : mSwapChainImageViews)
        {
            vkDestroyImageView(mDevice, imageView, nullptr);
        }

        vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
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

    void VulkanRenderer::Render(uint64_t deltaTimeMs)
    {
        if (mWindowResized)
        {
            ResizeWindow();
            float windowAspectRatio{ mWidth / static_cast<float>(mHeight) };
            mCamera->UpdateProjectionMatrix(windowAspectRatio);
        }
        /* Rotate the model */
        auto const elapsedTimeMs = Application::Get().GetTimeManager().GetCurrentTimeMs();
        auto const rotation = 0.0002f * DirectX::XM_PI * elapsedTimeMs;
        XMMATRIX const modelMatrix = XMMatrixMultiply(XMMatrixRotationY(rotation), XMMatrixRotationZ(rotation));

        mCamera->UpdateCamera(deltaTimeMs);
        XMMATRIX const & viewMatrix = mCamera->GetViewMatrix();
        XMMATRIX const & mProjectionMatrix = mCamera->GetProjectionMatrix();

        XMMATRIX mvpMatrix = XMMatrixMultiply(modelMatrix, viewMatrix);
        mvpMatrix = XMMatrixMultiply(mvpMatrix, mProjectionMatrix); 

        /* Record all the commands we need to render the scene into the command list. */
        //PopulateCommandList(mvpMatrix);

        ///* Execute the command list. */
        //ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
        //mCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        ///* Present the frame and inefficiently wait for the frame to render. */
        //ThrowIfFailed(mSwapChain->Present(1, 0));
        WaitForPreviousFrame();
    }

    void VulkanRenderer::WaitForPreviousFrame()
    {
        //uint64_t const fence{ mFenceValue };
        //ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), fence));
        //mFenceValue++;

        ///* Wait until the previous frame is finished */
        //if (mFence->GetCompletedValue() < fence)
        //{
        //    ThrowIfFailed(mFence->SetEventOnCompletion(fence, mFenceEvent));
        //    WaitForSingleObject(mFenceEvent, INFINITE);
        //}
    }

    //void VulkanRenderer::PopulateCommandList(XMMATRIX const & mvpMatrix)
    //{
    //    //ThrowIfFailed(mCommandAllocator->Reset());
    //    ThrowIfFailed(mCommandList->Reset(mCommandAllocator.Get(), mPipelineState.Get()));
    //    
    //    uint8_t const frameIndex{ static_cast<uint8_t>(mSwapChain->GetCurrentBackBufferIndex()) };
    //    /* Set all the state first */
    //    {            
    //        mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    //        mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
    //        mCommandList->IASetIndexBuffer(&mIndexBufferView);

    //        mCommandList->RSSetViewports(1, &mViewport);
    //        mCommandList->RSSetScissorRects(1, &mScissorRect);

    //        mCommandList->SetPipelineState(mPipelineState.Get());
    //        mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
    //        mCommandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / sizeof(float), &mvpMatrix, 0);

    //        mCommandList->OMSetRenderTargets(1, &mRtvHandles[frameIndex], true, &mDsvHandle);
    //    }

    //    {
    //        PIXScopedEvent(mCommandList.Get(), PIX_COLOR(0,0,255), L"RenderFrame");

    //        /* Back buffer to be used as a Render Target */
    //        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    //            mRenderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET
    //        );
    //        mCommandList->ResourceBarrier(1, &barrier);

    //        PIXSetMarker(mCommandList.Get(), PIX_COLOR_DEFAULT, L"SampleMarker");

    //        {  /* Record commands */
    //            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{ mRenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, mRtvDescriptorSize };
    //            float const clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    //            mCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    //            mCommandList->ClearDepthStencilView(mDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    //            mCommandList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
    //        }
    //        /* Indicate that the back buffer will now be used to present. */
    //        barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    //            mRenderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT
    //        );
    //        mCommandList->ResourceBarrier(1, &barrier);
    //    }
    //    ThrowIfFailed(mCommandList->Close());
    //}

} // namespace gg
