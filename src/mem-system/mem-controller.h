# include "mod-stack.h"
#ifndef MEM_CONTROLLER_H
#define MEM_CONTROLLER_H


extern struct str_map_t interval_kind_map;

extern int EV_MEM_CONTROLLER_ADAPT;
extern char * main_mem_report_file_name;

struct tuple_adapt_t
{

	struct mod_t * mod;
	struct linked_list_t * streams;

};


struct tuple_piggybacking_t
{
	unsigned int addr;
	int core;
	int thread;
};


/*States of request which  try to acces to main memory*/
enum acces_main_memory_state_t
{
        row_buffer_hit = 0,
        channel_busy,
        bank_accesed,
        row_buffer_miss

};

enum adapt_policy_t
{
	adapt_policy_none = 0,
	adapt_policy_enabled
};

struct mem_controller_adapt_stack_t
{
	struct mem_controller_t * mem_controller;
};


/* Memory controller*/

enum priority_t
{
	prio_threshold_normal_pref=0,
	prio_threshold_RowBufHit_FCFS,
	prio_threshold_normal_prefHit_prefGroup,
	prio_threshold_normal_prefHit_prefGroupCoalesce,
	prio_threshold_RowBufHit_normal_prefHit_prefGroup,
	prio_threshold_RowBufHit_normal_prefHit_prefGroupCoalesce,
	prio_threshold_RowBufHit_prefHit_normal_prefGroup,
	prio_threshold_RowBufHit_prefHit_normal_prefGroupCoalesce,
	prio_threshold_pref_normal,
	prio_threshold_prefRBH_normalRBH_normal_pref,
	prio_threshold_prefHit_normal_prefGroup,
	prio_threshold_prefHit_normal_prefGroupCoalesce,
	prio_threshold_prefHitRBH_normalRBH_normal_prefHit_prefGroup,
	prio_threshold_prefHitRBH_normalRBH_normal_prefHit_prefGroupCoalesce,
	prio_dynamic
};

enum priority_type_request_t
{
	prio_none = 0, // null priority
	prio_normal,
	prio_prefetch
};

enum step_priority_t
{
	step_threshold = 0,
	step_row_buffer_hit,
	step_row_buffer_miss
};

/*Policies memory controller queues*/
enum policy_mc_queue_t
{
	policy_one_queue_FCFS = 0, // one queue where prefetch and normal requests hace the same prioritiy, FCFS
	policy_prefetch_normal_queues, // prefetch queue and normal queue, normal is more priority
};

enum policy_coalesce_t
{
	policy_coalesce_disabled=0,
	policy_coalesce, //one queue for prefetch and normal, we can transfer several blocks from row buffer
	policy_coalesce_useful_blocks, //one queue for prefetch and normal, we transfer only useful blocks from row buffer (only coalesced blocks)
	policy_coalesce_delayed_request//one queue for prefetch and normal, we transfer several blocks from row buffer and then we check if some are in MC queue

};

struct mem_controller_queue_t
{
	/*Request queue*/
	struct linked_list_t *queue;

	/*Stadistics*/
	long long total_requests; // total number of stacks inserted inside the queue during all execution
	long long t_full; // cycles when queue is full
	long long instant_begin_full; // cycle when this queue is completed
	long long total_insertions;
};

struct mem_controller_t
{
	/*Max cycles waiting in the queue*/
	long long threshold;

	/*Mem controller is enabled?*/
	int enabled;

	/*Policy queues*/
	enum policy_mc_queue_t policy_queues;

	/*Priority queues*/
	enum priority_t priority_request_in_queue;

	/*Coalesce policy*/
	enum policy_coalesce_t coalesce;

	/*Number of queues*/
	int num_queues;

	/* Queues with different priority*/
	struct mem_controller_queue_t **pref_queue;
	struct mem_controller_queue_t **normal_queue;

	/*Photonic tecnology used in channels */
	int photonic_net;

	/*Number of stacks you can put inside*/
	int size_queue;

	/*Coalesce prefetch request into normal*/
	int piggybacking;

	/*There is a queue per bank or only one for all banks*/
	int queue_per_bank;

	/*List of bank registres to save information about memory acceses*/
	int num_regs_bank;
	int num_regs_rank;
	int num_regs_channel;

	int t_send_request; // time to send a request to main memory across the channel to main memory, in cycles

	/*ROW buffer*/
	int row_buffer_size;

	/*Channels*/
	struct reg_channel_t * regs_channel;

	/*Adaptative option*/
	int adaptative;
	float adapt_percent;
	int adapt_interval_kind;
	long long adapt_interval;
	struct linked_list_t * lived_streams;
	struct linked_list_t * useful_streams;

	/*Relation between cycles bus of main memory and cycles of processor*/
	int cycles_proc_bus;  // 1 cycle of bus= cycles_proc_bus cycles of proc

	/*Queue which has round robin expired*/
	int queue_round_robin;

	/*Last time that mc has been examined*/
	long long last_cycle;

	/*Stadistics*/
	long long t_wait; // time waiting in memory controller queues
	long long t_normal_wait;
	long long t_pref_wait;

	long long t_acces_main_memory;
	long long t_pref_acces_main_memory;
	long long t_normal_acces_main_memory;

	long long t_transfer; // time for tranfer a bloc from main memory
	long long t_pref_transfer;
	long long t_normal_transfer;

