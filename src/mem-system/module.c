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

#include <arch/x86/timing/cpu.h>
#include <arch/x86/emu/context.h>
#include <arch/x86/emu/emu.h>
#include <dramsim/bindings-c.h>
#include <lib/esim/esim.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/file.h>
#include <lib/util/linked-list.h>
#include <lib/util/list.h>
#include <lib/util/line-writer.h>
#include <lib/util/misc.h>
#include <lib/util/string.h>
#include <lib/util/repos.h>

#include "cache.h"
#include "directory.h"
#include "local-mem-protocol.h"
#include "mem-system.h"
#include "mod-stack.h"
#include "nmoesi-protocol.h"


static char *help_mod_report =
	"The mod report file shows some relevant statistics related to cache performance\n"
	"at specific intervals.\n"
	"The following fields are shown in each record:\n"
	"\n"
	"  <cycle>\n"
	"      Current simulation cycle.\n"
	"\n"
	"  <inst>\n"
	"      Current simulation instruction.\n"
	"\n"
	"  <...>\n"
	"      Global IPC observed so far. This value is equal to the number of executed\n"
	"      non-speculative instructions divided by the current cycle.\n"
	"\n"
	"  <...>\n"
	"      IPC observed in the current interval. This value is equal to the number\n"
	"      of instructions executed in the current interval divided by the number of\n"
	"      cycles of the interval.\n"
	"\n";


/* String map for access type */
struct str_map_t mod_access_kind_map =
{
	3, {
		{ "Load", mod_access_load },
		{ "Store", mod_access_store },
		{ "NCStore", mod_access_nc_store },
		{ "Prefetch", mod_access_prefetch }
	}
};

/* Event used for updating the state of adaptative prefetch policy */
int EV_MOD_ADAPT_PREF;

/* For reporting interval statistics */
int EV_MOD_REPORT;


/*
 * Public Functions
 */




struct mod_t *mod_create(char *name, enum mod_kind_t kind, int num_ports,
	int block_size, int latency)
{
	struct mod_t *mod;

	/* Initialize */
	mod = xcalloc(1, sizeof(struct mod_t));
	mod->name = xstrdup(name);
	mod->kind = kind;
	mod->latency = latency;

	/* Ports */
	mod->num_ports = num_ports;
	mod->ports = xcalloc(num_ports, sizeof(struct mod_port_t));

	/* Lists */
	mod->low_mod_list = linked_list_create();
	mod->high_mod_list = linked_list_create();

	/* Block size */
	mod->block_size = block_size;
	assert(!(block_size & (block_size - 1)) && block_size >= 4);
	mod->log_block_size = log_base2(block_size);

	mod->client_info_repos = repos_create(sizeof(struct mod_client_info_t), mod->name);

	mod->reachable_threads = xcalloc((long long) x86_cpu_num_cores * (long long) x86_cpu_num_threads, sizeof(char));
	mod->reachable_mm_modules = list_create();

	mod->mc_id = -1; /* By default */

	return mod;
}


