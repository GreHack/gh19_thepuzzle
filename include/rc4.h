#ifndef __RC4_H__
#define __RC4_H__

typedef struct rc4_state_s rc4_state_t;

rc4_state_t *rc4_init(const char *key, int key_len);
void rc4_free(rc4_state_t *state);
unsigned char rc4_stream(rc4_state_t *state);
char *rc4_crypt(const char *key, int key_len, char *input, int input_len); 

#endif
