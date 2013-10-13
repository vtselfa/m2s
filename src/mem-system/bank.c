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


#include "bank.h"


/*
 * Bank
 */

struct reg_bank_t* regs_bank_create( int num_banks, int t_row_hit, int t_row_miss, int row_buffer_per_bank){

        struct reg_bank_t * banks;
        banks = xcalloc( num_banks, sizeof(struct reg_bank_t));
        if (!banks)
                fatal("%s: out of memory", __FUNCTION__);
	
        for(int i=0; i<num_banks;i++){
		banks[i].row_buffer_per_bank = row_buffer_per_bank;
		banks[i].row_buffers=xcalloc(row_buffer_per_bank, sizeof(struct row_buffer_t));
		for(int j=0; j<row_buffer_per_bank;j++)
		{
               		banks[i].row_buffers[j].row=-1;
			banks[i].row_buffers[j].lru=-1;
		}
                //banks[i].row_is_been_accesed=-1;
                banks[i].t_row_buffer_miss=t_row_miss;
                banks[i].t_row_buffer_hit=t_row_hit;
        }

        return banks;


}
