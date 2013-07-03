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
 *  You should have received stack copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <assert.h>

#include <lib/esim/esim.h>
#include <lib/esim/trace.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/linked-list.h>
#include <lib/util/list.h>
#include <lib/util/misc.h>
#include <lib/util/string.h>
#include <network/network.h>
#include <network/node.h>

#include "cache.h"
#include "directory.h"
#include "mem-controller.h"
#include "mem-system.h"
#include "mmu.h"
#include "mod-stack.h"
#include "prefetcher.h"

#include "bank.h"
#include "channel.h"
#include "mem-controller.h"
#include "rank.h"
/* Events */

int EV_MOD_NMOESI_LOAD;
int EV_MOD_NMOESI_LOAD_LOCK;
int EV_MOD_NMOESI_LOAD_ACTION;
int EV_MOD_NMOESI_LOAD_MISS;
int EV_MOD_NMOESI_LOAD_UNLOCK;
int EV_MOD_NMOESI_LOAD_FINISH;

int EV_MOD_NMOESI_STORE;
int EV_MOD_NMOESI_STORE_LOCK;
int EV_MOD_NMOESI_STORE_ACTION;
int EV_MOD_NMOESI_STORE_UNLOCK;
int EV_MOD_NMOESI_STORE_FINISH;

int EV_MOD_NMOESI_PREFETCH;
int EV_MOD_NMOESI_PREFETCH_LOCK;
int EV_MOD_NMOESI_PREFETCH_ACTION;
int EV_MOD_NMOESI_PREFETCH_MISS;
int EV_MOD_NMOESI_PREFETCH_UNLOCK;
int EV_MOD_NMOESI_PREFETCH_FINISH;

int EV_MOD_NMOESI_NC_STORE;
int EV_MOD_NMOESI_NC_STORE_LOCK;
int EV_MOD_NMOESI_NC_STORE_WRITEBACK;
int EV_MOD_NMOESI_NC_STORE_ACTION;
int EV_MOD_NMOESI_NC_STORE_MISS;
int EV_MOD_NMOESI_NC_STORE_UNLOCK;
int EV_MOD_NMOESI_NC_STORE_FINISH;

int EV_MOD_NMOESI_FIND_AND_LOCK;
int EV_MOD_NMOESI_FIND_AND_LOCK_PORT;
int EV_MOD_NMOESI_FIND_AND_LOCK_PREF_STREAM;
int EV_MOD_NMOESI_FIND_AND_LOCK_ACTION;
int EV_MOD_NMOESI_FIND_AND_LOCK_FINISH;

int EV_MOD_NMOESI_EVICT;
int EV_MOD_NMOESI_EVICT_INVALID;
int EV_MOD_NMOESI_EVICT_ACTION;
int EV_MOD_NMOESI_EVICT_RECEIVE;
int EV_MOD_NMOESI_EVICT_PROCESS;
int EV_MOD_NMOESI_EVICT_PROCESS_NONCOHERENT;
int EV_MOD_NMOESI_EVICT_REPLY;
int EV_MOD_NMOESI_EVICT_REPLY_RECEIVE;
int EV_MOD_NMOESI_EVICT_FINISH;

int EV_MOD_NMOESI_WRITE_REQUEST;
int EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE;
int EV_MOD_NMOESI_WRITE_REQUEST_LOCK;
int EV_MOD_NMOESI_WRITE_REQUEST_ACTION;
int EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE;
int EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN;
int EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH;
int EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP;
int EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH;
int EV_MOD_NMOESI_WRITE_REQUEST_REPLY;
int EV_MOD_NMOESI_WRITE_REQUEST_FINISH;

int EV_MOD_NMOESI_READ_REQUEST;
int EV_MOD_NMOESI_READ_REQUEST_RECEIVE;
int EV_MOD_NMOESI_READ_REQUEST_LOCK;
int EV_MOD_NMOESI_READ_REQUEST_ACTION;
int EV_MOD_NMOESI_READ_REQUEST_UPDOWN;
int EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS;
int EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH;
int EV_MOD_NMOESI_READ_REQUEST_DOWNUP;
int EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS;
int EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH;
int EV_MOD_NMOESI_READ_REQUEST_REPLY;
int EV_MOD_NMOESI_READ_REQUEST_FINISH;

int EV_MOD_NMOESI_INVALIDATE;
int EV_MOD_NMOESI_INVALIDATE_FINISH;

int EV_MOD_NMOESI_PEER_SEND;
int EV_MOD_NMOESI_PEER_RECEIVE;
int EV_MOD_NMOESI_PEER_REPLY;
int EV_MOD_NMOESI_PEER_FINISH;

int EV_MOD_NMOESI_MESSAGE;
int EV_MOD_NMOESI_MESSAGE_RECEIVE;
int EV_MOD_NMOESI_MESSAGE_ACTION;
int EV_MOD_NMOESI_MESSAGE_REPLY;
int EV_MOD_NMOESI_MESSAGE_FINISH;

int EV_MOD_PREF;
int EV_MOD_PREF_LOCK;
int EV_MOD_PREF_ACTION;
int EV_MOD_PREF_MISS;
int EV_MOD_PREF_UNLOCK;
int EV_MOD_PREF_FINISH;

int EV_MOD_NMOESI_PREF_FIND_AND_LOCK;
int EV_MOD_NMOESI_PREF_FIND_AND_LOCK_PORT;
int EV_MOD_NMOESI_PREF_FIND_AND_LOCK_ACTION;
int EV_MOD_NMOESI_PREF_FIND_AND_LOCK_FINISH;

int EV_MOD_NMOESI_INVALIDATE_SLOT;
int EV_MOD_NMOESI_INVALIDATE_SLOT_LOCK;
int EV_MOD_NMOESI_INVALIDATE_SLOT_ACTION;
int EV_MOD_NMOESI_INVALIDATE_SLOT_UNLOCK;
int EV_MOD_NMOESI_INVALIDATE_SLOT_FINISH;


int EV_MOD_NMOESI_EXAMINE_ONLY_ONE_QUEUE_REQUEST;
int EV_MOD_NMOESI_EXAMINE_QUEUE_REQUEST;
int EV_MOD_NMOESI_ACCES_BANK;
int EV_MOD_NMOESI_TRANSFER_FROM_BANK;
int EV_MOD_NMOESI_REMOVE_MEMORY_CONTROLLER;
int EV_MOD_NMOESI_INSERT_MEMORY_CONTROLLER;

int EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER;
int EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_PORT;
int EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_ACTION;
int EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_FINISH;


/* AUXILIARY FUNCTIONS */

int must_enqueue_prefetch(struct mod_stack_t *stack)
{
	struct mod_t *target_mod;

	/* L1 prefetch */
	if (!stack->target_mod)
		return stack->mod->cache->pref_enabled && !stack->background;

	/* Deeper than L1 prefetch */
	target_mod = stack->target_mod;
	return target_mod->cache->pref_enabled && //El prefetch està habilitat
		!stack->prefetch && //No estem en una cadena de prefetch
		!stack->background && //La stack fast resumed ja haurà encuat el prefetch, si calia
		target_mod->kind == mod_kind_cache && //És una cache
		stack->request_dir == mod_request_up_down; //Petició up down
}


void enqueue_prefetch_group(int core, int thread, struct mod_t *mod, unsigned int base_addr, int stride)
{
	struct cache_t *cache = mod->cache;
	struct stream_buffer_t *sb;
	struct mod_client_info_t *client_info;
	unsigned int mmu_page_tag;
	unsigned int prev_address;
	int num_prefetches = cache->prefetch.aggressivity;
	int stream;
	int dead = 0; /* Address over/underflow or mem page changed */
	int eos = 0; /* End of stream */
	int i;

	/* If there is a stream with the same tag, replace it. If not, replace the last recently used one. */
	stream = cache_find_stream(cache, base_addr & ~cache->prefetch.stream_mask);
	if (stream == -1) /* Stream tag not found */
		stream = cache_select_stream(cache);
	sb = &cache->prefetch.streams[stream];

	prev_address = base_addr - stride;
	mmu_page_tag = prev_address & ~mmu_page_mask;

	/* Assertions */
	assert(stride); /* Valid stride */
	assert(mmu_page_tag == (base_addr & ~mmu_page_mask)); /* No end of page */
	assert(stride > 0 || (prev_address + stride) < prev_address); /* No underflow */
	assert(stride < 0 || (prev_address + stride) > prev_address); /* No overflow */

	/* Not enqueue prefetch if there are pending prefetches */
	if (sb->pending_prefetches)
	{
		mem_debug("    Canceled prefetch group at addr=0x%x to stream=%d with stride=0x%x(%d)\n", base_addr, stream, stride, stride);
		mod->canceled_prefetch_groups++;
		return;
	}

	/* Set stream's transcient tag to indicate the block is being brought */
	sb->stream_transcient_tag = base_addr & ~cache->prefetch.stream_mask;

	/* Set stream's new stride */
	sb->stride = stride;

	/* Debug */
	mem_debug("    Enqueued prefetch group at addr=0x%x to stream=%d with stride=0x%x(%d)\n", base_addr, stream, stride, stride);

	/* Mark the number of pending prefetches for this stream */
	sb->pending_prefetches += num_prefetches;
	sb->cycle = esim_time;

	/* Insert prefetches */
	sb->next_address = base_addr;
	for (i = 0; i < num_prefetches; i++)
	{
		unsigned int prefetch_addr = sb->next_address;
		enum mod_access_kind_t access_kind;
		int invalidate = 0;

		if(dead || eos)
		{
			prefetch_addr = sb->stream_transcient_tag;
			invalidate = 1;
		}
		else
		{
			prev_address = sb->next_address;
			sb->next_address += stride;

			/* End of page */
			if(mmu_page_tag != (sb->next_address & ~mmu_page_mask))
				dead = 1;

			/* Underflow */
			else if(stride < 0 && (prev_address + stride) > prev_address)
				dead = 1;

			/* Overflow */
			else if(stride > 0 && (prev_address + stride) < prev_address)
				dead = 1;

			/* End of stream */
			else if (sb->stream_transcient_tag != (sb->next_address & ~cache->prefetch.stream_mask))
				eos = 1;
		}

		client_info = mod_client_info_create(mod);
		client_info->core = core;
		client_info->thread = thread;
		client_info->stream = stream;
		client_info->slot = (sb->head + i) % sb->num_slots;

		if (invalidate)
		{
			mod->canceled_prefetches++;
			mod->canceled_prefetches_end_stream++;
			access_kind = mod_access_invalidate_slot;
		}
		else
			access_kind = mod_access_prefetch;

		mod_access(mod, access_kind, prefetch_addr, NULL, NULL, NULL, client_info);
	}

	/* Reset tail */
	sb->tail = sb->head; //TAIL

	/* Statistics */
	mod->group_prefetches++;

	/* Set dead flag */
	sb->dead = dead;
}


void enqueue_prefetch_on_miss(struct mod_stack_t *stack)
{
	struct mod_t *mod;

	assert(stack->stride);

	/* L1 prefetch */
	if (!stack->target_mod)
		mod = stack->mod;

	/* Deeper than L1 prefetch */
	else
		mod = stack->target_mod;

	/* Underflow */
	if(stack->stride < 0 && (stack->addr + stack->stride) > stack->addr) return;

	/* Overflow */
	if(stack->stride > 0 && (stack->addr + stack->stride) < stack->addr) return;

	/* End of page */
	if((stack->addr & ~mmu_page_mask) != ((stack->addr + stack->stride) & ~mmu_page_mask)) return;

	enqueue_prefetch_group(stack->client_info->core, stack->client_info->thread, mod, stack->addr + stack->stride, stack->stride);

	return;
}


void enqueue_prefetch_on_hit(struct mod_stack_t *stack)
{
	struct mod_t *mod;
	struct cache_t *cache;
	struct stream_buffer_t *sb;
	struct mod_client_info_t *client_info;
	unsigned int prev_address;
	unsigned int mmu_page_tag;

	/* L1 prefetch */
	if (!stack->target_mod)
		mod = stack->mod;

	/* Deeper than L1 prefetch */
	else
		mod = stack->target_mod;

	cache = mod->cache;
	sb = &cache->prefetch.streams[stack->pref_stream];

	/* Next address of this stream is out of page, over/underflowed or has a different stream tag */
	if (sb->dead)
		return;

	prev_address = sb->next_address - sb->stride;
	mmu_page_tag = prev_address & ~mmu_page_mask;

	/* Assertions */
	assert(sb->stride); /* Valid stride */
	assert(mmu_page_tag == (sb->next_address & ~mmu_page_mask)); /* End of page */
	assert(sb->stride > 0 || (prev_address + sb->stride) < prev_address); /* Underflow */
	assert(sb->stride < 0 || (prev_address + sb->stride) > prev_address); /* Overflow */

	/* Enqueue a new prefetch group if next_address is not in the stream anymore */
	if (sb->stream_transcient_tag != (sb->next_address & ~cache->prefetch.stream_mask))
	{
		sb->dead = 1; /* Only do this the first time */
		enqueue_prefetch_group(stack->client_info->core, stack->client_info->thread, mod, sb->next_address, sb->stride);
		return;
	}

	/* Statistics */
	mod->single_prefetches++;

	mem_debug("    Enqueued single prefetch at addr=0x%x to stream=%d with stride=0x%x(%d)\n", sb->next_address + sb->stride, sb->stream, sb->stride, sb->stride);

	client_info = mod_client_info_create(mod);
	client_info->core = stack->client_info->core;
	client_info->thread = stack->client_info->thread;
	client_info->stream = sb->stream;
	client_info->slot = sb->tail;
	mod_access(mod, mod_access_prefetch, sb->next_address, NULL, NULL, NULL, client_info);

	/* Update tail */
	sb->tail = (sb->tail + 1) % sb->num_slots; //TAIL

	/* Add a pending prefetch */
	sb->pending_prefetches++;
	sb->cycle = esim_time;

	/* Increment next address */
	prev_address = sb->next_address;
	sb->next_address = sb->next_address + sb->stride;

	/* End of page */
	if(mmu_page_tag != (sb->next_address & ~mmu_page_mask))
		sb->dead = 1;

	/* Underflow */
	else if(sb->stride < 0 && (prev_address + sb->stride) > prev_address)
		sb->dead = 1;

	/* Overflow */
	else if(sb->stride > 0 && (prev_address + sb->stride) < prev_address)
		sb->dead = 1;
}


void enqueue_prefetch_obl(struct mod_stack_t *stack)
{
/* 	struct mod_t *mod; */
/* 	struct x86_uop_t *uop; */
/*  */
/* 	#<{(| L1 prefetch |)}># */
/* 	if (!stack->target_mod) */
/* 		mod = stack->mod; */
/*  */
/* 	#<{(| Deeper than L1 prefetch |)}># */
/* 	else */
/* 		mod = stack->target_mod; */
/*  */
/* 	uop = x86_uop_create(); */
/* 	uop->uinst = x86_uinst_create(); */
/* 	uop->uinst->opcode = x86_uinst_prefetch; */
/* 	if (mod->cache->pref_enabled == prefetch_policy_obl_stride) */
/* 		uop->phy_addr = stack->addr + stack->stride; */
/* 	else */
/* 		uop->phy_addr = stack->addr + mod->block_size; */
/* 	uop->core = stack->core; */
/* 	uop->thread = stack->thread; */
/* 	uop->flags = X86_UINST_MEM; */
/* 	uop->pref.mod = mod; */
/*  */
/* 	if (mod->level == 1) */
/* 		x86_pq_insert(uop); */
/* 	else */
/* 	{ */
/* 		linked_list_out(mod->pq); */
/* 		linked_list_insert(mod->pq, uop); */
/* 	} */
}


/* NMOESI Protocol */

