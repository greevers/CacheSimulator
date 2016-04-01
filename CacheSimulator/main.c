//
//  main.c
//  CacheSimulator
//
//  Created by Sean Greevers on 1/28/16.
//  Copyright © 2016 Sean Greevers. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cachelab.h"

#define ADDRESS_SIZE 64
#define INVALID 0
#define VALID 1
#define HIT 5
#define MISS 6
#define EVICT 7
#define INSTRUCTION 'I'
#define LOAD 'L'
#define STORE 'S'
#define MODIFY 'M'

int sbits = 0, bbits = 0, S = 0, E = 0, hits = 0, misses = 0, evictions = 0;
char help = 0, verbose = 0;
long long LRU = 0;

typedef struct {
    char validBit;
    long tag;
    long setIndex;
    long blockOffset;
    long lruCount;
} CacheLine;

typedef struct {
    CacheLine *lines;
} CacheSet;

long bitMask(int bits) {
    long bitMask = 1;
    int i;
    for (i = 1; i < bits; i++) {
        bitMask <<= 1;
        bitMask++;
    }
    return bitMask;
}

long addressTag(long address) {
    long tagMask = bitMask(ADDRESS_SIZE - (bbits + sbits + 1));
    long tag = address;
    tag >>= (bbits + sbits);
    tag &= tagMask;
    return tag;
}

long addressSetIndex(long address) {
    long setIndexMask = bitMask(sbits);
    long setIndex = address;
    setIndex >>= bbits;
    setIndex &= setIndexMask;
    return setIndex;
}

long addressBlockOffset(long address) {
    long blockOffsetMask = bitMask(bbits);
    long blockOffset = address;
    blockOffset &= blockOffsetMask;
    return blockOffset;
}

void buildCache(CacheSet *cacheSets) {
    int i, k;
    for (i = 0; i < S; i++) {
        cacheSets[i].lines = (CacheLine*)malloc(sizeof(CacheLine) * E);
        for (k = 0; k < E; k++) {
            CacheLine cacheLine;
            cacheLine.validBit = INVALID;
            cacheLine.tag = 0;
            cacheLine.setIndex = 0;
            cacheLine.blockOffset = 0;
            cacheLine.lruCount = 0;
            *(cacheSets[i].lines + k) = cacheLine;
        }
    }
}

char searchCache(CacheSet currentSet, long tag, long setIndex, long blockOffset) {
    char hitType = MISS;
    long smallestLRU = LRU;
    int i, lineNum = 0;
    
    // Find line hit if it exists in cache
    for (i = 0; i < E; i++) {
        CacheLine cacheLine = currentSet.lines[i];
        
        if (cacheLine.lruCount < smallestLRU) {
            smallestLRU = cacheLine.lruCount;
            lineNum = i;
        }
        
        if ((cacheLine.validBit == VALID) && (cacheLine.tag == tag)) {
            cacheLine.lruCount = ++LRU;
            currentSet.lines[i] = cacheLine;
            hitType = HIT;
            hits++;
            break;
        }
    }
    
    // Determine if evict or only miss & load block
    if (hitType != HIT) {
        CacheLine currentLine = currentSet.lines[lineNum];
        
        if ((currentLine.validBit == VALID) && (currentLine.tag != tag)) {
            hitType = EVICT;
            evictions++;
        } else {
            hitType = MISS;
        }
        
        currentLine.validBit = VALID;
        currentLine.tag = tag;
        currentLine.setIndex = setIndex;
        currentLine.blockOffset = blockOffset;
        currentLine.lruCount = ++LRU;
        currentSet.lines[lineNum] = currentLine;
        misses++;
    }
    
    return hitType;
}

char cacheOperation(CacheSet *cacheSets, long address) {
    // Get tag, setIndex, blockOffset values
    long tag = addressTag(address);
    long setIndex = addressSetIndex(address);
    long blockOffset = addressBlockOffset(address);
    
    // Find set, search for line/block
    CacheSet currentSet = cacheSets[setIndex];
    return searchCache(currentSet, tag, setIndex, blockOffset);
}

void printVerbose(char hitType, char* addressLine) {
    char message[20];
    switch (hitType) {
        case HIT:
            strcpy(message, "hit");
            break;
        case EVICT:
            strcpy(message, "miss eviction");
            break;
        case MISS:
            strcpy(message, "miss");
            break;
        case (HIT + HIT):
            strcpy(message, "hit hit");
            break;
        case (MISS + HIT):
            strcpy(message, "miss hit");
            break;
        case (EVICT + HIT):
            strcpy(message, "miss eviction hit");
            break;
        default:
            break;
    }
    printf("%s %s\n", addressLine, message);
}

void printHelpAndExit(const char *arg) {
    printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", arg);
    printf("• -h: Optional help flag that prints usage info\n");
    printf("• -v: Optional verbose flag that displays trace info\n");
    printf("• -s <s>: Number of set index bits (S = 2s is the number of sets)\n");
    printf("• -E <E>: Associativity (number of lines per set)\n");
    printf("• -b <b>: Number of block bits (B = 2b is the block size)\n");
    printf("• -t <tracefile>: Name of the valgrind trace to replay\n");
    exit(1);
}

int main(int argc, const char * argv[]) {
    int i;
    FILE *trace = NULL;
    for (i = 0; i < argc; i++) {
        char arg = *++argv[i];
        
        switch (arg) {
            case 'h':
                help = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 's':
                sbits = atoi(argv[i+1]);
                S = 1 << sbits;
                break;
            case 'E':
                E = atoi(argv[i+1]);
                break;
            case 'b':
                bbits = atoi(argv[i+1]);
                break;
            case 't':
                trace = fopen(argv[i+1], "r");
                break;
            default:
                break;
        }
    }
    
    if (help == 1) {
        printHelpAndExit(argv[0]);
    }
    
    CacheSet cacheSets[S];
    buildCache(cacheSets);
    
    if (trace != NULL) {
        long address;
        int bufLen = 30;
        char buffer[bufLen], instruction, blockSize;
        while (fgets(buffer, bufLen, trace) != NULL) {
            sscanf(buffer, " %c %lx,%c", &instruction, &address, &blockSize);
            
            if (instruction != INSTRUCTION) {
                for (i = 0; i < bufLen; i++) {
                    if (buffer[i] == '\n') {
                        buffer[i] = '\0';
                    }
                }
                
                char hitType = 0;
                if (instruction == MODIFY) {
                    for (i = 0; i < 2; i++) {
                        hitType += cacheOperation(cacheSets, address);
                    }
                } else {
                    hitType = cacheOperation(cacheSets, address);
                }
                
                if (verbose == 1) {
                    printVerbose(hitType, buffer);
                }
            }
        }
        fclose(trace);
    } else {
        printf("Error: Must specify trace file, see usage\n");
        printHelpAndExit(argv[0]);
    }
    
    for (i = 0; i < S; i++) {
        free(cacheSets[i].lines);
    }
    
    printSummary(hits, misses, evictions);
    return 0;
}
