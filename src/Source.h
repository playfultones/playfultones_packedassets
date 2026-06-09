#pragma once
#include <memory>
#include <span>
#include "PackedAssetSource.h"
#include "TypedAssets.h"

namespace pt::packedassets {
// Test/explicit entry point.
std::shared_ptr<PackedAssetSource> createSourceFromSpan(std::span<const uint8_t> pak, const Key& key);
// Production entry point: locates the platform byte source + compiled-in key.
// Returns nullptr if assets can't be located (caller decides fallback).
std::shared_ptr<PackedAssetSource> createDefaultSource();
Key compiledInKey();
}
