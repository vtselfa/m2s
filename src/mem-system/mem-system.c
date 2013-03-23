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

#include <string.h>

#include <lib/esim/esim.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/file.h>
#include <lib/util/list.h>
#include <lib/util/string.h>
#include <network/network.h>

#include "cache.h"
#include "command.h"
#include "local-mem-protocol.h"
#include "mem-system.h"
#include "mmu.h"
#include "nmoesi-protocol.h"


/*
 * Global Variables
 */

int mem_debug_category;
int mem_trace_category;
int mem_system_peer_transfers;

struct mem_system_t *mem_system;




/*
 * Public Functions
 */

void mem_system_init(void)
{
	/* Try to open report file */
	if (*mem_report_file_name && !file_can_open_for_write(mem_report_file_name))
		fatal("%s: cannot open GPU cache report file",
			mem_report_file_name);

	/* Create memory system */
	mem_system = calloc(1, sizeof(struct mem_system_t));
	if (!mem_system)
		fatal("%s: out of memory", __FUNCTION__);

	/* Create network and module list */
	mem_system->net_list = list_create();
	mem_system->mod_list = list_create();

	//////////////////////////////////////////////////////////
        /*Create memory controller*/                            //
        mem_system->mem_controller=mem_controller_create();     //
        //////////////////////////////////////////////////////////


	/* Event handler for memory hierarchy commands */
	EV_MEM_SYSTEM_COMMAND = esim_register_event_with_name(mem_system_command_handler, "mem_system_command");
	EV_MEM_SYSTEM_END_COMMAND = esim_register_event_with_name(mem_system_end_command_handler, "mem_system_end_command");

	/* NMOESI memory event-driven simulation */

	EV_MOD_NMOESI_LOAD = esim_register_event_with_name(mod_handler_nmoesi_load, "mod_nmoesi_load");
	EV_MOD_NMOESI_LOAD_LOCK = esim_register_event_with_name(mod_handler_nmoesi_load, "mod_nmoesi_load_lock");
	EV_MOD_NMOESI_LOAD_ACTION = esim_register_event_with_name(mod_handler_nmoesi_load, "mod_nmoesi_load_action");
	EV_MOD_NMOESI_LOAD_MISS = esim_register_event_with_name(mod_handler_nmoesi_load, "mod_nmoesi_load_miss");
	EV_MOD_NMOESI_LOAD_UNLOCK = esim_register_event_with_name(mod_handler_nmoesi_load, "mod_nmoesi_load_unlock");
	EV_MOD_NMOESI_LOAD_FINISH = esim_register_event_with_name(mod_handler_nmoesi_load, "mod_nmoesi_load_finish");

	EV_MOD_NMOESI_STORE = esim_register_event_with_name(mod_handler_nmoesi_store, "mod_nmoesi_store");
	EV_MOD_NMOESI_STORE_LOCK = esim_register_event_with_name(mod_handler_nmoesi_store, "mod_nmoesi_store_lock");
	EV_MOD_NMOESI_STORE_ACTION = esim_register_event_with_name(mod_handler_nmoesi_store, "mod_nmoesi_store_action");
	EV_MOD_NMOESI_STORE_UNLOCK = esim_register_event_with_name(mod_handler_nmoesi_store, "mod_nmoesi_store_unlock");
	EV_MOD_NMOESI_STORE_FINISH = esim_register_event_with_name(mod_handler_nmoesi_store, "mod_nmoesi_store_finish");

	EV_MOD_NMOESI_NC_STORE = esim_register_event_with_name(mod_handler_nmoesi_nc_store, "mod_nmoesi_nc_store");
	EV_MOD_NMOESI_NC_STORE_LOCK = esim_register_event_with_name(mod_handler_nmoesi_nc_store, "mod_nmoesi_nc_store_lock");
	EV_MOD_NMOESI_NC_STORE_WRITEBACK = esim_register_event_with_name(mod_handler_nmoesi_nc_store, "mod_nmoesi_nc_store_writeback");
	EV_MOD_NMOESI_NC_STORE_ACTION = esim_register_event_with_name(mod_handler_nmoesi_nc_store, "mod_nmoesi_nc_store_action");
	EV_MOD_NMOESI_NC_STORE_MISS= esim_register_event_with_name(mod_handler_nmoesi_nc_store, "mod_nmoesi_nc_store_miss");
	EV_MOD_NMOESI_NC_STORE_UNLOCK = esim_register_event_with_name(mod_handler_nmoesi_nc_store, "mod_nmoesi_nc_store_unlock");
	EV_MOD_NMOESI_NC_STORE_FINISH = esim_register_event_with_name(mod_handler_nmoesi_nc_store, "mod_nmoesi_nc_store_finish");

	EV_MOD_NMOESI_FIND_AND_LOCK = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock, "mod_nmoesi_find_and_lock");
	EV_MOD_NMOESI_FIND_AND_LOCK_PORT = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock, "mod_nmoesi_find_and_lock_port");
	EV_MOD_NMOESI_FIND_AND_LOCK_PREF_STREAM = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock, "mod_nmoesi_find_and_lock_pref_stream");
	EV_MOD_NMOESI_FIND_AND_LOCK_ACTION = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock, "mod_nmoesi_find_and_lock_action");
	EV_MOD_NMOESI_FIND_AND_LOCK_FINISH = esim_register_event_with_name(mod_handler_nmoesi_find_and_lock, "mod_nmoesi_find_and_lock_finish");

	EV_MOD_NMOESI_EVICT = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict");
	EV_MOD_NMOESI_EVICT_INVALID = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_invalid");
	EV_MOD_NMOESI_EVICT_ACTION = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_action");
	EV_MOD_NMOESI_EVICT_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_receive");
	EV_MOD_NMOESI_EVICT_PROCESS = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_process");
	EV_MOD_NMOESI_EVICT_PROCESS_NONCOHERENT = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_process_noncoherent");
	EV_MOD_NMOESI_EVICT_REPLY = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_reply");
	EV_MOD_NMOESI_EVICT_REPLY = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_reply");
	EV_MOD_NMOESI_EVICT_REPLY_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_reply_receive");
	EV_MOD_NMOESI_EVICT_FINISH = esim_register_event_with_name(mod_handler_nmoesi_evict, "mod_nmoesi_evict_finish");

	EV_MOD_NMOESI_WRITE_REQUEST = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request");
	EV_MOD_NMOESI_WRITE_REQUEST_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_receive");
	EV_MOD_NMOESI_WRITE_REQUEST_LOCK = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_lock");
	EV_MOD_NMOESI_WRITE_REQUEST_ACTION = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_action");
	EV_MOD_NMOESI_WRITE_REQUEST_EXCLUSIVE = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_exclusive");
	EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_updown");
	EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_updown_finish");
	EV_MOD_NMOESI_WRITE_REQUEST_UPDOWN_FINISH_UPDATE_DIRECTORY = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_updown_finish_update_directory");
	EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_downup");
	EV_MOD_NMOESI_WRITE_REQUEST_DOWNUP_FINISH = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_downup_finish");
	EV_MOD_NMOESI_WRITE_REQUEST_REPLY = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_reply");
	EV_MOD_NMOESI_WRITE_REQUEST_FINISH = esim_register_event_with_name(mod_handler_nmoesi_write_request, "mod_nmoesi_write_request_finish");

	EV_MOD_NMOESI_READ_REQUEST = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request");
	EV_MOD_NMOESI_READ_REQUEST_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_receive");
	EV_MOD_NMOESI_READ_REQUEST_LOCK = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_lock");
	EV_MOD_NMOESI_READ_REQUEST_ACTION = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_action");
	EV_MOD_NMOESI_READ_REQUEST_UPDOWN = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_updown");
	EV_MOD_NMOESI_READ_REQUEST_UPDOWN_MISS = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_updown_miss");
	EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_updown_finish");
	EV_MOD_NMOESI_READ_REQUEST_UPDOWN_FINISH_UPDATE_DIRECTORY = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_updown_finish_update_directory");
	EV_MOD_NMOESI_READ_REQUEST_DOWNUP = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_downup");
	EV_MOD_NMOESI_READ_REQUEST_DOWNUP_WAIT_FOR_REQS = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_downup_wait_for_reqs");
	EV_MOD_NMOESI_READ_REQUEST_DOWNUP_FINISH = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_downup_finish");
	EV_MOD_NMOESI_READ_REQUEST_REPLY = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_reply");
	EV_MOD_NMOESI_READ_REQUEST_FINISH = esim_register_event_with_name(mod_handler_nmoesi_read_request, "mod_nmoesi_read_request_finish");

	EV_MOD_NMOESI_INVALIDATE = esim_register_event_with_name(mod_handler_nmoesi_invalidate, "mod_nmoesi_invalidate");
	EV_MOD_NMOESI_INVALIDATE_FINISH = esim_register_event_with_name(mod_handler_nmoesi_invalidate, "mod_nmoesi_invalidate_finish");

	EV_MOD_NMOESI_PEER_SEND = esim_register_event_with_name(mod_handler_nmoesi_peer, "mod_nmoesi_peer_send");
	EV_MOD_NMOESI_PEER_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_peer, "mod_nmoesi_peer_receive");
	EV_MOD_NMOESI_PEER_REPLY = esim_register_event_with_name(mod_handler_nmoesi_peer, "mod_nmoesi_peer_reply");
	EV_MOD_NMOESI_PEER_FINISH = esim_register_event_with_name(mod_handler_nmoesi_peer, "mod_nmoesi_peer_finish");

	EV_MOD_NMOESI_MESSAGE = esim_register_event_with_name(mod_handler_nmoesi_message, "mod_nmoesi_message");
	EV_MOD_NMOESI_MESSAGE_RECEIVE = esim_register_event_with_name(mod_handler_nmoesi_message, "mod_nmoesi_message_receive");
	EV_MOD_NMOESI_MESSAGE_ACTION = esim_register_event_with_name(mod_handler_nmoesi_message, "mod_nmoesi_message_action");
	EV_MOD_NMOESI_MESSAGE_REPLY = esim_register_event_with_name(mod_handler_nmoesi_message, "mod_nmoesi_message_reply");
	EV_MOD_NMOESI_MESSAGE_FINISH = esim_register_event_with_name(mod_handler_nmoesi_message, "mod_nmoesi_message_finish");

	/* Local memory event driven simulation */

	EV_MOD_LOCAL_MEM_LOAD = esim_register_event_with_name(mod_handler_local_mem_load, "mod_local_mem_load");
	EV_MOD_LOCAL_MEM_LOAD_LOCK = esim_register_event_with_name(mod_handler_local_mem_load, "mod_local_mem_load_lock");
	EV_MOD_LOCAL_MEM_LOAD_FINISH = esim_register_event_with_name(mod_handler_local_mem_load, "mod_local_mem_load_finish");

	EV_MOD_LOCAL_MEM_STORE = esim_register_event_with_name(mod_handler_local_mem_store, "mod_local_mem_store");
	EV_MOD_LOCAL_MEM_STORE_LOCK = esim_register_event_with_name(mod_handler_local_mem_store, "mod_local_mem_store_lock");
	EV_MOD_LOCAL_MEM_STORE_FINISH = esim_register_event_with_name(mod_handler_local_mem_store, "mod_local_mem_store_finish");

	EV_MOD_LOCAL_MEM_FIND_AND_LOCK = esim_register_event_with_name(mod_handler_local_mem_find_and_lock, "mod_local_mem_find_and_lock");
	EV_MOD_LOCAL_MEM_FIND_AND_LOCK_PORT = esim_register_event_with_name(mod_handler_local_mem_find_and_lock, "mod_local_mem_find_and_lock_port");
	EV_MOD_LOCAL_MEM_FIND_AND_LOCK_ACTION = esim_register_event_with_name(mod_handler_local_mem_find_and_lock, "mod_local_mem_find_and_lock_action");
	EV_MOD_LOCAL_MEM_FIND_AND_LOCK_FINISH = esim_register_event_with_name(mod_handler_local_mem_find_and_lock, "mod_local_mem_find_and_lock_finish");

	/* Streams prefetch */
	EV_MOD_PREF = esim_register_event(mod_handler_pref);
	EV_MOD_PREF_LOCK = esim_register_event(mod_handler_pref);
	EV_MOD_PREF_ACTION = esim_register_event(mod_handler_pref);
	EV_MOD_PREF_MISS = esim_register_event(mod_handler_pref);
	EV_MOD_PREF_UNLOCK = esim_register_event(mod_handler_pref);
	EV_MOD_PREF_FINISH = esim_register_event(mod_handler_pref);

	/* OBL prefetch */
	EV_MOD_NMOESI_PREF_OBL = esim_register_event(mod_handler_nmoesi_pref_obl);
	EV_MOD_NMOESI_PREF_OBL_LOCK = esim_register_event(mod_handler_nmoesi_pref_obl);
	EV_MOD_NMOESI_PREF_OBL_ACTION = esim_register_event(mod_handler_nmoesi_pref_obl);
	EV_MOD_NMOESI_PREF_OBL_MISS = esim_register_event(mod_handler_nmoesi_pref_obl);
	EV_MOD_NMOESI_PREF_OBL_UNLOCK = esim_register_event(mod_handler_nmoesi_pref_obl);
	EV_MOD_NMOESI_PREF_OBL_FINISH = esim_register_event(mod_handler_nmoesi_pref_obl);

	EV_MOD_NMOESI_PREF_EVICT = esim_register_event_with_name(
		mod_handler_nmoesi_pref_evict, "mod_nmoesi_pref_evict");
	EV_MOD_NMOESI_PREF_EVICT_INVALID = esim_register_event_with_name(
		mod_handler_nmoesi_pref_evict, "mod_nmoesi_pref_evict_invalid");
	EV_MOD_NMOESI_PREF_EVICT_ACTION = esim_register_event_with_name(
		mod_handler_nmoesi_pref_evict, "mod_nmoesi_pref_evict_action");
	EV_MOD_NMOESI_PREF_EVICT_RECEIVE = esim_register_event_with_name(
		mod_handler_nmoesi_pref_evict, "mod_nmoesi_pref_evict_receive");
	EV_MOD_NMOESI_PREF_EVICT_PROCESS = esim_register_event_with_name(
		mod_handler_nmoesi_pref_evict, "mod_nmoesi_pref_evict_process");
	EV_MOD_NMOESI_PREF_EVICT_REPLY = esim_register_event_with_name(
		mod_handler_nmoesi_pref_evict, "mod_nmoesi_pref_evict_reply");
	EV_MOD_NMOESI_PREF_EVICT_REPLY_RECEIVE = esim_register_event_with_name(
		mod_handler_nmoesi_pref_evict, "mod_nmoesi_pref_evict_reply_receive");
	EV_MOD_NMOESI_PREF_EVICT_FINISH = esim_register_event_with_name(
		mod_handler_nmoesi_pref_evict, "mod_nmoesi_pref_evict_finish");

	EV_MOD_NMOESI_PREF_FIND_AND_LOCK = esim_register_event_with_name(
		mod_handler_nmoesi_pref_find_and_lock, "mod_nmoesi_pref_find_and_lock");
	EV_MOD_NMOESI_PREF_FIND_AND_LOCK_PORT = esim_register_event_with_name(
		mod_handler_nmoesi_pref_find_and_lock, "mod_nmoesi_pref_find_and_lock_port");
	EV_MOD_NMOESI_PREF_FIND_AND_LOCK_ACTION = esim_register_event_with_name(
		mod_handler_nmoesi_pref_find_and_lock, "mod_nmoesi_pref_find_and_lock_action");
	EV_MOD_NMOESI_PREF_FIND_AND_LOCK_FINISH = esim_register_event_with_name(
		mod_handler_nmoesi_pref_find_and_lock, "mod_nmoesi_pref_find_and_lock_finish");

	EV_MOD_NMOESI_INVALIDATE_SLOT = esim_register_event_with_name( mod_handler_nmoesi_invalidate_slot, "mod_nmoesi_invalidate_slot");
	EV_MOD_NMOESI_INVALIDATE_SLOT_LOCK = esim_register_event_with_name( mod_handler_nmoesi_invalidate_slot, "mod_nmoesi_invalidate_slot_lock");
	EV_MOD_NMOESI_INVALIDATE_SLOT_ACTION = esim_register_event_with_name( mod_handler_nmoesi_invalidate_slot, "mod_nmoesi_invalidate_slot_action");
	EV_MOD_NMOESI_INVALIDATE_SLOT_UNLOCK = esim_register_event_with_name( mod_handler_nmoesi_invalidate_slot, "mod_nmoesi_invalidate_slot_unlock");
	EV_MOD_NMOESI_INVALIDATE_SLOT_FINISH = esim_register_event_with_name( mod_handler_nmoesi_invalidate_slot, "mod_nmoesi_invalidate_slot_finish");

	/*Main memory*/
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	EV_MOD_NMOESI_EXAMINE_ONLY_ONE_QUEUE_REQUEST=esim_register_event(mod_handler_nmoesi_request_main_memory);
        EV_MOD_NMOESI_EXAMINE_QUEUE_REQUEST=esim_register_event(mod_handler_nmoesi_request_main_memory);
        EV_MOD_NMOESI_ACCES_BANK = esim_register_event(mod_handler_nmoesi_request_main_memory);
        EV_MOD_NMOESI_TRANSFER_FROM_BANK=esim_register_event(mod_handler_nmoesi_request_main_memory);
        EV_MOD_NMOESI_REMOVE_MEMORY_CONTROLLER=esim_register_event(mod_handler_nmoesi_request_main_memory);
        EV_MOD_NMOESI_INSERT_MEMORY_CONTROLLER=esim_register_event(mod_handler_nmoesi_request_main_memory);

	/*Memory controller*/
	EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER = esim_register_event(mod_handler_nmoesi_find_and_lock_mem_controller);
	EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_PORT = esim_register_event(mod_handler_nmoesi_find_and_lock_mem_controller);
	EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_ACTION = esim_register_event(mod_handler_nmoesi_find_and_lock_mem_controller);
	EV_MOD_NMOESI_FIND_AND_LOCK_MEM_CONTROLLER_FINISH = esim_register_event(mod_handler_nmoesi_find_and_lock_mem_controller);
        //////////////////////////////////////////////////////////////////////////////////////////////////////////

	/* Read cache configuration file */
	mem_system_config_read();

	/* Initialize MMU */
	mmu_init();
}


