#include <vector>
#include <cstddef>
#include <fstream>
#include <string_view>
#include <ranges>
#include <print>
#include <expected>
#include <span>
#include <cctype> // for std::isprint

enum class ErrorCode 
{
    FileNotFound,
};

void print_error(const ErrorCode& error) 
{
    switch (error) 
    {
        case ErrorCode::FileNotFound:
            std::println("Error: File not found.");
            break;
        default:
            std::println("Error: Unknown error.");
            break;
    }
}

std::expected<std::vector<std::byte>, ErrorCode> load_binary_file(const std::string& path) 
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) 
    {
        return std::unexpected(ErrorCode::FileNotFound);
    }

    std::streamsize size = file.tellg();
    file.seekg(0);

    std::vector<std::byte> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return buffer;
}

void print_mnemonics(std::span<const std::byte> binary_data) noexcept
{
    std::println("bits 16");
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
            std::println("mov {}, {}", reg_name, rm_name);
        } 
        else 
        {
            std::println("mov {}, {}", rm_name, reg_name);
        }
    }
}

void print_binary(std::span<const std::byte> binary_data)
{
    std::print(";Binary representation of the file: \n;");
    for (const auto row : binary_data | std::views::chunk(8)) 
    {
        for (const std::byte byte : row)
        {
            std::print("{:08b} ", std::to_integer<unsigned>(byte));
        }

        if (row.size() % 8 == 0) 
        {
            std::print("\n;");
        }
    }
}

void print_hex(std::span<const std::byte> data)
{
    std::println(";Hexadecimal representation of the file:");

    std::size_t offset = 0;

    for (auto row : data | std::views::chunk(16)) 
    {
        std::print(";{:06x}: ", offset);

        for (const std::byte b : row) 
        {
            std::print("{:02x} ", std::to_integer<unsigned>(b));
        }

        if (row.size() < 16) 
        {
            for (std::size_t i = row.size(); i < 16; ++i) 
            {
                std::print("   "); // 3 spaces: "?? "
            }
        }

        // Print ASCII representation
        std::print(" |");

        for (const std::byte b : row) 
        {
            unsigned char c = std::to_integer<unsigned char>(b);
            std::print("{}", std::isprint(c) ? char(c) : '.');
        }

        std::println("|");

        offset += row.size();
    }
}

int main(int argc, char** argv)
{
    if (argc == 1)
    {
        std::println("No file path given");
        return 0;
    }
    
    auto loaded_binary = load_binary_file(argv[1]);
    if (!loaded_binary) 
    {
        print_error(loaded_binary.error());
        return 1;
    }
    
    auto& binary_data = loaded_binary.value();

    std::println("; {} disassembly : \n", argv[1]);
    print_binary(binary_data);
    std::println("\n");
    print_hex(binary_data);
    std::println();
    print_mnemonics(binary_data);

    return 0;
}