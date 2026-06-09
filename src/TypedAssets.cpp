#include "TypedAssets.h"

namespace pt::packedassets {
juce::MemoryBlock TypedAssets::getBlock(juce::StringRef name) const {
    auto b = src->getBytes(name.text.getAddress());
    if (!b) return {};
    return juce::MemoryBlock(b->data(), b->size());
}
juce::String TypedAssets::getString(juce::StringRef name) const {
    auto mb = getBlock(name);
    return juce::String::fromUTF8(static_cast<const char*>(mb.getData()), int(mb.getSize()));
}
juce::Image TypedAssets::getImage(juce::StringRef name) const {
    auto mb = getBlock(name);
    return juce::ImageFileFormat::loadFrom(mb.getData(), mb.getSize());
}
std::unique_ptr<juce::Drawable> TypedAssets::getDrawable(juce::StringRef name) const {
    auto mb = getBlock(name);
    return juce::Drawable::createFromImageData(mb.getData(), mb.getSize());
}
juce::Typeface::Ptr TypedAssets::getTypeface(juce::StringRef name) const {
    auto mb = getBlock(name);
    return juce::Typeface::createSystemTypefaceFor(mb.getData(), mb.getSize());
}
AudioAsset TypedAssets::getAudio(juce::StringRef name) const {
    auto mb = getBlock(name);
    AudioAsset out;
    juce::AudioFormatManager mgr; mgr.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(
        mgr.createReaderFor(std::make_unique<juce::MemoryInputStream>(mb, false)));
    if (rd == nullptr) return out;
    out.sampleRate = rd->sampleRate;
    out.buffer.setSize(int(rd->numChannels), int(rd->lengthInSamples));
    rd->read(&out.buffer, 0, int(rd->lengthInSamples), 0, true, true);
    return out;
}
}
