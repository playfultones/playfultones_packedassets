#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
TEST_CASE("module links") { REQUIRE(true); }

#include "format/PackFormat.h"
using namespace pt::packedassets;

TEST_CASE("le serialization round-trips") {
    std::vector<uint8_t> b;
    fmt::writeU32(b, 0x01020304u);
    fmt::writeU64(b, 0x1122334455667788ull);
    size_t off = 0;
    REQUIRE(fmt::readU32(b.data(), off) == 0x01020304u);
    REQUIRE(fmt::readU64(b.data(), off) == 0x1122334455667788ull);
    REQUIRE(off == 12);
}
TEST_CASE("crc32 known vector") {
    const char* s = "123456789";
    REQUIRE(fmt::crc32(reinterpret_cast<const uint8_t*>(s), 9) == 0xCBF43926u);
}

#include "src/Crypto.h"
extern "C" {
#include "third_party/tiny-aes/aes.h"
}
#include <vector>
#include <cstdint>

// The platform aesCtrXcrypt (CommonCrypto / AES-NI on Apple) MUST be
// byte-identical to the portable tiny-aes software path, or .pak files stop
// being interchangeable across platforms. On non-Apple both paths ARE tiny-aes,
// so this is a tautology there; on Apple it is the real cross-impl guard.
TEST_CASE("aesCtrXcrypt is byte-identical to the tiny-aes reference") {
    Key key{};
    for (int i = 0; i < 32; ++i) key[i] = uint8_t(i * 7 + 1);
    const uint64_t entryIndex = 0x0102030405060708ull;

    std::vector<uint8_t> plain(10000);
    for (size_t i = 0; i < plain.size(); ++i) plain[i] = uint8_t(i * 131 + 7);

    // Platform implementation (CommonCrypto on Apple).
    auto viaPlatform = plain;
    aesCtrXcrypt(key, entryIndex, viaPlatform);

    // tiny-aes reference, using the same IV derivation as Crypto.cpp.
    auto viaTinyAes = plain;
    {
        uint8_t iv[16] = {0};
        for (int i = 0; i < 8; ++i) iv[i] = uint8_t(entryIndex >> (8 * i));
        AES_ctx ctx;
        AES_init_ctx_iv(&ctx, key.data(), iv);
        AES_CTR_xcrypt_buffer(&ctx, viaTinyAes.data(), unsigned(viaTinyAes.size()));
    }

    REQUIRE(viaPlatform == viaTinyAes); // cross-implementation equivalence
    REQUIRE(viaPlatform != plain);      // sanity: it actually transformed the data

    // CTR is symmetric: applying the same call again restores the plaintext.
    aesCtrXcrypt(key, entryIndex, viaPlatform);
    REQUIRE(viaPlatform == plain);
}

#include "src/Crypto.h"

TEST_CASE("aes-ctr round-trips and is keyed") {
    pt::packedassets::Key key{}; for (int i=0;i<32;++i) key[i]=uint8_t(i);
    std::vector<uint8_t> data{1,2,3,4,5,6,7,8,9,10};
    auto orig = data;
    pt::packedassets::aesCtrXcrypt(key, 7, data);          // encrypt entry index 7
    REQUIRE(data != orig);                                  // changed
    pt::packedassets::aesCtrXcrypt(key, 7, data);          // CTR is symmetric
    REQUIRE(data == orig);                                  // restored
    auto data2 = orig;
    pt::packedassets::aesCtrXcrypt(key, 8, data2);          // different index => different stream
    auto data7 = orig; pt::packedassets::aesCtrXcrypt(key, 7, data7);
    REQUIRE(data2 != data7);
}

#include "src/Packer.h"

