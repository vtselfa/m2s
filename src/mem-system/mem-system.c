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

#include <arch/common/arch.h>
#include <dramsim/bindings-c.h>
#include <lib/esim/esim.h>
#include <lib/esim/trace.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/file.h>
#include <lib/util/hash-table.h>
#include <lib/util/list.h>
#include <lib/util/linked-list.h>
#include <lib/util/string.h>
#include <network/network.h>

#include "bank.h"
#include "cache.h"
#include "channel.h"
#include "command.h"
#include "config.h"
#include "local-mem-protocol.h"
#include "mem-system.h"
#include "mem-controller.h"
#include "module.h"
#include "nmoesi-protocol.h"


/*
 * Global Variables
 */

int mem_debug_category;
int mem_trace_category;
int mem_peer_transfers;

int EV_MAIN_MEMORY_TIC;

/* Frequency domain, as returned by function 'esim_new_domain'. */
int mem_frequency = 3000;
int mem_domain_index;

struct mem_system_t *mem_system;

char *mem_report_file_name = "";
char *main_mem_report_file_name = "";


/*
 * Memory System Object
 */

struct mem_system_t *mem_system_create(void)
{
	struct mem_system_t *mem_system;

	/* Initialize */
	mem_system = xcalloc(1, sizeof(struct mem_system_t));
	mem_system->net_list = list_create();
	mem_system->mod_list = list_create();
	mem_system->mm_mod_list = list_create();
	mem_system->mem_controllers = linked_list_create();
	mem_system->pref_into_normal = linked_list_create();
	mem_system->dram_systems = hash_table_create(0, 0); /* Dram systems, if any */

	/* Return */
	return mem_system;
}


void mem_system_free(struct mem_system_t *mem_system)
{
	char *key;
	struct dram_system_t *dram_system;

	/* Free piggybaking */
	linked_list_head(mem_system->pref_into_normal);
	while(!linked_list_is_end(mem_system->pref_into_normal))
	{
		free(linked_list_get(mem_system->pref_into_normal));
		linked_list_remove(mem_system->pref_into_normal);
	}
	linked_list_free(mem_system->pref_into_normal);

	/* Free mem controllers */
	LINKED_LIST_FOR_EACH(mem_system->mem_controllers)
		mem_controller_free(linked_list_get(mem_system->mem_controllers));
	linked_list_free(mem_system->mem_controllers);

	/* Free memory modules */
	while (list_count(mem_system->mod_list))
		mod_free(list_pop(mem_system->mod_list));
	list_free(mem_system->mod_list);
	list_free(mem_system->mm_mod_list);

	/* Free networks */
	while (list_count(mem_system->net_list))
		net_free(list_pop(mem_system->net_list));
	list_free(mem_system->net_list);

	/* Free dram_systems */
	HASH_TABLE_FOR_EACH(mem_system->dram_systems, key, dram_system)
	{
		free(dram_system->name);
		assert(linked_list_count(dram_system->pending_reads) == 0);
		linked_list_free(dram_system->pending_reads);
		dram_system_free(dram_system->handler);
		free(dram_system);
	}
	hash_table_free(mem_system->dram_systems);

	/* Free memory system */
	free(mem_system);
}


/*
 * Public Functions
 */

static char *mem_err_timing =
	"\tA command-line option related with the memory hierarchy ('--mem' prefix)\n"
	"\thas been specified, by no architecture is running a detailed simulation.\n"
	"\tPlease specify at least one detailed simulation (e.g., with option\n"
	"\t'--x86-sim detailed'.\n";


