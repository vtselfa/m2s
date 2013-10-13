#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <zlib.h>

#include <arch/x86/timing/cpu.h>
#include <arch/x86/emu/loader.h>
#include <arch/x86/emu/context.h>

#include <lib/esim/esim.h>
#include <lib/esim/trace.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/file.h>
#include <lib/util/linked-list.h>
#include <lib/util/line-writer.h>
#include <lib/util/list.h>
#include <lib/util/misc.h>
#include <lib/util/string.h>

#include "bank.h"
#include "cache.h"
#include "channel.h"
#include "mem-controller.h"
#include "mem-system.h"
#include "mod-stack.h"

static char *help_mem_controller_report =
	"The mod report file shows some relevant statistics related to cache performance\n"
	"at specific intervals.\n"
	"The following fields are shown in each record:\n"
	"\n"
	"  <cycle>\n"
	"      Current simulation cycle.\n"
	"\n"
	"  <inst>\n"
	"      Current simulation instruction.\n"
	"\n";

int EV_MEM_CONTROLLER_ADAPT;
int EV_MEM_CONTROLLER_REPORT;

/* For main memory access traces */
gzFile trace_file;



int row_buffer_find_row(struct mem_controller_t * mem_controller, struct mod_t *mod, unsigned int addr, unsigned int *channel_ptr,
	unsigned int *rank_ptr, unsigned int *bank_ptr, unsigned int *row_ptr,  int * tag_ptr, int *state_ptr)
{
	//struct cache_t *cache = mod->cache;

	unsigned int channel;
	unsigned int rank;
	unsigned int bank;
	unsigned int row;
	unsigned int row_buffer;
	int tag;

	row_buffer = addr %  mem_controller->row_buffer_size;

	unsigned int num_ranks = mem_controller->num_regs_rank ;
	unsigned int num_banks = mem_controller->num_regs_bank ;
	unsigned int num_channels = mem_controller->num_regs_channel ;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
	int set_rbtable = ((addr >> log2_row_size) % (num_banks*num_ranks));
	//unsigned int num_columns= mem_controller->row_buffer_size/cache->block_size;

	row= (addr >> (log2_row_size+ log_base2(num_banks)+ log_base2(num_ranks)));
	rank = (addr >> (log2_row_size+ log_base2(num_banks))) % num_ranks;
	bank = (addr >> log2_row_size) % num_banks;
	channel=(row_buffer >>7 )%num_channels;
	tag=addr &  mem_controller->row_buffer_size;


	PTR_ASSIGN(rank_ptr, rank);
	PTR_ASSIGN(bank_ptr, bank);
	PTR_ASSIGN(row_ptr, row);
	PTR_ASSIGN(channel_ptr, channel);
	PTR_ASSIGN(tag_ptr, tag);


	/*If it's row buffer table, it is inside of mem controller*/
	if(mem_controller->enable_row_buffer_table)
	{
		for(int i =0 ;i <mem_controller->row_buffer_table->assoc; i++)
		{		
			assert(mem_controller->row_buffer_table->sets[set_rbtable].bank == set_rbtable);
			if(mem_controller->row_buffer_table->sets[set_rbtable].entries[i].row==row)
			{
				if(mem_controller->row_buffer_table->sets[set_rbtable].entries[i].accessed ||
				mem_controller->row_buffer_table->sets[set_rbtable].entries[i].reserved!=-1)
				{
					PTR_ASSIGN(state_ptr, bank_accesed); // line being accessed
					//printf(" espera xk estan accedint a entra %d bank %d\n",i, set_rbtable);
					return 0;
				}else
				{
					PTR_ASSIGN(state_ptr, row_buffer_hit);
					//printf(" hit a entra %d bank %d\n",i, set_rbtable);
					return 1;
				}
			}
		}

		/*miss in row buffer table, we have to ensure a place to put the row*/
		/*for(int i =0 ;i <mem_controller->row_buffer_table->assoc; i++)
		{		
			if(mem_controller->row_buffer_table->sets[set_rbtable].entries[i].reserved==-1 && !mem_controller->row_buffer_table->sets[set_rbtable].entries[i].accessed) //not reserved
			{
				PTR_ASSIGN(state_ptr, row_buffer_miss); // is not inside table
				return 1;	
			}
		}
		PTR_ASSIGN(state_ptr, bank_accesed); // all entries  reserved
		*/
		
	}

	/*Is the channel free?*/
	if(mem_controller->regs_channel[channel].state==channel_state_busy){

		PTR_ASSIGN(state_ptr, channel_busy);
		return 0;
	}

	/*Is the bank been acceded?*/
	if(mem_controller->regs_channel[channel].regs_rank[rank].regs_bank[bank].is_been_accesed){
		PTR_ASSIGN(state_ptr, bank_accesed);
		return 0;
	}


	

	if(!mem_controller->enable_row_buffer_table)
	{
		/*Is the row inside the row buffer?*/
		for(int i=0; i<mem_controller->regs_channel[channel].regs_rank[rank].regs_bank[bank].row_buffer_per_bank; i++)
		{
			if(mem_controller->regs_channel[channel].regs_rank[rank].regs_bank[bank].row_buffers[i].row==row)
			{
				PTR_ASSIGN(state_ptr, row_buffer_hit);
				return 1;
			}
		}
		PTR_ASSIGN(state_ptr, row_buffer_miss);
		return 1;
	}else{
		
		PTR_ASSIGN(state_ptr, row_buffer_miss); // is not inside table
		return 1;
	}




}



//////////////////////////////////////////////////////////////////////////////////
struct mem_controller_queue_t *mem_controller_queue_create(void){
	struct mem_controller_queue_t *mem_controller_queue;
	mem_controller_queue = xcalloc(1, sizeof(struct mem_controller_queue_t));

	/* Create stack list */
	mem_controller_queue->queue = linked_list_create();


	return mem_controller_queue;

}


void mem_controller_queue_free(struct mem_controller_queue_t *mem_controller_queue)
{

	linked_list_head(mem_controller_queue->queue);

	while (linked_list_count(mem_controller_queue->queue)>0)
	{
		free( linked_list_get(mem_controller_queue->queue));
		linked_list_remove(mem_controller_queue->queue);
	}
	linked_list_free(mem_controller_queue->queue);
	free(mem_controller_queue);

}
///////////////////////////////////////////////////////////////////////////
struct mem_controller_t *mem_controller_create(void){
	struct mem_controller_t *mem_controller;
	mem_controller = xcalloc(1, sizeof(struct mem_controller_t));
	mem_controller->lived_streams = linked_list_create();
	mem_controller->useful_streams = linked_list_create();
	
	


	return mem_controller;
}


void mem_controller_init_main_memory(struct mem_controller_t *mem_controller, int channels, int ranks, int banks, int t_send_request, int row_size, int block_size,int cycles_proc_bus, enum policy_mc_queue_t policy, enum priority_t priority, long long size_queue,  long long threshold, int queue_per_bank, enum policy_coalesce_t coalesce, struct reg_rank_t *regs_rank, int bandwith, int enable_rbtable, int assoc_table){

	mem_controller->num_queues=1;

	if(queue_per_bank)
		mem_controller->num_queues=ranks*banks;

	mem_controller->num_regs_channel = channels;
	mem_controller->num_regs_rank = ranks;
	mem_controller->num_regs_bank = banks;
	mem_controller->row_buffer_size=row_size;
	mem_controller->t_send_request=t_send_request;
	mem_controller->cycles_proc_bus=cycles_proc_bus;
	mem_controller->size_queue=size_queue;
	mem_controller->policy_queues=policy;
	mem_controller->threshold=threshold;
	mem_controller->priority_request_in_queue=priority;
	mem_controller->queue_per_bank=queue_per_bank;
	mem_controller->queue_round_robin=0;
	mem_controller->coalesce= coalesce;
	mem_controller->bandwith = bandwith;

	mem_controller->normal_queue = xcalloc(mem_controller->num_queues, sizeof(struct mem_controller_queue_t *));
	mem_controller->pref_queue = xcalloc(mem_controller->num_queues, sizeof(struct mem_controller_queue_t *));
	for(int i=0; i<mem_controller->num_queues;i++)
	{
		mem_controller->normal_queue[i]=mem_controller_queue_create();
		mem_controller->pref_queue[i]=mem_controller_queue_create();
	}

	mem_controller->burst_size = xcalloc(row_size/block_size,sizeof(int*));
	mem_controller->successive_hit = xcalloc(row_size/block_size,sizeof(int*));
	for(int i = 0; i < row_size / block_size; i++)
		mem_controller->successive_hit[i] = xcalloc(row_size/block_size,sizeof(int));

	mem_controller->regs_channel = regs_channel_create(channels, ranks, banks, bandwith, regs_rank);
	
	/*ROw buffer table*/
	mem_controller->enable_row_buffer_table = enable_rbtable;
	if(enable_rbtable)
	{
		mem_controller->row_buffer_table = xcalloc(1, sizeof(struct row_buffer_table_t ));
		mem_controller->row_buffer_table->assoc = assoc_table;
		mem_controller->row_buffer_table->num_entries = assoc_table*banks*ranks;
		mem_controller->row_buffer_table->sets = xcalloc(ranks*banks, sizeof(struct row_buffer_table_set_t ));
		for(int i=0; i<ranks*banks;i++)
		{
			mem_controller->row_buffer_table->sets[i].bank=i;
			mem_controller->row_buffer_table->sets[i].entries = xcalloc(assoc_table, sizeof(struct row_buffer_table_entry_t));
			for(int j=0; j<assoc_table;j++)	
			{
				mem_controller->row_buffer_table->sets[i].entries[j].row=-1;
				mem_controller->row_buffer_table->sets[i].entries[j].reserved=-1;
				mem_controller->row_buffer_table->sets[i].entries[j].lru=-1;
			}
		}
	}


	/*mem_controller->row_in_buffer_banks = xcalloc(channels, sizeof(int **));
	if (!mem_controller->row_in_buffer_banks)
		fatal("%s: out of memory", __FUNCTION__);

	for(int c=0; c<channels;c++){

		mem_controller->row_in_buffer_banks[c] = xcalloc(ranks, sizeof(int *));
		if (!mem_controller->row_in_buffer_banks[c])
			fatal("%s: out of memory", __FUNCTION__);

		for(int r=0; r<ranks;r++){
			mem_controller->row_in_buffer_banks[c][r] = xcalloc(banks, sizeof( int));
			if (!mem_controller->row_in_buffer_banks[c][r])
				fatal("%s: out of memory", __FUNCTION__);

			for(int b=0; b<banks;b++)
				mem_controller->row_in_buffer_banks[c][r][b]=-1;
		}
	}*/


}


