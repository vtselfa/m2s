/*
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
#include <stdlib.h>

#include <arch/x86/timing/cpu.h>
#include <arch/x86/timing/uop.h>
#include <lib/esim/esim.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/linked-list.h>
#include <lib/util/list.h>
#include <lib/util/misc.h>
#include <lib/util/string.h>

#include "cache.h"
#include "directory.h"
#include "local-mem-protocol.h"
#include "mem-system.h"
#include "mod-stack.h"
#include "nmoesi-protocol.h"


/* String map for access type */
struct str_map_t mod_access_kind_map =
{
	4, {
		{ "Load", mod_access_load },
		{ "Store", mod_access_store },
		{ "NCStore", mod_access_nc_store },
		{ "Prefetch", mod_access_prefetch }
	}
};


int EV_CACHE_ADAPT_PREF;


/*
 * Public Functions
 */

///////////////////   MAIN MEMORY   ////////////////////////

struct reg_bank_t* regs_bank_create( int num_banks, int t_row_hit, int t_row_miss){

        struct reg_bank_t * banks;
        banks = calloc( num_banks, sizeof(struct reg_bank_t));
        if (!banks)
                fatal("%s: out of memory", __FUNCTION__);

        for(int i=0; i<num_banks;i++){
                banks[i].row_buffer=-1;
                banks[i].row_is_been_accesed=-1;
                banks[i].t_row_buffer_miss=t_row_miss;
                banks[i].t_row_buffer_hit=t_row_hit;
        }

        return banks;


}



struct reg_rank_t* regs_rank_create( int num_ranks, int num_banks, int t_row_hit, int t_row_miss){

        struct reg_rank_t * ranks;
        ranks = calloc(num_ranks, sizeof(struct reg_rank_t));
        if (!ranks)
                fatal("%s: out of memory", __FUNCTION__);


        for(int i=0; i<num_ranks;i++){
                ranks[i].num_regs_bank=num_banks;
                ranks[i].regs_bank=regs_bank_create(num_banks, t_row_hit, t_row_miss);

        }

        return ranks;

}


struct reg_channel_t* regs_channel_create( int num_channels, int num_ranks, int num_banks, int bandwith, struct reg_rank_t* regs_rank ){

        struct reg_channel_t * channels;
        channels = calloc(num_channels, sizeof(struct reg_channel_t));
        if (!channels)
                fatal("%s: out of memory", __FUNCTION__);


        for(int i=0; i<num_channels;i++){
                channels[i].state=channel_state_free;
                channels[i].num_regs_rank=num_ranks;
                channels[i].bandwith=bandwith;
                channels[i].regs_rank= regs_rank;
        }

        return channels;

}

void reg_channel_free(struct reg_channel_t * channels, int num_channels){

        for(int c=0; c<num_channels;c++)
                reg_rank_free(channels[c].regs_rank, channels[c].num_regs_rank);

        free(channels);

}

void reg_rank_free(struct reg_rank_t * rank, int num_ranks){

        for(int r=0; r<num_ranks;r++ )
                free(rank[r].regs_bank);

        free(rank);

}


/////////////////////////////////////////////////////////////////////////

struct mod_t *mod_create(char *name, enum mod_kind_t kind, int num_ports,
	int block_size, int latency)
{
	struct mod_t *mod;

	/* Allocate */
	mod = calloc(1, sizeof(struct mod_t));
	if (!mod)
		fatal("%s: out of memory", __FUNCTION__);

	/* Name */
	mod->name = strdup(name);
	if (!mod->name)
		fatal("%s: out of memory", __FUNCTION__);

	/* Initialize */
	mod->kind = kind;
	mod->latency = latency;

	/* Ports */
	mod->num_ports = num_ports;
	mod->ports = calloc(num_ports, sizeof(struct mod_port_t));
	if (!mod->ports)
		fatal("%s: out of memory", __FUNCTION__);

	/* Lists */
	mod->low_mod_list = linked_list_create();
	mod->high_mod_list = linked_list_create();
	mod->pq = linked_list_create();
	mod->threads = list_create();

	/* Block size */
	mod->block_size = block_size;
	assert(!(block_size & (block_size - 1)) && block_size >= 4);
	mod->log_block_size = log_base2(block_size);

	printf("Mod %s\n", mod->name);

	return mod;
}


