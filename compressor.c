#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xsidevice.h"

void *xsim = NULL;

unsigned char input[16384];
int inSize;

unsigned char output[8192];
int outSize;

int equalPart(int offset1, int offset2) {
    int i;
    int distance = offset1 - offset2;
    for(i = 0; i<inSize - offset1; i++) {
        if (offset2 + i == offset1) {
            offset2 -= distance;
        }
        if (input[offset1 + i] != input[offset2+i]) {
            return i;
        }
//        fprintf(stderr, "** i[%d]: %d = i[%d]: %d\n", offset1 + i, input[offset1+i], i + offset2, input[i + offset2]);
    }
    return i;
}

#define MARKERS 3

int marker[MARKERS];

int findMarkers() {
    int hist[256];
    int i, l;
    int cnt = 0;
    for(i = 0; i < 256; i++) {
        hist[i] = 0;
    }
    for(i = 0; i < inSize; i++) {
        hist[input[i]]++;
    }
    for(l = 0; l < inSize; l++) {
        for(i = 0; i < 256; i++) {
            if(hist[i] == l) {
                marker[cnt++] = i;
                if (cnt == MARKERS) {
                    return;
                }
            }
        }
    }
}

void compress(int logMarker0Length, int logMarker1Length, int logMarker2Length, int targetAddress, FILE *assembly) {
    int inIndex = 0;
    int offset, maxLength, maxOffset;
    int maxLength0 = 1 << logMarker0Length;
    int maxLength1 = 1 << logMarker1Length;
    int maxLength2 = 1 << logMarker2Length;
    int maxOffset0 = 256/maxLength0;
    int maxOffset1 = 256/maxLength1;
    int maxOffset2 = 65536/maxLength2;
    outSize = 0;
    int progLen = 124;
 
    while(inIndex != inSize) {
        maxLength =  0;
        for(offset = -2; offset >= -inIndex; offset-=2) {
            int length = equalPart(inIndex, inIndex + offset);
            if (length > maxLength) {
                if (length >= 3) {
                    int i;
                    for(i = 0; i < length; i++) {
//                        fprintf(stderr, "i[%d]: %d = i[%d]: %d\n", inIndex + i, input[inIndex+i], inIndex + i + offset, input[inIndex + i + offset]);
                    }
                }
                maxLength = length;
                maxOffset = -offset / 2;
            }
        }
        if (maxLength < 3 || maxOffset >= maxOffset2 + (maxOffset0-1)) {
            int i;
            int byte =  input[inIndex++];
            output[outSize++] = byte;
            for(i = 0; i < MARKERS; i++) {
                if (marker[i] == byte) {
                    output[outSize++] = 0;
                }
            }
//            printf("Byte %d\n", input[inIndex]);
        } else {
            if (maxOffset < maxOffset0) {
                if (maxLength >= maxLength0 + 3) {
                    maxLength = maxLength0 + 2;
                }
                output[outSize++] = marker[0];
                output[outSize++] = (maxLength-3) * maxOffset0 + maxOffset;
//                printf("Marker0: %d %d\n", maxLength, maxOffset);
                inIndex += maxLength;
            } else if (maxLength < maxLength1+3 && maxOffset < maxOffset1+(maxOffset0-1)) {
                output[outSize++] = marker[1];
                output[outSize++] = (maxLength-3) * maxOffset1 + (maxOffset-(maxOffset0-1));
//                printf("Marker1: %d %d\n", maxLength, maxOffset);
                inIndex += maxLength;
            } else if (maxLength == 3) {
                int i;
                int byte =  input[inIndex++];
                output[outSize++] = byte;
                for(i = 0; i < MARKERS; i++) {
                    if (marker[i] == byte) {
                        output[outSize++] = 0;
                    }
                }
            } else {
                if (maxLength >= maxLength2 + 3) {
                    maxLength = maxLength2 + 2;
                }
                output[outSize++] = marker[2];
                output[outSize++] = (maxLength-3) * (maxOffset2/256) + (maxOffset-(maxOffset0-1)) / 256;
                output[outSize++] = (maxOffset-(maxOffset0-1)) % 256;
//                printf("Marker2: %d %d\n", maxLength, maxOffset);
                inIndex += maxLength;
            }
        }
    }
    if (targetAddress != 0) {
        int i, j;
        fprintf(assembly, ".globl _start\n");
        fprintf(assembly, "_start:\n");
        fprintf(assembly, "    ldc r0, %d\n", marker[0]);
        fprintf(assembly, "    ldc r1, %d\n", marker[1]);
        fprintf(assembly, "    ldc r2, %d\n", marker[2]);
        fprintf(assembly, "    ldap r11, compressedBinaryEnd\n");
        fprintf(assembly, "    or r3, r11, r11\n");
        fprintf(assembly, "    ldc r4, 0x%x\n", targetAddress >> 8);
        fprintf(assembly, "    shl r4, r4, 8\n");
        if (targetAddress & 0xff) {
            fprintf(assembly, "    ldc r5, 0x%x\n", targetAddress & 0xff);
            fprintf(assembly, "    or r4, r5, r4\n");
        }
        fprintf(assembly, "    or r5, r4, r4\n");
        fprintf(assembly, "    ldap r11, compressedBinary\n");
        fprintf(assembly, "    ldc  r6, 0\n");
        fprintf(assembly, "loop:   \n");
        fprintf(assembly, "    lsu  r10, r11, r3\n");
        fprintf(assembly, "    bf   r10, done\n");
        fprintf(assembly, "    ld8u r7, r11[r6]\n");
        fprintf(assembly, "    add  r11, r11, 1\n");
        fprintf(assembly, "    eq   r10, r7, r0\n");
        fprintf(assembly, "    bt   r10, marker\n");
        fprintf(assembly, "    eq   r10, r7, r1\n");
        fprintf(assembly, "    bt   r10, marker\n");
        fprintf(assembly, "    eq   r10, r7, r2\n");
        fprintf(assembly, "    bt   r10, marker\n");
        fprintf(assembly, "copyChar:\n");
        fprintf(assembly, "    st8  r7, r4[r6]\n");
        fprintf(assembly, "    add  r4, r4, 1\n");
        fprintf(assembly, "    bu   loop\n");
        fprintf(assembly, "marker:\n");
        fprintf(assembly, "    ld8u r8, r11[r6]\n");
        fprintf(assembly, "    add  r11, r11, 1\n");
        fprintf(assembly, "    bf   r8, copyChar\n");
        fprintf(assembly, "    eq   r10, r7, r0\n");
        fprintf(assembly, "    bt   r10, marker0\n");
        fprintf(assembly, "    eq   r10, r7, r1\n");
        fprintf(assembly, "    bt   r10, marker1\n");
        fprintf(assembly, "marker2:\n");
        fprintf(assembly, "    shr  r7, r8, %d\n", (8-logMarker2Length));
        fprintf(assembly, "    zext r8, %d\n", (8-logMarker2Length));
        fprintf(assembly, "    ld8u r10, r11[r6]\n");
        fprintf(assembly, "    add  r11, r11, 1\n");
        fprintf(assembly, "    shl  r8, r8, 8\n");
        fprintf(assembly, "    or   r8, r8, r10\n");
        fprintf(assembly, "    bu   adjustR8\n");
        fprintf(assembly, "marker1:\n");
        fprintf(assembly, "    shr  r7, r8, %d\n", (8-logMarker1Length));
        fprintf(assembly, "    zext r8, %d\n", (8-logMarker1Length));
        fprintf(assembly, "adjustR8:\n");
        if (maxOffset0 <= 12) {
            fprintf(assembly, "    add  r8, r8, %d\n", maxOffset0-1);
        } else if (maxOffset0 <= 23) {
            fprintf(assembly, "    add  r8, r8, 11\n");
            fprintf(assembly, "    add  r8, r8, %d\n", maxOffset0 - 12);
        } else {
            fprintf(assembly, "    ldc  r10, %d\n", maxOffset0-1);
            fprintf(assembly, "    add  r8, r8, r10\n");
        }
        fprintf(assembly, "    bu   copyManyChars\n");
        fprintf(assembly, "marker0:\n");
        fprintf(assembly, "    shr  r7, r8, %d\n", (8-logMarker0Length));
        fprintf(assembly, "    zext r8, %d\n", (8-logMarker0Length));
        fprintf(assembly, "copyManyChars:\n");
        fprintf(assembly, "    shl  r8, r8, 1\n");
        fprintf(assembly, "    add  r7, r7, 3\n");
        fprintf(assembly, "    sub  r9, r6, r8\n");
        fprintf(assembly, "loopChar:   \n");
        fprintf(assembly, "    ld8u r10, r4[r9]\n");
        fprintf(assembly, "    st8  r10, r4[r6]\n");
        fprintf(assembly, "    add  r4, r4, 1\n");
        fprintf(assembly, "    sub  r7, r7, 1\n");
        fprintf(assembly, "    bt   r7, loopChar\n");
        fprintf(assembly, "    bu   loop\n");
        fprintf(assembly, "done:\n");
        fprintf(assembly, "    bau  r5\n");
        fprintf(assembly, "compressedBinary: // %d => %d + %d, compressed to %2d%%  \n", inSize, outSize, progLen, 100*(outSize+progLen)/inSize);
        for(i = 0; i < outSize; i+=8) {
            fprintf(assembly, "    .byte ");
            for(j = i; j < i + 8 && j < outSize; j++) {
                fprintf(assembly, "%c %3d", j == i? ' ' : ',', output[j]);
            }
            fprintf(assembly, "\n");
        }
        fprintf(assembly, "    .align 2\n");
        fprintf(assembly, "compressedBinaryEnd:\n");
    }
}