void mem_controller_free(struct mem_controller_t *mem_controller){


	struct mod_t *mod = list_get(mem_system->mod_list,0);
	struct tuple_adapt_t *tuple;

	/* Free prefetch queue */
	for(int i=0; i<mem_controller->num_queues;i++)
		mem_controller_queue_free(mem_controller->pref_queue[i]);
	free(mem_controller->pref_queue);

	/* Free normal queue */
	for(int i=0; i<mem_controller->num_queues;i++)
		mem_controller_queue_free(mem_controller->normal_queue[i]);
	free(mem_controller->normal_queue);

	for(int i=0; i< mem_controller->row_buffer_size/mod->cache->block_size;i++)
		free(mem_controller->successive_hit[i]);

	free(mem_controller->successive_hit);
	free(mem_controller->burst_size);
	///////////////////////////////////////////////////

	free(mem_controller->regs_channel);

	linked_list_head(mem_controller->useful_streams);
	while(!linked_list_is_end(mem_controller->useful_streams))
	{
		tuple=linked_list_get(mem_controller->useful_streams);
		linked_list_remove(mem_controller->useful_streams);
		linked_list_free(tuple->streams);
		free(tuple);
	}

	linked_list_head(mem_controller->lived_streams);
	while(!linked_list_is_end(mem_controller->lived_streams))
	{
		tuple=linked_list_get(mem_controller->lived_streams);
		linked_list_remove(mem_controller->lived_streams);
		linked_list_free(tuple->streams);
		free(tuple);
	}

	linked_list_free(mem_controller->useful_streams);
	linked_list_free(mem_controller->lived_streams);

	/* Interval report */
	if(mem_controller->report_stack)
	{
		line_writer_free(mem_controller->report_stack->line_writer);
		free(mem_controller->report_stack);
	}
	file_close(mem_controller->report_file);


	/*Free table*/
	if(mem_controller->enable_row_buffer_table)
	{
		for(int i=0; i<mem_controller->num_regs_rank*mem_controller->num_regs_bank;i++)
			free(mem_controller->row_buffer_table->sets[i].entries);
				
		free(mem_controller->row_buffer_table->sets);
		free(mem_controller->row_buffer_table);
	}

	free(mem_controller);
}


void mem_controller_normal_queue_add(struct mod_stack_t * stack)
{
	struct mem_controller_t * mem_controller=stack->mod->mem_controller;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
	unsigned int bank;
	long long ctx_threshold;
	assert(stack->client_info->core>=0 && stack->client_info->thread>=0);
	struct x86_ctx_t *ctx = x86_cpu->core[stack->client_info->core].thread[stack->client_info->thread].ctx;

	//row_buffer = stack->addr &  mem_controller->row_buffer_size;
//	channel=(row_buffer >>7 )%mem_controller->num_regs_channel;


	if(mem_controller->queue_per_bank)
		bank = ((stack->addr >> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));
	else
		bank=0;

	

	stack->threshold =mem_controller->threshold;

	/*TO avoid fairness*/
	if(ctx!=NULL)
	{
		ctx_threshold = ctx->loader->max_cycles_wait_MC;
		if(ctx_threshold != 100000000000) // if threshold if different than by default, context threshold is priorier
			stack->threshold = ctx_threshold;

		if(ctx->loader->mc_accesses_per_bank==NULL)
		{
			assert(ctx->loader->row_buffer_hits_per_bank==NULL);
			ctx->loader->mc_accesses_per_bank= xcalloc(mem_controller->num_regs_bank* mem_controller->num_regs_rank, sizeof(long long));
			ctx->loader->row_buffer_hits_per_bank= xcalloc(mem_controller->num_regs_bank* mem_controller->num_regs_rank, sizeof(long long));
			if(mem_controller->queue_per_bank)
				ctx->loader->num_banks = mem_controller->num_regs_bank* mem_controller->num_regs_rank;
			else
				ctx->loader->num_banks = 1;
			ctx->loader->num_ranks = mem_controller->num_regs_rank;
		}
		ctx->loader->mc_accesses_per_bank[bank]++;
		ctx->loader->mc_accesses++;
		
		
	}

	/*Add in queue*/
	linked_list_tail(mem_controller->normal_queue[bank]->queue);
	linked_list_add(mem_controller->normal_queue[bank]->queue, stack);
	linked_list_head(mem_controller->normal_queue[bank]->queue);

	/*Now queue is full?*/
	 if(linked_list_count(mem_controller->normal_queue[bank]->queue)==mem_controller->size_queue)
         	mem_controller->normal_queue[bank]->instant_begin_full = esim_cycle();

 	mem_controller->normal_queue[bank]->total_insertions++;

}


void mem_controller_prefetch_queue_add(struct mod_stack_t * stack){


	struct mem_controller_t * mem_controller=stack->mod->mem_controller;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
	unsigned int bank;
	long long ctx_threshold;
	struct x86_ctx_t *ctx = x86_cpu->core[stack->client_info->core].thread[stack->client_info->thread].ctx;
              
	
	assert(stack->client_info->core>=0 && stack->client_info->thread>=0);
	assert(stack->client_info->stream_request_kind>=0);

	if(mem_controller->queue_per_bank)
		bank =((stack->addr >> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));
	else
		bank=0;
	/*If mem controller policy is adaptative, mark useful and lived streams*/
	if(mem_controller->adaptative  && stack->client_info->stream_request_kind == stream_request_single)
	{
		assert(stack->client_info->stream>=0);
		mem_controller_mark_stream(stack, mem_controller->lived_streams);

		/*An useful pref has to be a row buffer hit, a second stream hit and
		another hit has to be enqueued*/
		if(stack->client_info->stream_request_kind == stream_request_single && mem_controller_is_useful_stream(stack, mem_controller->pref_queue[bank]))
			mem_controller_mark_stream(stack, mem_controller->useful_streams);
	}

	/*If mem controller policy is dynamic, mark priority*/
	if(mem_controller->priority_request_in_queue == prio_dynamic)
		mem_controller_mark_requests_same_stream(stack,mem_controller->pref_queue[bank]->queue);


	/*TO avoid fairness*/
	stack->threshold=mem_controller->threshold;

	if(ctx!=NULL)
	{
		ctx_threshold = x86_cpu->core[stack->client_info->core].thread[stack->client_info->thread].ctx->loader->max_cycles_wait_MC;

		if(ctx_threshold != 100000000000) // if threshold if different than by default, context threshold is priorier
			stack->threshold = ctx_threshold;
		
		if(ctx->loader->mc_accesses_per_bank==NULL)
		{
			assert(ctx->loader->row_buffer_hits_per_bank==NULL);
			ctx->loader->mc_accesses_per_bank= xcalloc(mem_controller->num_regs_bank* mem_controller->num_regs_rank,sizeof(long long));
			ctx->loader->row_buffer_hits_per_bank= xcalloc(mem_controller->num_regs_bank* mem_controller->num_regs_rank,sizeof(long long));
			if(mem_controller->queue_per_bank)
				ctx->loader->num_banks = mem_controller->num_regs_bank* mem_controller->num_regs_rank;
			else
				ctx->loader->num_banks = 1;
			ctx->loader->num_ranks = mem_controller->num_regs_rank;
		}
		ctx->loader->mc_accesses_per_bank[bank]++;
		ctx->loader->mc_accesses++;
	}

	/*Insert*/
	linked_list_tail(mem_controller->pref_queue[bank]->queue);
	linked_list_add(mem_controller->pref_queue[bank]->queue, stack);
	linked_list_head(mem_controller->pref_queue[bank]->queue);

	/*Now queue is full?*/
	 if(linked_list_count(mem_controller->pref_queue[bank]->queue) == mem_controller->size_queue)
         	mem_controller->pref_queue[bank]->instant_begin_full = esim_cycle();

	mem_controller->pref_queue[bank]->total_insertions++;

}