void mod_free(struct mod_t *mod)
{
	linked_list_free(mod->low_mod_list);
	linked_list_free(mod->high_mod_list);

	/* Free L2 prefetch queue */
	linked_list_head(mod->pq);
	while (linked_list_count(mod->pq))
	{
		/* Free all the uops and pref_groups associated (if any)
		 * remaining in the queue */
		struct x86_uop_t * uop = linked_list_get(mod->pq);
		linked_list_remove(mod->pq);
		x86_uop_free_if_not_queued(uop);
	}
	linked_list_free(mod->pq);

	/* Free the queue containing the threads that can access this module */
	list_head(mod->threads);
	while (list_count(mod->threads))
	{
		struct core_thread_tuple_t *tuple = list_pop(mod->threads);
		free(tuple);
	}
	list_free(mod->threads);

	if (mod->cache)
		cache_free(mod->cache);
	if (mod->dir)
		dir_free(mod->dir);

	///////////////////////////////////////////
        if(mod->regs_rank) // this module is main memory
                reg_rank_free(mod->regs_rank, mod->num_regs_rank);
        //////////////////////////////////////////

	free(mod->adapt_pref_stack);
	free(mod->ports);
	free(mod->name);
	free(mod);
}


void mod_dump(struct mod_t *mod, FILE *f)
{
}


/* Access a memory module.
 * Variable 'witness', if specified, will be increased when the access completes.
 * The function returns a unique access ID.
 */
long long mod_access(struct mod_t *mod, enum mod_access_kind_t access_kind,
	unsigned int addr, int *witness_ptr, struct linked_list_t *event_queue,
	void *event_queue_item, int core, int thread, int prefetch)
{
	struct mod_stack_t *stack;
	struct x86_uop_t *uop = (struct x86_uop_t *) event_queue_item;
	int event;

	/* Create module stack with new ID */
	mod_stack_id++;
	stack = mod_stack_create(mod_stack_id,
		mod, addr, ESIM_EV_NONE, NULL, core, thread, prefetch);

	/* Pass prefetch parameters */
	if(uop && uop->pref.kind)
		stack->pref = uop->pref;

	/* Initialize */
	stack->witness_ptr = witness_ptr;
	stack->event_queue = event_queue;
	stack->event_queue_item = event_queue_item;

	/* Select initial CPU/GPU event */
	if (mod->kind == mod_kind_cache || mod->kind == mod_kind_main_memory)
	{
		if (access_kind == mod_access_load)
		{
			event = EV_MOD_NMOESI_LOAD;
		}
		else if (access_kind == mod_access_store)
		{
			event = EV_MOD_NMOESI_STORE;
		}
		else if (access_kind == mod_access_nc_store)
		{
			event = EV_MOD_NMOESI_NC_STORE;
		}
		else if (access_kind == mod_access_prefetch)
		{
			if(mod->cache->prefetch_policy == prefetch_policy_streams)
				event = EV_MOD_PREF;
			else if(mod->cache->prefetch_policy == prefetch_policy_obl)
				event = EV_MOD_NMOESI_PREF_OBL;
			else if(mod->cache->prefetch_policy == prefetch_policy_obl_stride)
				event = EV_MOD_NMOESI_PREF_OBL;
			else
				panic("%s: invalid prefetch policy", __FUNCTION__);
		}
		else if (access_kind == mod_access_invalidate)
		{
			event = EV_MOD_NMOESI_INVALIDATE_SLOT;
		}
		else
		{
			panic("%s: invalid access kind", __FUNCTION__);
		}
	}
	else if (mod->kind == mod_kind_local_memory)
	{
		if (access_kind == mod_access_load)
		{
			event = EV_MOD_LOCAL_MEM_LOAD;
		}
		else if (access_kind == mod_access_store)
		{
			event = EV_MOD_LOCAL_MEM_STORE;
		}
		else if (access_kind == mod_access_prefetch)
		{
			event = EV_MOD_PREF;
		}
		else
		{
			panic("%s: invalid access kind", __FUNCTION__);
		}
	}


	/* Schedule */
	  esim_execute_event(event, stack);

	/* Return access ID */
	return stack->id;
}


