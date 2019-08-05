/*
 * cache.c
 *
 *  Created on: Dec 17, 2013
 *  Author: kostis
 */

#include <stdio.h>
#include <stdlib.h>
#include "cache.h"
#include <pthread.h>
#include "hashtable.h"
#include <string.h>
#include "strutils.h"
#include <sys/stat.h>




//equals needed by hashtable
static int equals(const void *a, const void *b){
	return strcmp((char *)a, (char *)b) == 0;
}

//hash function needed by hashtable
static size_t func(const void *f){
	return strhash((char*)f);
}

/**
 **	Description: dhmiourgei tin cache me maxpage kai ena deikti ths Cache
 **
 **	Arguments: 	 Cache* enas deiktis typou cache
 **				 int max_pages to megisto plithos selidwn pou prepei na dhmioyrgithoun
 **
 */
void cache_create(Cache *cache, int max_pages){
	if(pthread_mutex_init(&cache->lock,NULL)==1){
		fprintf(stderr,"The mutex cannot be initialized");
		abort();
	}
	cache->max_pages=max_pages;
	HashTable_init(&cache->table,131,&func,&equals);
	cache->pages=0;
	List_init(&cache->list);
	fprintf(stdout, "Cache created successfully with %d elements\n", &cache->max_pages );
}


/*
 * Description: eisagei mia selida sti cache.
 *
 * Arguments:	Cache *cache enas deiktis typou Cache
 * 				Page *page enas deiktis typou Page
 *
 */
void cache_insert(Cache *cache, Page *page){
	CacheEntry *entry;
	HashTableCell cell;

	entry=malloc(sizeof(CacheEntry));
	if(entry==NULL){
		fprintf(stderr,"Not enough memory to create Cache Entry...");
		abort();
	}
	entry->count=0;
	entry->page=page;
	if(pthread_mutex_init(&entry->mutex,NULL)){
		fprintf(stderr,"An error occured while initializing mutex");
		abort();
	}

	pthread_mutex_lock(&cache->lock);

	if(HashTable_find(&cache->table,page->path) == HashTable_end(&cache->table)){	//den uparxei stin cache
		entry->lst_position=List_push_front(&cache->list,page->path);
		cell=HashTable_insert(&cache->table,page->path,entry);
		if(cache->pages >= cache->max_pages){
			//max stoixeia
			cache_deleteLRU_elm(&cache);
		}else{
			cache->pages++;
		}
		pthread_mutex_unlock(&cache->lock);
	}else{	//uparxei stin cache
		pthread_mutex_destroy(&entry->mutex);
		cache_entry_destroy(entry);	//diagrafoume to entry pou dhmiourgisame prin
		pthread_mutex_unlock(&cache->lock);
	}
}
/*
 * Description: Destroys an entry of cache
 *
 * Arguments: CacheEntry * ebtry enas deiktis typou CacheEntry
 */
void cache_entry_destroy(CacheEntry *entry){
	pthread_mutex_destroy(&entry->mutex);
	free(entry->page->path);
	free(entry->page->htmtext);
	free(entry->page);
	free(entry);
}

/*
 * Description: Diagrafei apo ti cache to stoixeio to opoio einai to pio spania xrisimopoiimeno
 *
 * Arguments:	Cache *cache enas deiktis typou cache
 */
void cache_deleteLRU_elm(Cache *cache){
	void *key;
	CacheEntry *entry;
	key=List_pop_back(&cache->list);	//pairnoume to key tou teleutaioy stoixeiou tis listas
	entry= (CacheEntry*)HashTable_find(&cache->table,key)->value;
	HashTable_remove(&cache->table,key);
	cache_reduce_cnt(&entry);
}

/*
 * Description:	Meiwnei ton counter pou leei poses einai oi selides
 *
 * Arguments:	CacheEntry *entry enas deiktis tupou entry
 */

void cache_reduce_cnt(CacheEntry *entry){
	pthread_mutex_lock(&entry->mutex);
	entry->count--;
	pthread_mutex_unlock(&entry->mutex);
	if(&entry->count<0){
		cache_entry_destroy(&entry);
	}
}

/*
 * psaxnei na dei an uparxei i selida sti cache
 * Arguments : 	int fd,
 * 				Cache *cache
 * 				char *absolute_path
 *
 * 	Returns:	-1	NOT FOUND
 * 				 0	FOUND UPTODATE
 * 				 1	FOUND OUTDATED
 */
int cache_find( int fd, Cache *cache, char *absolute_path){
	HashTableCell cell;
	CacheEntry *entry;
	struct stat ls_stat;

	pthread_mutex_lock(&cache->lock);
	cell=HashTable_find(&cache->table,absolute_path);
	if(cell==HashTable_end(&cache->table)){
		pthread_mutex_unlock(&cache->lock);
		fprintf(stdout,"Page not found in cache!\n");
		return -1;
	}
	if(lstat(absolute_path,&ls_stat)){
		fprintf(stderr,"An error occured in lstat()");
		pthread_mutex_unlock(&cache->lock);
		return -1;
	}
	entry=(CacheEntry*)cell->value;
	if(entry->page->version < ls_stat.st_ctime){
		pthread_mutex_unlock(&cache->lock);
		fprintf(stdout,"Found in cache! BUT OUTDATED!!\n");
		return 1;
	}
	List_spliceOne(List_begin(&cache->list),entry->lst_position);
	pthread_mutex_lock(&entry->mutex);
	entry->count++;	//auxanoume ton counter afou uparxei
	pthread_mutex_unlock(&entry->mutex);
	pthread_mutex_unlock(&cache->lock);
	write_message(fd, entry->page->htmtext);
	cache_reduce_cnt(entry);
	fprintf(stdout,"Found in cache! UP TO DATE\n");
	return 0;
}


/*
 * Description: Replace the pages of the cache to the right order
 *
 * Arguments: Cache *cache enas deiktis typou cache
 * 			  char *absolute_path to monopati tis diadromis
 * 			  time_t version  i wra tis teleutaias morfopoiisis sto arxeio
 * 			  char *htmltext  to html text pou exei i cache selida
 */



void cache_replace_pages(Cache *cache, char *absolute_path, time_t version,char *htmltext){
	HashTableCell cell;
	Page *page;
	CacheEntry *entry;

	pthread_mutex_lock(&cache->lock);
	cell=HashTable_find(&cache->table,absolute_path);
	if(cell== HashTable_end(&cache->table)){	//den uparxei sto hashtable
		pthread_mutex_unlock(&cache->lock);
		fprintf(stderr,"Could not found path in Hash");
		return;
	}

	entry= (CacheEntry*)cell->value;
	page= entry->page;
	pthread_mutex_lock(&entry->mutex);

	while(&entry->count >0){
		pthread_mutex_unlock(&entry->mutex);
		sched_yield();
		pthread_mutex_lock(&entry->mutex);
	}
	//ananewnoume ti selida
	free(page->htmtext);
	page->htmtext = htmltext;
	page->htmllen = strlen(htmltext);
	page->version = version;

	List_spliceOne(List_begin(&cache->list), entry->lst_position);
	pthread_mutex_unlock(&entry->mutex);
	pthread_mutex_unlock(&cache->lock);

}
