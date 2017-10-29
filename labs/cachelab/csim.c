/*
 * =============================================================
 * CS360 Cachelab 
 * 
 * @author: Braden Hitchcock
 * @netid: hitchcob
 * @date: 2.14.2017 
 *
*/

#include "cachelab.h"
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


// ***************************************************
// Define variables
#define DEBUG_ENABLED 0

// 64 bit varaible to hold memory addresses
typedef unsigned long long int mem_addr_t;

// Given cache parameters in an (s, E, B) format
int setBits;            // s - # of set index bits (2^s)
int numLines;           // E - # of lines per set
int dataBits;           // B - # of data bytes per line
int verboseFlag = 0;    // flag for verbose mode
char* filepath;         // path to trace file

int hits;               // number of cache hits
int misses;             // number of cache misses 
int evictions;          // number of cache evictions (LRU)

typedef struct {
    int valid;
    int accessCount;
    mem_addr_t tag;
    char* block;
} Line;

typedef struct{
    Line* lines;
} Set;

// Our cache
typedef struct{
    Set* sets;
} Cache;

Cache cache;



// ***************************************************
// Define functions
int initializeArgs(int argc, char* argv[]);
void printUsage();
void printError();
int initializeCache();
int freeCache();
int findEmptyLine(Set* set);
int findLRU(Set* set, int* usedLines);
char* getCWD();
int cacheSimulate(int address);


// *************************************************************
// *************************************************************
// *************************************************************


int main(int argc, char* argv[]){

    // Verify and Initialize the arguments
    if(initializeArgs(argc,argv)) return 0;

    // Initialize the cache 
    initializeCache();

    // Find current working directory (may not need to)
    //char* CWD = getCWD();

    // Take as input the trace file and open for reading
    FILE* inTraceFile = fopen(filepath,"r");
    if(inTraceFile == NULL){
        printf("ERROR OPENING FILE: could not find file at '%s'",filepath);
        return 1;
    }

    // Parse the file, incrementing the appropriate variables
    char cmd;
    int addr;
    int size;

    while(fscanf(inTraceFile," %s %11x, %d ", &cmd, &addr, &size) == 3){
        //printf("  %s %11x, %d\n",&cmd,addr,size);
        switch(cmd){
            case 'I':   break;
            case 'L':   
            case 'S':   cacheSimulate(addr);
                        break;
            case 'M':   cacheSimulate(addr);
                        cacheSimulate(addr);
                        break;
            default:    break;
        }
    }

    // Free the cache 
    freeCache();

    // Print the results
    printSummary(hits, misses, evictions);

    // Close the file 
    fclose(inTraceFile);
    
    return 0;
}

/*
 * Initialize Args
 * takes the command line arguments and performs all 
 * checks and assigns values to program variables
 *
*/
int initializeArgs(int argc, char* argv[]){

    int reqArgs = 4;
    int i;
    char* arg;

    for(i = 1; i < argc; i++){
 
        arg = argv[i];

        if(strcmp(arg,"-h") == 0){
            printUsage();
            return 1;
        }
        else if(strcmp(arg, "-v") == 0){
            verboseFlag = 1;
        }
        else if(strcmp(arg, "-s") == 0){
            setBits = atoi(argv[++i]);
            reqArgs--;
        }
        else if(strcmp(arg, "-E") == 0){
            numLines = atoi(argv[++i]);
            reqArgs--;
        }
        else if(strcmp(arg, "-b") == 0){
            dataBits = atoi(argv[++i]);
            reqArgs--;
        }
        else if(strcmp(arg, "-t") == 0){
            filepath = argv[++i];
            reqArgs--;
        }
        else{
            printError("ERROR: Invalid argument");
            return 1;
        }

    }

    if(reqArgs){
        printError("ERROR: Missing required command line arguments");
        return 1;
    }

    return 0;

}

/*
 * Function to print the usage information about this program 
*/
void printUsage(){

    // Display syntax
    char* usg = "./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>";

    // Display help for each argument
    char* h = "  -h\t\tPrint this help message.";
    char* v = "  -v\t\tOptional verbose flag.";
    char* s = "  -s <num>\tNumber of set index bits.";
    char* e = "  -E <num>\tNumber of lines per set.";
    char* b = "  -b <num>\tNumber of block offset bits";
    char* t = "  -t <file>\tTrace file.";

    // Display Examples
    char* ex1 = "  linux> ./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace";
    char* ex2 = "  linux> ./csim-ref -v -s 8 -E 2 -b 4 -t traces/yi.trace";

    // Print to screen
    printf("Usage: %s\nOptions:\n%s\n%s\n%s\n%s\n%s\n%s\n\nExamples:\n%s\n%s\n",
            usg, h, v, s, e, b, t, ex1, ex2);
    
    return;

}