int endswith(char *in, char *trail) {
    int lin = strlen(in);
    int ltrail = strlen(trail);
    int i;
    
    for(i = lin - ltrail; i < lin; i++) {
        if (in[i] != trail[i - (lin-ltrail)]) {
            return 0;
        }
    }
    return 1;
}

char cmd[1024];

int main(int argc, char *argv[]) {
    int ml0, ml1, ml2;
    int bestml0, bestml1, bestml2;
    int minSize = 65536;
    int targetAddress = 0x1B000;
    char *decompressorAddress = "0x10000";
    FILE *fb = NULL, *fass;
    unsigned char c;
    int status, i, errors;
    char *target = NULL;
    char *xeOutput = "a.xe";
    char *assOutput = "a.S";
    int error = 0;

    for(i = 1; i < argc; i++) {
        if (strcmp(argv[i],"-t") == 0) {
            if (strncmp(argv[i+1], "0x", 2) == 0 || strncmp(argv[i+1], "0X", 2) == 0) {
                char *end;
                targetAddress = strtol(argv[i+1]+2, &end, 16);
            } else {
                targetAddress = atoi(argv[i+1]);
            }
            i++;
        } else if (strncmp(argv[i], "-target=", 8) == 0 || endswith(argv[i], ".xn")) {
            target = argv[i];
        } else if (strcmp(argv[i], "-b")==0) {
            decompressorAddress = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "-o")==0) {
            xeOutput = argv[i+1];
            if (!endswith(xeOutput, ".xe")) {
                fprintf(stderr, "output file must end with .xe\n");
                error = 1;
            } else {
                int len = strlen(xeOutput);
                assOutput = strdup(xeOutput);
                assOutput[len-1] = '\0';
                assOutput[len-2] = 'S';
            }
            i++;
        } else {
            if (fb == NULL) {
                fb = fopen(argv[i], "rb");
                if (fb == NULL) {
                    fprintf(stderr, "Cannot open input binary\n");
                    error = 1;
                }
            } else {
                fprintf(stderr, "Only one input binary\n");
                error = 1;
            }
        }
    }
    
    if (target == 0 || error || fb == NULL) {
        fprintf(stderr, "Usage: %s [options] -target=XXX <binary>.bin\n", argv[0]);
        fprintf(stderr, "     -t address: sets address for decompressed image, default 0x1B000\n");
        fprintf(stderr, "     -b address: sets address for decompressor itself, default 0x10000\n");
        fprintf(stderr, "     -o outputBinary: sets name for .xe file, default a.xe\n");
        fprintf(stderr, "     <binary>.bin: use xobjdump --split --strip to obtain this\n");
        fprintf(stderr, "     -target=XXX: target description\n");
        exit(1);
    }
    inSize = fread(input, 1, 16384, fb);
    fass = fopen(assOutput, "w");

    findMarkers();

    fprintf(stderr, "Finding parameter settings\n");
    for(ml0 = 2; ml0 <= 7; ml0++) {
        for(ml1 = 2; ml1 < ml0; ml1++) {
            for(ml2 = 2; ml2 <= 7; ml2++) {
                compress(ml0, ml1, ml2, 0, NULL);
                if (outSize < minSize) {
                    minSize = outSize;
                    bestml0 = ml0;
                    bestml1 = ml1;
                    bestml2 = ml2;
                    fprintf(stderr, "Better: %d %d %d: %d\n", ml0, ml1, ml2, outSize);
                }
            }
        }
    }
    compress(bestml0, bestml1, bestml2, targetAddress, fass);
    fclose(fass);
    fprintf(stderr, "Compiling %s %s into %s\n", assOutput, target, xeOutput);
    sprintf(cmd, "xcc -nostartfiles -Xmapper --image-base -Xmapper %s %s %s -o %s", decompressorAddress, assOutput, target, xeOutput);
    if (system(cmd) != 0) {
        exit(1);
    }
    status = xsi_create(&xsim, xeOutput);
    assert(status == XSI_STATUS_OK);

    for(i = 0; i < inSize; i++) {
        c = ~input[i];
        xsi_write_mem(xsim, "stdcore[0]", targetAddress + i, 1, &c);
    }
    fprintf(stderr, "Simulating\n");
    i = 0;
    do {
        i++;
        status = xsi_clock(xsim);
        assert(status == XSI_STATUS_OK || status == XSI_STATUS_DONE );
        xsi_read_mem(xsim, "stdcore[0]", targetAddress + inSize - 1, 1, &c);
    } while(c == (~input[inSize - 1] & 0xff) && i < outSize * 100);
    fprintf(stderr, "Done after %d steps\n", i);
    errors = 0;
    for(i = 0; i < inSize; i++) {
        xsi_read_mem(xsim, "stdcore[0]", targetAddress + i, 1, &c);
        if (c != input[i]) {
            printf("Error on address %d: expected 0x%02x got 0x%02x\n", i, input[i], c);
            errors++;
        }
    }
    status = xsi_terminate(xsim);
    if (errors == 0) {
        printf("Comparison complete, no errors\n");
    }

    return errors == 0 ? 0 : 1;
}
