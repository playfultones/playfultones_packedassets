#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace pt::packedassets {
using Key = std::array<uint8_t, 32>;
// In-place AES-256-CTR. IV is derived from entryIndex (first 8 bytes LE, rest zero).
void aesCtrXcrypt(const Key& key, uint64_t entryIndex, std::span<uint8_t> data);
inline void aesCtrXcrypt(const Key& key, uint64_t i, std::vector<uint8_t>& d){ aesCtrXcrypt(key,i,std::span<uint8_t>(d)); }
}
