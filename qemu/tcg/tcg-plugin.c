#include <stdbool.h> /* bool, true, false, */
#include <assert.h>  /* assert(3), */
#include <dlfcn.h>   /* dlopen(3), dlsym(3), */
#include <unistd.h>  /* access(2), STDERR_FILENO, getpid(2), */
#include <fcntl.h>   /* open(2), */
#include <stdlib.h>  /* getenv(3), */
#include <string.h>  /* strlen(3), */
#include <stdio.h>   /* *printf(3), memset(3), */
#include <pthread.h> /* pthread_*, */

#include "tcg-op.h"

#include "tcg-plugin.h"
#include "exec/exec-all.h"   /* TranslationBlock */
#include "qom/cpu.h"         /* CPUState */
#include "sysemu/sysemu.h"   /* max_cpus */

/* Interface for the TCG plugin.  */
static TCGPluginInterface tpi;

static bool mutex_protected;

static uint64_t current_pc = 0;

static uint64_t *icount_total;

/* Ensure resources used by *_helper_code are protected from
   concurrent access.  */
static pthread_mutex_t helper_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Avoid "recursive" instrumentation.  */
static bool in_gen_tpi_helper = false;

static TCGArg *tb_info;
static TCGArg *tb_data1;
static TCGArg *tb_data2;

static int init=0;

