#ifndef ARCHIVE_H
    #define ARCHIVE_H

    char *archive_extract(char *filename, char *target);
    int archive_create(char *filename, char *source);
#endif
