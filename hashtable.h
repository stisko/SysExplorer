/*
 * hashtable.h
 *
 *  Created on: Nov 17, 2010
 *      Author: vsam
 */

#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#include <stddef.h>

/*
 * Hash function type
 */
typedef size_t (*HashFunc)(const void*);

/*
 * Equality predicate type
 */
typedef int (*EqPred)(const void*, const void*);


/*
 * A HashTableCell (a.k.a. "cell") is a "position" in a hash table
 */
typedef struct htcell {
	struct htcell *next;
	const void *key, *value;
} *HashTableCell;


/*
 * Hash table type
 */
typedef struct HashTable {
	size_t tsize, nelem;
	HashFunc hfunc;
	EqPred eqpred;
	HashTableCell *table;
} HashTable;


/*
 * Initializer
 *
 * Construct a hash table of size tsize, with hash function f and equality predicate eq
 *
 * The size should be 30% bigger than the maximum expected number of elements, and
 * preferably a prime number.
 */
extern void HashTable_init(HashTable *this, size_t tsize, HashFunc f, EqPred eq);


/*
 * Destructor
 */
extern void HashTable_destroy(HashTable *this);


/*
 * Return the number of entries in this hashtable
 */
extern size_t HashTable_size(HashTable *this);


/*
 * Return nonzero if this is empty
 */
extern int HashTable_empty(HashTable* this);

/*
 * Clear all entries
 */
extern void HashTable_clear(HashTable *this);

/*
 * Return the "first" cell of the hash table
 */
extern HashTableCell HashTable_begin(HashTable* this);

/*
 * The "end" element
 */
extern HashTableCell HashTable_end(HashTable *this);

/*
 * The "next" element from the given cell, can be used to iterate over all
 * elements of the table.
 *
 * HashTable *table = ...;
 * HashTableCell p;
 *
 * for(p = HashTable_begin(table); p!=HashTable_end(table); p=HashTable_next(table, p)) {
 *  ...
 * }
 *
 */
extern HashTableCell HashTable_next(HashTable *this, HashTableCell c);

/*
 * Macro used to iterate over all contents of a hashtable. Use as follows:
 *
 * HashTableCell p;
 * HashTable table;
 * ...
 * HashTable_foreach(p, &table) {
 *   ... // use p->key and p->value
 * }
 */
#define HashTable_foreach(var, table) \
	for( (var) = HashTable_begin(table); \
	     (var)!=HashTable_end(table); \
	     (var)=HashTable_next(table,var))


/*
 * Find the cell for a key in the table.
 *
 * If the key is not in the table, return HashTable_end(this)
 */
extern HashTableCell HashTable_find(HashTable *this, const void* key);


/* Insert a new key/value into a hash table, and return the new cell,
 * unless the key already
 * exists, in which case return "end"
 */
extern HashTableCell HashTable_insert(HashTable *this, const void* key, const void* value);

/*
 * Remove a key/value binding, return true if found, false otherwise.
 */
extern int HashTable_remove(HashTable *this, const void* key);


/* Find and return the cell for a key if it exists, else insert key/value and return the new
 * cell. This call can be implemented using "find" and "insert" but it is faster.
 */
extern HashTableCell HashTable_find_or_insert(HashTable *this, const void* key, const void* value);


/* Return the value if the key exists, else return dfl.
 */
extern const void* HashTable_get(HashTable* this, const void *key, void* dfl);

/* Change the value for the given key if the key exists returning the old value,
 * or insert the key/value returning NULL.
 */
extern const void* HashTable_set(HashTable* this, const void* key, void* value);

/* Hash function for strings
 */
extern size_t strhash(const char* s);


/* Return a hash function for the given bytes/length. Note, this is NOT a suitable
 * function for passing to a HashTable, i.e. it does not conform to type HashFunc,
 * but it can be used to implement hash functions which do conform to HashFunc.
 *
 * For example, a hash function for a struct can be implemented by
 * - first filling an array by hashing each attribute, and
 * - then hashing this array using this function.
 */
extern size_t bytehash(const void* buf, size_t len);



#endif /* HASHTABLE_H_ */