/* Return true if module can be accessed. */
int mod_can_access(struct mod_t *mod, unsigned int addr)
{
	int non_coalesced_accesses;

	/* There must be a free port */
	assert(mod->num_locked_ports <= mod->num_ports);
	if (mod->num_locked_ports == mod->num_ports)
		return 0;

	/* If no MSHR is given, module can be accessed */
	if (!mod->mshr_size)
		return 1;

	/* Module can be accessed if number of non-coalesced in-flight
	 * accesses is smaller than the MSHR size. */
	non_coalesced_accesses = mod->access_list_count -
		mod->access_list_coalesced_count;
	return non_coalesced_accesses < mod->mshr_size;
}


void mod_get_tag_set(struct mod_t *mod, unsigned int addr, int *tag_ptr, int *set_ptr)
{
	struct cache_t *cache = mod->cache;
	int tag, set;

	tag = addr & ~cache->block_mask;
	if (mod->range_kind == mod_range_interleaved)
	{
		unsigned int num_mods = mod->range.interleaved.mod;
		set = ((tag >> cache->log_block_size) / num_mods) % cache->num_sets;
	}
	else if (mod->range_kind == mod_range_bounds)
	{
		set = (tag >> cache->log_block_size) % cache->num_sets;
	}
	else
	{
		panic("%s: invalid range kind (%d)", __FUNCTION__, mod->range_kind);
	}

	PTR_ASSIGN(tag_ptr, tag);
	PTR_ASSIGN(set_ptr, set);
}


/* Search for a block in a stream and return the slot where the block is found or -1 if the block is not in the stream */
int mod_find_block_in_stream(struct mod_t *mod, unsigned int addr, int stream)
{
	struct cache_t *cache = mod->cache;
	struct stream_buffer_t *sb = &cache->prefetch.streams[stream];
	struct stream_block_t *block;
	int tag = addr & ~cache->block_mask;
	int i, slot, count;

	count = sb->head + sb->num_slots;
	for(i = sb->head; i < count; i++){
		slot = i % sb->num_slots;
		block = cache_get_pref_block(cache, sb->stream, slot);
		if(block->tag == tag && block->state)
			return slot;
	}
	return -1;
}


/* Return {set, way, tag, state} for an address.
 * The function returns TRUE on hit, FALSE on miss. */
int mod_find_block(struct mod_t *mod, unsigned int addr, int *set_ptr,
	int *way_ptr, int *tag_ptr, int *state_ptr, int *prefetched_ptr)
{
	struct cache_t *cache = mod->cache;
	struct cache_block_t *blk;
	struct dir_lock_t *dir_lock;

	int set;
	int way;
	int tag;

	/* A transient tag is considered a hit if the block is
	 * locked in the corresponding directory. */
	tag = addr & ~cache->block_mask;
	if (mod->range_kind == mod_range_interleaved)
	{
		unsigned int num_mods = mod->range.interleaved.mod;
		set = ((tag >> cache->log_block_size) / num_mods) % cache->num_sets;
	}
	else if (mod->range_kind == mod_range_bounds)
	{
		set = (tag >> cache->log_block_size) % cache->num_sets;
	}
	else
	{
		panic("%s: invalid range kind (%d)", __FUNCTION__, mod->range_kind);
	}

	for (way = 0; way < cache->assoc; way++)
	{
		blk = &cache->sets[set].blocks[way];
		if (blk->tag == tag && blk->state)
			break;
		if (blk->transient_tag == tag)
		{
			dir_lock = dir_lock_get(mod->dir, set, way);
			if (dir_lock->lock)
				break;
			//else
				//assert(!dir_lock->lock_queue); //VVV
		}
	}

	PTR_ASSIGN(set_ptr, set);
	PTR_ASSIGN(tag_ptr, tag);

	/* Miss */
	if (way == cache->assoc)
		return 0;

	/* Hit */
	PTR_ASSIGN(way_ptr, way);
	PTR_ASSIGN(state_ptr, cache->sets[set].blocks[way].state);
	PTR_ASSIGN(prefetched_ptr, cache->sets[set].blocks[way].prefetched);
	return 1;
}


/* Look for a block in prefetch buffer.
 * The function returns 0 on miss, 1 if hit on head and 2 if hit in the middle of the stream. */
