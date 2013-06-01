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

#include <arch/x86/emu/context.h>
#include <arch/x86/emu/regs.h>
#include <lib/mhandle/mhandle.h>
#include <lib/util/debug.h>
#include <mem-system/memory.h>

#include "glu.h"


static char *glu_err_code =
	"\tAn invalid function code was generated by your application in a GLU system\n"
	"\tcall. Probably, this means that your application is using an incompatible\n"
	"\tversion of the Multi2Sim GLU runtime library ('libm2s-glu'). Please\n"
	"\trecompile your application and try again.\n";


/* Debug */
int glu_debug_category;


/*
 * GLU calls
 */

/* List of GLU runtime calls */
enum glu_call_t
{
	glu_call_invalid = 0,
#define X86_GLU_DEFINE_CALL(name, code) glu_call_##name = code,
#include "glu.dat"
#undef X86_GLU_DEFINE_CALL
	glu_call_count
};


/* List of GLU runtime call names */
char *glu_call_name[glu_call_count + 1] =
{
	NULL,
#define X86_GLU_DEFINE_CALL(name, code) #name,
#include "glu.dat"
#undef X86_GLU_DEFINE_CALL
	NULL
};


/* Forward declarations of GLU runtime functions */
#define X86_GLU_DEFINE_CALL(name, code) \
	static int glu_abi_##name(struct x86_ctx_t *ctx);
#include "glu.dat"
#undef X86_GLU_DEFINE_CALL

/* List of GLU runtime functions */
typedef int (*glu_abi_func_t)(struct x86_ctx_t *ctx);
static glu_abi_func_t glu_abi_table[glu_call_count + 1] =
{
	NULL,
#define X86_GLU_DEFINE_CALL(name, code) glu_abi_##name,
#include "glu.dat"
#undef X86_GLU_DEFINE_CALL
	NULL
};





/*
 * GLU global variables
 */


/*
 * GLU global functions
 */

void glu_init(void)
{

}


void glu_done(void)
{

}


int glu_abi_call(struct x86_ctx_t *ctx)
{
	struct x86_regs_t *regs = ctx->regs;

	int code;
	int ret;

	/* Function code */
	code = regs->ebx;
	if (code <= glu_call_invalid || code >= glu_call_count)
		fatal("%s: invalid GLU function (code %d).\n%s",
			__FUNCTION__, code, glu_err_code);

	/* Debug */
	glu_debug("GLU runtime call '%s' (code %d)\n",
		glu_call_name[code], code);

	/* Call GLU function */
	assert(glu_abi_table[code]);
	ret = glu_abi_table[code](ctx);

	/* Return value */
	return ret;
}




/*
 * GLU call #1 - init
 *
 * @param struct glu_version_t *version;
 *	Structure where the version of the GLU runtime implementation will be
 *	dumped. To succeed, the major version should match in the runtime
 *	library (guest) and runtime implementation (host), whereas the minor
 *	version should be equal or higher in the implementation (host).
 *
 *	Features should be added to the GLU runtime (guest and host) using the
 *	following rules:
 *	1)  If the guest library requires a new feature from the host
 *	    implementation, the feature is added to the host, and the minor
 *	    version is updated to the current Multi2Sim SVN revision both in
 *	    host and guest.
 *          All previous services provided by the host should remain available
 *          and backward-compatible. Executing a newer library on the older
 *          simulator will fail, but an older library on the newer simulator
 *          will succeed.
 *      2)  If a new feature is added that affects older services of the host
 *          implementation breaking backward compatibility, the major version is
 *          increased by 1 in the host and guest code.
 *          Executing a library with a different (lower or higher) major version
 *          than the host implementation will fail.
 *
 * @return
 *	The runtime implementation version is return in argument 'version'.
 *	The return value is always 0.
 */

#define X86_GLU_RUNTIME_VERSION_MAJOR	1
#define X86_GLU_RUNTIME_VERSION_MINOR	690

struct glu_version_t
{
	int major;
	int minor;
};

static int glu_abi_init(struct x86_ctx_t *ctx)
{
	struct x86_regs_t *regs = ctx->regs;
	struct mem_t *mem = ctx->mem;

	unsigned int version_ptr;
	struct glu_version_t version;

	/* Arguments */
	version_ptr = regs->ecx;
	glu_debug("\tversion_ptr=0x%x\n", version_ptr);

	/* Return version */
	assert(sizeof(struct glu_version_t) == 8);
	version.major = X86_GLU_RUNTIME_VERSION_MAJOR;
	version.minor = X86_GLU_RUNTIME_VERSION_MINOR;
	mem_write(mem, version_ptr, sizeof version, &version);
	glu_debug("\tGLU Runtime host implementation v. %d.%d\n", version.major, version.minor);

	/* Return success */
	return 0;
}

