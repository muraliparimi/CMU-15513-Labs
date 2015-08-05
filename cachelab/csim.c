// C/C++ File
// FILE:     csim.c
// CREATED:  2015-06-22 20:38:27
// MODIFIED: 2015-06-22 20:38:27
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include "cachelab.h"

int numOfSet = 1; //initial value of number of sets
int verbose = 0; //default value for verbose is 0(turned off)

struct queueNode { //denote data structure for LRU queue
    int setNum;
    int lineNum;
    int tag;
    struct queueNode* next;
};
struct queueNode* head;
struct queueNode* tail;

struct cache_line { //cache line definition
    int valid;
    long long int tag;
};

struct cache_line** build_cache(int setIndex, int lines) {
    //build cache using set index and number of lines
    while (setIndex --) {
        numOfSet = numOfSet << 1;
    }
    struct cache_line** cache;
    cache = (struct cache_line**)malloc(numOfSet * sizeof(struct cache_line*));
    int i = 0, j = 0;
    for (; i < numOfSet; i ++)
        cache[i] = (struct cache_line*)malloc(lines *  sizeof(struct cache_line));

    for (i = 0; i < numOfSet; i ++) { //initialize bits of every cache line
        for (j = 0; j < lines; j ++) {
            cache[i][j].valid = 0;
            cache[i][j].tag = 0;
        }
    }

    return cache;
}

void freeCache(struct cache_line** cache) { //free and allocated space for cache
    int i = 0;
    for (i = 0; i < numOfSet; i ++) {
        free((void*)cache[i]);
    }

    free((void*)cache);
}

int getSetIndex(int address, int setIndex, int blockBits) {
    // get set index of an address
    int temp = address >> blockBits;
    int mask = 1;
    int i = 1;
    for (; i <= setIndex; i ++) {
        mask = mask << 1;
    }
    mask --;
    return (temp & mask);
}

int findCache(struct cache_line** cache, int setNumber, int lines, int tag) {
    //find the match cache line
    int index = 0;
    int tempValid = 0;
    int tempTag = 0;
    for (index = 0; index < lines; index ++) {
        tempValid = cache[setNumber][index].valid;
        tempTag = cache[setNumber][index].tag;
        if (tempValid == 1 && tempTag == tag) {
            cache[setNumber][index].valid = 1;
            cache[setNumber][index].tag = tag;
            return 1;
        }
    }
    return 0;
}

int replaceCache(struct cache_line** cache, int setNumber, int lines, int tag) {
    // replace cache line in cache memory and LRU queue
    int index = 0;
    for (; index < lines; index ++) {
        if (cache[setNumber][index].valid == 0) {
            cache[setNumber][index].valid = 1;
            cache[setNumber][index].tag = tag;
            struct queueNode* newNode = (struct queueNode*)malloc(sizeof(struct queueNode));
            newNode->setNum = setNumber;
            newNode->tag = tag;
            newNode->lineNum = index;
            newNode->next = NULL;
            tail->next = newNode;
            tail = newNode;
            return 0; //find an empty cache line, no need to do eviction
        }
    }

    struct queueNode* walker = head->next;
    struct queueNode* pre = head;

    //no empty cache lines remained, need to evict one in LRU queue
    while (walker != NULL) {
        if (walker->setNum == setNumber) {
            if (tail != walker) {
                pre->next = walker->next;
                tail->next = walker;
                walker->next = NULL;
                tail = walker;
            }
            walker->tag = tag;
            cache[walker->setNum][walker->lineNum].tag = tag;
            return 1;
        }
        pre = pre->next;
        walker = walker->next;
    }

    // codes below should not be executed!
    fprintf(stderr, "Something went wrong....\n");
    exit(EXIT_FAILURE);
}

void updateCache(int setNumber, int tag) {
    //update cache in LRU queue and put it in the tail
    struct queueNode* walker = head->next;
    struct queueNode* pre = head;
    while (walker != NULL) {
        if (walker->setNum == setNumber && walker->tag == tag) {
            if (walker != tail) {
                pre->next = walker->next;
                tail->next = walker;
                walker->next = NULL;
                tail = walker;
            }
            return ;
        }
        pre = pre->next;
        walker = walker->next;
    }
    return ;
}

void printVerboseInfo(char* str, int hit, int miss, int evict) {
    //print the verbose mode information
    printf("%s %d hit, %d miss, %d eviction\n", str, hit, miss, evict);
}

void freeQueue() { //free LRU queue
    struct queueNode* walker = head->next;
    struct queueNode* temp;
    while (walker != NULL) {
       temp = walker->next;
       free(walker);
       walker = temp;
    }
}

void simulate(FILE* fp, int setIndex, int lines, int blockBits) {
    //simulate cache memory
    struct cache_line** cache = build_cache(setIndex, lines);
    char blank;
    char Op;
    int addressVal;
    int offset;

    char buf[1000];
    int hit = 0, miss = 0, eviction = 0;
    int setNumber;
    int tag;
    int tempHit, tempMiss, tempEvict;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        tempHit = tempMiss = tempEvict = 0;
        sscanf(buf, "%c %c %x,%d",&blank, &Op, &addressVal, &offset);
        if (blank == 'I')
            continue;

        setNumber = getSetIndex(addressVal, setIndex, blockBits);
        tag = addressVal >> (setIndex + blockBits);

        if (findCache(cache, setNumber, lines, tag)) {
            hit ++;
            tempHit ++;
            updateCache(setNumber, tag);
        } else {
            miss ++;
            tempMiss ++;
            tempEvict += replaceCache(cache, setNumber, lines, tag);
        }
        if (Op == 'M') {
            //if the operation is M, need to find cache again
            if (findCache(cache, setNumber, lines, tag)) {
                hit ++;
                tempHit ++;
                updateCache(setNumber, tag);
            } else {
                miss ++;
                tempMiss ++;
                tempEvict += replaceCache(cache, setNumber, lines, tag);
            }
        }
        eviction += tempEvict;
        if (verbose)
            printVerboseInfo(buf, tempHit, tempMiss, tempEvict);
    }
    //printf("%d %d %d\n", hit, miss, eviction);
    freeCache(cache);
    freeQueue();
    printSummary(hit, miss, eviction);
}


int main(int argc, char *argv[]) {
    int setIndex;
    int lines;
    int blockBits;
    char* input;

    int opt;
    while ((opt = getopt(argc, argv, "vs:E:b:t:")) != -1) {
        switch(opt) {
            case 'v':
                verbose = 1;
                break;
            case 's':
                setIndex = atoi(optarg);
                break;
            case 'E':
                lines = atoi(optarg);
                break;
            case 'b':
                blockBits = atoi(optarg);
                break;
            case 't':
                input = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-v] -s argument -E argument\
                         -b argument -t argument\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    /*
     *printf("%d %d %d %s\n", setIndex, lines, blockBits, input);
     *for debug use
     */
    if (setIndex <= 0 || lines <= 0 || blockBits <= 0 || input == NULL) {
        fprintf(stderr, "Usage: %s [-v] -s argument -E argument -b \
                argument -t argument\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    FILE *fp = fopen(input, "r");
    if (fp == NULL) {
        fprintf(stderr, "File does not exists!");
        exit(EXIT_FAILURE);
    }

    head = (struct queueNode*)malloc(sizeof(struct queueNode));
    tail = head;
    head->next = NULL;
    simulate(fp, setIndex, lines, blockBits);
    fclose(fp);

    return 0;
}