void mem_system_done(void)
{
	int i;

	/* Dump report */
	mem_system_dump_report();

	/* Finalize MMU */
	mmu_done();

	////////////////////////////////////////////////////
        /*Free memory controller*/                        //
        mem_controller_free(mem_system->mem_controller);  //
        ////////////////////////////////////////////////////

	/* Free memory modules */
	for (i = 0; i < list_count(mem_system->mod_list); i++)
		mod_free(list_get(mem_system->mod_list, i));
	list_free(mem_system->mod_list);

	/* Free networks */
	for (i = 0; i < list_count(mem_system->net_list); i++)
		net_free(list_get(mem_system->net_list, i));
	list_free(mem_system->net_list);

	/* Free memory system */
	free(mem_system);
}

//////////////////////////////////////////////////////////////


void main_memory_dump_report(char * main_mem_report_file_name)
{

        FILE *f;
        struct mod_t * mod;
        struct mem_controller_t * mem_controller=mem_system->mem_controller;
        double total_bank_parallelism=0;
        double total_rank_parallelism=0;
	long long total_acces=0;
	long long total_normal_acces=0;
	long long total_pref_acces=0;
	long long total_wait_in_mc=0;
	long long total_normal_wait_in_mc=0;
	long long total_pref_wait_in_mc=0;



        /* Open file */
        f = file_open_for_write(main_mem_report_file_name);
        if (!f)
                return;

        /*Select main memory module*/
        for (int i = 0; i < list_count(mem_system->mod_list); i++)
        {
                mod = list_get(mem_system->mod_list, i);
                if(mod->kind==mod_kind_main_memory)
                        break;
        }

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

        for(int c=0; c<mod->num_regs_channel;c++)
         {
                total_acces+=mod->regs_channel[c].acceses;
		total_pref_acces+=mod->regs_channel[c].pref_accesses;
		total_normal_acces+=mod->regs_channel[c].normal_accesses;
                total_wait_in_mc+=mod->regs_channel[c].t_wait_send_request;
		total_normal_wait_in_mc+=mod->regs_channel[c].t_normal_wait_send_request;
		total_pref_wait_in_mc+=mod->regs_channel[c].t_pref_wait_send_request;
                for(int r=0;r<mod->regs_channel[c].num_regs_rank;r++)
                {
                        total_rank_parallelism+=mod->regs_channel[c].regs_rank[r].parallelism;
                        for(int b=0; b<mod->regs_channel[c].regs_rank[r].num_regs_bank;b++)
                                total_bank_parallelism+=mod->regs_channel[c].regs_rank[r].regs_bank[b].parallelism;
                }
         }


 	fprintf(f, "[MAIN MEMORY]\n");
	fprintf(f, "TotalTime = %f\n",total_acces ? (double) (total_wait_in_mc+mem_controller->t_acces_main_memory+mem_controller->t_transfer)/total_acces:0.0);
        fprintf(f, "AvgTimeWaitMCQueue = %f\n",total_acces ? (double) total_wait_in_mc/total_acces:0.0);
        fprintf(f, "AvgTimeAccesMM = %f\n",total_acces ? (double) mem_controller->t_acces_main_memory/total_acces :0.0);
	fprintf(f, "AvgTimeTransferFromMM = %f\n",total_acces ? (double)mem_controller->t_transfer/total_acces:0.0 );
	fprintf(f,"TotalAccessesMC = %lld\n", total_acces);

	/*Normal requests*/
	fprintf(f, "TotalTimeNormal = %f\n",total_normal_acces ? (double) (total_normal_wait_in_mc+mem_controller->t_normal_acces_main_memory+mem_controller->t_normal_transfer)/total_normal_acces:0.0);
        fprintf(f, "AvgTimeNormalWaitMCQueueN = %f\n",total_normal_acces ? (double) total_normal_wait_in_mc/total_normal_acces:0.0);
        fprintf(f, "AvgTimeNormalAccesMM = %f\n",total_normal_acces ? (double) mem_controller->t_normal_acces_main_memory/total_normal_acces :0.0);
	fprintf(f,"AvgTimeNormalTransferFromMM = %f\n",total_normal_acces?(double)mem_controller->t_normal_transfer/total_normal_acces:0.0 );
	fprintf(f,"TotalNormalAccessesMC = %lld\n", total_normal_acces);


	/*Prefetch requests*/
	fprintf(f, "TotalTimePrefetch = %f\n",total_pref_acces ? (double) (total_pref_wait_in_mc+mem_controller->t_pref_acces_main_memory+mem_controller->t_pref_transfer)/total_normal_acces:0.0);
        fprintf(f, "AvgTimePrefetchWaitMCQueueN = %f\n",total_pref_acces ? (double) total_pref_wait_in_mc/total_pref_acces:0.0);
        fprintf(f, "AvgTimePrefetchAccesMM = %f\n",total_pref_acces?(double) mem_controller->t_pref_acces_main_memory/total_pref_acces :0.0);
	fprintf(f,"AvgTimePrefetchTransferFromMM = %f\n",total_pref_acces?(double)mem_controller->t_pref_transfer/total_pref_acces:0.0 );
	fprintf(f,"TotalPrefetchAccessesMC = %lld\n", total_pref_acces);

	fprintf(f,"\n\n");


        for(int c=0; c<mod->num_regs_channel;c++){
                fprintf(f, "[Channel %d]\n", c);

                fprintf(f, "RowBufferHitPercent = %F\n", mod->regs_channel[c].acceses?
			 (double)mod->regs_channel[c].row_buffer_hits/mod->regs_channel[c].acceses : 0.0);
                fprintf(f, "AvgTimeWaitRequestSend = %f\n",mod->regs_channel[c].acceses?
			 (double)mod->regs_channel[c].t_wait_send_request/mod->regs_channel[c].acceses : 0.0);
                fprintf(f, "AvgTimeWaitRequestSendChannelBusy = %f\n",mod->regs_channel[c].num_requests_transfered ?
			(double)mod->regs_channel[c].t_wait_channel_busy/mod->regs_channel[c].num_requests_transfered : 0.0);
                fprintf(f, "AvgTimeWaitRequestTransfer = %f\n",mod->regs_channel[c].num_requests_transfered?
			(double) mod->regs_channel[c].t_wait_transfer_request/mod->regs_channel[c].num_requests_transfered : 0.0);
                fprintf(f, "AvgTimeRequestTransfer = %f\n",mod->regs_channel[c].num_requests_transfered ?
			(double)mod->regs_channel[c].t_transfer/mod->regs_channel[c].num_requests_transfered : 0.0);

		/*Normal requests*/
		fprintf(f, "NormalRowBufferHitPercent = %F\n", mod->regs_channel[c].normal_accesses?
			 (double)mod->regs_channel[c].row_buffer_hits_normal/mod->regs_channel[c].normal_accesses : 0.0);
                fprintf(f, "AvgTimeNormalWaitRequestSend = %f\n",mod->regs_channel[c].normal_accesses?
			 (double)mod->regs_channel[c].t_normal_wait_send_request/mod->regs_channel[c].normal_accesses : 0.0);
                fprintf(f, "AvgTimeNormalWaitRequestSendChannelBusy = %f\n",mod->regs_channel[c].num_normal_requests_transfered ?
			(double)mod->regs_channel[c].t_normal_wait_channel_busy/mod->regs_channel[c].num_normal_requests_transfered : 0.0);
                fprintf(f, "AvgTimeNormalWaitRequestTransfer = %f\n",mod->regs_channel[c].num_normal_requests_transfered?
			(double)mod->regs_channel[c].t_normal_wait_transfer_request/mod->regs_channel[c].num_normal_requests_transfered: 0.0);

		/*Prefetch requests*/
		fprintf(f, "PrefetchRowBufferHitPercent = %F\n", mod->regs_channel[c].pref_accesses?
			 (double)mod->regs_channel[c].row_buffer_hits_pref/mod->regs_channel[c].pref_accesses : 0.0);
                fprintf(f, "AvgTimePrefetchWaitRequestSend = %f\n",mod->regs_channel[c].pref_accesses?
			 (double)mod->regs_channel[c].t_pref_wait_send_request/mod->regs_channel[c].pref_accesses : 0.0);
                fprintf(f, "AvgTimePrefetchWaitRequestSendChannelBusy = %f\n",mod->regs_channel[c].num_pref_requests_transfered ?
			(double)mod->regs_channel[c].t_pref_wait_channel_busy/mod->regs_channel[c].num_pref_requests_transfered : 0.0);
                fprintf(f, "AvgTimePrefetchWaitRequestTransfer = %f\n",mod->regs_channel[c].num_pref_requests_transfered?
			(double)mod->regs_channel[c].t_pref_wait_transfer_request/mod->regs_channel[c].num_pref_requests_transfered: 0.0);


		fprintf(f,"\n");
                for(int r=0; r<mod->regs_channel[c].num_regs_rank; r++){

                        fprintf(f, "[Rank %d  (Channel %d)]\n", r, c);
                        fprintf(f, "RowBufferHitPercent = %f\n",mod->regs_channel[c].regs_rank[r].acceses ?
				(double)mod->regs_channel[c].regs_rank[r].row_buffer_hits/mod->regs_channel[c].regs_rank[r].acceses:0.0);
			fprintf(f, "NormalRowBufferHitPercent = %f\n",mod->regs_channel[c].regs_rank[r].normal_accesses ?
				(double)mod->regs_channel[c].regs_rank[r].row_buffer_hits_normal/mod->regs_channel[c].regs_rank[r].normal_accesses:0.0);
			fprintf(f, "PrefetchRowBufferHitPercent = %f\n",mod->regs_channel[c].regs_rank[r].pref_accesses ?
				(double)mod->regs_channel[c].regs_rank[r].row_buffer_hits_pref/mod->regs_channel[c].regs_rank[r].pref_accesses:0.0);
                        fprintf(f, "ParallelismPercent = %f\n\n",total_rank_parallelism ?
				(double)mod->regs_channel[c].regs_rank[r].parallelism/total_rank_parallelism : 0.0);

                        for(int b=0; b<mod->regs_channel[c].regs_rank[r].num_regs_bank;b++){

                                fprintf(f, "[Bank %d  (Rank %d Channel %d)]\n", b,r,c);
                                fprintf(f, "RowBufferHitPercent = %f\n",mod->regs_channel[c].regs_rank[r].regs_bank[b].acceses ?
					(double)mod->regs_channel[c].regs_rank[r].regs_bank[b].row_buffer_hits/
                                        mod->regs_channel[c].regs_rank[r].regs_bank[b].acceses: 0.0);
				fprintf(f, "NormalRowBufferHitPercent = %f\n",mod->regs_channel[c].regs_rank[r].regs_bank[b].normal_accesses ?
					(double)mod->regs_channel[c].regs_rank[r].regs_bank[b].row_buffer_hits_normal/
                                         mod->regs_channel[c].regs_rank[r].regs_bank[b].normal_accesses: 0.0);
				fprintf(f, "PrefetchRowBufferHitPercent = %f\n",mod->regs_channel[c].regs_rank[r].regs_bank[b].pref_accesses ?
					(double)mod->regs_channel[c].regs_rank[r].regs_bank[b].row_buffer_hits_pref/
                                        mod->regs_channel[c].regs_rank[r].regs_bank[b].pref_accesses: 0.0);
                                fprintf(f, "ParallelismPercent = %f\n", total_bank_parallelism ?
				       (double)mod->regs_channel[c].regs_rank[r].regs_bank[b].parallelism/total_bank_parallelism:0.0);
                                fprintf(f, "Conflicts = %lld\n", mod->regs_channel[c].regs_rank[r].regs_bank[b].conflicts);
                                fprintf(f, "AvgTimeWaitBankBusy = %f\n", mod->regs_channel[c].regs_rank[r].regs_bank[b].acceses ?
					(double)mod->regs_channel[c].regs_rank[r].regs_bank[b].t_wait/
                                        mod->regs_channel[c].regs_rank[r].regs_bank[b].acceses:0.0);
				fprintf(f,"AvgTimeNormalWaitBankBusy = %f\n",mod->regs_channel[c].regs_rank[r].regs_bank[b].normal_accesses?
					(double)mod->regs_channel[c].regs_rank[r].regs_bank[b].t_normal_wait/
                                        mod->regs_channel[c].regs_rank[r].regs_bank[b].normal_accesses:0.0);
				fprintf(f,"AvgTimePrefetchWaitBankBusy = %f\n",mod->regs_channel[c].regs_rank[r].regs_bank[b].pref_accesses?
					(double)mod->regs_channel[c].regs_rank[r].regs_bank[b].t_pref_wait/
                                        mod->regs_channel[c].regs_rank[r].regs_bank[b].pref_accesses:0.0);



                        }


                }


        }



	fprintf(f, "\n[QUEUES MEMORY CONTROLLER]\n\n");
	for(int i=0; i<mem_controller->num_queues;i++)
	{
		struct mem_controller_queue_t *normal = mem_controller->normal_queue[i];
        	fprintf(f, "[Normal Queue %d]\n",i);
       		fprintf(f, "AvgNumRequests = %f\n",mem_controller->n_times_queue_examined?
				(double)normal->total_requests / esim_cycle:0.0);
        	fprintf(f, "TimeFullPercent = %f\n", esim_cycle ? (double)normal->t_full/esim_cycle:0.0);
		float avg_req=mem_controller->n_times_queue_examined ? (double)normal->total_requests/mem_controller->n_times_queue_examined:0.0;
		fprintf(f, "TimeResponse = %f\n\n ", normal->total_insertions ? (double) (avg_req*esim_cycle)/normal->total_insertions :0.0);

		fprintf(f, "[Prefetch Queue %i]\n",i);
		fprintf(f, "AvgNumRequests = %f\n",mem_controller->n_times_queue_examined?
                                (double)mem_controller->pref_queue[i]->total_requests/mem_controller->n_times_queue_examined:0.0);
                fprintf(f, "TimeFullPercent = %f\n", esim_cycle ? (double)mem_controller->pref_queue[i]->t_full/esim_cycle:0.0);
		avg_req=mem_controller->n_times_queue_examined ? (double)mem_controller->pref_queue[i]->total_requests/mem_controller->n_times_queue_examined:0.0;
                fprintf(f, "TimeResponse = %f\n\n ", normal->total_insertions ? (double) (avg_req*esim_cycle)/normal->total_insertions :0.0);



	}

        /* Done */
        fclose(f);
}

///////////////////////////////////////////////////////////////


void mem_system_dump_report()
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
			- mod->no_retry_write_hits);
		fprintf(f, "\n");

		fprintf(f, "HitsPref = %lld\n", mod->hits_pref);
		fprintf(f, "\n");

		fprintf(f, "ProgrammedPrefetches = %lld\n", mod->programmed_prefetches);
		fprintf(f, "CompletedPrefetches = %lld\n", mod->completed_prefetches);
		fprintf(f, "CanceledPrefetches = %lld\n", mod->canceled_prefetches);
		fprintf(f, "CanceledPrefetchEndStream = %lld\n", mod->canceled_prefetches_end_stream);
		fprintf(f, "CanceledPrefetchMSHR = %lld\n", mod->canceled_prefetches_mshr);
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