void mem_system_init(void)
{
	int count;
	int i;

	/* If any file name was specific for a command-line option related with the
	 * memory hierarchy, make sure that at least one architecture is running
	 * timing simulation. */
	count = arch_get_sim_kind_detailed_count();
	if (mem_report_file_name && *mem_report_file_name && !count)
		fatal("memory report file given, but no timing simulation.\n%s",
				mem_err_timing);
	if (mem_config_file_name && *mem_config_file_name && !count)
		fatal("memory configuration file given, but no timing simulation.\n%s",
				mem_err_timing);

	/* Create trace category. This needs to be done before reading the
	 * memory configuration file with 'mem_config_read', since the latter
	 * function generates the trace headers. */
	mem_trace_category = trace_new_category();

	/* Create global memory system. This needs to be done before reading the
	 * memory configuration file with 'mem_config_read', since the latter
	 * function inserts caches and networks in 'mem_system', and relies on
	 * these lists to have been created. */
	mem_system = mem_system_create();

	/* Read memory configuration file */
	mem_config_read();

	/* Try to open report file */
	if (*mem_report_file_name && !file_can_open_for_write(mem_report_file_name))
		fatal("%s: cannot open GPU cache report file",
			mem_report_file_name);

	/* Create Frequency domain */
	mem_domain_index = esim_new_domain(mem_frequency);

	/* Event handler for memory hierarchy commands */
	EV_MEM_SYSTEM_COMMAND = esim_register_event_with_name(mem_system_command_handler,
			mem_domain_index, "mem_system_command");
	EV_MEM_SYSTEM_END_COMMAND = esim_register_event_with_name(mem_system_end_command_handler,
			mem_domain_index, "mem_system_end_command");

	/* NMOESI memory event-driven simulation */

	EV_MOD_NMOESI_LOAD = esim_register_event_with_name(mod_handler_nmoesi_load,
			mem_domain_index, "mod_nmoesi_load");
	EV_MOD_NMOESI_LOAD_LOCK = esim_register_event_with_name(mod_handler_nmoesi_load,
			mem_domain_index, "mod_nmoesi_load_lock");
	EV_MOD_NMOESI_LOAD_ACTION = esim_register_event_with_name(mod_handler_nmoesi_load,
			mem_domain_index, "mod_nmoesi_load_action");
	EV_MOD_NMOESI_LOAD_MISS = esim_register_event_with_name(mod_handler_nmoesi_load,
			mem_domain_index, "mod_nmoesi_load_miss");
	EV_MOD_NMOESI_LOAD_UNLOCK = esim_register_event_with_name(mod_handler_nmoesi_load,
			mem_domain_index, "mod_nmoesi_load_unlock");
	EV_MOD_NMOESI_LOAD_FINISH = esim_register_event_with_name(mod_handler_nmoesi_load,
			mem_domain_index, "mod_nmoesi_load_finish");

	EV_MOD_NMOESI_STORE = esim_register_event_with_name(mod_handler_nmoesi_store,
			mem_domain_index, "mod_nmoesi_store");
	EV_MOD_NMOESI_STORE_LOCK = esim_register_event_with_name(mod_handler_nmoesi_store,
			mem_domain_index, "mod_nmoesi_store_lock");
	EV_MOD_NMOESI_STORE_ACTION = esim_register_event_with_name(mod_handler_nmoesi_store,
			mem_domain_index, "mod_nmoesi_store_action");
	EV_MOD_NMOESI_STORE_UNLOCK = esim_register_event_with_name(mod_handler_nmoesi_store,
			mem_domain_index, "mod_nmoesi_store_unlock");
	EV_MOD_NMOESI_STORE_FINISH = esim_register_event_with_name(mod_handler_nmoesi_store,
			mem_domain_index, "mod_nmoesi_store_finish");

	EV_MOD_NMOESI_NC_STORE = esim_register_event_with_name(mod_handler_nmoesi_nc_store,
			mem_domain_index, "mod_nmoesi_nc_store");
	EV_MOD_NMOESI_NC_STORE_LOCK = esim_register_event_with_name(mod_handler_nmoesi_nc_store,
			mem_domain_index, "mod_nmoesi_nc_store_lock");
	EV_MOD_NMOESI_NC_STORE_WRITEBACK = esim_register_event_with_name(mod_handler_nmoesi_nc_store,
			mem_domain_index, "mod_nmoesi_nc_store_writeback");
	EV_MOD_NMOESI_NC_STORE_ACTION = esim_register_event_with_name(mod_handler_nmoesi_nc_store,
			mem_domain_index, "mod_nmoesi_nc_store_action");
	EV_MOD_NMOESI_NC_STORE_MISS= esim_register_event_with_name(mod_handler_nmoesi_nc_store,
			mem_domain_index, "mod_nmoesi_nc_store_miss");
	EV_MOD_NMOESI_NC_STORE_UNLOCK = esim_register_event_with_name(mod_handler_nmoesi_nc_store,
			mem_domain_index, "mod_nmoesi_nc_store_unlock");
	EV_MOD_NMOESI_NC_STORE_FINISH = esim_register_event_with_name(mod_handler_nmoesi_nc_store,
			mem_domain_index, "mod_nmoesi_nc_store_finish");

	EV_MOD_NMOESI_PREFETCH = esim_register_event_with_name(mod_handler_nmoesi_prefetch,
			mem_domain_index, "mod_nmoesi_prefetch");
	EV_MOD_NMOESI_PREFETCH_LOCK = esim_register_event_with_name(mod_handler_nmoesi_prefetch,
			mem_domain_index, "mod_nmoesi_prefetch_lock");
	EV_MOD_NMOESI_PREFETCH_ACTION = esim_register_event_with_name(mod_handler_nmoesi_prefetch,
			mem_domain_index, "mod_nmoesi_prefetch_action");
	EV_MOD_NMOESI_PREFETCH_MISS = esim_register_event_with_name(mod_handler_nmoesi_prefetch,
			mem_domain_index, "mod_nmoesi_prefetch_miss");
	EV_MOD_NMOESI_PREFETCH_UNLOCK = esim_register_event_with_name(mod_handler_nmoesi_prefetch,
			mem_domain_index, "mod_nmoesi_prefetch_unlock");
	EV_MOD_NMOESI_PREFETCH_FINISH = esim_register_event_with_name(mod_handler_nmoesi_prefetch,
			mem_domain_index, "mod_nmoesi_prefetch_finish");

	EV_MOD_NMOESI_FIND_AND_LOCK = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock,
			mem_domain_index, "mod_nmoesi_find_and_lock");
	EV_MOD_NMOESI_FIND_AND_LOCK_PORT = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock,
			mem_domain_index, "mod_nmoesi_find_and_lock_port");
	EV_MOD_NMOESI_FIND_AND_LOCK_PREF_STREAM = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock,
			mem_domain_index, "mod_nmoesi_find_and_lock_pref_stream");
	EV_MOD_NMOESI_FIND_AND_LOCK_ACTION = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock,
			mem_domain_index, "mod_nmoesi_find_and_lock_action");
	EV_MOD_NMOESI_FIND_AND_LOCK_FINISH = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock,
			mem_domain_index, "mod_nmoesi_find_and_lock_finish");

	EV_MOD_NMOESI_EVICT = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict");
	EV_MOD_NMOESI_EVICT_INVALID = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_invalid");
	EV_MOD_NMOESI_EVICT_ACTION = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_action");
	EV_MOD_NMOESI_EVICT_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_receive");
	EV_MOD_NMOESI_EVICT_PROCESS = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_process");
	EV_MOD_NMOESI_EVICT_PROCESS_NONCOHERENT = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_process_noncoherent");
	EV_MOD_NMOESI_EVICT_REPLY = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_reply");
	EV_MOD_NMOESI_EVICT_REPLY = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_reply");
	EV_MOD_NMOESI_EVICT_REPLY_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_reply_receive");
	EV_MOD_NMOESI_EVICT_FINISH = esim_register_event_with_name(mod_handler_nmoesi_evict,
			mem_domain_index, "mod_nmoesi_evict_finish");

	EV_MOD_NMOESI_WRITE_REQUEST = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request");
	EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_receive");
	EV_MOD_NMOESI_WRITE_REQUEST_LOCK = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_lock");
	EV_MOD_NMOESI_WRITE_REQUEST_ACTION = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_action");
	EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_exclusive");
	EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_updown");
	EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_updown_finish");
	EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_LATENCY = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_updown_latency");
	EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_downup");
	EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_downup_finish");
	EV_MOD_NMOESI_WRITE_REQUEST_REPLY = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_reply");
	EV_MOD_NMOESI_WRITE_REQUEST_FINISH = esim_register_event_with_name(mod_handler_nmoesi_write_request,
			mem_domain_index, "mod_nmoesi_write_request_finish");

	EV_MOD_NMOESI_READ_REQUEST = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request");
	EV_MOD_NMOESI_READ_REQUEST_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_receive");
	EV_MOD_NMOESI_READ_REQUEST_LOCK = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_lock");
	EV_MOD_NMOESI_READ_REQUEST_ACTION = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_action");
	EV_MOD_NMOESI_READ_REQUEST_UPDOWN = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_updown");
	EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_updown_miss");
	EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_updown_finish");
	EV_MOD_NMOESI_READ_REQUEST_UPDOWN_LATENCY = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_updown_latency");
	EV_MOD_NMOESI_READ_REQUEST_DOWNUP = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_downup");
	EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_downup_wait_for_reqs");
	EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_downup_finish");
	EV_MOD_NMOESI_READ_REQUEST_REPLY = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_reply");
	EV_MOD_NMOESI_READ_REQUEST_FINISH = esim_register_event_with_name(mod_handler_nmoesi_read_request,
			mem_domain_index, "mod_nmoesi_read_request_finish");

	EV_MOD_NMOESI_INVALIDATE = esim_register_event_with_name(mod_handler_nmoesi_invalidate,
			mem_domain_index, "mod_nmoesi_invalidate");
	EV_MOD_NMOESI_INVALIDATE_FINISH = esim_register_event_with_name(mod_handler_nmoesi_invalidate,
			mem_domain_index, "mod_nmoesi_invalidate_finish");

	EV_MOD_NMOESI_PEER_SEND = esim_register_event_with_name(mod_handler_nmoesi_peer,
			mem_domain_index, "mod_nmoesi_peer_send");
	EV_MOD_NMOESI_PEER_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_peer,
			mem_domain_index, "mod_nmoesi_peer_receive");
	EV_MOD_NMOESI_PEER_REPLY = esim_register_event_with_name(mod_handler_nmoesi_peer,
			mem_domain_index, "mod_nmoesi_peer_reply");
	EV_MOD_NMOESI_PEER_FINISH = esim_register_event_with_name(mod_handler_nmoesi_peer,
			mem_domain_index, "mod_nmoesi_peer_finish");

	EV_MOD_NMOESI_MESSAGE = esim_register_event_with_name(mod_handler_nmoesi_message,
			mem_domain_index, "mod_nmoesi_message");
	EV_MOD_NMOESI_MESSAGE_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_message,
			mem_domain_index, "mod_nmoesi_message_receive");
	EV_MOD_NMOESI_MESSAGE_ACTION = esim_register_event_with_name(mod_handler_nmoesi_message,
			mem_domain_index, "mod_nmoesi_message_action");
	EV_MOD_NMOESI_MESSAGE_REPLY = esim_register_event_with_name(mod_handler_nmoesi_message,
			mem_domain_index, "mod_nmoesi_message_reply");
	EV_MOD_NMOESI_MESSAGE_FINISH = esim_register_event_with_name(mod_handler_nmoesi_message,
			mem_domain_index, "mod_nmoesi_message_finish");

	/* Streams prefetch */
	EV_MOD_PREF = esim_register_event_with_name(mod_handler_pref,
			mem_domain_index, "mod_nmoesi_prefetch_streams");
	EV_MOD_PREF_LOCK = esim_register_event_with_name(mod_handler_pref,
			mem_domain_index, "mod_nmoesi_prefetch_streams_lock");
	EV_MOD_PREF_ACTION = esim_register_event_with_name(mod_handler_pref,
			mem_domain_index, "mod_nmoesi_prefetch_streams_action");
	EV_MOD_PREF_MISS = esim_register_event_with_name(mod_handler_pref,
			mem_domain_index, "mod_nmoesi_prefetch_streams_miss");
	EV_MOD_PREF_UNLOCK = esim_register_event_with_name(mod_handler_pref,
			mem_domain_index, "mod_nmoesi_prefetch_streams_unlock");
	EV_MOD_PREF_FINISH = esim_register_event_with_name(mod_handler_pref,
			mem_domain_index, "mod_nmoesi_prefetch_streams_finish");

	EV_MOD_NMOESI_PREF_FIND_AND_LOCK = esim_register_event_with_name(mod_handler_nmoesi_pref_find_and_lock,
			mem_domain_index,"mod_nmoesi_pref_find_and_lock");
	EV_MOD_NMOESI_PREF_FIND_AND_LOCK_PORT = esim_register_event_with_name(mod_handler_nmoesi_pref_find_and_lock,
			mem_domain_index,"mod_nmoesi_pref_find_and_lock_port");
	EV_MOD_NMOESI_PREF_FIND_AND_LOCK_ACTION = esim_register_event_with_name(mod_handler_nmoesi_pref_find_and_lock,
			mem_domain_index,"mod_nmoesi_pref_find_and_lock_action");
	EV_MOD_NMOESI_PREF_FIND_AND_LOCK_FINISH = esim_register_event_with_name(mod_handler_nmoesi_pref_find_and_lock,
			mem_domain_index,"mod_nmoesi_pref_find_and_lock_finish");

	EV_MOD_NMOESI_INVALIDATE_SLOT = esim_register_event_with_name(mod_handler_nmoesi_invalidate_slot,
			mem_domain_index, "mod_nmoesi_invalidate_slot");
	EV_MOD_NMOESI_INVALIDATE_SLOT_LOCK = esim_register_event_with_name(mod_handler_nmoesi_invalidate_slot,
			mem_domain_index, "mod_nmoesi_invalidate_slot_lock");
	EV_MOD_NMOESI_INVALIDATE_SLOT_ACTION = esim_register_event_with_name(mod_handler_nmoesi_invalidate_slot,
			 mem_domain_index, "mod_nmoesi_invalidate_slot_action");
	EV_MOD_NMOESI_INVALIDATE_SLOT_UNLOCK = esim_register_event_with_name(mod_handler_nmoesi_invalidate_slot,
			mem_domain_index, "mod_nmoesi_invalidate_slot_unlock");
	EV_MOD_NMOESI_INVALIDATE_SLOT_FINISH = esim_register_event_with_name(mod_handler_nmoesi_invalidate_slot,
			mem_domain_index, "mod_nmoesi_invalidate_slot_finish");

	/* Event for interval reports */
	EV_MOD_REPORT = esim_register_event_with_name(mod_report_handler, mem_domain_index, "mod_report");
	EV_MEM_CONTROLLER_REPORT = esim_register_event_with_name(mem_controller_report_handler, mem_domain_index, "mem_controller_report");

	/* Event for adaptative prefetch */
	EV_MOD_ADAPT_PREF = esim_register_event_with_name(mod_adapt_pref_handler, mem_domain_index, "mod_adapt_pref");

	/* Event for calculing the stats needed for adaptative bandwidth control */
	EV_MEM_CONTROLLER_BW_CTRL_STATS = esim_register_event_with_name(mem_controller_bandwidth_control_stats_handler, mem_domain_index, "mem_controller_bandwidth_control_stats");

	LIST_FOR_EACH(mem_system->mod_list, i)
	{
		struct mod_t *mod = list_get(mem_system->mod_list, i);

		if (mod->kind != mod_kind_cache)
			continue;

		/* Schedule interval reporting */
		if (mod->report_enabled)
			mod_report_schedule(mod);

		/* Shedule adaptative prefetch */
		if (mod->cache->prefetch.adapt_policy)
			mod_adapt_pref_schedule(mod);
	}

	LINKED_LIST_FOR_EACH(mem_system->mem_controllers)
	{
		struct mem_controller_t * mem_controller= linked_list_get(mem_system->mem_controllers);
		if(mem_controller->report_enabled)
			mem_controller_report_schedule(mem_controller);
		/*if (mem_controller->bandwidth_control_stats_enabled)*/ /* Always enabled, for now */
			mem_controller_bandwidth_control_stats_schedule(mem_controller);
	}

	/* Main memory */
	EV_MOD_NMOESI_EXAMINE_ONLY_ONE_QUEUE_REQUEST=esim_register_event(mod_handler_nmoesi_request_main_memory, mem_domain_index);
	EV_MOD_NMOESI_EXAMINE_QUEUE_REQUEST=esim_register_event(mod_handler_nmoesi_request_main_memory, mem_domain_index);
	EV_MOD_NMOESI_ACCES_BANK = esim_register_event(mod_handler_nmoesi_request_main_memory, mem_domain_index);
	EV_MOD_NMOESI_TRANSFER_FROM_BANK=esim_register_event(mod_handler_nmoesi_request_main_memory, mem_domain_index);
	EV_MOD_NMOESI_ACCES_TABLE = esim_register_event(mod_handler_nmoesi_request_main_memory, mem_domain_index);
	EV_MOD_NMOESI_TRANSFER_FROM_TABLE=esim_register_event(mod_handler_nmoesi_request_main_memory, mem_domain_index);
	EV_MOD_NMOESI_REMOVE_MEMORY_CONTROLLER=esim_register_event(mod_handler_nmoesi_request_main_memory, mem_domain_index);
	EV_MOD_NMOESI_INSERT_MEMORY_CONTROLLER=esim_register_event(mod_handler_nmoesi_request_main_memory, mem_domain_index);

	/* Memory controller */
	EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER = esim_register_event(mod_handler_nmoesi_find_and_lock_mem_controller, mem_domain_index);
	EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_PORT = esim_register_event(mod_handler_nmoesi_find_and_lock_mem_controller, mem_domain_index);
	EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_ACTION = esim_register_event(mod_handler_nmoesi_find_and_lock_mem_controller, mem_domain_index);
	EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_FINISH = esim_register_event(mod_handler_nmoesi_find_and_lock_mem_controller, mem_domain_index);

	/* Adaptative */
	EV_MEM_CONTROLLER_ADAPT = esim_register_event(mem_controller_adapt_handler,mem_domain_index);

	/* Schedule adaptative prefetch */
	LINKED_LIST_FOR_EACH(mem_system->mem_controllers)
	{
		struct mem_controller_t* mem_controller=linked_list_get(mem_system->mem_controllers);

		if(mem_controller->adaptative)
			mem_controller_adapt_schedule(mem_controller);
	}


	/* Local memory event driven simulation */

	EV_MOD_LOCAL_MEM_LOAD = esim_register_event_with_name(mod_handler_local_mem_load,
			mem_domain_index, "mod_local_mem_load");
	EV_MOD_LOCAL_MEM_LOAD_LOCK = esim_register_event_with_name(mod_handler_local_mem_load,
			mem_domain_index, "mod_local_mem_load_lock");
	EV_MOD_LOCAL_MEM_LOAD_FINISH = esim_register_event_with_name(mod_handler_local_mem_load,
			mem_domain_index, "mod_local_mem_load_finish");

	EV_MOD_LOCAL_MEM_STORE = esim_register_event_with_name(mod_handler_local_mem_store,
			mem_domain_index, "mod_local_mem_store");
	EV_MOD_LOCAL_MEM_STORE_LOCK = esim_register_event_with_name(mod_handler_local_mem_store,
			mem_domain_index, "mod_local_mem_store_lock");
	EV_MOD_LOCAL_MEM_STORE_FINISH = esim_register_event_with_name(mod_handler_local_mem_store,
			mem_domain_index, "mod_local_mem_store_finish");

	EV_MOD_LOCAL_MEM_FIND_AND_LOCK = esim_register_event_with_name(mod_handler_local_mem_find_and_lock,
			mem_domain_index, "mod_local_mem_find_and_lock");
	EV_MOD_LOCAL_MEM_FIND_AND_LOCK_PORT = esim_register_event_with_name(mod_handler_local_mem_find_and_lock,
			mem_domain_index, "mod_local_mem_find_and_lock_port");
	EV_MOD_LOCAL_MEM_FIND_AND_LOCK_ACTION = esim_register_event_with_name(mod_handler_local_mem_find_and_lock,
			mem_domain_index, "mod_local_mem_find_and_lock_action");
	EV_MOD_LOCAL_MEM_FIND_AND_LOCK_FINISH = esim_register_event_with_name(mod_handler_local_mem_find_and_lock,
			mem_domain_index, "mod_local_mem_find_and_lock_finish");
}


