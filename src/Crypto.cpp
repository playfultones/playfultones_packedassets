#include "Crypto.h"
extern "C" {
#include "../third_party/tiny-aes/aes.h"
}
#include <cstring>

#if defined(__APPLE__)
 #include <CommonCrypto/CommonCryptor.h>
#elif defined(_WIN32)
 #include <windows.h>
 #include <bcrypt.h>
 #include <vector>
 #include <algorithm>
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

#if defined(_WIN32)
    // AES-256-CTR via Windows CNG (BCrypt). CNG has no native CTR mode, so we use
    // AES-ECB to build the keystream -- encrypt successive counter blocks and XOR
    // them into the data -- exactly as tiny-aes does internally, giving
    // byte-identical output (the crypto test asserts this). Hardware-accelerated
    // (AES-NI) where available. Returns false on any CNG failure so the caller
    // falls back to the software path.
    bool aesCtrXcryptBCrypt (const Key& key, uint64_t entryIndex, std::span<uint8_t> data)
    {
        BCRYPT_ALG_HANDLE alg = nullptr;
        if (! BCRYPT_SUCCESS (BCryptOpenAlgorithmProvider (&alg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
            return false;

        bool ok = BCRYPT_SUCCESS (BCryptSetProperty (alg, BCRYPT_CHAINING_MODE,
            (PUCHAR) BCRYPT_CHAIN_MODE_ECB, sizeof (BCRYPT_CHAIN_MODE_ECB), 0));

        BCRYPT_KEY_HANDLE hKey = nullptr;
        if (ok)
            ok = BCRYPT_SUCCESS (BCryptGenerateSymmetricKey (alg, &hKey, nullptr, 0,
                (PUCHAR) key.data(), (ULONG) key.size(), 0));

        if (ok)
        {
            uint8_t counter[16];
            buildIv (entryIndex, counter);

            constexpr size_t kBlocks = 4096; // 64 KiB of keystream per BCryptEncrypt call
            std::vector<uint8_t> ks (kBlocks * 16);
            size_t off = 0;

            while (ok && off < data.size())
            {
                const size_t remaining = data.size() - off;
                const size_t nBlocks = std::min (kBlocks, (remaining + 15) / 16);

                // Fill the keystream buffer with successive counter blocks; the
                // counter increments big-endian (from the last byte) to match
                // tiny-aes / CommonCrypto CTR_BE.
                for (size_t b = 0; b < nBlocks; ++b)
                {
                    std::memcpy (&ks[b * 16], counter, 16);
                    for (int i = 15; i >= 0; --i)
                        if (++counter[i] != 0)
                            break;
                }

                ULONG produced = 0;
                const ULONG len = (ULONG) (nBlocks * 16);
                ok = BCRYPT_SUCCESS (BCryptEncrypt (hKey, ks.data(), len, nullptr,
                    nullptr, 0, ks.data(), len, &produced, 0)); // ECB -> keystream in place

                if (ok)
                {
                    const size_t n = std::min (remaining, nBlocks * 16);
                    for (size_t i = 0; i < n; ++i)
                        data[off + i] ^= ks[i];
                    off += n;
                }
            }
            BCryptDestroyKey (hKey);
        }

        BCryptCloseAlgorithmProvider (alg, 0);
        return ok;
    }
#endif
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
#elif defined(_WIN32)
    // Hardware AES via Windows CNG (BCrypt), same role as CommonCrypto on Apple.
    if (aesCtrXcryptBCrypt (key, entryIndex, data))
        return;
    // CNG failed; fall through to the byte-identical software path.
#endif

    aesCtrXcryptSoftware (key, entryIndex, data);
}
}
