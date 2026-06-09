#pragma once
#include <span>
#include <vector>
#include <optional>
#include <unordered_map>
#include "Crypto.h"
#include "../format/PackFormat.h"

namespace pt::packedassets {
class PackedAssetSource {
public:
    PackedAssetSource(std::span<const uint8_t> pak, const Key& key);
    bool isValid() const { return valid; }
    // Decrypted raw bytes for an entry, or nullopt if absent/invalid.
    std::optional<std::vector<uint8_t>> getBytes(std::string_view name) const;
    // BinaryData::getNamedResource-compatible shim. Returns a pointer owned by an
    // internal cache (stable for the source's lifetime); sizeOut set to byte count.
    const char* getNamedResource(const char* nameUtf8, int& sizeOut) const;
private:
    struct Entry { uint64_t index, dataOffset, storedSize, originalSize; uint32_t crc; };
    std::span<const uint8_t> pak;
    Key key;
    bool valid = false;
    size_t dataSectionStart = 0;
    std::unordered_map<std::string, Entry> entries;
    mutable std::unordered_map<std::string, std::vector<uint8_t>> cache;
};
}
