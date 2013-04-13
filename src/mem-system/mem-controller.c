#include <assert.h>
#include <stdlib.h>

#include <lib/esim/trace.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/misc.h>
#include <lib/util/string.h>
/////////////////////////
#include "mem-controller.h"
#include "mem-system.h"
#include <lib/util/linked-list.h>
#include <lib/esim/esim.h>
#include "mod-stack.h"///
////////////////////////

int row_buffer_find_row(struct mod_t *mod, unsigned int addr, unsigned int *channel_ptr,
	unsigned int *rank_ptr, unsigned int *bank_ptr, unsigned int *row_ptr,  int * tag_ptr, int *state_ptr)
{
	//struct cache_t *cache = mod->cache;
	struct mem_controller_t *  mem_controller=mem_system->mem_controller;

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
	
	/*Is the channel free?*/
	if(mod->regs_channel[channel].state==channel_state_busy){

		PTR_ASSIGN(state_ptr, channel_busy);
		return 0;
	}
	
	/*Is the bank been acceded?*/
	if(mod->regs_channel[channel].regs_rank[rank].regs_bank[bank].is_been_accesed){
		PTR_ASSIGN(state_ptr, bank_accesed);
		return 0;
	}


	/*Is the row inside the row buffer?*/
	if(mod->regs_channel[channel].regs_rank[rank].regs_bank[bank].row_buffer!=row){
		PTR_ASSIGN(state_ptr, row_buffer_miss);
		return 1;
	}

	PTR_ASSIGN(state_ptr, row_buffer_hit);
	return 1;

	
	

}



//////////////////////////////////////////////////////////////////////////////////
struct mem_controller_queue_t *mem_controller_queue_create(void){
	struct mem_controller_queue_t *mem_controller_queue;
	mem_controller_queue = calloc(1, sizeof(struct mem_controller_queue_t));
	if (!mem_controller_queue)
		fatal("%s: out of memory", __FUNCTION__);

	/* Create stack list */
	mem_controller_queue->queue=linked_list_create();
	

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
	mem_controller = calloc(1, sizeof(struct mem_controller_t));
	if (!mem_controller)
		fatal("%s: out of memory", __FUNCTION__);

	return mem_controller;

}


void mem_controller_init_main_memory(struct mem_controller_t *mem_controller, int channels, int ranks, int banks, int t_send_request, int row_size, int cycles_proc_bus, enum policy_mc_queue_t policy, enum priority_t priority, long long size_queue,  long long threshold, int queue_per_bank){

	mem_controller->num_queues=1;

	if(queue_per_bank)
		mem_controller->num_queues=banks*ranks*channels;

	mem_controller->num_regs_channel = channels;
	mem_controller->num_regs_rank = ranks;
	mem_controller->num_regs_bank = banks;
	mem_controller->row_buffer_size=row_size;
	mem_controller->t_send_request=t_send_request;
	mem_controller->cycles_proc_bus=cycles_proc_bus;
	//////////////////////////////////////////////////
	mem_controller->size_queue=size_queue;
	mem_controller->policy_queues=policy;
	mem_controller->threshold=threshold;
	mem_controller->priority_request_in_queue=priority;
	mem_controller->queue_per_bank=queue_per_bank;
	mem_controller->queue_round_robin=0;
	
	mem_controller->normal_queue=calloc(mem_controller->num_queues, sizeof(struct mem_controller_queue_t *));
	mem_controller->pref_queue=calloc(mem_controller->num_queues, sizeof(struct mem_controller_queue_t *));
	for(int i=0; i<mem_controller->num_queues;i++)
	{
		mem_controller->normal_queue[i]=mem_controller_queue_create();
		mem_controller->pref_queue[i]=mem_controller_queue_create();
	}

	
	////////////////////////////////////////////////

	/*mem_controller->row_in_buffer_banks = calloc(channels, sizeof(int **));
	if (!mem_controller->row_in_buffer_banks)
		fatal("%s: out of memory", __FUNCTION__);
	
	for(int c=0; c<channels;c++){

		mem_controller->row_in_buffer_banks[c] =calloc(ranks, sizeof(int *));
		if (!mem_controller->row_in_buffer_banks[c])
			fatal("%s: out of memory", __FUNCTION__);

		for(int r=0; r<ranks;r++){
			mem_controller->row_in_buffer_banks[c][r] = calloc(banks, sizeof( int));
			if (!mem_controller->row_in_buffer_banks[c][r])
				fatal("%s: out of memory", __FUNCTION__);

			for(int b=0; b<banks;b++)
				mem_controller->row_in_buffer_banks[c][r][b]=-1;
		}
	}*/


}