TEST_CASE("packer emits header magic, version, entry count") {
    pt::packedassets::Key key{};
    std::vector<pt::packedassets::InputEntry> in {
        {"a.txt", {'h','i'}}, {"b.bin", {0,1,2,3}} };
    auto pak = pt::packedassets::pack(in, key);
    REQUIRE(pak.size() > pt::packedassets::fmt::kHeaderSize);
    REQUIRE(std::memcmp(pak.data(), pt::packedassets::fmt::kMagic, 4) == 0);
    size_t o = 4;
    REQUIRE(pt::packedassets::fmt::readU32(pak.data(), o) == pt::packedassets::fmt::kVersion);
    o = 12;
    REQUIRE(pt::packedassets::fmt::readU32(pak.data(), o) == 2u); // entryCount
}

#include "src/PackedAssetSource.h"

TEST_CASE("loader round-trips, fails on wrong key and truncation") {
    pt::packedassets::Key key{}; key[0]=42;
    std::vector<pt::packedassets::InputEntry> in {
        {"hello.txt", {'h','e','l','l','o'}}, {"raw.bin", {0,255,7,3}} };
    auto pak = pt::packedassets::pack(in, key);

    pt::packedassets::PackedAssetSource src(pak, key);
    auto a = src.getBytes("hello.txt");
    REQUIRE(a.has_value());
    REQUIRE(std::vector<uint8_t>(a->begin(), a->end()) == std::vector<uint8_t>{'h','e','l','l','o'});
    REQUIRE_FALSE(src.getBytes("nope.txt").has_value());

    pt::packedassets::Key wrong{}; wrong[0]=43;
    pt::packedassets::PackedAssetSource bad(pak, wrong);  // wrong key -> TOC garbage -> no valid entries
    REQUIRE_FALSE(bad.isValid());

    std::vector<uint8_t> trunc(pak.begin(), pak.begin()+10);
    pt::packedassets::PackedAssetSource t(trunc, key);
    REQUIRE_FALSE(t.isValid());
}

TEST_CASE("round-trip many entries including binary and empty") {
    pt::packedassets::Key key{}; for(int i=0;i<32;++i) key[i]=uint8_t(i*7);
    std::vector<pt::packedassets::InputEntry> in;
    in.push_back({"empty.bin", {}});
    std::vector<uint8_t> big(5000); for (size_t i=0;i<big.size();++i) big[i]=uint8_t(i*13);
    in.push_back({"big.bin", big});
    in.push_back({"u.txt", {'x'}});
    auto pak = pt::packedassets::pack(in, key);
    pt::packedassets::PackedAssetSource src(pak, key);
    REQUIRE(src.isValid());
    REQUIRE(src.getBytes("empty.bin").value().empty());
    REQUIRE(src.getBytes("big.bin").value() == big);
    REQUIRE(src.getBytes("u.txt").value() == std::vector<uint8_t>{'x'});
}

#include "src/TypedAssets.h"
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_formats/juce_audio_formats.h>

static std::vector<uint8_t> encodePng(int w,int h){
    juce::Image img(juce::Image::ARGB,w,h,true);
    juce::PNGImageFormat fmt; juce::MemoryOutputStream os;
    fmt.writeImageToStream(img, os);
    auto* d=(const uint8_t*)os.getData(); return {d, d+os.getDataSize()};
}
static std::vector<uint8_t> encodeWav(int frames,double sr){
    juce::AudioBuffer<float> buf(1,frames); buf.clear();
    // Use a heap-allocated stream so the writer can safely take ownership and delete it.
    auto* osPtr = new juce::MemoryOutputStream();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(osPtr, sr, 1, 16, {}, 0));
    w->writeFromAudioSampleBuffer(buf,0,frames);
    // Flush and copy data before the writer destroys the stream.
    w->flush();
    const auto* d = static_cast<const uint8_t*>(osPtr->getData());
    std::vector<uint8_t> result(d, d + osPtr->getDataSize());
    w.reset(); // writer deletes osPtr here
    return result;
}

