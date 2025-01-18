#include <fstream>
#include <vector>

int main() {
    // Assembly:
    // loop:
    //   b loop

    std::vector<uint8_t> payload;
    payload.push_back(0x00);
    payload.push_back(0x00);
    payload.push_back(0x00);
    payload.push_back(0x14);
     

    std::ofstream outfile("payload.bin", std::ios::binary);
    outfile.write(reinterpret_cast<char*>(payload.data()), payload.size());
    outfile.close();

    return 0;
}