void mem_controller_free(struct mem_controller_t *mem_controller){

	


	/* Free prefetch queue */
	///////////////////////////////////////////////////
	for(int i=0; i<mem_controller->num_queues;i++)
		mem_controller_queue_free(mem_controller->pref_queue[i]);

	free(mem_controller->pref_queue);
	
	/* Free normal queue */
	for(int i=0; i<mem_controller->num_queues;i++)
		mem_controller_queue_free(mem_controller->normal_queue[i]);

	free(mem_controller->normal_queue);
	///////////////////////////////////////////////////////////
	

	free(mem_controller);


}


void mem_controller_normal_queue_add(struct mod_stack_t * stack){
	
				

	struct mem_controller_t * mem_controller=mem_system->mem_controller;
	//////////////////////////////////////////////////////////////////////////
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
	unsigned int bank, channel, row_buffer;
	row_buffer = stack->addr &  mem_controller->row_buffer_size;
	channel=(row_buffer >>7 )%mem_controller->num_regs_channel;


	if(mem_controller->queue_per_bank)
		bank = channel|((stack->addr >> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));
	else
		bank=0;

	
	//printf("bank=%d  %d \n", bank,(stack->addr >> log2_row_size) %mem_controller->num_regs_bank);
	
	stack->threshold =mem_controller->threshold;
	linked_list_tail(mem_controller->normal_queue[bank]->queue);
	linked_list_add(mem_controller->normal_queue[bank]->queue, stack);
	linked_list_head(mem_controller->normal_queue[bank]->queue);

	/*Now queue is full?*/
	 if(linked_list_count(mem_controller->normal_queue[bank]->queue)==mem_controller->size_queue)
         	mem_controller->normal_queue[bank]->instant_begin_full=esim_cycle;

 	mem_controller->normal_queue[bank]->total_insertions++;
	////////////////////////////////////////////////////////////////

}


void mem_controller_prefetch_queue_add(struct mod_stack_t * stack){
	

	struct mem_controller_t * mem_controller=mem_system->mem_controller;
	unsigned int log2_row_size= log_base2( mem_controller->row_buffer_size);
	unsigned int bank, channel, row_buffer;

	row_buffer = stack->addr &  mem_controller->row_buffer_size;
	channel=(row_buffer >>7 )%mem_controller->num_regs_channel;


	if(mem_controller->queue_per_bank)
		bank =channel|((stack->addr >> log2_row_size) % (mem_controller->num_regs_bank*mem_controller->num_regs_rank));
	else
		bank=0;
	//printf("bank pref=%d  %d \n", bank,(stack->addr >> log2_row_size) %mem_controller->num_regs_bank);
	stack->threshold=mem_system->mem_controller->threshold;
	linked_list_tail(mem_controller->pref_queue[bank]->queue);
	linked_list_add(mem_controller->pref_queue[bank]->queue, stack);
	linked_list_head(mem_controller->pref_queue[bank]->queue);

	/*Now queue is full?*/
	 if(linked_list_count(mem_controller->pref_queue[bank]->queue)==mem_controller->size_queue)
         	mem_controller->pref_queue[bank]->instant_begin_full=esim_cycle;
	
	mem_controller->pref_queue[bank]->total_insertions++;

}

