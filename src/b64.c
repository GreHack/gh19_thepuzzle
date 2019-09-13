// C Program to decode a base64 
// Encoded string back to ASCII string 
/* taken from https://www.geeksforgeeks.org/decode-data-base-64-string-ascii-string/ */
/* Modified for GH19 13/09/19 */
  
#include <stdio.h> 
#include <stdlib.h> 
  
/* char_set = "ABCDEFGHIJKLMNOPQRSTUVWXYZ 
abcdefghijklmnopqrstuvwxyz0123456789+/" */
  
char *b64_decode(char *data, unsigned int data_len) 
{ 
    char *decoded = (char *)malloc(sizeof(char) * data_len); 
    int k = 0; 
    // stores the bitstream. 
    int num = 0; 
    // count_bits stores current 
    // number of bits in num. 
    int count_bits = 0; 
    // selects 4 characters from 
    // data string at a time. 
    // find the position of each data 
    // character in char_set and stores in num. 
    for (unsigned int i = 0; i < data_len; i += 4) { 
        num = 0, count_bits = 0; 
        for (unsigned int j = 0; j < 4; j++) { 
            // make space for 6 bits. 
            if (data[i + j] != '=') { 
                num = num << 6; 
                count_bits += 6; 
            } 
  
            /* Finding the position of each data  
            character in char_set  
            and storing in "num", use OR  
            '|' operator to store bits.*/
  
            // data[i + j] = 'E', 'E' - 'A' = 5 
            // 'E' has 5th position in char_set. 
            if (data[i + j] >= 'A' && data[i + j] <= 'Z') 
                num = num | (data[i + j] - 'A'); 
  
            // data[i + j] = 'e', 'e' - 'a' = 5, 
            // 5 + 26 = 31, 'e' has 31st position in char_set. 
            else if (data[i + j] >= 'a' && data[i + j] <= 'z') 
                num = num | (data[i + j] - 'a' + 26); 
  
            // data[i + j] = '8', '8' - '0' = 8 
            // 8 + 52 = 60, '8' has 60th position in char_set. 
            else if (data[i + j] >= '0' && data[i + j] <= '9') 
                num = num | (data[i + j] - '0' + 52); 
  
            // '+' occurs in 62nd position in char_set. 
            else if (data[i + j] == '+') 
                num = num | 62; 
  
            // '/' occurs in 63rd position in char_set. 
            else if (data[i + j] == '/') 
                num = num | 63; 
  
            // ( str[i + j] == '=' ) remove 2 bits 
            // to delete appended bits during encoding. 
            else { 
                num = num >> 2; 
                count_bits -= 2; 
            } 
        } 
  
        while (count_bits != 0) { 
            count_bits -= 8; 
  
            // 255 in binary is 11111111 
            decoded[k++] = (num >> count_bits) & 255; 
        } 
    } 
  
    // place NULL character to mark end of string. 
    decoded[k] = '\0'; 
  
    return decoded; 
} 