int mod_find_pref_block(struct mod_t *mod, unsigned int addr, int *pref_stream_ptr, int *pref_slot_ptr)
{
	struct cache_t *cache = mod->cache;
	struct stream_block_t *blk;
	struct dir_lock_t *dir_lock;
	struct stream_buffer_t *sb;

	/* A transient tag is considered a hit if the block is
	 * locked in the corresponding directory */
	int tag = addr & ~cache->block_mask;

	unsigned int stream_tag = addr & ~cache->prefetch.stream_mask;
	int stream, slot;
	int num_streams = cache->prefetch.num_streams;
	for(stream=0; stream<num_streams; stream++)
	{
		int i, count;
		sb = &cache->prefetch.streams[stream];

		/* Block can't be in this stream */
		/*if(!sb->stream_tag == stream_tag)
			continue;*/

		count = sb->head + sb->num_slots;
		for(i = sb->head; i < count; i++)
		{
			slot = i % sb->num_slots;
			blk = cache_get_pref_block(cache, stream, slot);

			/* Increment any invalid unlocked head */
			if(slot == sb->head && !blk->state)
			{
				dir_lock = dir_pref_lock_get(mod->dir, stream, slot);
				if(!dir_lock->lock)
				{
					sb->head = (sb->head + 1) % sb->num_slots;
					continue;
				}
			}

			/* Tag hit */
			if (blk->tag == tag && blk->state)
				goto hit;

			/* Locked block and transient tag hit */
			if (blk->transient_tag == tag)
			{
				dir_lock = dir_pref_lock_get(mod->dir, stream, slot);
				if (dir_lock->lock)
					goto hit;
			}
		}
	}

	/* Miss */
	if (stream == num_streams)
	{
		PTR_ASSIGN(pref_stream_ptr, -1);
		PTR_ASSIGN(pref_slot_ptr, -1);
		return 0;
	}

hit:
	assert(sb->stream_tag == stream_tag || sb->stream_transcient_tag == stream_tag); /* Assegurem-nos de que el bloc estava on tocava */
	PTR_ASSIGN(pref_stream_ptr, stream);
	PTR_ASSIGN(pref_slot_ptr, slot);
	if(sb->head == slot)
		return 1; //Hit in head
	else
		return 2; //Hit in the middle of the stream
}


/* Lock a port, and schedule event when done.
 * If there is no free port, the access is enqueued in the port
 * waiting list, and it will retry once a port becomes available with a
 * call to 'mod_unlock_port'. */
void mod_lock_port(struct mod_t *mod, struct mod_stack_t *stack, int event)
{
	struct mod_port_t *port = NULL;
	int i;

	/* No free port */
	if (mod->num_locked_ports >= mod->num_ports)
	{
		assert(!DOUBLE_LINKED_LIST_MEMBER(mod, port_waiting, stack));

		/* If the request to lock the port is down-up, give it priority since
		 * it is possibly holding up a large portion of the memory hierarchy */
		if (stack->request_dir == mod_request_down_up)
		{
			DOUBLE_LINKED_LIST_INSERT_HEAD(mod, port_waiting, stack);
		}
		else
		{
			DOUBLE_LINKED_LIST_INSERT_TAIL(mod, port_waiting, stack);
		}
		stack->port_waiting_list_event = event;
		return;
	}

	/* Get free port */
	for (i = 0; i < mod->num_ports; i++)
	{
		port = &mod->ports[i];
		if (!port->stack)
			break;
	}

	/* Lock port */
	assert(port && i < mod->num_ports);
	port->stack = stack;
	stack->port = port;
	mod->num_locked_ports++;

	/* Debug */
	mem_debug("  %lld stack %lld %s port %d locked\n", esim_cycle, stack->id, mod->name, i);

	/* Schedule event */
	esim_schedule_event(event, stack, 0);
}


void mod_unlock_port(struct mod_t *mod, struct mod_port_t *port,
	struct mod_stack_t *stack)
{
	int event;

	/* Checks */
	assert(mod->num_locked_ports > 0);
	assert(stack->port == port && port->stack == stack);
	assert(stack->mod == mod);

	/* Unlock port */
	stack->port = NULL;
	port->stack = NULL;
	mod->num_locked_ports--;

	/* Debug */
	mem_debug("  %lld %lld %s port unlocked\n", esim_cycle,
		stack->id, mod->name);

