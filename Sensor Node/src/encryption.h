#include <stdint.h>
#include <WString.h>
int pad_length(int len);
int pad(uint8_t* output, uint8_t* data, int len);
int encrypted_base64_length(int len);
int encrypt_to_base64(uint8_t* output, uint8_t* data, int len);
int encrypt_to_base64(uint8_t* output, String data);
const uint8_t* get_iv();