#include <vector>
#include <cstddef>
#include <fstream>
#include <cstdint>
#include <string_view>
#include <ranges>
#include <print>
#include <bitset>
#include <iostream>

std::ostream& operator<<(std::ostream& os, std::byte b)
{
    return os << std::bitset<8>(std::to_integer<int>(b));
}

std::vector<std::byte> load_binary(const std::string& path) 
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) 
    {
        std::cout << "Could not open file" << std::endl;
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0);

    std::vector<std::byte> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return buffer;
}

void print_mnemonics(const std::vector<std::byte>& binary_data) noexcept
{
    for (const auto& chunk : binary_data | std::views::chunk(2)) 
    {
        if (chunk.size() < 2) 
        {
            continue; // Skip incomplete instruction
        }

        std::byte byte_one = chunk[0];
        std::byte byte_two = chunk[1];

        int opcode = (std::to_integer<int>(byte_one) >> 2) & 0x3F;

        if (opcode != 0b100010) 
        {
            return; // Parse only MOV instructions
        }

        bool direction  =  std::to_integer<int>(byte_one) & 0x2;
        bool w          =  std::to_integer<int>(byte_one) & 0x1;
        int mod         = (std::to_integer<int>(byte_two) >> 6) & 0x3;
        int reg         = (std::to_integer<int>(byte_two) >> 3) & 0x7;
        int rm          =  std::to_integer<int>(byte_two) & 0x7;

        static constexpr std::string_view reg_names_8bit[]  = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
        static constexpr std::string_view reg_names_16bit[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};

        std::string_view reg_name = w ? reg_names_16bit[reg] : reg_names_8bit[reg];
        std::string_view rm_name  = w ? reg_names_16bit[rm] : reg_names_8bit[rm];

        if (direction) 
        {
            std::cout << "mov " << reg_name << ", " << rm_name << std::endl;
        } 
        else 
        {
            std::cout << "mov " << rm_name << ", " << reg_name << std::endl;
        }
    }
}

void print_binary(const std::vector<std::byte>& binary_data)
{
    for (const auto& byte : binary_data) 
    {
        std::cout << byte << " ";
    }
    std::cout << std::endl;
}

void print_hex(const std::vector<std::byte>& binary_data)
{
    for (size_t i = 0; i < binary_data.size(); ++i) 
    {
        std::cout << "0x" << std::hex << static_cast<int>(binary_data[i]) << " ";
        if ((i + 1) % 16 == 0) 
        {
            std::cout << std::endl;
        }
    }
    std::cout << std::dec << std::endl;
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        std::cout << ("No file path given") << std::endl;
        return 0;
    }

    std::cout << "; " << argv[1] << " disassembly : \n";
    std::cout << "bits 16\n";

    std::vector<std::byte> binary_data = load_binary(argv[1]);

    // print_binary(binary_data);
    // print_hex(binary_data);
    print_mnemonics(binary_data);

    return 0;
}