	/* Check if there was any access waiting for free port */
	if (!mod->port_waiting_list_count)
		return;

	/* Wake up one access waiting for a free port */
	stack = mod->port_waiting_list_head;
	event = stack->port_waiting_list_event;
	assert(DOUBLE_LINKED_LIST_MEMBER(mod, port_waiting, stack));
	DOUBLE_LINKED_LIST_REMOVE(mod, port_waiting, stack);
	mod_lock_port(mod, stack, event);

}


void mod_access_start(struct mod_t *mod, struct mod_stack_t *stack,
	enum mod_access_kind_t access_kind)
{
	int index;

	/* Record access kind */
	stack->access_kind = access_kind;

	/* Insert in access list */
	DOUBLE_LINKED_LIST_INSERT_TAIL(mod, access, stack);

	/* Insert in write access list */
	if (access_kind == mod_access_store)
		DOUBLE_LINKED_LIST_INSERT_TAIL(mod, write_access, stack);

	/* Insert in access hash table */
	index = (stack->addr >> mod->log_block_size) % MOD_ACCESS_HASH_TABLE_SIZE;
	DOUBLE_LINKED_LIST_INSERT_TAIL(&mod->access_hash_table[index], bucket, stack);
}


void mod_access_finish(struct mod_t *mod, struct mod_stack_t *stack)
{
	int index;

	/* Remove from access list */
	DOUBLE_LINKED_LIST_REMOVE(mod, access, stack);

	/* Remove from write access list */
	assert(stack->access_kind);
	if (stack->access_kind == mod_access_store)
		DOUBLE_LINKED_LIST_REMOVE(mod, write_access, stack);

	/* Remove from hash table */
	index = (stack->addr >> mod->log_block_size) % MOD_ACCESS_HASH_TABLE_SIZE;
	DOUBLE_LINKED_LIST_REMOVE(&mod->access_hash_table[index], bucket, stack);

	/* If this was a coalesced access, update counter */
	if (stack->coalesced)
	{
		assert(mod->access_list_coalesced_count > 0);
		mod->access_list_coalesced_count--;
	}
}


/* Return true if the access with identifier 'id' is in flight.
 * The address of the access is passed as well because this lookup is done on the
 * access truth table, indexed by the access address.
 */
int mod_in_flight_access(struct mod_t *mod, long long id, unsigned int addr)
{
	struct mod_stack_t *stack;
	int index;

	/* Look for access */
	index = (addr >> mod->log_block_size) % MOD_ACCESS_HASH_TABLE_SIZE;
	for (stack = mod->access_hash_table[index].bucket_list_head; stack; stack = stack->bucket_list_next)
		if (stack->id == id)
			return 1;

	/* Not found */
	return 0;
}


/* Return the youngest in-flight access older than 'older_than_stack' to block containing 'addr'.
 * If 'older_than_stack' is NULL, return the youngest in-flight access containing 'addr'.
 * The function returns NULL if there is no in-flight access to block containing 'addr'.
 */
struct mod_stack_t *mod_in_flight_address(struct mod_t *mod, unsigned int addr,
	struct mod_stack_t *older_than_stack)
{
	struct mod_stack_t *stack;
	int index;

	/* Look for address */
	index = (addr >> mod->log_block_size) % MOD_ACCESS_HASH_TABLE_SIZE;
	for (stack = mod->access_hash_table[index].bucket_list_head; stack;
		stack = stack->bucket_list_next)
	{
		/* This stack is not older than 'older_than_stack' */
		if (older_than_stack && stack->id >= older_than_stack->id)
			continue;

		/* Address matches */
		if (stack->addr >> mod->log_block_size == addr >> mod->log_block_size)
			return stack;
	}

	/* Not found */
	return NULL;
}


/* Return the youngest in-flight write older than 'older_than_stack'. If 'older_than_stack'
 * is NULL, return the youngest in-flight write. Return NULL if there is no in-flight write.
 */
struct mod_stack_t *mod_in_flight_write(struct mod_t *mod,
	struct mod_stack_t *older_than_stack)
{
	struct mod_stack_t *stack;

	/* No 'older_than_stack' given, return youngest write */
	if (!older_than_stack)
		return mod->write_access_list_tail;

