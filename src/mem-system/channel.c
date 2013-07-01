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

#include <assert.h>
#include <stdlib.h>

#include <lib/esim/trace.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <lib/util/misc.h>
#include <lib/util/string.h>



#include "channel.h"
#include "rank.h"
#include "bank.h"

/*
 * Channel
 */

struct reg_channel_t* regs_channel_create( int num_channels, int num_ranks, int num_banks, int bandwith, struct reg_rank_t* regs_rank ){

        struct reg_channel_t * channels;
        channels = xcalloc(num_channels, sizeof(struct reg_channel_t));
        if (!channels)
                fatal("%s: out of memory", __FUNCTION__);


        for(int i=0; i<num_channels;i++){
                channels[i].state=channel_state_free;
                channels[i].num_regs_rank=num_ranks;
                channels[i].bandwith=bandwith;
                channels[i].regs_rank= regs_rank;
        }

        return channels;

}

void reg_channel_free(struct reg_channel_t * channels, int num_channels){

        for(int c=0; c<num_channels;c++)
                reg_rank_free(channels[c].regs_rank, channels[c].num_regs_rank);

        free(channels);

}

