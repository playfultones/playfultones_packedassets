#pragma once
#include <memory>
#include <span>
#include "PackedAssetSource.h"
#include "TypedAssets.h"

namespace juce { class File; }

namespace pt::packedassets {
// Test/explicit entry point.
std::shared_ptr<PackedAssetSource> createSourceFromSpan(std::span<const uint8_t> pak, const Key& key);
// Production entry point: locates the platform byte source + compiled-in key.
// Returns nullptr if assets can't be located (caller decides fallback).
std::shared_ptr<PackedAssetSource> createDefaultSource();
Key compiledInKey();
// Load a pak from an arbitrary path (e.g. one deployed outside the bundle, into
// /Library/Application Support). Memory-maps the file and keeps the mapping alive
// for the returned source's lifetime (PackedAssetSource holds a non-owning span).
// Returns nullptr if the file is missing or cannot be mapped. Defaults to the
// compiled-in key so the same call site works for both dev and shipped paks.
std::shared_ptr<PackedAssetSource> createSourceFromFile(const juce::File& pakFile, const Key& key = compiledInKey());
}
