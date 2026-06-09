#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "Crypto.h"

namespace pt::packedassets {
struct InputEntry { std::string name; std::vector<uint8_t> bytes; };
// Recursively walk each directory in `dirs`, reading every regular file into an
// InputEntry keyed by basename. Skips .DS_Store. Returns entries sorted by name
// for deterministic output. Multiple roots may be supplied; basenames are
// expected to be unique across roots (the pak/loader keys by basename).
std::vector<InputEntry> collectEntries(const std::vector<std::string>& dirs);
// Build an encrypted .pak: [header][encrypted TOC][encrypted data]. Deterministic order = input order.
std::vector<uint8_t> pack(const std::vector<InputEntry>& entries, const Key& key);
}
