#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nettle/sm4.h>

void main(void)
{
    /* SM4, GB/T 32907-2016, Appendix A.1 */
    unsigned char key[16] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
    unsigned char plaindata[16] = {0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,0xfe,0xdc,0xba,0x98,0x76,0x54,0x32,0x10};
    unsigned char expected_cipherdata[16] = {0x68,0x1e,0xdf,0x34,0xd2,0x06,0x96,0x5e,0x86,0xb3,0xe9,0x4f,0x53,0x6e,0x42,0x46};
    unsigned char cipherdata[16] = {0};
    struct sm4_ctx ctx;
    size_t i;

    sm4_set_encrypt_key(&ctx, key);
    sm4_crypt(&ctx, 16, cipherdata, plaindata);

    for(i = 0; i<16; i++) {
        printf("computed data[%lu]: 0x%x, expected data[%lu]: 0x%x\n", i, cipherdata[i], i, expected_cipherdata[i]);
    }

    unsigned char *key_text = "0123456789abcdeffedcba9876543210";
    unsigned char *plain_text = "0123456789abcdeffedcba9876543210";
    unsigned char *cipher_text = malloc(16);
    memset(cipher_text, 0, 16);

    sm4_set_encrypt_key(&ctx, key_text);
    sm4_crypt(&ctx, 16, cipher_text, plain_text);

    for(i = 0; i<16; i++) {
        printf("cipher text: data[%lu]: 0x%x\n", i, cipher_text[i]);
    }
}
