#include "Crypto.h"
extern "C" {
#include "../third_party/tiny-aes/aes.h"
}
#include <cstring>

namespace pt::packedassets {
void aesCtrXcrypt(const Key& key, uint64_t entryIndex, std::span<uint8_t> data){
    uint8_t iv[16] = {0};
    for (int i=0;i<8;++i) iv[i] = uint8_t(entryIndex >> (8*i));
    AES_ctx ctx;
    AES_init_ctx_iv(&ctx, key.data(), iv);
    AES_CTR_xcrypt_buffer(&ctx, data.data(), unsigned(data.size()));
}
}
