#include <iostream>

#include <vector>
#include <cstddef>
#include <fstream>
#include <bitset>
#include <cstdint>

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

void print_mnemonics(const std::vector<std::byte>& binary_data)
{

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

    std::cout << "Mnemonics of binary file : " << argv[1] << std::endl;

    std::vector<std::byte> binary_data = load_binary(argv[1]);

    print_binary(binary_data);
    // print_hex(binary_data);
    // print_mnemonics(binary_data);
    return 0;
}