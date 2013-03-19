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

#ifndef MEM_SYSTEM_MODULE_H
#define MEM_SYSTEM_MODULE_H

#include <stdio.h>


/* Port */
struct mod_port_t
{
	/* Port lock status */
	int locked;
	long long lock_when;  /* Cycle when it was locked */
	struct mod_stack_t *stack;  /* Access locking port */

	/* Waiting list */
	struct mod_stack_t *waiting_list_head;
	struct mod_stack_t *waiting_list_tail;
	int waiting_list_count;
	int waiting_list_max;
};

/* String map for access type */
extern struct str_map_t mod_access_kind_map;

/* Access type */
enum mod_access_kind_t
{
	mod_access_invalid = 0,
	mod_access_load,
	mod_access_store,
	mod_access_nc_store,
	mod_access_prefetch,
	mod_access_read_request,
	mod_access_write_request,
	mod_access_invalidate //VVV
};

/* Module types */
enum mod_kind_t
{
	mod_kind_invalid = 0,
	mod_kind_cache,
	mod_kind_main_memory,
	mod_kind_local_memory
};

/* Type of address range */
enum mod_range_kind_t
{
	mod_range_invalid = 0,
	mod_range_bounds,
	mod_range_interleaved
};

///////////////////////////////////////
/*Control if it has to read or write no prefetched misses on a file*/
int misses_no_prefetch;
/////////////////////////////////////

#define MOD_ACCESS_HASH_TABLE_SIZE  17

/* Memory module */
struct mod_t
{
	/* Parameters */
	enum mod_kind_t kind;
	char *name;
	int block_size;
	int log_block_size;
	int latency;
	int dir_latency;
	int mshr_size;

	/* Main memory module */
	struct reg_channel_t * regs_channel;
	int num_regs_channel;

	/* Module level starting from entry points */
	int level;

	/* Address range served by module */
	enum mod_range_kind_t range_kind;
	union
	{
		/* For range_kind = mod_range_bounds */
		struct
		{
			unsigned int low;
			unsigned int high;
		} bounds;

		/* For range_kind = mod_range_interleaved */
		struct
		{
			unsigned int mod;
			unsigned int div;
			unsigned int eq;
		} interleaved;
	} range;

	/* Prefetch queue */
	struct linked_list_t *pq;

	/* Ports */
	struct mod_port_t *ports;
	int num_ports;
	int num_locked_ports;

	/* Accesses waiting to get a port */
	struct mod_stack_t *port_waiting_list_head;
	struct mod_stack_t *port_waiting_list_tail;
	int port_waiting_list_count;
	int port_waiting_list_max;

	/* Directory */
	struct dir_t *dir;
	int dir_size;
	int dir_assoc;
	int dir_num_sets;

	/* Waiting list of events */
	struct mod_stack_t *waiting_list_head;
	struct mod_stack_t *waiting_list_tail;
	int waiting_list_count;
	int waiting_list_max;

	/* Cache structure */
	struct cache_t *cache;

	/* Low and high memory modules */
	struct linked_list_t *high_mod_list;
	struct linked_list_t *low_mod_list;

	/* Smallest block size of high nodes. When there is no high node, the
	 * sub-block size is equal to the block size. */
	int sub_block_size;
	int num_sub_blocks;  /* block_size / sub_block_size */

	/* Interconnects */
	struct net_t *high_net;
	struct net_t *low_net;
	struct net_node_t *high_net_node;
	struct net_node_t *low_net_node;

	/* Access list */
	struct mod_stack_t *access_list_head;
	struct mod_stack_t *access_list_tail;
	int access_list_count;
	int access_list_max;

	/* Write access list */
	struct mod_stack_t *write_access_list_head;
	struct mod_stack_t *write_access_list_tail;
	int write_access_list_count;
	int write_access_list_max;

	/* Number of in-flight coalesced accesses. This is a number
	 * between 0 and 'access_list_count' at all times. */
	int access_list_coalesced_count;

	/* Hash table of accesses */
	struct
	{
		struct mod_stack_t *bucket_list_head;
		struct mod_stack_t *bucket_list_tail;
		int bucket_list_count;
		int bucket_list_max;
	} access_hash_table[MOD_ACCESS_HASH_TABLE_SIZE];

