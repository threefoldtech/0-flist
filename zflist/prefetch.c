#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <poll.h>
#include "libflist.h"
#include "zflist.h"
#include "prefetch.h"
#include "tools.h"

static int fetch_append(fetchlist_t *fetchlist, char *filename) {
    if(fetchlist->used == fetchlist->size) {
        fetchlist->size += 1024;

        if(!(fetchlist->list = realloc(fetchlist->list, sizeof(fetch_t *) * fetchlist->size)))
            return 1;
    }

    fetch_t *item;
    if(!(item = malloc(sizeof(fetch_t))))
        return 1;

    item->filename = strdup(filename);

    fetchlist->list[fetchlist->used] = item;
    fetchlist->used += 1;

    return 0;
}

static void *readfile(void *userptr) {
    thworker_t *worker = (thworker_t *) userptr;
    char buffer[2048];
    ssize_t valid, found;
    char *filename = (char *) worker->userptr;
    uint64_t value = 1;
    int fd;

    // printf("checking: %s\n", filename);

    if((fd = open(filename, O_RDONLY)) < 0) {
        perror(filename);
        goto next;
    }

    valid = 0;
    // off_t length = lseek(fd, 0, SEEK_END);
    // lseek(fd, 0, SEEK_SET);

    while((found = read(fd, buffer, sizeof(buffer))) > 0) {
        valid += found;
    }

    if(found < 0)
        perror(filename);

    /*
    if(valid != length)
        printf("file: %s: NOK\n", filename);
    */

    close(fd);

next:
    if(write(worker->notify, &value, sizeof(value)) != sizeof(value))
        return NULL;

    return userptr;
}

static int listdir(const char *name, fetchlist_t *fetchlist) {
    struct dirent *entry;
    DIR *dir;

    if(!(dir = opendir(name)))
        return 0;

    while((entry = readdir(dir))) {
        char *path;

        if(asprintf(&path, "%s/%s", name, entry->d_name) < 0)
            return 1;

        if(entry->d_type == DT_DIR) {
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            // printf("Dir: %s\n", path);
            if(listdir(path, fetchlist) == 1)
                return 1;

        } else {
            // skipping non regular files
            // only regular files have payload on the backend
            if(entry->d_type != DT_REG)
                continue;

            // printf("File: %s\n", path);
            if(fetch_append(fetchlist, path) == 1)
                return 1;
        }
    }

    closedir(dir);

    return 0;
}

static thworker_t *thworker_find(workers_t *workers, int fd) {
    for(size_t i = 0; i < workers->size; i++)
        if(workers->workers[i].notify == fd)
            return &workers->workers[i];

    return NULL;
}

int zf_prefetcher(zf_callback_t *cb, char *rootdir) {
    //
    // first, build the files list
    //

    fetchlist_t *fetchlist = NULL;

    if(!(fetchlist = calloc(sizeof(fetchlist_t), 1)))
        zf_diep(cb, "fetchlist: calloc");

    if(listdir(rootdir, fetchlist) == 1)
        zf_diep(cb, "listdir");

    #if 0
    for(size_t i = 0; i < fetchlist->used; i++)
        printf(">> %s\n", fetchlist->list[i]->filename);
    #endif

    // nothing to pre fetch
    if(fetchlist->used == 0)
        return 0;

    //
    // initialize fetchers threads
    //

    workers_t workers;

    // default to 16 workers
    workers.size = 16;

    if(!(workers.workers = calloc(sizeof(thworker_t), workers.size)))
        zf_diep(cb, "workers: calloc");

    struct pollfd *fds;

    if(!(fds = calloc(sizeof(struct pollfd), workers.size)))
        zf_diep(cb, "fds: calloc");

    for(size_t i = 0; i < workers.size; i++) {
        workers.workers[i].userptr = NULL;

        if((workers.workers[i].notify = eventfd(1, 0)) < 0)
            zf_diep(cb, "eventfd");

        fds[i].fd = workers.workers[i].notify;
        fds[i].events = POLLIN;
    }

    //
    // fetching files
    //

    // printf("Fetching %lu files\n", fetchlist->used);

    while(poll(fds, workers.size, -1) > 0) {
        for(size_t i = 0; i < workers.size; i++) {
            if(fetchlist->next == fetchlist->used - 1)
                break;

            if(fds[i].revents & POLLIN) {
                uint64_t value;

                if(read(fds[i].fd, &value, sizeof(value)) != sizeof(value))
                    zf_diep(cb, "eventfd: read");

                // printf("EVENT READY ON %d\n", fds[i].fd);

                thworker_t *worker = thworker_find(&workers, fds[i].fd);

                worker->userptr = fetchlist->list[fetchlist->next]->filename;
                pthread_create(&worker->thread, NULL, readfile, worker);

                fetchlist->next += 1;
            }
        }

        if(fetchlist->next == fetchlist->used - 1)
            break;
    }

    for(size_t i = 0; i < workers.size; i++)
        if(workers.workers[i].userptr)
            pthread_join(workers.workers[i].thread, NULL);

    // printf("All done\n");

    return 0;
}
