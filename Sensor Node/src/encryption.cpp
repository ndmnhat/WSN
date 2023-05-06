#include <Arduino.h>
#include <pkcs7_padding.h>
#include <Crypto.h>
#include <AES.h>
#include <CBC.h>
#include "base64.hpp"
#include <secret.h>

#define IV_SIZE 16

const uint8_t* generateIV() {
  /* Change to allowable characters */
  const uint8_t possible[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  static uint8_t iv[IV_SIZE+1];
  for(int i = 0; i < IV_SIZE+1; i++){
    int r = random(0, 61);
    iv[i] = possible[r];
  }
  iv[IV_SIZE] = '\0';
  return iv;
}

const uint8_t* iv = generateIV();

const uint8_t* get_iv()
{
    return iv;
}
int pad_length(int len)
{
    int dlenu = len;
    if (len % 16) {
        dlenu += 16 - (len % 16);
        
    }
    return dlenu;
}

int pad(uint8_t* output, uint8_t* data, int len)
{
    int dlenu = pad_length(len);
    memset(output, 0, dlenu);
    memccpy(output, data, 0, len);
    pkcs7_padding_pad_buffer(output, len, dlenu, 16 );
    

    return dlenu;
}

int encrypted_base64_length(int len)
{
    return encode_base64_length(pad_length(len));
}

int encrypt_to_base64(uint8_t* output, uint8_t* data, int len)
{
    int padded_len = pad_length(len);
    uint8_t padded[padded_len];
    pad(padded, data, len);

    static CBC<AESSmall128> cbc;
    generateIV();
    cbc.clear();
    cbc.setKey(key, cbc.keySize());
    cbc.setIV(iv, cbc.ivSize());
    
    uint8_t encrypted[padded_len];
    cbc.encrypt(encrypted, padded, padded_len);
    cbc.clear();
    encode_base64(encrypted, padded_len, output);

    free(padded);
    return encode_base64_length(padded_len);
    
}

int encrypt_to_base64(uint8_t* output, String data)
{
    int len = data.length();
    uint8_t data_array[len];
    memccpy(data_array, data.c_str(), 0, len);
    return encrypt_to_base64(output, data_array, len);
}