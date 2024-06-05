#ifndef _SYS_CIPHER_H_INCLUDED_
#define _SYS_CIPHER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct sys_cipher 
{
    EVP_CIPHER_CTX * ctx;
    int typ;
    int actn;
    int tn;
} sys_cipher_t;


int aes_cfb_encrypt(unsigned char * in, int inlen, unsigned char * out);
int aes_cfb_decrypt(unsigned char * in, int inlen, unsigned char * out);


int sys_cipher_conv(sys_cipher_t * ctx, unsigned char * in, int inn);
int sys_cipher_ctx_init(sys_cipher_t ** ctx, int typ);
int sys_cipher_ctx_deinit(sys_cipher_t * ctx);

    



#ifdef __cplusplus
}
#endif
        
#endif

