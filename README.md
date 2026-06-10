# playfultones_packedassets

A JUCE user module that packs plugin assets into a single encrypted `.pak`,
delivered as a bundle resource on macOS and an `RCDATA` resource on Windows,
and read back through one loader keyed by the original filename.

## Why

JUCE's `BinaryData` embeds assets directly in the binary. In a macOS universal
build that data is duplicated once per architecture slice, so a large asset set
is paid for twice. Packing the assets into one resource file stores them a
single time regardless of slice count, which can meaningfully shrink a
universal bundle.

## What it does

- `pak_packer` CLI walks one or more asset directories and writes an encrypted
  `.pak` (AES-256-CTR, per-entry CRC, a table of contents keyed by original
  filename).
- `PackedAssetSource` maps the embedded pak at runtime and returns the bytes for
  a given filename. `createDefaultSource()` locates the platform resource
  (macOS bundle `Resources/assets.pak`, Windows `RCDATA`) and is memoized so
  multiple consumers share one mapping.
- Typed accessors decode bytes into `juce::Image`, `juce::String`, audio,
  typeface, and drawable.
- `CMake/PackedAssets.cmake` provides `playfultones_packedassets_add_pack(...)`
  to build the pak at configure/build time, inject the key, and embed the
  resource into the target's bundle.

## Crypto backends

AES-256-CTR runs on a hardware-accelerated backend at runtime, with a portable
software fallback. All three are verified **byte-identical** (a unit test asserts
each matches the tiny-aes reference), so a `.pak` packed on one platform decrypts
on any other:

- **macOS / iOS** — Apple CommonCrypto (`CCCryptor`; AES-NI / ARMv8 crypto).
- **Windows** — CNG / BCrypt (`bcrypt.lib`; AES-NI). Linked automatically via the
  module's `windowsLibs` / `mingwLibs` — no extra CMake.
- **Other** — vendored tiny-aes (software fallback).

CRC verification is opt-in (`PackedAssetSource(pak, key, /*verifyCrc=*/true)`),
**off by default**: a pak shipped inside a codesigned bundle / RCDATA can't
realistically corrupt, so the per-asset CRC is redundant work on the boot path
(a wrong key still surfaces as undecodable bytes downstream).

## Usage

**Pack + embed (CMake)** — call `add_pack` on the target that *compiles* the
module sources (your plugin / SharedCode target; the key is force-included there):

```cmake
juce_add_module(path/to/playfultones_packedassets)
include(path/to/playfultones_packedassets/CMake/PackedAssets.cmake)
target_link_libraries(MyPlugin PRIVATE playfultones_packedassets)

playfultones_packedassets_add_pack(
    TARGET    MyPlugin
    ASSET_DIR ${CMAKE_SOURCE_DIR}/Assets/a ${CMAKE_SOURCE_DIR}/Assets/b  # 1+ roots; unique basenames
    KEY       <64 hex chars / 32 bytes>)   # FORMATS optional -> defaults to every plugin format built
```

This builds `assets.pak` at build time, force-includes the key, and embeds the
pak into each plugin format (macOS `Resources/`, Windows `RCDATA`).

**Read at runtime** — one shared, memoized source, keyed by original filename:

```cpp
auto src = pt::packedassets::createDefaultSource();   // shared_ptr; nullptr if the pak isn't present
if (auto bytes = src->getBytes ("Knob_0.png"))        // std::optional<std::vector<uint8_t>>
    auto img = juce::ImageFileFormat::loadFrom (bytes->data(), bytes->size());
```

For JUCE UIs, `bd_ui_loader`'s `PackedAssetImageLoader` (auto-detected via
`__has_include`) drives a `UILoader` straight from the pak, with parallel
filmstrip decode.

## Security note

The encryption here is obfuscation, not a guarded secret. The key is compiled
into the shipped binary by necessity (the plugin must decrypt offline), so it is
recoverable by anyone with the binary. The intent is to keep assets from being
trivially extractable with a hex editor or a stock asset ripper, not to provide
cryptographic protection.

## Third-party

`third_party/tiny-aes/` vendors [Tiny AES in C](https://github.com/kokke/tiny-AES-c)
by kokke, released into the public domain under The Unlicense. See
`third_party/tiny-aes/LICENSE`.

## License

MIT. See [LICENSE](LICENSE).
