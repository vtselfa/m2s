/*
 *  Libstruct
 *  Copyright (C) 2012  Rafael Ubal (ubal@ece.neu.edu)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>
#include <ctype.h>

#include <lib/mhandle/mhandle.h>

#include "debug.h"
#include "hash-table-gen.h"

#define	ALIGNED_POINTER(p, t) ((((unsigned long)(p)) & (unsigned long) (sizeof(t)-1)) == 0)
#define	HASH_TABLE_GEN_MIN_INITIAL_SIZE  128

unsigned int murmurhash2(const void *key, int len, int seed)
{
	/*
	 * Note: 'm' and 'r' are mixing constants generated offline.
	 * They're not really 'magic', they just happen to work well.
	 * Initialize the hash to a 'random' value.
	 */
	const unsigned int m = 0x5bd1e995;
	const unsigned int r = 24;

	const unsigned char *data = key;
	unsigned int h = (unsigned int) seed ^ (unsigned int) len;

	if (ALIGNED_POINTER(key, unsigned int))
	{
		while (len >= sizeof(unsigned int))
		{
			unsigned int k = *(const unsigned int *)data;

			k *= m;
			k ^= k >> r;
			k *= m;

			h *= m;
			h ^= k;

			data += sizeof(unsigned int);
			len -= sizeof(unsigned int);
		}
	}
	else
	{
		while (len >= sizeof(unsigned int))
		{
			unsigned int k;

			k  = data[0];
			k |= data[1] << 8;
			k |= data[2] << 16;
			k |= data[3] << 24;

			k *= m;
			k ^= k >> r;
			k *= m;

			h *= m;
			h ^= k;

			data += sizeof(unsigned int);
			len -= sizeof(unsigned int);
		}
	}

	/* Handle the last few bytes of the input array. */
	switch (len) {
	case 3:
		h ^= data[2] << 16;
		/* FALLTHROUGH */
	case 2:
		h ^= data[1] << 8;
		/* FALLTHROUGH */
	case 1:
		h ^= data[0];
		h *= m;
	}

	/*
	 * Do a few final mixes of the hash to ensure the last few
	 * bytes are well-incorporated.
	 */
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}


int hash(const void *key, int len)
{
	return murmurhash2(key, len, 42);
}


/*
 * Hash Table Element
 */

struct hash_table_gen_elem_t
{
	unsigned char *key; /* May contain '\0' bytes. It's not a string. */
	int key_len;
	void *data;
	struct hash_table_gen_elem_t *next;
};


struct hash_table_gen_elem_t *hash_table_gen_elem_create(void *key, int key_len, void *data)
{
	struct hash_table_gen_elem_t *elem;
	unsigned char *aux = (unsigned char *) key;

	/* Initialize */
	elem = xcalloc(1, sizeof(struct hash_table_gen_elem_t));
	elem->key = xcalloc(key_len, sizeof(unsigned char));
	for (int i=0; i<key_len; i++)
		elem->key[i] = aux[i];
	elem->data = data;

	/* Return */
	return elem;
}


void hash_table_gen_elem_free(struct hash_table_gen_elem_t *elem)
{
	free(elem->key);
	free(elem);
}




/*
 * Hash Table
 */

struct hash_table_gen_t
{
	unsigned int count;
	unsigned int size;

	int find_op;
	int find_index;

	struct hash_table_gen_elem_t *find_elem;

	struct hash_table_gen_elem_t **elem_vector;

	int (*compare_func)(const void *, const void *, int);
	int (*hash_func)(const void *, const int);
};


static unsigned int hash_table_gen_get_index(struct hash_table_gen_t *table, void *key, int key_len)
{
	unsigned int index = table->hash_func(key, key_len) % table->size;
	return index;
}


