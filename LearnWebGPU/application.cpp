#include "application.h"

#include <iostream>
#include <vector>
#include <cassert>

bool Application::Initialize()
{
    // Move the whole initialization here
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

    wgpuAdapterRelease(adapter);

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

    return true;
}

void Application::Terminate()
{
    // Move all the release/destroy/terminate calls here
    wgpuQueueRelease(m_queue);
    wgpuSurfaceRelease(m_surface);
    wgpuDeviceRelease(m_device);
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Application::MainLoop()
{
    glfwPollEvents();

    // Also move here the tick/poll but NOT the emscripten sleep
#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(m_device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device, false, nullptr);
#endif
}

bool Application::IsRunning()
{
    return !glfwWindowShouldClose(m_window);
}