int mem_controller_remove(struct mod_stack_t * stack, struct mem_controller_queue_t * queue){

	linked_list_head(queue->queue);
	while(!linked_list_is_end(queue->queue)){
		struct mod_stack_t * stack_aux=linked_list_get(queue->queue);
		if(stack_aux->id==stack->id){
			////mem_debug("borra %lld\n", stack->id);
			linked_list_remove(queue->queue);
			return 1;
		}
		linked_list_next(queue->queue);
	}
	return 0;

}







/*ROUND ROBIN*/

int mem_controller_get_bank_queue(int num_queue_examined,struct mem_controller_t * mem_controller )
{

	int pos=(mem_controller->queue_round_robin+num_queue_examined)%mem_controller->num_queues;
	assert(pos>=0);
	assert(pos<mem_controller->num_queues);
	return  pos;

}



/*struct mod_stack_t * mem_controller_select_request2(int n_queues_examined, enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;

	struct mem_controller_queue_t * normal_queue= mem_controller->normal_queue[n_queues_examined%mem_controller->num_queues];
	struct mem_controller_queue_t * pref_queue= mem_controller->pref_queue[n_queues_examined%mem_controller->num_queues];

	*//*First priority: threshold normal
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}
	*/
	/*Second priority: threshold prefetch
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}

	*/

	/*Third priority: row buffer hit normal
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	if(priority==prio_threshold_normal_pref)
	{
	*/	/*Four priority: FCFS normal
		linked_list_head(normal_queue->queue);
		while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
		{
			struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
			if(can_acces_bank)
				return stack;
			linked_list_next(normal_queue->queue);
		}

	*/	/*Five priority: row buffer hit prefetch
		linked_list_head(pref_queue->queue);
		while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
		{
			struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
			if(can_acces_bank&&stack->state==row_buffer_hit)
				return stack;
			linked_list_next(pref_queue->queue);
		}


	}
	else // row buffer hit> fcfs
	{
	*/	/*Four priority: row buffer hit prefetch
		linked_list_head(pref_queue->queue);
		while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
		{
			struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
			if(can_acces_bank&&stack->state==row_buffer_hit)
				return stack;
			linked_list_next(pref_queue->queue);
		}

	*/	/*Five priority: FCFS normal
		linked_list_head(normal_queue->queue);
		while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
		{
			struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
			if(can_acces_bank)
				return stack;
			linked_list_next(normal_queue->queue);
		}
	}

	*//*Six priority: FCFS pref
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(pref_queue->queue);
	}
	return NULL;

}

*/


struct mod_stack_t * mem_controller_select_prefRBH_normalRBH_normal_pref_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;


	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}


	/*Third priority: row buffer hit prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Four priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Five priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}
	/*Six priority: FCFS pref*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	return NULL;

}



struct mod_stack_t * mem_controller_select_prefHit_normal_prefGroup_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;


	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}


	/*Third priority: row buffer hit prefetch single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Four priority: FCFS pref single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank && stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}


	/*Five priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Six priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}
	/*Seven priority: FCFS pref group*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,&stack->channel, &stack->rank,
				&stack->bank, &stack->row, & stack->tag, &stack->state_main_memory);
		if(can_acces_bank)
		{
			if(priority== prio_threshold_prefHit_normal_prefGroupCoalesce)
			{
				mem_controller_coalesce_acces_row_buffer(stack, pref_queue->queue);
				assert(stack->coalesced_stacks!=NULL);

				LINKED_LIST_FOR_EACH(stack->coalesced_stacks) //remove current stack from coalesced
				{
					struct mod_stack_t * stack_aux=linked_list_get(stack->coalesced_stacks);
					if(stack_aux->id==stack->id)
					{
						linked_list_remove(stack->coalesced_stacks);
						if(linked_list_count(stack->coalesced_stacks)==0)
						{
							linked_list_free(stack->coalesced_stacks);
							stack->coalesced_stacks=NULL;
						}
						break;
					}
				}

			}
			return stack;
		}
		linked_list_next(pref_queue->queue);
	}


	return NULL;

}


struct mod_stack_t * mem_controller_select_prefHitRBH_normalRBH_normal_prefHit_prefGroup_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;


	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}


	/*Third priority: row buffer hit prefetch single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Four priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Five priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Six priority: FCFS pref single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank && stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}


	/*Seven priority: FCFS pref group*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,&stack->channel, &stack->rank,
				&stack->bank, &stack->row, & stack->tag, &stack->state_main_memory);
		if(can_acces_bank)
		{
			if(priority== prio_threshold_prefHitRBH_normalRBH_normal_prefHit_prefGroupCoalesce)
			{
				mem_controller_coalesce_acces_row_buffer(stack, pref_queue->queue);
				assert(stack->coalesced_stacks!=NULL);

				LINKED_LIST_FOR_EACH(stack->coalesced_stacks) //remove current stack from coalesced
				{
					struct mod_stack_t * stack_aux=linked_list_get(stack->coalesced_stacks);
					if(stack_aux->id==stack->id)
					{
						linked_list_remove(stack->coalesced_stacks);
						if(linked_list_count(stack->coalesced_stacks)==0)
						{
							linked_list_free(stack->coalesced_stacks);
							stack->coalesced_stacks=NULL;
						}
						break;
					}
				}

			}
			return stack;
		}
		linked_list_next(pref_queue->queue);
	}

	return NULL;

}


struct mod_stack_t * mem_controller_select_request(int n_queues_examined, enum priority_t priority, struct mem_controller_t * mem_controller)
{

	struct mem_controller_queue_t * normal_queue= mem_controller->normal_queue[n_queues_examined%mem_controller->num_queues];
	struct mem_controller_queue_t * pref_queue= mem_controller->pref_queue[n_queues_examined%mem_controller->num_queues];

	if(priority ==prio_threshold_normal_pref)
		return mem_controller_select_normal_pref_prio(normal_queue, pref_queue, priority, mem_controller);

	else if(priority == prio_threshold_RowBufHit_FCFS)
		return mem_controller_select_rbh_fcfs_prio(normal_queue, pref_queue, priority, mem_controller);
	else if(priority == prio_threshold_normal_prefHit_prefGroup || priority == prio_threshold_normal_prefHit_prefGroupCoalesce )
		return mem_controller_select_normal_prefHit_prefGroup_prio(normal_queue, pref_queue, priority, mem_controller);
	else if(priority == prio_threshold_RowBufHit_normal_prefHit_prefGroup || priority == prio_threshold_RowBufHit_normal_prefHit_prefGroupCoalesce )
		return mem_controller_select_rbh_normal_prefHit_prefGroup_prio(normal_queue, pref_queue, priority, mem_controller);
	else if(priority == prio_threshold_RowBufHit_prefHit_normal_prefGroup || priority == prio_threshold_RowBufHit_prefHit_normal_prefGroupCoalesce )
		return mem_controller_select_rbh_prefHit_normal_prefGroup_prio(normal_queue, pref_queue, priority, mem_controller);


	else if(priority == prio_threshold_pref_normal)
		return mem_controller_select_pref_normal_prio(normal_queue, pref_queue, priority, mem_controller);

	else if(priority == prio_threshold_prefRBH_normalRBH_normal_pref)
		return mem_controller_select_prefRBH_normalRBH_normal_pref_prio(normal_queue, pref_queue, priority, mem_controller);

	else if(priority == prio_threshold_prefHit_normal_prefGroup || priority == prio_threshold_prefHit_normal_prefGroupCoalesce )
		return mem_controller_select_prefHit_normal_prefGroup_prio(normal_queue, pref_queue, priority, mem_controller);

	else if(priority == prio_threshold_prefHitRBH_normalRBH_normal_prefHit_prefGroup || priority == prio_threshold_prefHitRBH_normalRBH_normal_prefHit_prefGroupCoalesce )
		return mem_controller_select_prefHitRBH_normalRBH_normal_prefHit_prefGroup_prio(normal_queue, pref_queue, priority, mem_controller);
	else if(priority == prio_dynamic)
		return mem_controller_select_dynamic_prio(normal_queue, pref_queue, priority, mem_controller);
	else if(priority == prio_FCFS_normal_pref)
		return mem_controller_select_FCFS_prio(normal_queue, pref_queue, priority, mem_controller);
	else
		fatal("Policy Mc doesnt exist");
	return NULL;

}

struct mod_stack_t * mem_controller_select_FCFS_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;


	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}
	

	/*Thrird priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Four priority: FCFS pref */
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(pref_queue->queue);
	}



	return NULL;

}




