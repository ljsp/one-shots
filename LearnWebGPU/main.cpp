#include <webgpu/webgpu.h>
#include "webgpu-utils.h"

#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif //WEBGPU_BACKEND_WGPU 

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>

int main()
{
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
        return 1;
    }

    std::cout << "WGPU instance : " << instance << std::endl;

    // Create the adapter
    std::cout << "Requesting adapter..." << std::endl;

    WGPURequestAdapterOptions adapter_options = {};
    adapter_options.nextInChain = nullptr;
    WGPUAdapter adapter = requestAdapterSync(instance, &adapter_options);

    std::cout << "Got adapter: " << adapter << std::endl;
    wgpuInstanceRelease(instance);

    inspectAdapter(adapter);

    // Create the device
    std::cout << "Requesting device..." << std::endl;

    WGPUDeviceDescriptor device_descriptor     = {};
    device_descriptor.nextInChain              = nullptr;
    device_descriptor.label                    = "My Device";
    device_descriptor.requiredFeatureCount     = 0;
    device_descriptor.requiredLimits           = nullptr;
    device_descriptor.defaultQueue.nextInChain = nullptr;
    device_descriptor.defaultQueue.label       = "The default queue";
    device_descriptor.deviceLostCallback       = [](WGPUDeviceLostReason reason, const char* message, [[maybe_unused]] void* user_data)
        {
            std::cout << "Device lost : reason " << reason;
            if (message) 
                std::cout << " (" << message << ")";
            std::cout << std::endl;
        };

    WGPUDevice device = requestDeviceSync(adapter, &device_descriptor);

    std::cout << "Got device: " << device << std::endl;

    wgpuAdapterRelease(adapter);

    auto onDeviceError = [](WGPUErrorType type, const char* message, [[maybe_unused]] void* user_data)
    {
        std::cout << "Uncaptured device error: type " << type;
        if (message)
            std::cout << " (" << message << ")";
        std::cout << std::endl;
    };

    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr /*user_data*/);

    inspectDevice(device);
    
    // Create the queue
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /* user_data */)
        {
            std::cout << "Queued work finished with status: " << status << std::endl;
        };
    wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr /* user_data */);

    // Create the encoder
    WGPUCommandEncoderDescriptor encoder_descriptor = {};
    encoder_descriptor.nextInChain = nullptr;
    encoder_descriptor.label = "My command encoder";
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoder_descriptor);

    wgpuCommandEncoderInsertDebugMarker(encoder, "Do one thing");
    wgpuCommandEncoderInsertDebugMarker(encoder, "Do another thing");

    // Create the command buffer
    WGPUCommandBufferDescriptor command_buffer_descriptor = {};
    command_buffer_descriptor.nextInChain = nullptr;
    command_buffer_descriptor.label = "Command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &command_buffer_descriptor);
    wgpuCommandEncoderRelease(encoder);

    std::cout << "Submitting command..." << std::endl;
    wgpuQueueSubmit(queue, 1, &command);
    wgpuCommandBufferRelease(command);
    std::cout << "Command submitted." << std::endl;

    for (int i = 0; i < 5; i++)
    {
        std::cout << "Tick/Poll device..." << std::endl;
    #if defined(WEBGPU_BACKEND_DAWN)
        wgpuDeviceTick(device);
    #elif defined(WEBGPU_BACKEND_WGPU)
        wgpuDevicePoll(device, false, nullptr);
    #elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
        emscripten_sleep(100);
    #endif
    }

    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
}