//
//  main.c
//  base64_ap
//
//  Created by ap on 14/5/2021.
//

#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

// Mask used in calculating base64 rep
const uint32_t M00111111 = 63;
const uint32_t M00000011 = 3;

// For each 3 input bytes, there are 4 output chars...
static const size_t BYTE_BUFFER_SIZE = 1024 * 3;
static const size_t BASE64_BUFFER_SIZE = 1024 * 4;

static const char* CHARMAP = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char REVERSEMAP[] = {
    62,
    0,  0,  0,
    63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
     0,  0,  0,  0,  0,  0,  0,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
     0,  0,  0,  0,  0,  0,
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

static inline char char_to_int(char c) {
    // We take the ascii value of c, subtract 43 (which is the ascii value of +,
    // the lowest valued ascii character in the base64 char set) and look up in the array
    // above. The array has padding for ascii values we don't use.
    // 43 == ascii value of '+'
    return REVERSEMAP[(uint8_t)c - 43];
}

//void test_char_to_int(void) {
//    for (int i = 0; i < 64; ++i) {
//        printf("%c -> %d\n", CHARMAP[i], char_to_int(CHARMAP[i]));
//        assert(CHARMAP[i] == CHARMAP[char_to_int(CHARMAP[i])]);
//    }
//}

void decode(void) {
    char bytes[BYTE_BUFFER_SIZE];
    char base64[BASE64_BUFFER_SIZE];
    
    size_t sparebytes = 0;
    while (1) {
        ssize_t ret = read(0, base64 + sparebytes, BASE64_BUFFER_SIZE - sparebytes);
        if (ret == -1) {
            fprintf(stderr, "Failed to read from stdin");
            _exit(-1);
        }
        
        if (ret == 0) {
            if (sparebytes != 0) {
                // TODO: base64 doesn't care about this. It just decodes, i.e. it allows missing padding
                fprintf(stderr, "Invalid base64. Input buffer was not a multiple of 4 bytes");
                _exit(-1);
            }
            break;
        }
        
        // Handle edge case where we haven't reached the end of the buffer.
        // I see no reason why this couldn't happen. Not sure how to trigger
        // it though.
        sparebytes = ret % 4;
        ret -= sparebytes;
        if (ret == 0) {
            continue;
        }
        
        size_t inbufPos = 0;
        size_t outbufPos = 0;
        bool havePadding = base64[ret - 1] == '=';
        
        for (; inbufPos < ret - havePadding ? 4 : 0; inbufPos += 4) {
            // 00000011 11112222 22333333 <- Mapping of 3 bytes to 4 base64 chars
            char d1 = char_to_int(base64[inbufPos]);
            char d2 = char_to_int(base64[inbufPos + 1]);
            char d3 = char_to_int(base64[inbufPos + 2]);
            char d4 = char_to_int(base64[inbufPos + 3]);
            
            bytes[outbufPos+2] = (d3 << 6) | d4;
            bytes[outbufPos+1] = (d3 >> 2) | d2 << 4;
            bytes[outbufPos] = (M00000011 & d2 >> 4) | (d1 << 2);
            // Increment here rather than in the for loop so outbufPos always points to end of buffer
            outbufPos += 3;
        }
        
        if (havePadding) {
            if (base64[ret - 2] == '=') {
                // We have padding ==
                // 00000011 [0000] <- Mapping of 2 bytes to 3 base64 chars
                char d1 = char_to_int(base64[ret - 4]);
                char d2 = char_to_int(base64[ret - 3]);
                bytes[outbufPos] = d1 << 2 | d2 >> 4;
                outbufPos += 1;
            } else {
                // We have padding =
                // 00000011 11112222 [00] <- Mapping of 2 bytes to 3 base64 chars
                char d1 = char_to_int(base64[ret - 4]);
                char d2 = char_to_int(base64[ret - 3]);
                char d3 = char_to_int(base64[ret - 2]);
                bytes[outbufPos+1] = d2 << 4 | d3 >> 2;
                bytes[outbufPos] = d2 >> 4 | d1 << 2;
                outbufPos += 2;
            }
        }
        
        // Print this way as the buffer won't be null terminated/
        printf("%.*s", (int)outbufPos, bytes);
        
        // Copy any spare bytes to the start of the buffer. We'll handle
        // them next loop.
        for (size_t i = 0; i < sparebytes; ++i) {
            base64[i] = base64[i + ret];
        }
    }
    
    return;
}

void encode(void) {
    
    char bytes[BYTE_BUFFER_SIZE];
    char base64[BASE64_BUFFER_SIZE];
    
    while (1) {
        ssize_t ret = read(0, bytes, BYTE_BUFFER_SIZE);
        if (ret == -1) {
            fprintf(stderr, "Failed to read from stdin");
            _exit(-1);
        }
        
        if (ret == 0) {
            break;
        }
        
        size_t endOf3byteBlocks = (size_t)(ret / 3) * 3;
        size_t inbufPos = 0;
        size_t outbufPos = 0;
        
        for (; inbufPos < endOf3byteBlocks; inbufPos += 3) {
            // 00000011 11112222 22333333 <- Mapping of 3 bytes to 4 base64 chars
            base64[outbufPos+3] = CHARMAP[M00111111 & bytes[inbufPos+2]];
            base64[outbufPos+2] = CHARMAP[M00111111 & (bytes[inbufPos+1] << 2 | bytes[inbufPos+2] >> 6)];
            base64[outbufPos+1] = CHARMAP[M00111111 & (bytes [inbufPos] << 4 | bytes[inbufPos+1] >> 4)];
            base64[outbufPos] = CHARMAP[M00111111 & (bytes[inbufPos] >> 2)];
            // Increment here rather than in the for loop so outbufPos always points to end of buffer
            outbufPos += 4;
        }
        
        // Handle trailing 1 or 2 bytes
        size_t trailing = ret - endOf3byteBlocks;
        if (trailing == 1) {
            // 00000011 [0000] <- Mapping of 2 bytes to 3 base64 chars
            base64[outbufPos+3] = '=';
            base64[outbufPos+2] = '=';
            base64[outbufPos+1] = CHARMAP[M00111111 & bytes[inbufPos] << 4];
            base64[outbufPos] = CHARMAP[bytes[inbufPos] >> 2];
            outbufPos += 4;
        } else if (trailing == 2) {
            // 00000011 11112222 [00] <- Mapping of 2 bytes to 3 base64 chars
            base64[outbufPos+3] = '=';
            base64[outbufPos+2] = CHARMAP[M00111111 & (bytes[inbufPos+1] << 2)];
            base64[outbufPos+1] = CHARMAP[M00111111 & (bytes[inbufPos] << 4 | bytes[inbufPos+1] >> 4)];
            base64[outbufPos] = CHARMAP[bytes[inbufPos] >> 2];
            outbufPos += 4;
        }
        
        // Print this way as the buffer won't be null terminated/
        printf("%.*s", (int)outbufPos, base64);
    }
    
    return;
}

void usage(FILE* file) {
    const char* usage =
        "Usage:    base64_ap [-hde]\n"
        "  -e [Default behaviour] Encode data passed to stdin\n"
        "  -d Decode base64 string passed to stdin\n"
        "  -h Displays this help\n";
    fprintf(file, "%s", usage);
}

int main(int argc, const char * argv[]) {
    int opt;
    bool doEncode = true;
    while((opt = getopt(argc, (char**)argv, ":edh")) != -1) {
        switch(opt)
        {
            case 'h':
                usage(stdout);
                _exit(0);
            case 'd':
                doEncode = false;
                break;
            case 'e':
                doEncode = true;
                break;
            case '?':
                fprintf(stderr, "Unrecognised option: %c\n", optopt);
                usage(stderr);
                _exit(1);
        }
    }
    
    doEncode ? encode() : decode();
    return 0;
}