	/* Search */
	for (stack = older_than_stack->access_list_prev; stack;
		stack = stack->access_list_prev)
		if (stack->access_kind == mod_access_store)
			return stack;

	/* Not found */
	return NULL;
}


int mod_serves_address(struct mod_t *mod, unsigned int addr)
{
	/* Address bounds */
	if (mod->range_kind == mod_range_bounds)
		return addr >= mod->range.bounds.low &&
			addr <= mod->range.bounds.high;

	/* Interleaved addresses */
	if (mod->range_kind == mod_range_interleaved)
		return (addr / mod->range.interleaved.div) %
			mod->range.interleaved.mod ==
			mod->range.interleaved.eq;

	/* Invalid */
	panic("%s: invalid range kind", __FUNCTION__);
	return 0;
}


/* Return the low module serving a given address. */
struct mod_t *mod_get_low_mod(struct mod_t *mod, unsigned int addr)
{
	struct mod_t *low_mod;
	struct mod_t *server_mod;

	/* Main memory does not have a low module */
	assert(mod_serves_address(mod, addr));
	if (mod->kind == mod_kind_main_memory)
	{
		assert(!linked_list_count(mod->low_mod_list));
		return NULL;
	}

	/* Check which low module serves address */
	server_mod = NULL;
	LINKED_LIST_FOR_EACH(mod->low_mod_list)
	{
		/* Get new low module */
		low_mod = linked_list_get(mod->low_mod_list);
		if (!mod_serves_address(low_mod, addr))
			continue;

		/* Address served by more than one module */
		if (server_mod)
			fatal("%s: low modules %s and %s both serve address 0x%x",
				mod->name, server_mod->name, low_mod->name, addr);

		/* Assign server */
		server_mod = low_mod;
	}

	/* Error if no low module serves address */
	if (!server_mod)
		fatal("module %s: no lower module serves address 0x%x",
			mod->name, addr);

	/* Return server module */
	return server_mod;
}


int mod_get_retry_latency(struct mod_t *mod)
{
	return random() % mod->latency + mod->latency;
}


/* Check if an access to a module can be coalesced with another access older
 * than 'older_than_stack'. If 'older_than_stack' is NULL, check if it can
 * be coalesced with any in-flight access.
 * If it can, return the access that it would be coalesced with. Otherwise,
 * return NULL. */
struct mod_stack_t *mod_can_coalesce(struct mod_t *mod,enum mod_access_kind_t access_kind, unsigned int addr,
	struct mod_stack_t *older_than_stack)
{
	struct mod_stack_t *stack;
	struct mod_stack_t *tail;

	/* For efficiency, first check in the hash table of accesses
	 * whether there is an access in flight to the same block. */
	assert(access_kind);
	/* En el cas dels prefetch a L2+ no podem usar la id per saber
	 * si ha arribat o no abans al mòdul, per tant no podem usar
	 * la funció mod_in_flight_address. Es pot millorar. */
	if ((!mod->cache->prefetch_policy || mod->level == 1) && !mod_in_flight_address(mod, addr, older_than_stack))
		return NULL;

	/* Get youngest access older than 'older_than_stack' */
	tail = older_than_stack ? older_than_stack->access_list_prev :
		mod->access_list_tail;