	/* For coloring algorithm used to check collisions between CPU and GPU
	 * memory hierarchies. Remove when fused. */
	int color;

	/* Statistics */
	long long accesses;
	long long hits;
	long long hits_pref; /* Hits de stacks de prefetch en els moduls inferiors */

	long long reads;
	long long effective_reads;
	long long effective_read_hits;
	long long writes;
	long long effective_writes;
	long long effective_write_hits;
	long long nc_writes;
	long long effective_nc_writes;
	long long effective_nc_write_hits;
	long long evictions;

	long long blocking_reads;
	long long non_blocking_reads;
	long long read_hits;
	long long blocking_writes;
	long long non_blocking_writes;
	long long write_hits;
	long long blocking_nc_writes;
	long long non_blocking_nc_writes;
	long long nc_write_hits;

	long long read_retries;
	long long write_retries;
	long long nc_write_retries;

	long long no_retry_accesses;
	long long no_retry_hits;
	long long no_retry_reads;
	long long no_retry_read_hits;
	long long no_retry_writes;
	long long no_retry_write_hits;
	long long no_retry_nc_writes;
	long long no_retry_nc_write_hits;

	/* Prefetch */
	long long programmed_prefetches;
	long long completed_prefetches;
	long long canceled_prefetches;
	long long useful_prefetches;

	long long prefetch_retries;

	long long delayed_hits; /* Hit on a block being brougth by a prefetch */
	long long delayed_hit_cycles; /* Cicles lost due delayed hits */
	long long delayed_hits_cycles_counted; /* Number of delayed hits whose lost cycles has been counted */

	long long single_prefetches; /* Prefetches on hit */
	long long group_prefetches; /* Number of GROUPS */
	long long canceled_prefetch_groups;

	long long canceled_prefetches_end_stream;
	long long canceled_prefetches_mshr;

	long long up_down_hits;
	long long up_down_head_hits;
	long long down_up_read_hits;
	long long down_up_write_hits;

	long long fast_resumed_accesses;
	long long write_buffer_read_hits;
	long long write_buffer_write_hits;
	long long write_buffer_prefetch_hits;

	long long stream_evictions;

	/* Silent replacement */
	long long down_up_read_misses;
	long long down_up_write_misses;
	long long block_already_here;

	long long faults_mem_without_pref;
};

////////////////////////////////////////////////////////////

/*--------- MAIN MEMORY STRUCTURES------*/
/* Reg bank*/
struct reg_bank_t{

        int row_is_been_accesed; // row which is been accessed
        int row_buffer; // row which is inside row buffer
        int is_been_accesed;// show if a bank is been accedid for some instruction

        /*Stadistics*/
        long long row_buffer_hits; // number of acceses to row buffer which are hits
   	long long row_buffer_hits_pref;
	long long row_buffer_hits_normal;
	int t_row_buffer_hit; // cycles needed to acces the bank if the row is in row buffer
        int t_row_buffer_miss; // cycles needed to acces the bank if the row isn't in row buffer
        long long conflicts;
        long long acceses;
        long long pref_accesses;
	long long normal_accesses;
	long long t_wait; // time waited by the requestes to acces to bank
    	long long t_pref_wait;
	long long t_normal_wait;
	long long parallelism;////////////
};


/* Reg rank*/
struct reg_rank_t{

        struct reg_bank_t * regs_bank;
        int num_regs_bank;
        int is_been_accesed; //true or false

        /*Stadistics*/
        long long parallelism;// number of acceses which acces when that rank is been accesed by others
        long long acceses;
	long long pref_accesses;
	long long normal_accesses;
        long long row_buffer_hits; // number of acceses to row buffer which are hits
	long long row_buffer_hits_pref;
	long long row_buffer_hits_normal;
};

/*Reg channels*/
enum channel_state_t
{
        channel_state_free = 0,
        channel_state_busy
};


struct reg_channel_t{
	enum channel_state_t state; // busy, free
	int bandwith;
	struct reg_rank_t * regs_rank; // ranks which this channels connects with
	int num_regs_rank;

