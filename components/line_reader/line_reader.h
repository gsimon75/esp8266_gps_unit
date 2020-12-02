#ifndef LINE_READER_H
#define LINE_READER_H

#include <sys/types.h>
#include <stdbool.h>

typedef ssize_t (*lr_source_t)(void*, unsigned char*, size_t);

typedef struct {
    lr_source_t source;
    void *source_arg;
    size_t bufsize, content_length, content_remaining;
    unsigned char *rdpos, *wrpos;
    unsigned char buf[1];
} line_reader_t;

line_reader_t * lr_new(size_t bufsize, lr_source_t source, void *source_arg);
void lr_free(line_reader_t *self);
unsigned char * lr_read_line(line_reader_t *self);
bool lr_read_chunk(line_reader_t *self, unsigned char **data, size_t *datalen);

#endif // LINE_READER_H
// vim: set sw=4 ts=4 indk= et si:
