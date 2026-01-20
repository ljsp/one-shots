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

enum class InstructionEncoding
{
    Register_Or_Memory_From_Or_To_Register = 0b100010,
    Immediate_To_Register_Or_Memory
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
    file.seekg(0);

    std::vector<std::byte> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return buffer;
}

void print_mnemonics(std::span<const std::byte> binary_data) noexcept
{
    static constexpr std::string_view reg_names_8bit[]  = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
    static constexpr std::string_view reg_names_16bit[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
    static constexpr std::string_view reg_adress_calculation[]  = { "bx + si", "bx + di", "bp + si", "bp + di",  "si",  "di",  "bp",  "bx" };

    std::println("bits 16");
    
    int nb_bytes_read = 1;
    for (size_t i = 0; i < binary_data.size(); i += nb_bytes_read) 
    {
        std::byte byte_one = binary_data[i];
        std::byte byte_two = binary_data[i + 1];

        int opcode = (std::to_integer<std::uint8_t>(byte_one) >> 2) & 0x3F;

        if (opcode == static_cast<int>(InstructionEncoding::Register_Or_Memory_From_Or_To_Register)) 
        {
            bool direction  =  std::to_integer<int>(byte_one) & 0x2;
            bool w          =  std::to_integer<int>(byte_one) & 0x1;
            int mod         = (std::to_integer<int>(byte_two) >> 6) & 0x3;
            int reg         = (std::to_integer<int>(byte_two) >> 3) & 0x7;
            int rm          =  std::to_integer<int>(byte_two) & 0x7;

            std::string_view reg_name = w ? reg_names_16bit[reg] : reg_names_8bit[reg];
            std::string rm_name;

            if (mod == static_cast<int>(ModeFieldEncoding::Register_Mode))
            {
                rm_name = w ? reg_names_16bit[rm]  : reg_names_8bit[rm];
                nb_bytes_read = 2;
            }
            else
            {
                if (mod == static_cast<int>(ModeFieldEncoding::Memory_Mode_No_Displacement) && rm != 0b110)
                {
                    rm_name = std::format("[{}]", reg_adress_calculation[rm]);
                    nb_bytes_read = 2;
                    
                }
                else if (mod == static_cast<int>(ModeFieldEncoding::Memory_Mode_8_Bit_Displacement))
                {
                    std::byte byte_three = binary_data[i + 2];
                    rm_name = std::format("[{} + {}]", reg_adress_calculation[rm], std::to_integer<int>(byte_three));
                    nb_bytes_read = 3;
                }
                else if 
                (
                    mod == static_cast<int>(ModeFieldEncoding::Memory_Mode_16_Bit_Displacement) 
                    || mod == static_cast<int>(ModeFieldEncoding::Memory_Mode_No_Displacement) && rm == 0b110
                )
                {
                    std::byte byte_three = binary_data[i + 2];
                    std::byte byte_four  = binary_data[i + 3];
                    int displacement = std::to_integer<int>(byte_three) | (std::to_integer<int>(byte_four) << 8);

                    if (rm == 0b110 && mod == static_cast<int>(ModeFieldEncoding::Memory_Mode_No_Displacement))
                    {
                        rm_name = std::format("[{}]", displacement);
                    }
                    else
                    {
                        rm_name = std::format("[{} + {}]", reg_adress_calculation[rm], displacement);
                    }
                    nb_bytes_read = 4;
                }
            }
            if (direction) 
            {
                std::println("mov {}, {}", reg_name, rm_name);
            } 
            else 
            {
                std::println("mov {}, {}", rm_name, reg_name);
            }
        }
        else if (opcode >> 2 == 0b1011)
        {
            bool w          = (std::to_integer<int>(byte_one) >> 3) & 0x1;
            int reg         = std::to_integer<int>(byte_one) & 0x7;

            std::string_view reg_name = w ? reg_names_16bit[reg] : reg_names_8bit[reg];

            int immediate;
            
            if (w)
            {
                std::byte byte_three = binary_data[i + 2];
                immediate = std::to_integer<int>(byte_two) | (std::to_integer<int>(byte_three) << 8);
                nb_bytes_read = 3;
            }
            else
            {
                immediate = std::to_integer<int>(byte_two);
                nb_bytes_read = 2;
            }

            std::println("mov {}, {}", reg_name, immediate);
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
    
    auto loaded_binary = load_binary_file(argv[1]);
    if (!loaded_binary) 
    {
        print_error(loaded_binary.error());
        return 1;
    }
    
    std::vector<std::byte>& binary_data = loaded_binary.value();

    std::println("; {} disassembly : \n", argv[1]);
    print_binary(binary_data);

    std::println("\n");
    print_hex(binary_data);
    
    std::println();
    print_mnemonics(binary_data);

    return 0;
}