static void hash_table_gen_grow(struct hash_table_gen_t *table)
{
	int old_size;
	int index;
	int i;

	struct hash_table_gen_elem_t **old_elem_vector;
	struct hash_table_gen_elem_t *elem;

	/* Save old vector */
	old_size = table->size;
	old_elem_vector = table->elem_vector;

	/* Allocate new vector */
	table->size = old_size * 2;
	table->elem_vector = xcalloc(table->size, sizeof(void *));

	/* Move elements to new vector */
	for (i = 0; i < old_size; i++)
	{
		while ((elem = old_elem_vector[i]))
		{
			/* Remove from old vector */
			old_elem_vector[i] = elem->next;

			/* Insert in new vector */
			index = hash_table_gen_get_index(table, elem->key, elem->key_len);
			elem->next = table->elem_vector[index];
			table->elem_vector[index] = elem;
		}
	}

	/* Free old vector */
	free(old_elem_vector);
}


static struct hash_table_gen_elem_t *hash_table_gen_find(struct hash_table_gen_t *table, void *key, int key_len, int *index_ptr)
{
	struct hash_table_gen_elem_t *elem;
	unsigned char *aux = (unsigned char *) key;

	int index;

	/* Get index */
	index = hash_table_gen_get_index(table, key, key_len);
	if (index_ptr)
		*index_ptr = index;

	/* Look for element */
	for (elem = table->elem_vector[index]; elem; elem = elem->next)
	{
		if (!table->compare_func)
		{
			int i;
			for (i = 0; i < key_len; i++)
				if (aux[i] != elem->key[i])
					break;
			if (i == key_len)
				return elem;
			else
				continue;
		}
		else
		{
			if (table->compare_func(key, elem->key, elem->key_len))
				return elem;
		}
	}

	/* Not found */
	return NULL;
}


struct hash_table_gen_t *hash_table_gen_create(unsigned int size)
{
	struct hash_table_gen_t *table;
	assert(sizeof(int) == 4);
	assert(sizeof(unsigned char) == 1);

	/* Assign fields */
	table = xcalloc(1, sizeof(struct hash_table_gen_t));
	table->size = size < HASH_TABLE_GEN_MIN_INITIAL_SIZE ? HASH_TABLE_GEN_MIN_INITIAL_SIZE : size;

	table->hash_func = hash;

	/* Return */
	table->elem_vector = xcalloc(table->size, sizeof(void *));
	return table;
}


void hash_table_gen_free(struct hash_table_gen_t *table)
{
	if (!table) return;

	/* Clear table */
	hash_table_gen_clear(table);

	/* Free element vector and hash table */
	free(table->elem_vector);
	free(table);
}


void hash_table_gen_clear(struct hash_table_gen_t *table)
{
	struct hash_table_gen_elem_t *elem;
	struct hash_table_gen_elem_t *elem_next;

	int i;

	/* No find operation */
	table->find_op = 0;

	/* Free elements */
	for (i = 0; i < table->size; i++)
	{
		while ((elem = table->elem_vector[i]))
		{
			elem_next = elem->next;
			hash_table_gen_elem_free(elem);
			table->elem_vector[i] = elem_next;
		}
	}

	/* Reset count */
	table->count = 0;
}


int hash_table_gen_insert(struct hash_table_gen_t *table, char *key, int key_len, void *data)
{
	struct hash_table_gen_elem_t *elem;

	int index;

	/* No find operation */
	table->find_op = 0;

	/* Data cannot be null */
	if (!data)
		return 0;

	/* Rehashing */
	if (table->count >= table->size / 2)
		hash_table_gen_grow(table);

	/* Element must not exist */
	elem = hash_table_gen_find(table, key, key_len, &index);
	if (elem)
		return 0;

	/* Create element and insert at the head of collision list */
	elem = hash_table_gen_elem_create(key, key_len, data);
	elem->next = table->elem_vector[index];
	table->elem_vector[index] = elem;

	/* One more element */
	table->count++;
	assert(table->count < table->size);

	/* Success */
	return 1;
}


int hash_table_gen_set(struct hash_table_gen_t *table, char *key, int key_len, void *data)
{
	struct hash_table_gen_elem_t *elem;

	/* Data cannot be null */
	if (!data)
		return 0;

	/* Find element */
	elem = hash_table_gen_find(table, key, key_len, NULL);
	if (!elem)
		return 0;

	/* Set new data, success */
	elem->data = data;
	return 1;
}


unsigned int hash_table_gen_count(struct hash_table_gen_t *table)
{
	return table->count;
}