void mem_system_done(void)
{
	/* Dump report */
	mem_system_dump_report();

	/* Free memory system */
	mem_system_free(mem_system);
}


void mem_controller_dump_core_report(char * name)
{

	struct mem_controller_t *mem_controller = linked_list_get(mem_system->mem_controllers);
	char buf[MAX_STRING_SIZE];
	FILE * f;
	for (int i=0; i< mem_controller->num_cores;i++)
	{
		snprintf(buf, sizeof buf, "%s.core%d", name,i);

		f = file_open_for_write(buf);
		if (!f)
			return;

		fprintf(f, "[MAIN-MEMORY]\n");
		fprintf(f, "TotalTime = %f\n",mem_controller->core_mc_accesses[i] ? (double) (mem_controller->t_core_wait[i]+ mem_controller->t_core_acces[i]+mem_controller->t_core_transfer[i])/mem_controller->core_mc_accesses[i]:0.0);
		fprintf(f, "AvgTimeWaitMCQueue = %f\n",mem_controller->core_mc_accesses[i] ? (double)mem_controller->t_core_wait[i]/mem_controller->core_mc_accesses[i]:0.0);
		fprintf(f, "AvgTimeAccesMM = %f\n",mem_controller->core_mc_accesses[i] ? (double) mem_controller->t_core_acces[i]/mem_controller->core_mc_accesses[i] :0.0);
		fprintf(f, "AvgTimeTransferFromMM = %f\n",mem_controller->core_mc_accesses[i] ?(double)mem_controller->t_core_transfer[i]/mem_controller->core_mc_accesses[i]:0.0 );

		fprintf(f,"TotalAccessesMC = %lld\n",mem_controller->core_mc_accesses[i]);
		fprintf(f,"TotalNormalAccessesMC = %lld\n", mem_controller->core_normal_mc_accesses[i]);
		fprintf(f,"TotalPrefetchAccessesMC = %lld\n", mem_controller->core_pref_mc_accesses[i]);
		fprintf(f, "PercentRowBufferHit = %f\n",mem_controller->core_mc_accesses[i]?(double) mem_controller->core_row_buffer_hits[i]/mem_controller->core_mc_accesses[i]:0.0 );

		fprintf(f,"\n\n");

		for (int j=0; j<mem_controller->num_regs_bank*mem_controller->num_regs_rank;j++)
		{
			fprintf(f, "[Bank-%d]\n",j);
			fprintf(f, "PercentRowBufferHit = %f\n",mem_controller->core_mc_accesses_per_bank[i][j]?(double) mem_controller->core_row_buffer_hits_per_bank[i][j]/mem_controller->core_mc_accesses_per_bank[i][j]:0.0 );
			fprintf(f,"TotalAccessesMC = %lld\n", mem_controller->core_mc_accesses_per_bank[i][j]);
		}
		fprintf(f,"\n\n");

		if (mem_controller->num_tables)
		{
			struct row_buffer_table_t * table = mem_controller_get_row_buffer_table(mem_controller,i);

			fprintf(f, "[RowBufferTable]\n");
			fprintf(f,"TotalAccesses = %lld\n", table->accesses);
			fprintf(f,"RowBufferHitPercent = %F\n", table->accesses?(double)table->hit_accesses/table->accesses : 0.0);
			fprintf(f,"TotalTransferedBlocks = %lld\n", table->transfered_blocks);
			fprintf(f,"PercentUsefulBlocks = %f\n", table->transfered_blocks ?  (double)table->useful_blocks/table->transfered_blocks:0);
			fprintf(f,"BlocksPerTransference = %f\n", table->num_transfers ?  (double)table->transfered_blocks/table->num_transfers:0);
			fprintf(f,"TotalNormalAccesses = %lld\n", table->normal_accesses);
			fprintf(f,"NormalRowBufferHitPercent = %F\n",table->normal_accesses?(double)
				table->normal_hit_accesses/table->normal_accesses : 0.0);
			fprintf(f,"TotalPrefetchAccesses = %lld\n", table->pref_accesses);
			fprintf(f,"PrefetchRowBufferHitPercent = %F\n", table->pref_accesses?(double)
				table->pref_hit_accesses/table->pref_accesses : 0.0);
			fprintf(f,"\n");
		}

		fclose(f);
	}
}


