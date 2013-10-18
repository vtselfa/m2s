# include "mod-stack.h"
#ifndef MEM_CONTROLLER_H
#define MEM_CONTROLLER_H


extern struct str_map_t interval_kind_map;

extern int EV_MEM_CONTROLLER_ADAPT;
extern int EV_MEM_CONTROLLER_REPORT;
extern char * main_mem_report_file_name;

struct tuple_adapt_t
{

	struct mod_t * mod;
	struct linked_list_t * streams;

};

struct mem_controller_report_stack_t
{
	struct mem_controller_t *mem_controller;
	struct line_writer_t *line_writer;
	long long inst_count;
	long long t_acces;
	long long t_wait;
	long long t_transfer;
	long long accesses;
	long long served;
	long long normal_accesses;
	long long pref_accesses;

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
	prio_dynamic,
	prio_FCFS_normal_pref /* if only one queue enable, it is FCFS*/
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
	long long current_request_num;

	/*Stadistics*/
	long long total_requests; // total number of stacks inserted inside the queue during all execution
	long long t_full; // cycles when queue is full
	long long instant_begin_full; // cycle when this queue is completed
	long long total_insertions;
};

struct row_buffer_table_entry_t
{
	long long int lru; // cycle accessed
	int row;
	long long int  reserved; // stack id which has reserved this space
	int accessed;
	unsigned int block_max; //used for the coalesce
	unsigned int block_min;

	/*Stadistics*/
	struct linked_list_t * used_blocks;// save accesed blocks in a specific row

};

struct row_buffer_table_set_t
{
	int bank;
	struct  row_buffer_table_entry_t* entries;
};

struct row_buffer_table_t
{
	long long accesses;
	long long normal_accesses;
	long long pref_accesses;
	long long hit_accesses;
	long long pref_hit_accesses;
	long long normal_hit_accesses;
	long long transfered_blocks;
	long long useful_blocks;
	long long num_transfers;

	int coalesce_enabled; // enable bank line coalesce transfer
	int num_entries;
	unsigned int assoc; // associativity
	struct row_buffer_table_set_t * sets; // sets of assoc entries , sets*assoc = num_entries
	unsigned int core; // this table is used by core
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
	int enable_row_buffer_table;
	struct row_buffer_table_t ** row_buffer_table; // a row buffer table inside mem controller
	int row_buf_per_bank_per_ctx; // indicates if row buffer entries in the table are distribuited betwen contexts, and between banks for each context
	int num_tables;

	/*Channels*/
	struct reg_channel_t * regs_channel;
	int bandwith;



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
	long long num_requests_transfered;
	long long row_buffer_hits;
	long long row_buffer_hits_pref;
	long long row_buffer_hits_normal;

	long long blocks_transfered;
	long long useful_blocks_transfered;

	int ** successive_hit; // inside a burst consecutive blocks
	int * burst_size; //counter of coalesced requests

	int num_cores;
	long long *t_core_wait;
	long long *t_core_acces;
	long long *t_core_transfer; 
	long long * core_normal_mc_accesses;
	long long * core_pref_mc_accesses;
	long long * core_mc_accesses;
	long long * core_row_buffer_hits;
	long long **core_row_buffer_hits_per_bank;
	long long **core_mc_accesses_per_bank;

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

	/* Reporting statistics at intervals */
	int report_enabled;
	struct mem_controller_report_stack_t *report_stack;
	int report_interval;
	enum interval_kind_t report_interval_kind;
	FILE *report_file;
};

struct mem_controller_t * mem_controller_create(void);
void mem_controller_free(struct mem_controller_t * mem_controller);
void mem_controller_dump_report();
void mem_controller_dump_core_report();
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
struct mod_stack_t * mem_controller_select_FCFS_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller);
void mem_controller_mark_requests_same_stream(struct mod_stack_t* stack, struct linked_list_t * queue);

/*Stadistics*/
void mem_controller_report_schedule(struct mem_controller_t *mem_controller);
void mem_controller_report_handler(int event, void *data);


/*ROW BUFFER*/
int row_buffer_find_row(struct mem_controller_t * mem_controller, struct mod_stack_t * stack, struct mod_t *mod, unsigned int addr, unsigned int *channel_ptr,unsigned int *rank_ptr,
	unsigned int *bank_ptr, unsigned int *row_ptr, int * tag_ptr, int *state_ptr);
void mem_controller_row_buffer_allocate_row(struct mod_stack_t *stack);


/*Table*/
struct row_buffer_table_t* mem_controller_get_row_buffer_table(struct mem_controller_t * mem_controller, int pid);
void mem_controller_row_buffer_table_create(struct mem_controller_t *mem_controller, int enable_rbtable, int assoc_table, int enable_coalesce, int buf_per_bank_per_ctx ,int ranks, int banks);
void mem_controller_row_buffer_table_per_ctx_create(struct mem_controller_t *mem_controller, int enable_rbtable, int assoc_table, int enable_coalesce, int buf_per_bank_per_ctx ,int ranks, int banks);
void mem_controller_row_buffer_table_free(struct mem_controller_t * mem_controller);
void mem_controller_row_buffer_table_reserve_entry(struct mod_stack_t *stack);
struct row_buffer_table_entry_t *mem_controller_row_buffer_table_get_entry(struct mod_stack_t *stack);
struct row_buffer_table_set_t* mem_controller_row_buffer_table_get_set(struct mod_stack_t *stack);
struct row_buffer_table_entry_t *mem_controller_row_buffer_table_get_reserved_entry(struct mod_stack_t *stack);
void mem_controller_row_buffer_table_reset_used_block(struct mod_stack_t *stack);
int mem_controller_row_buffer_table_count_used_block(struct mod_stack_t *stack);
int mem_controller_coalesce_acces_row_buffer_table( struct mod_stack_t * stack, struct linked_list_t * queue);

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
