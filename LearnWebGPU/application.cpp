#include "application.h"

#include <iostream>
#include <vector>
#include <cassert>
#include <numeric>

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
    toggles.chain.next          = nullptr;
    toggles.chain.sType         = WGPUSType_DawnTogglesDescriptor;
    toggles.disabledToggleCount = 0;
    toggles.enabledToggleCount  = 1;
    const char* toggle_name     = "enable_immediate_error_handling";
    toggles.enabledToggles      = &toggle_name;

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
    device_descriptor.nextInChain               = nullptr;
    device_descriptor.label                     = "My Device";
    device_descriptor.requiredFeatureCount      = 0;
    device_descriptor.requiredLimits            = nullptr;
    device_descriptor.defaultQueue.nextInChain  = nullptr;
    device_descriptor.defaultQueue.label        = "The default queue";
    device_descriptor.deviceLostCallback        = [](WGPUDeviceLostReason reason, const char* message, [[maybe_unused]] void* user_data)
        {
            std::cout << "Device lost : reason " << reason;
            if (message)
                std::cout << " (" << message << ")";
            std::cout << std::endl;
        };
    WGPURequiredLimits required_limits = GetWGPURequiredLimits(adapter);
    device_descriptor.requiredLimits = &required_limits;

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

    WGPUSupportedLimits supportedLimits{};
    supportedLimits.nextInChain = nullptr;
    wgpuAdapterGetLimits(adapter, &supportedLimits);
    std::cout << "adapter.maxVertexAttributes: " << supportedLimits.limits.maxVertexAttributes << std::endl;

    wgpuDeviceGetLimits(m_device, &supportedLimits);
    std::cout << "device.maxVertexAttributes: " << supportedLimits.limits.maxVertexAttributes << std::endl;
    std::cout << "device.maxVertexBuffers: " << supportedLimits.limits.maxVertexBuffers << std::endl;


    inspectDevice(m_device);

    // Create the queue
    m_queue = wgpuDeviceGetQueue(m_device);

    auto onQueueWorkDone = [](WGPUQueueWorkDoneStatus status, void* /* user_data */)
        {
            std::cout << "Queued work finished with status: " << status << std::endl;
        };
    wgpuQueueOnSubmittedWorkDone(m_queue, onQueueWorkDone, nullptr /* user_data */);

    m_surface_format = wgpuSurfaceGetPreferredFormat(m_surface, adapter);
    WGPUSurfaceConfiguration config = {};
    config.nextInChain      = nullptr;
    config.width            = 640;
    config.height           = 480;
    config.format           = m_surface_format;
    config.viewFormatCount  = 0;
    config.viewFormats      = nullptr;
    config.usage            = WGPUTextureUsage_RenderAttachment;
    config.device           = m_device;
    config.presentMode      = WGPUPresentMode_Fifo;
    config.alphaMode        = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(m_surface, &config);

    wgpuAdapterRelease(adapter);

    InitializePipeline();

    //PlayingWithBuffers();

    InitializeBuffers();

    return true;
}

bool Application::InitializePipeline()
{
    const char* shader_source =
        R"(
        struct VertexInput
        {
            @location(0) position: vec2f,
            @location(1) color: vec3f,
        }

        struct VertexOutput
        {
            @builtin(position) position: vec4f,
            @location(1) color: vec3f,
        }

        @vertex
        fn vs_main(in: VertexInput) -> VertexOutput
        {
            var out: VertexOutput;
            out.position = vec4f(in.position, 0.0, 1.0);
            out.color = in.color;
            return out;
        }

        @fragment
        fn fs_main(in: VertexOutput) -> @location(0) vec4f
        {
            return vec4f(in.color, 1.0);
        }
        )";

    WGPUShaderModuleDescriptor shader_descriptor{};

#ifdef WEBGPU_BACKEND_WGPU
    shader_descriptor.hintCount                             = 0;
    shader_descriptor.hints                                 = nullptr;