	/*Stadistics*/
	long long acceses;
	long long pref_accesses;
	long long normal_accesses;
	//int parallelism_rank; // number of acceses which acces to rank accesed by others
	long long t_wait_send_request; // time waiting to send a request because the channel is busy or the bank is busy
	long long t_pref_wait_send_request;
	long long t_normal_wait_send_request;
	long long t_wait_channel_busy;  // time waiting to send a request because the channel is busy
	long long t_normal_wait_channel_busy;
	long long t_pref_wait_channel_busy;
	long long t_wait_transfer_request; // time waiting to transfer the block
	long long t_normal_wait_transfer_request; // time waiting to transfer the block
	long long t_pref_wait_transfer_request; // time waiting to transfer the bloc
	long long t_transfer;
	long long num_requests_transfered;
	long long row_buffer_hits; // number of acceses to row buffer which are hits
	long long row_buffer_hits_pref;
	long long row_buffer_hits_normal;
	long long num_pref_requests_transfered;
	long long num_normal_requests_transfered;
};

/*States of request which  try to acces to main memory*/
enum acces_main_memory_state_t
{
        row_buffer_hit = 0,
        channel_busy,
        bank_accesed,
        row_buffer_miss

};

////////////////////////// MAIN MEMORY  //////////////////////////
struct reg_bank_t* regs_bank_create( int num_banks, int t_row_hit, int t_row_miss);
struct reg_rank_t* regs_rank_create( int num_ranks, int num_banks, int t_row_buffer_miss, int t_row_buffer_hit);
struct reg_channel_t* regs_channel_create( int num_channels, int num_ranks, int num_banks, int bandwith,
                                        int t_row_buffer_miss, int t_row_buffer_hit);
void reg_channel_free(struct reg_channel_t * channels, int num_channels);
void reg_rank_free(struct reg_rank_t * ranks, int num_ranks);
void main_memory_dump_report(char * main_mem_report_file_name);////
///////////////////////////////////////////////////////////////////
struct mod_t *mod_create(char *name, enum mod_kind_t kind, int num_ports,
	int block_size, int latency);
void mod_free(struct mod_t *mod);
void mod_dump(struct mod_t *mod, FILE *f);
void mod_stack_set_reply(struct mod_stack_t *stack, int reply);
struct mod_t *mod_stack_set_peer(struct mod_t *peer, int state);

long long mod_access(struct mod_t *mod, enum mod_access_kind_t access_kind,
	unsigned int addr, int *witness_ptr, struct linked_list_t *event_queue,
	void *event_queue_item, int core, int thread, int prefetch);
int mod_can_access(struct mod_t *mod, unsigned int addr);

int mod_find_block(struct mod_t *mod, unsigned int addr, int *set_ptr, int *way_ptr,
	int *tag_ptr, int *state_ptr, int *prefetched_prt);

void mod_lock_port(struct mod_t *mod, struct mod_stack_t *stack, int event);
void mod_unlock_port(struct mod_t *mod, struct mod_port_t *port,
	struct mod_stack_t *stack);

void mod_access_start(struct mod_t *mod, struct mod_stack_t *stack,
	enum mod_access_kind_t access_kind);
void mod_access_finish(struct mod_t *mod, struct mod_stack_t *stack);

int mod_in_flight_access(struct mod_t *mod, long long id, unsigned int addr);
struct mod_stack_t *mod_in_flight_address(struct mod_t *mod, unsigned int addr,
	struct mod_stack_t *older_than_stack);
struct mod_stack_t *mod_in_flight_write(struct mod_t *mod,
	struct mod_stack_t *older_than_stack);

int mod_serves_address(struct mod_t *mod, unsigned int addr);
struct mod_t *mod_get_low_mod(struct mod_t *mod, unsigned int addr);

int mod_get_retry_latency(struct mod_t *mod);

struct mod_stack_t *mod_can_coalesce(struct mod_t *mod,
	enum mod_access_kind_t access_kind, unsigned int addr,
	struct mod_stack_t *older_than_stack);
void mod_coalesce(struct mod_t *mod, struct mod_stack_t *master_stack,
	struct mod_stack_t *stack);

/* Prefetch */
int mod_find_pref_block(struct mod_t *mod, unsigned int addr, int *pref_stream_ptr, int* pref_slot_ptr);
int mod_find_block_in_stream(struct mod_t *mod, unsigned int addr, int stream);

#endif

