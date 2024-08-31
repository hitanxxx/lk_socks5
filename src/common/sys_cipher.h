#ifndef _SYS_CIPHER_H_INCLUDED_
#define _SYS_CIPHER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct {
    char tag[16];
    uint32_t datan;
    char data[0];
} sys_cipher_msg_t;

typedef struct {
    EVP_CIPHER_CTX * ctx;
    int typ;
} sys_cipher_t;

int sys_genrand_16byte(char * outbuf);

int aes_cfb_encrypt(unsigned char * in, int inlen, unsigned char * out);
int aes_cfb_decrypt(unsigned char * in, int inlen, unsigned char * out);

int sys_aesgcm_ctx_init(sys_cipher_t ** sctx, int typ);
int sys_aesgcm_ctx_exit(sys_cipher_t * ctx);
int sys_aesgcm_enc(sys_cipher_t * ctx, unsigned char * in, int inn, unsigned char * out, char * tag);   
int sys_aesgcm_dec(sys_cipher_t * ctx, unsigned char * in, int inn, unsigned char * out, char * tag);



#ifdef __cplusplus
}
#endif
        
#endif