void mod_free(struct mod_t *mod)
{
	linked_list_free(mod->low_mod_list);
	linked_list_free(mod->high_mod_list);

	free(mod->reachable_threads);
	list_free(mod->reachable_mm_modules);

	if (mod->cache)
		cache_free(mod->cache);
	if (mod->dir)
		dir_free(mod->dir);

	free(mod->ports);
	repos_free(mod->client_info_repos);
	free(mod->name);

	/* Adaptative prefetch */
	if (mod->adapt_pref_stack)
		free(mod->adapt_pref_stack);

	/* Interval report */
	if(mod->report_stack)
	{
		line_writer_free(mod->report_stack->line_writer);
		free(mod->report_stack);
	}
	file_close(mod->report_file);

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
	void *event_queue_item, struct mod_client_info_t *client_info)
{
	struct mod_stack_t *stack;
	int event = ESIM_EV_NONE;

	/* Create module stack with new ID */
	mod_stack_id++;
	stack = mod_stack_create(mod_stack_id,
		mod, addr, ESIM_EV_NONE, NULL, access_kind == mod_access_prefetch);

	/* Initialize */
	stack->witness_ptr = witness_ptr;
	stack->event_queue = event_queue;
	stack->event_queue_item = event_queue_item;
	stack->client_info = client_info;


	/* Select initial CPU/GPU event */
	if (mod->kind == mod_kind_cache || mod->kind == mod_kind_main_memory)
	{
		if (access_kind == mod_access_load)
		{
			event = EV_MOD_NMOESI_LOAD;
			assert(client_info->ctx_pid>=0);
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
			assert(mod->cache->prefetch.type);
			if (prefetcher_uses_stream_buffers(mod->cache->prefetch.type))
				event = EV_MOD_PREF;

			/* Prefetches go to cache */
			else
				event = EV_MOD_NMOESI_PREFETCH;
		}
		else if (access_kind == mod_access_invalidate_slot)
		{
			assert(prefetcher_uses_stream_buffers(mod->cache->prefetch.type));
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
		else
		{
			panic("%s: invalid access kind", __FUNCTION__);
		}
	}
	else
	{
		panic("%s: invalid mod kind", __FUNCTION__);
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


/* Search for a block in a stream and return the slot where the block is found or -1 if the block is not in the stream */
int mod_find_block_in_stream(struct mod_t *mod, unsigned int addr, int stream)
{
	struct cache_t *cache = mod->cache;
	struct stream_buffer_t *sb = &cache->prefetch.streams[stream];
	struct stream_block_t *block;
	int tag = addr & ~cache->block_mask;
	int i, slot, count;

	count = sb->head + cache->prefetch.max_num_slots;
	for(i = sb->head; i < count; i++){
		slot = i % cache->prefetch.max_num_slots;
		block = cache_get_pref_block(cache, sb->stream, slot);
		if(block->tag == tag && block->state)
			return slot;
	}
	return -1;
}


/* Return {set, way, tag, state} for an address.
 * The function returns TRUE on hit, FALSE on miss. */
int mod_find_block(struct mod_t *mod, unsigned int addr, int *set_ptr,
	int *way_ptr, int *tag_ptr, int *state_ptr)
{
	struct cache_t *cache = mod->cache;
	struct cache_block_t *blk;
	struct dir_lock_t *dir_lock;

	int set = -1;
	int way = -1;
	int tag = -1;

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
	return 1;
}


/* Look for a block in prefetch buffer.
 * The function returns 0 on miss, 1 if hit on head and 2 if hit in the middle of the stream. */
int mod_find_pref_block(struct mod_t *mod, unsigned int addr, int *pref_stream_ptr, int *pref_slot_ptr)
{
	struct cache_t *cache = mod->cache;
	struct stream_block_t *blk;
	struct dir_lock_t *dir_lock;
	struct stream_buffer_t *sb = NULL;

	/* A transient tag is considered a hit if the block is
	 * locked in the corresponding directory */
	int tag = addr & ~cache->block_mask;

	unsigned int stream_tag = addr & cache->prefetch.stream_tag_mask;
	int stream = -1;
	int slot = -1;
	int num_streams = cache->prefetch.max_num_streams;

	for(stream = 0; stream < num_streams; stream++)
	{
		int i, count;
		sb = &cache->prefetch.streams[stream];

		/* Block can't be in this stream */
		/*if(!sb->stream_tag == stream_tag)
			continue;*/

		count = sb->head + cache->prefetch.max_num_slots;
		for(i = sb->head; i < count; i++)
		{
			slot = i % cache->prefetch.max_num_slots;
			blk = cache_get_pref_block(cache, stream, slot);

			/* Increment any invalid unlocked head */
			if(slot == sb->head && !blk->state)
			{
				dir_lock = dir_pref_lock_get(mod->dir, stream, slot);
				if(!dir_lock->lock)
				{
					sb->head = (sb->head + 1) % cache->prefetch.max_num_slots;
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


void mod_block_set_prefetched(struct mod_t *mod, unsigned int addr, int val)
{
	int set, way;

	assert(mod->kind == mod_kind_cache && mod->cache != NULL);
	if (mod->cache->prefetcher && mod_find_block(mod, addr, &set, &way, NULL, NULL))
	{
		mod->cache->sets[set].blocks[way].prefetched = val;
	}
}


int mod_block_get_prefetched(struct mod_t *mod, unsigned int addr)
{
	int set, way;

	assert(mod->kind == mod_kind_cache && mod->cache != NULL);
	if (mod->cache->prefetcher && mod_find_block(mod, addr, &set, &way, NULL, NULL))
	{
		return mod->cache->sets[set].blocks[way].prefetched;
	}

	return 0;
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
	mem_debug("  %lld stack %lld %s port %d locked\n", esim_time, stack->id, mod->name, i);

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
	mem_debug("  %lld %lld %s port unlocked\n", esim_time,
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

	if (stack->background)
		return;

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
	if ((!mod->cache->prefetch.type || mod->level == 1) && !mod_in_flight_address(mod, addr, older_than_stack))
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
			if (stack->access_kind != mod_access_load &&
			    stack->access_kind != mod_access_prefetch)
				return NULL;

			if (stack->addr >> mod->log_block_size ==
				addr >> mod->log_block_size)
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

	case mod_access_prefetch:
	{
		for (stack = tail; stack; stack = stack->access_list_prev)
		{
			if (stack->addr >> mod->log_block_size ==
				addr >> mod->log_block_size)
				return stack;
		}
		break;
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
	mem_debug("  %lld %lld 0x%x %s coalesce with %lld\n", esim_time,
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

	assert(mod->cache->prefetch.adapt_policy);

	/* Create new stack */
	stack = xcalloc(1, sizeof(struct mod_adapt_pref_stack_t));
	stack->mod = mod;
	mod->adapt_pref_stack = stack;

	if (cache->prefetch.adapt_interval_kind == interval_kind_cycles)
	{
		/* Schedule first event */
		assert(mod->cache->prefetch.adapt_interval);
		esim_schedule_event(EV_MOD_ADAPT_PREF, stack, mod->cache->prefetch.adapt_interval);
	}
}


void mod_adapt_pref_handler(int event, void *data)
{
	struct mod_adapt_pref_stack_t *stack = data;
	struct mod_t *mod = stack->mod;
	struct cache_t *cache = mod->cache;

	/* If simulation has ended, no more
	 * events to schedule. */
	if (esim_finish)
		return;

	/* Completed prefetches */
	long long completed_prefetches_int = mod->completed_prefetches -
		stack->last_completed_prefetches;

	/* Useful prefetches */
	long long useful_prefetches_int = mod->useful_prefetches -
		stack->last_useful_prefetches;

	/* Delayed prefetch hits */
	long long delayed_hits_int = mod->delayed_hits - stack->last_delayed_hits;

	/* Lateness */
	double lateness_int = useful_prefetches_int ? (double) delayed_hits_int / useful_prefetches_int : 0.0;
	lateness_int = lateness_int > 1 ? 1 : lateness_int; /* May be slightly greather than 1 due bad timing with cycles */

	/* Cache misses */
	long long accesses_int = mod->no_retry_accesses - stack->last_no_retry_accesses;
	long long hits_int = mod->no_retry_hits - stack->last_no_retry_hits;
	long long misses_int = accesses_int - hits_int;

	/* Pseudocoverage */
	double pseudocoverage_int = (misses_int + useful_prefetches_int) ? (double) useful_prefetches_int / (misses_int + useful_prefetches_int) : 0.0;

	/* Accuracy */
	double accuracy_int = completed_prefetches_int ? (double) useful_prefetches_int / completed_prefetches_int : 0.0;
	accuracy_int = accuracy_int > 1 ? 1 : accuracy_int; /* May be slightly greather than 1 due bad timing with cycles */

	double BWNO_int = 0; /* Bandwidth Needed By Others */

	/* ROB % stalled cicles due a memory instruction */
	long long cycles_stalled = 0;
	long long cycles_stalled_int;
	long long uinst_count = 0;
	{
		int cores = 0;
		for (int core = 0; core < x86_cpu_num_cores; core++)
		{
			/* Reachable cores */
			if (mod->reachable_threads[core * x86_cpu_num_threads])
			{
				uinst_count += X86_CORE.num_committed_uinst;
				cycles_stalled += x86_cpu->core[core].dispatch_stall_cycles_rob_mem;
				cores++;
			}

			/* Non reachable cores */
			else
			{
				int i;
				LIST_FOR_EACH(mod->reachable_mm_modules, i)
				{
					struct mod_t *mm_mod = list_get(mod->reachable_mm_modules, i);
					BWNO_int += dram_system_get_bwn(mm_mod->dram_system->handler, mm_mod->mc_id, core);
				}
			}
		}
		cycles_stalled /= cores;
		cycles_stalled_int = cycles_stalled - stack->last_cycles_stalled;
	}

	double ratio_cycles_stalled = esim_cycle() - stack->last_cycle > 0 ? (double)
		cycles_stalled_int / (esim_cycle() - stack->last_cycle) : 0.0;

	/* Mean IPC for all the contexts accessing this module */
	double ipc_int = (double) (uinst_count - stack->last_uinst_count) /
		(esim_cycle() - stack->last_cycle);

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

			case adapt_pref_policy_adp:
				if (pseudocoverage_int < 0.3)
					cache->pref_enabled = 0;
				break;

			case adapt_pref_policy_adp_gbwc:
			{
				const double BWNO_th = cache->prefetch.thresholds.bwno;

				const double pseudocoverage_th = cache->prefetch.thresholds.pseudocoverage;
				const double ahigh = cache->prefetch.thresholds.accuracy_high;
				const double alow = cache->prefetch.thresholds.accuracy_low;
				const double averylow = cache->prefetch.thresholds.accuracy_very_low;

				cache->prefetch.flags = 0; /* Stats reporting */

				/* Very low accuracy */
				if (accuracy_int < averylow)
				{
					/* Reduce aggr. and disable */
					cache->prefetch.aggr = 2;
					cache->pref_enabled = 0;

					/* Good coverage and others don't require lots of BW */
					if (pseudocoverage_int > pseudocoverage_th && BWNO_int < BWNO_th)
						cache->pref_enabled = 1;
				}

				/* Low accuracy */
				if (accuracy_int < alow)
				{
					/* Reduce aggr. */
					cache->prefetch.aggr = 2;

					/* Bad coverage and others need bw */
					if (pseudocoverage_int < pseudocoverage_th && BWNO_int > BWNO_th)
						cache->pref_enabled = 0;
				}

				/* Medium accuracy */
				else if(accuracy_int < ahigh)
				{
					/* Others need bandwidth */
					if (BWNO_int > BWNO_th)
						cache->prefetch.aggr = 2; /* Reduce aggr. */
				}

				/* High accuracy */
				else
				{
					/* Others need bandwidth */
					if (BWNO_int > BWNO_th)
						cache->prefetch.aggr = 2;

					/* Others don't need a lot of bw */
					else
					{
						/* Bad coverage */
						if (pseudocoverage_int < pseudocoverage_th)
						{
							cache->prefetch.aggr = 4;
						}
					}
				}
				break;
			}

			case adapt_pref_policy_fdp:
			case adapt_pref_policy_fdp_gbwc:
			{
				/* FDP */
				const double ahigh = 0.75;
				const double alow = 0.40;
				const double tlateness = 0.05;
				const int a1 = 1;
				const int a2 = 2;
				const int a3 = 4;

				/* GBWC */
				const double BWNO_th = 2.75;
				const double acc_th = 0.6;

				assert(mod->cache->prefetch.max_num_slots >= a3);
				assert(accuracy_int >= 0 && accuracy_int <= 1);
				assert(lateness_int >= 0 && lateness_int <= 1);

				/* Level 1 */
				if (mod->cache->prefetch.aggr == a1)
				{
					/* Low accuracy */
					if (accuracy_int < alow)
					{}

					/* Medium accuracy */
					else if (accuracy_int < ahigh)
					{
						/* Late */
						if (lateness_int > tlateness)
							mod->cache->prefetch.aggr = a2; /* Increment to increase timeliness */
					}

					/* High accuracy */
					else
					{
						/* Late */
						if (lateness_int > tlateness)
							mod->cache->prefetch.aggr = a2; /* Increment to increase timeliness */
					}

					/* Global Bandwidth Control */
					if (mod->cache->prefetch.adapt_policy == adapt_pref_policy_fdp_gbwc)
					{}
				}

				/* Level 2 */
				else if (mod->cache->prefetch.aggr == a2)
				{
					/* Low accuracy */
					if (accuracy_int < alow)
					{
						/* Late */
						if (lateness_int > tlateness)
							mod->cache->prefetch.aggr = a1; /* Bad and late prefetches -> decrement */
					}

					/* Medium accuracy */
					else if (accuracy_int < ahigh)
					{
						/* Late */
						if (lateness_int > tlateness)
							mod->cache->prefetch.aggr = a3; /* Increment to increase timeliness */
					}

					/* High accuracy */
					else
					{
						/* Late */
						if (lateness_int > tlateness) /* Increment to increase timeliness */
							mod->cache->prefetch.aggr = a3;
					}

					/* Global Bandwidth Control */
					if (mod->cache->prefetch.adapt_policy == adapt_pref_policy_fdp_gbwc)
					{
						if (accuracy_int < acc_th)
						{
							if (BWNO_int > BWNO_th)
								mod->cache->prefetch.aggr = a1; /* Enforce throttle down */
							else if(mod->cache->prefetch.aggr > a2)
								mod->cache->prefetch.aggr = a2; /* Only allow throttle down */
						}
					}
				}

				/* Level 3 */
				else if (mod->cache->prefetch.aggr == a3)
				{
					/* Low accuracy */
					if (accuracy_int < alow)
					{
						/* Late */
						if (lateness_int > tlateness)
							mod->cache->prefetch.aggr = a2; /* Bad and late prefetches -> decrement */
					}

					/* Medium accuracy */
					else if (accuracy_int < ahigh)
					{}

					/* High accuracy */
					else
					{}

					/* Global Bandwidth Control */
					if (mod->cache->prefetch.adapt_policy == adapt_pref_policy_fdp_gbwc)
					{
						if (accuracy_int < acc_th)
						{
							if (BWNO_int > BWNO_th)
								mod->cache->prefetch.aggr = a2; /* Enforce throttle down */
							else if(mod->cache->prefetch.aggr > a3)
								mod->cache->prefetch.aggr = a3; /* Only allow throttle down */
						}
					}
				}

				/* Invalid level */
				else
					fatal("Invalid FDP level");

				break;
			}

			default:
				fatal("Invalid adaptative prefetch policy");
				break;
		}
		if(!mod->cache->pref_enabled)
			stack->last_cycle_pref_disabled = esim_cycle();
	}

	/* Enable prefetch */
	else
	{
		switch(mod->cache->prefetch.adapt_policy)
		{
			case adapt_pref_policy_none:
				break;

			case adapt_pref_policy_adp:
			{
				if ((misses_int > stack->last_misses_int * 1.1) ||
					(ratio_cycles_stalled > 0.4 && strides_detected_int > 250) ||
					(stack->last_cycle_pref_disabled == stack->last_cycle && ipc_int < 0.9 * stack->last_ipc_int))
				{
					mod->cache->pref_enabled = 1;
				}
				break;
			}

			case adapt_pref_policy_adp_gbwc:
			{
				const double BWNO_th = cache->prefetch.thresholds.bwno;
				const double misses_th = cache->prefetch.thresholds.misses;
				const double ipc_th = cache->prefetch.thresholds.ipc;
				const double ratio_cycles_stalled_th = cache->prefetch.thresholds.ratio_cycles_stalled;

				int conditions_fulfilled = 0;

				/* In order to reenable the prefetcher one of the three first conditions AND the forth one must be fulfilled */
				if (misses_int > stack->last_misses_int * misses_th) /* Cond 1 */
					conditions_fulfilled |= 1;

				if (ratio_cycles_stalled > ratio_cycles_stalled_th) /* Cond 2 */
					conditions_fulfilled |= 2;

				if (ipc_int < stack->last_ipc_int * ipc_th) /* Cond 3 */
					conditions_fulfilled |= 4;

				if (BWNO_int < BWNO_th) /* Cond 4 */
					conditions_fulfilled |= 8;

				if (conditions_fulfilled > 8)
					cache->pref_enabled = 1;

				cache->prefetch.flags = conditions_fulfilled;

				break;
			}

			case adapt_pref_policy_fdp:
				fatal("Current policy doesn't disable prefetch");
				break;

			default:
				fatal("Invalid adaptative prefetch policy");
				break;
		}
	}

	stack->last_cycle = esim_cycle();
	stack->last_cycles_stalled = cycles_stalled;
	stack->last_useful_prefetches = mod->useful_prefetches;
	stack->last_delayed_hits = mod->delayed_hits;
	stack->last_completed_prefetches = mod->completed_prefetches;
	stack->last_no_retry_accesses = mod->no_retry_accesses;
	stack->last_no_retry_hits = mod->no_retry_hits;
	stack->last_misses_int = misses_int;
	stack->last_strides_detected = cache->prefetch.stride_detector.strides_detected;
	stack->last_uinst_count = uinst_count;
	stack->last_ipc_int = ipc_int;

	/* Schedule new event */
	assert(mod->cache->prefetch.adapt_interval);
	esim_schedule_event(event, stack, mod->cache->prefetch.adapt_interval);
}


struct mod_client_info_t *mod_client_info_create(struct mod_t *mod)
{
	struct mod_client_info_t *client_info;

	/* Create object */
	client_info = repos_create_object(mod->client_info_repos);

	client_info->core = -1;
	client_info->thread = -1;
	client_info->ctx_pid = -1;

	client_info->stream = -1;
	client_info->slot = -1;

	client_info->prefetcher_eip = -1;

	/* Return */
	return client_info;
}


struct mod_client_info_t *mod_client_info_clone(struct mod_t *mod, struct mod_client_info_t *original)
{
	struct mod_client_info_t *client_info;

	/* Create object */
	client_info = repos_create_object(mod->client_info_repos);

	*client_info = *original;

	/* Return */
	return client_info;
}


void mod_client_info_free(struct mod_t *mod, struct mod_client_info_t *client_info)
{
	repos_free_object(mod->client_info_repos, client_info);
}


void mod_report_schedule(struct mod_t *mod)
{
	struct mod_report_stack_t *stack;
	struct line_writer_t *lw;
	FILE *f = mod->report_file;
	int size;
	int i;

	/* Create new stack */
	stack = xcalloc(1, sizeof(struct mod_report_stack_t));

	/* Initialize */
	assert(mod->report_file);
	assert(mod->report_interval > 0);
	stack->mod = mod;

	/* Print header */
	fprintf(f, "%s", help_mod_report);

	lw = line_writer_create(" ");
	lw->heuristic_size_enabled = 1;

	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "cycle");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "inst");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "completed-prefetches-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "completed-prefetches-glob");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "prefetch-accuracy-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "delayed-hits-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "delayed-hit-avg-lost-cycles-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "misses-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "stream-hits-int");
//	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "effective-prefetch-accuracy-int");
	line_writer_add_column(lw, 8, line_writer_align_right, "%s", "mpki-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "pseudocoverage-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "prefetch-active-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "strides-detected-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "pct-rob-stalled-int");
	line_writer_add_column(lw, 6, line_writer_align_right, "%s", "bwc-int");
	line_writer_add_column(lw, 6, line_writer_align_right, "%s", "bwn-int");
	line_writer_add_column(lw, 6, line_writer_align_right, "%s", "bwno-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "flags");

	size = line_writer_write(lw, f);
	line_writer_clear(lw);

	for (i = 0; i < size - 1; i++)
		fprintf(f, "-");
	fprintf(f, "\n");

	mod->report_stack = stack;
	stack->line_writer = lw;

	/* Schedule first event */
	if(mod->report_interval_kind == interval_kind_cycles)
		esim_schedule_event(EV_MOD_REPORT, stack, mod->report_interval);
}


void mod_report_handler(int event, void *data)
{
	struct mod_report_stack_t *stack = data;
	struct mod_t *mod = stack->mod;
	struct line_writer_t *lw = stack->line_writer;

	/* If simulation has ended, no more
	 * events to schedule. */
	if (esim_finish)
		return;

	/* Iterate all reachable threads */
	/* Stop reporting stats for this module if all threads accessing it have reached its minimum of executed instructions */
	for (int core = 0; core < x86_cpu_num_cores; core++)
		for (int thread = 0; thread < x86_cpu_num_threads; thread++)
			if (mod->reachable_threads[core * x86_cpu_num_threads + thread]) /* Can this thread access the module? */
				if (!x86_emu_min_inst_per_ctx || (X86_THREAD.ctx && X86_THREAD.ctx->inst_count < x86_emu_min_inst_per_ctx)) /* Reached limit? */
					goto minimum_not_reached;
	return;
	minimum_not_reached:;

	/* Prefetch accuracy */
	long long completed_prefetches_int = mod->completed_prefetches -
		stack->completed_prefetches;
	long long useful_prefetches_int = mod->useful_prefetches -
		stack->useful_prefetches;
	double prefetch_accuracy_int = completed_prefetches_int ?
		(double) useful_prefetches_int / completed_prefetches_int : 0.0;
	prefetch_accuracy_int = prefetch_accuracy_int > 1 ? 1 : prefetch_accuracy_int; /* May be slightly greather than 1 due bad timing with cycles */

	/* Delayed hits */
	long long delayed_hits_int = mod->delayed_hits -
		stack->delayed_hits;
	long long delayed_hit_cycles_int = mod->delayed_hit_cycles -
		stack->delayed_hit_cycles;
	double delayed_hit_avg_lost_cycles_int = delayed_hits_int ?
		(double) delayed_hit_cycles_int / delayed_hits_int : 0.0;

	/* Cache misses */
	long long accesses_int = mod->no_retry_accesses - stack->no_retry_accesses;
	long long hits_int = mod->no_retry_hits - stack->no_retry_hits;
	long long misses_int = accesses_int - hits_int;

	/* Stream hits */
	long long stream_hits_int = mod->no_retry_stream_hits - stack->no_retry_stream_hits;

	/* Effective prefetch accuracy */
	long long effective_useful_prefetches_int = mod->effective_useful_prefetches -
		stack->effective_useful_prefetches;
	double effective_prefetch_accuracy_int = completed_prefetches_int ?
		(double) effective_useful_prefetches_int / completed_prefetches_int : 0.0;
	effective_prefetch_accuracy_int = effective_prefetch_accuracy_int > 1 ? 1 : effective_prefetch_accuracy_int; /* May be slightly greather than 1 due bad timing with cycles */

	double BWNO_int = 0; /* Bandwidth Needed By Others */
	long long uinst_count = 0;
	long long cycles_stalled = 0;
	double pct_rob_stalled_int; /* ROB % stalled cicles due a memory instruction */
	{
		int cores = 0;
		long long cycles_int = esim_cycle() - stack->last_cycle;
		long long cycles_stalled_int;

		for (int core = 0; core < x86_cpu_num_cores; core++)
		{
			/* Reachable cores */
			if (mod->reachable_threads[core * x86_cpu_num_threads])
			{
				uinst_count += X86_CORE.num_committed_uinst;
				cycles_stalled += x86_cpu->core[core].dispatch_stall_cycles_rob_mem;
				cores++;
			}

			/* Non reachable cores */
			else
			{
				int i;
				LIST_FOR_EACH(mod->reachable_mm_modules, i)
				{
					struct mod_t *mm_mod = list_get(mod->reachable_mm_modules, i);
					BWNO_int += dram_system_get_bwn(mm_mod->dram_system->handler, mm_mod->mc_id, core);
				}
			}
		}

		cycles_stalled /= cores;
		cycles_stalled_int = cycles_stalled - stack->last_cycles_stalled;
		pct_rob_stalled_int = (esim_cycle() - stack->last_cycle) ? (double)
				100 * cycles_stalled_int / cycles_int : 0.0;
	}

	/* MPKI */
	double mpki_int = (double) misses_int / ((uinst_count - stack->uinst_count) / 1000.0);

	/* Pseudocoverage */
	double pseudocoverage_int = (misses_int + useful_prefetches_int) ?
		(double) useful_prefetches_int / (misses_int + useful_prefetches_int) : 0.0;

	/* Detected strides */
	long long detected_strides_int = mod->cache->prefetch.stride_detector.strides_detected - stack->strides_detected;

	/* Dump stats */
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", esim_cycle());
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", uinst_count);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", completed_prefetches_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", mod->completed_prefetches);
	line_writer_add_column(lw, 9, line_writer_align_right, "%.3f", prefetch_accuracy_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", delayed_hits_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%.3f", delayed_hit_avg_lost_cycles_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", misses_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", stream_hits_int);
	line_writer_add_column(lw, 8, line_writer_align_right, "%.2f", mpki_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%.3f", pseudocoverage_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%u", mod->cache->pref_enabled && mod->accesses ? mod->cache->prefetch.aggr : 0);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", detected_strides_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%.3f", pct_rob_stalled_int);
	line_writer_add_column(lw, 6, line_writer_align_right, "%.2f", BWNO_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%u", mod->cache->prefetch.flags);

	line_writer_write(lw, mod->report_file);
	line_writer_clear(lw);

	/* Update counters */
	stack->last_cycle = esim_cycle();
	stack->uinst_count = uinst_count;
	stack->delayed_hits = mod->delayed_hits;
	stack->delayed_hit_cycles = mod->delayed_hit_cycles;
	stack->useful_prefetches = mod->useful_prefetches;
	stack->completed_prefetches = mod->completed_prefetches;
	stack->no_retry_accesses = mod->no_retry_accesses;
	stack->no_retry_hits = mod->no_retry_hits;
	stack->no_retry_stream_hits = mod->no_retry_stream_hits;
	stack->effective_useful_prefetches = mod->effective_useful_prefetches;
	stack->misses_int = misses_int;
	stack->strides_detected = mod->cache->prefetch.stride_detector.strides_detected;
	stack->last_cycles_stalled = cycles_stalled;

	/* Schedule new event */
	assert(mod->report_interval);
	esim_schedule_event(event, stack, mod->report_interval);
}


void mod_report_stack_reset_stats(struct mod_report_stack_t *stack)
{
	int i;
	int size;
	struct line_writer_t *lw = stack->line_writer;
	FILE *f = stack->mod->report_file;

	stack->last_cycle = esim_cycle();
	stack->uinst_count = 0;
	stack->delayed_hits = 0;
	stack->delayed_hit_cycles = 0;
	stack->useful_prefetches = 0;
	stack->completed_prefetches = 0;
	stack->no_retry_accesses = 0;
	stack->no_retry_hits = 0;
	stack->no_retry_stream_hits = 0;
	stack->effective_useful_prefetches = 0;
	stack->misses_int = 0;
	stack->strides_detected = 0;
	stack->last_cycles_stalled = 0;

	/* Erase report file */
	fseeko(f, 0, SEEK_SET);

	/* Print header */
	fprintf(f, "%s\nRESETED\n", help_mod_report);

	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "cycle");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "inst");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "completed-prefetches-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "completed-prefetches-glob");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "prefetch-accuracy-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "delayed-hits-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "delayed-hit-avg-lost-cycles-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "misses-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "stream-hits-int");
	line_writer_add_column(lw, 8, line_writer_align_right, "%s", "mpki-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "pseudocoverage-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "prefetch-active-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "strides-detected-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "pct-rob-stalled-int");
	line_writer_add_column(lw, 6, line_writer_align_right, "%s", "bwc-int");
	line_writer_add_column(lw, 6, line_writer_align_right, "%s", "bwn-int");
	line_writer_add_column(lw, 6, line_writer_align_right, "%s", "bwno-int");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "flags");

	size = line_writer_write(lw, f);
	line_writer_clear(lw);

	for (i = 0; i < size - 1; i++)
		fprintf(f, "-");
	fprintf(f, "\n");
}


void mod_adapt_pref_stack_reset_stats(struct mod_adapt_pref_stack_t *stack)
{
	stack->last_uinst_count = 0;
	stack->last_cycle = esim_cycle();
	stack->last_useful_prefetches = 0;
	stack->last_completed_prefetches = 0;
	stack->last_no_retry_accesses = 0;
	stack->last_no_retry_hits = 0;
	stack->last_cycles_stalled = 0;
	stack->last_misses_int = 0;
	stack->last_strides_detected = 0;
	stack->last_delayed_hits = 0;
	stack->last_cycle_pref_disabled = 0;
	stack->last_ipc_int = 0;
}


void mod_reset_stats(struct mod_t *mod)
{
	mod->accesses = 0;
	mod->hits = 0;

	mod->reads = 0;
	mod->effective_reads = 0;
	mod->effective_read_hits = 0;
	mod->writes = 0;
	mod->effective_writes = 0;
	mod->effective_write_hits = 0;
	mod->nc_writes = 0;
	mod->effective_nc_writes = 0;
	mod->effective_nc_write_hits = 0;
	mod->prefetches = 0;
	mod->evictions = 0;

	mod->blocking_reads = 0;
	mod->non_blocking_reads = 0;
	mod->read_hits = 0;
	mod->blocking_writes = 0;
	mod->non_blocking_writes = 0;
	mod->write_hits = 0;
	mod->blocking_nc_writes = 0;
	mod->non_blocking_nc_writes = 0;
	mod->nc_write_hits = 0;

	mod->read_retries = 0;
	mod->write_retries = 0;
	mod->nc_write_retries = 0;

	mod->no_retry_accesses = 0;
	mod->no_retry_hits = 0;
	mod->no_retry_reads = 0;
	mod->no_retry_read_hits = 0;
	mod->no_retry_writes = 0;
	mod->no_retry_write_hits = 0;
	mod->no_retry_nc_writes = 0;
	mod->no_retry_nc_write_hits = 0;
	mod->no_retry_stream_hits = 0;

	/* Prefetch */
	mod->programmed_prefetches = 0;
	mod->completed_prefetches = 0;
	mod->canceled_prefetches = 0;
	mod->canceled_prefetches_end_stream = 0;
	mod->canceled_prefetches_coalesce = 0;
	mod->canceled_prefetches_cache_hit = 0;
	mod->canceled_prefetches_stream_hit = 0;
	mod->canceled_prefetches_retry = 0;
	mod->useful_prefetches = 0;
	mod->effective_useful_prefetches = 0; /* Useful prefetches with less delay hit cicles than 1/3 of the delay of accesing MM */

	mod->prefetch_retries = 0;

	mod->stream_hits = 0;
	mod->delayed_hits = 0; /* Hit on a block being brougth by a prefetch */
	mod->delayed_hit_cycles = 0; /* Cicles lost due delayed hits */
	mod->delayed_hits_cycles_counted = 0; /* Number of delayed hits whose lost cycles has been counted */

	mod->single_prefetches = 0; /* Prefetches on hit */
	mod->group_prefetches = 0; /* Number of GROUPS */
	mod->canceled_prefetch_groups = 0;

	mod->up_down_hits = 0;
	mod->up_down_head_hits = 0;
	mod->down_up_read_hits = 0;
	mod->down_up_write_hits = 0;

	mod->fast_resumed_accesses = 0;
	mod->write_buffer_read_hits = 0;
	mod->write_buffer_write_hits = 0;
	mod->write_buffer_prefetch_hits = 0;

	mod->stream_evictions = 0;

	/* Silent replacement */
	mod->down_up_read_misses = 0;
	mod->down_up_write_misses = 0;
	mod->block_already_here = 0;

	/* Reset stacks */
	if (mod->report_stack)
		mod_report_stack_reset_stats(mod->report_stack);
	if (mod->adapt_pref_stack)
		mod_adapt_pref_stack_reset_stats(mod->adapt_pref_stack);
}


/* Up-down recursive reset of module stats */
void mod_recursive_reset_stats(struct mod_t *mod)
{
	mod_reset_stats(mod);
	LINKED_LIST_FOR_EACH(mod->low_mod_list)
	{
		struct mod_t *low_mod = linked_list_get(mod->low_mod_list);
		mod_recursive_reset_stats(low_mod);
	}
}