/*
 * Function to print error messages followed by the usage information
 * for the program 
*/
void printError(char* e){
    printf("%s\n",e);
    printUsage();
    return;
}

/*
 * Initialize cache 
 * Sets up a space in memory using malloc() for the cache 
*/
int initializeCache(){

    int set;
    int line;

    int numSets = pow(2,setBits);
    //int numDataCells = pow(2,dataBits);

    // NULL variables;
    Set tmpSet;
    Line tmpLine;

    cache.sets = (Set*) malloc(sizeof(Set) * numSets);

    // Allocate memory for each line and set 
    for(set = 0; set < numSets; set++){
        tmpSet.lines = (Line*) malloc(sizeof(Line) * numLines);
        cache.sets[set] = tmpSet;
        for(line = 0; line < numLines; line++){
            tmpLine.valid = 0;
            tmpLine.accessCount = 0;
            tmpLine.tag = 0;
            tmpSet.lines[line] = tmpLine;
        }
    }
    return 0;
}

/*
 * Frees the allocated memory for the cache 
*/
int freeCache(){

    int i;
    int numSets = pow(2,setBits);
    for(i = 0; i < numSets; i++){
        if(cache.sets[i].lines != NULL){
            free(cache.sets[i].lines);
        }
    }
    if(cache.sets != NULL){
        free(cache.sets);
    }
    return 0;
}

/*
 * Searches the given set for a line with it's valid bit 
 * set to zero
*/
int findEmptyLine(Set* set){
    int i;
    for(i = 0; i < numLines; i++){
        if(set->lines[i].valid == 0){
            return i;
        }
    }
    return -1;
}

/*
 * Uses a least recently used replacement policy to find the line 
 * in the given set that should be evicted if the cache is 
 * full. Does so by checking min and max counts of access for each 
 * line (almost like a time stamp)
*/
int findLRU(Set* set, int* usedLines){
    int line;
    int maxUsed = set->lines[0].accessCount;
    int minUsed = set->lines[0].accessCount;
    int lruIndex = 0;
    Line currentLine;

    for(line = 0; line < numLines; line++){
        currentLine = set->lines[line];

        if(minUsed > currentLine.accessCount){
            lruIndex = line;
            minUsed = currentLine.accessCount;
        }
        if(maxUsed < currentLine.accessCount){
            maxUsed = currentLine.accessCount;
        }
    }

    usedLines[0] = minUsed;
    usedLines[1] = maxUsed;

    return lruIndex;
}

/*
char* getCWD(){
    // Determine the current working directory (may not need this)
    char* CWD;
    if(getcwd(CWD,sizeof(CWD)) != NULL);
    else perror("Could not determine current working directory");
    if(DEBUG_ENABLED){
        printf("Current working directory: %s\n",CWD);
    }
    return CWD;
}
*/

/*
 * Simulate Cache 
 * this function simulates the cache given an address 
 * called by the main() function
*/
int cacheSimulate(int address){

    int line;
    int cFull = 1;
    int hitFlag = 0;

    // Mask the flag bits from the address 
    int tagBits = (64 - (setBits + dataBits));
    unsigned int shift = address << tagBits;
    unsigned int setIndex = shift >> (tagBits + dataBits);

    // Determine which tag address to check in the cache 
    int inputTag = address >> (setBits + dataBits);

    // Look into the address specified set index 
    Set currentSet = cache.sets[setIndex];

    // Go through every line to determine if hit or miss or evict 
    for(line = 0; line < numLines; line++){

        Line currentLine = currentSet.lines[line];

        if(currentLine.valid){
            if(currentLine.tag == inputTag){
                currentLine.accessCount++;
                hitFlag = 1;
                hits++;
                currentSet.lines[line] = currentLine;
            }
        }
        else if(!currentLine.valid && cFull){
            cFull = 0;
        }

    }

    // If there was no hit, increment the miss count 
    if(!hitFlag){
        misses++;
    }
    else{
        return 0;
    }

    // Assuming we reach this point, we have a cache miss 
    // If the cache is full, then we need to evict. Otherwise we 
    // need to find an empty line in the set 
    int* usedLines = (int*)malloc(sizeof(int)*2);
    int lruIndex = findLRU(&currentSet,usedLines);

    if(cFull){
        evictions++;
        currentSet.lines[lruIndex].tag = inputTag;
        currentSet.lines[lruIndex].accessCount = usedLines[1] + 1;
    }
    else{
        int mtLineIndex = findEmptyLine(&currentSet);
        currentSet.lines[mtLineIndex].tag = inputTag;
        currentSet.lines[mtLineIndex].valid = 1;
        currentSet.lines[mtLineIndex].accessCount = usedLines[1] + 1;
    }
    free(usedLines);

    return 0;

}