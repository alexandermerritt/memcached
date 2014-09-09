#include <libmemcached/memcached.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#define SERVER "--SERVER=192.168.1.221:11211"
int main(int argc, char *argv[])
{
    memcached_st *memc;
    memcached_return_t mret;

    if (argc != 3)
        return 1;

    int numitems = atoi(argv[1]);
    int itemsize = atoi(argv[2]);

    void *buf = malloc(itemsize);
    if (!buf)
        return ENOMEM;

    memc = memcached(SERVER, strlen(SERVER));
    if (!memc)
        return 1;

    srand(time(NULL) + getpid());

    char key[64];
    while (numitems-- > 0) {
        snprintf(key, 64, "%d-%d",
                numitems, rand());
        mret = memcached_set(memc, key, strlen(key),
                buf, itemsize, 0, 0);
        if (mret != MEMCACHED_SUCCESS) {
            fprintf(stderr, ">> memcached_set: %s\n",
                    memcached_strerror(memc, mret));
            return 2;
        }
    }
    return 0;
}
