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

#ifndef DRAM_BANK_H
#define DRAM_BANK_H

#include <stdio.h>

/*Row buffer*/
struct row_buffer_t
{
	long long int lru; // cycle accessed
	int row;

};

/*
 * Bank
 */



struct reg_bank_t{

        int row_is_been_accesed; // row which is been accessed
        struct row_buffer_t * row_buffers; // row which is inside row buffer
        int is_been_accesed;// show if a bank is been accedid for some instruction
	long long int t_row_come; // instant when the row starts to be transfered into row buffer
	int row_buffer_per_bank; // number of row buffer inside a bank

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

struct reg_bank_t* regs_bank_create( int num_banks, int t_row_hit, int t_row_miss, int rb_per_bank);


#endif
