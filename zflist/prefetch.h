#ifndef ZFLIST_PREFETCH_H
    #define ZFLIST_PREFETCH_H

    typedef struct thworker_t {
        pthread_t thread;
        int notify;
        void *userptr;

    } thworker_t;

    typedef struct workers_t {
        thworker_t *workers;
        size_t size;

    } workers_t;

    typedef struct fetch_t {
        char *filename;

    } fetch_t;

    typedef struct fetchlist_t {
        fetch_t **list;
        size_t size;
        size_t used;
        size_t next;

    } fetchlist_t;

    int zf_prefetcher(zf_callback_t *cb, char *rootdir);
#endif
