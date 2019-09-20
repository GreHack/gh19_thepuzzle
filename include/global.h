
#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include "rc4.h"

/* use KD tree for data structure of OCR? */
#define KD_TREE 1

#define FREE(p) do { free(p); p = NULL; } while(0)

#ifndef RELEASE
#define EXIT(code, msg) fprintf(stderr, "[!] %s -- aborting\n", msg); exit(code);
#else
#define EXIT(code, msg) exit(code);
#endif

/***** OBFUSCATION-RELATED VARIABLES *****/

// GiveUs2PacBack
#define TUPAC_BEG asm volatile(".byte 0x47, 0x69, 0x76, 0x65, 0x55, 0x73, 0x32, 0x50, 0x61, 0x63, 0x42, 0x61, 0x63, 0x6b");
// LetTheLegendResurrect
#define TUPAC_END asm volatile(".byte 0x4c, 0x65, 0x74, 0x54, 0x68, 0x65, 0x4c, 0x65, 0x67, 0x65, 0x6e, 0x64, 0x52, 0x65, 0x73, 0x75, 0x72, 0x72, 0x65, 0x63, 0x74");

#ifndef KD_LOAD
#define KD_DUMP
#else
rc4_state_t *kd_rc4_state;
#define KD_RC4_KEY "This program cannot be run in DOS mode"
#define FREAD_KD(p, sz, nb, fd) fread(p, sz, nb, fd); for (unsigned int idx = 0; idx < sz*nb; idx++) { ((unsigned char *) p)[idx] ^= rc4_stream(kd_rc4_state); }// rc4_enc((unsigned char *) p, sz*nb);
#endif


/**** Here are some options to tweak for performances issues ****/

// Max number of call to a breakpoint
#define MAX_BP_CALL 20

// Define maximum number of call to function unpacking
#define MAX_UNPACK 3
// Define maximum number of packed functions in the binary
#define NFUNC 26

#endif