void *hash_table_gen_get(struct hash_table_gen_t *table, char *key, int key_len)
{
	struct hash_table_gen_elem_t *elem;

	/* Find element */
	elem = hash_table_gen_find(table, key, key_len, NULL);
	if (!elem)
		return NULL;

	/* Return data */
	return elem->data;
}


void *hash_table_gen_remove(struct hash_table_gen_t *table, char *key, int key_len)
{
	struct hash_table_gen_elem_t *elem;
	struct hash_table_gen_elem_t *elem_prev;

	int index;

	void *data;

	unsigned char *aux = (unsigned char *) key;

	/* No find operation */
	table->find_op = 0;

	/* Find element */
	index = hash_table_gen_get_index(table, key, key_len);
	elem_prev = NULL;
	for (elem = table->elem_vector[index]; elem; elem = elem->next)
	{
		if (!table->compare_func)
		{
			int i;
			for (i = 0; i < key_len; i++)
				if (aux[i] != elem->key[i])
					break;
			if (i == key_len)
				break; /* Found */
		}
		else
		{
			if (table->compare_func(key, elem->key, elem->key_len))
				break; /* Found */
		}

		/* Record previous element */
		elem_prev = elem;
	}

	/* Element not found */
	if (!elem)
		return NULL;

	/* Delete element from collision list */
	if (elem_prev)
		elem_prev->next = elem->next;
	else
		table->elem_vector[index] = elem->next;

	/* Free element */
	data = elem->data;
	hash_table_gen_elem_free(elem);

	/* One less element */
	assert(table->count > 0);
	table->count--;

	/* Return associated data */
	return data;
}


void hash_table_gen_find_first(struct hash_table_gen_t *table, void **key_ptr, int *key_len_ptr, void **data_ptr)
{
	struct hash_table_gen_elem_t *elem;
	int index;

	assert(key_ptr && key_len_ptr);

	/* Record find operation */
	table->find_op = 1;
	table->find_index = 0;
	table->find_elem = NULL;
	if (data_ptr)
		*data_ptr = NULL;

	/* Table is empty */
	if (!table->count)
	{
		*key_ptr = NULL;
		*key_len_ptr = 0;
		return;
	}

	/* Find first element */
	for (index = 0; index < table->size; index++)
	{
		elem = table->elem_vector[index];
		if (elem)
		{
			table->find_index = index;
			table->find_elem = elem;
			if (data_ptr)
				*data_ptr = elem->data;
			*key_ptr = elem->key;
			*key_len_ptr = elem->key_len;
			return;
		}
	}

	/* Never get here */
	panic("%s: inconsistent hash table", __FUNCTION__);
}


void hash_table_gen_find_next(struct hash_table_gen_t *table, void **key_ptr, int *key_len_ptr, void **data_ptr)
{
	struct hash_table_gen_elem_t *elem;
	int index;

	/* Not allowed if last operation is not 'hash_table_gen_find_xxx' operation. */
	if (!table->find_op)
		panic("%s: hash table enumeration interrupted", __FUNCTION__);

	/* End of enumeration reached in previous calls */
	if (!table->find_elem){
		*key_ptr = NULL;
		*key_len_ptr = 0;
		return;
	}

	/* Continue enumeration in collision list */
	elem = table->find_elem->next;
	if (elem)
	{
		table->find_elem = elem;
		if (data_ptr)
			*data_ptr = elem->data;
		*key_ptr = elem->key;
		*key_len_ptr = elem->key_len;
		return;
	}

	/* Continue enumeration in vector */
	table->find_index++;
	for (index = table->find_index; index < table->size; index++)
	{
		elem = table->elem_vector[index];
		if (elem)
		{
			table->find_index = index;
			table->find_elem = elem;
			if (data_ptr)
				*data_ptr = elem->data;
			*key_ptr = elem->key;
			*key_len_ptr = elem->key_len;
			return;
		}
	}

	/* No element found */
	table->find_index = 0;
	table->find_elem = NULL;
	if (data_ptr)
		*data_ptr = NULL;
	*key_ptr = NULL;
	*key_len_ptr = 0;
	return;
}
