#include "PackedAssetSource.h"
#include <cstring>

namespace pt::packedassets {
using namespace fmt;
static constexpr uint64_t kLoaderTocIv = 0xFFFFFFFFFFFFFFFFull;

PackedAssetSource::PackedAssetSource(std::span<const uint8_t> p, const Key& k) : pak(p), key(k) {
    if (pak.size() < kHeaderSize) return;
    if (std::memcmp(pak.data(), kMagic, 4) != 0) return;
    size_t o = 4;
    if (readU32(pak.data(), o) != kVersion) return;
    o = 8; (void) readU32(pak.data(), o);                  // flags
    uint32_t count = readU32(pak.data(), o);
    uint32_t tocSize = readU32(pak.data(), o);
    if (size_t(kHeaderSize) + tocSize > pak.size()) return;
    dataSectionStart = kHeaderSize + tocSize;
    const uint64_t dataSize = pak.size() - dataSectionStart; // dataSectionStart <= pak.size() (checked above)

    std::vector<uint8_t> toc(pak.begin()+kHeaderSize, pak.begin()+kHeaderSize+tocSize);
    aesCtrXcrypt(key, kLoaderTocIv, toc);                  // decrypt TOC
    size_t t = 0;
    for (uint32_t i=0;i<count;++i){
        if (t + 2 > toc.size()) return;
        uint16_t nameLen = readU16(toc.data(), t);
        if (t + nameLen + 28 > toc.size()) return;         // name + 3xU64 + U32
        std::string name(reinterpret_cast<const char*>(toc.data()+t), nameLen); t += nameLen;
        Entry e; e.index=i;
        e.dataOffset=readU64(toc.data(),t); e.storedSize=readU64(toc.data(),t);
        e.originalSize=readU64(toc.data(),t); e.crc=readU32(toc.data(),t);
        // Overflow-safe bounds check: dataOffset/storedSize are u64 from the
        // (attacker/corruption-controllable) TOC, so a naive sum can wrap.
        if (e.dataOffset > dataSize || e.storedSize > dataSize - e.dataOffset) return;
        entries.emplace(std::move(name), e);
    }
    valid = !entries.empty();
}

std::optional<std::vector<uint8_t>> PackedAssetSource::getBytes(std::string_view name) const {
    if (!valid) return std::nullopt;
    auto it = entries.find(std::string(name));
    if (it == entries.end()) return std::nullopt;
    const Entry& e = it->second;
    // Defense in depth: re-validate the slice is in-bounds before constructing it.
    const uint64_t dataSize = pak.size() - dataSectionStart;
    if (e.dataOffset > dataSize || e.storedSize > dataSize - e.dataOffset) return std::nullopt;
    std::vector<uint8_t> out(pak.begin()+dataSectionStart+e.dataOffset,
                             pak.begin()+dataSectionStart+e.dataOffset+e.storedSize);
    aesCtrXcrypt(key, e.index, out);
    if (crc32(out.data(), out.size()) != e.crc) return std::nullopt;
    return out;
}

const char* PackedAssetSource::getNamedResource(const char* nameUtf8, int& sizeOut) const {
    sizeOut = 0;
    auto bytes = getBytes(nameUtf8);
    if (!bytes) return nullptr;
    auto [it, ins] = cache.insert_or_assign(std::string(nameUtf8), std::move(*bytes));
    sizeOut = int(it->second.size());
    return reinterpret_cast<const char*>(it->second.data());
}
}
