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
