// Usage: pak_packer --key <64hex> --out <assets.pak> --dir <assetDir> [--dir <assetDir2> ...]
#include "../src/Packer.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static pt::packedassets::Key parseKey(const std::string& hex){
    pt::packedassets::Key k{};
    for (size_t i=0;i<32 && i*2+1<hex.size();++i)
        k[i] = uint8_t(std::stoi(hex.substr(i*2,2), nullptr, 16));
    return k;
}
int main(int argc, char** argv){
    std::string keyHex, out;
    std::vector<std::string> dirs;
    for (int i=1;i+1<argc;i+=2){ std::string a=argv[i];
        if(a=="--key")keyHex=argv[i+1]; else if(a=="--out")out=argv[i+1];
        else if(a=="--dir")dirs.push_back(argv[i+1]); }
    if (keyHex.empty()||out.empty()||dirs.empty()){ std::cerr<<"missing args\n"; return 2; }
    auto entries = pt::packedassets::collectEntries(dirs);
    auto pak = pt::packedassets::pack(entries, parseKey(keyHex));
    std::ofstream o(out, std::ios::binary);
    o.write(reinterpret_cast<const char*>(pak.data()), std::streamsize(pak.size()));
    std::cerr << "packed " << entries.size() << " entries -> " << out << " (" << pak.size() << " bytes)\n";
    return 0;
}