void mod_handler_pref(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;
	struct mod_t *mod = stack->mod;
	struct cache_t *cache = mod->cache;
	struct stream_buffer_t *sb;

	stack->prefetch = mod->level;

	if (event == EV_MOD_PREF)
	{
		mem_debug("%lld %lld 0x%x %s stream=%d slot=%d pref\n", esim_time, stack->id, stack->addr, mod->name, stack->pref.dest_stream, stack->pref.dest_slot);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"pref\" state=\"%s:pref\" addr=0x%x\n", stack->id, mod->name, stack->addr);

		/* Record access */
		mod_access_start(mod, stack, mod_access_prefetch);

		/* Statistics */
		mod->programmed_prefetches++;

		/* Set pref stream and slot */
		stack->pref_stream = stack->client_info->stream;
		stack->pref_slot = stack->client_info->slot;

		/* Next event */
		esim_schedule_event(EV_MOD_PREF_LOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_PREF_LOCK)
	{
		struct mod_stack_t *master_stack;
		struct mod_stack_t *older_stack;

		mem_debug("  %lld %lld 0x%x %s pref lock\n", esim_time, stack->id, stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_lock\"\n", stack->id, mod->name);

		/* Coalesce access, if so die or invalidate slot */
		master_stack = mod_can_coalesce(mod, mod_access_prefetch, stack->addr, stack);
		if (master_stack)
		{
			mem_debug("    %lld will finish due to %lld\n", stack->id, master_stack->id);
			mod->canceled_prefetches++; /* Statistics */
			mod->canceled_prefetches_mshr++; /* Statistics */
			new_stack = mod_stack_create(stack->id, mod, stack->addr, EV_MOD_PREF_FINISH, stack, stack->prefetch);
			new_stack->client_info = mod_client_info_clone(stack->mod, stack->client_info);
			new_stack->retry = 0;
			esim_schedule_event(EV_MOD_NMOESI_INVALIDATE_SLOT, new_stack, 0);
			return;
		}

		/* If there is any older write, wait for it */
		older_stack = mod_in_flight_write(mod, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for write %lld\n",
				stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_PREF_LOCK);
			return;
		}

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr, EV_MOD_PREF_ACTION, stack, stack->prefetch);
		new_stack->blocking = 1;
		new_stack->read = 1;
		new_stack->retry = stack->retry;
		new_stack->access_kind = mod_access_prefetch;
		new_stack->request_dir = mod_request_up_down;
		new_stack->pref_stream = stack->pref_stream;
		new_stack->pref_slot = stack->pref_slot;
		esim_schedule_event(EV_MOD_NMOESI_PREF_FIND_AND_LOCK, new_stack, 0);
		return;
	}

	if (event == EV_MOD_PREF_ACTION)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s pref action\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_action\"\n",
			stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			mod->prefetch_retries++;
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_PREF_LOCK, stack, retry_lat);
			return;
		}

		/* Hit */
		if (stack->hit)
		{
			mem_debug("    will finish due to block already in cache (or write buffer)\n");
			mod->canceled_prefetches++; /* Statistics */
			esim_schedule_event(EV_MOD_PREF_UNLOCK, stack, 0);
			return;
		}

		/* Stream hit -- Block already in the stream */
		if (stack->stream_hit)
		{
			mem_debug("     block is already in the stream ");
			if (stack->pref_slot == stack->src_pref_slot)
			{
				mem_debug("and in the correct slot\n");
				mod->canceled_prefetches++; /* Statistics */
				esim_schedule_event(EV_MOD_PREF_FINISH, stack, 0);
			}
			else
			{
				mem_debug("but not in the correct slot\n");
				esim_schedule_event(EV_MOD_PREF_MISS, stack, 0);
			}
			return;
		}

		/* Miss */
		new_stack = mod_stack_create(stack->id, mod, stack->tag,
			EV_MOD_PREF_MISS, stack, stack->prefetch);
		new_stack->peer = mod;
		new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);
		return;
	}

	if (event == EV_MOD_PREF_MISS)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s pref miss\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_miss\"\n",
			stack->id, mod->name);

		/* Error on read request. Unlock block and retry prefetch */
		if (stack->err)
		{
			mod->read_retries++;
			retry_lat = mod_get_retry_latency(mod);
			dir_pref_entry_unlock(mod->dir, stack->pref_stream, stack->pref_slot);
			mod->cache->prefetch.streams[stack->pref_stream].count--; //COUNT
			mem_debug("    pref lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_PREF_LOCK, stack, retry_lat);
			return;
		}

		cache_get_pref_block_data(cache, stack->pref_stream, stack->pref_slot, NULL, &stack->state);
		assert(!stack->state);

		/* Store block */
		cache_set_pref_block(cache, stack->pref_stream, stack->pref_slot, stack->tag, stack->shared ? cache_block_shared : cache_block_exclusive);

		/* If block comes from another slot in the same stream invalidate the original one */
		if (stack->stream_hit)
			cache_set_pref_block(cache, stack->src_pref_stream, stack->src_pref_slot, 0, cache_block_invalid);

		/* Continue */
		esim_schedule_event(EV_MOD_PREF_UNLOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_PREF_UNLOCK)
	{
		mem_debug("  %lld %lld 0x%x %s pref unlock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_unlock\"\n",
			stack->id, mod->name);

		/* Unlock directory entry */
		dir_pref_entry_unlock(mod->dir, stack->pref_stream, stack->pref_slot);

		/* Block was in the stream */
		if (stack->stream_hit)
			dir_pref_entry_unlock(mod->dir, stack->src_pref_stream, stack->src_pref_slot);

		/* When only remains one pending prefetch the stream tag is set */
		sb = &cache->prefetch.streams[stack->pref_stream];
		assert(stack->pref_stream >= 0 && stack->pref_stream < cache->prefetch.num_streams);
		assert(stack->pref_slot >= 0 && stack->pref_slot < cache->prefetch.aggressivity);
		assert(sb->pending_prefetches > 0);
		if (sb->pending_prefetches == 1)
		{
			sb->stream_tag = stack->addr & ~cache->prefetch.stream_mask;
			assert(sb->stream_tag == sb->stream_transcient_tag);
		}
		sb->pending_prefetches--;

		/* Statitistics */
		if(!stack->hit)
			mod->completed_prefetches++;

		/* Continue */
		esim_schedule_event(EV_MOD_PREF_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_PREF_FINISH)
	{
		mem_debug("%lld %lld 0x%x %s pref finish\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_finish\"\n",
			stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n",
			stack->id);

		/* Increment witness variable */
		if (stack->witness_ptr)
			(*stack->witness_ptr)++;

		/* Return event queue element into event queue */
		if (stack->event_queue && stack->event_queue_item)
			linked_list_add(stack->event_queue, stack->event_queue_item);

		/* Finish access */
		mod_access_finish(mod, stack);

		/* Free the mod_client_info object, if any */
		if (stack->client_info)
			mod_client_info_free(mod, stack->client_info);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

/* NMOESI Protocol */

void mod_handler_nmoesi_load(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct cache_t *cache = mod->cache;

	if (event == EV_MOD_NMOESI_LOAD)
	{
		struct mod_stack_t *master_stack;

		mem_debug("%lld %lld 0x%x %s load\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"load\" "
			"state=\"%s:load\" addr=0x%x\n",
			stack->id, mod->name, stack->addr);

		/* Record access */
		mod_access_start(mod, stack, mod_access_load);

		/* Add access to stride detector and record if there is a stride */
		if (cache->prefetch.type)
			stack->stride = cache_detect_stride(mod->cache, stack->addr);

		/* Coalesce access */
		master_stack = mod_can_coalesce(mod, mod_access_load, stack->addr, stack);
		if (master_stack)
		{
			mod->reads++;
			mod_coalesce(mod, master_stack, stack);

			if (master_stack->access_kind == mod_access_prefetch)
			{

				/*Inform to main memory*/
				mem_controller_coalesce_pref_into_normal(stack);

				/* Statistics */
				mod->delayed_hits++;
				/* Prefetch stream stacks leave block in prefetch buffer, not in cache, so first load to coalesce must bring block from stream to cache.
				 * Next stacks must coalesce with the load, not with the prefetch. This is done in mod_coalesce(...). */
				if(cache->prefetch.type == prefetcher_type_czone_streams)
				{
					mod_stack_wait_in_stack(stack, master_stack, EV_MOD_NMOESI_LOAD_LOCK);
					return;
				}
			}

			/* Si la master stack te el bit de coalesced pero no master stack, vol dir que ha fet coalesce amb un stream prefetch, per tant açò és un delayed hit */
			if(master_stack->coalesced)
				mod->delayed_hits++;

			mod_stack_wait_in_stack(stack, master_stack, EV_MOD_NMOESI_LOAD_FINISH);
			return;
		}

		/* Next event */
		esim_schedule_event(EV_MOD_NMOESI_LOAD_LOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_LOCK)
	{
		struct mod_stack_t *older_stack;

		mem_debug("  %lld %lld 0x%x %s load lock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_lock\"\n",
			stack->id, mod->name);

		/* If there is any older write, wait for it */
		older_stack = mod_in_flight_write(mod, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for write %lld\n",
				stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_LOAD_LOCK);
			return;
		}

		/* If there is any older access to the same address that this access could not
		 * be coalesced with, wait for it. */
		older_stack = mod_in_flight_address(mod, stack->addr, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for access %lld\n",
				stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_LOAD_LOCK);
			return;
		}

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr,
			EV_MOD_NMOESI_LOAD_ACTION, stack, stack->prefetch);
		new_stack->blocking = 1;
		new_stack->read = 1;
		new_stack->retry = stack->retry;
		new_stack->stream_retried = stack->stream_retried;
		new_stack->stream_retried_cycle = stack->stream_retried_cycle;
		new_stack->background = stack->background;
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_ACTION)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s load action\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_action\"\n",
			stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			mod->read_retries++;
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry |= 1 << mod->level;
			esim_schedule_event(EV_MOD_NMOESI_LOAD_LOCK, stack, retry_lat);
			return;
		}

		/* Enqueue prefetches */
		if (must_enqueue_prefetch(stack))
		{
			/* Enqueue STREAM prefetch */
			if (cache->prefetch.type == prefetcher_type_czone_streams)
			{
				if (stack->stream_hit)
				{
					/* Prefetch only one block */
					if (stack->stream_head_hit)
					{
						assert(stack->pref_stream >= 0 && stack->pref_stream < cache->prefetch.num_streams);
						assert(stack->pref_slot >= 0 && stack->pref_slot < cache->prefetch.aggressivity);
						enqueue_prefetch_on_hit(stack);
					}
				}
				/* Fill all the stream buffer if a stride is detected */
				else
				{
					if (stack->stride)
						enqueue_prefetch_on_miss(stack);
				}
			}
		}

		/* Hit in write buffer */
		if (stack->wb_hit)
		{
			esim_schedule_event(EV_MOD_NMOESI_LOAD_FINISH, stack, 0);
			return;
		}

		/* Hit in cache */
		if (stack->state)
		{
			esim_schedule_event(EV_MOD_NMOESI_LOAD_UNLOCK, stack, 0);

			/* The prefetcher may have prefetched this earlier and hence
			 * this is a hit now. Let the prefetcher know of this hit
			 * since without the prefetcher, this may have been a miss. */
			prefetcher_access_hit(stack, mod);
			return;
		}

		/* Hit in stream */
		if (stack->stream_hit)
		{
			esim_schedule_event(EV_MOD_NMOESI_LOAD_MISS, stack, 0);
			return;
		}

		/* Background stack moving block from stream to cache */
		if (stack->background)
		{
			esim_schedule_event(EV_MOD_NMOESI_LOAD_MISS, stack, 0);
			return;
		}

		/* Miss */
		new_stack = mod_stack_create(stack->id, mod, stack->tag,
			EV_MOD_NMOESI_LOAD_MISS, stack, stack->prefetch);
		new_stack->peer = mod_stack_set_peer(mod, stack->state);
		new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
		new_stack->retry = stack->retry; //VVV
		new_stack->stream_retried = stack->stream_retried;
		new_stack->stream_retried_cycle = stack->stream_retried_cycle;
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);

		/* The prefetcher may be interested in this miss */
		prefetcher_access_miss(stack, mod);

		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_MISS)
	{
		int retry_lat;
		struct stream_buffer_t *sb;

		mem_debug("  %lld %lld 0x%x %s load miss\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_miss\"\n",
			stack->id, mod->name);

		/* Error on read request. Unlock block and retry load. */
		if (stack->err)
		{
			mod->read_retries++;
			retry_lat = mod_get_retry_latency(mod);
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry |= 1 << mod->level;
			esim_schedule_event(EV_MOD_NMOESI_LOAD_LOCK, stack, retry_lat);
			return;
		}

		/* If fast resumed because prefetch hit, there are some pending memory operations, so moving the block from prefetch buffer to cache and unlocking directories must wait for them to complete. The background stack created when fast resumed this access will take care of moving the blocks. */
		if (stack->fast_resume)
		{
			esim_schedule_event(EV_MOD_NMOESI_LOAD_FINISH, stack, 0);
			return;
		}
		else if (stack->background)
		{
			struct write_buffer_block_t *block;
			assert(linked_list_count(cache->wb.blocks));
			LINKED_LIST_FOR_EACH(cache->wb.blocks)
			{
				block = linked_list_get(cache->wb.blocks);
				if (block->tag == stack->tag)
				{
					cache_set_block(cache, stack->set, stack->way, stack->tag, block->state);
					mod_stack_wake_up_write_buffer(block);
					linked_list_remove(cache->wb.blocks);
					free(block);
					block = NULL;
					break;
				}
			}
			assert(!block);

			/* Statistics */
			mod->useful_prefetches++;
			if(stack->stream_retried_cycle)
			{
				int i;
				LIST_FOR_EACH(mem_system->mm_mod_list, i)
				{
					struct mod_t *mm_mod = list_get(mem_system->mm_mod_list, i);
					if (mod_serves_address(mm_mod, stack->addr) && esim_time - stack->stream_retried_cycle < mm_mod->latency / 3)
						mod->effective_useful_prefetches++;
				}
			}
			else
				mod->effective_useful_prefetches++;

		}
		else if (stack->stream_hit)
		{
			int tag;
			cache_get_pref_block_data(cache, stack->pref_stream, stack->pref_slot, &tag, &stack->state);
			assert(stack->tag == tag);
			assert(stack->state);

			/* Write block in cache */
			cache_set_block(cache, stack->set, stack->way, tag, stack->state);

			/* Free buffer entry */
			cache_set_pref_block(cache, stack->pref_stream, stack->pref_slot, -1, cache_block_invalid);
			sb = &cache->prefetch.streams[stack->pref_stream];
			sb->count--; //COUNT

			/* Statistics */
			mod->useful_prefetches++;
			if(stack->stream_retried_cycle)
			{
				int i;
				LIST_FOR_EACH(mem_system->mm_mod_list, i)
				{
					struct mod_t *mm_mod = list_get(mem_system->mm_mod_list, i);
					if (mod_serves_address(mm_mod, stack->addr) && esim_time - stack->stream_retried_cycle < mm_mod->latency / 3)
						mod->effective_useful_prefetches++;
				}
			}
			else
				mod->effective_useful_prefetches++;
		}
		else
		{
			/* Set block state to excl/shared depending on return var 'shared'.
			* Also set the tag of the block. */
			cache_set_block(cache, stack->set, stack->way, stack->tag, stack->shared ? cache_block_shared : cache_block_exclusive);
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_LOAD_UNLOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_UNLOCK)
	{
		mem_debug("  %lld %lld 0x%x %s load unlock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_unlock\"\n",
			stack->id, mod->name);

		/* Unlock directory entry */
		dir_entry_unlock(mod->dir, stack->set, stack->way);
		if (stack->stream_hit && !stack->background)
			dir_pref_entry_unlock(mod->dir, stack->pref_stream, stack->pref_slot);

		/* Impose the access latency before continuing */
		esim_schedule_event(EV_MOD_NMOESI_LOAD_FINISH, stack,
			mod->latency);
		return;
	}

	if (event == EV_MOD_NMOESI_LOAD_FINISH)
	{
		mem_debug("%lld %lld 0x%x %s load finish\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_finish\"\n",
			stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n",
			stack->id);

		/* Background stacks don't have to do this */
		if (!stack->background)
		{
			/* Increment witness variable */
			if (stack->witness_ptr)
				(*stack->witness_ptr)++;

			/* Increment head */
			if(stack->stream_hit && stack->stream_head_hit)
			{
				struct stream_buffer_t *sb = &cache->prefetch.streams[stack->pref_stream];
				sb->head = (stack->pref_slot + 1) % sb->num_slots; //HEAD
			}

			/* Return event queue element into event queue */
			if (stack->event_queue && stack->event_queue_item)
				linked_list_add(stack->event_queue, stack->event_queue_item);

			/* Free the mod_client_info object, if any */
			if (stack->client_info)
				mod_client_info_free(mod, stack->client_info);

			/* Finish access */
			mod_access_finish(mod, stack);
		}

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_store(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct cache_t *cache = mod->cache;


	if (event == EV_MOD_NMOESI_STORE)
	{
		struct mod_stack_t *master_stack;

		mem_debug("%lld %lld 0x%x %s store\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"store\" "
			"state=\"%s:store\" addr=0x%x\n",
			stack->id, mod->name, stack->addr);

		/* Record access */
		mod_access_start(mod, stack, mod_access_store);

		/* Add access to stride detector and record if there is a stride */
		if (mod->cache->prefetch.type)
			stack->stride = cache_detect_stride(mod->cache, stack->addr);

		/* Coalesce access */
		master_stack = mod_can_coalesce(mod, mod_access_store, stack->addr, stack);
		if (master_stack)
		{
			mod->writes++;
			mod_coalesce(mod, master_stack, stack);
			mod_stack_wait_in_stack(stack, master_stack, EV_MOD_NMOESI_STORE_FINISH);

			/* Increment witness variable */
			if (stack->witness_ptr)
				(*stack->witness_ptr)++;

			if (master_stack->access_kind == mod_access_prefetch)
				/*Inform to main memory*/
				mem_controller_coalesce_pref_into_normal(stack);

			return;
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_STORE_LOCK, stack, 0);
		return;
	}


	if (event == EV_MOD_NMOESI_STORE_LOCK)
	{
		struct mod_stack_t *older_stack;

		mem_debug("  %lld %lld 0x%x %s store lock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:store_lock\"\n",
			stack->id, mod->name);

		/* If there is any older access, wait for it */
		older_stack = stack->access_list_prev;
		if (older_stack)
		{
			mem_debug("    %lld wait for access %lld\n",
				stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_STORE_LOCK);
			return;
		}

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr,
			EV_MOD_NMOESI_STORE_ACTION, stack, stack->prefetch);
		new_stack->blocking = 1;
		new_stack->write = 1;
		new_stack->retry = stack->retry;
		new_stack->stream_retried = stack->stream_retried;
		new_stack->stream_retried_cycle = stack->stream_retried_cycle;
		new_stack->witness_ptr = stack->witness_ptr;
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK, new_stack, 0);

		/* Set witness variable to NULL so that retries from the same
		 * stack do not increment it multiple times */
		stack->witness_ptr = NULL;

		return;
	}

	if (event == EV_MOD_NMOESI_STORE_ACTION)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s store action\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:store_action\"\n",
			stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			mod->write_retries++;
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry |= 1 << mod->level;
			esim_schedule_event(EV_MOD_NMOESI_STORE_LOCK, stack, retry_lat);
			return;
		}

		/* Enqueue prefetches */
		if (must_enqueue_prefetch(stack))
		{
			/* Enqueue STREAM prefetch */
			if (cache->prefetch.type == prefetcher_type_czone_streams)
			{
				if (stack->stream_hit)
				{
					/* Prefetch only one block */
					if (stack->stream_head_hit)
					{
						assert(stack->pref_stream >= 0 && stack->pref_stream < cache->prefetch.num_streams);
						assert(stack->pref_slot >= 0 && stack->pref_slot < cache->prefetch.aggressivity);
						enqueue_prefetch_on_hit(stack);
					}
				}
				/* Fill all the stream buffer if a stride is detected */
				else
				{
					if (stack->stride)
						enqueue_prefetch_on_miss(stack);
				}
			}
		}

		/* Set state if hit in stream */
		if (stack->stream_hit)
			cache_get_pref_block_data(mod->cache, stack->pref_stream, stack->pref_slot, NULL, &stack->state);


		/* Hit - state=M/E */
		if (stack->state == cache_block_modified ||
			stack->state == cache_block_exclusive)
		{
			esim_schedule_event(EV_MOD_NMOESI_STORE_UNLOCK, stack, 0);

			/* The prefetcher may have prefetched this earlier and hence
			 * this is a hit now. Let the prefetcher know of this hit
			 * since without the prefetcher, this may have been a miss. */
			prefetcher_access_hit(stack, mod);

			return;
		}

		/* Miss - state=O/S/I/N */
		new_stack = mod_stack_create(stack->id, mod, stack->tag,
			EV_MOD_NMOESI_STORE_UNLOCK, stack, stack->prefetch);
		new_stack->peer = mod_stack_set_peer(mod, stack->state);
		new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
		new_stack->retry = stack->err; //VVV
		new_stack->stream_retried = stack->stream_retried;
		new_stack->stream_retried_cycle = stack->stream_retried_cycle;
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST, new_stack, 0);
		new_stack->access_kind = mod_access_store; //PPP

		/* The prefetcher may be interested in this miss */
		prefetcher_access_miss(stack, mod);

		return;
	}

	if (event == EV_MOD_NMOESI_STORE_UNLOCK)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s store unlock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:store_unlock\"\n",
			stack->id, mod->name);

		/* Error in write request, unlock block and retry store. */
		if (stack->err)
		{
			mod->write_retries++;
			retry_lat = mod_get_retry_latency(mod);
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry |= 1 << mod->level;
			esim_schedule_event(EV_MOD_NMOESI_STORE_LOCK, stack, retry_lat);
			return;
		}

		/* Update tag/state and unlock */
		cache_set_block(mod->cache, stack->set, stack->way,
			stack->tag, cache_block_modified);
		dir_entry_unlock(mod->dir, stack->set, stack->way);

		/* Impose the access latency before continuing */
		esim_schedule_event(EV_MOD_NMOESI_STORE_FINISH, stack,
			mod->latency);
		return;
	}

	if (event == EV_MOD_NMOESI_STORE_FINISH)
	{
		mem_debug("%lld %lld 0x%x %s store finish\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:store_finish\"\n",
			stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n",
			stack->id);

		/* Return event queue element into event queue */
		if (stack->event_queue && stack->event_queue_item)
			linked_list_add(stack->event_queue, stack->event_queue_item);

		/* Free the mod_client_info object, if any */
		if (stack->client_info)
			mod_client_info_free(mod, stack->client_info);

		/* Finish access */
		mod_access_finish(mod, stack);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_nc_store(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;


	if (event == EV_MOD_NMOESI_NC_STORE)
	{
		struct mod_stack_t *master_stack;

		mem_debug("%lld %lld 0x%x %s nc store\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"nc_store\" "
			"state=\"%s:nc store\" addr=0x%x\n", stack->id, mod->name, stack->addr);

		/* Record access */
		mod_access_start(mod, stack, mod_access_nc_store);

		/* Coalesce access */
		master_stack = mod_can_coalesce(mod, mod_access_nc_store, stack->addr, stack);
		if (master_stack)
		{
			mod->nc_writes++;
			mod_coalesce(mod, master_stack, stack);
			mod_stack_wait_in_stack(stack, master_stack, EV_MOD_NMOESI_NC_STORE_FINISH);
			return;
		}

		/* Next event */
		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_LOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_LOCK)
	{
		struct mod_stack_t *older_stack;

		mem_debug("  %lld %lld 0x%x %s nc store lock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_lock\"\n",
			stack->id, mod->name);

		/* If there is any older write, wait for it */
		older_stack = mod_in_flight_write(mod, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for write %lld\n", stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_NC_STORE_LOCK);
			return;
		}

		/* If there is any older access to the same address that this access could not
		 * be coalesced with, wait for it. */
		older_stack = mod_in_flight_address(mod, stack->addr, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for write %lld\n", stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_NC_STORE_LOCK);
			return;
		}

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr,
			EV_MOD_NMOESI_NC_STORE_WRITEBACK, stack, stack->prefetch);
		new_stack->blocking = 1;
		new_stack->nc_write = 1;
		new_stack->retry = stack->retry;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_WRITEBACK)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s nc store writeback\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_writeback\"\n",
			stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			mod->nc_write_retries++;
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_LOCK, stack, retry_lat);
			return;
		}

		/* If the block has modified data, evict it so that the lower-level cache will
		 * have the latest copy */
		if (stack->state == cache_block_modified || stack->state == cache_block_owned)
		{
			stack->eviction = 1;
			new_stack = mod_stack_create(stack->id, mod, 0,
				EV_MOD_NMOESI_NC_STORE_ACTION, stack, stack->prefetch);
			new_stack->set = stack->set;
			new_stack->way = stack->way;
			esim_schedule_event(EV_MOD_NMOESI_EVICT, new_stack, 0);
			return;
		}

		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_ACTION, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_ACTION)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s nc store action\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_action\"\n",
			stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			mod->nc_write_retries++;
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_LOCK, stack, retry_lat);
			return;
		}

		/* Main memory modules are a special case */
		if (mod->kind == mod_kind_main_memory)
		{
			/* For non-coherent stores, finding an E or M for the state of
			 * a cache block in the directory still requires a message to
			 * the lower-level module so it can update its owner field.
			 * These messages should not be sent if the module is a main
			 * memory module. */
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_UNLOCK, stack, 0);
			return;
		}

		/* N/S are hit */
		if (stack->state == cache_block_shared || stack->state == cache_block_noncoherent)
		{
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_UNLOCK, stack, 0);
		}
		/* E state must tell the lower-level module to remove this module as an owner */
		else if (stack->state == cache_block_exclusive)
		{
			new_stack = mod_stack_create(stack->id, mod, stack->tag,
				EV_MOD_NMOESI_NC_STORE_MISS, stack, stack->prefetch);
			new_stack->message = message_clear_owner;
			new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
			esim_schedule_event(EV_MOD_NMOESI_MESSAGE, new_stack, 0);
		}
		/* Modified and Owned states need to call read request because we've already
		 * evicted the block so that the lower-level cache will have the latest value
		 * before it becomes non-coherent */
		else
		{
			new_stack = mod_stack_create(stack->id, mod, stack->tag,
				EV_MOD_NMOESI_NC_STORE_MISS, stack, stack->prefetch);
			new_stack->peer = mod_stack_set_peer(mod, stack->state);
			new_stack->nc_write = 1;
			new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
			new_stack->request_dir = mod_request_up_down;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);
		}

		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_MISS)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s nc store miss\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_miss\"\n",
			stack->id, mod->name);

		/* Error on read request. Unlock block and retry nc store. */
		if (stack->err)
		{
			mod->nc_write_retries++;
			retry_lat = mod_get_retry_latency(mod);
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_NC_STORE_LOCK, stack, retry_lat);
			return;
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_UNLOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_UNLOCK)
	{
		mem_debug("  %lld %lld 0x%x %s nc store unlock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_unlock\"\n",
			stack->id, mod->name);

		/* Set block state to excl/shared depending on return var 'shared'.
		 * Also set the tag of the block. */
		cache_set_block(mod->cache, stack->set, stack->way, stack->tag,
			cache_block_noncoherent);

		/* Unlock directory entry */
		dir_entry_unlock(mod->dir, stack->set, stack->way);

		/* Impose the access latency before continuing */
		esim_schedule_event(EV_MOD_NMOESI_NC_STORE_FINISH, stack,
			mod->latency);
		return;
	}

	if (event == EV_MOD_NMOESI_NC_STORE_FINISH)
	{
		mem_debug("%lld %lld 0x%x %s nc store finish\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:nc_store_finish\"\n",
			stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n",
			stack->id);

		/* Increment witness variable */
		if (stack->witness_ptr)
			(*stack->witness_ptr)++;

		/* Return event queue element into event queue */
		if (stack->event_queue && stack->event_queue_item)
			linked_list_add(stack->event_queue, stack->event_queue_item);

		/* Free the mod_client_info object, if any */
		if (stack->client_info)
			mod_client_info_free(mod, stack->client_info);

		/* Finish access */
		mod_access_finish(mod, stack);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_pref_find_and_lock(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	//struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct cache_t *cache = mod->cache;
	struct stream_buffer_t *sb;

	assert(stack->request_dir);
	assert(stack->access_kind);

	if (event == EV_MOD_NMOESI_PREF_FIND_AND_LOCK)
	{
		mem_debug("  %lld %lld 0x%x %s pref find and lock (blocking=%d)\n", esim_time, stack->id, stack->addr, mod->name, stack->blocking);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_find_and_lock\"\n", stack->id, mod->name);

		/* Look for block in write buffer */
		struct write_buffer_block_t *block;
		stack->tag = stack->addr & ~cache->block_mask;
		LINKED_LIST_FOR_EACH(cache->wb.blocks)
		{
			block = linked_list_get(cache->wb.blocks);
			assert(block->state == cache_block_exclusive || block->state == cache_block_shared);
			if (block->tag == stack->tag)
			{
				stack->state = block->state;
				mem_debug("    %lld 0x%x %s write buffer hit: pos=%d, state=%s\n",
					stack->id, stack->tag, mod->name, linked_list_current(cache->wb.blocks),
					str_map_value(&cache_block_state_map, stack->state));
				stack->hit = 1;
				stack->wb_hit = 1;
				mod->write_buffer_prefetch_hits++; /* Statistics */
			}
		}

		/* Default return values */
		ret->err = 0;

		/* Get a port */
		mod_lock_port(mod, stack, EV_MOD_NMOESI_PREF_FIND_AND_LOCK_PORT);
		return;
	}

	if (event == EV_MOD_NMOESI_PREF_FIND_AND_LOCK_PORT)
	{
		struct mod_port_t *port = stack->port;
		struct dir_lock_t *dir_lock;
		struct stream_block_t *sblock;
		int slot;

		assert(stack->port);
		mem_debug("  %lld %lld 0x%x %s pref find and lock port\n", esim_time, stack->id, stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_find_and_lock_port\"\n", stack->id, mod->name);

		/* Set parent stack flag expressing that port has already been locked.
		 * This flag is checked by new writes to find out if it is already too
		 * late to coalesce. */
		ret->port_locked = 1;

		/* Search block in cache and stream */
		if (stack->access_kind != mod_access_invalidate_slot && !stack->wb_hit)
		{
			sb = &cache->prefetch.streams[stack->pref_stream];

			/* Search in cache */
			stack->hit = mod_find_block(mod, stack->addr, &stack->set, &stack->way, &stack->tag, &stack->state);

			/* Block in cache */
			if (stack->hit)
				mem_debug("    %lld 0x%x %s hit: set=%d, way=%d, state=%s\n", stack->id, stack->tag, mod->name, stack->set, stack->way, str_map_value(&cache_block_state_map, stack->state));

			/* Block is not in cache */
			else
			{
				/* Block may be in this stream */
				if (sb->stream_tag == sb->stream_transcient_tag)
				{
					slot = mod_find_block_in_stream(mod, stack->addr, stack->pref_stream);

					/* Block found */
					if (slot > -1)
					{
						/* Block already in the correct slot. Do nothing. */
						if (slot == stack->pref_slot)
						{
							ret->stream_hit = 1;
							ret->src_pref_slot = stack->pref_slot;
							ret->src_pref_stream = stack->pref_stream;
							mod_unlock_port(mod, port, stack);
							mod_stack_return(stack);
							return;
						}

						/* Block is not in the correct slot */ //TODO: Posible condició de carrera
						else
						{
							/* Lock it if is unlocked */
							dir_lock = dir_pref_lock_get(mod->dir, sb->stream, slot);
							if (!dir_lock->lock)
							{
								dir_pref_entry_lock(mod->dir, sb->stream, slot, EV_MOD_NMOESI_PREF_FIND_AND_LOCK, stack);
								sblock = cache_get_pref_block(cache, sb->stream, slot);
								sblock->transient_tag = -1;
								ret->stream_hit = 1;
								ret->src_pref_slot = slot;
								ret->src_pref_stream = stack->pref_stream;
							}
						}
					}
				}
			}
		}

		/* Assertions */
		sb = &cache->prefetch.streams[stack->pref_stream];
		assert(stack->pref_slot >= 0 && stack->pref_slot < sb->num_slots);
		assert(stack->pref_stream >= 0 && stack->pref_stream < cache->prefetch.num_streams);

		/* Lock prefetch entry */
		dir_lock = dir_pref_lock_get(mod->dir, stack->pref_stream, stack->pref_slot);
		if (dir_lock->lock && !stack->blocking)
		{
			struct stream_block_t * block = cache_get_pref_block(cache, stack->pref_stream, stack->pref_slot);
			mem_debug("    %lld 0x%x %s pref_stream=%d pref_slot=%d containing 0x%x (0x%x) already locked by stack %lld, retrying...\n", stack->id, stack->tag, mod->name, stack->pref_stream, stack->pref_slot, block->tag, block->transient_tag, dir_lock->stack_id);
			ret->err = 1;
			mod_unlock_port(mod, port, stack);
			mod_stack_return(stack);
			return;
		}
		if (!dir_pref_entry_lock(mod->dir, stack->pref_stream, stack->pref_slot, EV_MOD_NMOESI_PREF_FIND_AND_LOCK, stack))
		{
			mem_debug("    %lld 0x%x %s pref_stream=%d pref_slot=%d already locked by stack %lld, waiting...\n", stack->id, stack->tag, mod->name, stack->pref_stream, stack->pref_slot, dir_lock->stack_id);
			mod_unlock_port(mod, port, stack);
			return;
		}

		/* Entry is locked. Record the transient tag so that a subsequent lookup
		 * detects that the block is being brought. */
		struct stream_block_t *block = cache_get_pref_block(cache, stack->pref_stream, stack->pref_slot);
		block->transient_tag = !stack->hit && stack->access_kind != mod_access_invalidate_slot ?
			stack->tag : -1; /* If cache hit, block will be removed */

		mem_debug("    %lld 0x%x %s stream=%d, slot=%d, state=%s\n", stack->id, stack->tag, mod->name, stack->pref_stream, stack->pref_slot, str_map_value(&cache_block_state_map, block->state));

		/* Update count */
		if (stack->access_kind != mod_access_invalidate_slot)
		{
			assert(sb->stream == stack->pref_stream);
			if (!block->state && !stack->hit)
				sb->count++; //COUNT
		}

		/* Update LRU stream */
		cache_access_stream(mod->cache, stack->pref_stream);

		/* Access latency */
		esim_schedule_event(EV_MOD_NMOESI_PREF_FIND_AND_LOCK_ACTION, stack, mod->latency);
		return;
	}

	if (event == EV_MOD_NMOESI_PREF_FIND_AND_LOCK_ACTION)
	{
		struct mod_port_t *port = stack->port;

		assert(port);
		mem_debug("  %lld %lld 0x%x %s pref find and lock action\n", esim_time, stack->id, stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_find_and_lock_action\"\n", stack->id, mod->name);

		/* Release port */
		mod_unlock_port(mod, port, stack);

		/* Prefetch buffer slot eviction */
		struct stream_block_t *block = cache_get_pref_block(cache, stack->pref_stream, stack->pref_slot);
		if (block->state)
		{
			/* assert(stack->request_dir == mod_request_up_down);
			stack->pref_eviction = 1;
			new_stack = mod_stack_create(stack->id, mod, 0, EV_MOD_NMOESI_PREF_FIND_AND_LOCK_FINISH, stack, stack->prefetch);
			new_stack->pref_stream = stack->pref_stream;
			new_stack->pref_slot = stack->pref_slot;
			new_stack->access_kind = stack->access_kind;
			esim_schedule_event(EV_MOD_NMOESI_PREF_EVICT, new_stack, 0);
			return; */

			/* Silent eviction */
			block->state = cache_block_invalid;
			stack->pref_eviction = 1;
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_PREF_FIND_AND_LOCK_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_PREF_FIND_AND_LOCK_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s pref find and lock finish (err=%d)\n", esim_time, stack->id, stack->tag, mod->name, stack->err);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:pref_find_and_lock_finish\"\n", stack->id, mod->name);

		/* If evict produced err, return err */ //TODO: Deprecated
		if (stack->err)
		{
			assert(stack->pref_eviction);
			cache_get_pref_block_data(mod->cache, stack->pref_stream, stack->pref_slot, NULL, &stack->state);
			assert(stack->state);
			ret->err = 1;
			dir_pref_entry_unlock(mod->dir, stack->pref_stream, stack->pref_slot);
			mod_stack_return(stack);
			return;
		}

		/* Eviction */
		if (stack->pref_eviction)
		{
			mod->stream_evictions++;
			/* Block is removed, not replaced */
			if (stack->hit || stack->access_kind == mod_access_invalidate_slot)
				mod->cache->prefetch.streams[stack->src_pref_stream].count--; //COUNT
			cache_get_pref_block_data(mod->cache, stack->pref_stream, stack->pref_slot, NULL, &stack->state);
			assert(!stack->state);
		}

		/* Return */
		ret->err = 0;
		ret->state = stack->state;
		ret->tag = stack->tag;
		ret->hit = stack->hit;
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_prefetch(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;


	if (event == EV_MOD_NMOESI_PREFETCH)
	{
		struct mod_stack_t *master_stack;

		mem_debug("%lld %lld 0x%x %s prefetch\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"store\" "
			"state=\"%s:prefetch\" addr=0x%x\n",
			stack->id, mod->name, stack->addr);

		/* Record access */
		mod_access_start(mod, stack, mod_access_prefetch);

		/* Coalesce access */
		master_stack = mod_can_coalesce(mod, mod_access_prefetch, stack->addr, stack);
		if (master_stack)
		{
			/* Doesn't make sense to prefetch as the block is already being fetched */
			mem_debug("  %lld %lld 0x%x %s useless prefetch - already being fetched\n",
				  esim_time, stack->id, stack->addr, mod->name);

			mod->useless_prefetches++;
			esim_schedule_event(EV_MOD_NMOESI_PREFETCH_FINISH, stack, 0);

			/* Increment witness variable */
			if (stack->witness_ptr)
				(*stack->witness_ptr)++;

			return;
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_PREFETCH_LOCK, stack, 0);
		return;
	}


	if (event == EV_MOD_NMOESI_PREFETCH_LOCK)
	{
		struct mod_stack_t *older_stack;

		mem_debug("  %lld %lld 0x%x %s prefetch lock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:prefetch_lock\"\n",
			stack->id, mod->name);

		/* If there is any older write, wait for it */
		older_stack = mod_in_flight_write(mod, stack);
		if (older_stack)
		{
			mem_debug("    %lld wait for write %lld\n",
				stack->id, older_stack->id);
			mod_stack_wait_in_stack(stack, older_stack, EV_MOD_NMOESI_PREFETCH_LOCK);
			return;
		}

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr,
			EV_MOD_NMOESI_PREFETCH_ACTION, stack, stack->prefetch);
		new_stack->blocking = 0;
		new_stack->prefetch = 1;
		new_stack->retry = 0;
		new_stack->witness_ptr = stack->witness_ptr;
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK, new_stack, 0);

		/* Set witness variable to NULL so that retries from the same
		 * stack do not increment it multiple times */
		stack->witness_ptr = NULL;

		return;
	}

	if (event == EV_MOD_NMOESI_PREFETCH_ACTION)
	{
		mem_debug("  %lld %lld 0x%x %s prefetch action\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:prefetch_action\"\n",
			stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			/* Don't want to ever retry prefetches if getting a lock failed.
			Effectively this means that prefetches are of low priority.
			This can be improved to not retry only when the current lock
			holder is writing to the block. */
			mod->prefetch_aborts++;
			mem_debug("    lock error, aborting prefetch\n");
			esim_schedule_event(EV_MOD_NMOESI_PREFETCH_FINISH, stack, 0);
			return;
		}

		/* Hit */
		if (stack->state)
		{
			/* Block already in cache */
			mem_debug("  %lld %lld 0x%x %s useless prefetch - cache hit\n",
				  esim_time, stack->id, stack->addr, mod->name);

			mod->useless_prefetches++;
			esim_schedule_event(EV_MOD_NMOESI_PREFETCH_UNLOCK, stack, 0);
			return;
		}

		/* Miss */
		new_stack = mod_stack_create(stack->id, mod, stack->tag,
			EV_MOD_NMOESI_PREFETCH_MISS, stack, stack->prefetch);
		new_stack->peer = mod_stack_set_peer(mod, stack->state);
		new_stack->target_mod = mod_get_low_mod(mod, stack->tag);
		new_stack->request_dir = mod_request_up_down;
		new_stack->prefetch = 1;
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);
		return;
	}
	if (event == EV_MOD_NMOESI_PREFETCH_MISS)
	{
		mem_debug("  %lld %lld 0x%x %s prefetch miss\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:prefetch_miss\"\n",
			stack->id, mod->name);

		/* Error on read request. Unlock block and abort. */
		if (stack->err)
		{
			/* Don't want to ever retry prefetches if read request failed.
			 * Effectively this means that prefetches are of low priority.
			 * This can be improved depending on the reason for read request fail */
			mod->prefetch_aborts++;
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mem_debug("    lock error, aborting prefetch\n");
			esim_schedule_event(EV_MOD_NMOESI_PREFETCH_FINISH, stack, 0);
			return;
		}

		/* Set block state to excl/shared depending on return var 'shared'.
		 * Also set the tag of the block. */
		cache_set_block(mod->cache, stack->set, stack->way, stack->tag,
			stack->shared ? cache_block_shared : cache_block_exclusive);

		/* Mark the prefetched block as prefetched. This is needed to let the
		 * prefetcher know about an actual access to this block so that it
		 * is aware of all misses as they would be without the prefetcher.
		 * TODO: The lower caches that will be filled because of this prefetch
		 * do not know if it was a prefetch or not. Need to have a way to mark
		 * them as prefetched too. */
		mod_block_set_prefetched(mod, stack->addr, 1);

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_PREFETCH_UNLOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_PREFETCH_UNLOCK)
	{
		mem_debug("  %lld %lld 0x%x %s prefetch unlock\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:prefetch_unlock\"\n",
			stack->id, mod->name);

		/* Unlock directory entry */
		dir_entry_unlock(mod->dir, stack->set, stack->way);

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_PREFETCH_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_PREFETCH_FINISH)
	{
		mem_debug("%lld %lld 0x%x %s prefetch\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:prefetch_finish\"\n",
			stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n",
			stack->id);

		/* Increment witness variable */
		if (stack->witness_ptr)
			(*stack->witness_ptr)++;

		/* Return event queue element into event queue */
		if (stack->event_queue && stack->event_queue_item)
			linked_list_add(stack->event_queue, stack->event_queue_item);

		/* Free the mod_client_info object, if any */
		if (stack->client_info)
			mod_client_info_free(mod, stack->client_info);

		/* Finish access */
		mod_access_finish(mod, stack);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_find_and_lock(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;
	struct mod_t *mod = stack->mod;
	struct cache_t *cache = mod->cache;

	assert(stack->request_dir);

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK)
	{
		mem_debug("  %lld %lld 0x%x %s find and lock (blocking=%d)\n",
			esim_time, stack->id, stack->addr, mod->name, stack->blocking);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock\"\n",
			stack->id, mod->name);

		/* Default return values */
		ret->err = 0;

		/* If block is in write buffer and request dir is up down, retry. Else, wait. */
		if (!stack->background)
		{
			assert(mod->kind!=mod_kind_main_memory && stack->request_dir != mod_request_up_down); // P

			struct write_buffer_block_t *block;
			stack->tag = stack->addr & ~cache->block_mask;
			LINKED_LIST_FOR_EACH(cache->wb.blocks)
			{
				block = linked_list_get(cache->wb.blocks);
				assert(block->state == cache_block_exclusive || block->state == cache_block_shared);
				if (block->tag == stack->tag)
				{
					stack->state = block->state;
					if (stack->read)
					{
						mem_debug("    %lld 0x%x %s write buffer hit: pos=%d, state=%s\n",
							stack->id, stack->tag, mod->name, linked_list_current(cache->wb.blocks),
							str_map_value(&cache_block_state_map, stack->state));
						mod->write_buffer_read_hits++; /* Statistics */
					}
					else if (stack->write)
					{
						mem_debug("    %lld 0x%x %s write buffer hit: pos=%d, state=%s\n",
							stack->id, stack->tag, mod->name, linked_list_current(cache->wb.blocks),
							str_map_value(&cache_block_state_map, stack->state));
						mod->write_buffer_write_hits++; /* Statistics */
					}
					else
						fatal("Unknown memory operation type");

					if(stack->request_dir == mod_request_up_down)
					{
						mem_debug("    %lld retry in wb due to stack %lld\n", stack->id, block->stack_id);
						ret->err = 1;
						mod_stack_return(stack);
					}
					else
					{
						mem_debug("    %lld wait in wb for stack %lld\n",
							stack->id, block->stack_id);
						mod_stack_wait_in_write_buffer(stack, block, EV_MOD_NMOESI_FIND_AND_LOCK);

					}

					return;
				}
			}
		}

		/* If this stack has already been assigned a way, keep using it */
		stack->way = ret->way;

		/* Get a port */
		mod_lock_port(mod, stack, EV_MOD_NMOESI_FIND_AND_LOCK_PORT);

		stack->hit = stack->stream_hit = 0;
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_PORT)
	{
		struct mod_port_t *port = stack->port;
		struct dir_lock_t *dir_lock;

		assert(stack->port);
		mem_debug("  %lld %lld 0x%x %s find and lock port\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_port\"\n",
			stack->id, mod->name);

		/* Set parent stack flag expressing that port has already been locked.
		 * This flag is checked by new writes to find out if it is already too
		 * late to coalesce. */
		ret->port_locked = 1;

		/* Look for block. */
		stack->hit = mod_find_block(mod, stack->addr, &stack->set,
			&stack->way, &stack->tag, &stack->state);

		/* Debug */
		if (stack->hit)
			mem_debug("    %lld 0x%x %s hit: set=%d, way=%d, state=%s\n", stack->id,
				stack->tag, mod->name, stack->set, stack->way,
				str_map_value(&cache_block_state_map, stack->state));

		/* If there is a background stack, no other stack can be retrieving the same block */
		assert(!(stack->background && stack->hit));

		/* Statistics */
		mod->accesses++;
		if (stack->hit)
			mod->hits++;

		if (stack->read)
		{
			mod->reads++;
			mod->effective_reads++;
			stack->blocking ? mod->blocking_reads++ : mod->non_blocking_reads++;
			if (stack->hit)
				mod->read_hits++;
		}
		else if (stack->prefetch)
		{
			mod->prefetches++;
		}
		else if (stack->nc_write)  /* Must go after read */
		{
			mod->nc_writes++;
			mod->effective_nc_writes++;
			stack->blocking ? mod->blocking_nc_writes++ : mod->non_blocking_nc_writes++;
			if (stack->hit)
				mod->nc_write_hits++;
		}
		else if (stack->write)
		{
			mod->writes++;
			mod->effective_writes++;
			stack->blocking ? mod->blocking_writes++ : mod->non_blocking_writes++;

			/* Increment witness variable when port is locked */
			if (stack->witness_ptr)
			{
				(*stack->witness_ptr)++;
				stack->witness_ptr = NULL;
			}

			if (stack->hit)
				mod->write_hits++;
		}
		else if (stack->message)
		{
			/* FIXME */
		}
		else
		{
			fatal("Unknown memory operation type");
		}

		if (!(stack->retry & (1 << mod->level)))
		{
			mod->no_retry_accesses++;
			if (stack->hit)
				mod->no_retry_hits++;

			if (stack->read)
			{
				mod->no_retry_reads++;
				if (stack->hit)
					mod->no_retry_read_hits++;
			}
			else if (stack->nc_write)  /* Must go after read */
			{
				mod->no_retry_nc_writes++;
				if (stack->hit)
					mod->no_retry_nc_write_hits++;
			}
			else if (stack->write)
			{
				mod->no_retry_writes++;
				if (stack->hit)
					mod->no_retry_write_hits++;
			}
			else if (stack->prefetch)
			{
				/* No retries currently for prefetches */
			}
			else if (stack->message)
			{
				/* FIXME */
			}
			else
			{
				fatal("Unknown memory operation type");
			}
		}

		if (!stack->hit)
		{
			/* Find victim */
			if (stack->way < 0)
				stack->way = cache_replace_block(mod->cache, stack->set);
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(stack->state || !dir_entry_group_shared_or_owned(mod->dir, stack->set, stack->way));
			mem_debug("    %lld 0x%x %s lru: set=%d, way=%d, state=%s\n", stack->id, stack->tag, mod->name, stack->set, stack->way, str_map_value(&cache_block_state_map, stack->state));
		}
		assert(stack->way >= 0);

		/* If cache hit or is a up_down request lock cache entry */
		if (stack->hit || stack->request_dir == mod_request_up_down )
		{
			/* If directory entry is locked and the call to FIND_AND_LOCK is not
		 	* blocking, release port and return error. */
			dir_lock = dir_lock_get(mod->dir, stack->set, stack->way);
			if (dir_lock->lock && !stack->blocking)
			{
				mem_debug("    %lld 0x%x %s block already locked at set=%d, way=%d by A-%lld - aborting\n",
					stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
				ret->err = 1;
				mod_unlock_port(mod, port, stack);
				ret->port_locked = 0;
				mod_stack_return(stack);
				return;
			}

			/* Lock directory entry. If lock fails, port needs to be released to prevent
			 * deadlock.  When the directory entry is released, locking port and
			 * directory entry will be retried. */
			if (!dir_entry_lock(mod->dir, stack->set, stack->way, EV_MOD_NMOESI_FIND_AND_LOCK,
				stack))
			{
				mem_debug("    %lld 0x%x %s block already locked at set=%d, way=%d by A-%lld - waiting\n",
					stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
				mod_unlock_port(mod, port, stack);
				ret->port_locked = 0;
				return;
			}

			/* This stack has been retried because the block it was looking for was locked in the stream and now has found the block in cache. Delayed hit statistics must be updated. */
			if(stack->hit && stack->stream_retried)
			{
				assert(!(mod->kind==mod_kind_main_memory && stack->request_dir == mod_request_up_down)); // P
				assert(stack->stream_retried_cycle);
				mod->delayed_hit_cycles += esim_time - stack->stream_retried_cycle;
				mod->delayed_hits_cycles_counted++;
				ret->stream_retried = 0;
			}

			/* Cache entry is locked. Record the transient tag so that a subsequent lookup
			* detects that the block is being brought.
			* Also, update LRU counters here. */
			cache_set_transient_tag(mod->cache, stack->set, stack->way, stack->tag);
			cache_access_block(mod->cache, stack->set, stack->way);
		}



		/* Access latency */
		if (!stack->hit && !stack->background && cache->prefetch.type == prefetcher_type_czone_streams)
		{
			assert(!(mod->kind==mod_kind_main_memory && stack->request_dir == mod_request_up_down)); // P
			esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_PREF_STREAM, stack, 0);
		}else
			esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_ACTION, stack, mod->dir_latency); /* Access latency */
		return;
	}


	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_PREF_STREAM)
	{
		int result;
		struct mod_port_t *port = stack->port;
		struct stream_block_t *block;
		struct stream_buffer_t *sb;
		struct dir_lock_t *dir_lock;

		mem_debug("  %lld %lld 0x%x %s find and lock pref stream\n", esim_time, stack->id, stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_pref_stream\"\n", stack->id, mod->name);

		/* Look for block in prefetch stream buffers */
		stack->stream_hit = 0;
		result = mod_find_pref_block(mod, stack->addr, &stack->pref_stream, &stack->pref_slot);
		if (result)
		{
			sb = &cache->prefetch.streams[stack->pref_stream];
			stack->stream_hit = 1;
			if (result == 1) /* {0 miss, 1 head hit, 2 middle stream hit} */
				stack->stream_head_hit = 1;
			mem_debug("    %lld 0x%x %s stream_hit=%d stream=%d slot=%d in head=%d up_down=%d\n", stack->id, stack->tag, mod->name, stack->stream_hit, stack->pref_stream, stack->pref_slot, stack->stream_head_hit, stack->request_dir == mod_request_up_down);

			/* Print debug info */
			int i, slot, count;
			count = sb->head + sb->num_slots;
			for (i = sb->head; i < count; i++)
			{
				slot = i % sb->num_slots;
				block = cache_get_pref_block(cache, sb->stream, slot);
				dir_lock = dir_pref_lock_get(mod->dir, sb->stream, slot);
				mem_debug("\t\t{slot=%d, tag=0x%x, transient_tag=0x%x, state=%s, locked=%d}\n", slot, block->tag, block->transient_tag, str_map_value(&cache_block_state_map, block->state), dir_lock->lock);
			}

			/* Statistics */
			mod->stream_hits++;
			mod->hits++;
			if (!(stack->retry & (1 << mod->level)))
			{
				mod->no_retry_stream_hits++;
				mod->no_retry_hits++;
			}

			if (stack->request_dir == mod_request_up_down)
			{
				if (stack->stream_head_hit)
					mod->up_down_head_hits++;
				mod->up_down_hits++;
			}
			else
			{
				if (stack->read)
					mod->down_up_read_hits++;
				else
					mod->down_up_write_hits++;
			}
		}

		/* If stream hit lock slot */
		if (stack->stream_hit)
		{
			block = cache_get_pref_block(cache, stack->pref_stream, stack->pref_slot);
			dir_lock = dir_pref_lock_get(mod->dir, stack->pref_stream, stack->pref_slot);
			if (dir_lock->lock && stack->request_dir == mod_request_up_down)
			{
				mem_debug("    %lld 0x%x %s pref_stream %d pref_slot %d containing 0x%x (0x%x) already locked by stack %lld, retrying...\n", stack->id, stack->tag, mod->name, stack->pref_stream, stack->pref_slot, block->tag, block->transient_tag, dir_lock->stack_id);
				if(!stack->stream_retried)
				{
					assert(!stack->stream_retried_cycle);
					if(dir_lock->prefetch_stack)
					{
						mod->delayed_hits++;
						ret->stream_retried_cycle = esim_time;
						ret->stream_retried = 1;

						/*Piggybacking*/
						if(!stack->prefetch)
							mem_controller_coalesce_pref_into_normal(stack);
					}
				}
				ret->err = 1;
				mod_unlock_port(mod, port, stack);
				ret->port_locked = 0;
				dir_entry_unlock(mod->dir, stack->set, stack->way); /* Unlock cache entry */
				mod_stack_return(stack);
				return;
			}
			if (!dir_pref_entry_lock(mod->dir, stack->pref_stream, stack->pref_slot, EV_MOD_NMOESI_FIND_AND_LOCK, stack))
			{
				mem_debug("    %lld 0x%x %s pref_stream %d pref_slot %d containing 0x%x (0x%x) already locked by stack %lld, waiting...\n", stack->id, stack->tag, mod->name, stack->pref_stream, stack->pref_slot, block->tag, block->transient_tag, dir_lock->stack_id);
				mod_unlock_port(mod, port, stack);
				ret->port_locked = 0;
				return;
			}

			if(stack->stream_retried)
			{
				assert(stack->stream_retried_cycle);
				mod->delayed_hit_cycles += esim_time - stack->stream_retried_cycle;
				mod->delayed_hits_cycles_counted++;
				ret->stream_retried = 0;
			}
		}

		/* Prefetch buffer entry is locked. Record 0xFFFFFFFF as transient tag so that a subsequent lookup
		 * detects that the block is being removed from buffer. TODO: Crec q no fa falta tocar la tag...
		 * Also, update LRU counters here. */
		if (stack->request_dir == mod_request_up_down && stack->stream_hit)
		{
			block = cache_get_pref_block(cache, stack->pref_stream, stack->pref_slot);
			block->transient_tag = -1;
			if (stack->stream_head_hit)
				cache_access_stream(cache, stack->pref_stream);
		}

		/* Access latency */
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_ACTION, stack, mod->dir_latency);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_ACTION)
	{
		struct mod_port_t *port = stack->port;

		assert(port);

		/* Because of silent replacement used in streams, there may be misses in down up requests */
		if(!stack->hit && !stack->stream_hit && stack->request_dir == mod_request_down_up)
		{
			mem_debug("  %lld %lld 0x%x %s down up miss due a silent eviction in a stream\n", esim_time, stack->id,
				stack->tag, mod->name);
			stack->state = cache_block_invalid;
			if(stack->read)
				mod->down_up_read_misses++;
			else if(stack->write)
				mod->down_up_write_misses++;
			else
				fatal("Unknown memory operation type");
		}

		/* No request can't be a hit and a prefetch hit at same time */
		assert(!stack->hit || !stack->stream_hit);

		mem_debug("  %lld %lld 0x%x %s find and lock action\n", esim_time, stack->id,
			stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_action\"\n",
			stack->id, mod->name);

		/* Release port */
		mod_unlock_port(mod, port, stack);
		ret->port_locked = 0;

		/* On miss, evict if victim is a valid block. */
		if (!stack->hit && stack->state && stack->request_dir == mod_request_up_down)
		{
			/* If stream hit, leave a background stack do the tough job and continue bringing the block to cpu */
			if (stack->stream_hit)
			{
				struct mod_stack_t *background_stack;
				struct mod_stack_t *find_and_lock_stack;
				struct mod_stack_t *eviction_stack;
				struct write_buffer_block_t *block;
				struct stream_buffer_t *sb;

				mem_debug("  %lld %lld 0x%x %s fast resume access\n", esim_time, stack->id, stack->tag, mod->name);

				assert(!(mod->level==1 && stack->write)); //TODO: Arreglar stores

				/* Stack that will write block from prefetch buffer to cache */
				background_stack = mod_stack_create(stack->id, ret->mod, stack->addr, ESIM_EV_NONE,
					NULL, stack->prefetch);
				background_stack->target_mod = ret->target_mod;
				background_stack->request_dir = ret->request_dir;
				background_stack->reply_size = ret->reply_size;
				background_stack->reply = ret->reply;

				/* Stack that will wait for eviction to complete */
				find_and_lock_stack = mod_stack_create(stack->id, mod, stack->addr,
					stack->ret_event, background_stack, stack->prefetch);
				find_and_lock_stack->blocking = stack->blocking; /* Not used */
				find_and_lock_stack->read = stack->read; /* Not used */
				find_and_lock_stack->write = stack->write; /* Not used */
				find_and_lock_stack->retry = stack->retry; /* Not used */
				find_and_lock_stack->set = stack->set;
				find_and_lock_stack->way = stack->way;
				find_and_lock_stack->state = stack->state;
				find_and_lock_stack->tag = stack->tag;
				find_and_lock_stack->pref_stream = -1; /* For this stack there hasn't been a prefetch hit */
				find_and_lock_stack->pref_slot = -1; /* " */
				find_and_lock_stack->stream_hit = 0; /* " */
				find_and_lock_stack->stream_head_hit = 0; /* " */
				find_and_lock_stack->request_dir = stack->request_dir;
				find_and_lock_stack->eviction = 1;
				find_and_lock_stack->background = 1;

				/* Eviction stack */
				eviction_stack = mod_stack_create(stack->id, mod, 0,
					EV_MOD_NMOESI_FIND_AND_LOCK_FINISH, find_and_lock_stack, stack->prefetch);
				eviction_stack->set = stack->set;
				eviction_stack->way = stack->way;

				/* Copy block from stream to cache write buffer */
				block = xcalloc(1, sizeof(struct write_buffer_block_t));
				cache_get_pref_block_data(cache, stack->pref_stream, stack->pref_slot, &block->tag, (int *) &block->state);
				block->stack_id = background_stack->id;
				linked_list_add(cache->wb.blocks, block);

				/* This stack will be fast resumed */
				stack->fast_resume = 1;
				stack->state = cache_block_invalid;

				/* Free stream entry */
				cache_set_pref_block(cache, stack->pref_stream, stack->pref_slot, -1, cache_block_invalid);
				sb = &cache->prefetch.streams[stack->pref_stream];
				sb->count--; //COUNT

				/* Update head */
				if (stack->stream_head_hit)
					sb->head = (sb->head + 1) % sb->num_slots; //HEAD

				/* Unlock stream entry */
				dir_pref_entry_unlock(mod->dir, stack->pref_stream, stack->pref_slot);

				/* Statistics */
				mod->fast_resumed_accesses++;

				esim_schedule_event(EV_MOD_NMOESI_EVICT, eviction_stack, 0);
			}
			else
			{
				stack->eviction = 1;
				new_stack = mod_stack_create(stack->id, mod, 0,
					EV_MOD_NMOESI_FIND_AND_LOCK_FINISH, stack, stack->prefetch);
				new_stack->set = stack->set;
				new_stack->way = stack->way;
				assert(!(mod->kind==mod_kind_main_memory && stack->request_dir == mod_request_up_down)); // It shouldn't happen because we want avoid evictions in mm directory
				esim_schedule_event(EV_MOD_NMOESI_EVICT, new_stack, 0);
				return;
			}
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s find and lock finish (err=%d fr=%d bg=%d)\n", esim_time, stack->id,
			stack->tag, mod->name, stack->err, stack->fast_resume, stack->background);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_finish\"\n",
			stack->id, mod->name);

		/* If evict produced err, return err */
		if (stack->err)
		{
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(stack->state);
			assert(stack->eviction);
			ret->err = 1;
			ret->background = stack->background;
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			if (stack->stream_hit)
			{
				assert(!(mod->kind==mod_kind_main_memory && stack->request_dir == mod_request_up_down)); // P
				dir_pref_entry_unlock(mod->dir, stack->pref_stream, stack->pref_slot);
			}
			mod_stack_return(stack);
			return;
		}

		/* Eviction */
		if (stack->eviction)
		{
			mod->evictions++;
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(!stack->state);
		}

		/* If this is a main memory, the block is here. A previous miss was just a miss
		 * in the directory. */
		if (mod->kind == mod_kind_main_memory && !stack->state)
		{
			stack->state = cache_block_exclusive;
			cache_set_block(mod->cache, stack->set, stack->way,
				stack->tag, stack->state);
		}

		/* Return */
		ret->err = 0;
		ret->set = stack->set;
		ret->way = stack->way;
		ret->state = stack->state;
		ret->tag = stack->tag;
		ret->stream_hit = stack->stream_hit;
		ret->stream_head_hit = stack->stream_head_hit;
		ret->pref_stream = stack->pref_stream;
		ret->pref_slot = stack->pref_slot;
		ret->fast_resume = stack->fast_resume;
		ret->background = stack->background;
		ret->wb_hit = stack->wb_hit;
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_evict(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct mod_t *target_mod = stack->target_mod;
	struct mem_controller_t * mem_controller;

	struct dir_t *dir;
	struct dir_entry_t *dir_entry;

	uint32_t dir_entry_tag, z;


	if (event == EV_MOD_NMOESI_EVICT)
	{
		/* Default return value */
		ret->err = 0;

		/* Get block info */
		cache_get_block(mod->cache, stack->set, stack->way, &stack->tag, &stack->state);
		assert(stack->state || !dir_entry_group_shared_or_owned(mod->dir,
			stack->set, stack->way));
		mem_debug("  %lld %lld 0x%x %s evict (set=%d, way=%d, state=%s)\n", esim_time, stack->id,
			stack->tag, mod->name, stack->set, stack->way,
			str_map_value(&cache_block_state_map, stack->state));
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict\"\n",
			stack->id, mod->name);

		/* Save some data */
		stack->src_set = stack->set;
		stack->src_way = stack->way;
		stack->src_tag = stack->tag;
		stack->target_mod = mod_get_low_mod(mod, stack->tag);

		/* Send write request to all sharers */
		new_stack = mod_stack_create(stack->id, mod, 0, EV_MOD_NMOESI_EVICT_INVALID, stack, stack->prefetch);
		new_stack->except_mod = NULL;
		new_stack->set = stack->set;
		new_stack->way = stack->way;
		esim_schedule_event(EV_MOD_NMOESI_INVALIDATE, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_EVICT_INVALID)
	{
		mem_debug("  %lld %lld 0x%x %s evict invalid\n", esim_time, stack->id,
			stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict_invalid\"\n",
			stack->id, mod->name);

		/* If module is main memory, we just need to set the block as invalid,
		 * and finish. */
		if (mod->kind == mod_kind_main_memory)
		{
			cache_set_block(mod->cache, stack->src_set, stack->src_way,
				0, cache_block_invalid);
			esim_schedule_event(EV_MOD_NMOESI_EVICT_FINISH, stack, 0);
			return;
		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_EVICT_ACTION, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_EVICT_ACTION)
	{
		struct mod_t *low_mod;
		struct net_node_t *low_node;
		int msg_size;

		mem_debug("  %lld %lld 0x%x %s evict action\n", esim_time, stack->id,
			stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict_action\"\n",
			stack->id, mod->name);

		/* Get low node */
		low_mod = stack->target_mod;
		low_node = low_mod->high_net_node;
		assert(low_mod != mod);
		assert(low_mod == mod_get_low_mod(mod, stack->tag));
		assert(low_node && low_node->user_data == low_mod);

		/* Update the cache state since it may have changed after its
		 * higher-level modules were invalidated */
		cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);

		/* State = I */
		if (stack->state == cache_block_invalid)
		{
			esim_schedule_event(EV_MOD_NMOESI_EVICT_FINISH, stack, 0);
			return;
		}

		/* If state is M/O/N, data must be sent to lower level mod */
		if (stack->state == cache_block_modified || stack->state == cache_block_owned ||
			stack->state == cache_block_noncoherent)
		{
			/* Need to transmit data to low module */
			msg_size = 8 + mod->block_size;
			stack->reply = reply_ack_data;
		}
		/* If state is E/S, just an ack needs to be sent */
		else
		{
			msg_size = 8;
			stack->reply = reply_ack;
		}

		/* Send message */
		stack->msg = net_try_send_ev(mod->low_net, mod->low_net_node,
			low_node, msg_size, EV_MOD_NMOESI_EVICT_RECEIVE, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_EVICT_RECEIVE)
	{
		mem_controller = target_mod->mem_controller;
		mem_debug("  %lld %lld 0x%x %s evict receive\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict_receive\"\n",
			stack->id, target_mod->name);

		/* Is mem controller queue busy? */
		/*if(target_mod->kind == mod_kind_main_memory && mod_request_up_down &&
		mem_controller_get_size_queue(stack) >= mem_controller->size_queue)
		{
			esim_schedule_event(EV_MOD_NMOESI_EVICT_RECEIVE, stack, 1);
			return;
		}*/

		/* Receive message */
		net_receive(target_mod->high_net, target_mod->high_net_node, stack->msg);

		/* Find and lock */
		if (stack->state == cache_block_noncoherent)
		{
			new_stack = mod_stack_create(stack->id, target_mod, stack->src_tag,
				EV_MOD_NMOESI_EVICT_PROCESS_NONCOHERENT, stack, stack->prefetch);
		}
		else
		{
			new_stack = mod_stack_create(stack->id, target_mod, stack->src_tag,
				EV_MOD_NMOESI_EVICT_PROCESS, stack, stack->prefetch);
		}

		/* FIXME It's not guaranteed to be a write */
		new_stack->blocking = 0;
		new_stack->write = 1;
		new_stack->retry = 0;
		new_stack->request_dir = mod_request_up_down;
		//if (target_mod->kind == mod_kind_main_memory && mem_controller->enabled)
		//	esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER, new_stack, 0);
		//else
			esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_EVICT_PROCESS)
	{
		mem_debug("  %lld %lld 0x%x %s evict process\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict_process\"\n",
			stack->id, target_mod->name);

		assert(!stack->stream_hit); //VVV
		assert(stack->state); //VVV

		/* Error locking block */
		if (stack->err)
		{
			ret->err = 1;
			esim_schedule_event(EV_MOD_NMOESI_EVICT_REPLY, stack, 0);
			return;
		}

		/* If data was received, set the block to modified */
		if (stack->reply == reply_ack)
		{
			/* Nothing to do */
		}
		else if (stack->reply == reply_ack_data)
		{
			if (stack->state == cache_block_exclusive)
			{
				cache_set_block(target_mod->cache, stack->set, stack->way,
					stack->tag, cache_block_modified);
			}
			else if (stack->state == cache_block_modified)
			{
				/* Nothing to do */
			}
			else
			{
				fatal("%s: Invalid cache block state: %d\n", __FUNCTION__,
					stack->state);
			}
		}
		else
		{
			fatal("%s: Invalid cache block state: %d\n", __FUNCTION__,
				stack->state);
		}

		/* Remove sharer and owner */
		dir = target_mod->dir;
		for (z = 0; z < dir->zsize; z++)
		{
			/* Skip other subblocks */
			dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
			assert(dir_entry_tag < stack->tag + target_mod->block_size);
			if (dir_entry_tag < stack->src_tag ||
				dir_entry_tag >= stack->src_tag + mod->block_size)
			{
				continue;
			}

			dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
			dir_entry_clear_sharer(dir, stack->set, stack->way, z,
				mod->low_net_node->index);
			if (dir_entry->owner == mod->low_net_node->index)
			{
				dir_entry_set_owner(dir, stack->set, stack->way, z,
					DIR_ENTRY_OWNER_NONE);
			}
		}

		/* Unlock the directory entry */
		dir = target_mod->dir;
		dir_entry_unlock(dir, stack->set, stack->way);

		esim_schedule_event(EV_MOD_NMOESI_EVICT_REPLY, stack, target_mod->latency);
		return;
	}

	if (event == EV_MOD_NMOESI_EVICT_PROCESS_NONCOHERENT)
	{
		mem_debug("  %lld %lld 0x%x %s evict process noncoherent\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict_process_noncoherent\"\n",
			stack->id, target_mod->name);

		/* Error locking block */
		if (stack->err)
		{
			ret->err = 1;
			esim_schedule_event(EV_MOD_NMOESI_EVICT_REPLY, stack, 0);
			return;
		}

		/* If data was received, set the block to modified */
		if (stack->reply == reply_ack_data)
		{
			if (stack->state == cache_block_exclusive)
			{
				cache_set_block(target_mod->cache, stack->set, stack->way,
					stack->tag, cache_block_modified);
			}
			else if (stack->state == cache_block_owned ||
				stack->state == cache_block_modified)
			{
				/* Nothing to do */
			}
			else if (stack->state == cache_block_shared ||
				stack->state == cache_block_noncoherent)
			{
				cache_set_block(target_mod->cache, stack->set, stack->way,
					stack->tag, cache_block_noncoherent);
			}
			else
			{
				fatal("%s: Invalid cache block state: %d\n", __FUNCTION__,
					stack->state);
			}
		}
		else
		{
			fatal("%s: Invalid cache block state: %d\n", __FUNCTION__,
				stack->state);
		}

		/* Remove sharer and owner */
		dir = target_mod->dir;
		for (z = 0; z < dir->zsize; z++)
		{
			/* Skip other subblocks */
			dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
			assert(dir_entry_tag < stack->tag + target_mod->block_size);
			if (dir_entry_tag < stack->src_tag ||
				dir_entry_tag >= stack->src_tag + mod->block_size)
			{
				continue;
			}

			dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
			dir_entry_clear_sharer(dir, stack->set, stack->way, z,
				mod->low_net_node->index);
			if (dir_entry->owner == mod->low_net_node->index)
			{
				dir_entry_set_owner(dir, stack->set, stack->way, z,
					DIR_ENTRY_OWNER_NONE);
			}
		}

		/* Unlock the directory entry */
		dir = target_mod->dir;
		dir_entry_unlock(dir, stack->set, stack->way);

		esim_schedule_event(EV_MOD_NMOESI_EVICT_REPLY, stack, target_mod->latency);
		return;
	}

	if (event == EV_MOD_NMOESI_EVICT_REPLY)
	{
		mem_debug("  %lld %lld 0x%x %s evict reply\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict_reply\"\n",
			stack->id, target_mod->name);

		/* Send message */
		stack->msg = net_try_send_ev(target_mod->high_net, target_mod->high_net_node,
			mod->low_net_node, 8, EV_MOD_NMOESI_EVICT_REPLY_RECEIVE, stack,
			event, stack);
		return;

	}

	if (event == EV_MOD_NMOESI_EVICT_REPLY_RECEIVE)
	{
		mem_debug("  %lld %lld 0x%x %s evict reply receive\n", esim_time, stack->id,
			stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict_reply_receive\"\n",
			stack->id, mod->name);

		/* Receive message */
		net_receive(mod->low_net, mod->low_net_node, stack->msg);

		/* Invalidate block if there was no error. */
		if (!stack->err)
			cache_set_block(mod->cache, stack->src_set, stack->src_way,
				0, cache_block_invalid);

		assert(!dir_entry_group_shared_or_owned(mod->dir, stack->src_set, stack->src_way));
		esim_schedule_event(EV_MOD_NMOESI_EVICT_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_EVICT_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s evict finish\n", esim_time, stack->id,
			stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:evict_finish\"\n",
			stack->id, mod->name);

		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_invalidate_slot(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;
	struct mod_t *mod = stack->mod;

	if (event == EV_MOD_NMOESI_INVALIDATE_SLOT)
	{
		mem_debug("%lld %lld 0x%x %s invalidate slot\n", esim_time, stack->id, stack->addr, mod->name);
		mem_trace("mem.new_access name=\"A-%lld\" type=\"invalidate_slot\" state=\"%s:invalidate_slot\" addr=0x%x\n", stack->id, mod->name, stack->addr);

		/* Next event */
		esim_schedule_event(EV_MOD_NMOESI_INVALIDATE_SLOT_LOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_INVALIDATE_SLOT_LOCK)
	{
		mem_debug("  %lld %lld 0x%x %s invalidate slot lock\n", esim_time, stack->id, stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:invalidate_slot_lock\"\n", stack->id, mod->name);

		/* Call pref find and lock */
		new_stack = mod_stack_create(stack->id, mod, stack->addr, EV_MOD_NMOESI_INVALIDATE_SLOT_ACTION, stack, stack->prefetch);
		new_stack->blocking = 1;
		new_stack->retry = stack->retry;
		new_stack->pref_stream = stack->pref_stream;
		new_stack->pref_slot = stack->pref_slot;
		new_stack->access_kind = mod_access_invalidate_slot;
		new_stack->request_dir = mod_request_up_down;
		esim_schedule_event(EV_MOD_NMOESI_PREF_FIND_AND_LOCK, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_INVALIDATE_SLOT_ACTION)
	{
		int retry_lat;

		mem_debug("  %lld %lld 0x%x %s invalidate slot action\n", esim_time, stack->id, stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:invalidate_slot_action\"\n", stack->id, mod->name);

		/* Error locking */
		if (stack->err)
		{
			mod->read_retries++;
			retry_lat = mod_get_retry_latency(mod);
			mem_debug("    lock error, retrying in %d cycles\n", retry_lat);
			stack->retry = 1;
			esim_schedule_event(EV_MOD_NMOESI_INVALIDATE_SLOT_LOCK, stack, retry_lat);
			return;
		}

		esim_schedule_event(EV_MOD_NMOESI_INVALIDATE_SLOT_UNLOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_INVALIDATE_SLOT_UNLOCK)
	{
		mem_debug("  %lld %lld 0x%x %s invalidate slot unlock\n", esim_time, stack->id, stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:invalidate_slot_unlock\"\n", stack->id, mod->name);

		/* Unlock directory entry */
		dir_pref_entry_unlock(mod->dir, stack->pref_stream, stack->pref_slot);

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_INVALIDATE_SLOT_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_INVALIDATE_SLOT_FINISH)
	{
		mem_debug("%lld %lld 0x%x %s invalidate slot finish\n", esim_time, stack->id, stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:load_finish\"\n", stack->id, mod->name);
		mem_trace("mem.end_access name=\"A-%lld\"\n", stack->id);

		/* Increment witness variable */
		if (stack->witness_ptr)
			(*stack->witness_ptr)++;

		/* Return event queue element into event queue */
		if (stack->event_queue && stack->event_queue_item)
			linked_list_add(stack->event_queue, stack->event_queue_item);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_read_request(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct mod_t *target_mod = stack->target_mod;
	struct mem_controller_t *mem_controller = target_mod->mem_controller;


	struct dir_t *dir;
	struct dir_entry_t *dir_entry;

	uint32_t dir_entry_tag, z;

	if (event == EV_MOD_NMOESI_READ_REQUEST)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s read request\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request\"\n",
			stack->id, mod->name);

		/* Default return values */
		ret->shared = 0;
		ret->err = 0;

		/* Checks */
		assert(stack->request_dir);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
			stack->request_dir == mod_request_down_up);
		assert(mod_get_low_mod(target_mod, stack->addr) == mod ||
			stack->request_dir == mod_request_up_down);

		/* Get source and destination nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = mod->low_net_node;
			dst_node = target_mod->high_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = mod->high_net_node;
			dst_node = target_mod->low_net_node;
		}

		/* Start to count time that a request spends since it leaves L2 until goes back */
		if (stack->request_dir == mod_request_up_down &&
			target_mod->kind == mod_kind_main_memory && mem_controller->enabled)
		stack->t_access_net = esim_cycle();

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, 8,
			EV_MOD_NMOESI_READ_REQUEST_RECEIVE, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_RECEIVE)
	{
		mem_debug("  %lld %lld 0x%x %s read request receive\n", esim_time, stack->id,
			stack->addr, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_receive\"\n",
			stack->id, target_mod->name);


		/* Is mem controller queue busy? */
		/*if(target_mod->kind == mod_kind_main_memory &&
			mod_request_up_down &&
			mem_controller_get_size_queue(stack) >= mem_controller->size_queue)
		{
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_RECEIVE, stack, 1);
			return;
		}*/

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
			net_receive(target_mod->high_net, target_mod->high_net_node, stack->msg);
		else
			net_receive(target_mod->low_net, target_mod->low_net_node, stack->msg);

		if (stack->request_dir == mod_request_up_down &&
			target_mod->kind != mod_kind_main_memory &&
			target_mod->cache->prefetch.type)
		{
			/* Add access to stride detector and record if there is a stride */
			if (!stack->prefetch && !(stack->retry & (1 << target_mod->level)))
				stack->stride = cache_detect_stride(target_mod->cache, stack->addr);

			/* Record access */
			mod_access_start(target_mod, stack, mod_access_read_request);
		}

		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_LOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_LOCK)
	{

		mem_controller=stack->target_mod->mem_controller;

		mem_debug("  %lld %lld 0x%x %s read request lock\n", esim_time, stack->id,
			stack->addr, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_lock\"\n",
			stack->id, mod->name);

		/* Find and lock */
		new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
			EV_MOD_NMOESI_READ_REQUEST_ACTION, stack, stack->prefetch);
		new_stack->blocking = stack->request_dir == mod_request_down_up;
		new_stack->read = 1;
		new_stack->retry = stack->retry; //VVV
		new_stack->background = stack->background; /* Per si hi ha fallada (err=-1) en la eviction */
		new_stack->stream_retried = stack->stream_retried;
		new_stack->stream_retried_cycle = stack->stream_retried_cycle;
		new_stack->request_dir = stack->request_dir;
		new_stack->request_type = read_request;
		//if (target_mod->kind == mod_kind_main_memory && mem_controller->enabled)
		//	esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER, new_stack, 0);
		//else
			esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_ACTION)
	{
		mem_debug("  %lld %lld 0x%x %s read request action\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_action\"\n",
			stack->id, target_mod->name);

		/* Enqueue prefetches */
		if (target_mod->kind != mod_kind_main_memory && must_enqueue_prefetch(stack))
		{
			struct cache_t *target_cache = stack->target_mod->cache;

			/* Enqueue STREAM prefetch */
			if (target_cache->prefetch.type == prefetcher_type_czone_streams)
			{
				if (stack->stream_hit)
				{
					/* Prefetch only one block */
					if (stack->stream_head_hit)
					{
						assert(stack->pref_stream >= 0 && stack->pref_stream < target_cache->prefetch.num_streams);
						assert(stack->pref_slot >= 0 && stack->pref_slot < target_cache->prefetch.aggressivity);
						enqueue_prefetch_on_hit(stack);
					}
				}
				/* Fill all the stream buffer if a stride is detected */
				else
				{
					if (stack->stride)
						enqueue_prefetch_on_miss(stack);
				}
			}
		}

		/* Check block locking error. If read request is down-up, there should not
		 * have been any error while locking. */
		if (stack->err)
		{
			assert(stack->request_dir == mod_request_up_down);

			/* If this is a background stack ret must be null */
			if(stack->background)
			{
				assert(!ret);
				assert(!stack->stream_hit);
				esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_LOCK, stack, 0);
				return;
			}

			ret->err = 1;
			ret->retry |= 1 << target_mod->level;
			ret->stream_retried = stack->stream_retried;
			ret->stream_retried_cycle = stack->stream_retried_cycle;
			mod_stack_set_reply(ret, reply_ack_error);
			stack->reply_size = 8;

			/* Delete access */
			if (target_mod->cache->prefetch.type && target_mod->kind != mod_kind_main_memory)
				mod_access_finish(target_mod, stack);

			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY, stack, 0);
			return;
		}

		esim_schedule_event(stack->request_dir == mod_request_up_down ?
			EV_MOD_NMOESI_READ_REQUEST_UPDOWN : EV_MOD_NMOESI_READ_REQUEST_DOWNUP, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_UPDOWN)
	{
		struct mod_t *owner;

		mem_debug("  %lld %lld 0x%x %s read request updown\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_updown\"\n",
			stack->id, target_mod->name);

		stack->pending = 1;

		/* Set the initial reply message and size.  This will be adjusted later if
		 * a transfer occur between peers. */
		stack->reply_size = mod->block_size + 8;
		mod_stack_set_reply(stack, reply_ack_data);

		/* If acces fast resumed, finish */
		if (stack->fast_resume)
		{
			assert(target_mod->kind != mod_kind_main_memory);
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH, stack, 0);
			return;
		}

		/* If stream_hit or background stack, block can't be in any upper cache */
		if (stack->stream_hit || stack->background)
		{
			assert(target_mod->kind != mod_kind_main_memory);
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS, stack, 0);
			return;
		}

		if (stack->state)
		{
			/* Status = M/O/E/S/N
			 * Check: address is a multiple of requester's block_size
			 * Check: no sub-block requested by mod is already owned by mod */
			assert(stack->addr % mod->block_size == 0);
			dir = target_mod->dir;
			for (z = 0; z < dir->zsize; z++)
			{
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
				assert(dir_entry_tag < stack->tag + target_mod->block_size);
				if (dir_entry_tag < stack->addr || dir_entry_tag >= stack->addr + mod->block_size)
					continue;
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				//assert(dir_entry->owner != mod->low_net_node->index);
				if(dir_entry->owner == mod->low_net_node->index)
					mod->block_already_here++;
			}

			/* TODO If there is only sharers, should one of them
			 *      send the data to mod instead of having target_mod do it? */

			/* Send read request to owners other than mod for all sub-blocks. */
			for (z = 0; z < dir->zsize; z++)
			{
				struct net_node_t *node;

				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;

				/* No owner */
				if (!DIR_ENTRY_VALID_OWNER(dir_entry))
					continue;

				/* Owner is mod */
				if (dir_entry->owner == mod->low_net_node->index)
					continue;

				/* Get owner mod */
				node = list_get(target_mod->high_net->node_list, dir_entry->owner);
				assert(node->kind == net_node_end);
				owner = node->user_data;
				assert(owner);

				/* Not the first sub-block */
				if (dir_entry_tag % owner->block_size)
					continue;

				/* Send read request */
				stack->pending++;
				new_stack = mod_stack_create(stack->id, target_mod, dir_entry_tag,
					EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH, stack, stack->prefetch);
				/* Only set peer if its a subblock that was requested */
				if (dir_entry_tag >= stack->addr &&
					dir_entry_tag < stack->addr + mod->block_size)
				{
					new_stack->peer = mod_stack_set_peer(mod, stack->state);
				}
				new_stack->target_mod = owner;
				new_stack->request_dir = mod_request_down_up;
				esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);
			}
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH, stack, 0);

			/* The prefetcher may have prefetched this earlier and hence
			 * this is a hit now. Let the prefetcher know of this hit
			 * since without the prefetcher, this may have been a miss.
			 * TODO: I'm not sure how relavant this is here for all states. */
			prefetcher_access_hit(stack, target_mod);
		}
		else
		{
			/* State = I */
			assert(!dir_entry_group_shared_or_owned(target_mod->dir,
				stack->set, stack->way));
			new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
				EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS, stack, stack->prefetch);
			/* Peer is NULL since we keep going up-down */
			new_stack->target_mod = mod_get_low_mod(target_mod, stack->tag);
			new_stack->retry = stack->retry; //VVV
			new_stack->stream_retried = stack->stream_retried;
			new_stack->stream_retried_cycle = stack->stream_retried_cycle;
			new_stack->request_dir = mod_request_up_down;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);

			/* The prefetcher may be interested in this miss */
			prefetcher_access_miss(stack, target_mod);

		}
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS)
	{
		mem_debug("  %lld %lld 0x%x %s read request updown miss\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_updown_miss\"\n",
			stack->id, target_mod->name);

		/* Check error */
		if (stack->err)
		{
			assert(!stack->background);

			/* Unlock dir */
			dir_entry_unlock(target_mod->dir, stack->set, stack->way);
			if (stack->stream_hit)
				dir_pref_entry_unlock(target_mod->dir, stack->pref_stream, stack->pref_slot);

			/* Delete access */
			if (target_mod->cache->prefetch.type && target_mod->kind != mod_kind_main_memory)
				mod_access_finish(target_mod, stack);

			ret->err = 1;
			ret->retry |= 1 << target_mod->level;
			ret->stream_retried = stack->stream_retried;
			ret->stream_retried_cycle = stack->stream_retried_cycle;
			mod_stack_set_reply(ret, reply_ack_error);
			stack->reply_size = 8;

			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY, stack, 0);
			return;
		}

		/* Write block from write buffer to cache */
		if(stack->background)
		{
			struct write_buffer_block_t *wb_block;
			struct cache_t *target_cache = target_mod->cache;
			assert(linked_list_count(target_cache->wb.blocks));
			LINKED_LIST_FOR_EACH(target_cache->wb.blocks)
			{
				wb_block = linked_list_get(target_cache->wb.blocks);
				if (wb_block->tag == stack->tag)
				{
					cache_set_block(target_cache, stack->set, stack->way, stack->tag, wb_block->state);
					mod_stack_wake_up_write_buffer(wb_block);
					linked_list_remove(target_cache->wb.blocks);
					free(wb_block);
					wb_block = NULL;
					break;
				}
			}
			assert(!wb_block);

			/* Statistics */
			target_mod->useful_prefetches++;
			if(stack->stream_retried_cycle)
			{
				int i;
				LIST_FOR_EACH(mem_system->mm_mod_list, i)
				{
					struct mod_t *mm_mod = list_get(mem_system->mm_mod_list, i);
					if (mod_serves_address(mm_mod, stack->addr) && esim_time - stack->stream_retried_cycle < mm_mod->latency / 3)
						target_mod->effective_useful_prefetches++;
				}
			}
			else
				target_mod->effective_useful_prefetches++;
		}

		/* Move block from stream to cache */
		else if (stack->stream_hit)
		{
			struct stream_block_t *block = cache_get_pref_block(target_mod->cache, stack->pref_stream, stack->pref_slot);
			assert(stack->tag == block->tag);
			cache_set_block(target_mod->cache, stack->set, stack->way, block->tag, block->state);
			block->state = cache_block_invalid;
			block->tag = -1;
			struct stream_buffer_t *sb = &target_mod->cache->prefetch.streams[stack->pref_stream];
			sb->count--; //COUNT
			if (stack->stream_head_hit)
				sb->head = (sb->head + 1) % sb->num_slots; //HEAD
			/* Statistics */
			target_mod->useful_prefetches++;
			if(stack->stream_retried_cycle)
			{
				int i;
				LIST_FOR_EACH(mem_system->mm_mod_list, i)
				{
					struct mod_t *mm_mod = list_get(mem_system->mm_mod_list, i);
					if (mod_serves_address(mm_mod, stack->addr) && esim_time - stack->stream_retried_cycle < mm_mod->latency / 3)
						target_mod->effective_useful_prefetches++;
				}
			}
			else
				target_mod->effective_useful_prefetches++;
		}

		/* Write block coming from lower level to cache */
		else
		{
			/* Set block state to excl/shared depending on the return value 'shared'
			* that comes from a read request into the next cache level.
			* Also set the tag of the block. */
			cache_set_block(target_mod->cache, stack->set, stack->way, stack->tag,
				stack->shared ? cache_block_shared : cache_block_exclusive);
		}

		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH)
	{
		int shared;

		/* Ensure that a reply was received */
		assert(stack->reply);

		/* Ignore while pending requests */
		assert(stack->pending > 0);
		stack->pending--;
		if (stack->pending)
			return;

		/* Trace */
		mem_debug("  %lld %lld 0x%x %s read request updown finish\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_updown_finish\"\n",
			stack->id, target_mod->name);

		/* If another module has not given the block,
		access main memory */
		if(target_mod->kind == mod_kind_main_memory &&
			mem_controller->enabled &&
			stack->reply != reply_ack_data_sent_to_peer)
		{
			assert(stack->t_access_net >= 0);
			mem_controller->t_inside_net += esim_cycle() - stack->t_access_net;
			stack->t_access_net =- 1;
			new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
				EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH, stack, stack->prefetch);
			new_stack->blocking = stack->request_dir == mod_request_down_up;
			new_stack->read = 1;
			new_stack->retry = 0;
			new_stack->request_type = read_request;
			esim_schedule_event(EV_MOD_NMOESI_INSERT_MEMORY_CONTROLLER, new_stack, 0);
			return;
		}

		if(!stack->background)
		{
			/* If blocks were sent directly to the peer, the reply size would
			 * have been decreased.  Based on the final size, we can tell whether
			 * to send more data or simply ack */
			if (stack->reply_size == 8)
				mod_stack_set_reply(ret, reply_ack);
			else if (stack->reply_size > 8)
				mod_stack_set_reply(ret, reply_ack_data);
			else
				fatal("Invalid reply size: %d", stack->reply_size);
		}

		if(stack->fast_resume)
		{
			int latency = stack->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->latency;
			mod_access_finish(target_mod, stack);
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY, stack, latency);
			return;
		}

		dir = target_mod->dir;

		shared = 0;
		/* With the Owned state, the directory entry may remain owned by the sender */
		if (!stack->retain_owner)
		{
			/* Set owner to 0 for all directory entries not owned by mod. */
			for (z = 0; z < dir->zsize; z++)
			{
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				if (dir_entry->owner != mod->low_net_node->index)
					dir_entry_set_owner(dir, stack->set, stack->way, z,
						DIR_ENTRY_OWNER_NONE);
			}
		}

		/* For each sub-block requested by mod, set mod as sharer, and
		 * check whether there is other cache sharing it. */
		for (z = 0; z < dir->zsize; z++)
		{
			dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
			if (dir_entry_tag < stack->addr || dir_entry_tag >= stack->addr + mod->block_size)
				continue;
			dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
			dir_entry_set_sharer(dir, stack->set, stack->way, z, mod->low_net_node->index);
			if (dir_entry->num_sharers > 1 || stack->nc_write || stack->shared)
				shared = 1;

			/* If the block is owned, non-coherent, or shared,
			 * mod (the higher-level cache) should never be exclusive */
			if (stack->state == cache_block_owned ||
				stack->state == cache_block_noncoherent ||
				stack->state == cache_block_shared )
				shared = 1;
		}

		dir_entry_unlock(dir, stack->set, stack->way);

		/* A background stack has no more work to do */
		if(stack->background)
		{
			mod_stack_return(stack);
			return;
		}

		if (stack->stream_hit)
			dir_pref_entry_unlock(target_mod->dir, stack->pref_stream, stack->pref_slot);

		/* If no sub-block requested by mod is shared by other cache, set mod
		 * as owner of all of them. Otherwise, notify requester that the block is
		 * shared by setting the 'shared' return value to true. */
		ret->shared = shared;
		if (!shared)
		{
			for (z = 0; z < dir->zsize; z++)
			{
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
				if (dir_entry_tag < stack->addr || dir_entry_tag >= stack->addr + mod->block_size)
					continue;
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				dir_entry_set_owner(dir, stack->set, stack->way, z, mod->low_net_node->index);
			}
		}

		/* Delete access */
		if (target_mod->cache->prefetch.type && target_mod->kind != mod_kind_main_memory)
			mod_access_finish(target_mod, stack);

		int latency = stack->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->latency;
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY, stack, latency);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_DOWNUP)
	{
		struct mod_t *owner;

		mem_debug("  %lld %lld 0x%x %s read request downup\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_downup\"\n",
			stack->id, target_mod->name);

		if (stack->stream_hit)
			cache_get_pref_block_data(target_mod->cache, stack->pref_stream, stack->pref_slot, NULL, &stack->state);

		/* Check: state must not be invalid or shared.
		 * By default, only one pending request.
		 * Response depends on state */
		//assert(stack->state != cache_block_invalid);
		assert(stack->state != cache_block_shared);
		assert(stack->state != cache_block_noncoherent);
		stack->pending = 1;

		/* Block was in streams but was silently evicted so there is nothing to send */
		if(stack->state == cache_block_invalid)
			stack->peer = NULL;

		/* Si ha hagut stream_hit, el bloc esta en el buffer de prefetch, per tant,
		 * ninguna caché en un nivell superior pot tenir el bloc. Si ha hagut una fallada down up, tampoc. */
		if (stack->stream_hit || !stack->state)
		{
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS, stack, 0);
			return;
		}

		/* Send a read request to the owner of each subblock. */
		dir = target_mod->dir;
		for (z = 0; z < dir->zsize; z++)
		{
			struct net_node_t *node;

			dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
			assert(dir_entry_tag < stack->tag + target_mod->block_size);
			dir_entry = dir_entry_get(dir, stack->set, stack->way, z);

			/* No owner */
			if (!DIR_ENTRY_VALID_OWNER(dir_entry))
				continue;

			/* Get owner mod */
			node = list_get(target_mod->high_net->node_list, dir_entry->owner);
			assert(node && node->kind == net_node_end);
			owner = node->user_data;

			/* Not the first sub-block */
			if (dir_entry_tag % owner->block_size)
				continue;

			stack->pending++;
			new_stack = mod_stack_create(stack->id, target_mod, dir_entry_tag,
				EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS, stack, stack->prefetch);
			new_stack->target_mod = owner;
			new_stack->request_dir = mod_request_down_up;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST, new_stack, 0);
		}

		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS)
	{
		/* Ignore while pending requests */
		assert(stack->pending > 0);
		stack->pending--;
		if (stack->pending)
			return;

		mem_debug("  %lld %lld 0x%x %s read request downup wait for reqs\n",
			esim_time, stack->id, stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_downup_wait_for_reqs\"\n",
			stack->id, target_mod->name);

		if (stack->peer)
		{
			/* Send this block (or subblock) to the peer */
			new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
				EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH, stack, stack->prefetch);
			new_stack->peer = mod_stack_set_peer(stack->peer, stack->state);
			new_stack->target_mod = stack->target_mod;
			esim_schedule_event(EV_MOD_NMOESI_PEER_SEND, new_stack, 0);
		}
		else
		{
			/* No data to send to peer, so finish */
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH, stack, 0);
		}

		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s read request downup finish\n",
			esim_time, stack->id, stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_downup_finish\"\n",
			stack->id, target_mod->name);

		if (stack->reply == reply_ack_data)
		{
			/* If data was received, it was owned or modified by a higher level cache.
			 * We need to continue to propagate it up until a peer is found */

			if (stack->peer)
			{
				/* Peer was found, so this directory entry should be changed
				 * to owned */
				cache_set_block(target_mod->cache, stack->set, stack->way,
					stack->tag, cache_block_owned);

				/* Higher-level cache changed to shared, set owner of
				 * sub-blocks to NONE. */
				dir = target_mod->dir;
				for (z = 0; z < dir->zsize; z++)
				{
					dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
					assert(dir_entry_tag < stack->tag + target_mod->block_size);
					dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
					dir_entry_set_owner(dir, stack->set, stack->way, z,
						DIR_ENTRY_OWNER_NONE);
				}

				stack->reply_size = 8;
				mod_stack_set_reply(ret, reply_ack_data_sent_to_peer);

				/* Decrease the amount of data that mod will have to send back
				 * to its higher level cache */
				ret->reply_size -= target_mod->block_size;
				assert(ret->reply_size >= 8);

				/* Let the lower-level cache know not to delete the owner */
				ret->retain_owner = 1;
			}
			else
			{
				/* Set state to shared */
				cache_set_block(target_mod->cache, stack->set, stack->way,
					stack->tag, cache_block_shared);

				/* State is changed to shared, set owner of sub-blocks to 0. */
				dir = target_mod->dir;
				for (z = 0; z < dir->zsize; z++)
				{
					dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
					assert(dir_entry_tag < stack->tag + target_mod->block_size);
					dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
					dir_entry_set_owner(dir, stack->set, stack->way, z,
						DIR_ENTRY_OWNER_NONE);
				}

				stack->reply_size = target_mod->block_size + 8;
				mod_stack_set_reply(ret, reply_ack_data);
			}
		}
		else if (stack->reply == reply_ack)
		{
			/* Higher-level cache was exclusive with no modifications above it */
			stack->reply_size = 8;

			/* Set state to shared */
			cache_set_block(target_mod->cache, stack->set, stack->way,
				stack->tag, cache_block_shared);

			/* State is changed to shared, set owner of sub-blocks to 0. */
			dir = target_mod->dir;
			for (z = 0; z < dir->zsize; z++)
			{
				dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
				assert(dir_entry_tag < stack->tag + target_mod->block_size);
				dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
				dir_entry_set_owner(dir, stack->set, stack->way, z,
					DIR_ENTRY_OWNER_NONE);
			}

			if (stack->peer)
			{
				stack->reply_size = 8;
				mod_stack_set_reply(ret, reply_ack_data_sent_to_peer);

				/* Decrease the amount of data that mod will have to send back
				 * to its higher level cache */
				ret->reply_size -= target_mod->block_size;
				assert(ret->reply_size >= 8);
			}
			else
			{
				mod_stack_set_reply(ret, reply_ack);
				stack->reply_size = 8;
			}
		}
		else if (stack->reply == reply_none)
		{
			/* This block is not present in any higher level caches */

			if (stack->peer)
			{
				stack->reply_size = 8;
				mod_stack_set_reply(ret, reply_ack_data_sent_to_peer);

				/* Decrease the amount of data that mod will have to send back
				 * to its higher level cache */
				ret->reply_size -= target_mod->sub_block_size;
				assert(ret->reply_size >= 8);

				if (stack->stream_hit)
				{
					/* Set prefetched block to shared */
					struct stream_block_t *block = cache_get_pref_block(target_mod->cache, stack->pref_stream, stack->pref_slot);
					block->state = cache_block_shared;
				}
				else if (stack->state == cache_block_modified ||
					stack->state == cache_block_owned)
				{
					/* Let the lower-level cache know not to delete the owner */
					ret->retain_owner = 1;

					/* Set block to owned */
					cache_set_block(target_mod->cache, stack->set, stack->way,
						stack->tag, cache_block_owned);
				}
				else
				{
					/* Set block to shared */
					cache_set_block(target_mod->cache, stack->set, stack->way,
						stack->tag, cache_block_shared);
				}
			}
			else
			{
				if (stack->state == cache_block_exclusive ||
					stack->state == cache_block_shared)
				{
					stack->reply_size = 8;
					mod_stack_set_reply(ret, reply_ack);

				}
				else if (stack->state == cache_block_owned ||
					stack->state == cache_block_modified ||
					stack->state == cache_block_noncoherent)
				{
					/* No peer exists, so data is returned to mod */
					stack->reply_size = target_mod->sub_block_size + 8;
					mod_stack_set_reply(ret, reply_ack_data);
				}
				else if(stack->state == cache_block_invalid)
				{
					stack->reply_size = 8;
					mod_stack_set_reply(ret, reply_ack);

					/* If down up miss, remove sharer and owner */
					dir = mod->dir;
					for (z = 0; z < dir->zsize; z++)
					{
						/* Skip other subblocks */
						dir_entry_tag = (stack->tag & ~mod->cache->block_mask) + z * mod->sub_block_size;
						assert(dir_entry_tag < (stack->tag & ~mod->cache->block_mask) + mod->block_size);
						if (dir_entry_tag != stack->tag)
							continue;

						/* We want the set and way of mod cache, not of target_mod cache.
						 * In stack we have the values for target_mod, so we must use ret. */
						dir_entry = dir_entry_get(dir, ret->set, ret->way, z);
						dir_entry_clear_sharer(dir, ret->set, ret->way, z, target_mod->low_net_node->index);
						if (dir_entry->owner == target_mod->low_net_node->index)
						{
							dir_entry_set_owner(dir, ret->set, ret->way, z,
								DIR_ENTRY_OWNER_NONE);
						}
					}
				}
				else
				{
					fatal("Invalid cache block state: %d\n", stack->state);
				}

				/* Set block state */
				if (stack->stream_hit)
				{
					/* TODO: usar una funció */
					struct stream_block_t * block = cache_get_pref_block(
						target_mod->cache, stack->pref_stream, stack->pref_slot);
					block->state = cache_block_shared;
				}
				else if(stack->state)
				{
					cache_set_block(target_mod->cache, stack->set, stack->way,
						stack->tag, cache_block_shared);
				}
			}
		}
		else
		{
			fatal("Unexpected reply type: %d\n", stack->reply);
		}


		if(stack->state)
		{
			if (stack->stream_hit)
				dir_pref_entry_unlock(target_mod->dir, stack->pref_stream, stack->pref_slot);
			else
				dir_entry_unlock(target_mod->dir, stack->set, stack->way);
		}


		int latency = stack->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->latency;
		esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_REPLY, stack, latency);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_REPLY)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s read request reply\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_reply\"\n",
			stack->id, target_mod->name);

		/* Checks */
		assert(stack->reply_size);
		assert(stack->request_dir);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
			mod_get_low_mod(target_mod, stack->addr) == mod);

		/* Get network and nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = target_mod->high_net_node;
			dst_node = mod->low_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = target_mod->low_net_node;
			dst_node = mod->high_net_node;
		}

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, stack->reply_size,
			EV_MOD_NMOESI_READ_REQUEST_FINISH, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_READ_REQUEST_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s read request finish\n", esim_time, stack->id,
			stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:read_request_finish\"\n",
			stack->id, mod->name);

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
			net_receive(mod->low_net, mod->low_net_node, stack->msg);
		else
			net_receive(mod->high_net, mod->high_net_node, stack->msg);

		if (stack->request_dir == mod_request_up_down &&
			target_mod->kind == mod_kind_main_memory &&
			mem_controller->enabled)
		{
			assert(stack->t_access_net >= 0);
			mem_controller->t_inside_net += esim_time-stack->t_access_net;
			stack->t_access_net = -1;
		}

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_write_request(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct mod_t *target_mod = stack->target_mod;

	struct mem_controller_t *mem_controller = target_mod->mem_controller;

	struct dir_t *dir;
	struct dir_entry_t *dir_entry;

	uint32_t dir_entry_tag, z;


	if (event == EV_MOD_NMOESI_WRITE_REQUEST)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s write request\n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request\"\n",
			stack->id, mod->name);

		/* Default return values */
		ret->err = 0;

		/* For write requests, we need to set the initial reply size because
		 * in updown, peer transfers must be allowed to decrease this value
		 * (during invalidate). If the request turns out to be downup, then
		 * these values will get overwritten. */
		stack->reply_size = mod->block_size + 8;
		mod_stack_set_reply(stack, reply_ack_data);

		/* Checks */
		assert(stack->request_dir);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
			stack->request_dir == mod_request_down_up);
		assert(mod_get_low_mod(target_mod, stack->addr) == mod ||
			stack->request_dir == mod_request_up_down);

		/* Get source and destination nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = mod->low_net_node;
			dst_node = target_mod->high_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = mod->high_net_node;
			dst_node = target_mod->low_net_node;
		}

		if (stack->request_dir == mod_request_up_down &&
			target_mod->kind == mod_kind_main_memory &&
			mem_controller->enabled)
			stack->t_access_net = esim_cycle();

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, 8,
			EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE)
	{
		mem_debug("  %lld %lld 0x%x %s write request receive\n", esim_time, stack->id,
			stack->addr, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_receive\"\n",
			stack->id, target_mod->name);

		/* Is mem controller queue busy? */
		/*if(target_mod->kind == mod_kind_main_memory &&
			mod_request_up_down &&
			mem_controller_get_size_queue(stack) >= mem_controller->size_queue)
		{
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE, stack, 1);
			return;
		}*/

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
			net_receive(target_mod->high_net, target_mod->high_net_node, stack->msg);
		else
			net_receive(target_mod->low_net, target_mod->low_net_node, stack->msg);

		if (stack->request_dir == mod_request_up_down &&
			target_mod->kind != mod_kind_main_memory &&
			target_mod->cache->prefetch.type)
		{
			/* Add access to stride detector and record if there is a stride */
			if (!(stack->retry & (1 << target_mod->level)))
				stack->stride = cache_detect_stride(target_mod->cache, stack->addr);

			/* Record access */
			mod_access_start(target_mod, stack, mod_access_write_request);
		}

		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_LOCK, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_LOCK)
	{
		mem_controller=stack->target_mod->mem_controller;

		mem_debug("  %lld %lld 0x%x %s write request lock\n", esim_time, stack->id,
			stack->addr, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_lock\"\n",
			stack->id, target_mod->name);

		/* Call find and lock */
		new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
			EV_MOD_NMOESI_WRITE_REQUEST_ACTION, stack, stack->prefetch);
		new_stack->blocking = stack->request_dir == mod_request_down_up;
		new_stack->write = 1;
		new_stack->retry = stack->retry; //VVV
		new_stack->background = stack->background;
		new_stack->stream_retried = stack->stream_retried;
		new_stack->stream_retried_cycle = stack->stream_retried_cycle;
		new_stack->request_dir = stack->request_dir;

		//if(target_mod->kind == mod_kind_main_memory && mem_controller->enabled)
		//	esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER, new_stack, 0);
		//else
			esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_ACTION)
	{
		mem_debug("  %lld %lld 0x%x %s write request action\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_action\"\n",
			stack->id, target_mod->name);

		/* Enqueue prefetches */
		if (target_mod->kind != mod_kind_main_memory && must_enqueue_prefetch(stack))
		{
			struct cache_t *target_cache = stack->target_mod->cache;

			/* Enqueue STREAM prefetch */
			if (target_cache->prefetch.type == prefetcher_type_czone_streams)
			{
				if (stack->stream_hit)
				{
					/* Prefetch only one block */
					if (stack->stream_head_hit)
					{
						assert(stack->pref_stream >= 0 && stack->pref_stream < target_cache->prefetch.num_streams);
						assert(stack->pref_slot >= 0 && stack->pref_slot < target_cache->prefetch.aggressivity);
						enqueue_prefetch_on_hit(stack);
					}
				}
				/* Fill all the stream buffer if a stride is detected */
				else
				{
					if (stack->stride)
						enqueue_prefetch_on_miss(stack);
				}
			}
		}

		/* Check lock error. If write request is down-up, there should
		 * have been no error. */
		if (stack->err)
		{
			assert(stack->request_dir == mod_request_up_down);

			/* If this is a background stack ret must be null */
			if(stack->background)
			{
				assert(!ret);
				esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_LOCK, stack, 0);
				return;
			}

			ret->err = 1;
			ret->retry |= 1 << target_mod->level;
			ret->stream_retried = stack->stream_retried;
			ret->stream_retried_cycle = stack->stream_retried_cycle;
			stack->reply_size = 8;

			/* Delete access */
			if (target_mod->cache->prefetch.type && target_mod->kind != mod_kind_main_memory)
				mod_access_finish(target_mod, stack);

			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY, stack, 0);
			return;
		}

		/* If fast resumed access send block */
		if (stack->fast_resume)
		{
			assert(target_mod->kind != mod_kind_main_memory);

			/* Delete access */
			if (target_mod->cache->prefetch.type && target_mod->kind != mod_kind_main_memory)
				mod_access_finish(target_mod, stack);

			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY, stack, target_mod->latency);
			return;
		}

		/* If stream_hit, background stack or down up miss (silent eviction of stream block) there aren't any upper level sharers of the block */
		if (stack->stream_hit || stack->background ||
			(!stack->stream_hit && !stack->state && stack->request_dir == mod_request_down_up))
		{
			mem_debug("  %lld %lld 0x%x %s write prefetch hit direction %d\n",
				esim_time, stack->id, stack->tag, target_mod->name,
				stack->request_dir == mod_request_up_down);
			assert(target_mod->kind != mod_kind_main_memory);

			/* Block in write buffer */
			if(stack->background)
			{
				struct write_buffer_block_t *wb_block;
				struct cache_t *target_cache = target_mod->cache;
				assert(linked_list_count(target_cache->wb.blocks));
				LINKED_LIST_FOR_EACH(target_cache->wb.blocks)
				{
					wb_block = linked_list_get(target_cache->wb.blocks);
					if (wb_block->tag == stack->tag)
					{
						stack->state = wb_block->state;
						break;
					}
				}
				assert(wb_block);
				assert(wb_block->tag == stack->tag);
			}

			/* Stream hit */
			else if(stack->stream_hit)
			{
				int tag;
				cache_get_pref_block_data(target_mod->cache, stack->pref_stream, stack->pref_slot,
					&tag, &stack->state);
				assert(stack->tag == tag);
			}

			/* Else, block is a down up miss due a silent eviction in a stream */

			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE, stack, 0);
			return;
		}

		/* Invalidate the rest of upper level sharers */
		new_stack = mod_stack_create(stack->id, target_mod, 0,
			EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE, stack, stack->prefetch);
		new_stack->except_mod = mod;
		new_stack->set = stack->set;
		new_stack->way = stack->way;
		new_stack->peer = mod_stack_set_peer(stack->peer, stack->state);
		esim_schedule_event(EV_MOD_NMOESI_INVALIDATE, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE)
	{
		mem_debug("  %lld %lld 0x%x %s write request exclusive\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_exclusive\"\n",
			stack->id, target_mod->name);

		if (stack->request_dir == mod_request_up_down)
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN, stack, 0);
		else
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN)
	{
		mem_debug("  %lld %lld 0x%x %s write request updown\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_updown\"\n",
			stack->id, target_mod->name);

		/* state = M/E */
		if (stack->state == cache_block_modified ||
			stack->state == cache_block_exclusive)
		{
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH, stack, 0);
		}

		/* state = O/S/I/N */
		else if (stack->state == cache_block_owned || stack->state == cache_block_shared ||
			stack->state == cache_block_invalid || stack->state == cache_block_noncoherent)
		{
			new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
				EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH, stack, stack->prefetch);
			new_stack->peer = mod_stack_set_peer(mod, stack->state);
			new_stack->target_mod = mod_get_low_mod(target_mod, stack->tag);
			new_stack->retry = stack->retry; //VVV
			new_stack->stream_retried = stack->stream_retried;
			new_stack->stream_retried_cycle = stack->stream_retried_cycle;
			new_stack->request_dir = mod_request_up_down;
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST, new_stack, 0);

			if (stack->state == cache_block_invalid)
			{
				/* The prefetcher may be interested in this miss */
				prefetcher_access_miss(stack, target_mod);
			}
		}
		else
		{
			fatal("Invalid cache block state: %d\n", stack->state);
		}

		if (stack->state != cache_block_invalid)
		{
			/* The prefetcher may have prefetched this earlier and hence
			 * this is a hit now. Let the prefetcher know of this hit
			 * since without the prefetcher, this may been a miss.
			 * TODO: I'm not sure how relavant this is here for all states. */
			prefetcher_access_hit(stack, target_mod);
		}

		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s write request updown finish\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_updown_finish\"\n",
			stack->id, target_mod->name);

		/* If another module has not given the block,
		access main memory */
		if (target_mod->kind == mod_kind_main_memory &&
			mem_controller->enabled &&
			stack->reply != reply_ack_data_sent_to_peer)
		{
			assert(stack->t_access_net >= 0);
			mem_controller->t_inside_net+=esim_time-stack->t_access_net;
			stack->t_access_net=-1;
			new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
				EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH, stack, stack->prefetch);
			new_stack->blocking = stack->request_dir == mod_request_down_up;
			new_stack->write = 1;
			new_stack->retry = 0;
			new_stack->request_dir = stack->request_dir;
			new_stack->request_type = write_request;
			esim_schedule_event(EV_MOD_NMOESI_INSERT_MEMORY_CONTROLLER, new_stack, 0);
			return;
		}

		/* Ensure that a reply was received */
		assert(stack->reply);

		/* Error in write request to next cache level */
		if (stack->err)
		{
			assert(!stack->background);

			/* Unlock dirs */
			dir_entry_unlock(target_mod->dir, stack->set, stack->way);
			if (stack->stream_hit)
				dir_pref_entry_unlock(target_mod->dir, stack->pref_stream, stack->pref_slot);

			/* Delete access */
			if (target_mod->cache->prefetch.type && target_mod->kind != mod_kind_main_memory)
				mod_access_finish(target_mod, stack);

			ret->err = 1;
			ret->retry |= 1 << target_mod->level;
			ret->stream_retried = stack->stream_retried;
			ret->stream_retried_cycle = stack->stream_retried_cycle;
			mod_stack_set_reply(ret, reply_ack_error);
			stack->reply_size = 8;
			esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY, stack, 0);
			return;
		}

		/* Check that addr is a multiple of mod.block_size.
		 * Set mod as sharer and owner. */
		dir = target_mod->dir;
		for (z = 0; z < dir->zsize; z++)
		{
			assert(stack->addr % mod->block_size == 0);
			dir_entry_tag = stack->tag + z * target_mod->sub_block_size;
			assert(dir_entry_tag < stack->tag + target_mod->block_size);
			if (dir_entry_tag < stack->addr || dir_entry_tag >= stack->addr + mod->block_size)
				continue;
			dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
			dir_entry_set_sharer(dir, stack->set, stack->way, z, mod->low_net_node->index);
			dir_entry_set_owner(dir, stack->set, stack->way, z, mod->low_net_node->index);
			assert(dir_entry->num_sharers == 1);
		}

		/* Remove block from write buffer */
		if(stack->background)
		{
			struct write_buffer_block_t *wb_block;
			struct cache_t *target_cache = target_mod->cache;
			assert(linked_list_count(target_cache->wb.blocks));
			LINKED_LIST_FOR_EACH(target_cache->wb.blocks)
			{
				wb_block = linked_list_get(target_cache->wb.blocks);
				if (wb_block->tag == stack->tag)
				{
					mod_stack_wake_up_write_buffer(wb_block);
					linked_list_remove(target_cache->wb.blocks);
					free(wb_block);
					wb_block = NULL;
					break;
				}
			}
			assert(!wb_block);

			/* Statistics */
			target_mod->useful_prefetches++;
			if(stack->stream_retried_cycle)
			{
				int i;
				LIST_FOR_EACH(mem_system->mm_mod_list, i)
				{
					struct mod_t *mm_mod = list_get(mem_system->mm_mod_list, i);
					if (mod_serves_address(mm_mod, stack->addr) && esim_time - stack->stream_retried_cycle < mm_mod->latency / 3)
						target_mod->effective_useful_prefetches++;
				}
			}
			else
				target_mod->effective_useful_prefetches++;
		}

		/* Remove block from stream */
		else if (stack->stream_hit)
		{
			cache_set_pref_block(target_mod->cache, stack->pref_stream, stack->pref_slot, -1, cache_block_invalid);
			struct stream_buffer_t *sb = &target_mod->cache->prefetch.streams[stack->pref_stream];
			sb->count--; //COUNT
			if (stack->stream_head_hit)
				sb->head = (sb->head + 1) % sb->num_slots; //HEAD
			dir_pref_entry_unlock(target_mod->dir, stack->pref_stream, stack->pref_slot);

			/* Statistics */
			target_mod->useful_prefetches++;
			if(stack->stream_retried_cycle)
			{
				int i;
				LIST_FOR_EACH(mem_system->mm_mod_list, i)
				{
					struct mod_t *mm_mod = list_get(mem_system->mm_mod_list, i);
					if (mod_serves_address(mm_mod, stack->addr) && esim_time - stack->stream_retried_cycle < mm_mod->latency / 3)
						target_mod->effective_useful_prefetches++;
				}
			}
			else
				target_mod->effective_useful_prefetches++;
		}

		/* Set states O/E/S/I->E */
		cache_set_block(target_mod->cache, stack->set, stack->way,
			stack->tag, cache_block_exclusive);

		/* If blocks were sent directly to the peer, the reply size would
		 * have been decreased.  Based on the final size, we can tell whether
		 * to send more data up or simply ack */
		if(!stack->background)
		{
			if (stack->reply_size == 8)
				mod_stack_set_reply(ret, reply_ack);
			else if (stack->reply_size > 8)
				mod_stack_set_reply(ret, reply_ack_data);
			else
				fatal("Invalid reply size: %d", stack->reply_size);
		}

		/* Unlock, reply_size is the data of the size of the requester's block. */
		dir_entry_unlock(target_mod->dir, stack->set, stack->way);

		/* There is no more work for this stack */
		if (stack->background)
		{
			mod_stack_return(stack);
			return;
		}

		/* Delete access */
		if (target_mod->cache->prefetch.type && target_mod->kind != mod_kind_main_memory)
			mod_access_finish(target_mod, stack);

		int latency = stack->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->latency;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY, stack, latency);

		if (stack->request_dir == mod_request_up_down &&
			target_mod->kind == mod_kind_main_memory &&
			mem_controller->enabled)
			stack->t_access_net=esim_time;

		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP)
	{
		mem_debug("  %lld %lld 0x%x %s write request downup\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_downup\"\n",
			stack->id, target_mod->name);

		//assert(stack->state != cache_block_invalid);
		assert(stack->stream_hit || stack->state == cache_block_invalid || // Stream hit or silent eviction
			!dir_entry_group_shared_or_owned(target_mod->dir, stack->set, stack->way));

		/* Compute reply size */
		if (stack->state == cache_block_exclusive ||
			stack->state == cache_block_shared ||
			stack->state == cache_block_invalid) //VVV
		{
			/* Exclusive and shared states send an ack */
			stack->reply_size = 8;
			mod_stack_set_reply(ret, reply_ack);
		}
		else if (stack->state == cache_block_noncoherent)
		{
			/* Non-coherent state sends data */
			stack->reply_size = target_mod->block_size + 8;
			mod_stack_set_reply(ret, reply_ack_data);
		}
		else if (stack->state == cache_block_modified ||
			stack->state == cache_block_owned)
		{
			if (stack->peer)
			{
				/* Modified or owned entries send data directly to peer
				 * if it exists */
				mod_stack_set_reply(ret, reply_ack_data_sent_to_peer);
				stack->reply_size = 8;

				/* This control path uses an intermediate stack that disappears, so
				 * we have to update the return stack of the return stack */
				ret->ret_stack->reply_size -= target_mod->block_size;
				assert(ret->ret_stack->reply_size >= 8);

				/* Send data to the peer */
				new_stack = mod_stack_create(stack->id, target_mod, stack->tag,
					EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH, stack, stack->prefetch);
				new_stack->peer = mod_stack_set_peer(stack->peer, stack->state);
				new_stack->target_mod = stack->target_mod;

				esim_schedule_event(EV_MOD_NMOESI_PEER_SEND, new_stack, 0);
				return;
			}
			else
			{
				/* If peer does not exist, data is returned to mod */
				mod_stack_set_reply(ret, reply_ack_data);
				stack->reply_size = target_mod->block_size + 8;
			}
		}
		else
		{
			fatal("Invalid cache block state: %d\n", stack->state);
		}

		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH, stack, 0);

		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s write request downup complete\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_downup_finish\"\n",
			stack->id, target_mod->name);

		/* Set state to I, unlock */
		if (stack->stream_hit)
		{
			cache_set_pref_block(target_mod->cache, stack->pref_stream, stack->pref_slot, -1, cache_block_invalid);
			struct stream_buffer_t *sb = &target_mod->cache->prefetch.streams[stack->pref_stream];
			sb->count--; //COUNT
			dir_pref_entry_unlock(target_mod->dir, stack->pref_stream, stack->pref_slot);
			/* If we remove head, we must increment it */
			if (stack->pref_slot == sb->head)
				sb->head = (stack->pref_slot + 1) % sb->num_slots; //HEAD
		}
		else if(stack->state) /* Sols fem això si no ha hagut un down_up miss degut als streams */
		{
			cache_set_block(target_mod->cache, stack->set, stack->way, 0, cache_block_invalid);
			dir_entry_unlock(target_mod->dir, stack->set, stack->way);
		}

		int latency = ret->reply == reply_ack_data_sent_to_peer ? 0 : target_mod->latency;
		esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST_REPLY, stack, latency);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_REPLY)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s write request reply\n", esim_time, stack->id,
			stack->tag, target_mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_reply\"\n",
			stack->id, target_mod->name);

		/* Checks */
		assert(stack->reply_size);
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
			mod_get_low_mod(target_mod, stack->addr) == mod);

		/* Get network and nodes */
		if (stack->request_dir == mod_request_up_down)
		{
			net = mod->low_net;
			src_node = target_mod->high_net_node;
			dst_node = mod->low_net_node;
		}
		else
		{
			net = mod->high_net;
			src_node = target_mod->low_net_node;
			dst_node = mod->high_net_node;
		}

		stack->msg = net_try_send_ev(net, src_node, dst_node, stack->reply_size,
			EV_MOD_NMOESI_WRITE_REQUEST_FINISH, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_WRITE_REQUEST_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s write request finish\n", esim_time, stack->id,
			stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:write_request_finish\"\n",
			stack->id, mod->name);

		/* Receive message */
		if (stack->request_dir == mod_request_up_down)
			net_receive(mod->low_net, mod->low_net_node, stack->msg);
		else
			net_receive(mod->high_net, mod->high_net_node, stack->msg);

		if (stack->request_dir == mod_request_up_down &&
			target_mod->kind == mod_kind_main_memory &&
			mem_controller->enabled)
		{
			assert(stack->t_access_net >= 0);
			mem_controller->t_inside_net += esim_time-stack->t_access_net;
			stack->t_access_net =- 1;
		}

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_request_main_memory(int event, void *data )
{

        struct mod_stack_t *stack = data;
        struct mod_stack_t * new_stack;
        struct mod_t *mod = stack->mod;
        struct mod_stack_t *ret = stack->ret_stack;
        struct reg_channel_t * channel;
        struct reg_bank_t * bank;


        struct mem_controller_t * mem_controller=stack->mod->mem_controller;

        struct mem_controller_queue_t * normal_queue;
        struct mem_controller_queue_t * pref_queue;

        int t_trans;
        int can_acces_bank;
        int size_queue = mem_controller->size_queue;
        int num_queues = mem_controller->num_queues;
        int queues_examined = 0;
        int num_bank;
        int dir_rank, dir_bank;
        int num_banks = mem_controller->num_regs_bank;
        int num_ranks = mem_controller->num_regs_rank;
        int time;
        int cycles_proc_by_bus=mem_controller->cycles_proc_bus;


        channel = mem_controller->regs_channel;
	if (event == EV_MOD_NMOESI_INSERT_MEMORY_CONTROLLER)
		{

		        mem_debug("  %lld %lld 0x%x %s insert request in mem. controller\n", esim_time, stack->id, stack->addr&~mod->cache->block_mask, mod->name);
		        mem_trace("mem.access name=\"A-%lld\" state=\"%s:insert request in MC (pref=%d)\"\n", stack->id,mod->name , stack->prefetch);


		        assert(mod->kind==mod_kind_main_memory);
		        /* Statistics */
		        time = esim_cycle() - mem_controller->last_cycle;
		        mem_controller->accesses++;

		        for (int i = 0; i < num_queues; i++)
		        {
		                pref_queue = mem_controller->pref_queue[i];
		                normal_queue = mem_controller->normal_queue[i];

		                if (linked_list_count(pref_queue->queue) < size_queue)
		                        pref_queue->total_requests += linked_list_count(pref_queue->queue) * time;
		                else // queue full, add the limit
		                        pref_queue->total_requests += size_queue * time;
		                if (linked_list_count(normal_queue->queue) < size_queue)
		                        normal_queue->total_requests += linked_list_count(normal_queue->queue) * time;
		                else // queue full, add the limit
		                        normal_queue->total_requests += size_queue * time;


		                /* if(linked_list_count(normal_queue->queue)== size_queue)
		                        normal_queue->instant_begin_full=esim_cycle;

		                if(linked_list_count(pref_queue->queue)== size_queue)
		                        pref_queue->instant_begin_full=esim_cycle;

		                if(linked_list_count(normal_queue)>= size_queue)
		                        mem_controller->t_normal_queue_full+=cycles_proc_by_bus;

		                if(linked_list_count(pref_queue)>= size_queue)
		                        mem_controller->t_pref_queue_full+=cycles_proc_by_bus; */
			}
		        mem_controller->n_times_queue_examined += time; //se podria cambiar dividinto x esim al final
		        /* Update last cycle queue will be examined */
		        mem_controller->last_cycle = esim_cycle();



		        /* Depending on MC priory policy,  select and add request one of the MC queues */
		        if (mem_controller->policy_queues == policy_prefetch_normal_queues)
		        {
		                if (!stack->prefetch)
		                {
		                        mem_controller->normal_accesses++;
		                        mem_controller_normal_queue_add(stack);
		                }
		                else
		                {
		                        mem_controller->pref_accesses++;
		                        if(mem_controller->piggybacking && mem_controller_is_piggybacked(stack))
		                        {
		                                mem_controller_normal_queue_add(stack);

		                        }else
		                                mem_controller_prefetch_queue_add(stack);

		                }
		        }
		        else if (mem_controller->policy_queues == policy_one_queue_FCFS)
		        {       mem_controller_normal_queue_add(stack);

		                if(!stack->prefetch)
		                        mem_controller->normal_accesses++;
		                else
		                        mem_controller->pref_accesses++;
		        }else
		                fatal("Invalid MC priroity policy of queues :%d\n", mem_controller->policy_queues);

		        /* Was the MC examining the queues to send the requests? */
		        if (mem_controller_stacks_normalQueues_count(mem_controller) + mem_controller_stacks_prefQueues_count(mem_controller) == 1) //this request is the only one in the queue
		        {
		                /* Calcul when is the next cycle of bus */
		                int when = esim_cycle() % mem_controller->cycles_proc_bus;

		                /*Stadistics*/
		                mem_controller->t_wait+=when;
		                if(!stack->prefetch) mem_controller->t_normal_wait+=when;
		                else mem_controller->t_pref_wait+=when;

		                /* X cycles less that ALL request have to wait before they throw */
		                mem_controller_update_requests_threshold(when, mem_controller);
		                assert(stack->mod->kind==mod_kind_main_memory);

		                /*Create a new stack with default values to examine mc every cycle */
		                new_stack=xcalloc(1,sizeof(struct mod_stack_t));
		                new_stack->mod=stack->mod;


		                if (mem_controller->queue_per_bank)
		                        esim_schedule_event(EV_MOD_NMOESI_EXAMINE_QUEUE_REQUEST, new_stack, when);
		                else
		                        esim_schedule_event(EV_MOD_NMOESI_EXAMINE_ONLY_ONE_QUEUE_REQUEST, new_stack, when);
		        }

		        return;
        	}

		if (event == EV_MOD_NMOESI_EXAMINE_ONLY_ONE_QUEUE_REQUEST)
		{

		        struct linked_list_t * normal = mem_controller->normal_queue[0]->queue;
		        struct linked_list_t * pref = mem_controller->pref_queue[0]->queue;
		        struct mod_stack_t *stack_orig = stack; // save this stack, because that has original values


		        new_stack = mem_controller_select_request(0, mem_controller->priority_request_in_queue,mem_controller);

		        while (new_stack != NULL)
		        {
		                can_acces_bank = row_buffer_find_row(mem_controller,new_stack->mod, new_stack->addr, &new_stack->channel, &new_stack->rank,
		                        &new_stack->bank, &new_stack->row, & new_stack->tag, &new_stack->state_main_memory);
		                mem_trace("mem.access name=\"A-%lld\" state=\"%s:examine queue MC\"\n", new_stack->id, new_stack->mod->name);
		                mem_debug("  %lld %lld 0x%x %s examine queue MC (row=%d bank=%d rank=%d channel=%d)\n", esim_time, new_stack->id,
		                        new_stack->addr&~new_stack->mod->cache->block_mask, new_stack->mod->name, new_stack->row, new_stack->bank,
		                        new_stack->rank, new_stack->channel);

		                /* We can serve the request */
		                if (can_acces_bank)
		                {
		                        /* Is the row in the row buffer? */
		                        if (new_stack->state_main_memory == row_buffer_hit)
		                        {
		                                /* HIT */
		                                channel[new_stack->channel].regs_rank[new_stack->rank].regs_bank[new_stack
		                                        ->bank].row_buffer_hits++;
		                                channel[new_stack->channel].regs_rank[new_stack->rank].row_buffer_hits++;
		                                channel[new_stack->channel].row_buffer_hits++;

		                                if(new_stack->prefetch)
		                                {
		                                        channel[new_stack->channel].regs_rank[new_stack->rank].regs_bank[new_stack->bank].row_buffer_hits_pref++;
		                                        channel[new_stack->channel].regs_rank[new_stack->rank].row_buffer_hits_pref++;
		                                        channel[new_stack->channel].row_buffer_hits_pref++;
		                                }else{

							channel[new_stack->channel].regs_rank[new_stack->rank].regs_bank[new_stack->bank].row_buffer_hits_normal++;
                                                channel[new_stack->channel].regs_rank[new_stack->rank].row_buffer_hits_normal++;
                                                channel[new_stack->channel].row_buffer_hits_normal++;
                                        }
                                }
                                else{/* MISS */}

                                /* Reserve the acces */
                                if(!mem_controller->photonic_net) //if we use a photonic net, it can tranfer in full directions
                                        channel[new_stack->channel].state = channel_state_busy;

                                channel[new_stack->channel].regs_rank[new_stack->rank].regs_bank[new_stack->bank].is_been_accesed = 1;
                                channel[new_stack->channel].regs_rank[new_stack->rank].is_been_accesed = 1;

                                normal_queue = mem_controller->normal_queue[0];
                                pref_queue = mem_controller->pref_queue[0];


                                /* Stadistics */
                                time = esim_cycle() - mem_controller->last_cycle;
                                if (linked_list_count(pref_queue->queue) < size_queue)
                                        pref_queue->total_requests += linked_list_count(pref_queue->queue) * time;
                                else // queue full, add the limit
                                        pref_queue->total_requests += size_queue * time;
                                if (linked_list_count(normal_queue->queue) < size_queue)
                                        normal_queue->total_requests += linked_list_count(normal_queue->queue) * time;
                                else // queue full, add the limit
                                        normal_queue->total_requests += size_queue * time;
                                mem_controller->last_cycle=esim_cycle();
                                mem_controller->n_times_queue_examined += time; //se podria cambiar dividinto x esim al final
                                mem_controller->t_acces_main_memory += mem_controller->t_send_request*cycles_proc_by_bus;

                                if(new_stack->prefetch)
                                        mem_controller->t_pref_acces_main_memory += mem_controller->t_send_request*cycles_proc_by_bus;
                                else
                                        mem_controller->t_normal_acces_main_memory += mem_controller->t_send_request*cycles_proc_by_bus;

				 /* Delete the request in the MC queue */
                                if (mem_controller_remove(new_stack, normal_queue)&&linked_list_count(normal_queue->queue) == size_queue - 1)
                                        normal_queue->t_full += esim_cycle() - normal_queue->instant_begin_full;
                                else if (mem_controller_remove(new_stack, pref_queue)&&linked_list_count(pref_queue->queue)==size_queue - 1)
                                        pref_queue->t_full += esim_cycle() - pref_queue->instant_begin_full;

                                /* Send the request to the bank */
                                esim_schedule_event(EV_MOD_NMOESI_ACCES_BANK, new_stack, mem_controller->t_send_request*cycles_proc_by_bus);
                        }
                        new_stack = mem_controller_select_request(0, mem_controller->priority_request_in_queue,mem_controller);
                }

                /* Stadistics */
                normal = mem_controller->normal_queue[0]->queue;
                pref = mem_controller->pref_queue[0]->queue;

                linked_list_head(normal);
                while (!linked_list_is_end(normal) && linked_list_current(normal) < size_queue)
                {
                        stack = linked_list_get(normal);
                        row_buffer_find_row(mem_controller, stack->mod, stack->addr,&stack->channel,&stack->rank,&stack->bank,NULL,NULL,NULL);
                        if ( channel[stack->channel].state == channel_state_busy)
                        {
                                channel[stack->channel].t_wait_channel_busy += cycles_proc_by_bus;
                                channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                mem_controller->t_wait+=cycles_proc_by_bus;

                                if(!stack->prefetch)// if it's only one queue, prefetches can be in the normal queue
                                {
                                        channel[stack->channel].t_normal_wait_channel_busy+=cycles_proc_by_bus;
                                        channel[stack->channel].t_normal_wait_send_request+=cycles_proc_by_bus;
                                        mem_controller->t_normal_wait+=cycles_proc_by_bus;
                                }
                                else
                                {
                                        channel[stack->channel].t_pref_wait_channel_busy+=cycles_proc_by_bus;
                                        channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                        mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                }
                        }
			else if(channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].is_been_accesed)
                        {
                                channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                mem_controller->t_wait+=cycles_proc_by_bus;
                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].conflicts++;
                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_wait += cycles_proc_by_bus;
                                if(!stack->prefetch) // if it's only one queue, prefetches can be in the normal queue
                                {
                                        mem_controller->t_normal_wait+=cycles_proc_by_bus;
                                        channel[stack->channel].t_normal_wait_send_request+=cycles_proc_by_bus;
                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_normal_wait+=cycles_proc_by_bus;
                                }
                                else
                                {
                                        mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                        channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_pref_wait+=cycles_proc_by_bus;
                                }
                        }
                        linked_list_next(normal);
                }
		linked_list_head(pref);
                while (!linked_list_is_end(pref) && linked_list_current(pref) < size_queue)
                {
                        stack = linked_list_get(pref);
                        row_buffer_find_row(mem_controller, stack->mod, stack->addr,&stack->channel,&stack->rank,&stack->bank,NULL,NULL,NULL);
                        if (channel[stack->channel].state == channel_state_busy)
                        {
                                mem_controller->t_wait+=cycles_proc_by_bus;
                                mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                channel[stack->channel].t_wait_channel_busy += cycles_proc_by_bus;
                                channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                channel[stack->channel].t_pref_wait_channel_busy+=cycles_proc_by_bus;
                                channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                        }
                        else if(channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].is_been_accesed)
                        {
                                mem_controller->t_wait+=cycles_proc_by_bus;
                                mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].conflicts++;
                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_wait += cycles_proc_by_bus;
                                channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_pref_wait += cycles_proc_by_bus;
                        }
                        linked_list_next(pref);
                }

                /* One cycle less (memory bus clock) that ALL request have to wait before they throw */
                mem_controller_update_requests_threshold(1, mem_controller);

                if ((mem_controller_stacks_normalQueues_count(mem_controller) + mem_controller_stacks_prefQueues_count(mem_controller)) > 0)
                        esim_schedule_event(EV_MOD_NMOESI_EXAMINE_ONLY_ONE_QUEUE_REQUEST, stack_orig, cycles_proc_by_bus);
                else
                        free(stack_orig);

                return;
        }

	if (event == EV_MOD_NMOESI_EXAMINE_QUEUE_REQUEST)
        {
                struct mod_stack_t *stack_orig = stack;


                while (queues_examined < num_queues)
                {
                        /* Depending of round robin priority, select apropiate bank to throw requests */
                        num_bank = mem_controller_get_bank_queue(queues_examined, mem_controller);
                        assert(num_bank >= 0 && num_bank < num_queues);
                        //dir_channel = num_bank % num_channels;
                        //assert(dir_channel >= 0 && dir_channel < num_channels);
                        dir_rank = (num_bank >>  log_base2(num_banks)) % mem_controller->num_regs_rank;
                        assert(dir_rank >= 0 && dir_rank < num_ranks);
                        dir_bank = num_bank % mem_controller->num_regs_bank;
                        assert(dir_bank >= 0 && dir_bank < num_banks);

                        normal_queue = mem_controller->normal_queue[num_bank];
                        pref_queue = mem_controller->pref_queue[num_bank];

                        new_stack = mem_controller_select_request(num_bank, mem_controller->priority_request_in_queue, mem_controller);
                        if (new_stack == NULL)
                        {
                                /* Next queue */
                                queues_examined++;

                                /* Stadistics */
                                linked_list_head(normal_queue->queue);
                                while (!linked_list_is_end(normal_queue->queue) && linked_list_current(normal_queue->queue) < size_queue)
                                {
                                        stack = linked_list_get(normal_queue->queue);
                                        row_buffer_find_row(mem_controller,stack->mod, stack->addr,&stack->channel,&stack->rank,&stack->bank,NULL,NULL,NULL);
                                        if ( channel[stack->channel].state == channel_state_busy)
                                        {
                                                channel[stack->channel].t_wait_channel_busy += cycles_proc_by_bus;
                                                channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                                mem_controller->t_wait+=cycles_proc_by_bus;
                                                if(!stack->prefetch)// if it's only one queue, prefetches can be in the normal queue
                                                {
							channel[stack->channel].t_normal_wait_channel_busy+=cycles_proc_by_bus;
                                                        channel[stack->channel].t_normal_wait_send_request+=cycles_proc_by_bus;
                                                        mem_controller->t_normal_wait+=cycles_proc_by_bus;
                                                }
                                                else
                                                {
                                                        mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                                        channel[stack->channel].t_pref_wait_channel_busy+=cycles_proc_by_bus;
                                                        channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                                }
                                        }
                                        else if(channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].is_been_accesed)
                                        {
                                                mem_controller->t_wait+=cycles_proc_by_bus;
                                                channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].conflicts++;
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_wait += cycles_proc_by_bus;
                                                if(!stack->prefetch) // if it's only one queue, prefetches can be in the normal queue
                                                {
                                                        mem_controller->t_normal_wait+=cycles_proc_by_bus;
                                                        channel[stack->channel].t_normal_wait_send_request+=cycles_proc_by_bus;
                                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_normal_wait+=cycles_proc_by_bus;
                                                }
                                                else
                                                {
                                                        mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                                        channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_pref_wait+=cycles_proc_by_bus;
                                                }
                                        }
                                        linked_list_next(normal_queue->queue);
                                }


				linked_list_head(pref_queue->queue);
                                while (!linked_list_is_end(pref_queue->queue) && linked_list_current(pref_queue->queue) < size_queue)
                                {
                                        stack = linked_list_get(pref_queue->queue);
                                        row_buffer_find_row(mem_controller,stack->mod, stack->addr,&stack->channel,&stack->rank,&stack->bank,NULL,NULL,NULL);
                                        if ( channel[stack->channel].state == channel_state_busy)
                                        {
                                                mem_controller->t_wait+=cycles_proc_by_bus;
                                                mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                                channel[stack->channel].t_wait_channel_busy += cycles_proc_by_bus;
                                                channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                                channel[stack->channel].t_pref_wait_channel_busy += cycles_proc_by_bus;
                                                channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                        }
                                        else if(channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].is_been_accesed)
                                        {
                                                mem_controller->t_wait+=cycles_proc_by_bus;
                                                mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                                channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].conflicts++;
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_wait+= cycles_proc_by_bus;
                                                channel[stack->channel].t_pref_wait_send_request += cycles_proc_by_bus;
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_pref_wait+=cycles_proc_by_bus;
                                        }
                                        linked_list_next(pref_queue->queue);
                                }
                                continue;
                        }

                        can_acces_bank = row_buffer_find_row(mem_controller,new_stack->mod, new_stack->addr, &new_stack->channel,
                                &new_stack->rank, &new_stack->bank, &new_stack->row, & new_stack->tag, &new_stack->state_main_memory);
                        mem_trace("mem.access name=\"A-%lld\" state=\"%s:examine queue MC\"\n", new_stack->id,new_stack->mod->name);
                        mem_debug("  %lld %lld 0x%x %s examine queue MC (row=%d bank=%d rank=%d channel=%d)\n",
                                esim_time, new_stack->id, new_stack->addr&~new_stack->mod->cache->block_mask,
                                new_stack->mod->name, new_stack->row, new_stack->bank, new_stack->rank, new_stack->channel);
                        assert(dir_bank == new_stack->bank);
                        assert( dir_rank == new_stack->rank);

			 /* We can serve the request */
                        if (can_acces_bank)
                        {

                                /* Reserve the acces */
                                if(!mem_controller->photonic_net) //if we use a photonic net, it can tranfer in full directions
                                        channel[new_stack->channel].state = channel_state_busy;
                                channel[new_stack->channel].regs_rank[new_stack->rank].regs_bank[new_stack->bank].is_been_accesed = 1;
                                channel[new_stack->channel].regs_rank[new_stack->rank].is_been_accesed = 1;

                                /* Stadistics */
                                time = esim_cycle() - mem_controller->last_cycle;
                                for (int i = 0; i < num_queues; i++)
                                {
                                        struct mem_controller_queue_t* preq_aux = mem_controller->pref_queue[i];
                                        struct mem_controller_queue_t* normq_aux = mem_controller->normal_queue[i];

                                        if (linked_list_count(preq_aux->queue) < size_queue)
                                                preq_aux->total_requests += linked_list_count(preq_aux->queue) * time;
                                        else // queue full, add the limit
                                                preq_aux->total_requests += size_queue * time;
                                        if (linked_list_count(normq_aux->queue) < size_queue)
                                                normq_aux->total_requests += linked_list_count(normq_aux->queue) * time;
                                        else // queue full, add the limit
                                                normq_aux->total_requests += size_queue * time;
                                }
                                mem_controller->n_times_queue_examined += time; //se podria cambiar dividinto x esim al final
                                /* Update last cycle queue will be examined */
                                mem_controller->last_cycle = esim_cycle();

                                /* Delete the request in the MC queue */
                                if (mem_controller_remove(new_stack, normal_queue)&&linked_list_count(normal_queue->queue) == size_queue - 1)
                                        normal_queue->t_full += esim_cycle() - normal_queue->instant_begin_full;
                                else if (mem_controller_remove(new_stack,pref_queue)&&linked_list_count(pref_queue->queue) == size_queue - 1)
                                        pref_queue->t_full += esim_cycle() - pref_queue->instant_begin_full;


				 /*Coalesce stacks whose bocks will be in the row buffer
                                  when stack will get its block from row buffer*/
                                if(mem_controller->coalesce!=policy_coalesce_disabled)
                                {
                                        int n_coalesce=mem_controller_coalesce_acces_row_buffer(new_stack, pref_queue->queue);
                                        if (mem_controller_remove(new_stack,pref_queue)&&linked_list_count(pref_queue->queue) == size_queue -n_coalesce)
                                                pref_queue->t_full += esim_cycle() - pref_queue->instant_begin_full;

                                        n_coalesce=mem_controller_coalesce_acces_row_buffer(new_stack, normal_queue->queue);
                                        if (mem_controller_remove(new_stack, normal_queue)&&linked_list_count(normal_queue->queue) == size_queue-n_coalesce)
                                                normal_queue->t_full += esim_cycle() - normal_queue->instant_begin_full;


                                }

                                mem_controller->t_acces_main_memory += mem_controller->t_send_request*cycles_proc_by_bus;

                                if(new_stack->prefetch)
                                        mem_controller->t_pref_acces_main_memory += mem_controller->t_send_request*cycles_proc_by_bus;
                                else
                                        mem_controller->t_normal_acces_main_memory += mem_controller->t_send_request*cycles_proc_by_bus;


                                /* Send the request to the bank */
                                esim_schedule_event(EV_MOD_NMOESI_ACCES_BANK, new_stack, mem_controller->t_send_request* cycles_proc_by_bus);
                        }

                        /* Next queue */
                        queues_examined++;

          		/* Stadistics */
                        linked_list_head(normal_queue->queue);
                        while (!linked_list_is_end(normal_queue->queue) && linked_list_current(normal_queue->queue) < size_queue)
                        {
                                stack = linked_list_get(normal_queue->queue);
                                row_buffer_find_row(mem_controller,stack->mod, stack->addr,&stack->channel,&stack->rank,&stack->bank,NULL,NULL,NULL);
                                if ( channel[stack->channel].state == channel_state_busy)
                                {
                                        mem_controller->t_wait+=cycles_proc_by_bus;
                                        channel[stack->channel].t_wait_channel_busy += cycles_proc_by_bus;
                                        channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                        if(!stack->prefetch) // if it's only one queue, prefetches can be in normal queue
                                        {
                                                mem_controller->t_normal_wait+=cycles_proc_by_bus;
                                                channel[stack->channel].t_normal_wait_channel_busy += cycles_proc_by_bus;
                                                channel[stack->channel].t_normal_wait_send_request+=cycles_proc_by_bus;
                                        }
                                        else
                                        {
                                                mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                                channel[stack->channel].t_pref_wait_channel_busy += cycles_proc_by_bus;
                                                channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                        }
                                }
                                else if(channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].is_been_accesed)
                                {
                                        mem_controller->t_wait+=cycles_proc_by_bus;
                                        channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].conflicts++;
                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_wait += cycles_proc_by_bus;
                                        if(!stack->prefetch) // if it's only one queue, prefetches can be in normal queue
                                        {
                                                mem_controller->t_normal_wait+=cycles_proc_by_bus;
                                                channel[stack->channel].t_normal_wait_send_request+=cycles_proc_by_bus;
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_normal_wait+=cycles_proc_by_bus;
                                        }
                                        else
                                        {
						 mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                                channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_pref_wait+=cycles_proc_by_bus;
                                        }
                                }
                                linked_list_next(normal_queue->queue);
                        }

                        linked_list_head(pref_queue->queue);
                        while (!linked_list_is_end(pref_queue->queue) && linked_list_current(pref_queue->queue) < size_queue)
                        {
                                stack = linked_list_get(pref_queue->queue);
                                row_buffer_find_row(mem_controller,stack->mod, stack->addr,&stack->channel,&stack->rank,&stack->bank,NULL,NULL,NULL);
                                if (channel[stack->channel].state == channel_state_busy)
                                {
                                        mem_controller->t_wait+=cycles_proc_by_bus;
                                        mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                        channel[stack->channel].t_wait_channel_busy += cycles_proc_by_bus;
                                        channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                        channel[stack->channel].t_pref_wait_channel_busy += cycles_proc_by_bus;
                                        channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                }
                                else if(channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].is_been_accesed)
                                {
                                        mem_controller->t_wait+=cycles_proc_by_bus;
                                        mem_controller->t_pref_wait+=cycles_proc_by_bus;
                                        channel[stack->channel].t_wait_send_request+=cycles_proc_by_bus;
                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].conflicts++;
                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_wait += cycles_proc_by_bus;
                                        channel[stack->channel].t_pref_wait_send_request+=cycles_proc_by_bus;
                                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].t_pref_wait += cycles_proc_by_bus;
                                }
                                linked_list_next(pref_queue->queue);
                        }
                }

		/* Update round robin to next queue */
                mem_controller->queue_round_robin = (mem_controller->queue_round_robin + 1) % num_queues;

                /* One cycle less (memory bus clock )that ALL request have to wait before they throw */
                mem_controller_update_requests_threshold(1, mem_controller);


                if ((mem_controller_stacks_normalQueues_count(mem_controller) + mem_controller_stacks_prefQueues_count(mem_controller)) > 0)
                        esim_schedule_event(EV_MOD_NMOESI_EXAMINE_QUEUE_REQUEST, stack_orig, cycles_proc_by_bus);
                else
                        free(stack_orig);

                return;
        }

        if (event == EV_MOD_NMOESI_ACCES_BANK)  // the request has arrived until the rank and bank
        {

                struct mod_stack_t * stack_aux;
                int n_coalesce= stack->coalesced_stacks!=NULL ? linked_list_count(stack->coalesced_stacks): 0;
                bank = mem_controller->regs_channel[stack->channel].regs_rank[stack->rank].regs_bank;

                mem_debug("  %lld %lld 0x%x %s acces bank\n", esim_time, stack->id, stack->addr & ~mod->cache->block_mask, mod->name);
                mem_trace("mem.access name=\"A-%lld\" state=\"%s:acces bank\"\n", stack->id, mod->name);


                /* Free channel */
                mem_controller->regs_channel[stack->channel].state = channel_state_free;


                /* Acces the bank. Is the row in the row buffer? */
                if (stack->state_main_memory == row_buffer_hit)
                {
                        /* HIT */
                        mem_debug("  %lld %lld 0x%x %s hit row buffer acces bank\n", esim_time, stack->id,
                                stack->addr&~mod->cache->block_mask, mod->name);
                        esim_schedule_event(EV_MOD_NMOESI_TRANSFER_FROM_BANK, stack, bank[stack->bank].t_row_buffer_hit);
			/* Stadistics */
                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].row_buffer_hits+=1+ n_coalesce;
                        channel[stack->channel].regs_rank[stack->rank].row_buffer_hits+=1+ n_coalesce;
                        channel[stack->channel].row_buffer_hits+=1+ n_coalesce;
                        mem_controller->t_acces_main_memory += bank[stack->bank].t_row_buffer_hit*(1+n_coalesce);

                        if(stack->prefetch) // main request
                        {
                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].row_buffer_hits_pref+=1;
                                channel[stack->channel].regs_rank[stack->rank].row_buffer_hits_pref+=1;
                                channel[stack->channel].row_buffer_hits_pref+=1;
                                mem_controller->t_pref_acces_main_memory += bank[stack->bank].t_row_buffer_hit;
                        }else{
                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].row_buffer_hits_normal+=1;
                                channel[stack->channel].regs_rank[stack->rank].row_buffer_hits_normal+=1;
                                channel[stack->channel].row_buffer_hits_normal+=1;
                                mem_controller->t_normal_acces_main_memory += bank[stack->bank].t_row_buffer_hit;
                        }

                        if(stack->coalesced_stacks!=NULL) // coalesce requests
                        {
                                linked_list_head(stack->coalesced_stacks);
                                while(!linked_list_is_end(stack->coalesced_stacks))
                                {
                                        stack_aux=linked_list_get(stack->coalesced_stacks);

                                        if(stack_aux->prefetch)
                                        {
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].row_buffer_hits_pref++;
                                                channel[stack->channel].regs_rank[stack->rank].row_buffer_hits_pref+=1;
                                                channel[stack->channel].row_buffer_hits_pref+=1;
                                                mem_controller->t_pref_acces_main_memory += bank[stack->bank].t_row_buffer_hit;
                                        }else{
                                                channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].row_buffer_hits_normal++;
                                                channel[stack->channel].regs_rank[stack->rank].row_buffer_hits_normal+=1;
                                                channel[stack->channel].row_buffer_hits_normal+=1;
                                                mem_controller->t_normal_acces_main_memory += bank[stack->bank].t_row_buffer_hit;
                                        }
                                        linked_list_next(stack->coalesced_stacks);
                                }
			 }


                }
                else    /* MISS */
                {
                        mem_debug("  %lld %lld 0x%x %s miss row buffer acces bank\n", esim_time, stack->id,
                                stack->addr&~mod->cache->block_mask, mod->name);
                        esim_schedule_event(EV_MOD_NMOESI_TRANSFER_FROM_BANK, stack, bank[stack->bank].t_row_buffer_miss);

                        /* Stadistics */
                        mem_controller->t_acces_main_memory += bank[stack->bank].t_row_buffer_miss*(1+n_coalesce);
                        if(stack->prefetch) // main request
                                mem_controller->t_pref_acces_main_memory += bank[stack->bank].t_row_buffer_miss;
                        else
                                mem_controller->t_normal_acces_main_memory += bank[stack->bank].t_row_buffer_miss;

                        if(stack->coalesced_stacks!=NULL) // coalesce requests
                        {
                                LINKED_LIST_FOR_EACH(stack->coalesced_stacks)
                                {
                                        stack_aux=linked_list_get(stack->coalesced_stacks);
                                        if(stack_aux->prefetch)
                                                mem_controller->t_pref_acces_main_memory+=bank[stack->bank].t_row_buffer_miss;
                                        else
                                                mem_controller->t_normal_acces_main_memory += bank[stack->bank].t_row_buffer_miss;
                                }
                        }
                }

                /* Update the new row into row buffer */
                bank[stack->bank].row_buffer = stack->row;

                /* Stadistics */
                for (int i = 0; i < channel[stack->channel].regs_rank[stack->rank].num_regs_bank; i++)
                        if (bank[i].is_been_accesed)
                                bank[i].parallelism++;

                for (int i = 0; i < channel[stack->channel].num_regs_rank; i++)
                        if (channel[stack->channel].regs_rank[i].is_been_accesed)
                        	 channel[stack->channel].regs_rank[i].parallelism++;

                /*bank[stack->bank].acceses+=1+ n_coalesce;
                channel[stack->channel].regs_rank[stack->rank].acceses+=1+ n_coalesce;
                channel[stack->channel].acceses+=1+ n_coalesce;*/
                mem_controller->non_coalesced_accesses++;
                /*if(stack->prefetch) // main request
                {
                        bank[stack->bank].pref_accesses+=1;
                        channel[stack->channel].regs_rank[stack->rank].pref_accesses+=1;
                        channel[stack->channel].pref_accesses+=1;
                }
                else
                {
                        bank[stack->bank].normal_accesses+=1;
                        channel[stack->channel].regs_rank[stack->rank].normal_accesses+=1;
                        channel[stack->channel].normal_accesses+=1;
                }

                if(stack->coalesced_stacks!=NULL) // coalesce requests
                {
                        linked_list_head(stack->coalesced_stacks);
                        while(!linked_list_is_end(stack->coalesced_stacks))
                        {
                                stack_aux=linked_list_get(stack->coalesced_stacks);
                                if(stack_aux->prefetch)
                                {
                                        bank[stack->bank].pref_accesses+=1;
                                        channel[stack->channel].regs_rank[stack->rank].pref_accesses+=1;
                                        channel[stack->channel].pref_accesses+=1;
                                }else{
                                        bank[stack->bank].normal_accesses+=1;
                                        channel[stack->channel].regs_rank[stack->rank].normal_accesses+=1;
                                        channel[stack->channel].normal_accesses+=1;
                                }
                                linked_list_next(stack->coalesced_stacks);
                        }
                }*/
                return;
        }

	if (event == EV_MOD_NMOESI_TRANSFER_FROM_BANK)
        {
                int num_blocks;
                struct mod_stack_t * stack_aux;
                struct linked_list_t * coalesced_stacks;
                int n_coalesce= stack->coalesced_stacks!=NULL ? linked_list_count(stack->coalesced_stacks): 0;

                mem_trace("mem.access name=\"A-%lld\" state=\"%s:transfer from bank\"\n", stack->id, mod->name);

                /* Calcul the cycles of proccesor for transfering one block*/
                if(mem_controller->photonic_net)
                        t_trans=1;
                else
                        t_trans = mod->cache->block_size / (channel[stack->channel].bandwith * 2) * cycles_proc_by_bus; //2 because is DDR3


                if (channel[stack->channel].state == channel_state_free)
                {
                        mem_debug("  %lld %lld 0x%x %s transfer from bank\n", esim_time, stack->id,stack->addr&~mod->cache->block_mask, mod->name);
                        /* Busy the channel */
                        if(!mem_controller->photonic_net) //if we use a photonic net, it can tranfer in full directions
                                channel[stack->channel].state = channel_state_busy;

                        if(mem_controller->coalesce== policy_coalesce_disabled || stack->coalesced_stacks==NULL)
                                num_blocks=1;
                        else{
                                /*Transfer all blocks that have coalesced*/
                                if(mem_controller->coalesce == policy_coalesce || mem_controller->coalesce ==
                                policy_coalesce_delayed_request) //tranfer all blocks between min and max block
                                        num_blocks=mem_controller_calcul_number_blocks_transfered(stack);
                                else
                                        num_blocks=linked_list_count(stack->coalesced_stacks)+1;//transfer only blocks required (this and coalesced)
                                assert(num_blocks>0 && num_blocks <= mem_controller->row_buffer_size/mod->cache->block_size);
                                mem_debug("    blocks transfered = %d  coalesced requests=%d\n",num_blocks,n_coalesce);

                                //mem_controller->useful_blocks_transfered+=1+n_coalesce;
                        }
		/* Stadistics */
                        mem_controller->blocks_transfered+=num_blocks;
                        mem_controller->t_transfer += t_trans*(1+n_coalesce);
                        channel[stack->channel].t_transfer += t_trans*(1+n_coalesce);
                        if(stack->prefetch)      mem_controller->t_pref_transfer += t_trans;
                        else    mem_controller->t_normal_transfer += t_trans;

                        if(stack->coalesced_stacks!=NULL)
                        {
                                LINKED_LIST_FOR_EACH(stack->coalesced_stacks)
                                {
                                        stack_aux=linked_list_get(stack->coalesced_stacks);
                                        if(stack_aux->prefetch) mem_controller->t_pref_transfer += t_trans;
                                        else mem_controller->t_normal_transfer += t_trans;
                                }
                        }

                        /*Transfer sorted blocks*/
                        if(stack->coalesced_stacks!=NULL && linked_list_count(stack->coalesced_stacks)>0)
                        {
                                mem_controller_sort_by_block(stack);
                                coalesced_stacks=stack->coalesced_stacks;
                                stack->coalesced_stacks=NULL;
                                int add=1;

                                linked_list_head(coalesced_stacks);
                                while(!linked_list_is_end(coalesced_stacks))
                                {
                                        stack_aux=linked_list_get(coalesced_stacks);
                                        if(stack->addr % mem_controller->row_buffer_size < stack_aux->addr % mem_controller->row_buffer_size)
                                        {
                                                linked_list_insert(coalesced_stacks,stack);
                                                add=0;
                                                break;
                                        }
                                        linked_list_next(coalesced_stacks);
                                }
                                if(add)linked_list_insert(coalesced_stacks,stack);

                                mem_controller_count_successive_hits(coalesced_stacks);
				linked_list_head(coalesced_stacks);
                                new_stack=linked_list_get(coalesced_stacks);
                                linked_list_remove(coalesced_stacks);
                                //if(new_stack->coalesced_stacks!=NULL) linked_list_free(new_stack->coalesced_stacks);
                                new_stack->coalesced_stacks=coalesced_stacks;
                                new_stack->bank=stack->bank;
                                new_stack->rank=stack->rank;
                                new_stack->channel=stack->channel;
                                new_stack->row=stack->row;
                                esim_schedule_event(EV_MOD_NMOESI_REMOVE_MEMORY_CONTROLLER, new_stack, t_trans);
                                //linked_list_free(coalesced_stacks);
                                assert(new_stack->coalesced_stacks!=NULL && linked_list_count(new_stack->coalesced_stacks)>0);
                                return;
                        }else{
                                mem_controller->burst_size[0]++;
                                mem_controller->successive_hit[0][0]++;
                        }

                        esim_schedule_event(EV_MOD_NMOESI_REMOVE_MEMORY_CONTROLLER, stack, t_trans);
                        return;
                }
                else
                {
                        mem_debug("  %lld %lld 0x%x %s wait for transfer from bank\n", esim_time, stack->id, stack->addr&~mod->cache->block_mask, mod->name);

                        /* We can't transfer the block because the channed is busy, so we have to wait */
                        esim_schedule_event(EV_MOD_NMOESI_TRANSFER_FROM_BANK, stack, 1);

                        /* Stadistics */
                        channel[stack->channel].t_wait_transfer_request += 1+n_coalesce;
                        mem_controller->t_transfer += 1+n_coalesce;
                        if(stack->prefetch){
                                channel[stack->channel].t_pref_wait_transfer_request += 1;
                                mem_controller->t_pref_transfer += 1;
                        }else{
                                channel[stack->channel].t_normal_wait_transfer_request += 1;
                                mem_controller->t_normal_transfer += 1;
                        }
			/*Coalesce request stadistics*/
                        if(stack->coalesced_stacks!=NULL)
                        {
                                LINKED_LIST_FOR_EACH(stack->coalesced_stacks)
                                {
                                        stack_aux=linked_list_get(stack->coalesced_stacks);
                                        if(stack_aux->prefetch){
                                                channel[stack->channel].t_pref_wait_transfer_request += 1; //go to the same channel
                                                mem_controller->t_pref_transfer += 1;
                                        }else{
                                                channel[stack->channel].t_normal_wait_transfer_request += 1;
                                                mem_controller->t_normal_transfer += 1;
                                        }
                                }
                        }
                }
                return;
        }


        if (event == EV_MOD_NMOESI_REMOVE_MEMORY_CONTROLLER)
        {
                struct mod_stack_t * stack_aux;
                int n_coalesce=0;
                struct mem_controller_queue_t* normq_aux, *prefq_aux;
                bank = channel[stack->channel].regs_rank[stack->rank].regs_bank;
                int accesed = 0;
                unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
                int t_trans;
                unsigned int min_block;
                unsigned int max_block;
                unsigned int current_block;

                mem_debug("  %lld %lld 0x%x %s remove request in MC\n ",esim_time,stack->id, stack->addr&~mod->cache->block_mask,mod->name);
                mem_trace("mem.access name=\"A-%lld\" state=\"%s:remove request in MC\"\n", stack->id, mod->name);

                if(mem_controller->photonic_net) // all blocks arrive at the same tiem
                        t_trans=0;
                else
                        t_trans=mod->cache->block_size/(channel[stack->channel].bandwith*2)*cycles_proc_by_bus;

		/*Free when all coalesced request have arrived*/
                if(stack->coalesced_stacks==NULL || linked_list_count(stack->coalesced_stacks)==0)
                {
                        /* Free the bank */
                        channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank].is_been_accesed = 0;

                        /* Free the rank */
                        for (int i = 0; i < channel[stack->channel].regs_rank[stack->rank].num_regs_bank; i++)
                                if (channel[stack->channel].regs_rank[stack->rank].regs_bank[i].is_been_accesed)
                                {
                                        accesed = 1;
                                        break;
                                }
                        if (!accesed) channel[stack->channel].regs_rank[stack->rank].is_been_accesed = 0;

                        /* Free channel*/
                        channel[stack->channel].state = channel_state_free;
                }

                /*Coalesced delayed requests*/
                if(mem_controller->coalesce == policy_coalesce_delayed_request&& stack->coalesced_stacks!=NULL)
                {
                        assert(mem_controller->queue_per_bank);
                        int q=((stack->addr>> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));

                        /*Check if any request in MC queue goes to the same row in the bank,
                        and if the block is transfering*/
                        min_block=mem_controller_min_block(stack);
                        max_block=mem_controller_max_block(stack);

                        n_coalesce=mem_controller_coalesce_acces_between_blocks(stack,mem_controller->pref_queue[q]->queue, min_block,max_block);
                        if (linked_list_count(mem_controller->pref_queue[q]->queue) == size_queue - n_coalesce &&n_coalesce>0)
                                mem_controller->pref_queue[q]->t_full += esim_cycle() - mem_controller->pref_queue[q]->instant_begin_full;

                        n_coalesce=mem_controller_coalesce_acces_between_blocks(stack,mem_controller->normal_queue[q]->queue, min_block,max_block);
                        if (linked_list_count(mem_controller->normal_queue[q]->queue) == size_queue - n_coalesce &&n_coalesce>0)
                                mem_controller->normal_queue[q]->t_full += esim_cycle() - mem_controller->normal_queue[q]->instant_begin_full;

			 /*Sort by block*/
                        mem_controller_sort_by_block(stack);

                        /*Stadistics*/
                        time = esim_cycle() - mem_controller->last_cycle;
                        for (int i = 0; i < num_queues; i++)
                        {
                                normq_aux = mem_controller->normal_queue[i];

                                if (linked_list_count(normq_aux->queue) < size_queue)
                                        normq_aux->total_requests += linked_list_count(normq_aux->queue) * time;
                                else // queue full, add the limit
                                        normq_aux->total_requests += size_queue * time;

                                prefq_aux = mem_controller->pref_queue[i];

                                if (linked_list_count(prefq_aux->queue) < size_queue)
                                        prefq_aux->total_requests += linked_list_count(prefq_aux->queue) * time;
                                else // queue full, add the limit
                                        prefq_aux->total_requests += size_queue * time;
                        }
                        mem_controller->n_times_queue_examined += time; //se podria cambiar dividinto x esim al final
                        mem_controller->last_cycle = esim_cycle();


                }

                /*Program the next arrival of a block coalesced*/
                if(stack->coalesced_stacks!=NULL && linked_list_count(stack->coalesced_stacks)>0)
                {
                        current_block=stack->addr %  mem_controller->row_buffer_size;
                        linked_list_head(stack->coalesced_stacks);
                        new_stack=linked_list_get(stack->coalesced_stacks);

                        /*Delete this current acces*/
                        linked_list_remove(stack->coalesced_stacks);
                        if(new_stack->coalesced_stacks!=NULL) linked_list_free(new_stack->coalesced_stacks);
                        new_stack->coalesced_stacks=stack->coalesced_stacks;
                        stack->coalesced_stacks=NULL;
                        assert(new_stack->coalesced_stacks!=NULL);
			 new_stack->bank=stack->bank;
                        new_stack->rank=stack->rank;
                        new_stack->channel=stack->channel;
                        new_stack->row=stack->row;
                        n_coalesce = linked_list_count(new_stack->coalesced_stacks);

                        if(mem_controller->coalesce==policy_coalesce_useful_blocks)
                                esim_schedule_event(EV_MOD_NMOESI_REMOVE_MEMORY_CONTROLLER, new_stack, t_trans);
                        else
                        {
                                unsigned int next_block=new_stack->addr %  mem_controller->row_buffer_size;
                                int num_blocks=(next_block-current_block)/stack->mod->cache->block_size;
                                assert(num_blocks>0);
                                esim_schedule_event(EV_MOD_NMOESI_REMOVE_MEMORY_CONTROLLER, new_stack, t_trans*num_blocks);
                        }

                        /*Stadistics*/
                        mem_controller->t_transfer += t_trans*(1+n_coalesce);
                        channel[stack->channel].t_transfer += t_trans*(1+n_coalesce);
                        if(new_stack->prefetch)  mem_controller->t_pref_transfer += t_trans;
                        else    mem_controller->t_normal_transfer += t_trans;

                        LINKED_LIST_FOR_EACH(new_stack->coalesced_stacks)
                        {
                                stack_aux=linked_list_get(new_stack->coalesced_stacks);
                                if(stack_aux->prefetch)
                                        mem_controller->t_pref_transfer += t_trans;
                                else
                                        mem_controller->t_normal_transfer += t_trans;
                        }
                }


                /*FInish the acces*/
                /* Stadistics */
                bank[stack->bank].acceses++;
                channel[stack->channel].regs_rank[stack->rank].acceses++;
                channel[stack->channel].acceses++;
                mem_controller->useful_blocks_transfered++;
                channel[stack->channel].num_requests_transfered+=1;//nked_list_count(coalesced_stacks);

		 if(stack->prefetch)
                {
                        bank[stack->bank].pref_accesses++;
                        channel[stack->channel].regs_rank[stack->rank].pref_accesses++;
                        channel[stack->channel].pref_accesses++;
                        channel[stack->channel].num_pref_requests_transfered++;
                }
                else
                {
                        bank[stack->bank].normal_accesses++;
                        channel[stack->channel].regs_rank[stack->rank].normal_accesses++;
                        channel[stack->channel].normal_accesses++;
                        channel[stack->channel].num_normal_requests_transfered++;
                }


                /* Parametres block */
                stack->hit = 1;
                stack->port = NULL;

                if (stack->request_type == read_request) // read request
                        stack->state = cache_block_exclusive;
                else if (stack->request_type == write_request) //write request
                        stack->state = cache_block_exclusive;
                else fatal("Invalid main memory acces\n");

                stack->reply_size = 8;
                stack->tag=stack->addr&~mod->cache->block_mask;///////// si fique aso salta
                ret->err = 0;
                ret->state = stack->state;
                ret->tag = stack->tag;
                ret->reply_size = 8;
                ret->prefetch = stack->prefetch;
                mod_stack_return(stack);
                return;
        }

        abort();
}