int mem_controller_remove(struct mod_stack_t * stack, struct mem_controller_queue_t * queue){
	
	linked_list_head(queue->queue);
	while(!linked_list_is_end(queue->queue)){
		struct mod_stack_t * stack_aux=linked_list_get(queue->queue);
		if(stack_aux->id==stack->id){
			////printf("borra %lld\n", stack->id);
			linked_list_remove(queue->queue);
			return 1;
		}
		linked_list_next(queue->queue);
	}
	return 0;

}







/*ROUND ROBIN*/

int mem_controller_get_bank_queue(int num_queue_examined)
{
	struct mem_controller_t * mem_controller=mem_system->mem_controller;
	int pos=(mem_controller->queue_round_robin+num_queue_examined)%mem_controller->num_queues;
	assert(pos>=0);
	assert(pos<mem_controller->num_queues);
	return  pos;
		
}


struct mod_stack_t * mem_controller_select_request(int n_queues_examined, enum priority_t priority)
{
	
	struct mem_controller_t * mem_controller=mem_system->mem_controller;
	int can_acces_bank;
	int size_queue=mem_controller->size_queue;

	struct mem_controller_queue_t * normal_queue= mem_controller->normal_queue[n_queues_examined%mem_controller->num_queues];
	struct mem_controller_queue_t * pref_queue= mem_controller->pref_queue[n_queues_examined%mem_controller->num_queues];