void mem_controller_dump_report()
{
	FILE *f;
	struct mod_t * mod;

	/* TODO: cambiar per a varios mc */
	linked_list_head(mem_system->mem_controllers);
	struct mem_controller_t *mem_controller = linked_list_get(mem_system->mem_controllers);
	double total_bank_parallelism = 0;
	double total_rank_parallelism = 0;
	long long total_acces = 0;
	long long total_normal_acces = 0;
	long long total_pref_acces = 0;
	long long total_bursts = 0;
	long long total_burst_accesses = 0;
	long long table_accesses = 0;
	long long table_hit_accesses = 0;
	long long table_normal_accesses = 0;
	long long table_normal_hit_accesses = 0;
	long long table_pref_accesses = 0;
	long long table_pref_hit_accesses = 0;
	long long table_trans_blocks = 0;
	long long table_useful_blocks = 0;
	long long table_num_trans = 0;
	char buf[MAX_STRING_SIZE];

	/* Open file */
	f = file_open_for_write(main_mem_report_file_name);
	if (!f)
		return;

	/* Intro */
	fprintf(f, ";Report for channels, banks, ranks and row buffer\n");
	fprintf(f, ";    AvgTimeAccesMM- Average time per access to acces to MM, depends of row buffer hit/miss and time to send a request \n");
	fprintf(f, ";    AvgTimeWaitMCQueue- Average time waiting per access to acces to main memory in mem controller queue \n");
	fprintf(f, ";    AvgTimeTransferFromMM- Average time per access to transfer a block from MM, including acces channel delay\n");
	fprintf(f, ";    Conflicts - Total number of attempts to acces to bank when this bank is busy\n");
	fprintf(f, ";    AvgTimeWaitRequestSend - Average Cycles per access waiting until channel and bank are free and request can be sent to MM\n");
	fprintf(f, ";    AvgTimeWaitRequestTransfer - Average Cycles per access waiting until channel is free and block can be transfered from MM\n");
	fprintf(f, ";    AvgTimeRequestTransfer - Average Cycles per access which is needed to transfer a block to MC from MM\n");
	fprintf(f, ";    AvgTimeWaitBankBusy- Average time which the bank is being accessed and requests have to wait \n");
	fprintf(f, ";    AvgTimeWaitequestSendChannelBusy- Average time per access waiting until channel is free \n");
	fprintf(f, ";    RowBufferHitPercent- Percent of accesses which hit in row buffer bank \n");
	fprintf(f, ";    AvgNumRequest - Average number of requests inside a queue every cycle \n");
	fprintf(f, ";    TimeFullPercent - Percent of time which queue is full \n");
	fprintf(f, "\n\n");

	/* Select main memory module */
	for (int m = 0; m < list_count(mem_system->mod_list); m++)
	{
		mod = list_get(mem_system->mod_list, m);
		if(mod->kind!=mod_kind_main_memory)
			continue;

		mem_controller=mod->mem_controller;

		for(int c=0; c<mem_controller->num_regs_channel;c++)
		{
			total_acces += mem_controller->regs_channel[c].acceses;
			total_pref_acces+=mem_controller->regs_channel[c].pref_accesses;
			total_normal_acces+=mem_controller->regs_channel[c].normal_accesses;
			for(int r=0;r<mem_controller->regs_channel[c].num_regs_rank;r++)
			{
				total_rank_parallelism+=mem_controller->regs_channel[c].regs_rank[r].parallelism;
				for(int b=0; b<mem_controller->regs_channel[c].regs_rank[r].num_regs_bank;b++)
					total_bank_parallelism+=mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].parallelism;
			}
		}

		snprintf(buf, sizeof buf, "%s.%s", main_mem_report_file_name,mod->name );
		mem_controller_dump_core_report(buf);

		for(int i=0; i<mem_controller->row_buffer_size/mod->cache->block_size;i++)
		{
			total_bursts+=mem_controller->burst_size[i];
			total_burst_accesses+=mem_controller->burst_size[i]*(i+1);
		}

		fprintf(f, "[MAIN-MEMORY-%s]\n",mod->name);
		fprintf(f, "TotalTimeGoComeL2 = %f\n",mem_controller->accesses ? (double) (mem_controller->t_wait+
					mem_controller->t_acces_main_memory+mem_controller->t_transfer+mem_controller->t_inside_net)/mem_controller->accesses:0.0);
		fprintf(f, "TotalTime = %f\n",mem_controller->accesses ? (double) (mem_controller->t_wait+
					mem_controller->t_acces_main_memory+mem_controller->t_transfer)/mem_controller->accesses:0.0);
		fprintf(f, "AvgTimeWaitMCQueue = %f\n",mem_controller->accesses ? (double)
				mem_controller->t_wait/mem_controller->accesses:0.0);
		fprintf(f, "AvgTimeAccesMM = %f\n",mem_controller->accesses ? (double)
				mem_controller->t_acces_main_memory/mem_controller->accesses :0.0);
		fprintf(f, "AvgTimeTransferFromMM = %f\n",mem_controller->accesses?(double)
				mem_controller->t_transfer/mem_controller->accesses:0.0 );
		fprintf(f, "AvgTimeInsideNet = %f\n",mem_controller->accesses?(double)
				mem_controller->t_inside_net/mem_controller->accesses:0.0 );
		fprintf(f,"TotalAccessesMC = %lld\n", mem_controller->accesses);
		fprintf(f, "RowBufferHitPercent = %F\n", mem_controller->accesses?(double)
					mem_controller->row_buffer_hits/mem_controller->accesses : 0.0);
		fprintf(f,"TotalNonCoalescedAccessesMC = %lld\n", mem_controller->non_coalesced_accesses);
		fprintf(f,"RequestsPerCoalesdedAcces = %f\n", mem_controller->non_coalesced_accesses ?(double)
				mem_controller->accesses/mem_controller->non_coalesced_accesses:0);
		fprintf(f,"AccuracyTransferedBlocks = %f\n", mem_controller->blocks_transfered ? (double)
				mem_controller->useful_blocks_transfered/mem_controller->blocks_transfered:0);

		fprintf(f,"\n");

		/*Normal requests*/
		fprintf(f, "TotalTimeNormal = %f\n",mem_controller->normal_accesses ? (double) (mem_controller->t_normal_wait+
					mem_controller->t_normal_acces_main_memory+mem_controller->t_normal_transfer)/mem_controller->normal_accesses:0.0);
		fprintf(f, "AvgTimeNormalWaitMCQueueN = %f\n",mem_controller->normal_accesses ? (double)
				mem_controller->t_normal_wait/mem_controller->normal_accesses:0.0);
		fprintf(f, "AvgTimeNormalAccesMM = %f\n",mem_controller->normal_accesses ? (double)
				mem_controller->t_normal_acces_main_memory/mem_controller->normal_accesses :0.0);
		fprintf(f,"AvgTimeNormalTransferFromMM = %f\n",mem_controller->normal_accesses?(double)
				mem_controller->t_normal_transfer/mem_controller->normal_accesses:0.0 );
		fprintf(f,"TotalNormalAccessesMC = %lld\n", mem_controller->normal_accesses);
		fprintf(f, "NormalhRowBufferHitPercent = %F\n", mem_controller->normal_accesses?(double)
					mem_controller->row_buffer_hits_normal/mem_controller->normal_accesses : 0.0);

		fprintf(f,"\n");

		/*Prefetch requests*/
		fprintf(f, "TotalTimePrefetch = %f\n",mem_controller->pref_accesses ? (double) (mem_controller->t_pref_wait+
					mem_controller->t_pref_acces_main_memory+mem_controller->t_pref_transfer)/mem_controller->pref_accesses:0.0);
		fprintf(f, "AvgTimePrefetchWaitMCQueueN = %f\n",mem_controller->pref_accesses ? (double)
				mem_controller->t_pref_wait/mem_controller->pref_accesses:0.0);
		fprintf(f, "AvgTimePrefetchAccesMM = %f\n",mem_controller->pref_accesses?(double)
				mem_controller->t_pref_acces_main_memory/mem_controller->pref_accesses :0.0);
		fprintf(f,"AvgTimePrefetchTransferFromMM = %f\n",mem_controller->pref_accesses?(double)
				mem_controller->t_pref_transfer/mem_controller->pref_accesses:0.0 );
		fprintf(f,"TotalPrefetchAccessesMC = %lld\n", mem_controller->pref_accesses);
		fprintf(f, "PrefetchRowBufferHitPercent = %F\n", mem_controller->pref_accesses?(double)
					mem_controller->row_buffer_hits_pref/mem_controller->pref_accesses : 0.0);
		fprintf(f,"\n");

		for(int i=0; i<mem_controller->row_buffer_size/mod->cache->block_size;i++)
			fprintf(f,"PercentAccessesBurst%dSize = %f\n",i+1,total_burst_accesses>0 ? (float)
					(mem_controller->burst_size[i]*(i+1))/total_burst_accesses: 0);
		fprintf(f,"\n");

		for(int i=0; i<mem_controller->row_buffer_size/mod->cache->block_size;i++)
		{
			fprintf(f,"PercentTimesBurst%dSize = %f\n", i+1,total_bursts>0?(float)mem_controller->burst_size[i]/total_bursts: 0);
			for(int j=0; j<=i;j++)
				fprintf(f,"	PercentTimes%dSuccessiveHitsInBurst%d = %f\n", j+1,i+1, mem_controller->burst_size[i]>0 ?
						(float)mem_controller->successive_hit[i][j]/mem_controller->burst_size[i]: 0);
		}
		fprintf(f,"\n\n");

		for(int i=0; i< mem_controller->num_tables;i++)
		{
			table_accesses+=mem_controller->row_buffer_table[i]->accesses;
			table_hit_accesses+=mem_controller->row_buffer_table[i]->hit_accesses;
			table_normal_accesses+=mem_controller->row_buffer_table[i]->normal_accesses;
			table_normal_hit_accesses+=mem_controller->row_buffer_table[i]->normal_hit_accesses;

			table_pref_accesses+=mem_controller->row_buffer_table[i]->pref_accesses;
			table_pref_hit_accesses+=mem_controller->row_buffer_table[i]->pref_hit_accesses;

			table_trans_blocks+=mem_controller->row_buffer_table[i]->transfered_blocks;
			table_useful_blocks+=mem_controller->row_buffer_table[i]->useful_blocks;
			table_num_trans+=mem_controller->row_buffer_table[i]->num_transfers;
		}

		fprintf(f, "[RowBufferTable-Average]\n");
		fprintf(f,"TotalAccesses = %lld\n", table_accesses);
		fprintf(f,"RowBufferHitPercent = %F\n", table_accesses?(double)table_hit_accesses/table_accesses : 0.0);
		fprintf(f,"TotalTransferedBlocks = %lld\n", table_trans_blocks);
		fprintf(f,"TotalUsefulBlocks = %lld\n", table_useful_blocks);
		fprintf(f,"PercentUsefulBlocks = %f\n", table_trans_blocks ?  (double)table_useful_blocks/table_trans_blocks:0);
		fprintf(f,"BlocksPerTransference = %f\n", table_num_trans ?  (double)table_trans_blocks/table_num_trans:0);


		fprintf(f,"TotalNormalAccesses = %lld\n", table_normal_accesses);
		fprintf(f,"NormalRowBufferHitPercent = %F\n",table_normal_accesses?(double)
			table_normal_hit_accesses/table_normal_accesses : 0.0);

		fprintf(f,"TotalPrefetchAccesses = %lld\n", table_pref_accesses);
		fprintf(f,"PrefetchRowBufferHitPercent = %F\n", table_pref_accesses?(double)
			table_pref_hit_accesses/table_pref_accesses : 0.0);

fprintf(f,"\n\n");

		for (int c=0; c<mem_controller->num_tables;c++)
		{
			struct row_buffer_table_t * table = mem_controller_get_row_buffer_table(mem_controller,c);

			fprintf(f, "[RowBufferTable-Core%d]\n",c);
			fprintf(f,"TotalAccesses = %lld\n", table->accesses);
			fprintf(f,"RowBufferHitPercent = %F\n", table->accesses?(double)table->hit_accesses/table->accesses : 0.0);
			fprintf(f,"TotalTransferedBlocks = %lld\n", table->transfered_blocks);
			fprintf(f,"PercentUsefulBlocks = %f\n", table->transfered_blocks ?  (double)table->useful_blocks/table->transfered_blocks:0);
			fprintf(f,"BlocksPerTransference = %f\n", table->num_transfers ?  (double)table->transfered_blocks/table->num_transfers:0);
			fprintf(f,"TotalNormalAccesses = %lld\n", table->normal_accesses);
			fprintf(f,"NormalRowBufferHitPercent = %F\n",table->normal_accesses?(double)
				table->normal_hit_accesses/table->normal_accesses : 0.0);
			fprintf(f,"TotalPrefetchAccesses = %lld\n", table->pref_accesses);
			fprintf(f,"PrefetchRowBufferHitPercent = %F\n", table->pref_accesses?(double)
				table->pref_hit_accesses/table->pref_accesses : 0.0);
			fprintf(f,"\n");
		}
		fprintf(f,"\n\n");

		for(int c=0; c<mem_controller->num_regs_channel;c++)
		{
			fprintf(f, "[Channel-%d (%s)]\n", c,mod->name);


			fprintf(f, "AvgTimeWaitRequestSend = %f\n",mem_controller->regs_channel[c].acceses?
					(double)mem_controller->regs_channel[c].t_wait_send_request/mem_controller->regs_channel[c].acceses : 0.0);
			fprintf(f, "AvgTimeWaitRequestSendChannelBusy = %f\n",mem_controller->regs_channel[c].num_requests_transfered ?
					(double)mem_controller->regs_channel[c].t_wait_channel_busy/
					mem_controller->regs_channel[c].num_requests_transfered : 0.0);
			fprintf(f, "AvgTimeWaitRequestTransfer = %f\n",mem_controller->regs_channel[c].num_requests_transfered?
					(double) mem_controller->regs_channel[c].t_wait_transfer_request/
					mem_controller->regs_channel[c].num_requests_transfered : 0.0);
			fprintf(f, "AvgTimeRequestTransfer = %f\n",mem_controller->regs_channel[c].num_requests_transfered ?(double)
					mem_controller->regs_channel[c].t_transfer/mem_controller->regs_channel[c].num_requests_transfered : 0.0);
			fprintf(f,"\n");

			/*Normal requests*/

			fprintf(f, "AvgTimeNormalWaitRequestSend = %f\n",mem_controller->regs_channel[c].normal_accesses?(double)
					mem_controller->regs_channel[c].t_normal_wait_send_request/
					mem_controller->regs_channel[c].normal_accesses:0);
			fprintf(f, "AvgTimeNormalWaitRequestSendChannelBusy = %f\n",
					mem_controller->regs_channel[c].num_normal_requests_transfered ? (double)
					mem_controller->regs_channel[c].t_normal_wait_channel_busy/
					mem_controller->regs_channel[c].num_normal_requests_transfered : 0.0);
			fprintf(f, "AvgTimeNormalWaitRequestTransfer = %f\n",mem_controller->regs_channel[c].num_normal_requests_transfered?
					(double)mem_controller->regs_channel[c].t_normal_wait_transfer_request/
					mem_controller->regs_channel[c].num_normal_requests_transfered: 0.0);
			fprintf(f,"\n");

			/*Prefetch requests*/

			fprintf(f, "AvgTimePrefetchWaitRequestSend = %f\n",mem_controller->regs_channel[c].pref_accesses?(double)
					mem_controller->regs_channel[c].t_pref_wait_send_request/mem_controller->regs_channel[c].pref_accesses : 0);
			fprintf(f, "AvgTimePrefetchWaitRequestSendChannelBusy = %f\n",
					mem_controller->regs_channel[c].num_pref_requests_transfered ?(double)
					mem_controller->regs_channel[c].t_pref_wait_channel_busy/
					mem_controller->regs_channel[c].num_pref_requests_transfered : 0.0);
			fprintf(f, "AvgTimePrefetchWaitRequestTransfer = %f\n",mem_controller->regs_channel[c].num_pref_requests_transfered?
					(double)mem_controller->regs_channel[c].t_pref_wait_transfer_request/
					mem_controller->regs_channel[c].num_pref_requests_transfered: 0.0);


			fprintf(f,"\n");
			for(int r=0; r<mem_controller->regs_channel[c].num_regs_rank; r++){
				fprintf(f, "[Rank-%d  (Channel-%d %s)]\n", r, c,mod->name);
				fprintf(f, "RowBufferHitPercent = %f\n",mem_controller->regs_channel[c].regs_rank[r].acceses ?(double)
						mem_controller->regs_channel[c].regs_rank[r].row_buffer_hits/
						mem_controller->regs_channel[c].regs_rank[r].acceses:0.0);
				fprintf(f, "NormalRowBufferHitPercent = %f\n",mem_controller->regs_channel[c].regs_rank[r].normal_accesses ?
						(double)mem_controller->regs_channel[c].regs_rank[r].row_buffer_hits_normal/
						mem_controller->regs_channel[c].regs_rank[r].normal_accesses:0.0);
				fprintf(f, "PrefetchRowBufferHitPercent = %f\n",mem_controller->regs_channel[c].regs_rank[r].pref_accesses ?
						(double)mem_controller->regs_channel[c].regs_rank[r].row_buffer_hits_pref/
						mem_controller->regs_channel[c].regs_rank[r].pref_accesses:0.0);
				fprintf(f, "ParallelismPercent = %f\n\n",total_rank_parallelism ?
						(double)mem_controller->regs_channel[c].regs_rank[r].parallelism/total_rank_parallelism : 0.0);

				for(int b=0; b<mem_controller->regs_channel[c].regs_rank[r].num_regs_bank;b++)
				{

					fprintf(f, "[Bank-%d  (Rank-%d Channel-%d %s)]\n", b,r,c,mod->name);
					fprintf(f, "RowBufferHitPercent = %f\n",
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].acceses ?
							(double)mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].row_buffer_hits/
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].acceses: 0.0);
					fprintf(f, "NormalRowBufferHitPercent = %f\n",
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].normal_accesses ?
							(double)mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].row_buffer_hits_normal/
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].normal_accesses: 0.0);
					fprintf(f, "PrefetchRowBufferHitPercent = %f\n",
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].pref_accesses ?
							(double)mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].row_buffer_hits_pref/
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].pref_accesses: 0.0);
					fprintf(f, "ParallelismPercent = %f\n", total_bank_parallelism ?(double)
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].parallelism/
							total_bank_parallelism:0);
					fprintf(f, "Conflicts = %lld\n",mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].conflicts);
					fprintf(f, "AvgTimeWaitBankBusy = %f\n",
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].acceses ?
							(double)mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].t_wait/
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].acceses:0.0);
					fprintf(f,"AvgTimeNormalWaitBankBusy = %f\n",
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].normal_accesses?
							(double)mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].t_normal_wait/
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].normal_accesses:0.0);
					fprintf(f,"AvgTimePrefetchWaitBankBusy = %f\n",
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].pref_accesses?
							(double)mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].t_pref_wait/
							mem_controller->regs_channel[c].regs_rank[r].regs_bank[b].pref_accesses:0.0);
					fprintf(f,"\n");
				}
			}
		}

		//fprintf(f, "\n[QUEUES-MEMORY-CONTROLLER (%s)]\n\n",mod->name);
		for(int i=0; i<mem_controller->num_queues;i++)
		{
			struct mem_controller_queue_t *normal = mem_controller->normal_queue[i];
			fprintf(f, "[Normal-Queue-%d (%s)]\n",i, mod->name);
			fprintf(f, "AvgNumRequests = %f\n",mem_controller->n_times_queue_examined?
				(double) normal->total_requests / esim_cycle() : 0.0);
			fprintf(f, "TimeFullPercent = %f\n", esim_cycle() ?
				(double) normal->t_full / esim_cycle() : 0.0);
			float avg_req = mem_controller->n_times_queue_examined ?
				(double) normal->total_requests / mem_controller->n_times_queue_examined : 0.0;
			fprintf(f, "TimeResponse = %f\n\n ",normal->total_insertions?(double)
				(avg_req*esim_cycle())/normal->total_insertions:0);

			fprintf(f, "[Prefetch-Queue-%i (%s)]\n",i, mod->name);
			fprintf(f, "AvgNumRequests = %f\n",mem_controller->n_times_queue_examined?(double)
				mem_controller->pref_queue[i]->total_requests/mem_controller->n_times_queue_examined:0.0);
			fprintf(f, "TimeFullPercent = %f\n", esim_cycle() ? (double)mem_controller->pref_queue[i]->t_full/esim_cycle():0.0);
			avg_req=mem_controller->n_times_queue_examined ? (double)
				mem_controller->pref_queue[i]->total_requests/mem_controller->n_times_queue_examined:0.0;
			fprintf(f, "TimeResponse = %f\n\n ", normal->total_insertions ? (double)
				(avg_req*esim_cycle())/normal->total_insertions : 0.0);
		}
		fprintf(f, "\n\n\n");
	}

	/* Done */
	fclose(f);
}