/* Wrapper to ensure only non-generic plugins can access non-generic data.  */
#define TPI_CALLBACK_NOT_GENERIC(callback, ...) \
    do {                                        \
        if (!tpi.is_generic) {                  \
            tpi.env = env;                      \
            tpi.tb = tb;                        \
        }                                       \
        tpi.callback(&tpi, ##__VA_ARGS__);      \
        tpi.env = NULL;                         \
        tpi.tb = NULL;                          \
    } while (0);


static void cpus_stopped(const TCGPluginInterface *tpi)
{
    unsigned int i;
    for (i = 0; i < tpi->nb_cpus; i++) {
        printf("number of executed instructions on CPU #%d = %" PRIu64 "\n",
                 i, icount_total[i]);
    }
}


static void pre_tb_helper_code(const TCGPluginInterface *tpi,
                               TPIHelperInfo info, uint64_t address,
                               uint64_t data1, uint64_t data2)
{
    icount_total[info.cpu_index] += info.icount;
}

/* Hook called once all CPUs are stopped/paused.  */
void tcg_plugin_cpus_stopped(void)
{
    tpi.cpus_stopped = cpus_stopped;
    if (tpi.cpus_stopped) {
	tpi.cpus_stopped(&tpi);
    }
}


/* Hook called before the Intermediate Code Generation (ICG).  */
void tcg_plugin_before_gen_tb(CPUState *env, TranslationBlock *tb)
{
    if(init == 0)
    {
	TPI_INIT_VERSION_GENERIC(tpi);
    	tpi.pre_tb_helper_code = pre_tb_helper_code;
    	tpi.cpus_stopped = cpus_stopped;
	tpi.nb_cpus = 1;

    	icount_total = g_malloc0(tpi.nb_cpus * sizeof(uint64_t));    	
    	init++;
    }

    assert(!in_gen_tpi_helper);
    in_gen_tpi_helper = true;

    if (tpi.before_gen_tb) {
        TPI_CALLBACK_NOT_GENERIC(before_gen_tb);
    }

    if (tpi.pre_tb_helper_code) {
    /* Generate TCG opcodes to call helper_tcg_plugin_tb*().  */
        TCGv_i64 data1;
        TCGv_i64 data2;

        TCGv_i64 address = tcg_const_i64((uint64_t)tb->pc);

        /* Patched in tcg_plugin_after_gen_tb().  */
        tb_info = tcg_ctx.gen_opparam_ptr + 1;
        TCGv_i64 info = tcg_const_i64(0);

        /* Patched in tcg_plugin_after_gen_tb().  */
        tb_data1 = tcg_ctx.gen_opparam_ptr + 1;
        data1 = tcg_const_i64(0);

        /* Patched in tcg_plugin_after_gen_tb().  */
        tb_data2 = tcg_ctx.gen_opparam_ptr + 1;
        data2 = tcg_const_i64(0);

        gen_helper_tcg_plugin_pre_tb(address, info, data1, data2);

        tcg_temp_free_i64(data2);
        tcg_temp_free_i64(data1);
        tcg_temp_free_i64(info);
        tcg_temp_free_i64(address);
    }

    in_gen_tpi_helper = false;
}

/* Hook called after the Intermediate Code Generation (ICG).  */
void tcg_plugin_after_gen_tb(CPUState *env, TranslationBlock *tb)
{
    assert(!in_gen_tpi_helper);
    in_gen_tpi_helper = true;

    if (tpi.pre_tb_helper_code) {
        /* Patch helper_tcg_plugin_tb*() parameters.  */
        ((TPIHelperInfo *)tb_info)->cpu_index = env->cpu_index;
        ((TPIHelperInfo *)tb_info)->size = tb->size;
#if TCG_TARGET_REG_BITS == 64
        ((TPIHelperInfo *)tb_info)->icount = tb->icount;
#else
        /* i64 variables use 2 arguments on 32-bit host.  */
        *(tb_info + 2) = tb->icount;
#endif

        /* Callback variables have to be initialized [when not used]
         * to ensure deterministic code generation, e.g. on some host
         * the opcode "movi_i64 tmp,$value" isn't encoded the same
         * whether $value fits into a given host instruction or
         * not.  */
        uint64_t data1 = 0;
        uint64_t data2 = 0;

        if (tpi.pre_tb_helper_data) {
            TPI_CALLBACK_NOT_GENERIC(pre_tb_helper_data, *(TPIHelperInfo *)tb_info, tb->pc, &data1, &data2);
        }

#if TCG_TARGET_REG_BITS == 64
        *(uint64_t *)tb_data1 = data1;
        *(uint64_t *)tb_data2 = data2;
#else
        /* i64 variables use 2 arguments on 32-bit host.  */
        *tb_data1 = data1 & 0xFFFFFFFF;
        *(tb_data1 + 2) = data1 >> 32;

        *tb_data2 = data2 & 0xFFFFFFFF;
        *(tb_data2 + 2) = data2 >> 32;
#endif
    }
    if (tpi.after_gen_tb) {
        TPI_CALLBACK_NOT_GENERIC(after_gen_tb);
    }
    in_gen_tpi_helper = false;
}

/* Hook called each time a TCG opcode is generated.  */
void tcg_plugin_after_gen_opc(uint16_t *opcode, TCGArg *opargs, uint8_t nb_args)
{
    TPIOpCode tpi_opcode;
    in_gen_tpi_helper = true;

    nb_args = MIN(nb_args, TPI_MAX_OP_ARGS);

    tpi_opcode.pc   = current_pc;
    tpi_opcode.nb_args = nb_args;

    tpi_opcode.opcode = opcode;
    tpi_opcode.opargs = opargs;

    if (tpi.after_gen_opc) {
        tpi.after_gen_opc(&tpi, &tpi_opcode);
    }
    in_gen_tpi_helper = false;
}

/* TCG helper used to call pre_tb_helper_code() in a thread-safe
 * way.  */
void helper_tcg_plugin_pre_tb(uint64_t address, uint64_t info,
                              uint64_t data1, uint64_t data2)
{
    int error;

    if (mutex_protected) {
        error = pthread_mutex_lock(&helper_mutex);
        if (error) {
            fprintf(stderr, "plugin: in call_pre_tb_helper_code(), "
                    "pthread_mutex_lock() has failed: %s\n",
                    strerror(error));
            goto end;
        }
    }
    tpi.pre_tb_helper_code(&tpi, *(TPIHelperInfo *)&info, address, data1, data2);

end:
    if (mutex_protected) {
        pthread_mutex_unlock(&helper_mutex);
    }
}
