module;
#include <algorithm>
#include <cstdint>
#include <DirectXMath.h>
#include <format>
#include <fstream>
#include <glm/glm.hpp>
#include <optional>
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

    static std::vector<char> readFile(const std::string& filename) 
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open()) 
        {
            throw std::runtime_error("failed to open file!");
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
        if (vkCreateShaderModule(mVkDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

    VulkanRenderer::VulkanRenderer(uint32_t width, uint32_t height, SDL_Window* windowHandle)
        : mWidth{ width }
        , mHeight{ height }
        , mWindowHandle{ windowHandle }
        , mVkPhysicalDevice{ VK_NULL_HANDLE }
        //, mScissorRect{ D3D12_DEFAULT_SCISSOR_STARTX, D3D12_DEFAULT_SCISSOR_STARTY, D3D12_VIEWPORT_BOUNDS_MAX, D3D12_VIEWPORT_BOUNDS_MAX }
        //, mViewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f }
        , mCamera{ std::make_unique<Camera>() }
    {

        uint32_t extension_count;
        if (!SDL_Vulkan_GetInstanceExtensions(windowHandle, &extension_count, nullptr)) {
            throw std::exception("Could not get the number of required instance extensions from SDL.");
        }
        std::vector<char const*> extensions(extension_count);
        if (!SDL_Vulkan_GetInstanceExtensions(windowHandle, &extension_count, extensions.data())) {
            throw std::exception("Could not get the names of required instance extensions from SDL.");
        }

        std::vector<char const*> layers;
        if constexpr (IsDebug())
        { /* Enable the debug layer */
            layers.emplace_back("VK_LAYER_KHRONOS_validation");
        }

        CreateVkInstance(layers, extensions);
        SelectPhysicalDevice();
        CreateLogicalDevice();

        __debugbreak(); // TODO: make sure the surface is create at the right time

        // Create a Vulkan surface for rendering
        if (!SDL_Vulkan_CreateSurface(windowHandle, mVkInstance, &mVkSurface)) {
            throw std::exception("Could not create a Vulkan surface.");
        }

        /* Create render targets */
        ResizeRenderTargets();
        /* Create depth buffer */
        ResizeDepthBuffer();

        /* Read shaders */
        {
            std::vector<char> vertexShaderBlob = readFile("shaders//colored_surface_VS.spv");
            std::vector<char> pixelShaderBlob = readFile("shaders//colored_surface_PS.spv");
            mVertexShader = createShaderModule(vertexShaderBlob);
            mPixelShader = createShaderModule(pixelShaderBlob);
        }
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

        VkResult const result = vkCreateInstance(&instInfo, nullptr, &mVkInstance);
        if (VK_ERROR_INCOMPATIBLE_DRIVER == result)
        {
            throw std::exception("Unable to find a compatible Vulkan Driver.");
        }
        else if (result)
        {
            throw std::exception("Could not create a Vulkan instance (for unknown reasons).");
        }
    }

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        bool IsComplete() const { return graphicsFamily.has_value(); }
    };

    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice const device)
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
            if (indices.IsComplete())
                break;
            i++;
        }
        return indices;
    }

    static bool IsDeviceSuitable(VkPhysicalDevice const device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == deviceProperties.deviceType
            && FindQueueFamilies(device).IsComplete();
    }

    void VulkanRenderer::SelectPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(mVkInstance, &deviceCount, nullptr);
        if (0 == deviceCount)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(mVkInstance, &deviceCount, devices.data());
        for (auto const & d : devices)
        {
            if (IsDeviceSuitable(d))
            {
                mVkPhysicalDevice = d;
                break;
            }
        }
        if (VK_NULL_HANDLE == mVkPhysicalDevice) 
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void VulkanRenderer::CreateLogicalDevice()
    {
        QueueFamilyIndices indices = FindQueueFamilies(mVkPhysicalDevice);

        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;
        if (VK_SUCCESS != vkCreateDevice(mVkPhysicalDevice, &createInfo, nullptr, &mVkDevice))
        {
            throw std::runtime_error("failed to create logical device!");
        }
        vkGetDeviceQueue(mVkDevice, indices.graphicsFamily.value(), 0, &mVkGraphicsQueue);
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

        vkDestroySurfaceKHR(mVkInstance, mVkSurface, nullptr);
        SDL_DestroyWindow(mWindowHandle);
        SDL_Quit();
        vkDestroyDevice(mVkDevice, nullptr);
        vkDestroyInstance(mVkInstance, nullptr);
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