struct mod_stack_t * mem_controller_select_dynamic_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller)
{
	int can_acces_bank;
	int size_queue=mem_controller->size_queue;


	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}


	/*Third priority: row buffer hit prefetch single >2*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_single && stack->priority>=2) // 3 requests
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Five priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Third priority: row buffer hit prefetch group >3*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_grouped && stack->priority>=3) //4requests
			return stack;
		linked_list_next(pref_queue->queue);
	}

	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_single && stack->priority>=1) // 2 requests
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Third priority: row buffer hit prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}



	/*Six priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Third priority: row buffer hit prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Third priority: row buffer hit prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(pref_queue->queue);
	}
	return NULL;

}


struct mod_stack_t * mem_controller_select_pref_normal_prio(struct mem_controller_queue_t * normal_queue, struct mem_controller_queue_t * pref_queue, enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;


	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}


	/*Third priority: row buffer hit prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Four priority: FCFS pref*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Five priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Six priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}


	return NULL;

}


struct mod_stack_t * mem_controller_select_rbh_fcfs_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;

	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}



	/*Third priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}


	/*Four priority: row buffer hit prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Five priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}




	/*Six priority: FCFS pref*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(pref_queue->queue);
	}
	return NULL;

}


struct mod_stack_t * mem_controller_select_normal_pref_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;

	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}



	/*Third priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}


	/*Four priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Five priority: row buffer hit prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(pref_queue->queue);
	}




	/*Six priority: FCFS normal*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(pref_queue->queue);
	}
	return NULL;

}


struct mod_stack_t * mem_controller_select_normal_prefHit_prefGroup_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;

	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}


	/*Third priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}


	/*Four priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}


	/*Five priority: row buffer hit prefetch single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}



	/*Six priority: prefetch single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Seven priority:  pref group*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,&stack->channel, &stack->rank,
				&stack->bank, &stack->row, & stack->tag, &stack->state_main_memory);
		if(can_acces_bank)
		{
			if(priority== prio_threshold_normal_prefHit_prefGroupCoalesce)
			{
				mem_controller_coalesce_acces_row_buffer(stack, pref_queue->queue);
				assert(stack->coalesced_stacks!=NULL);

				LINKED_LIST_FOR_EACH(stack->coalesced_stacks) //remove current stack from coalesced
				{
					struct mod_stack_t * stack_aux=linked_list_get(stack->coalesced_stacks);
					if(stack_aux->id==stack->id)
					{
						linked_list_remove(stack->coalesced_stacks);
						if(linked_list_count(stack->coalesced_stacks)==0)
						{
							linked_list_free(stack->coalesced_stacks);
							stack->coalesced_stacks=NULL;
						}
						break;
					}
				}

			}
			return stack;
		}
		linked_list_next(pref_queue->queue);
	}
	return NULL;

}


struct mod_stack_t * mem_controller_select_rbh_prefHit_normal_prefGroup_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;

	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}


	/*Third priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Four priority: row buffer hit prefetch single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Five priority: prefetch single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}


	/*Six priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Seven priority:  pref group*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,&stack->channel, &stack->rank,
				&stack->bank, &stack->row, & stack->tag, &stack->state_main_memory);
		if(can_acces_bank)
		{
			if(priority== prio_threshold_RowBufHit_prefHit_normal_prefGroupCoalesce)
			{
				mem_controller_coalesce_acces_row_buffer(stack, pref_queue->queue);
				assert(stack->coalesced_stacks!=NULL);

				LINKED_LIST_FOR_EACH(stack->coalesced_stacks) //remove current stack from coalesced
				{
					struct mod_stack_t * stack_aux=linked_list_get(stack->coalesced_stacks);
					if(stack_aux->id==stack->id)
					{
						linked_list_remove(stack->coalesced_stacks);
						if(linked_list_count(stack->coalesced_stacks)==0)
						{
							linked_list_free(stack->coalesced_stacks);
							stack->coalesced_stacks=NULL;
						}
						break;
					}
				}
			}
			return stack;
		}
		linked_list_next(pref_queue->queue);
	}
	return NULL;

}


struct mod_stack_t * mem_controller_select_rbh_normal_prefHit_prefGroup_prio(struct mem_controller_queue_t * normal_queue,struct mem_controller_queue_t * pref_queue , enum priority_t priority, struct mem_controller_t * mem_controller)
{


	int can_acces_bank;
	int size_queue=mem_controller->size_queue;

	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}
		linked_list_next(normal_queue->queue);
	}

	/*Second priority: threshold prefetch*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
			if(can_acces_bank)
				return stack;
		}

		linked_list_next(pref_queue->queue);
	}

	/*Third priority: row buffer hit normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Four priority: row buffer hit prefetch single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit && stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Five priority: FCFS normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	/*Six priority: prefetch single*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->client_info->stream_request_kind == stream_request_single)
			return stack;
		linked_list_next(pref_queue->queue);
	}

	/*Seven priority:  pref group*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(mem_controller,stack->mod,stack->addr,&stack->channel, &stack->rank,
				&stack->bank, &stack->row, & stack->tag, &stack->state_main_memory);
		if(can_acces_bank)
		{
			if(priority== prio_threshold_RowBufHit_normal_prefHit_prefGroupCoalesce)
			{
				mem_controller_coalesce_acces_row_buffer(stack, pref_queue->queue);
				assert(stack->coalesced_stacks!=NULL);

				LINKED_LIST_FOR_EACH(stack->coalesced_stacks) //remove current stack from coalesced
				{
					struct mod_stack_t * stack_aux=linked_list_get(stack->coalesced_stacks);
					if(stack_aux->id==stack->id)
					{
						linked_list_remove(stack->coalesced_stacks);
						if(linked_list_count(stack->coalesced_stacks)==0)
						{
							linked_list_free(stack->coalesced_stacks);
							stack->coalesced_stacks=NULL;
						}
						break;
					}
				}

			}
			return stack;
		}
		linked_list_next(pref_queue->queue);
	}
	return NULL;

}


int mem_controller_queue_has_consumed_threshold(struct linked_list_t * queue, long long size_queue)
{
	struct mod_stack_t * stack;
	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<size_queue){
		stack=linked_list_get(queue);
		if(stack->threshold==0)
			return 1;
		linked_list_next(queue);
	}
	return 0;
}


/*int mem_controller_queue_has_row_buffer_hit(struct linked_list_t * queue, long long size_queue)
{
	int state;
	struct mod_stack_t * stack;
	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<size_queue){
		stack=linked_list_get(queue);
		row_buffer_find_row(stack->mod, stack->addr, NULL,NULL,NULL, NULL, NULL, &state);
		if(state==row_buffer_hit)
			return 1;
		linked_list_next(queue);
	}
	return 0;



}*/

void mem_controller_update_requests_threshold(int cycles, struct mem_controller_t * mem_controller)
{
	struct mod_stack_t * stack;

	for(int i=0; i<mem_controller->num_queues;i++)
	{
		/*Decrease threshold of prefetch request 1 cycle */
		linked_list_head(mem_controller->pref_queue[i]->queue );
		while(!linked_list_is_end(mem_controller->pref_queue[i]->queue)&&
		linked_list_current(mem_controller->pref_queue[i]->queue)<mem_controller->size_queue){
			stack=linked_list_get(mem_controller->pref_queue[i]->queue);
			if((stack->threshold-cycles)>0)
				stack->threshold-=cycles;
			else
				stack->threshold=0;
			linked_list_next(mem_controller->pref_queue[i]->queue);
		}

		/*Decrease threshold of normal request 1 cycle */
		linked_list_head(mem_controller->normal_queue[i]->queue);
		while(!linked_list_is_end(mem_controller->normal_queue[i]->queue)&&
		linked_list_current(mem_controller->normal_queue[i]->queue)<mem_controller->size_queue){
			stack=linked_list_get(mem_controller->normal_queue[i]->queue);
			if((stack->threshold-cycles)>0)
				stack->threshold-=cycles;
			else
				stack->threshold=0;
			linked_list_next(mem_controller->normal_queue[i]->queue);
		}
	}

}

