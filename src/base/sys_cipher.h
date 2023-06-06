#ifndef _SYS_CIPHER_H_INCLUDED_
#define _SYS_CIPHER_H_INCLUDED_

#ifdef __cplusplus
extern "C"
{
#endif

int aes_cfb_encrypt( unsigned char * in, int inlen, unsigned char * out );
int aes_cfb_decrypt( unsigned char * in, int inlen, unsigned char * out );

#ifdef __cplusplus
}
#endif
        
#endif

