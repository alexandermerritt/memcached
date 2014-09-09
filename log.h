/**
 * log.h
 *
 * Record accesses to memc server instance, per-thread. Also see comments in log.c
 */

#ifndef MEMCACHED_LOG_H_INCLUDED
#define MEMCACHED_LOG_H_INCLUDED

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include "defines.h"

/* defines */

#define LOG_SLABS           1024
#define LOG_SLAB_ENTRIES    1024
#define LOG_SLAB_SIZE       (sizeof(struct _log_entry) * LOG_SLAB_ENTRIES)

#define LOG_MAX_REGISTERED  1024

// keep consistent with log_op_strings
enum LOG_OP
{
    OP_INVALID = 0,
    OP_GET,
    OP_UPDATE,
    OP_DELETE, // TODO
    OP_MAX
};

struct _log_entry
{
    char key[KEY_MAX_LENGTH]; // XXX keys assumed to be ASCII
    size_t ns, len;
    enum LOG_OP op;
    // TODO client issuing request to server
    // TODO request latency
} __attribute__((packed));

struct _log
{
    struct _log_entry *slabs[LOG_SLABS];
    size_t slab /* current slab */, next /* next avail entry */;
    bool registered;
    pid_t tid;
};

typedef struct _log log_t;
typedef struct _log_entry log_e;

/* globals */

extern pthread_key_t  log_key;
extern pthread_once_t log_once;

extern log_t *log_registered[LOG_MAX_REGISTERED];

extern const char *log_op_strings[OP_MAX];

/* funcs */

void log_makekey(void);
void log_register(void);

static inline void log_initlocal(void)
{
    pthread_once(&log_once, log_makekey);
    log_register();
}

log_t *log_get(void);
log_e *log_getnext(void);
void log_resetall(void);
int log_dumpto(FILE *fp);

#endif /* MEMCACHED_LOG_H_INCLUDED */

