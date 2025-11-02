#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <webgpu/webgpu.h>
#include "webgpu-utils.h"

#ifdef WEBGPU_BACKEND_WGPU
#include <webgpu/wgpu.h>
#endif //WEBGPU_BACKEND_WGPU 

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <cstdint>

class Application
{
public:
    // Initialize everything and return true if it went all right
    bool Initialize();

    // Uninitialize everything that was initialized
    void Terminate();

    // Draw a frame and handle events
    void MainLoop();

    // Return true as long as the main loop should keep on running
    bool IsRunning();

private:
    WGPUTextureView GetNextSurfaceViewData();
    WGPURequiredLimits GetWGPURequiredLimits(WGPUAdapter adapter) const;

    bool InitializePipeline();
    bool InitializeBuffers();
    bool PlayingWithBuffers();

    // We put here all the variables that are shared between init and main loop
    GLFWwindow*         m_window;
    WGPUDevice          m_device;
    WGPUQueue           m_queue;
    WGPURenderPipeline  m_pipeline;
    WGPUSurface         m_surface;
    WGPUTextureFormat   m_surface_format;

    std::uint32_t       m_vertex_count{};
    WGPUBuffer          m_vertex_buffer;
};