void mod_handler_nmoesi_peer(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_t *src = stack->target_mod;
	struct mod_t *peer = stack->peer;

	if (event == EV_MOD_NMOESI_PEER_SEND)
	{
		mem_debug("  %lld %lld 0x%x %s %s peer send\n", esim_time, stack->id,
			stack->tag, src->name, peer->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:peer\"\n",
			stack->id, src->name);

		/* Send message from src to peer */
		stack->msg = net_try_send_ev(src->low_net, src->low_net_node, peer->low_net_node,
			src->block_size + 8, EV_MOD_NMOESI_PEER_RECEIVE, stack, event, stack);

		return;
	}

	if (event == EV_MOD_NMOESI_PEER_RECEIVE)
	{
		mem_debug("  %lld %lld 0x%x %s %s peer receive\n", esim_time, stack->id,
			stack->tag, src->name, peer->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:peer_receive\"\n",
			stack->id, peer->name);

		/* Receive message from src */
		net_receive(peer->low_net, peer->low_net_node, stack->msg);

		esim_schedule_event(EV_MOD_NMOESI_PEER_REPLY, stack, 0);

		return;
	}

	if (event == EV_MOD_NMOESI_PEER_REPLY)
	{
		mem_debug("  %lld %lld 0x%x %s %s peer reply ack\n", esim_time, stack->id,
			stack->tag, src->name, peer->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:peer_reply_ack\"\n",
			stack->id, peer->name);

		/* Send ack from peer to src */
		stack->msg = net_try_send_ev(peer->low_net, peer->low_net_node, src->low_net_node,
				8, EV_MOD_NMOESI_PEER_FINISH, stack, event, stack);

		return;
	}

	if (event == EV_MOD_NMOESI_PEER_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s %s peer finish\n", esim_time, stack->id,
			stack->tag, src->name, peer->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:peer_finish\"\n",
			stack->id, src->name);

		/* Receive message from src */
		net_receive(src->low_net, src->low_net_node, stack->msg);

		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_invalidate(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;

	struct dir_t *dir;
	struct dir_entry_t *dir_entry;

	uint32_t dir_entry_tag;
	uint32_t z;

	if (event == EV_MOD_NMOESI_INVALIDATE)
	{
		struct mod_t *sharer;
		int i;

		/* Get block info */
		cache_get_block(mod->cache, stack->set, stack->way, &stack->tag, &stack->state);
		mem_debug("  %lld %lld 0x%x %s invalidate (set=%d, way=%d, state=%s)\n", esim_time, stack->id,
			stack->tag, mod->name, stack->set, stack->way,
			str_map_value(&cache_block_state_map, stack->state));
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:invalidate\"\n",
			stack->id, mod->name);

		/* At least one pending reply */
		stack->pending = 1;

		/* Send write request to all upper level sharers except 'except_mod' */
		dir = mod->dir;
		for (z = 0; z < dir->zsize; z++)
		{
			dir_entry_tag = stack->tag + z * mod->sub_block_size;
			assert(dir_entry_tag < stack->tag + mod->block_size);
			dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
			for (i = 0; i < dir->num_nodes; i++)
			{
				struct net_node_t *node;

				/* Skip non-sharers and 'except_mod' */
				if (!dir_entry_is_sharer(dir, stack->set, stack->way, z, i))
					continue;

				node = list_get(mod->high_net->node_list, i);
				sharer = node->user_data;
				if (sharer == stack->except_mod)
					continue;

				/* Clear sharer and owner */
				dir_entry_clear_sharer(dir, stack->set, stack->way, z, i);
				if (dir_entry->owner == i)
					dir_entry_set_owner(dir, stack->set, stack->way, z, DIR_ENTRY_OWNER_NONE);

				/* Send write request upwards if beginning of block */
				if (dir_entry_tag % sharer->block_size)
					continue;
				new_stack = mod_stack_create(stack->id, mod, dir_entry_tag,
					EV_MOD_NMOESI_INVALIDATE_FINISH, stack, stack->prefetch);
				new_stack->target_mod = sharer;
				new_stack->request_dir = mod_request_down_up;

				esim_schedule_event(EV_MOD_NMOESI_WRITE_REQUEST, new_stack, 0);
				stack->pending++;
			}
		}
		esim_schedule_event(EV_MOD_NMOESI_INVALIDATE_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_INVALIDATE_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s invalidate finish\n", esim_time, stack->id,
			stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:invalidate_finish\"\n",
			stack->id, mod->name);

		if (stack->reply == reply_ack_data)
			cache_set_block(mod->cache, stack->set, stack->way, stack->tag,
				cache_block_modified);

		/* Ignore while pending */
		assert(stack->pending > 0);
		stack->pending--;
		if (stack->pending)
			return;
		mod_stack_return(stack);
		return;
	}

	abort();
}

