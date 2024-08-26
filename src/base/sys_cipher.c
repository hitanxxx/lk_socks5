#include "common.h"

static char * iv = "tanxaes_deadbeef";
// key is md5 of string iv
static char * key = "f29baf6ff9f9acb2cbf6441cd8eb29e4";

int sys_aesgcm_enc(sys_cipher_t * ctx, unsigned char * in, int inn, char * tag)
{
    sassert(ctx != NULL);
    int outl = 0;
    int len;

    schk(1 == EVP_EncryptUpdate(ctx->ctx, in, &len, in, inn), return -1);
    outl += len;
    schk(1 == EVP_EncryptFinal_ex(ctx->ctx, in + len, &len), return -1);
    outl += len;
    schk(1 == EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_GET_TAG, 16, tag), return -1);
    return outl;
}

int sys_aesgcm_dec(sys_cipher_t * ctx, unsigned char * in, int inn, char * tag)
{
    sassert(ctx != NULL);
    int outl = 0;
    int len = 0;

    schk(1 == EVP_DecryptUpdate(ctx->ctx, in, &len, in, inn), return -1);
    outl += len;
    schk(1 == EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_SET_TAG, 16, tag), return -1);
    int rc = EVP_DecryptFinal_ex(ctx->ctx, in + len, &len);
    if(rc > 0) {
        outl += len;
        return outl;
    }
    return -1;
}

int sys_aesgcm_ctx_init(sys_cipher_t ** sctx, int typ)
{
    int ret = -1;
    sys_cipher_t * ctx = sys_alloc(sizeof(sys_cipher_t));
    if(!ctx) {
        err("ctx alloc err\n");
        return -1;
    }
    
    do {
        ctx->typ = typ;
        schk((ctx->ctx = EVP_CIPHER_CTX_new()) != NULL, break);
        if(typ == 0) {
            schk(1 == EVP_EncryptInit_ex(ctx->ctx, EVP_aes_256_gcm(), NULL, NULL, NULL), break);
            schk(1 == EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL), break);
            schk(1 == EVP_EncryptInit_ex(ctx->ctx, NULL, NULL, (unsigned char*)key, (unsigned char*)iv), break);
        } else {
            schk(1 == EVP_DecryptInit_ex(ctx->ctx, EVP_aes_256_gcm(), NULL, NULL, NULL), break);
            schk(1 == EVP_CIPHER_CTX_ctrl(ctx->ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL), break);
            schk(1 == EVP_DecryptInit_ex(ctx->ctx, NULL, NULL, (unsigned char*)key, (unsigned char*)iv), break);
        }
        ret = 0;
    } while(0);

    if(ret != 0) {
        if(ctx) {
            if(ctx->ctx) EVP_CIPHER_CTX_free(ctx->ctx);
            sys_free(ctx);
        }
        return -1;
    }
    *sctx = ctx;
    return 0;
}

int sys_aesgcm_ctx_exit(sys_cipher_t * ctx)
{
    if(ctx) {
        if(ctx->ctx) EVP_CIPHER_CTX_free(ctx->ctx);
        sys_free(ctx);
    }
    return 0;
}



int aes_cfb_encrypt(unsigned char * in, int inlen, unsigned char * out)
{
    int enc_len = 0, tmp_len = 0;

    sassert(in != NULL);
    sassert(out != NULL);

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

    sassert(in != NULL);
    sassert(out != NULL);

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

