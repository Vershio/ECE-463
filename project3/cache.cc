//-------------------------------------
//      ECE 463 Project 3
//          Alan Zheng
//      NCSU Spring 2021
//-------------------------------------
#include "cache.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <iomanip>
#include <cmath>
#include "bitset"


using namespace std;

cache::cache(unsigned size, 
      unsigned associativity,
      unsigned line_size,
      write_policy_t wr_hit_policy,
      write_policy_t wr_miss_policy,
      unsigned hit_time,
      unsigned miss_penalty,
      unsigned address_width
){
    //reset Cache
    numRead = 0;
    numReadMiss = 0;
    numWrite = 0;
    numWriteMiss = 0;
    numEvict = 0;
    numMemWrite = 0;
    AvgMem_time = 0;
    //Cache Configuration
    c_size = size;
    numWays = associativity;
    blockSize = line_size;
    hitPolicy = wr_hit_policy;
    missPolicy = wr_miss_policy;
    hitTime = hit_time;
    missPenalty = miss_penalty;
    memAddressSize = address_width;

    //Bits
    c_set = c_size/(blockSize*numWays);
    blkoffBits = log2(blockSize);
    setBits = log2(c_set);
    tagBits = memAddressSize - (setBits+blkoffBits);

    //maskBlockOffsetBits = getMemAddrBits(blkoffBits,0);
    maskSetBits = getMemAddrBits(setBits,blkoffBits);
    //maskTagBits = getMemAddrBits(tagBits,(blkoffBits+setBits));

    cacheTable = vector<vector<cache_entries>>(numWays,vector<cache_entries>(c_set));

    for(unsigned i = 0; i < numWays; i++){
        for(unsigned j = 0; j < blkoffBits; j++){
            cacheTable[i][j].valid = 0;
            cacheTable[i][j].dirty = 0;
            cacheTable[i][j].set = 0;
            cacheTable[i][j].tag = 0;
            cacheTable[i][j].LRU = 0;
        }
    }
}

void cache::print_configuration(){
	/* edit here */
	cout << "CACHE CONFIGURATION" << endl;
    cout << "size = " << (c_size/1024) << " KB" <<endl;
    cout << "associativity = " << numWays << "-way" <<endl;
    cout << "cache line size = " << blockSize << " B" <<endl;
    cout << "write hit policy = ";
    switch(hitPolicy){
        case WRITE_THROUGH:
            cout << "write-through" << endl;
            break;
        case WRITE_BACK:
            cout << "write-back" << endl;
            break;
    }
    cout << "write miss policy = ";
    switch(missPolicy){
        case WRITE_ALLOCATE:
            cout << "write-allocate" << endl;
            break;
        case NO_WRITE_ALLOCATE:
            cout << "no-write-allocate" << endl;
            break;
    }
    cout << "cache hit time = " << hitTime << " CLK" <<endl;
    cout << "cache miss penalty = " << missPenalty << " CLK" <<endl;
    cout << "memory address width = " << memAddressSize << " bits" <<endl;
}

cache::~cache(){
	cacheTable.clear();
	numRead = 0;
	numReadMiss = 0;
	numWrite = 0;
	numWriteMiss = 0;
	numEvict = 0;
	numMemWrite = 0;
	AvgMem_time = 0;
	number_memory_accesses = 0;
}

void cache::load_trace(const char *filename){
   stream.open(filename);
}