TEST_CASE("typed accessors decode") {
    pt::packedassets::Key key{}; key[1]=9;
    std::vector<pt::packedassets::InputEntry> in {
        {"pic.png", encodePng(12,7)},
        {"snd.wav", encodeWav(480, 48000.0)},
        {"data.json", {'{','}'}} };
    auto pak = pt::packedassets::pack(in, key);
    auto src = std::make_shared<pt::packedassets::PackedAssetSource>(pak, key);
    pt::packedassets::TypedAssets ta(src);

    auto img = ta.getImage("pic.png");
    REQUIRE(img.getWidth() == 12); REQUIRE(img.getHeight() == 7);
    auto au = ta.getAudio("snd.wav");
    REQUIRE(au.sampleRate == Catch::Approx(48000.0));
    REQUIRE(au.buffer.getNumSamples() == 480);
    REQUIRE(ta.getString("data.json") == juce::String("{}"));
}

#include "src/Source.h"
TEST_CASE("createSourceFromSpan builds a source") {
    pt::packedassets::Key key{}; key[3]=5;
    std::vector<pt::packedassets::InputEntry> in {{"x.txt",{'y'}}};
    auto pak = pt::packedassets::pack(in,key);
    auto src = pt::packedassets::createSourceFromSpan(pak, key);
    REQUIRE(src->isValid());
}

#include <fstream>
#include <filesystem>
TEST_CASE("packer collects from multiple roots") {
    namespace fs = std::filesystem;
    auto tmp = fs::temp_directory_path() / "pa_multidir_test";
    fs::remove_all(tmp); fs::create_directories(tmp/"a"); fs::create_directories(tmp/"b");
    { std::ofstream f(tmp/"a"/"one.txt"); f << "AAA"; }
    { std::ofstream f(tmp/"b"/"two.txt"); f << "BBB"; }
    // collectEntries is the unit-under-test (extracted from the CLI).
    auto entries = pt::packedassets::collectEntries({ (tmp/"a").string(), (tmp/"b").string() });
    pt::packedassets::Key key{}; key[0]=1;
    auto pak = pt::packedassets::pack(entries, key);
    pt::packedassets::PackedAssetSource src(pak, key);
    REQUIRE(src.getBytes("one.txt").has_value());
    REQUIRE(src.getBytes("two.txt").has_value());
    fs::remove_all(tmp);
}

TEST_CASE("loader rejects overflowing TOC offsets without OOB") {
    // Craft a pak whose decrypted TOC has a dataOffset chosen so that
    // dataSectionStart + dataOffset + storedSize wraps past 2^64 to a small
    // value. A naive bounds check would admit the entry, then getBytes would
    // build an out-of-bounds slice. The loader must reject it instead.
    pt::packedassets::Key key{}; key[0] = 7;
    const std::string name = "x";
    std::vector<uint8_t> toc;
    fmt::writeU16(toc, (uint16_t) name.size());
    toc.insert(toc.end(), name.begin(), name.end());
    fmt::writeU64(toc, 0xFFFFFFFFFFFFFF8Dull); // dataOffset: makes the sum wrap
    fmt::writeU64(toc, 64ull);                 // storedSize
    fmt::writeU64(toc, 64ull);                 // originalSize
    fmt::writeU32(toc, 0u);                    // crc
    auto tocEnc = toc;
    aesCtrXcrypt(key, 0xFFFFFFFFFFFFFFFFull, tocEnc); // TOC IV convention

    std::vector<uint8_t> pak;
    pak.insert(pak.end(), fmt::kMagic, fmt::kMagic + 4);
    fmt::writeU32(pak, fmt::kVersion);
    fmt::writeU32(pak, 0u);                        // flags
    fmt::writeU32(pak, 1u);                        // count
    fmt::writeU32(pak, (uint32_t) tocEnc.size());  // tocSize
    pak.insert(pak.end(), tocEnc.begin(), tocEnc.end()); // no data section

    pt::packedassets::PackedAssetSource src(pak, key);
    REQUIRE_FALSE(src.isValid());                  // must reject, not admit via overflow
    REQUIRE_FALSE(src.getBytes("x").has_value());  // and must never OOB-read
}
