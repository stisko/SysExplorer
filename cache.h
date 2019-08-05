/*
 * cache.h
 *
 *  Created on: Dec 17, 2013
 *      Author: kostis
 */
#include <time.h>
#include <pthread.h>
#include "hashtable.h"
#include "linkedlist.h"

#ifndef CACHE_H_
#define CACHE_H_

typedef struct{
	char *path;
	char *htmtext;
	unsigned htmllen;
	time_t version;
}Page;

typedef struct{
	HashTable table;
	List list;
	int max_pages;
	int pages;
	pthread_mutex_t lock;
}Cache;

typedef struct{
	Page *page;
	pthread_mutex_t mutex;
	int count;
	ListNode lst_position;
}CacheEntry;

extern int cache_find( int fd, Cache *cache, char *absolute_path);

extern void cache_reduce_cnt(CacheEntry *entry);

extern void cache_replace_pages(Cache *cache, char *absolute_path, time_t version,char *htmltext);

extern void cache_deleteLRU_elm(Cache *cache);

extern void cache_entry_destroy(CacheEntry *entry);

extern void cache_insert(Cache *cache, Page *page);

extern void cache_create(Cache *cache, int max_pages);

#endif /* CACHE_H_ */
