#include <cstddef>
#include <fstream>
#include <string_view>
#include <ranges>
#include <print>
#include <expected>
#include <cctype> // for std::isprint
#include <array>
#include <utility>
#include <vector>
#include <span>

enum class ErrorCode 
{
    FileNotFound,
    FileReadError,
};

enum class Instruction
{
    MOV_Register_Or_Memory_To_Register = 0b100010,
    MOV_Immediate_To_Register_Or_Memory = 0b1011,
};

enum class ModeFieldEncoding
{
    Memory_Mode_No_Displacement     = 0b00,
    Memory_Mode_8_Bit_Displacement  = 0b01,
    Memory_Mode_16_Bit_Displacement = 0b10,
    Register_Mode                   = 0b11,
};

void print_error(const ErrorCode& error) noexcept
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

[[nodiscard]] std::expected<std::vector<std::byte>, ErrorCode> load_binary_file(const std::string& path) 
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) 
    {
        return std::unexpected(ErrorCode::FileNotFound);
    }

    std::streamsize size = file.tellg();
    if (size < 0)
    {
        return std::unexpected(ErrorCode::FileNotFound);
    }

    file.seekg(0, std::ios::beg);

    std::vector<std::byte> buffer(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        return std::unexpected(ErrorCode::FileReadError);
    }

    return buffer;
}

static constexpr std::array<std::string_view, 8> reg_names_8bit = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
static constexpr std::array<std::string_view, 8> reg_names_16bit = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di" };
static constexpr std::array<std::string_view, 8> reg_adress_calculation = { "bx + si", "bx + di", "bp + si", "bp + di",  "si",  "di",  "bp",  "bx" };

std::pair<std::string, size_t> decode_mode_field(std::span<const std::byte> binary_data, size_t offset, bool w, int mod, int rm)
{
    if (mod == std::to_underlying(ModeFieldEncoding::Register_Mode))
    {
        std::string rm_name(w ? reg_names_16bit[rm] : reg_names_8bit[rm]);
        return {rm_name, 2 };
    }

    if (mod == std::to_underlying(ModeFieldEncoding::Memory_Mode_No_Displacement) && rm != 0b110)
    {
        return { std::format("[{}]", reg_adress_calculation[rm]), 2 };
    }

    std::byte byte_three = binary_data[offset + 2];
    if (mod == std::to_underlying(ModeFieldEncoding::Memory_Mode_8_Bit_Displacement))
    {
        return { std::format("[{} + {}]", reg_adress_calculation[rm], std::to_integer<int>(byte_three)), 3};
    }

    std::byte byte_four = binary_data[offset + 3];
    int displacement = std::to_integer<int>(byte_three) | (std::to_integer<int>(byte_four) << 8);

    if (rm == 0b110 && mod == std::to_underlying(ModeFieldEncoding::Memory_Mode_No_Displacement))
    {
        return { std::format("[{}]", displacement), 4 };
    }

    if (mod == std::to_underlying(ModeFieldEncoding::Memory_Mode_16_Bit_Displacement))
    {
        return { std::format("[{} + {}]", reg_adress_calculation[rm], displacement), 4 };
    }

    return {"", 2};
}

size_t decode_register_memory_instruction(std::span<const std::byte> binary_data, size_t offset)
{
    std::byte byte_one = binary_data[offset];
    std::byte byte_two = binary_data[offset + 1];

    bool direction  =  std::to_integer<int>(byte_one) & 0x2;
    bool w          =  std::to_integer<int>(byte_one) & 0x1;

    int mod         = (std::to_integer<int>(byte_two) >> 6) & 0x3;
    int reg         = (std::to_integer<int>(byte_two) >> 3) & 0x7;
    int rm          =  std::to_integer<int>(byte_two) & 0x7;

    std::string_view reg_name = w ? reg_names_16bit[reg] : reg_names_8bit[reg];

    const auto& [rm_name, nb_bytes_read] = decode_mode_field(binary_data, offset, w, mod, rm);

    if (direction)
    {
        std::println("mov {}, {}", reg_name, rm_name);
    }
    else
    {
        std::println("mov {}, {}", rm_name, reg_name);
    }

	return nb_bytes_read;
}

size_t decode_immediate_to_register_instruction(std::span<const std::byte> data, size_t offset)
{
	size_t nb_bytes_read = 0;
    std::byte byte_one = data[offset];
    std::byte byte_two = data[offset + 1];

    bool w = (std::to_integer<int>(byte_one) >> 3) & 0x1;
    int reg = std::to_integer<int>(byte_one) & 0x7;

    std::string_view reg_name = w ? reg_names_16bit[reg] : reg_names_8bit[reg];

    int immediate;
    if (w)
    {
        std::byte byte_three = data[offset + 2];
        immediate = std::to_integer<int>(byte_two) | (std::to_integer<int>(byte_three) << 8);
        nb_bytes_read = 3;
    }
    else
    {
        immediate = std::to_integer<int>(byte_two);
        nb_bytes_read = 2;
    }

    std::println("mov {}, {}", reg_name, immediate);
    
    return nb_bytes_read;
}

void print_mnemonics(std::span<const std::byte> binary_data) noexcept
{   
    std::println("bits 16");
    
    for (size_t offset = 0; offset < binary_data.size();) 
    {
        std::byte byte_one = binary_data[offset];

        int opcode = (std::to_integer<std::uint8_t>(byte_one) >> 2) & 0x3F;

        if (opcode == std::to_underlying(Instruction::MOV_Register_Or_Memory_To_Register)) 
        {
            offset += decode_register_memory_instruction(binary_data, offset);
        }
        else if (opcode >> 2 == std::to_underlying(Instruction::MOV_Immediate_To_Register_Or_Memory))
        {
            offset += decode_immediate_to_register_instruction(binary_data, offset);
        }
        else 
        {
            return; // Parse only MOV instructions
        }
    }
}

void print_binary(std::span<const std::byte> binary_data)
{
    std::print(";Binary representation of the file: \n;");
    for (const auto& row : binary_data | std::views::chunk(8)) 
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

    for (const auto& row : data | std::views::chunk(16)) 
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
    
    const std::string filename = argv[1];
    auto loaded_binary = load_binary_file(filename);
    if (!loaded_binary) 
    {
        print_error(loaded_binary.error());
        std::println("Given path : {}", filename);
        return 1;
    }
    
    std::vector<std::byte>& binary_data = loaded_binary.value();

    std::println("; {} disassembly : \n", filename);
    print_binary(binary_data);

    std::println("\n");
    print_hex(binary_data);
    
    std::println();
    print_mnemonics(binary_data);

    return 0;
}