void mod_handler_nmoesi_message(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;

	struct mod_t *mod = stack->mod;
	struct mod_t *target_mod = stack->target_mod;

	struct dir_t *dir;
	struct dir_entry_t *dir_entry;
	uint32_t z;

	if (event == EV_MOD_NMOESI_MESSAGE)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s message\n", esim_time, stack->id,
			stack->addr, mod->name);

		stack->reply_size = 8;
		stack->reply = reply_ack;

		/* Default return values*/
		ret->err = 0;

		/* Checks */
		assert(stack->message);

		/* Get source and destination nodes */
		net = mod->low_net;
		src_node = mod->low_net_node;
		dst_node = target_mod->high_net_node;

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, 8,
			EV_MOD_NMOESI_MESSAGE_RECEIVE, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_MESSAGE_RECEIVE)
	{
		mem_debug("  %lld %lld 0x%x %s message receive\n", esim_time, stack->id,
			stack->addr, target_mod->name);

		/* Receive message */
		net_receive(target_mod->high_net, target_mod->high_net_node, stack->msg);

		/* Find and lock */
		new_stack = mod_stack_create(stack->id, target_mod, stack->addr,
			EV_MOD_NMOESI_MESSAGE_ACTION, stack, stack->prefetch);
		new_stack->message = stack->message;
		new_stack->blocking = 0;
		new_stack->retry = 0;
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK, new_stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_MESSAGE_ACTION)
	{
		mem_debug("  %lld %lld 0x%x %s clear owner action\n", esim_time, stack->id,
			stack->tag, target_mod->name);

		assert(stack->message);

		/* Check block locking error. */
		mem_debug("stack err = %u\n", stack->err);
		if (stack->err)
		{
			ret->err = 1;
			mod_stack_set_reply(ret, reply_ack_error);
			esim_schedule_event(EV_MOD_NMOESI_MESSAGE_REPLY, stack, 0);
			return;
		}

		if (stack->message == message_clear_owner)
		{
			/* Remove owner */
			dir = target_mod->dir;
			for (z = 0; z < dir->zsize; z++)
			{
				/* Skip other subblocks */
				if (stack->addr == stack->tag + z * target_mod->sub_block_size)
				{
					/* Clear the owner */
					dir_entry = dir_entry_get(dir, stack->set, stack->way, z);
					assert(dir_entry->owner == mod->low_net_node->index);
					dir_entry_set_owner(dir, stack->set, stack->way, z,
						DIR_ENTRY_OWNER_NONE);
				}
			}

		}
		else
		{
			fatal("Unexpected message");
		}

		/* Unlock the directory entry */
		dir_entry_unlock(dir, stack->set, stack->way);

		esim_schedule_event(EV_MOD_NMOESI_MESSAGE_REPLY, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_MESSAGE_REPLY)
	{
		struct net_t *net;
		struct net_node_t *src_node;
		struct net_node_t *dst_node;

		mem_debug("  %lld %lld 0x%x %s message reply\n", esim_time, stack->id,
			stack->tag, target_mod->name);

		/* Checks */
		assert(mod_get_low_mod(mod, stack->addr) == target_mod ||
			mod_get_low_mod(target_mod, stack->addr) == mod);

		/* Get network and nodes */
		net = mod->low_net;
		src_node = target_mod->high_net_node;
		dst_node = mod->low_net_node;

		/* Send message */
		stack->msg = net_try_send_ev(net, src_node, dst_node, stack->reply_size,
			EV_MOD_NMOESI_MESSAGE_FINISH, stack, event, stack);
		return;
	}

	if (event == EV_MOD_NMOESI_MESSAGE_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s message finish\n", esim_time, stack->id,
			stack->tag, mod->name);

		/* Receive message */
		net_receive(mod->low_net, mod->low_net_node, stack->msg);

		/* Return */
		mod_stack_return(stack);
		return;
	}

	abort();
}


