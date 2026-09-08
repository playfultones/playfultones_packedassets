#include "Source.h"
#include "GeneratedKey.h"
#include <juce_core/juce_core.h>

#if JUCE_MAC
 #include <dlfcn.h>
#elif JUCE_WINDOWS
 #include <windows.h>
#endif

namespace pt::packedassets {
Key compiledInKey(){ return Key PT_PACKEDASSETS_KEY; }

std::shared_ptr<PackedAssetSource> createSourceFromSpan(std::span<const uint8_t> pak, const Key& key){
    return std::make_shared<PackedAssetSource>(pak, key);
}

std::shared_ptr<PackedAssetSource> createSourceFromFile(const juce::File& pakFile, const Key& key){
    if (! pakFile.existsAsFile())
        return nullptr;
    auto mapped = std::make_shared<juce::MemoryMappedFile>(pakFile, juce::MemoryMappedFile::readOnly);
    if (mapped->getData() == nullptr || mapped->getSize() == 0)
        return nullptr;
    std::span<const uint8_t> span(static_cast<const uint8_t*>(mapped->getData()), (size_t) mapped->getSize());
    // The source keeps a non-owning span over the mapped bytes, so the mapping
    // must outlive it. Hand ownership of `mapped` to the shared_ptr's deleter:
    // the mmap is released only after the source is destroyed.
    auto* src = new PackedAssetSource(span, key);
    return std::shared_ptr<PackedAssetSource>(src, [mapped](PackedAssetSource* p){ delete p; });
}

#if JUCE_MAC
// The binary this code lives in: the plugin/app inside its bundle, or a plain
// executable (a test or tool built from the same tree).
static juce::File pt_source_ownBinary(){
    Dl_info info{};
    if (dladdr((void*)&pt_source_ownBinary, &info) && info.dli_fname != nullptr)
        return juce::File(juce::CharPointer_UTF8(info.dli_fname));
    return {};
}
std::shared_ptr<PackedAssetSource> createDefaultSource(){
    // Memoized: both UILoaders (main + FX) share one mapping; avoids mmapping the pak twice.
    static std::shared_ptr<PackedAssetSource> cached = [](){
        const auto bin = pt_source_ownBinary();
        if (bin == juce::File()) return std::shared_ptr<PackedAssetSource>(nullptr);
        // .../Contents/MacOS/Exe -> .../Contents/Resources/assets.pak, else a
        // pak embed_into placed beside a plain executable.
        for (const auto& pak : { bin.getParentDirectory().getSiblingFile("Resources").getChildFile("assets.pak"),
                                 bin.getSiblingFile("assets.pak") })
            if (auto src = createSourceFromFile(pak))
                return src;
        return std::shared_ptr<PackedAssetSource>(nullptr);
    }();
    return cached;
}
#elif JUCE_WINDOWS
std::shared_ptr<PackedAssetSource> createDefaultSource(){
    // Memoized: both UILoaders (main + FX) share one mapping; avoids locating the resource twice.
    static std::shared_ptr<PackedAssetSource> cached = [](){
        HMODULE mod = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)&createDefaultSource, &mod);
        // RT_RCDATA expands to MAKEINTRESOURCE(10); without UNICODE defined in
        // this TU that's the ANSI MAKEINTRESOURCEA (LPSTR), which won't bind to
        // FindResourceW's LPCWSTR. The value is just the integer 10 packed into
        // the pointer, so cast it to LPCWSTR (there is no RT_RCDATAW).
        HRSRC h = FindResourceW(mod, L"PACKEDASSETS", (LPCWSTR) RT_RCDATA);
        if (h == nullptr) return std::shared_ptr<PackedAssetSource>(nullptr);
        HGLOBAL g = LoadResource(mod, h);
        auto* p = static_cast<const uint8_t*>(LockResource(g));
        DWORD sz = SizeofResource(mod, h);
        if (p == nullptr || sz == 0) return std::shared_ptr<PackedAssetSource>(nullptr);
        return std::make_shared<PackedAssetSource>(std::span<const uint8_t>(p, sz), compiledInKey());
    }();
    return cached;
}
#else
std::shared_ptr<PackedAssetSource> createDefaultSource(){
    // Memoized for parity with the platform definitions. Only a pak placed
    // beside the executable (embed_into) can be found here.
    static std::shared_ptr<PackedAssetSource> cached = createSourceFromFile(
        juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("assets.pak"));
    return cached;
}
#endif
}