#endif // WEBGPU_BACKEND_WGPU

    WGPUShaderModuleWGSLDescriptor shader_code_descriptor{};
    shader_code_descriptor.chain.next                       = nullptr;
    shader_code_descriptor.chain.sType                      = WGPUSType_ShaderModuleWGSLDescriptor;
    shader_code_descriptor.code                             = shader_source;
    
    shader_descriptor.nextInChain = &shader_code_descriptor.chain;
    
    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(m_device, &shader_descriptor);

    WGPUVertexBufferLayout vertex_buffer_layout{};

    std::vector<WGPUVertexAttribute> vertex_attributes(2);
    // == For each attribute, describe its layout, i.e, how to interpret the raw data ==
    // Corresponds to @location(...)
    vertex_attributes[0].shaderLocation = 0; // @location(0)
    vertex_attributes[1].shaderLocation = 1; // @location(1)
    // Means vec2f in the shader
    vertex_attributes[0].format = WGPUVertexFormat_Float32x2;
    vertex_attributes[1].format = WGPUVertexFormat_Float32x3;
    // Index of the first element
    vertex_attributes[0].offset = 0;
    vertex_attributes[1].offset = 2 * sizeof(float);

    vertex_buffer_layout.attributeCount = vertex_attributes.size();
    vertex_buffer_layout.attributes     = vertex_attributes.data();
    vertex_buffer_layout.arrayStride    = 5 * sizeof(float);
    vertex_buffer_layout.stepMode       = WGPUVertexStepMode_Vertex;

    WGPURenderPipelineDescriptor pipeline_descriptor{};

    pipeline_descriptor.nextInChain                         = nullptr;

    pipeline_descriptor.vertex.bufferCount                  = 1;
    pipeline_descriptor.vertex.buffers                      = &vertex_buffer_layout;
    pipeline_descriptor.vertex.module                       = shader_module;
    pipeline_descriptor.vertex.entryPoint                   = "vs_main";
    pipeline_descriptor.vertex.constantCount                = 0;
    pipeline_descriptor.vertex.constants                    = nullptr;

    pipeline_descriptor.primitive.topology                  = WGPUPrimitiveTopology_TriangleList;
    pipeline_descriptor.primitive.stripIndexFormat          = WGPUIndexFormat_Undefined;
    pipeline_descriptor.primitive.frontFace                 = WGPUFrontFace_CCW;
    pipeline_descriptor.primitive.cullMode                  = WGPUCullMode_None;

    WGPUFragmentState fragment_state{};
    fragment_state.module                                   = shader_module;
    fragment_state.entryPoint                               = "fs_main";
    fragment_state.constantCount                            = 0;
    fragment_state.constants                                = nullptr;

    WGPUBlendState blend_state{};

    blend_state.color.srcFactor                             = WGPUBlendFactor_SrcAlpha;
    blend_state.color.dstFactor                             = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_state.color.operation                             = WGPUBlendOperation_Add;

    blend_state.alpha.srcFactor                             = WGPUBlendFactor_Zero;
    blend_state.alpha.dstFactor                             = WGPUBlendFactor_One;
    blend_state.alpha.operation                             = WGPUBlendOperation_Add;

    pipeline_descriptor.multisample.count                   = 1;
    pipeline_descriptor.multisample.mask                    = ~0u;
    pipeline_descriptor.multisample.alphaToCoverageEnabled  = false;

    WGPUColorTargetState color_target{};
    color_target.format                                     = m_surface_format;
    color_target.blend                                      = &blend_state;
    color_target.writeMask                                  = WGPUColorWriteMask_All;

    fragment_state.targetCount                              = 1;
    fragment_state.targets                                  = &color_target;

    pipeline_descriptor.fragment                            = &fragment_state;
    pipeline_descriptor.depthStencil                        = nullptr;

    pipeline_descriptor.layout                              = nullptr;

    m_pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipeline_descriptor);

    wgpuShaderModuleRelease(shader_module);

    return true;
}

