#pragma once
#include <memory>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PackedAssetSource.h"

namespace pt::packedassets {
struct AudioAsset { juce::AudioBuffer<float> buffer; double sampleRate = 0.0; };

class TypedAssets {
public:
    explicit TypedAssets(std::shared_ptr<const PackedAssetSource> s) : src(std::move(s)) {}
    juce::MemoryBlock     getBlock   (juce::StringRef name) const;
    juce::String          getString  (juce::StringRef name) const;
    juce::Image           getImage   (juce::StringRef name) const;
    std::unique_ptr<juce::Drawable> getDrawable (juce::StringRef name) const;
    juce::Typeface::Ptr   getTypeface(juce::StringRef name) const;
    AudioAsset            getAudio   (juce::StringRef name) const;
private:
    std::shared_ptr<const PackedAssetSource> src;
};
}