/////////////////////////////////////////////////////////////////////
int mem_controller_stacks_normalQueues_count(struct mem_controller_t * mem_controller)
{
	int num_stacks=0;

	for(int i=0; i<mem_controller->num_queues;i++)
			num_stacks+=linked_list_count(mem_controller->normal_queue[i]->queue);
	return num_stacks;
}


int mem_controller_stacks_prefQueues_count(struct mem_controller_t * mem_controller)
{
	int num_stacks=0;

	for(int i=0; i<mem_controller->num_queues;i++)
			num_stacks+=linked_list_count(mem_controller->pref_queue[i]->queue);
	return num_stacks;
}
//////////////////////////////////////////////////////////////////



int mem_controller_coalesce_acces_row_buffer( struct mod_stack_t * stack, struct linked_list_t * queue)
{
	struct mod_stack_t * stack_aux;
	struct mem_controller_t * mem_controller=stack->mod->mem_controller;
	unsigned int num_ranks = mem_controller->num_regs_rank ;
	unsigned int num_banks = mem_controller->num_regs_bank ;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
	int count =0;

	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<mem_controller->size_queue)
	{
		stack_aux=linked_list_get(queue);

		/*This stack has its block in the same main mamory row than origin stack*/
		int row=(stack_aux->addr>>(log2_row_size+log_base2(num_banks)+log_base2(num_ranks)));
		int rank = (stack_aux->addr >> (log2_row_size+ log_base2(num_banks))) % num_ranks;
		int bank = (stack_aux->addr >> log2_row_size) % num_banks;


		assert(rank==stack->rank && bank==stack->bank);

		if(row==stack->row)
		{
			/*Coalesce*/
			mem_debug("   stack %lld coalesced with stack %lld\n", stack_aux->id, stack->id);
			if(stack->coalesced_stacks==NULL)
				stack->coalesced_stacks=linked_list_create();
			count++;
			linked_list_add(stack->coalesced_stacks, stack_aux);
			linked_list_remove(queue);
		}else
			linked_list_next(queue);

	}

	return count;
}

int mem_controller_coalesce_acces_between_blocks(struct mod_stack_t * stack, struct linked_list_t *queue, int block_min, int block_max)
{

	struct mod_stack_t * stack_aux;
	struct mem_controller_t * mem_controller=stack->mod->mem_controller;
	int min_max, max_min;

	unsigned int num_ranks = mem_controller->num_regs_rank ;
	unsigned int num_banks = mem_controller->num_regs_bank ;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
	int n_coal=0;

	if(linked_list_count(stack->coalesced_stacks)==0)
		return 0;
	linked_list_tail(stack->coalesced_stacks);
	stack_aux = linked_list_get(stack->coalesced_stacks);
	min_max = stack_aux->addr %  mem_controller->row_buffer_size;
	max_min = stack->addr %  mem_controller->row_buffer_size;


	/*printf("%d . ", stack->addr %  mem_controller->row_buffer_size);
	linked_list_head(stack->coalesced_stacks);
	while(!linked_list_is_end(stack->coalesced_stacks))
	{
		stack_aux=linked_list_get(stack->coalesced_stacks);
		printf("%d . ", stack_aux->addr %  mem_controller->row_buffer_size);
		linked_list_next(stack->coalesced_stacks);
	}
	printf("-----b=%d r=%d ----------- min=%d min_max=%d max_min=%d max=%d\n", stack->bank, stack->rank,block_min, min_max, max_min, block_max);*/

	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<mem_controller->size_queue)
	{
		stack_aux=linked_list_get(queue);

		/*This stack has its block in the same main mamory row than origin stack*/
		unsigned int row=(stack_aux->addr>>(log2_row_size+log_base2(num_banks)+log_base2(num_ranks)));
		unsigned int rank = (stack_aux->addr >> (log2_row_size+ log_base2(num_banks))) % num_ranks;
		unsigned int bank = (stack_aux->addr >> log2_row_size) % num_banks;
		unsigned int block=stack_aux->addr %  mem_controller->row_buffer_size;
		//printf("   stack %lld b=%d r=%d b=%d\n", stack_aux->id, bank,rank, block);
			
		assert(rank==stack->rank && bank==stack->bank);

		/*Is this request between the min and max block? */
		if((row==stack->row && block>=block_min&& block<=min_max) || (row==stack->row && block>=max_min&& block<=block_max ))
		{

			/*Coalesce*/
			mem_debug("   stack %lld coalesced with stack %lld\n", stack_aux->id, stack->id);
			if(stack->coalesced_stacks==NULL)
				stack->coalesced_stacks=linked_list_create();
			linked_list_add(stack->coalesced_stacks, stack_aux);
			linked_list_remove(queue);
			n_coal++;
		}else
			linked_list_next(queue);

	}

	return n_coal;

}

int mem_controller_coalesce_acces_block(struct mod_stack_t * stack, struct linked_list_t *queue)
{

	struct mod_stack_t * stack_aux;
	int n_coal=0;
	struct mem_controller_t * mem_controller=stack->mod->mem_controller;

	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<mem_controller->size_queue)
	{
		stack_aux=linked_list_get(queue);


		/*Is the request demanding this block? */
		if(stack_aux->addr==stack->addr)
		{
			/*Coalesce*/
			mem_debug("   stack %lld coalesced with stack %lld\n", stack_aux->id, stack->id);
			if(stack->coalesced_stacks==NULL)
				stack->coalesced_stacks=linked_list_create();
			linked_list_add(stack->coalesced_stacks, stack_aux);
			linked_list_remove(queue);
			n_coal++;
		}else
			linked_list_next(queue);

	}

	return n_coal;

}



void mem_controller_sort_by_block(struct mod_stack_t * stack)
{

	struct mod_stack_t * stack_aux, *stack_list;
	struct linked_list_t * list=linked_list_create();
	struct mem_controller_t * mem_controller=stack->mod->mem_controller;
	struct linked_list_t *after= linked_list_create();
	struct linked_list_t *before = linked_list_create();
				
        int add=1;

	unsigned int block;

	/*Crescent address order*/
	linked_list_head(stack->coalesced_stacks);
	while(!linked_list_is_end(stack->coalesced_stacks))
	{
		stack_aux=linked_list_get(stack->coalesced_stacks);
		block=stack_aux->addr %  mem_controller->row_buffer_size;
		int add=1;

		linked_list_head(list);
		while(!linked_list_is_end(list))
		{
			stack_list=linked_list_get(list);
			if(block < stack_list->addr %  mem_controller->row_buffer_size)
			{
				//linked_list_prev(list);
				linked_list_insert(list,stack_aux);
				linked_list_remove(stack->coalesced_stacks);
				add=0;
				break;
			}
			linked_list_next(list);
		}
		if(add)
		{
			//linked_list_prev(list);
			linked_list_insert(list,stack_aux);
			linked_list_remove(stack->coalesced_stacks);
		}
	}

	linked_list_free(stack->coalesced_stacks);
	
        linked_list_head(list);
        while(!linked_list_is_end(list))
        {
                stack_aux=linked_list_get(list);
                if(stack->addr % mem_controller->row_buffer_size < stack_aux->addr % mem_controller->row_buffer_size)
                {
                        linked_list_insert(list,stack);
                        add=0;
                        break;
                }
                linked_list_next(list);
        }
        if(add)linked_list_insert(list,stack);

        mem_controller_count_successive_hits(list);


	
	/*SPlit queue into address before and after stack*/
	LINKED_LIST_FOR_EACH(list)
        {
                stack_aux=linked_list_get(list);
                if(stack->addr % mem_controller->row_buffer_size < stack_aux->addr % mem_controller->row_buffer_size)
                        linked_list_add(after,stack_aux);
                else if(stack->id!=stack_aux->id) // is not the main stack
			linked_list_add(before,stack_aux);
			
               
        }
	


	/*Create a circular order*/
	linked_list_tail(after);
	LINKED_LIST_FOR_EACH(before)
        {
                stack_aux=linked_list_get(before);
                linked_list_add(after,stack_aux);
        }
	
	
        linked_list_free(list);
	linked_list_free(before);

        //if(new_stack->coalesced_stacks!=NULL) linked_list_free(new_stack->coalesced_stacks);
        stack->coalesced_stacks=after;

	//linked_list_free(list);
	assert(stack->coalesced_stacks!=NULL);
	//assert( linked_list_count(stack->coalesced_stacks)>0);

	
	/*TODO quitar cuando se vea que va bien*/
	/*printf("%d . ", stack->addr %  mem_controller->row_buffer_size);
	linked_list_head(after);
	while(!linked_list_is_end(after))
	{
		stack_aux=linked_list_get(after);
		printf("%d . ", stack_aux->addr %  mem_controller->row_buffer_size);
		linked_list_next(after);
	}
	printf("\n");*/
 	
	

}