bool Application::InitializeBuffers()
{
    // x, y, r, g, b
    std::vector<float> vertex_data =
    {
        -0.5f,  -0.5f, 1.0, 0.0, 0.0,
         0.5f,  -0.5f, 0.0, 1.0, 0.0,
         0.0f,   0.5f, 0.0, 0.0, 1.0,

        -0.55f, -0.5f, 1.0, 0.0, 0.0,
        -0.05f,  0.5f, 0.0, 1.0, 0.0,
        -0.55f,  0.5f, 0.0, 0.0, 1.0,

         0.55f, -0.5f, 1.0, 0.0, 0.0,
         0.05f,  0.5f, 0.0, 1.0, 0.0,
         0.55f,  0.5f, 0.0, 0.0, 1.0,
    };

    m_vertex_count = static_cast<std::uint32_t>(vertex_data.size() / 5);

    WGPUBufferDescriptor buffer_descriptor{};
    buffer_descriptor.nextInChain       = nullptr;
    buffer_descriptor.label             = "Vertex buffer";
    buffer_descriptor.usage             = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
    buffer_descriptor.size              = vertex_data.size() * sizeof(float);
    buffer_descriptor.mappedAtCreation  = false;

    m_vertex_buffer = wgpuDeviceCreateBuffer(m_device, &buffer_descriptor);

    wgpuQueueWriteBuffer(m_queue, m_vertex_buffer, 0, vertex_data.data(), buffer_descriptor.size);
    
    return true;
}

void Application::Terminate()
{
    // Move all the release/destroy/terminate calls here
    wgpuBufferRelease(m_vertex_buffer);
    wgpuRenderPipelineRelease(m_pipeline);
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
    render_pass_descriptor.nextInChain              = nullptr;

    WGPURenderPassColorAttachment render_pass_color_attachment = {};
    render_pass_color_attachment.view               = target_view;
    render_pass_color_attachment.resolveTarget      = nullptr;
    render_pass_color_attachment.loadOp             = WGPULoadOp_Clear;
    render_pass_color_attachment.storeOp            = WGPUStoreOp_Store;
    render_pass_color_attachment.clearValue         = WGPUColor{ 0.05, 0.05, 0.05, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
    render_pass_color_attachment.depthSlice         = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // !WEBGPU_BACKEND_WGPU

    render_pass_descriptor.colorAttachmentCount     = 1;
    render_pass_descriptor.colorAttachments         = &render_pass_color_attachment;
    render_pass_descriptor.depthStencilAttachment   = nullptr;
    render_pass_descriptor.timestampWrites          = nullptr;

    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_descriptor);

    wgpuRenderPassEncoderSetPipeline(render_pass, m_pipeline);

    wgpuRenderPassEncoderSetVertexBuffer(render_pass, 0, m_vertex_buffer, 0, wgpuBufferGetSize(m_vertex_buffer));

    wgpuRenderPassEncoderDraw(render_pass, m_vertex_count, 1, 0, 0);

    wgpuRenderPassEncoderEnd(render_pass);
    wgpuRenderPassEncoderRelease(render_pass);

    WGPUCommandBufferDescriptor command_buffer_descriptor   = {};
    command_buffer_descriptor.nextInChain                   = nullptr;
    command_buffer_descriptor.label                         = "Command buffer";
    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &command_buffer_descriptor);
    wgpuCommandEncoderRelease(encoder);

    // Submit the command queue
    //std::cout << "Submitting Command..." << std::endl;
    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);
    //std::cout << "Command submitted." << std::endl;

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
    view_descriptor.nextInChain     = nullptr;
    view_descriptor.label           = "Surface texture view";
    view_descriptor.format          = wgpuTextureGetFormat(surface_texture.texture);
    view_descriptor.dimension       = WGPUTextureViewDimension_2D;
    view_descriptor.baseMipLevel    = 0;
    view_descriptor.mipLevelCount   = 1;
    view_descriptor.baseArrayLayer  = 0;
    view_descriptor.arrayLayerCount = 1;
    view_descriptor.aspect          = WGPUTextureAspect_All;
    
    WGPUTextureView target_view = wgpuTextureCreateView(surface_texture.texture, &view_descriptor);

    #ifndef WEBGPU_BACKEND_WGPU
    // We no longer need the texture, only its view
    // (NB: with wgpu-native, surface textures must be release after the call to wgpuSurfacePresent)
    wgpuTextureRelease(surface_texture.texture);
    #endif // WEBGPU_BACKEND_WGPU

    return target_view;
}

