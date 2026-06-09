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
    // verifyCrc is opt-IN: the pak normally ships sealed inside a codesigned
    // bundle / RCDATA, so runtime corruption is effectively impossible and the
    // per-asset CRC check is redundant work on the hot boot path. Pass true to
    // re-enable it (e.g. for integrity testing or untrusted sources).
    PackedAssetSource(std::span<const uint8_t> pak, const Key& key, bool verifyCrc = false);
    bool isValid() const { return valid; }
    // Decrypted raw bytes for an entry, or nullopt if absent/invalid. Decrypts
    // fresh on each call (no shared mutable state), so it is safe to call
    // concurrently from multiple threads -- e.g. a parallel sequence decode.
    std::optional<std::vector<uint8_t>> getBytes(std::string_view name) const;
    // BinaryData::getNamedResource-compatible shim. Returns a pointer owned by an
    // internal cache (stable for the source's lifetime); sizeOut set to byte count.
    // Unlike getBytes this mutates the cache and is NOT thread-safe.
    const char* getNamedResource(const char* nameUtf8, int& sizeOut) const;
private:
    struct Entry { uint64_t index, dataOffset, storedSize, originalSize; uint32_t crc; };
    std::span<const uint8_t> pak;
    Key key;
    bool valid = false;
    bool verifyCrc = false;
    size_t dataSectionStart = 0;
    std::unordered_map<std::string, Entry> entries;
    mutable std::unordered_map<std::string, std::vector<uint8_t>> cache;
};
}
