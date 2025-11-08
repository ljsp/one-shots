#include "ressource-manager.h"

#include <fstream>
#include <sstream>
#include <string>

WGPUShaderModule ResourceManager::LoadShaderModule(WGPUDevice device, const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string shader_source(size, ' ');
    file.seekg(0);
    file.read(shader_source.data(), size);

    WGPUShaderModuleWGSLDescriptor shader_code_descriptor{};
    shader_code_descriptor.chain.next = nullptr;
    shader_code_descriptor.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    shader_code_descriptor.code = shader_source.c_str();

    WGPUShaderModuleDescriptor shader_descriptor{};
    shader_descriptor.nextInChain = nullptr;
#ifdef WEBGPU_BACKEND_WGPU
    shader_descriptor.hintCount = 0;
    shader_descriptor.hints = nullptr;
#endif// WEBGPU_BACKEND_WGPU
    shader_descriptor.nextInChain = &shader_code_descriptor.chain;
    return wgpuDeviceCreateShaderModule(device, &shader_descriptor);
}

bool ResourceManager::LoadGeometry
(
    const std::filesystem::path& path,
    std::vector<float>& point_data,
    std::vector<uint16_t>& index_data
)
{
    std::ifstream file(path);
    if (!file.is_open()) 
    {
        return false;
    }

    point_data.clear();
    index_data.clear();

    enum class Section 
    {
        None,
        Points,
        Indices,
    };
    Section currentSection = Section::None;

    float value;
    uint16_t index;
    std::string line;
    while (!file.eof())
    {
        getline(file, line);

        // overcome the `CRLF` problem
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line == "[points]")
        {
            currentSection = Section::Points;
        }
        else if (line == "[indices]")
        {
            currentSection = Section::Indices;
        }
        else if (line[0] == '#' || line.empty())
        {
            // Do nothing, this is a comment
        }
        else if (currentSection == Section::Points)
        {
            std::istringstream iss(line);
            // Get x, y, r, g, b
            for (int i = 0; i < 5; ++i)
            {
                iss >> value;
                point_data.push_back(value);
            }
        }
        else if (currentSection == Section::Indices)
        {
            std::istringstream iss(line);
            // Get corners #0 #1 and #2
            for (int i = 0; i < 3; ++i)
            {
                iss >> index;
                index_data.push_back(index);
            }
        }
    }
    return true;
}