WGPURequiredLimits Application::GetWGPURequiredLimits(WGPUAdapter adapter) const
{
    WGPUSupportedLimits supported_limits{};
    supported_limits.nextInChain = nullptr;
    wgpuAdapterGetLimits(adapter, &supported_limits);
    
    WGPURequiredLimits required_limits{};
    required_limits.limits = GetDefaultLimits();

    // These two limits are different because they are "minimum" limits,
    // they are the only ones we may forward from the adapter's supported
    // limits.
    required_limits.limits.minUniformBufferOffsetAlignment = supported_limits.limits.minUniformBufferOffsetAlignment;
    required_limits.limits.minStorageBufferOffsetAlignment = supported_limits.limits.minStorageBufferOffsetAlignment;

    // We use at most 2 vertex attribute for now
    required_limits.limits.maxVertexAttributes = 2;
    // We should also tell that we use 1 vertex buffers
    required_limits.limits.maxVertexBuffers = 1;
    // Maximum size of a buffer is 6 vertices of 2 float each
    required_limits.limits.maxBufferSize = 9 * 5 * sizeof(float);
    // Maximum stride between 2 consecutive vertices in the vertex buffer
    required_limits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);

    // There is a maximum of 3 float (color) forwarded from vertex to fragment shader
    required_limits.limits.maxInterStageShaderComponents = 3;

    return required_limits;
}

bool Application::PlayingWithBuffers()
{
    WGPUBufferDescriptor buffer_descriptor{};
    buffer_descriptor.nextInChain = nullptr;
    buffer_descriptor.label = "Some GPU-side data buffer";
    buffer_descriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;
    buffer_descriptor.size = 16;
    buffer_descriptor.mappedAtCreation = false;

    WGPUBuffer buffer_1 = wgpuDeviceCreateBuffer(m_device, &buffer_descriptor);

    buffer_descriptor.label = "Output buffer";
    buffer_descriptor.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer buffer_2 = wgpuDeviceCreateBuffer(m_device, &buffer_descriptor);

    std::vector<std::uint8_t> numbers(16);
    std::iota(numbers.begin(), numbers.end(), static_cast<std::uint8_t>(0));

    wgpuQueueWriteBuffer(m_queue, buffer_1, 0, numbers.data(), numbers.size());

    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(m_device, nullptr);

    wgpuCommandEncoderCopyBufferToBuffer(encoder, buffer_1, 0, buffer_2, 0, 16);

    WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(m_queue, 1, &command);
    wgpuCommandBufferRelease(command);

    struct Context
    {
        bool ready = false;
        WGPUBuffer buffer;
    };

    auto onBuffer2Mapped = [](WGPUBufferMapAsyncStatus status, void* user_data)
        {
            Context* context = reinterpret_cast<Context*>(user_data);
            context->ready = true;
            std::cout << "Buffer 2 mapped with status " << status << std::endl;
            if (status != WGPUBufferMapAsyncStatus_Success)
            {
                return;
            }

            uint8_t* buffer_data = (uint8_t*)wgpuBufferGetConstMappedRange(context->buffer, 0, 16);

            std::cout << "bufferData = [";
            for (int i = 0; i < 16; ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << (int)buffer_data[i];
            }
            std::cout << "]" << std::endl;

            wgpuBufferUnmap(context->buffer);
        };

    Context context = { false, buffer_2 };

    wgpuBufferMapAsync(buffer_2, WGPUMapMode_Read, 0, 16, onBuffer2Mapped, (void*)&context);

    while (!context.ready)
    {
        wgpuPollEvents(m_device, true);
    }

    wgpuBufferRelease(buffer_1);
    wgpuBufferRelease(buffer_2);

    return true;
}