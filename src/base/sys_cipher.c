#include "common.h"

static char * iv = "tanxaes_deadbeef";
// key is md5 of string iv
static char * key = "f29baf6ff9f9acb2cbf6441cd8eb29e4";

/// @brief convert raw data into encrypt data with cipher ctx, the encrypt data will be replace raw data
int sys_cipher_conv(sys_cipher_t * ctx, unsigned char * in, int inn)
{
    if(ctx) {
        if(ctx->typ == 0) {            
            if(!EVP_EncryptUpdate(ctx->ctx, in, &ctx->tn, in, inn)) {
                err("sys evp enc update failed\n");
                return -1;
            }
            ctx->actn += ctx->tn;
        } else {
            if(!EVP_DecryptUpdate(ctx->ctx, in, &ctx->tn, in, inn)) {
                err("sys evp dec update failed\n");
                return -1;
            }
            ctx->actn += ctx->tn;
        }
        return ctx->tn;
    } else {
        err("ctx empty!\n");
        return -1;
    }
}

/// @brief create a cipher ctx with specify type, 0: encrypt 1:decrypt
int sys_cipher_ctx_init(sys_cipher_t ** ctx, int typ)
{
    int ret = -1;
    sys_cipher_t * local_ctx = NULL;
    
    do {
        local_ctx = (sys_cipher_t *)sys_alloc( sizeof(sys_cipher_t) );
        if(local_ctx) {
            local_ctx->typ = typ;
            local_ctx->ctx = EVP_CIPHER_CTX_new();
            if(local_ctx->ctx) {
                EVP_CIPHER_CTX_init(local_ctx->ctx);
                if(local_ctx->typ == 0) {
                    EVP_EncryptInit_ex(local_ctx->ctx, EVP_aes_256_cfb8(), NULL, (unsigned char*)key, (unsigned char*)iv);
                    ret = 0;    
                } else {
                    EVP_DecryptInit_ex(local_ctx->ctx, EVP_aes_256_cfb8(), NULL, (unsigned char*)key, (unsigned char*)iv);
                    ret = 0;
                }
            } else {
                err("sys cipher ctx new fialed\n");
                break;
            }
        } else {
            err("sys alloc cipher ctx failed. [%d]\n", errno);
            break;
        }
    } while(0);
    if(ret == 0) {
        *ctx = local_ctx;
    }
    return ret;
}

/// @brief destory a cipher ctx 
int sys_cipher_ctx_deinit(sys_cipher_t * ctx)
{
    if(ctx) {
        if(ctx->ctx) {
            EVP_CIPHER_CTX_free(ctx->ctx);
        }
        sys_free((void*)ctx);
    }
    return 0;
}



int aes_cfb_encrypt(unsigned char * in, int inlen, unsigned char * out)
{
    int enc_len = 0, tmp_len = 0;

    sys_assert(in != NULL);
    sys_assert(out != NULL);

    if(inlen < 1) {
        return 0;
    }

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if(ctx) {
        EVP_CIPHER_CTX_init(ctx);
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cfb8(), NULL, (unsigned char*)key, (unsigned char*)iv);

        do {
            if(!EVP_EncryptUpdate(ctx, out, &tmp_len, in, inlen)) {
                err("evp enc update failed\n");
                break;
            }
            enc_len += tmp_len;

            if(!EVP_EncryptFinal_ex(ctx, out + enc_len, &tmp_len)) {
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

int aes_cfb_decrypt(unsigned char * in, int inlen, unsigned char * out)
{
    int dec_len = 0, tmp_len = 0;

    sys_assert(in != NULL);
    sys_assert(out != NULL);

    if(inlen < 1)
        return 0;

    EVP_CIPHER_CTX  *ctx = EVP_CIPHER_CTX_new();
    if(ctx) {
        EVP_CIPHER_CTX_init(ctx);
        EVP_DecryptInit_ex(ctx, EVP_aes_256_cfb8(), NULL, (unsigned char*)key, (unsigned char*)iv);
        do {
            if(!EVP_DecryptUpdate( ctx, out, &tmp_len, in, inlen)) {
                err("evp dec update failed\n");
                break;
            }
            dec_len += tmp_len;

            if(!EVP_DecryptFinal_ex(ctx, out + dec_len, &tmp_len)) {
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

