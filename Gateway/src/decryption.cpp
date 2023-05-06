#include <Arduino.h>
#include <pkcs7_padding.h>
#include <Crypto.h>
#include <AES.h>
#include <CBC.h>
#include "base64.hpp"
#include <secret.h>

#define IV_SIZE 16

int decrypted_len(uint8_t* data, int len)
{
    return decode_base64_length(data, len);
}

int decrypt_to_base64(uint8_t* output, uint8_t* data, int len, uint8_t* iv)
{
    crypto_feed_watchdog();

    static CBC<AESSmall128> cbc;
    cbc.clear();
    cbc.setKey(key, cbc.keySize());
    cbc.setIV(iv, cbc.ivSize());
    int decoded_len = decode_base64_length(data, len);

    uint8_t decoded[decoded_len];
    decode_base64(data, len, decoded);
    
    cbc.decrypt(output, decoded, decoded_len);

    //free(decoded);
    return decoded_len;
    
}