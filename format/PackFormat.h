#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

namespace pt::packedassets::fmt {

constexpr uint8_t  kMagic[4] = {'B','D','P','K'};
constexpr uint32_t kVersion  = 1;
constexpr uint32_t kHeaderSize = 20; // magic(4)+version(4)+flags(4)+entryCount(4)+tocSize(4)

inline void writeU16(std::vector<uint8_t>& b, uint16_t v){ b.push_back(uint8_t(v)); b.push_back(uint8_t(v>>8)); }
inline void writeU32(std::vector<uint8_t>& b, uint32_t v){ for(int i=0;i<4;++i) b.push_back(uint8_t(v>>(8*i))); }
inline void writeU64(std::vector<uint8_t>& b, uint64_t v){ for(int i=0;i<8;++i) b.push_back(uint8_t(v>>(8*i))); }
inline uint16_t readU16(const uint8_t* p, size_t& o){ uint16_t v=uint16_t(p[o])|uint16_t(p[o+1])<<8; o+=2; return v; }
inline uint32_t readU32(const uint8_t* p, size_t& o){ uint32_t v=0; for(int i=0;i<4;++i) v|=uint32_t(p[o+i])<<(8*i); o+=4; return v; }
inline uint64_t readU64(const uint8_t* p, size_t& o){ uint64_t v=0; for(int i=0;i<8;++i) v|=uint64_t(p[o+i])<<(8*i); o+=8; return v; }

inline uint32_t crc32(const uint8_t* data, size_t n){
    uint32_t c = 0xFFFFFFFFu;
    for(size_t i=0;i<n;++i){ c ^= data[i];
        for(int k=0;k<8;++k) c = (c>>1) ^ (0xEDB88320u & (~(c&1)+1)); }
    return c ^ 0xFFFFFFFFu;
}

// TOC entry, in-memory form
struct EntryMeta { std::string name; uint64_t dataOffset, storedSize, originalSize; uint32_t crc; };

} // namespace
