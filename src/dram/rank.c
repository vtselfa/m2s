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


#include <lib/mhandle/mhandle.h>


#include "rank.h"

/*
 * Rank
 */

struct reg_rank_t* regs_rank_create( int num_ranks, int num_banks, int t_row_hit, int t_row_miss){

        struct reg_rank_t * ranks;
        ranks = calloc(num_ranks, sizeof(struct reg_rank_t));
        if (!ranks)
                fatal("%s: out of memory", __FUNCTION__);


        for(int i=0; i<num_ranks;i++){
                ranks[i].num_regs_bank=num_banks;
                ranks[i].regs_bank=regs_bank_create(num_banks, t_row_hit, t_row_miss);

        }

        return ranks;

}

void reg_rank_free(struct reg_rank_t * rank, int num_ranks){

        for(int r=0; r<num_ranks;r++ )
                free(rank[r].regs_bank);

        free(rank);

}

