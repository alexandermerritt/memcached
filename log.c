/**
 * log.c
 *
 * Each thread that handles events registers itself with the logger and receives
 * its own log structure. Using the pthread API we enable threads to reference
 * its state without using locks. "Registration" allows for aggregation of logs
 * when issued the "dump" command to a memcached server.
 *
 * Function process_command() demultiplexes strings received from a client.
 * Refer to this to find where logging happens.
 *
 * XXX Not all commands are logged. See LOG_OP and look in code for where
 * log_next() is invoked.
 */

#if defined(_GNU_SOURCE)
#else
#define _GNU_SOURCE 1
#endif

#include "log.h"
#include "memcached.h"
#include <stdlib.h>
#include <assert.h>
#include <sys/syscall.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>

pthread_key_t  log_key;
pthread_once_t log_once = PTHREAD_ONCE_INIT;

log_t *log_registered[LOG_MAX_REGISTERED];
static pthread_mutex_t registered_lock = PTHREAD_MUTEX_INITIALIZER;

const char *log_op_strings[OP_MAX] = {
    [OP_INVALID] = "invalid",
    [OP_GET] = "get",
    [OP_UPDATE] = "update",
    [OP_DELETE] = "delete",
};

static inline void set_now(log_e *entry)
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    entry->ns = t.tv_sec * 1e9 + t.tv_nsec;
}

static inline void do_register(void)
{
    int i;
    log_t *log = log_get();
    if (unlikely(!log))
        abort();
    if (likely(log->registered))
        return;
    for (i = 0; i < LOG_MAX_REGISTERED; i++)
        if (!log_registered[i])
            break;
    if (unlikely(i >= LOG_MAX_REGISTERED))
        abort();
    log_registered[i] = log;
    log->registered = true;
    log->tid = syscall(SYS_gettid);
}

void log_register(void)
{
    pthread_mutex_lock(&registered_lock);
    do_register();
    pthread_mutex_unlock(&registered_lock);
}

void log_makekey(void)
{
    pthread_key_create(&log_key, NULL);
}

// acquire log specific to thread
log_t *log_get(void)
{
    log_t *log = pthread_getspecific(log_key);
    if (unlikely(!log)) {
        log = calloc(1, sizeof(*log));
        if (!log)
            return NULL;
        log->slabs[0] = malloc(LOG_SLAB_SIZE);
        if (unlikely(!log->slabs[0]))
            return NULL;
        pthread_setspecific(log_key, log);
    }
    return log;
}

// return next empty log entry for use;
// lazy allocation of additional slab
log_e *log_getnext(void)
{
    log_e *entry;
    log_t *log = log_get();
    if (unlikely(!log))
        abort(); // fubar
    if (unlikely(log->next >= LOG_SLAB_ENTRIES)) {
        if (unlikely(log->slab >= (LOG_SLABS - 1)))
            abort(); // recompile to increase slabs
        log->slabs[++(log->slab)] = malloc(LOG_SLAB_SIZE);
        if (unlikely(!log->slabs[log->slab]))
            abort(); // OOM
        log->next = 0;
    }
    entry = &log->slabs[log->slab][log->next++];
    set_now(entry);
    return entry;
}

void log_resetall(void)
{
    log_t *log;
    for (int i = 0; i < LOG_MAX_REGISTERED; i++)
        if ((log = log_registered[i]))
            log->slab = log->next = 0;
}

static void getaddr(const struct sockaddr_storage *saddr,
        char *out_ip, size_t iplen, int *out_port)
{
    if (!out_ip || iplen < 1 || !out_port)
        abort();
    // getpeername was already invoked by memcached in conn_new
    if (saddr->ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)saddr;
        *out_port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, out_ip, iplen);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)saddr;
        *out_port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, out_ip, iplen);
    }
    out_ip[iplen] = '\0';
}

int log_dumpto(FILE *fp)
{
    char line[512];
    size_t len;
    int i;

    if (!fp)
        return -1;

    const char entry_fmt[] = "%d %s %d %lu %s %s %lu\n";
    char ipstr[16 + 1]; // ipv6 is 128 bits, one for nul
    int port;

    snprintf(line, sizeof(line), "==TRACE==\ntid ip:port time_ns op key len\n");
    len = strlen(line);
    if (len > fwrite(line, sizeof(*line), len, fp))
        return -1;

    // sort the logs yourself
    for (i = 0; i < LOG_MAX_REGISTERED; i++) {
        log_t *log = log_registered[i];
        if (!log) // last entry
            break;
        size_t slab, e;
        // dump out all full slabs first
        for (slab = 0; slab < log->slab; slab++) {
            for (e = 0; e < LOG_SLAB_ENTRIES; e++) {
                const log_e *entry = &log->slabs[slab][e];
                getaddr(&entry->saddr, ipstr, sizeof(ipstr), &port);
                snprintf(line, sizeof(line),
                        entry_fmt, log->tid, ipstr, port,
                        entry->ns, log_op_strings[entry->op],
                        entry->key, entry->len);
                len = strlen(line);
                if (len > fwrite(line, sizeof(*line), len, fp))
                    return -1;
                if (feof(fp) || ferror(fp))
                    return -1;
            }
        }
        // dump last slab
        slab = log->slab;
        for (e = 0; e < log->next; e++) {
            const log_e *entry = &log->slabs[slab][e];
            getaddr(&entry->saddr, ipstr, sizeof(ipstr), &port);
            snprintf(line, sizeof(line),
                    entry_fmt, log->tid, ipstr, port,
                    entry->ns, log_op_strings[entry->op],
                    entry->key, entry->len);
            len = strlen(line);
            if (len > fwrite(line, sizeof(*line), len, fp))
                return -1;
            if (feof(fp) || ferror(fp))
                return -1;
        }
    }
    return 0;
}