unsigned int mem_controller_min_block(struct mod_stack_t *stack)
{

	struct linked_list_t * queue=stack->coalesced_stacks;
	struct mod_stack_t * stack_aux;
	struct mem_controller_t * mem_controller=stack->mod->mem_controller;

	unsigned int first;
	unsigned int block;

	first = stack->addr %  mem_controller->row_buffer_size;

	LINKED_LIST_FOR_EACH(queue)
	{
		stack_aux=linked_list_get(queue);
		block=stack_aux->addr %  mem_controller->row_buffer_size;


		if(first>block)
			first=block;

	}
	assert(first+stack->mod->cache->block_size <= mem_controller->row_buffer_size);
	assert(first>=0 );
	assert(first % stack->mod->cache->block_size==0);

	return first;

}


unsigned int mem_controller_max_block(struct mod_stack_t *stack)
{

	struct linked_list_t * queue=stack->coalesced_stacks;
	struct mod_stack_t * stack_aux;
	struct mem_controller_t * mem_controller=stack->mod->mem_controller;

	unsigned int last;
	unsigned int block;

	last = stack->addr %  mem_controller->row_buffer_size;

	LINKED_LIST_FOR_EACH(queue)
	{
		stack_aux=linked_list_get(queue);
		block=stack_aux->addr %  mem_controller->row_buffer_size;

		if(last<block)
			last=block;

	}
	assert(last+stack->mod->cache->block_size <= mem_controller->row_buffer_size);
	assert(last>=0);
	assert(last % stack->mod->cache->block_size==0);

	return last;

}

int mem_controller_calcul_number_blocks_transfered(struct mod_stack_t *stack)
{

	struct linked_list_t * queue=stack->coalesced_stacks;
	struct mod_stack_t * stack_aux;
	struct mem_controller_t * mem_controller=stack->mod->mem_controller;

	unsigned int first;
	unsigned int last;
	unsigned int block;

	/*Initialize*/
	first = last = stack->addr %  mem_controller->row_buffer_size;
	LINKED_LIST_FOR_EACH(queue)
	{
		stack_aux=linked_list_get(queue);
		block=stack_aux->addr %  mem_controller->row_buffer_size;
		/*Get fist block of this row*/
		if(first>block)
			first=block;

		else if(last<block)
			last=block;

	}
	assert(last+stack->mod->cache->block_size <= mem_controller->row_buffer_size);
	assert(first>=0 && last>=0);
	assert((last-first) % stack->mod->cache->block_size==0);

	return (last-first)/stack->mod->cache->block_size + 1;

}


void mem_controller_count_successive_hits(struct linked_list_t * coalesced_stacks)
{

	int count =0;
	int max=1;
	int block_size;
	int i=0;
	struct mod_stack_t * stack_aux, *stack;
	struct mem_controller_t * mem_controller;

	linked_list_head(coalesced_stacks);
	stack=linked_list_get(coalesced_stacks);
	mem_controller=stack->mod->mem_controller;
	enum priority_t priority = mem_controller->priority_request_in_queue;

	block_size=stack->mod->cache->block_size;
	unsigned int first_block= stack->addr %  mem_controller->row_buffer_size;

	mem_controller->burst_size[linked_list_count(coalesced_stacks)-1]++;

	if(mem_controller->coalesce==policy_coalesce || mem_controller->coalesce == policy_coalesce_delayed_request)
		mem_controller->successive_hit[linked_list_count(coalesced_stacks)-1][linked_list_count(coalesced_stacks)-1]++;

	else if(mem_controller->coalesce == policy_coalesce_useful_blocks || priority == prio_threshold_normal_prefHit_prefGroupCoalesce || priority == prio_threshold_RowBufHit_normal_prefHit_prefGroupCoalesce || priority == prio_threshold_RowBufHit_prefHit_normal_prefGroupCoalesce || priority == prio_threshold_prefHit_normal_prefGroupCoalesce || priority == prio_threshold_prefHitRBH_normalRBH_normal_prefHit_prefGroupCoalesce)
	{

		LINKED_LIST_FOR_EACH(coalesced_stacks)
		{
			stack_aux= linked_list_get(coalesced_stacks);
			//mem_debug("%d == %d ", block_size*i+first_block,stack_aux->addr %  mem_controller->row_buffer_size );

			if(block_size*i+first_block==stack_aux->addr %  mem_controller->row_buffer_size)// succesive blocks
			{
				count++;
				i++;
			}else{
				if(count>max)//is not a successive block , so we count the max number of consecutive accesses
					max=count;
				first_block=stack_aux->addr %  mem_controller->row_buffer_size;
				count=1;
				i=1;
			}

		}
		if(count>max)//is not a successive block , so we count the max number of consecutive accesses
			max=count;
		mem_controller->successive_hit[linked_list_count(coalesced_stacks)-1][max-1]++;
	//	mem_debug("  [%d,%d]\n", linked_list_count(coalesced_stacks),max);
	}
	else
		fatal("Error policy\n");

}

int mem_controller_get_size_queue(struct mod_stack_t* stack)
{
	int size = 0;

	struct mem_controller_t * mem_controller=stack->target_mod->mem_controller;

	unsigned int bank;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);

	if(mem_controller->queue_per_bank)
		bank = ((stack->addr >> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));
	else
		bank=0;
	if(!stack->prefetch)
	 	size = mem_controller->normal_queue[bank]->current_request_num;
	else
		size = mem_controller->pref_queue[bank]->current_request_num;

	if(size == mem_controller->size_queue)
		 mem_debug("  %lld %lld 0x%x %s queue %d pref %d full \n", esim_cycle(), stack->id,
                        stack->addr, stack->target_mod->name, bank, stack->prefetch);
	mem_debug("%lld %d b%d p%d.......\n", stack->id, size, bank, stack->prefetch );
	assert(size>=0);

	assert(size<=mem_controller->size_queue);

	return size;
}

void mem_controller_register_in_queue(struct mod_stack_t* stack)
{
	
	struct mem_controller_t * mem_controller=stack->target_mod->mem_controller;

	unsigned int bank;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);

	if(mem_controller->queue_per_bank)
		bank = ((stack->addr >> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));
	else
		bank=0;
	if(!stack->prefetch)
	{
	 	mem_controller->normal_queue[bank]->current_request_num++;
		mem_debug("  %lld %lld 0x%x %s queue %d pref %d count before %lld insert \n", esim_cycle(), stack->id,stack->addr, stack->target_mod->name, bank, stack->prefetch, mem_controller->normal_queue[bank]->current_request_num);

	}else{
		mem_controller->pref_queue[bank]->current_request_num++;
		mem_debug("  %lld %lld 0x%x %s queue %d pref %d count before %lld insert \n", esim_cycle(), stack->id,stack->addr, stack->target_mod->name, bank, stack->prefetch, mem_controller->pref_queue[bank]->current_request_num);
	}
	
}


void mem_controller_remove_in_queue(struct mod_stack_t* stack)
{
	struct mem_controller_t * mem_controller=stack->target_mod->mem_controller;

	unsigned int bank;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);

	if(mem_controller->queue_per_bank)
		bank = ((stack->addr >> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));
	else
		bank=0;


	if(!stack->prefetch)
	{
	 	mem_controller->normal_queue[bank]->current_request_num--;
		mem_debug("  %lld %lld 0x%x %s queue %d pref %d count after %lld delete \n", esim_cycle(), stack->id,stack->addr, stack->target_mod->name, bank, stack->prefetch, mem_controller->normal_queue[bank]->current_request_num);
		assert(mem_controller->normal_queue[bank]->current_request_num>=0);
	}else
	{
		mem_controller->pref_queue[bank]->current_request_num--;
		mem_debug("  %lld %lld 0x%x %s queue %d pref %d count after %lld delete \n", esim_cycle(), stack->id,stack->addr, stack->target_mod->name, bank, stack->prefetch, mem_controller->pref_queue[bank]->current_request_num);
		assert(mem_controller->pref_queue[bank]->current_request_num>=0);
	}


}


void mem_controller_adapt_schedule(struct mem_controller_t * mem_controller)
{

	if (mem_controller->adapt_interval_kind == interval_kind_cycles)
	{

		/* Create new stack */
		/*struct mem_controller_adapt_stack_t* stack = xcalloc(1, sizeof(struct mem_controller_adapt_stack_t));
		stack->mem_controller = mem_controller;*/

		/* Schedule next event */
		esim_schedule_event(EV_MEM_CONTROLLER_ADAPT, mem_controller, mem_controller->adapt_interval);
	}
}