void mod_handler_nmoesi_find_and_lock_mem_controller(int event, void *data)
{
	struct mod_stack_t *stack = data;
	struct mod_stack_t *ret = stack->ret_stack;
	struct mod_stack_t *new_stack;
	struct mod_t *mod = stack->mod;

	assert(stack->request_dir);
	assert(mod->kind==mod_kind_main_memory);
	assert(stack->request_dir == mod_request_up_down);

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER)
	{
		mem_debug("  %lld %lld 0x%x %s find and lock memory controller (blocking=%d)\n",
			esim_time, stack->id, stack->addr, mod->name, stack->blocking);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_mem_controller\"\n",
			stack->id, mod->name);

		/* Default return values */
		ret->err = 0;

		stack->way = ret->way; /* If this stack has already been assigned a way, keep using it */
		mod_lock_port(mod, stack, EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_PORT);

		stack->hit = stack->stream_hit = 0;
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_PORT)
	{
		struct mod_port_t *port = stack->port;
		struct dir_lock_t *dir_lock;

		assert(stack->port);
		mem_debug("  %lld %lld 0x%x %s find and lock memory controller port \n", esim_time, stack->id,
			stack->addr, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_mem_controller_port\"\n",
			stack->id, mod->name);

		/* Set parent stack flag expressing that port has already been locked.
		 * This flag is checked by new writes to find out if it is already too
		 * late to coalesce. */
		ret->port_locked = 1;

		/* Look for block */
		stack->hit = mod_find_block(mod, stack->addr, &stack->set, &stack->way, &stack->tag, &stack->state);

		//assert(!prefetch);

		if (stack->hit)
			mem_debug("    %lld 0x%x %s hit mem controller directory : set=%d, way=%d, state=%s\n", stack->id, stack->tag, mod->name, stack->set, stack->way, str_map_value(&cache_block_state_map, stack->state));

		/* Statistics */
		mod->accesses++;
		if (stack->hit)
		{
			mod->hits++;
		}

		if (stack->read)
		{
			mod->reads++;
			mod->effective_reads++;
			stack->blocking ? mod->blocking_reads++ : mod->non_blocking_reads++;
			if (stack->hit)
				mod->read_hits++;
		}
		else if (stack->nc_write)  /* Must go after read */
		{
			mod->nc_writes++;
			mod->effective_nc_writes++;
			stack->blocking ? mod->blocking_nc_writes++ : mod->non_blocking_nc_writes++;
			if (stack->hit)
				mod->nc_write_hits++;
		}
		else if (stack->write)
		{
			mod->writes++;
			mod->effective_writes++;
			stack->blocking ? mod->blocking_writes++ : mod->non_blocking_writes++;

			/* Increment witness variable when port is locked */
			if (stack->witness_ptr)
			{
				(*stack->witness_ptr)++;
				stack->witness_ptr = NULL;
			}

			if (stack->hit)
				mod->write_hits++;
		}
		else if (stack->message)
		{
			/* FIXME */
		}
		else if (stack->prefetch)
		{
			/* FIXME */
		}
		else
		{
			fatal("Unknown memory operation type");
		}

		if (!stack->retry)
		{
			mod->no_retry_accesses++;
			if (stack->hit)
				mod->no_retry_hits++;

			if (stack->read)
			{
				mod->no_retry_reads++;
				if (stack->hit)
					mod->no_retry_read_hits++;
			}
			else if (stack->nc_write)  /* Must go after read */
			{
				mod->no_retry_nc_writes++;
				if (stack->hit)
					mod->no_retry_nc_write_hits++;
			}
			else if (stack->write)
			{
				mod->no_retry_writes++;
				if (stack->hit)
					mod->no_retry_write_hits++;
			}
			else if (stack->prefetch)
			{
				/* No retries currently for prefetches */
			}
			else if (stack->message)
			{
				/* FIXME */
			}
			else
			{
				fatal("Unknown memory operation type");
			}
		}

		if (!stack->hit)
		{
			if (stack->way < 0)
				stack->way = cache_replace_block(mod->cache, stack->set);

			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(stack->state || !dir_entry_group_shared_or_owned(mod->dir, stack->set, stack->way));
			mem_debug("    %lld 0x%x %s miss directory mem controller -> lru: set=%d, way=%d, state=%s\n", stack->id, stack->tag, mod->name, stack->set, stack->way, str_map_value(&cache_block_state_map, stack->state));
		}
		assert(stack->way >= 0);


		/* If directory entry is locked and the call to FIND_AND_LOCK is not
		 * blocking, release port and return error. */
		dir_lock = dir_lock_get(mod->dir, stack->set, stack->way);
		if (dir_lock->lock && !stack->blocking)
		{
			mem_debug("    %lld 0x%x %s block already locked: set=%d, way=%d already locked by stack %lld, retrying...\n", stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
			ret->err = 1;
			mod_unlock_port(mod, port, stack);
			ret->port_locked = 0;
			mod_stack_return(stack);
			return;
		}

		/* Lock directory entry. If lock fails, port needs to be released to prevent
		 * deadlock.  When the directory entry is released, locking port and
		 * directory entry will be retried. */
		if (!dir_entry_lock(mod->dir, stack->set, stack->way, EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER, stack))
		{
			mem_debug("    %lld 0x%x %s block locked at set=%d, way=%d already locked by stack %lld, waiting...\n", stack->id, stack->tag, mod->name, stack->set, stack->way, dir_lock->stack_id);
			mod_unlock_port(mod, port, stack);
			ret->port_locked = 0;
			return;
		}


		/* DIrectory entry is locked. Record the transient tag so that a subsequent lookup
		 * detects that the block is being brought.
		 * Also, update LRU counters here. */
		cache_set_transient_tag(mod->cache, stack->set, stack->way, stack->tag);
		cache_access_block(mod->cache, stack->set, stack->way);

		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_ACTION, stack, mod->dir_latency); /* Access latency */
		return;
	}



	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_ACTION)
	{
		struct mod_port_t *port = stack->port;

		assert(port);
		mem_debug("  %lld %lld 0x%x %s find and lock mem controller action\n", esim_time, stack->id,stack->tag, mod->name);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_mem_controller_action\"\n",stack->id, mod->name);

		/* Release port */
		mod_unlock_port(mod, port, stack);
		ret->port_locked = 0;

		/* On miss, evict if victim is a valid block. */
		if (!stack->hit && stack->state)
		{
			stack->eviction = 1;
			new_stack = mod_stack_create(stack->id, mod, 0, EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_FINISH, stack, stack->prefetch);
			new_stack->set = stack->set;
			new_stack->way = stack->way;
			esim_schedule_event(EV_MOD_NMOESI_EVICT, new_stack, 0);
			return;

		}

		/* Continue */
		esim_schedule_event(EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_FINISH, stack, 0);
		return;
	}

	if (event == EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_FINISH)
	{
		mem_debug("  %lld %lld 0x%x %s find and lock memory controller finish (err=%d fr=%d bg=%d)\n", esim_time, stack->id,stack->tag, mod->name, stack->err, stack->fast_resume, stack->background);
		mem_trace("mem.access name=\"A-%lld\" state=\"%s:find_and_lock_mem_controller_finish\"\n",stack->id, mod->name);

		/* If evict produced err, return err */
		if (stack->err)
		{
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(stack->state);
			assert(stack->eviction);
			ret->err = 1;
			dir_entry_unlock(mod->dir, stack->set, stack->way);
			mod_stack_return(stack);
			return;
		}

		/* Eviction */
		if (stack->eviction)
		{
			mod->evictions++;
			cache_get_block(mod->cache, stack->set, stack->way, NULL, &stack->state);
			assert(!stack->state);
		}

		/* A previous miss was just a miss in the directory. */
		if (!stack->state) // MIRAR-------------------
		{
			stack->state = cache_block_exclusive;
			cache_set_block(mod->cache, stack->set, stack->way, stack->tag, stack->state);
		}

		/* Return */
		ret->err = 0;
		ret->set = stack->set;
		ret->way = stack->way;
		ret->state = stack->state;
		ret->tag = stack->tag;
		mod_stack_return(stack);
		return;
	}

	abort();
}
