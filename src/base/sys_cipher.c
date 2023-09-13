#include "common.h"

static char * iv = "tanxaes_deadbeef";
// key is md5 of string iv
static char * key = "f29baf6ff9f9acb2cbf6441cd8eb29e4";


int aes_cfb_encrypt( unsigned char * in, int inlen, unsigned char * out )
{
    int enc_len = 0, tmp_len = 0;

    sys_assert( in != NULL );
    sys_assert( out != NULL );

    if( inlen < 1 ) {
        return 0;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if( ctx ) {
        EVP_CIPHER_CTX_init( ctx );
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cfb8(), NULL, (unsigned char*)key, (unsigned char*)iv );

        do {
            if( ! EVP_EncryptUpdate( ctx, out, &tmp_len, in, inlen ) ) {
                err("evp enc update failed\n");
                break;
            }
            enc_len += tmp_len;

            if( ! EVP_EncryptFinal_ex( ctx, out + enc_len, &tmp_len ) ) {
                err("evp enc final failed\n");
                break;
            }
            enc_len += tmp_len;

        } while(0);
        EVP_CIPHER_CTX_free(ctx);
    } else {
        err("evp ctx alloc failed\n");
    }
    return enc_len;
}

int aes_cfb_decrypt( unsigned char * in, int inlen, unsigned char * out )
{
    int dec_len = 0, tmp_len = 0;

    sys_assert( in != NULL );
    sys_assert( out != NULL );

    if( inlen < 1 )
    return 0;

    EVP_CIPHER_CTX  *ctx = EVP_CIPHER_CTX_new();
    if( ctx ) {
        EVP_CIPHER_CTX_init( ctx );
        EVP_DecryptInit_ex(ctx, EVP_aes_256_cfb8(), NULL, (unsigned char*)key, (unsigned char*)iv );
        do {
            if( ! EVP_DecryptUpdate( ctx, out, &tmp_len, in, inlen )  ) {
                err("evp dec update failed\n");
                break;
            }
            dec_len += tmp_len;

            if( ! EVP_DecryptFinal_ex( ctx, out + dec_len, &tmp_len ) ) {
                err("evp dec final failed\n");
                break;
            }
            dec_len += tmp_len;

        } while(0);
        EVP_CIPHER_CTX_free(ctx);
    } else {
        err("evp ctx alloc failed\n");
    }
    return dec_len;
}