void mem_controller_adapt_handler(int event, void *data)
{
	struct mem_controller_t *mem_controller = (struct mem_controller_t *) data;
	int useful_streams = 0;
	int lived_streams = 0;
	struct tuple_adapt_t *tuple;

	/* If simulation has ended, no more
	 * events to schedule. */
	if (esim_finish)
	{
		return;
	}

	LINKED_LIST_FOR_EACH(mem_controller->lived_streams)
	{
		tuple=linked_list_get(mem_controller->lived_streams);
		lived_streams+=linked_list_count(tuple->streams);
	}

	LINKED_LIST_FOR_EACH(mem_controller->useful_streams)
	{
		tuple=linked_list_get(mem_controller->useful_streams);
		useful_streams+=linked_list_count(tuple->streams);
	}

	linked_list_head(mem_controller->useful_streams);
	while(!linked_list_is_end(mem_controller->useful_streams))
	{
		tuple=linked_list_get(mem_controller->useful_streams);
		linked_list_remove(mem_controller->useful_streams);
		linked_list_free(tuple->streams);
		free(tuple);
	}

	linked_list_head(mem_controller->lived_streams);
	while(!linked_list_is_end(mem_controller->lived_streams))
	{
		tuple=linked_list_get(mem_controller->lived_streams);
		linked_list_remove(mem_controller->lived_streams);
		linked_list_free(tuple->streams);
		free(tuple);
	}

	if((lived_streams>0 ? (double) useful_streams/lived_streams : 0) > mem_controller->adapt_percent)
		mem_controller->priority_request_in_queue=prio_threshold_RowBufHit_FCFS;
	else
		mem_controller->priority_request_in_queue=prio_threshold_normal_pref;

	// TODO: Arrgelar, sempre imprimeix NAN mem_debug("%f   prio=%d\n", (double)useful_streams/lived_streams, mem_controller->priority_request_in_queue);

	if (mem_controller->adapt_interval_kind == interval_kind_cycles)
	{
		/* Schedule new event */
		assert(mem_controller->adapt_interval);
		esim_schedule_event(EV_MEM_CONTROLLER_ADAPT, mem_controller, mem_controller->adapt_interval);
	}
}


void mem_controller_mark_stream(struct mod_stack_t* stack, struct linked_list_t *list)
{
	int exists=0;
	struct tuple_adapt_t * tuple;


	assert(stack->client_info->stream>=0);
	LINKED_LIST_FOR_EACH(list)
	{
		tuple=linked_list_get(list);
		if(tuple->mod->name==stack->mod->name)
		{
			exists=1;
			LINKED_LIST_FOR_EACH(tuple->streams)
			{
				int * stre=linked_list_get(tuple->streams);
				assert(stre>=0);
				if(*stre==stack->client_info->stream)
					break;

			}
			linked_list_add(tuple->streams, &stack->client_info->stream);
			
			break;
		}
	}

	if(!exists)
	{
		tuple=xcalloc(1,sizeof(struct tuple_adapt_t));
		tuple->mod=stack->mod;
		tuple->streams=linked_list_create();
		linked_list_add(tuple->streams, &stack->client_info->stream);
		
		linked_list_add(list,tuple);
	}

}


int mem_controller_is_useful_stream(struct mod_stack_t* stack, struct mem_controller_queue_t * queue)
{

	unsigned int row;
	int state;
	struct mod_stack_t* stack_aux;

	row_buffer_find_row(stack->mod->mem_controller, stack->mod, stack->addr, NULL, NULL, NULL, &row, NULL, &state);


	/*It has to be a hit in row buffer*/
	if(state==row_buffer_miss)
		return 0;

	/*Bank queue has another request comming from the same stream*/
	LINKED_LIST_FOR_EACH(queue->queue)
	{
		stack_aux=linked_list_get(queue->queue);
		assert(stack_aux->client_info->stream>=0);

		/*The other request has te be a stream hit*/
		if(stack->client_info->stream == stack_aux->client_info->stream && stack_aux->client_info->stream_request_kind == stream_request_single)
			return 1;
	}

	return 0;



}

int mem_controller_count_requests_same_stream(struct mod_stack_t* stack, struct linked_list_t * queue)
{
	int count =0;
	struct mod_stack_t* stack_aux;
	struct mem_controller_t * mem_controller= stack->mod->mem_controller;

	LINKED_LIST_FOR_EACH(queue)
	{
		stack_aux=linked_list_get(queue);
		row_buffer_find_row(mem_controller,stack_aux->mod,stack_aux->addr,&stack_aux->channel, &stack_aux->rank,&stack_aux->bank,&stack_aux->row, NULL, NULL);
		row_buffer_find_row(mem_controller,stack->mod,stack->addr,&stack->channel, &stack->rank,&stack->bank,&stack->row, NULL, NULL);

		assert(stack_aux->channel==stack->channel && stack_aux->rank == stack->rank && stack->bank == stack_aux->bank);
		if(stack_aux->client_info->stream==stack->client_info->stream && stack->id!=stack_aux->id && stack->row == stack_aux->row)
			count++;
	}

	return count;
}


void mem_controller_mark_requests_same_stream(struct mod_stack_t* stack, struct linked_list_t * queue)
{
	int count =0;
	struct mod_stack_t* stack_aux;
	struct mem_controller_t * mem_controller= stack->mod->mem_controller;

	LINKED_LIST_FOR_EACH(queue)
	{
		stack_aux=linked_list_get(queue);
		row_buffer_find_row(mem_controller,stack_aux->mod,stack_aux->addr,&stack_aux->channel, &stack_aux->rank,&stack_aux->bank,&stack_aux->row, NULL, NULL);
		row_buffer_find_row(mem_controller,stack->mod,stack->addr,&stack->channel, &stack->rank,&stack->bank,&stack->row, NULL, NULL);

		assert(stack_aux->rank == stack->rank && stack->bank == stack_aux->bank);
		if(stack_aux->client_info->stream == stack->client_info->stream &&
			stack->id != stack_aux->id && stack->row == stack_aux->row &&
			stack_aux->client_info->stream_request_kind == stack->client_info->stream_request_kind)
		{
			stack_aux->priority++;
			//mem_debug("%lld->%d->%d ", stack_aux->id, stack_aux->priority, stack_aux->client_info->kind);
			count=stack_aux->priority;
		}
	}

	stack->priority=count;
	//mem_debug("%lld->%d->%d \n", stack->id, stack->priority, stack->client_info->kind);
}

void mem_controller_coalesce_pref_into_normal(struct mod_stack_t* stack)
{

	unsigned int log2_row_size;
	unsigned int bank;
	struct mod_stack_t * stack_aux;
	struct mem_controller_t * mem_controller;
	int pig=0;

	LINKED_LIST_FOR_EACH(mem_system->mem_controllers)
	{
		mem_controller=linked_list_get(mem_system->mem_controllers);
		pig = pig | mem_controller->piggybacking;
		if(!mem_controller->piggybacking)
			continue;
		log2_row_size= log_base2( mem_controller->row_buffer_size);
		if(mem_controller->queue_per_bank)
			bank = ((stack->addr >> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));
		else
			bank=0;
		assert(mem_controller->piggybacking);
		LINKED_LIST_FOR_EACH(mem_controller->pref_queue[bank]->queue)
		{
			stack_aux=linked_list_get(mem_controller->pref_queue[bank]->queue);
			assert(stack->client_info->core!=-1);
			assert(stack_aux->client_info->core!=-1);
			if(stack->addr==stack_aux->addr && stack->client_info->core == stack_aux->client_info->core)
			{
				/*TODO Afegir estaidsitques de cuan la cola se plena y se buida*/
				linked_list_head(mem_controller->normal_queue[bank]->queue);
				linked_list_add(mem_controller->normal_queue[bank]->queue, stack_aux);
				linked_list_remove(mem_controller->pref_queue[bank]->queue);
				return;
			}
		}
	}
	if(pig)
	{
		struct tuple_piggybacking_t *tuple_pig= xcalloc(1, sizeof(struct tuple_piggybacking_t));
		tuple_pig->addr=stack->addr;
		tuple_pig->core= stack->client_info->core;
		tuple_pig->thread= stack->client_info->thread;

		linked_list_tail(mem_system->pref_into_normal);
		linked_list_add(mem_system->pref_into_normal, tuple_pig);

		assert(stack->client_info->core!=-1);


	}

}

int mem_controller_is_piggybacked(struct mod_stack_t * stack)
{
	struct tuple_piggybacking_t * tuple_pig;
	assert(stack->client_info->core != -1);
	linked_list_head(mem_system->pref_into_normal);

	while(!linked_list_is_end(mem_system->pref_into_normal))
	{
		tuple_pig=linked_list_get(mem_system->pref_into_normal);

		assert(tuple_pig->core != -1);
		assert(tuple_pig->thread != -1);

		if(tuple_pig->addr==stack->addr && tuple_pig->core == stack->client_info->core)
		{
			free(tuple_pig);
			linked_list_remove(mem_system->pref_into_normal);
			return 1;
		}

		linked_list_next(mem_system->pref_into_normal);

	}


	return 0;
}


