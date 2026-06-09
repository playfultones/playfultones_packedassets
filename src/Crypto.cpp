#include "Crypto.h"
extern "C" {
#include "../third_party/tiny-aes/aes.h"
}
#include <cstring>

#if defined(__APPLE__)
 #include <CommonCrypto/CommonCryptor.h>
#endif

namespace pt::packedassets {
namespace {
    // 16-byte CTR counter block: first 8 bytes = entryIndex little-endian, rest
    // zero. The counter then increments big-endian (from the last byte), which is
    // what both tiny-aes and CommonCrypto's CTR_BE mode do.
    inline void buildIv (uint64_t entryIndex, uint8_t iv[16])
    {
        std::memset (iv, 0, 16);
        for (int i = 0; i < 8; ++i)
            iv[i] = uint8_t (entryIndex >> (8 * i));
    }

    // Portable software AES-256-CTR (tiny-aes). The canonical reference; the
    // hardware path below is verified byte-identical to it by a unit test.
    void aesCtrXcryptSoftware (const Key& key, uint64_t entryIndex, std::span<uint8_t> data)
    {
        uint8_t iv[16];
        buildIv (entryIndex, iv);
        AES_ctx ctx;
        AES_init_ctx_iv (&ctx, key.data(), iv);
        AES_CTR_xcrypt_buffer (&ctx, data.data(), unsigned (data.size()));
    }
}

void aesCtrXcrypt (const Key& key, uint64_t entryIndex, std::span<uint8_t> data)
{
    if (data.empty())
        return;

#if defined(__APPLE__)
    // Hardware AES (AES-NI) via CommonCrypto. ~1-2 orders of magnitude faster
    // than the tiny-aes software path, which dominated cold-boot time when
    // decrypting a large asset pack. CTR is symmetric, so one call both
    // encrypts and decrypts. CTR_BE matches the software counter, keeping .pak
    // files byte-compatible across platforms (asserted by the crypto test).
    uint8_t iv[16];
    buildIv (entryIndex, iv);

    CCCryptorRef cryptor = nullptr;
    if (CCCryptorCreateWithMode (kCCEncrypt, kCCModeCTR, kCCAlgorithmAES, ccNoPadding,
            iv, key.data(), key.size(), nullptr, 0, 0,
            kCCModeOptionCTR_BE, &cryptor) == kCCSuccess)
    {
        size_t moved = 0;
        CCCryptorUpdate (cryptor, data.data(), data.size(), data.data(), data.size(), &moved);
        CCCryptorRelease (cryptor);
        return;
    }
    // CommonCrypto failed (not expected for valid AES-256-CTR params). Fall
    // through to the software path: it produces byte-identical output, so this
    // preserves correctness (it is not a degraded result, only a slower route).
#endif

    aesCtrXcryptSoftware (key, entryIndex, data);
}
}
