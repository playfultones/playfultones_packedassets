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

#if JUCE_MAC
static juce::File pt_source_ownBundleResourcesPak(){
    Dl_info info{};
    if (dladdr((void*)&pt_source_ownBundleResourcesPak, &info) && info.dli_fname != nullptr){
        juce::File bin(juce::CharPointer_UTF8(info.dli_fname));
        // .../Contents/MacOS/Exe -> .../Contents/Resources/assets.pak
        return bin.getParentDirectory().getSiblingFile("Resources").getChildFile("assets.pak");
    }
    return {};
}
std::shared_ptr<PackedAssetSource> createDefaultSource(){
    // Memoized: both UILoaders (main + FX) share one mapping; avoids mmapping the pak twice.
    static std::shared_ptr<PackedAssetSource> cached = [](){
        static juce::MemoryMappedFile* mapped = nullptr;
        auto pak = pt_source_ownBundleResourcesPak();
        if (! pak.existsAsFile()) return std::shared_ptr<PackedAssetSource>(nullptr);
        mapped = new juce::MemoryMappedFile(pak, juce::MemoryMappedFile::readOnly);
        std::span<const uint8_t> span(static_cast<const uint8_t*>(mapped->getData()), mapped->getSize());
        return std::make_shared<PackedAssetSource>(span, compiledInKey());
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
    // Memoized for parity with the platform definitions; no assets to locate here.
    static std::shared_ptr<PackedAssetSource> cached = nullptr;
    return cached;
}
#endif
}