void cache::run(unsigned num_entries){
    long long memoryTagBits;
    long long memorySetBits;
    long long cacheSetIndex;

    unsigned evictWayIndex;
    bool isFull = true;

    unsigned first_access = number_memory_accesses;
    string line;
    unsigned line_nr=0;

    while (getline(stream,line)){
        line_nr++;
        char *str = const_cast<char*>(line.c_str());

        // tokenize the instruction
        char *op = strtok (str," ");
        char *addr = strtok (NULL, " ");
        address_t address = strtoull(addr, NULL, 16);

        if(op[0] == 'r'){
            numRead++;
            //cout << "read" << endl;
            if(read(address)){
                numReadMiss++;
                //cout << "read miss" << endl;
                memoryTagBits = address >> (blkoffBits + setBits);
                memorySetBits = (address >> blkoffBits) & maskSetBits;
                cacheSetIndex = memorySetBits % c_set;

                for(unsigned i = 0; i < numWays; i++){
                    if(!cacheTable[i][cacheSetIndex].valid){
                        cacheTable[i][cacheSetIndex].valid = 1;
                        cacheTable[i][cacheSetIndex].set = memorySetBits;
                        cacheTable[i][cacheSetIndex].tag = memoryTagBits;
                        cacheTable[i][cacheSetIndex].LRU = number_memory_accesses;
                        isFull = false;
                        break;
                        //cout << "cache added" << endl;
                    }
                }
                if(isFull){
                    evictWayIndex = evict(cacheSetIndex);
                    //cout << "cache is full, evict Index: " << evictWayIndex << endl;
                    if(cacheTable[evictWayIndex][cacheSetIndex].dirty == 1){
                        cacheTable[evictWayIndex][cacheSetIndex].dirty = 0;
                        numMemWrite++;
                    }
                    cacheTable[evictWayIndex][cacheSetIndex].set = memorySetBits;
                    cacheTable[evictWayIndex][cacheSetIndex].tag = memoryTagBits;
                    cacheTable[evictWayIndex][cacheSetIndex].LRU = number_memory_accesses;
                }
            }
            else{
                //cout << "read hit" << endl;
            }
        }
        else if(op[0] == 'w'){
            numWrite++;
            //cout << "write" << endl;
            if(write(address)){
                numWriteMiss++;
                //cout << "write miss" << endl;
                if(missPolicy == WRITE_ALLOCATE){
                    memoryTagBits = address >> (blkoffBits + setBits);
                    memorySetBits = (address >> blkoffBits) & maskSetBits;
                    cacheSetIndex = memorySetBits % c_set;

                    for(unsigned i = 0; i < numWays; i++){
                        if(!cacheTable[i][cacheSetIndex].valid){
                            cacheTable[i][cacheSetIndex].valid = 1;
                            if(hitPolicy == WRITE_BACK){
                                cacheTable[i][cacheSetIndex].dirty = 1;
                            }
                            else if(hitPolicy == WRITE_THROUGH){
                                numMemWrite++;
                            }
                            cacheTable[i][cacheSetIndex].set = memorySetBits;
                            cacheTable[i][cacheSetIndex].tag = memoryTagBits;
                            cacheTable[i][cacheSetIndex].LRU = number_memory_accesses;
                            isFull = false;
                            break;
                            //cout << "cache added" << endl;
                        }
                    }
                    if(isFull){
                        evictWayIndex = evict(cacheSetIndex);
                        //cout << "cache is full, evict Index: " << evictWayIndex << endl;
                        if(cacheTable[evictWayIndex][cacheSetIndex].dirty){
                            cacheTable[evictWayIndex][cacheSetIndex].dirty = 0;
                            numMemWrite++;
                        }
                        if(hitPolicy == WRITE_BACK){
                            cacheTable[evictWayIndex][cacheSetIndex].dirty = 1;
                        }
                        else if(hitPolicy == WRITE_THROUGH){
                            numMemWrite++;
                        }
                        cacheTable[evictWayIndex][cacheSetIndex].set = memorySetBits;
                        cacheTable[evictWayIndex][cacheSetIndex].tag = memoryTagBits;
                        cacheTable[evictWayIndex][cacheSetIndex].LRU = number_memory_accesses;
                    }
                }
                else if(missPolicy == NO_WRITE_ALLOCATE){
                    if(hitPolicy == WRITE_THROUGH) numMemWrite++;
                }
            }
            else{
                if(hitPolicy == WRITE_THROUGH) numMemWrite++;
            }
        }
        isFull = true;
        number_memory_accesses++;
        if (num_entries!=0 && (number_memory_accesses-first_access)==num_entries)
            break;
    }
}

