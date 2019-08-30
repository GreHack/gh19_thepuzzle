
#ifndef __GLOBAL_H__
#define __GLOBAL_H__

/* length of the flag to read (number of decimal digits) */
#define FLAG_LEN 4
/* use KD tree for data structure of OCR? */
#define KD_TREE 1

#define FREE(p) free(p); p = NULL

/***** OBFUSCATION-RELATED VARIABLES *****/

// GiveUs2PacBack
#define TUPAC_BEG asm volatile(".byte 0x47, 0x69, 0x76, 0x65, 0x55, 0x73, 0x32, 0x50, 0x61, 0x63, 0x42, 0x61, 0x63, 0x6b"); 
// LetTheLegendResurrect
#define TUPAC_END asm volatile(".byte 0x4c, 0x65, 0x74, 0x54, 0x68, 0x65, 0x4c, 0x65, 0x67, 0x65, 0x6e, 0x64, 0x52, 0x65, 0x73, 0x75, 0x72, 0x72, 0x65, 0x63, 0x74");

#endif
