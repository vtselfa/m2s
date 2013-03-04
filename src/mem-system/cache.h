/*
 *  Multi2Sim
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

#ifndef MEM_SYSTEM_CACHE_H
#define MEM_SYSTEM_CACHE_H


extern struct str_map_t cache_policy_map;
extern struct str_map_t prefetch_policy_map;
extern struct str_map_t cache_block_state_map;

enum cache_policy_t
{
	cache_policy_invalid = 0,
	cache_policy_lru,
	cache_policy_fifo,
	cache_policy_random
};

enum prefetch_policy_t
{
	prefetch_disabled = 0,
	prefetch_obl,
	prefetch_obl_stride,
	prefetch_streams
};

enum cache_block_state_t
{
	cache_block_invalid = 0,
	cache_block_noncoherent,
	cache_block_modified,
	cache_block_owned,
	cache_block_exclusive,
	cache_block_shared
};

struct write_buffer_block_t
{
	int tag;
	enum cache_block_state_t state;
	struct mod_stack_t *stack;
};

struct cache_write_buffer
{
	struct linked_list_t *blocks;
};

struct cache_block_t
{
	struct cache_block_t *way_next;
	struct cache_block_t *way_prev;

	int tag;
	int transient_tag;
	int way;

	enum cache_block_state_t state;
	int prefetched : 1;
};

struct cache_set_t
{
	struct cache_block_t *way_head;
	struct cache_block_t *way_tail;
	struct cache_block_t *blocks;
};

/* Prefetching */
struct stream_block_t
{
	int slot;
	int tag;
	int transient_tag;
	enum cache_block_state_t state;
};

struct stride_detector_camp_t
{
	int tag;
	int last_addr;
	int stride;
};

struct stream_buffer_t
{
	int stream;
	int stream_tag; /* Tag of stream */
	int stream_transcient_tag; /* Tag of stream being brougth */
	struct stream_buffer_t *stream_next;
	struct stream_buffer_t *stream_prev;
	struct stream_block_t *blocks;

	int pending_prefetches; /* Remaining prefetches of a prefetch group */
	long long cycle; /* Cycle last prefetch was asigned to this stream. For debug. */
	int num_slots;
	int count;
	int head;
	int tail;
	int stride;
	int next_address;
};

struct cache_t
{
	char *name;

	unsigned int num_sets;
	unsigned int block_size;
	unsigned int assoc;
	enum cache_policy_t policy;

	struct cache_set_t *sets;
	unsigned int block_mask;
	int log_block_size;

	/* Prefetching */
	enum prefetch_policy_t prefetch_enabled;
	struct {
		unsigned int num_streams; 	/* Number of streams for prefetch */
		unsigned int aggressivity; 	/* Number of blocks per stream */
		unsigned int stream_mask; 	/* For obtaining stream_tag */

		struct stream_buffer_t *streams;
		struct stream_buffer_t *stream_head;
		struct stream_buffer_t *stream_tail;

		struct linked_list_t *stride_detector;
	} prefetch;

	struct cache_write_buffer wb;
};

struct cache_t *cache_create(char *name, unsigned int num_sets, unsigned int block_size,
	unsigned int assoc, unsigned int num_streams, unsigned int pref_aggr, enum cache_policy_t policy);
void cache_free(struct cache_t *cache);

void cache_decode_address(struct cache_t *cache, unsigned int addr,
	int *set_ptr, int *tag_ptr, unsigned int *offset_ptr);
int cache_find_block(struct cache_t *cache, unsigned int addr, int *set_ptr, int *pway,
	int *state_ptr);
void cache_set_block(struct cache_t *cache, int set, int way, int tag, int state, unsigned int prefetched);
void cache_get_block(struct cache_t *cache, int set, int way, int *tag_ptr, int *state_ptr);

void cache_access_block(struct cache_t *cache, int set, int way);
int cache_replace_block(struct cache_t *cache, int set);
void cache_set_transient_tag(struct cache_t *cache, int set, int way, int tag);

/* Prefetching */
int cache_find_stream(struct cache_t *cache, unsigned int stream_tag);
void cache_set_pref_block(struct cache_t *cache, int pref_stream, int pref_slot, int tag, int state);
struct stream_block_t * cache_get_pref_block(struct cache_t *cache, int pref_stream, int pref_slot);
void cache_get_pref_block_data(struct cache_t *cache, int pref_stream, int pref_slot, int *tag_ptr, int *state_ptr);
int cache_select_stream(struct cache_t *cache);
void cache_access_stream(struct cache_t *cache, int stream);
int cache_detect_stride(struct cache_t *cache, int addr);

#endif

