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

#ifndef DRAM_RANK_H
#define DRAM_RANK_H

#include <stdio.h>


/*
 * Rank
 */

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


struct reg_rank_t* regs_rank_create( int num_ranks, int num_banks, int t_row_buffer_miss, int t_row_buffer_hit, int rb_per_bank);
void reg_rank_free(struct reg_rank_t * ranks, int num_ranks);


#endif
