#pragma once 

#include <webgpu/webgpu.h>

#include <vector>
#include <filesystem>

class ResourceManager
{
public:
    static WGPUShaderModule LoadShaderModule
    (
        WGPUDevice device,
        const std::filesystem::path& path
    );

    static bool LoadGeometry
    (
        const std::filesystem::path& path,
        std::vector<float>& point_data,
        std::vector<uint16_t>& index_data
    );
};