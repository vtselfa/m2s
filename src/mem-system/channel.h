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

#ifndef CHANNEL_H
#define CHANNEL_H

#include <stdio.h>
#include "rank.h"

/*
 * Channel
 */

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
	
	long long num_pref_requests_transfered;
	long long num_normal_requests_transfered;
};


struct reg_channel_t* regs_channel_create( int num_channels, int num_ranks, int num_banks, int bandwith,struct reg_rank_t * regs_rank);
void reg_channel_free(struct reg_channel_t * channels, int num_channels);


#endif
