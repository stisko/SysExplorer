/*
 * hashtable.c
 *
 *  Created on: Nov 17, 2010
 *      Author: vsam
 */

#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

/*
 * Private methods
 */

static HashTableCell* ht_alloc(size_t n)
{
	HashTableCell* ret;
	size_t i;

	ret = (HashTableCell*) malloc(n * sizeof(HashTableCell));
	for(i=0;i<n;i++) ret[i] = NULL;
	return ret;
}

static HashTableCell ht_nextfrom(HashTable* this, size_t pos)
{
	for( ; pos < this->tsize; pos++)
		if(this->table[pos] != NULL) return this->table[pos];
	return HashTable_end(this);
}

static HashTableCell ht_find(HashTable *this, HashTableCell c, const void* key)
{
	while(c != NULL) {
		if(this->eqpred(c->key,key)!=0) break;
		c = c->next;
	}
	return  c;
}

static void ht_remove(HashTable* this, HashTableCell* ptr)
{
	HashTableCell c;
	c = *ptr;
	*ptr = c->next;
	free(c);
	this->nelem --;
}

static size_t ht_bucket(HashTable* this, const void* key)
{
	return this->hfunc(key) % this->tsize;
}

static HashTableCell ht_insert(HashTable *this, size_t h, const void* key, const void* value)
{
	HashTableCell c;
	c = malloc(sizeof(struct htcell));
	c->next = this->table[h];
	this->table[h] = c;
	this->nelem ++;
	c->key = key;
	c->value = value;
	return c;
}



/*
 * Initializer
 */
void HashTable_init(HashTable *this, size_t tsize, HashFunc f, EqPred eq)
{
	this->nelem = 0;
	this->eqpred = eq;
	this->hfunc = f;

	this->tsize = tsize;
	this->table = ht_alloc(this->tsize);
}

/*
 * Destructor
 */
void HashTable_destroy(HashTable *this)
{
	HashTable_clear(this);
	free(this->table);
}

/*
 * Number of elements
 */
size_t HashTable_size(HashTable *this)
{
	return this->nelem;
}

/*
 * Empty predicate
 */
int HashTable_empty(HashTable* this)
{
	return this->nelem == 0;
}


/*
 * Return the first cell of the hash table
 */
HashTableCell HashTable_begin(HashTable* this)
{
	return ht_nextfrom(this, 0);
}

HashTableCell HashTable_next(HashTable *this, HashTableCell c)
{
	size_t h;
	if(c == HashTable_end(this)) return c;
	if(c->next != NULL) return c->next;
	h = ht_bucket(this,c->key);
	return ht_nextfrom(this, h);
}

/*
 * The "end" element
 */
HashTableCell HashTable_end(HashTable *this)
{
	return NULL;
}

/*
 * if( key in table)
 *   return its cell
 * else
 *   return end
 */
HashTableCell HashTable_find(HashTable *this, const void* key)
{
	size_t h;

	h = ht_bucket(this, key);
	return ht_find(this, this->table[h], key);
}

/*
 * if( find(key) == end )
 * 		return new_cell
 * else
 *      return end
 */
HashTableCell HashTable_insert(HashTable *this, const void* key, const void* value)
{
	size_t h;
	HashTableCell c;

	h = ht_bucket(this, key);
	c = ht_find(this, this->table[h], key);

	/* key exists, no insert */
	if(c!=NULL) return HashTable_end(this);

	/* create new node */
	return ht_insert(this, h, key, value);
}

/*
 * Remove a key binding
 */
int HashTable_remove(HashTable *this, const void* key)
{
	HashTableCell * ptr;
	size_t h;

	h = ht_bucket(this, key);
	for(ptr = &this->table[h]; *ptr != NULL; ptr=& ((*ptr)->next))
		if( this->eqpred((*ptr)->key, key) )
		{
			ht_remove(this, ptr);	 /* Found, remove it and return true */
			return 1;
		}
	/* Not found, return false */
	return 0;
}

/*
 * This is the same as
 * if( find(key) )
 *   return find(key);
 * else
 *   return insert(key, value);
 */
HashTableCell HashTable_find_or_insert(HashTable *this, const void* key, const void* value)
{
	size_t h;
	HashTableCell c;

	h = ht_bucket(this,key);
	c = ht_find(this, this->table[h], key);
	if(c!=NULL) return c;
	return ht_insert(this, h, key, value);
}


const void* HashTable_get(HashTable* this, const void *key, void* dfl)
{
	HashTableCell c;
	c = ht_find(this, this->table[ht_bucket(this,key)], key);
	if(c==NULL) return dfl;
	return c->value;
}

const void* HashTable_set(HashTable* this, const void* key, void* value)
{
	const void *ret;
	size_t h;
	HashTableCell c;

	h = ht_bucket(this,key);
	c = ht_find(this, this->table[h], key);
	if(c==NULL) {
		ht_insert(this, h, key, value);
		return NULL;
	}
	ret = c->value;
	c->value = value;
	return ret;
}




void HashTable_clear(HashTable *this)
{
	size_t i;
	for(i=0; i < this->tsize; i++) {
		while(this->table[i]!=NULL)
			ht_remove(this, & this->table[i]);
	}
}


size_t strhash(const char* s)
{
	size_t h  = 5381;
	unsigned char c;

	while((c = *s++)) {
		h = ((h << 5) + h)+c;
	}

	return h;
}


size_t bytehash(const void* buffer, size_t len)
{
	size_t h = 0;
	while(len--) {
		h = (int) *((char*)buffer)
				+ (h<<6) + (h<<16) -h;
		buffer = ((char*)buffer)+1;
	}
	return h;
}

