#include <gcrypt.h>
void main(void)
{
    gcry_cipher_hd_t handler;
    gcry_cipher_open(&handler, GCRY_CIPHER_SM4, GCRY_CIPHER_MODE_ECB, 0);
}
