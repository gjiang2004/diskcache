#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  clock++;
  if (cache != NULL) {
    return -1;
  }
  if (num_entries < 2 || num_entries > 4096) {
    return -1;
  }
  cache = malloc(num_entries * sizeof(cache_entry_t));
  for (int i = 0; i < num_entries; i++) {
    cache[i].valid = false;
  }
  cache_size = num_entries;
  return 1;
}

int cache_destroy(void) {
  clock++;
  if (cache == NULL) {
    return -1;
  }
  free(cache);
  cache = NULL;
  cache_size = 0;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  clock++;
  if (!cache_enabled()) {
    return -1;
  }
  if (buf == NULL) {
    return -1;
  }
  if (disk_num < 0 || disk_num > JBOD_NUM_DISKS) {
    return -1;
  }
  if (block_num < 0 || block_num > JBOD_NUM_BLOCKS_PER_DISK) {
    return -1;
  }
  num_queries++;
  for(int i = 0; i < cache_size; i++) {
    if (cache[i].valid == false) {
      continue;
    } else {
      if (cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
        memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
        num_hits++;
        cache[i].clock_accesses = clock;
        return 1;
      }
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  clock++;
  for(int i = 0; i < cache_size; i++) {
    if (cache[i].valid == false) {
      continue;
    } else {
      if (cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
        memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
        cache[i].clock_accesses = clock;
        break;
      }
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  clock++;
  if (!cache_enabled()) {
    return -1;
  }
  if (buf == NULL) {
    return -1;
  }
  if (disk_num < 0 || disk_num > JBOD_NUM_DISKS) {
    return -1;
  }
  if (block_num < 0 || block_num > JBOD_NUM_BLOCKS_PER_DISK) {
    return -1;
  }
  for(int i = 0; i < cache_size; i++) {
    if (cache[i].valid == false) {
      continue;
    } else {
      if (cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
        return -1;
      }
    }
  }
  int time = cache[0].clock_accesses;
  int t = 0;
  for(int i = 0; i < cache_size; i++) {
    if (cache[i].valid == false) {
      cache[i].valid = true;
      cache[i].disk_num = disk_num;
      cache[i].block_num = block_num;
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      cache[i].clock_accesses = clock;
      return 1;
    } else {
      if (time < cache[i].clock_accesses) {
        time = cache[i].clock_accesses;
        t = i;
      }
    }
  }
  cache[t].valid = true;
  cache[t].disk_num = disk_num;
  cache[t].block_num = block_num;
  memcpy(cache[t].block, buf, JBOD_BLOCK_SIZE);
  cache[t].clock_accesses = clock;
  return 1;
}

bool cache_enabled(void) {
  clock++;
  return cache != NULL;
}

void cache_print_hit_rate(void) {
	clock++;
  fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

int compare(const void *a, const void *b) {
  return (((cache_entry_t*) a)->clock_accesses - ((cache_entry_t*) b)->clock_accesses);
}

int cache_resize(int new_num_entries) {
  clock++;
  // sort cache
  if (!cache_enabled()) {
    return -1;
  }
  if (new_num_entries < 2 || new_num_entries > 4096) {
    return -1;
  }
  qsort(cache, cache_size, sizeof(cache_entry_t), compare);
  cache_entry_t *cache2 = malloc(new_num_entries * sizeof(cache_entry_t));
  int count = cache_size <= new_num_entries ? cache_size : new_num_entries;
  for (int i = 0; i < count; i++) {
    cache2[i] = cache[i];
  }
  for (int i = count; i < new_num_entries; i++) {
    cache2[i].valid = false;
  }
  free(cache);
  cache = cache2;
  cache_size = new_num_entries;
  return 1;
}