void mem_system_dump_report(void)
{
	struct net_t *net;
	struct mod_t *mod;
	struct cache_t *cache;

	FILE *f;

	int i;

	/* Open file */
	f = file_open_for_write(mem_report_file_name);
	if (!f)
		return;

	/* Intro */
	fprintf(f, "; Report for caches, TLBs, and main memory\n");
	fprintf(f, ";    Accesses - Total number of accesses\n");
	fprintf(f, ";    Hits, Misses - Accesses resulting in hits/misses\n");
	fprintf(f, ";    HitRatio - Hits divided by accesses\n");
	fprintf(f, ";    Evictions - Invalidated or replaced cache blocks\n");
	fprintf(f, ";    Retries - For L1 caches, accesses that were retried\n");
	fprintf(f, ";    ReadRetries, WriteRetries, NCWriteRetries - Read/Write retried accesses\n");
	fprintf(f, ";    NoRetryAccesses - Number of accesses that were not retried\n");
	fprintf(f, ";    NoRetryHits, NoRetryMisses - Hits and misses for not retried accesses\n");
	fprintf(f, ";    NoRetryHitRatio - NoRetryHits divided by NoRetryAccesses\n");
	fprintf(f, ";    NoRetryReads, NoRetryWrites - Not retried reads and writes\n");
	fprintf(f, ";    Reads, Writes, NCWrites - Total read/write accesses\n");
	fprintf(f, ";    BlockingReads, BlockingWrites, BlockingNCWrites - Reads/writes coming from lower-level cache\n");
	fprintf(f, ";    NonBlockingReads, NonBlockingWrites, NonBlockingNCWrites - Coming from upper-level cache\n");
	fprintf(f, ";    Programmed Prefetches - Number of programmed prefetch accesses\n");
	fprintf(f, ";    Completed Prefetches - Number of completed prefetch accesses\n");
	fprintf(f, ";    Canceled Prefetches - Number of canceled prefetch accesses\n");
	fprintf(f, ";    Useful Prefetches - Number of useful prefetches\n");
	fprintf(f, ";    Delayed hits - Number of loads that access a block with is being fetched by a prefetch\n");
	fprintf(f, ";    Prefetch Accuracy - Useful prefetches / total completed prefetches\n");
	fprintf(f, ";    Prefetch Coverage - Useful prefetches / Faults if we dont use prefetch\n");
	fprintf(f, ";    MPKI - Misses / commited instructions\n");
	fprintf(f, "\n\n");

	/* Report for each cache */
	for (i = 0; i < list_count(mem_system->mod_list); i++)
	{
		mod = list_get(mem_system->mod_list, i);
		cache = mod->cache;
		fprintf(f, "[ %s ]\n", mod->name);
		fprintf(f, "\n");

		/* Configuration */
		if (cache) {
			fprintf(f, "Sets = %d\n", cache->num_sets);
			fprintf(f, "Assoc = %d\n", cache->assoc);
			fprintf(f, "Policy = %s\n", str_map_value(&cache_policy_map, cache->policy));
		}
		fprintf(f, "BlockSize = %d\n", mod->block_size);
		fprintf(f, "Latency = %d\n", mod->latency);
		fprintf(f, "Ports = %d\n", mod->num_ports);
		fprintf(f, "\n");

		/* Statistics */
		fprintf(f, "Accesses = %lld\n", mod->accesses);
		fprintf(f, "Hits = %lld\n", mod->hits);
		fprintf(f, "Misses = %lld\n", mod->accesses - mod->hits);
		fprintf(f, "HitRatio = %.4g\n", mod->accesses ?
			(double) mod->hits / mod->accesses : 0.0);
		fprintf(f, "Evictions = %lld\n", mod->evictions);
		fprintf(f, "Retries = %lld\n", mod->read_retries + mod->write_retries +
			mod->nc_write_retries);
		fprintf(f, "PercentDirectoryQueueFull = %f\n", (double)esim_cycle() ? (double)mod->cycles_queue_full/(double)esim_cycle() : 0.0);
		fprintf(f, "\n");
		fprintf(f, "Reads = %lld\n", mod->reads);
		fprintf(f, "ReadRetries = %lld\n", mod->read_retries);
		fprintf(f, "BlockingReads = %lld\n", mod->blocking_reads);
		fprintf(f, "NonBlockingReads = %lld\n", mod->non_blocking_reads);
		fprintf(f, "ReadHits = %lld\n", mod->read_hits);
		fprintf(f, "ReadMisses = %lld\n", mod->reads - mod->read_hits);
		fprintf(f, "\n");
		fprintf(f, "Writes = %lld\n", mod->writes);
		fprintf(f, "WriteRetries = %lld\n", mod->write_retries);
		fprintf(f, "BlockingWrites = %lld\n", mod->blocking_writes);
		fprintf(f, "NonBlockingWrites = %lld\n", mod->non_blocking_writes);
		fprintf(f, "WriteHits = %lld\n", mod->write_hits);
		fprintf(f, "WriteMisses = %lld\n", mod->writes - mod->write_hits);
		fprintf(f, "\n");
		fprintf(f, "NCWrites = %lld\n", mod->nc_writes);
		fprintf(f, "NCWriteRetries = %lld\n", mod->nc_write_retries);
		fprintf(f, "NCBlockingWrites = %lld\n", mod->blocking_nc_writes);
		fprintf(f, "NCNonBlockingWrites = %lld\n", mod->non_blocking_nc_writes);
		fprintf(f, "NCWriteHits = %lld\n", mod->nc_write_hits);
		fprintf(f, "NCWriteMisses = %lld\n", mod->nc_writes - mod->nc_write_hits);
		fprintf(f, "Prefetches = %lld\n", mod->prefetches);
		fprintf(f, "\n");
		fprintf(f, "NoRetryAccesses = %lld\n", mod->no_retry_accesses);
		fprintf(f, "NoRetryHits = %lld\n", mod->no_retry_hits);
		fprintf(f, "NoRetryMisses = %lld\n", mod->no_retry_accesses - mod->no_retry_hits);
		fprintf(f, "NoRetryHitRatio = %.4g\n", mod->no_retry_accesses ?
			(double) mod->no_retry_hits / mod->no_retry_accesses : 0.0);
		fprintf(f, "NoRetryReads = %lld\n", mod->no_retry_reads);
		fprintf(f, "NoRetryReadHits = %lld\n", mod->no_retry_read_hits);
		fprintf(f, "NoRetryReadMisses = %lld\n", (mod->no_retry_reads -
			mod->no_retry_read_hits));
		fprintf(f, "NoRetryWrites = %lld\n", mod->no_retry_writes);
		fprintf(f, "NoRetryWriteHits = %lld\n", mod->no_retry_write_hits);
		fprintf(f, "NoRetryWriteMisses = %lld\n", mod->no_retry_writes
			- mod->no_retry_write_hits);
		fprintf(f, "NoRetryNCWrites = %lld\n", mod->no_retry_nc_writes);
		fprintf(f, "NoRetryNCWriteHits = %lld\n", mod->no_retry_nc_write_hits);
		fprintf(f, "NoRetryNCWriteMisses = %lld\n", mod->no_retry_nc_writes
			- mod->no_retry_nc_write_hits);
		fprintf(f, "\n");
		fprintf(f, "\n");

		fprintf(f, "ProgrammedPrefetches = %lld\n", mod->programmed_prefetches);
		fprintf(f, "CompletedPrefetches = %lld\n", mod->completed_prefetches);
		fprintf(f, "CanceledPrefetches = %lld\n", mod->canceled_prefetches);
		fprintf(f, "CanceledPrefetchEndStream = %lld\n", mod->canceled_prefetches_end_stream);
		fprintf(f, "CanceledPrefetchCoalesce = %lld\n", mod->canceled_prefetches_coalesce);
		fprintf(f, "CanceledPrefetchCacheHit = %lld\n", mod->canceled_prefetches_cache_hit);
		fprintf(f, "CanceledPrefetchStreamHit = %lld\n", mod->canceled_prefetches_stream_hit);
		fprintf(f, "CanceledPrefetchRetry = %lld\n", mod->canceled_prefetches_retry);
		fprintf(f, "PrefetchRetries = %lld\n", mod->prefetch_retries);
		fprintf(f, "\n");

		fprintf(f, "UsefulPrefetches = %lld\n", mod->useful_prefetches);
		fprintf(f, "PrefetchAccuracy = %.4g\n", mod->completed_prefetches ? (double) mod->useful_prefetches / mod->completed_prefetches : 0.0);
		fprintf(f, "\n");

		fprintf(f, "SinglePrefetches = %lld\n", mod->single_prefetches);
		fprintf(f, "GroupPrefetches = %lld\n", mod->group_prefetches);
		fprintf(f, "CanceledPrefetchGroups = %lld\n", mod->canceled_prefetch_groups);

		fprintf(f, "\n");
		fprintf(f, "DelayedHits = %lld\n", mod->delayed_hits);
		fprintf(f, "DelayedHitsCyclesCounted = %lld\n", mod->delayed_hits_cycles_counted);
		fprintf(f, "DelayedHitAvgLostCycles = %.4g\n", mod->delayed_hits_cycles_counted? mod->delayed_hit_cycles / (double) mod->delayed_hits_cycles_counted : 0.0);
		fprintf(f, "\n");

		fprintf(f, "StreamHits = %lld\n", mod->stream_hits);

		fprintf(f, "PrefetchHits (rw)(up_down) = %lld\n", mod->up_down_hits);
		fprintf(f, "PrefetchHeadHits (rw)(up_down) = %lld\n", mod->up_down_head_hits);
		fprintf(f, "PrefetchHits(r)(down_up) = %lld\n", mod->down_up_read_hits);
		fprintf(f, "PrefetchHits(w)(down_up) = %lld\n", mod->down_up_write_hits);
		fprintf(f, "\n");
		fprintf(f, "FastResumedAccesses  = %lld\n", mod->fast_resumed_accesses);
		fprintf(f, "WriteBufferReadHits = %lld\n", mod->write_buffer_read_hits);
		fprintf(f, "WriteBufferWriteHits = %lld\n", mod->write_buffer_read_hits);
		fprintf(f, "\n");
		fprintf(f, "StreamEvictions = %lld\n", mod->stream_evictions);
		fprintf(f, "DownUpReadMisses = %lld\n", mod->down_up_read_misses);
		fprintf(f, "DownUpWriteMisses = %lld\n", mod->down_up_write_misses);
		fprintf(f, "BlocksAlreadyHere = %lld\n", mod->block_already_here);
		fprintf(f, "\n\n");
	}

	/* Dump report for networks */
	for (i = 0; i < list_count(mem_system->net_list); i++)
	{
		net = list_get(mem_system->net_list, i);
		net_dump_report(net, f);
	}

	/* Done */
	fclose(f);
}


