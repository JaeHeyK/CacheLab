/*
 * Name: Beyza Bozbey
 * USCID: 9125537373
 * username: bozbey
 * USC email: bozbey@usc.edu
 */

#include "cachelab.h"
#include "dogfault.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

typedef struct Line
{
	unsigned long long block;
	short valid;
	unsigned long long tag;
	// holds the place in used lines
	// the greater the rate, that much recent it is
	int r_rate;
	int p_rate;
}Line;

typedef struct Set
{
	Line *lines;
	int recentRate;
	int placementRate;
}Set;

typedef struct Cache
{
	int setBits;
	int linesPerSet;
	int blockBits;
	Set *sets;
	int eviction_count;
	int hit_count;
	int miss_count;
	// 0 for LRU, 1 for FIFO
	short eviction_policy;
	short displayTrace;
}Cache;

// Check if the address is found in the cache. If so, return -1
// If not, then check if there is an empty spot. If so, place the address there and return 2
// If there is no empty spot, then return -2
unsigned long long checkCache(const unsigned long long address, const Cache *cache)
{
	// calculate tag and set values
	short isEmptySpot = 0;
	unsigned long long index = 0;
	unsigned long long tagBits = 64 - cache->setBits - cache->blockBits;
	unsigned long long tag = address >> (64 - tagBits);
	unsigned long long set = (address << tagBits) >> (64 - cache->setBits);
	
	for(unsigned long long i=0; i < cache->linesPerSet; i++)
		if(cache->sets[set].lines[i].valid && cache->sets[set].lines[i].tag == tag)
		{
			cache->sets[set].lines[i].r_rate = ++cache->sets[set].recentRate;

			if(cache->displayTrace)
				printf(" hit");

			return 1;
		}
		else if(!cache->sets[set].lines[i].valid)
		{
			isEmptySpot = 1;
			index = i;
		}

	// address is not in the cache but no worries, we found an empty spot!
	if(isEmptySpot)
	{
		cache->sets[set].lines[index].tag = tag;
		cache->sets[set].lines[index].valid = 1;
		cache->sets[set].lines[index].r_rate = ++cache->sets[set].recentRate;
		cache->sets[set].lines[index].p_rate = ++cache->sets[set].placementRate;
		return 2;
	}
	// sorry no empty spot :(
	return 0;
}

// if policy is 0, then it is LRU
// if policy is 1, then it is FIFO
// evict based on evict policy
void evict(const unsigned long long address, Cache *cache, const short policy)
{
	// calculate tag and set values
	unsigned long long tagBits = 64 - cache->setBits - cache->blockBits;
	unsigned long long tag = address >> (64 - tagBits);
	unsigned long long set = (address << tagBits) >> (64 - cache->setBits);
	// initialize the index to be evicted
	unsigned long long index = 0;
	unsigned long long min;
	// decide which variable to use based on the policy
	if(!policy)
		min = cache->sets[set].lines[0].r_rate;
	else
		min = cache->sets[set].lines[0].p_rate;
	for(unsigned long long i=1; i < cache->linesPerSet; i++)
	{
		unsigned long long value;
		if(!policy)
			value = cache->sets[set].lines[i].r_rate;
		else
			value = cache->sets[set].lines[i].p_rate;
		if(min > value)
		{
			min = value;
			index = i;
		}
	}
	cache->sets[set].lines[index].tag = tag;
	// update the recency and placement rate
	cache->sets[set].lines[index].r_rate = ++cache->sets[set].recentRate;
	cache->sets[set].lines[index].p_rate = ++cache->sets[set].placementRate;
}

//checks if the address is in the cache, if not and if the cache is full
//evicts an address
void operateCache(const unsigned long long address, Cache *cache)
{
	// checkCache checks if the address is in the cache
	unsigned long long checkResult = checkCache(address, cache);

	// found it!
	if(checkResult == 1)
		cache->hit_count++;
	// couldn't find it :(
	else
	{
		cache->miss_count++;

		if(cache->displayTrace)
			printf(" miss");
		// and we couldn't find an empty spot so we will evict one
		if(checkResult == 0)
		{
			evict(address, cache, cache->eviction_policy);
			cache->eviction_count++;

			if(cache->displayTrace)
				printf(" eviction");
		}
	}
}

// get the input from the file and call operateCache function to see if the address
// is in the cache. 
void operateFlags(char *traceFile, Cache *cache)
{
	FILE *input = fopen(traceFile, "r");
	int size;
	char operation;
	unsigned long long address;
	while(fscanf(input, " %c %llx,%d", &operation, &address, &size) == 3)
	{
		if(cache->displayTrace)
			printf("%c %llx,%d", operation, address, size);
		
		switch(operation)
		{
			case 'M':
				operateCache(address, cache);
				cache->hit_count++;
				if(cache->displayTrace)
					printf(" hit");
				break;
			case 'L':
				operateCache(address, cache);
				break;
			case 'S':
				operateCache(address, cache);
				break;
		}
		if(cache->displayTrace)
			printf("\n");

	}
	fclose(input);
}

// initialize the cache
void cacheSetUp(Cache *cache)
{
	cache->hit_count = 0;
	cache->eviction_count = 0;
	cache->miss_count = 0;
	unsigned long long set = pow(2, cache->setBits);
	cache->sets = (Set *)malloc(sizeof(Set)*set);
	for(unsigned long long i=0; i < set; i++)
	{
		cache->sets[i].recentRate = 0;
		cache->sets[i].placementRate = 0;
		cache->sets[i].lines = (Line *)malloc(sizeof(Line)*cache->linesPerSet);
		for(unsigned long long j=0; j < cache->linesPerSet; j++)
		{
			cache->sets[i].lines[j].valid = 0;
		}
	}
}

// deallocate memory
void deallocate(Cache *cache)
{
	unsigned long long set = pow(2, cache->setBits);
	for(unsigned long long i=0; i < set; i++)
		free(cache->sets[i].lines);
	free(cache->sets);
}

int main(int argc, char *argv[])
{
	Cache cache;
	
	opterr = 0;
	cache.displayTrace = 0;
	int option = 0;
	char *traceFile;
	// accepting command-line options
	// "assistance from" https://www.gnu.org/software/libc/manual/html_node/Example-of-Getopt.html#Example-of-Getopt
	while((option = getopt(argc, argv,"s:E:b:t:LFv")) != -1)
	{
		switch(option)
		{
			// select the number of set bits (i.e., use S = 2s sets)
			case 's':
				cache.setBits = atoi(optarg);
				break;
			// select the number of lines per set (associativity)
			case 'E':
				cache.linesPerSet = atoi(optarg);
				break;
			// select the number of block bits (i.e., use B = 2b bytes / block)
			case 'b':
				cache.blockBits = atoi(optarg);
				break;
			case 't':
				traceFile = optarg;
				break;
			// select the LRU policy
			case 'L':
				cache.eviction_policy = 0;
				break;
			// select the FIFO policy
			case 'F':
				cache.eviction_policy = 1;
				break;
			case 'v':
				cache.displayTrace = 1;
				break;
		}
	}
	// initializes the cache
	cacheSetUp(&cache);
	// check the flag and call appropriate function
	operateFlags(traceFile, &cache);
	// prints the summary
	printSummary(cache.hit_count, cache.miss_count, cache.eviction_count);
	// deallocates the memory
	deallocate(&cache);
    return 0;
}