	long long t_inside_net;// time a request is travelling acroos the network

	long long n_times_queue_examined; // times which queue is examined
	long long accesses;
	long long non_coalesced_accesses;
	long long pref_accesses;
	long long normal_accesses;

	long long blocks_transfered;
	long long useful_blocks_transfered;

	int ** successive_hit; // inside a burst consecutive blocks
	int * burst_size; //counter of coalesced requests

	/*Interval acumulative stadistics*/
	long long last_accesses;
	long long last_pref_accesses;
	long long last_normal_accesses;
	long long last_t_mc_total;
	long long last_t_normal_mc_total;
	long long last_t_pref_mc_total;
	long long last_row_buffer_hits;
	long long last_normal_row_buffer_hits;
	long long last_pref_row_buffer_hits;
};

struct mem_controller_t * mem_controller_create(void);
void mem_controller_free(struct mem_controller_t * mem_controller);
void mem_controller_dump_report();
void mem_controller_normal_queue_add(struct mod_stack_t * stack);
void mem_controller_prefetch_queue_add(struct mod_stack_t * stack);
int mem_controller_remove(struct mod_stack_t * stack, struct mem_controller_queue_t * queue);
void mem_controller_init_main_memory(struct mem_controller_t *mem_controller, int channels, int ranks,
	int banks, int t_send_request, int row_size, int block_size,int cycles_proc_bus,  enum policy_mc_queue_t policy,
	enum priority_t priority, long long size_queue, long long cycles_wait_MCqueue, int queue_per_bank, enum policy_coalesce_t coalesce, struct reg_rank_t * regs_rank, int bandwith);
void mem_controller_update_requests_threshold(int cycles,struct mem_controller_t * mem_controller);
int mem_controller_queue_has_consumed_threshold(struct linked_list_t * queue, long long size);
struct mod_stack_t* mem_controller_select_request(int queues_examined, enum priority_t select, struct mem_controller_t * mem_controller);
void mem_controller_coalesce_pref_into_normal(struct mod_stack_t* stack);
int mem_controller_is_piggybacked(struct mod_stack_t * stack);
int mem_controller_count_requests_same_stream(struct mod_stack_t* stack, struct linked_list_t * queue);
//int mem_controller_queue_has_row_buffer_hit(struct linked_list_t * queue, long long size);

/*Coalesce*/
int mem_controller_calcul_number_blocks_transfered(struct mod_stack_t *stack);
int mem_controller_coalesce_acces_row_buffer( struct mod_stack_t * stack, struct linked_list_t * queue);
int mem_controller_coalesce_acces_between_blocks(struct mod_stack_t * stack, struct linked_list_t *queue, int block_min, int block_max);
unsigned int mem_controller_max_block(struct mod_stack_t *stack);
unsigned int mem_controller_min_block(struct mod_stack_t *stack);
int mem_controller_coalesce_acces_block(struct mod_stack_t * stack, struct linked_list_t *queue);
void mem_controller_sort_by_block(struct mod_stack_t * stack);
void mem_controller_count_successive_hits(struct linked_list_t * coalesced_stacks);

/*Memory controller queue*/
int mem_controller_stacks_normalQueues_count(struct mem_controller_t * mem_controller);
int mem_controller_stacks_prefQueues_count(struct mem_controller_t * mem_controller);
void mem_controller_queue_free(struct mem_controller_queue_t * mem_controller_queue);
struct mem_controller_queue_t * mem_controller_queue_create(void);
int mem_controller_get_bank_queue(int num_queue_examined, struct mem_controller_t * mem_controller);
int mem_controller_get_size_queue(struct mod_stack_t* stack);
void mem_controller_remove_in_queue(struct mod_stack_t* stack);
void mem_controller_register_in_queue(struct mod_stack_t* stack);


/*Policies*/
struct mod_stack_t * mem_controller_select_prefRBH_normalRBH_normal_pref_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_prefHit_normal_prefGroup_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_prefHitRBH_normalRBH_normal_prefHit_prefGroup_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_pref_normal_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_rbh_fcfs_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_normal_pref_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_rbh_normal_prefHit_prefGroup_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_rbh_prefHit_normal_prefGroup_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_normal_prefHit_prefGroup_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller);
struct mod_stack_t * mem_controller_select_dynamic_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller);
void mem_controller_mark_requests_same_stream(struct mod_stack_t* stack, struct linked_list_t * queue);


/*ROW BUFFER*/
int row_buffer_find_row(struct mem_controller_t * mem_controller, struct mod_t *mod, unsigned int addr, unsigned int *channel_ptr,unsigned int *rank_ptr,
	unsigned int *bank_ptr, unsigned int *row_ptr, int * tag_ptr, int *state_ptr);

/*Adaptative*/
void mem_controller_adapt_schedule(struct mem_controller_t * mem_controller);
void mem_controller_adapt_handler(int event, void *data);
void mem_controller_mark_stream(struct mod_stack_t* stack, struct linked_list_t *list);
int mem_controller_is_useful_stream(struct mod_stack_t* stack,struct mem_controller_queue_t * queue);

/* Trace of main memory accesses */
void main_mem_trace_init(char *file_name);
void main_mem_trace_done();
void main_mem_trace(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

#endif