struct mod_t *mem_system_get_mod(char *mod_name)
{
	struct mod_t *mod;

	int mod_id;

	/* Look for module */
	LIST_FOR_EACH(mem_system->mod_list, mod_id)
	{
		mod = list_get(mem_system->mod_list, mod_id);
		if (!strcasecmp(mod->name, mod_name))
			return mod;
	}

	/* Not found */
	return NULL;
}


struct net_t *mem_system_get_net(char *net_name)
{
	struct net_t *net;

	int net_id;

	/* Look for network */
	LIST_FOR_EACH(mem_system->net_list, net_id)
	{
		net = list_get(mem_system->net_list, net_id);
		if (!strcasecmp(net->name, net_name))
			return net;
	}

	/* Not found */
	return NULL;
}


void main_memory_power_callback(double a, double b, double c, double d)
{
}


void main_memory_read_callback(void *payload, unsigned int id, uint64_t address, uint64_t clock_cycle)
{
	int found = 0;
	struct mod_stack_t *stack;
	struct dram_system_t *dram_system = (struct dram_system_t *) payload;

	/* You cannnot use LINKED_LIST_FOR_EACH if you plan to remove elements */
	linked_list_head(dram_system->pending_reads);
	while(!linked_list_is_end(dram_system->pending_reads))
	{
		stack = linked_list_get(dram_system->pending_reads);
		if (stack->addr == address)
		{
			mem_debug("  %lld %lld 0x%x %s dram access completed\n", esim_time, stack->id, stack->tag, stack->target_mod->dram_system->name);
			stack->main_memory_accessed = 1;
			esim_schedule_event(EV_MOD_NMOESI_READ_REQUEST_UPDOWN_LATENCY, stack, 0);
			linked_list_remove(dram_system->pending_reads);
			found++;
		}
		else
			linked_list_next(dram_system->pending_reads);
	}

	assert(found == 1);
}