	/* Coalesce depending on access kind */
	switch (access_kind)
	{

	case mod_access_load:
	{
		for (stack = tail; stack; stack = stack->access_list_prev)
		{
			/* Only coalesce with groups of loads or prefetches at the tail */
			if (stack->access_kind != mod_access_load && stack->access_kind != mod_access_prefetch)
				return NULL;

			/* Only coalesce if destination module is the same */
			if(stack->mod != older_than_stack->mod)
				continue;

			if (stack->addr >> mod->log_block_size == addr >> mod->log_block_size)
				return stack->master_stack ? stack->master_stack : stack;
		}
		break;
	}

	case mod_access_prefetch:
	{
		for (stack = tail; stack; stack = stack->access_list_prev)
		{
			struct mod_t *stack_mod;

			if (stack->access_kind != mod_access_load &&
				stack->access_kind != mod_access_prefetch &&
				stack->access_kind != mod_access_read_request && /* Up down */
				stack->access_kind != mod_access_write_request) /* Up down */
				return NULL;

			if (!stack->target_mod)
				stack_mod = stack->mod;
			else
				stack_mod = stack->target_mod;

			assert(mod && stack_mod);

			/* Only coalesce if destination module is the same */
			if(mod != stack_mod)
				continue;

			if (stack->addr >> mod->log_block_size == addr >> mod->log_block_size)
				return stack->master_stack ? stack->master_stack : stack;
		}
		break;
	}

	case mod_access_read_request:
	case mod_access_write_request:
	{
		for (stack = tail; stack; stack = stack->access_list_prev)
		{
			struct mod_t *stack_mod;
			if (!stack->target_mod)
				stack_mod = stack->mod;
			else
				stack_mod = stack->target_mod;

			assert(mod && stack_mod);

			/* Only coalesce if destination module is the same */
			if(mod != stack_mod)
				continue;

			if (stack->addr >> mod->log_block_size == addr >> mod->log_block_size)
				return stack->master_stack ? stack->master_stack : stack;
		}
		break;
	}

	case mod_access_store:
	{
		/* Only coalesce with last access */
		stack = tail;
		if (!stack)
			return NULL;

		/* Only if it is a write */
		if (stack->access_kind != mod_access_store)
			return NULL;

		/* Only if it is an access to the same block */
		if (stack->addr >> mod->log_block_size != addr >> mod->log_block_size)
			return NULL;

		/* Only if previous write has not started yet */
		if (stack->port_locked)
			return NULL;

		/* Coalesce */
		return stack->master_stack ? stack->master_stack : stack;
	}

	case mod_access_nc_store:
	{
		/* Only coalesce with last access */
		stack = tail;
		if (!stack)
			return NULL;

		/* Only if it is a non-coherent write */
		if (stack->access_kind != mod_access_nc_store)
			return NULL;

		/* Only if it is an access to the same block */
		if (stack->addr >> mod->log_block_size != addr >> mod->log_block_size)
			return NULL;

		/* Only if previous write has not started yet */
		if (stack->port_locked)
			return NULL;

		/* Coalesce */
		return stack->master_stack ? stack->master_stack : stack;
	}

	default:
		panic("%s: invalid access type", __FUNCTION__);
		break;
	}

	/* No access found */
	return NULL;
}


void mod_coalesce(struct mod_t *mod, struct mod_stack_t *master_stack,
	struct mod_stack_t *stack)
{
	/* Debug */
	mem_debug("  %lld %lld 0x%x %s coalesce with %lld\n", esim_cycle,
		stack->id, stack->addr, mod->name, master_stack->id);

	/* Master stack must not have a parent. We only want one level of
	 * coalesced accesses. */
	assert(!master_stack->master_stack);

	/* Access must have been recorded already, which sets the access
	 * kind to a valid value. */
	assert(stack->access_kind);

	/* Set slave stack as a coalesced access */
	stack->coalesced = 1;

	/* If master stack is a prefetch only this access will coalesce with it.
	 * Next accesses will coalesce with this access. */
	if(master_stack->access_kind != mod_access_prefetch) //VVV
		stack->master_stack = master_stack;

	assert(mod->access_list_coalesced_count <= mod->access_list_count);

	/* Record in-flight coalesced access in module */
	mod->access_list_coalesced_count++;
}


void mod_adapt_pref_schedule(struct mod_t *mod)
{
	struct mod_adapt_pref_stack_t *stack;
	struct cache_t *cache = mod->cache;

	/* Create new stack */
	stack = calloc(1, sizeof(struct mod_adapt_pref_stack_t));
	if (!stack)
		fatal("%s: out of memory", __FUNCTION__);
	stack->mod = mod;
	mod->adapt_pref_stack = stack;

	if (cache->prefetch.adapt_interval_kind == interval_kind_cycles)
	{
		/* Schedule first event */
		assert(mod->cache->prefetch.adapt_interval);
		esim_schedule_event(EV_CACHE_ADAPT_PREF, stack, mod->cache->prefetch.adapt_interval);
	}
}