void main_mem_trace(const char *fmt, ...)
{
	va_list va;
	char buf[MAX_STRING_SIZE];
	int len;

	/* Do nothing is no file name was given */
	if (!trace_file)
		return;

	va_start(va, fmt);
	len = vsnprintf(buf, sizeof buf, fmt, va);

	/* Message exceeded buffer */
	if (len + 1 == sizeof buf)
		fatal("%s: buffer too small", __FUNCTION__);

	/* Dump message */
	gzwrite(trace_file, buf, len);
}


void main_mem_trace_init(char *file_name)
{
	/* Do nothing is no file name was given */
	if (!file_name || !*file_name)
		return;

	/* Open destination file */
	trace_file = gzopen(file_name, "wt");
	if (!trace_file)
		fatal("%s: cannot open trace file", file_name);
}


void main_mem_trace_done(void)
{
	/* Nothing if trace is inactive */
	if (!trace_file)
		return;

	/* Close trace file */
	gzclose(trace_file);
}


void mem_controller_report_schedule(struct mem_controller_t *mem_controller)
{
	struct mem_controller_report_stack_t *stack;
	struct line_writer_t *lw;
	FILE *f = mem_controller->report_file;
	int size;
	int i;

	/* Create new stack */
	stack = xcalloc(1, sizeof(struct mem_controller_report_stack_t));

	/* Initialize */
	assert(mem_controller->report_file);
	assert(mem_controller->report_interval > 0);
	stack->mem_controller = mem_controller;

	/* Print header */
	fprintf(f, "%s", help_mem_controller_report);

	lw = line_writer_create(" ");
	lw->heuristic_size_enabled = 1;

	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "cycle");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "inst");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "acceses");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "served");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "t-total");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "t-wait");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "t-acces");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "t-transfer");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "normal-acceses");
	line_writer_add_column(lw, 9, line_writer_align_right, "%s", "prefetch-acceses");
	

	size = line_writer_write(lw, f);
	line_writer_clear(lw);

	for (i = 0; i < size - 1; i++)
		fprintf(f, "-");
	fprintf(f, "\n");

	mem_controller->report_stack = stack;
	stack->line_writer = lw;

	/* Schedule first event */
	if(mem_controller->report_interval_kind == interval_kind_cycles)
		esim_schedule_event(EV_MEM_CONTROLLER_REPORT, stack, mem_controller->report_interval);
}


void mem_controller_report_handler(int event, void *data)
{
	struct mem_controller_report_stack_t *stack = data;
	struct mem_controller_t *mem_controller = stack->mem_controller;
	struct line_writer_t *lw = stack->line_writer;

	/* If simulation has ended, no more
	 * events to schedule. */
	if (esim_finish)
		return;

	long long accesses = mem_controller->accesses - stack->accesses;
	long long served = 0;
	for(int i = 0; i < mem_controller->num_regs_channel; i++)
		served += mem_controller->regs_channel[i].num_requests_transfered;
	long long served_int = served - stack->served;
	long long t_wait = mem_controller->t_wait - stack->t_wait;
	long long t_acces = mem_controller->t_acces_main_memory - stack->t_acces;
	long long t_transfer = mem_controller->t_transfer - stack->t_transfer;
	long long t_total=t_wait+t_acces+t_transfer;
	long long normal_accesses = mem_controller->normal_accesses-stack->normal_accesses;
	long long pref_accesses = mem_controller->pref_accesses - stack->pref_accesses;



	/* Dump stats */
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", esim_cycle());
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", stack->inst_count);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", accesses);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", served_int);
	line_writer_add_column(lw, 9, line_writer_align_right, "%.3f", served_int > 0 ? (double) t_total / served_int : 0);
	line_writer_add_column(lw, 9, line_writer_align_right, "%.3f", served_int > 0 ? (double) t_wait / served_int : 0);
	line_writer_add_column(lw, 9, line_writer_align_right, "%.3f", served_int > 0 ? (double) t_acces / served_int : 0);
	line_writer_add_column(lw, 9, line_writer_align_right, "%.3f", served_int > 0 ? (double) t_transfer / served_int : 0);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", normal_accesses);
	line_writer_add_column(lw, 9, line_writer_align_right, "%lld", pref_accesses);

	line_writer_write(lw, mem_controller->report_file);
	line_writer_clear(lw);

	/* Update counters */
	stack->accesses = mem_controller->accesses;
	stack->served = served;
	stack->normal_accesses = mem_controller->normal_accesses;
	stack->pref_accesses = mem_controller->pref_accesses;
	stack->t_wait = mem_controller->t_wait;
	stack->t_acces = mem_controller->t_acces_main_memory;
	stack->t_transfer = mem_controller->t_transfer;

	/* Schedule new event */
	assert(mem_controller->report_interval);


	if(mem_controller->report_interval_kind == interval_kind_cycles)
		esim_schedule_event(event, stack, mem_controller->report_interval);
}


struct row_buffer_table_set_t* mem_controller_row_buffer_table_get_set(struct mod_stack_t *stack)
{
	struct mem_controller_t *mem_controller = stack->mod->mem_controller;
	
	unsigned int num_ranks = mem_controller->num_regs_rank ;
	unsigned int num_banks = mem_controller->num_regs_bank ;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
	
	int set_rbtable = ((stack->addr >> log2_row_size) % (num_banks*num_ranks));
	
	assert(mem_controller->row_buffer_table->sets[set_rbtable].bank == set_rbtable);
	return 	&mem_controller->row_buffer_table->sets[set_rbtable];	

} 

struct row_buffer_table_entry_t *mem_controller_row_buffer_table_get_entry(struct mod_stack_t *stack)
{
	struct mem_controller_t *mem_controller = stack->mod->mem_controller;
	
	struct row_buffer_table_set_t *set= mem_controller_row_buffer_table_get_set(stack);
	

	for(int i =0 ;i <mem_controller->row_buffer_table->assoc; i++)
	{		
		if(set->entries[i].row==stack->row)
			return &set->entries[i];
		
	}
	
	return NULL;

} 

struct row_buffer_table_entry_t *mem_controller_row_buffer_table_get_reserved_entry(struct mod_stack_t *stack)
{
	struct mem_controller_t *mem_controller = stack->mod->mem_controller;
	
	struct row_buffer_table_set_t *set= mem_controller_row_buffer_table_get_set(stack);
	

	for(int i =0 ;i <mem_controller->row_buffer_table->assoc; i++)
	{		
		if(set->entries[i].reserved==stack->id)
		{
			//printf("%lld agarra la reserva  %d  banc%d\n", stack->id, i , set->bank);				
			return &set->entries[i];
		}
		
	}
	
	return NULL;

} 


void mem_controller_row_buffer_table_reserve_entry(struct mod_stack_t *stack)
{
	struct mem_controller_t *mem_controller = stack->mod->mem_controller;
	int lru_entry = -1;
	int found = 0;

	assert(mem_controller->enable_row_buffer_table);
	struct row_buffer_table_set_t *set= mem_controller_row_buffer_table_get_set(stack);
	for(int i =0 ;i <mem_controller->row_buffer_table->assoc; i++)
	{		
		if(set->entries[i].reserved == -1 && !set->entries[i].accessed)
		{
			if(!found)
			{
				lru_entry=i;
				found=1;
			}else if(set->entries[lru_entry].lru!=-1 && set->entries[i].lru==-1)
		
	{
				lru_entry=i;
			}
			else if( set->entries[lru_entry].lru>set->entries[i].lru )
			{
				lru_entry=i;
				
			}
			
		}
	}
	//printf("%lld reserva entra %d  banc%d\n", stack->id, lru_entry , set->bank);
	assert(lru_entry!=-1);
	set->entries[lru_entry].reserved=stack->id;
				
}

void mem_controller_row_buffer_allocate_row(struct mod_stack_t *stack)
{
	struct mem_controller_t *mem_controller = stack->mod->mem_controller;
	int lru_entry = -1;
	int found = 0;

	struct reg_bank_t bank= mem_controller->regs_channel[stack->channel].regs_rank[stack->rank].regs_bank[stack->bank];
	for(int i =0 ;i <bank.row_buffer_per_bank; i++)
	{		
		if(!found)
		{
			lru_entry=i;
			found=1;
		}else if(bank.row_buffers[lru_entry].lru!=-1 && bank.row_buffers[i].lru==-1)
		{
			lru_entry=i;
		}
		else if( bank.row_buffers[lru_entry].lru>bank.row_buffers[i].lru )
		{
			lru_entry=i;
			
		}
			
	
	}
	//printf("asigna la fila %d en la fila%d b%d r%d\n",stack->row, lru_entry, stack->bank, stack->rank );
	//printf("%lld reserva entra %d  banc%d\n", stack->id, lru_entry , set->bank);
	assert(lru_entry!=-1);
	//bank.row_buffers[lru_entry].reserved=stack->id;
	bank.row_buffers[lru_entry].row = stack->row;
	bank.row_buffers[lru_entry].lru=esim_cycle();
				
}