void main_memory_write_callback(void *payload, unsigned int id, uint64_t address, uint64_t clock_cycle)
{
}


/* Schedules an event to notify dramsim that a main memory cycle has passed */
void main_memory_tic_scheduler(struct dram_system_t *ds)
{
	int cpu_freq = arch_x86->frequency; /* In MHz */
	int dram_freq = dram_system_get_dram_freq(ds->handler) / 1000000; /* In MHz */

	assert(cpu_freq >= dram_freq);

	/* New domain and event for dramsim clock tics */
	ds->dram_domain_index = esim_new_domain(dram_freq);
	EV_MAIN_MEMORY_TIC = esim_register_event_with_name(main_memory_tic_handler, ds->dram_domain_index, "dram_system_tic");

	esim_schedule_event(EV_MAIN_MEMORY_TIC, ds, 1);
}


/* Notifies dramsim that a main memory cycle has passed */
void main_memory_tic_handler(int event, void *data)
{
	struct dram_system_t *ds = (struct dram_system_t*) data;

	/* If simulation has ended and dram system has no more petitions, no more
	 * events to schedule. */
	if (esim_finish && !linked_list_count(ds->pending_reads) && !esim_event_count())
		return;

	dram_system_dram_tick(ds->handler);
	esim_schedule_event(event, ds, 1);
}

