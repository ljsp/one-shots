#include "application.h"

#include <iostream>
#include <vector>
#include <cassert>

bool Application::Initialize()
{
    // GLFW Initialize
    if (!glfwInit())
    {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // <-- extra info for glfwCreateWindow
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);

    if (!m_window)
    {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        return false;
    }

    // WEBGPU Initialize

    // Create a descriptor
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr;

#ifdef WEBGPU_BACKEND_DAWN
    //Make sure the uncaptured error callback is called as soon as an error
    // occurs rather than at the next call to "wgpuDeviceTick".
    WGPUDawnTogglesDescriptor toggles;
    toggles.chain.next = nullptr;
    toggles.chain.sType = WGPUSType_DawnTogglesDescriptor;
    toggles.disabledToggleCount = 0;
    toggles.enabledToggleCount = 1;
    const char* toggle_name = "enable_immediate_error_handling";
    toggles.enabledToggles = &toggle_name;

    desc.nextInChain = &toggles.chain;
#endif // WEBGPU_BACKEND_DAWN

    // Create the instance
#if WEBGPU_BACKEND_EMSCRIPTEN
    WGPUInstance instance = wgpuCreateInstance(nullptr);
#else
    WGPUInstance instance = wgpuCreateInstance(&desc);
#endif // WEBGPU_BACKEND_EMSCRIPTEN

    if (!instance)
    {
        std::cerr << "Could not initialize WebGPU." << std::endl;
        return false;
    }

    std::cout << "WGPU instance : " << instance << std::endl;

    // Create the adapter
    std::cout << "Requesting adapter..." << std::endl;

    m_surface = glfwGetWGPUSurface(instance, m_window);

    WGPURequestAdapterOptions adapter_options = {};
    adapter_options.nextInChain = nullptr;
    WGPUAdapter adapter = requestAdapterSync(instance, &adapter_options);

    std::cout << "Got adapter: " << adapter << std::endl;
    wgpuInstanceRelease(instance);

    inspectAdapter(adapter);



    // Create the device
    std::cout << "Requesting device..." << std::endl;

    WGPUDeviceDescriptor device_descriptor = {};
    device_descriptor.nextInChain = nullptr;
    device_descriptor.label = "My Device";
    device_descriptor.requiredFeatureCount = 0;
    device_descriptor.requiredLimits = nullptr;
    device_descriptor.defaultQueue.nextInChain = nullptr;
    device_descriptor.defaultQueue.label = "The default queue";
    device_descriptor.deviceLostCallback = [](WGPUDeviceLostReason reason, const char* message, [[maybe_unused]] void* user_data)
        {
            std::cout << "Device lost : reason " << reason;
            if (message)
                std::cout << " (" << message << ")";
            std::cout << std::endl;
        };

    m_device = requestDeviceSync(adapter, &device_descriptor);

    std::cout << "Got device: " << m_device << std::endl;

    auto onDeviceError = [](WGPUErrorType type, const char* message, [[maybe_unused]] void* user_data)
        {
            std::cout << "Uncaptured device error: type " << type;
            if (message)
                std::cout << " (" << message << ")";
            std::cout << std::endl;
        };

    wgpuDeviceSetUncapturedErrorCallback(m_device, onDeviceError, nullptr /*user_data*/);

    inspectDevice(m_device);

    // Create the queue
    m_queue = wgpuDeviceGetQueue(m_device);

    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /* user_data */)
        {
            std::cout << "Queued work finished with status: " << status << std::endl;
        };
    wgpuQueueOnSubmittedWorkDone(m_queue, onQueueWorkDone, nullptr /* user_data */);

    WGPUTextureFormat surface_format = wgpuSurfaceGetPreferredFormat(m_surface, adapter);
    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.width = 640;
    config.height = 480;
    config.format = surface_format;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.device = m_device;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(m_surface, &config);

    wgpuAdapterRelease(adapter);

    return true;
}

void Application::Terminate()
{
    // Move all the release/destroy/terminate calls here
    wgpuQueueRelease(m_queue);
    wgpuSurfaceUnconfigure(m_surface);
    wgpuSurfaceRelease(m_surface);
    wgpuDeviceRelease(m_device);
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Application::MainLoop()
{
    glfwPollEvents();

    WGPUTextureView target_view = GetNextSurfaceViewData();
    
    if (!target_view)
        return;

    WGPUCommandEncoderDescriptor encoder_descriptor = {};
    encoder_descriptor.nextInChain = nullptr;
    encoder_descriptor.label = "My command encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, &encoder_descriptor);

    WGPURenderPassDescriptor render_pass_descriptor = {};
    render_pass_descriptor.nextInChain = nullptr;

    WGPURenderPassColorAttachment render_pass_color_attachment = {};
    render_pass_color_attachment.view = target_view;
    render_pass_color_attachment.resolveTarget = nullptr;
    render_pass_color_attachment.loadOp = WGPULoadOp_Clear;
    render_pass_color_attachment.storeOp = WGPUStoreOp_Store;
    render_pass_color_attachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
    render_pass_color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // !WEBGPU_BACKEND_WGPU

    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &render_pass_color_attachment;
    render_pass_descriptor.depthStencilAttachment = nullptr;
    render_pass_descriptor.timestampWrites = nullptr;

    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_descriptor);

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);

    WGPUCommandBufferDescriptor command_buffer_descriptor = {};
    command_buffer_descriptor.nextInChain = nullptr;
    command_buffer_descriptor.label = "Command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &command_buffer_descriptor);
    wgpuCommandEncoderRelease(encoder);

    // Submit the command queue
    std::cout << "Submitting Command..." << std::endl;
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);
    std::cout << "Command submitted." << std::endl;


    wgpuTextureViewRelease(target_view);
#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(m_surface);
#endif // !__EMSCRIPTEN__

#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(m_device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(m_device, false, nullptr);
#endif
}

bool Application::IsRunning()
{
    return !glfwWindowShouldClose(m_window);
}

WGPUTextureView Application::GetNextSurfaceViewData()
{
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(m_surface, &surface_texture);

    if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_Success)
    {
        return nullptr;
    }

    WGPUTextureViewDescriptor view_descriptor;
    view_descriptor.nextInChain = nullptr;
    view_descriptor.label = "Surface texture view";
    view_descriptor.format = wgpuTextureGetFormat(surface_texture.texture);
    view_descriptor.dimension = WGPUTextureViewDimension_2D;
    view_descriptor.baseMipLevel = 0;
    view_descriptor.mipLevelCount = 1;
    view_descriptor.baseArrayLayer = 0;
    view_descriptor.arrayLayerCount = 1;
    view_descriptor.aspect = WGPUTextureAspect_All;
    WGPUTextureView target_view = wgpuTextureCreateView(surface_texture.texture, &view_descriptor);

    #ifndef WEBGPU_BACKEND_WGPU
    // We no longer need the texture, only its view
    // (NB: with wgpu-native, surface textures must be release after the call to wgpuSurfacePresent)
    wgpuTextureRelease(surface_texture.texture);
    #endif // WEBGPU_BACKEND_WGPU

    return target_view;
}