void cache::print_statistics(){
    float missRate;
	cout << "STATISTICS" << endl;
	/* edit here */
	missRate = (float(numWriteMiss) + float(numReadMiss))/float(number_memory_accesses);
	AvgMem_time = float(hitTime) + (missRate * float(missPenalty));

    cout << "memory accesses = " << dec << number_memory_accesses <<endl;
    cout << "read = " << dec << numRead <<endl;
    cout << "read misses = " << dec << numReadMiss <<endl;
    cout << "write = " << dec << numWrite <<endl;
    cout << "write misses = " << dec << numWriteMiss <<endl;
    cout << "evictions = " << dec << numEvict <<endl;
    cout << "memory writes = " << dec << numMemWrite <<endl;
    cout << "average memory access time = " << dec << AvgMem_time <<endl;

}

access_type_t cache::read(address_t address){
	long long cachetag = address >> (blkoffBits + setBits);
	long long cacheset = (address >> blkoffBits) & maskSetBits;
	long long setnum = cacheset % c_set;
	for(unsigned i = 0; i < numWays; i++){
	    if(cacheTable[i][setnum].valid == 1 &&
	    cacheTable[i][setnum].tag == cachetag){
            cacheTable[i][setnum].LRU = number_memory_accesses;
            return HIT;
	    }
	}
	return MISS;
}

access_type_t cache::write(address_t address){
    long long cachetag = address >> (blkoffBits + setBits);
    long long cacheset = (address >> blkoffBits) & maskSetBits;
    long long setnum = cacheset % c_set;
    for(unsigned i = 0; i < numWays; i++){
        if(cacheTable[i][setnum].valid == 1 &&
           cacheTable[i][setnum].tag == cachetag){
            if(hitPolicy == WRITE_BACK) cacheTable[i][setnum].dirty = 1;
            cacheTable[i][setnum].LRU = number_memory_accesses;
            return HIT;
        }
    }
	return MISS;
}

void cache::print_tag_array(){
	cout << "TAG ARRAY" << endl;
    for (unsigned i = 0; i < numWays; i++) {
        cout << "BLOCKS " << i << endl;
        if (hitPolicy == WRITE_BACK) {
            cout << setfill(' ') << setw(7) << "index" << setw(6) << "dirty" << setw(4 + tagBits/4) << "tag" <<endl;
            for (unsigned j = 0; j < c_set; j++) {
                if (cacheTable[i][j].valid == 1) {
                    cout << setfill(' ') << setw(7) << dec << cacheTable[i][j].set << setw(6) << dec << cacheTable[i][j].dirty << setw(4) << "0x" << hex << cacheTable[i][j].tag <<endl;
                }
            }

        }
        else {
            cout << setfill(' ') << setw(7) << "index" << setw(6) << setw(4 + tagBits/4) << "tag" <<endl;
            for (unsigned j = 0; j < c_set; j++) {
                if (cacheTable[i][j].valid == 1) {
                    cout << setfill(' ') << setw(7) << dec << cacheTable[i][j].set << setw(4) << "0x" << hex << cacheTable[i][j].tag <<endl;
                }
            }
        }

    }

}

unsigned cache::evict(unsigned index){
	unsigned way = 0;
	unsigned smallestLRU = number_memory_accesses;

	numEvict++;
	for(unsigned i = 0; i < numWays; i++){
	    if(cacheTable[i][index].LRU <= smallestLRU){
	        smallestLRU = cacheTable[i][index].LRU;
	        way = i;
	    }
	}
	return way;
}

long long cache::getMemAddrBits(long long numofbits, long long position){
    long long Bits = UNDEFINED;
    long long mask = (((1 << numofbits) - 1) & (Bits >> (position)));
    std::bitset<64> a(mask);
    //cout << "mask is: " <<  a << " #" << dec << mask << endl;
    return mask;
}
