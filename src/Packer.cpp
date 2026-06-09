#include "Packer.h"
#include "../format/PackFormat.h"
#include <cstring>

// collectEntries() walks the filesystem and is build-tooling only: it is used
// by the host packer CLI and the unit tests, never by the shipped runtime
// (no runtime TU includes Packer.h). std::filesystem is unavailable in libc++
// below macOS 10.15, and this .cpp is part of the module unity build that the
// plugin (deployment target 10.13) also compiles. So compile collectEntries
// (and pull in <filesystem>) only where the host actually supports it; the
// packer tool and the test target raise their min target to 10.15 explicitly.
#if !defined(__APPLE__) \
    || (defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__) \
        && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 101500)
 #define PT_PA_HAVE_COLLECT_ENTRIES 1
 #include <filesystem>
 #include <fstream>
 #include <algorithm>
 #include <iterator>
 #include <stdexcept>
#endif

namespace pt::packedassets {
using namespace fmt;
static constexpr uint64_t kPackerTocIv = 0xFFFFFFFFFFFFFFFFull;

#ifdef PT_PA_HAVE_COLLECT_ENTRIES
std::vector<InputEntry> collectEntries(const std::vector<std::string>& dirs){
    namespace fs = std::filesystem;
    std::vector<InputEntry> entries;
    for (const auto& dir : dirs)
        for (auto& e : fs::recursive_directory_iterator(dir)){
            if (!e.is_regular_file()) continue;
            auto name = e.path().filename().string();
            if (name == ".DS_Store") continue;
            std::ifstream f(e.path(), std::ios::binary);
            if (! f) throw std::runtime_error("collectEntries: cannot read " + e.path().string());
            entries.push_back({ name,
                { std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>() } });
        }
    std::sort(entries.begin(), entries.end(),
              [](auto& a, auto& b){ return a.name < b.name; });          // deterministic
    return entries;
}
#endif

std::vector<uint8_t> pack(const std::vector<InputEntry>& entries, const Key& key){
    // 1. Build the data section (each entry encrypted with IV = its index).
    std::vector<uint8_t> data;
    std::vector<EntryMeta> metas;
    for (size_t i=0;i<entries.size();++i){
        const auto& e = entries[i];
        EntryMeta m; m.name=e.name; m.originalSize=e.bytes.size();
        m.crc = crc32(e.bytes.data(), e.bytes.size());
        m.dataOffset = data.size();
        std::vector<uint8_t> enc = e.bytes;
        aesCtrXcrypt(key, i, enc);
        m.storedSize = enc.size();
        data.insert(data.end(), enc.begin(), enc.end());
        metas.push_back(std::move(m));
    }
    // 2. Build the TOC (plaintext), then encrypt it with the reserved TOC IV.
    std::vector<uint8_t> toc;
    for (auto& m : metas){
        writeU16(toc, uint16_t(m.name.size()));
        toc.insert(toc.end(), m.name.begin(), m.name.end());
        writeU64(toc, m.dataOffset); writeU64(toc, m.storedSize);
        writeU64(toc, m.originalSize); writeU32(toc, m.crc);
    }
    std::vector<uint8_t> tocEnc = toc;
    aesCtrXcrypt(key, kPackerTocIv, tocEnc);
    // 3. Header. Data offsets in the TOC are relative to the start of the data section,
    //    which begins at kHeaderSize + tocEnc.size().
    std::vector<uint8_t> out;
    out.insert(out.end(), kMagic, kMagic+4);
    writeU32(out, kVersion); writeU32(out, 0u);
    writeU32(out, uint32_t(entries.size())); writeU32(out, uint32_t(tocEnc.size()));
    out.insert(out.end(), tocEnc.begin(), tocEnc.end());
    out.insert(out.end(), data.begin(), data.end());
    return out;
}
}