	/*First priority: threshold normal*/
	linked_list_head(normal_queue->queue);
	while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
		if(stack->threshold==0)
		{
			can_acces_bank=row_buffer_find_row(stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
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
			can_acces_bank=row_buffer_find_row(stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, NULL);
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
		can_acces_bank=row_buffer_find_row(stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank&&stack->state==row_buffer_hit)
			return stack;
		linked_list_next(normal_queue->queue);
	}

	if(priority==prio_threshold_normal_pref)
	{
		/*Four priority: FCFS normal*/
		linked_list_head(normal_queue->queue);
		while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
		{
			struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
			can_acces_bank=row_buffer_find_row(stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
			if(can_acces_bank)
				return stack;
			linked_list_next(normal_queue->queue);
		}

		/*Five priority: row buffer hit prefetch*/
		linked_list_head(pref_queue->queue);
		while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
		{
			struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
			can_acces_bank=row_buffer_find_row(stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
			if(can_acces_bank&&stack->state==row_buffer_hit)
				return stack;
			linked_list_next(pref_queue->queue);
		}

	
	}
	else // row buffer hit> fcfs
	{
		/*Four priority: row buffer hit prefetch*/
		linked_list_head(pref_queue->queue);
		while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
		{
			struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
			can_acces_bank=row_buffer_find_row(stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
			if(can_acces_bank&&stack->state==row_buffer_hit)
				return stack;
			linked_list_next(pref_queue->queue);
		}

		/*Five priority: FCFS normal*/
		linked_list_head(normal_queue->queue);
		while(!linked_list_is_end(normal_queue->queue)&&linked_list_current(normal_queue->queue)<size_queue)
		{
			struct mod_stack_t *stack=linked_list_get(normal_queue->queue);
			can_acces_bank=row_buffer_find_row(stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
			if(can_acces_bank)
				return stack;
			linked_list_next(normal_queue->queue);
		}	
	}

	/*Six priority: FCFS normal*/
	linked_list_head(pref_queue->queue);
	while(!linked_list_is_end(pref_queue->queue)&&linked_list_current(pref_queue->queue)<size_queue)
	{
		struct mod_stack_t *stack=linked_list_get(pref_queue->queue);
		can_acces_bank=row_buffer_find_row(stack->mod,stack->addr,NULL,NULL, NULL,NULL,NULL, &stack->state);
		if(can_acces_bank)
			return stack;
		linked_list_next(pref_queue->queue);
	}
	return NULL;
	
}




int mem_controller_queue_has_consumed_threshold(struct linked_list_t * queue)
{
	struct mod_stack_t * stack;
	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<mem_system->mem_controller->size_queue){
		stack=linked_list_get(queue);
		if(stack->threshold==0)
			return 1;
		linked_list_next(queue);
	}
	return 0;



}


int mem_controller_queue_has_row_buffer_hit(struct linked_list_t * queue)
{
	int state;
	struct mod_stack_t * stack;
	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<mem_system->mem_controller->size_queue){
		stack=linked_list_get(queue);
		row_buffer_find_row(stack->mod, stack->addr, NULL,NULL,NULL, NULL, NULL, &state);
		if(state==row_buffer_hit)
			return 1;
		linked_list_next(queue);
	}
	return 0;



}

void mem_controller_update_requests_threshold(int cycles)
{
	struct mem_controller_t * mem_controller=mem_system->mem_controller;
	struct mod_stack_t * stack;
	
	for(int i=0; i<mem_controller->num_queues;i++)
	{
		/*Decrease threshold of prefetch request 1 cycle */
		linked_list_head(mem_controller->pref_queue[i]->queue);
		while(!linked_list_is_end(mem_controller->pref_queue[i]->queue)){
			stack=linked_list_get(mem_controller->pref_queue[i]->queue);
			if((stack->threshold-cycles)>0)
				stack->threshold-=cycles;
			else
				stack->threshold=0;
			linked_list_next(mem_controller->pref_queue[i]->queue);
		}
	
		/*Decrease threshold of normal request 1 cycle */	
		linked_list_head(mem_controller->normal_queue[i]->queue);
		while(!linked_list_is_end(mem_controller->normal_queue[i]->queue)){
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
int mem_controller_stacks_normalQueues_count()
{
	int num_stacks=0;

	for(int i=0; i<mem_system->mem_controller->num_queues;i++)
			num_stacks+=linked_list_count(mem_system->mem_controller->normal_queue[i]->queue);
	return num_stacks;
}


int mem_controller_stacks_prefQueues_count()
{
	int num_stacks=0;
	
	for(int i=0; i<mem_system->mem_controller->num_queues;i++)
			num_stacks+=linked_list_count(mem_system->mem_controller->pref_queue[i]->queue);
	return num_stacks;
}
//////////////////////////////////////////////////////////////////



void mem_controller_coalesce_acces_row_buffer( struct mod_stack_t * stack, struct linked_list_t * queue)
{
	struct mod_stack_t * stack_aux;
	unsigned int num_ranks = mem_system->mem_controller->num_regs_rank ;
	unsigned int num_banks = mem_system->mem_controller->num_regs_bank ;
	unsigned int log2_row_size= log_base2( mem_system->mem_controller->row_buffer_size);
	unsigned int num_channels = mem_system->mem_controller->num_regs_channel ;

	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<mem_system->mem_controller->size_queue)
	{
		stack_aux=linked_list_get(queue);
		
		/*This stack has its block in the same main mamory row than origin stack*/
		unsigned int row_buffer = stack_aux->addr &  mem_system->mem_controller->row_buffer_size;
		int row=(stack_aux->addr>>(log2_row_size+log_base2(num_banks)+log_base2(num_ranks)));
		int rank = (stack_aux->addr >> (log2_row_size+ log_base2(num_banks))) % num_ranks;
		int bank = (stack_aux->addr >> log2_row_size) % num_banks;
		int channel=(row_buffer >>7 )%num_channels;
		

		assert(rank==stack->rank && bank==stack->bank && channel==stack->channel);
		
	
		if(row==stack->row)
		{
			/*Coalesce*/
			mem_debug("   stack %lld coalesced with stack %lld\n", stack_aux->id, stack->id);
			if(stack->coalesced_stacks==NULL)
				stack->coalesced_stacks=linked_list_create();
			linked_list_add(stack->coalesced_stacks, stack_aux);
			linked_list_remove(queue);
		}else
			linked_list_next(queue);

	}

}

int mem_controller_coalesce_acces_between_blocks(struct mod_stack_t * stack, struct linked_list_t *queue, int block_min, int block_max)
{

	struct mod_stack_t * stack_aux;
	unsigned int num_ranks = mem_system->mem_controller->num_regs_rank ;
	unsigned int num_banks = mem_system->mem_controller->num_regs_bank ;
	unsigned int log2_row_size= log_base2( mem_system->mem_controller->row_buffer_size);
	unsigned int num_channels = mem_system->mem_controller->num_regs_channel ;
	int n_coal=0;

	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<mem_system->mem_controller->size_queue)
	{
		stack_aux=linked_list_get(queue);
		
		/*This stack has its block in the same main mamory row than origin stack*/
		unsigned int row_buffer = stack_aux->addr &  mem_system->mem_controller->row_buffer_size;
		unsigned int row=(stack_aux->addr>>(log2_row_size+log_base2(num_banks)+log_base2(num_ranks)));
		unsigned int rank = (stack_aux->addr >> (log2_row_size+ log_base2(num_banks))) % num_ranks;
		unsigned int bank = (stack_aux->addr >> log2_row_size) % num_banks;
		unsigned int channel=(row_buffer >>7 )%num_channels;
		unsigned int block=stack_aux->addr %  mem_system->mem_controller->row_buffer_size;

		assert(rank==stack->rank && bank==stack->bank && channel==stack->channel);
		
		/*Is this request between the min and max block? */
		if(row==stack->row && block>=block_min&& block<=block_max)
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

	linked_list_head(queue);
	while(!linked_list_is_end(queue)&&linked_list_current(queue)<mem_system->mem_controller->size_queue)
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
	struct mem_controller_t * mem_controller=mem_system->mem_controller;
	unsigned int block;
	
	
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
	
	/*TODO quitar cuando se vea que va bien*/
	linked_list_head(list);
	while(!linked_list_is_end(list))
	{
		stack_aux=linked_list_get(list);
		printf("%d . ", stack_aux->addr %  mem_controller->row_buffer_size);
		linked_list_next(list);
	}
	printf("\n");

	linked_list_free(stack->coalesced_stacks);
	stack->coalesced_stacks=list;

	//linked_list_free(list);
	assert(stack->coalesced_stacks!=NULL);
	//assert( linked_list_count(stack->coalesced_stacks)>0);

}

unsigned int mem_controller_min_block(struct mod_stack_t *stack)
{

	struct linked_list_t * queue=stack->coalesced_stacks;
	struct mod_stack_t * stack_aux;
	struct mem_controller_t * mem_controller= mem_system->mem_controller;
	unsigned int first;
	unsigned int block;

	first = stack->addr %  mem_controller->row_buffer_size;

	linked_list_head(queue);
	while(!linked_list_is_end(queue))
	{
		stack_aux=linked_list_get(queue);
		block=stack_aux->addr %  mem_controller->row_buffer_size;


		if(first>block)
			first=block;

		linked_list_next(queue);
		
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
	struct mem_controller_t * mem_controller= mem_system->mem_controller;
	unsigned int last;
	unsigned int block;

	last = stack->addr %  mem_controller->row_buffer_size;

	linked_list_head(queue);
	while(!linked_list_is_end(queue))
	{
		stack_aux=linked_list_get(queue);
		block=stack_aux->addr %  mem_controller->row_buffer_size;

		if(last<block)
			last=block;

		linked_list_next(queue);
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
	struct mem_controller_t * mem_controller= mem_system->mem_controller;
	unsigned int first;
	unsigned int last;
	unsigned int block;

	/*Initialize*/
	first = last = stack->addr %  mem_controller->row_buffer_size;
	linked_list_head(queue);
	while(!linked_list_is_end(queue))
	{
		stack_aux=linked_list_get(queue);
		block=stack_aux->addr %  mem_controller->row_buffer_size;
		/*Get fist block of this row*/
		if(first>block)
			first=block;
		
		else if(last<block)
			last=block;

		linked_list_next(queue);
		
	}
	assert(last+stack->mod->cache->block_size <= mem_controller->row_buffer_size);
	assert(first>=0 && last>=0);
	assert((last-first) % stack->mod->cache->block_size==0);

	return (last-first)/stack->mod->cache->block_size + 1;

}