void mod_adapt_pref_handler(int event, void *data)
{
	struct mod_adapt_pref_stack_t *stack = data;
	struct mod_t *mod = stack->mod;
	struct cache_t *cache = mod->cache;
	struct core_thread_tuple_t *tuple;
	int i;

	/* If simulation has ended, no more
	 * events to schedule. */
	if (esim_finish)
		return;

	/* Useful prefetches */
	long long useful_prefetches_int = mod->useful_prefetches -
		stack->last_useful_prefetches;

	/* Cache misses */
	long long accesses_int = mod->no_retry_accesses - stack->last_no_retry_accesses;
	long long hits_int = mod->no_retry_hits - stack->last_no_retry_hits;
	long long misses_int = accesses_int - hits_int;

	/* Pseudocoverage */
	double pseudocoverage_int = (misses_int + useful_prefetches_int) ?
		(double) useful_prefetches_int / (misses_int + useful_prefetches_int) : 0.0;

	/* ROB % stalled cicles due a memory instruction */
	long long cycles_stalled;
	long long cycles_stalled_int;
	{
		int cores = 0;
		int *cores_presence_vector = calloc(x86_cpu_num_cores, sizeof(int));
		LIST_FOR_EACH(mod->threads, i)
		{
			tuple = (struct core_thread_tuple_t *) list_get(mod->threads, i);
			if(!cores_presence_vector[tuple->core])
			{
				cycles_stalled += x86_cpu->core[tuple->core].dispatch_stall_cycles_rob_mem;
				cores_presence_vector[tuple->core] = 1;
				cores++;
			}
		}
		free(cores_presence_vector);
		cycles_stalled /= cores;
		cycles_stalled_int = cycles_stalled - stack->last_cycles_stalled;
	}
	double percentage_cycles_stalled = esim_cycle - stack->last_cycle > 0 ? (double)
		100 * cycles_stalled_int / (esim_cycle - stack->last_cycle) : 0.0;

	/* Mean IPC for all the contexts accessing this module */
	double ipc_int = (double) (stack->inst_count - stack->last_inst_count) /
		(esim_cycle - stack->last_cycle);

	/* Strides detected */
	long long strides_detected_int = cache->prefetch.stride_detector.strides_detected -
		stack->last_strides_detected;

	/* Disable prefetch */
	if(mod->cache->pref_enabled)
	{
		switch(mod->cache->prefetch.adapt_policy)
		{
			case adapt_pref_policy_none:
				break;

			case adapt_pref_policy_misses:
			case adapt_pref_policy_misses_enhanced:
				if((double) misses_int / (misses_int + useful_prefetches_int) > 0.8)
					mod->cache->pref_enabled = 0;
				break;

			case adapt_pref_policy_pseudocoverage:
				if(pseudocoverage_int < 0.2)
					mod->cache->pref_enabled = 0;
				break;

			default:
				fatal("Invalid adaptative prefetch policy");
				break;
		}
		if(!mod->cache->pref_enabled)
			stack->last_cycle_pref_disabled = esim_cycle;
	}

	/* Enable prefetch */
	else
	{
		switch(mod->cache->prefetch.adapt_policy)
		{
			case adapt_pref_policy_none:
				break;

			case adapt_pref_policy_misses:
				if (misses_int > stack->last_misses_int * 1.1)
					mod->cache->pref_enabled = 1;

			break;

			case adapt_pref_policy_misses_enhanced:
				if ((misses_int > stack->last_misses_int * 1.1) ||
					(percentage_cycles_stalled > 0.4 && strides_detected_int > 1500) ||
					(stack->last_cycle_pref_disabled == stack->last_cycle && ipc_int < 0.9 * stack->last_ipc_int))
				{
					mod->cache->pref_enabled = 1;
				}
				break;

			default:
				fatal("Invalid adaptative prefetch policy");
				break;
		}
	}

	stack->last_cycle = esim_cycle;
	stack->last_cycles_stalled = cycles_stalled;
	stack->last_useful_prefetches = mod->useful_prefetches;
	stack->last_no_retry_accesses = mod->no_retry_accesses;
	stack->last_no_retry_hits = mod->no_retry_hits;
	stack->last_misses_int = misses_int;
	stack->last_strides_detected = cache->prefetch.stride_detector.strides_detected;
	stack->last_inst_count = stack->inst_count;
	stack->last_ipc_int = ipc_int;

	if (cache->prefetch.adapt_interval_kind == interval_kind_cycles)
	{
		/* Schedule new event */
		assert(mod->cache->prefetch.adapt_interval);
		esim_schedule_event(event, stack, mod->cache->prefetch.adapt_interval);
	}
}
