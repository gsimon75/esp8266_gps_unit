#include "line_reader.h"

#include <stdlib.h>
#include <string.h>


line_reader_t *
lr_new(size_t bufsize, lr_source_t source, void *source_arg) {
    line_reader_t *self = (line_reader_t*)malloc(sizeof(line_reader_t) + bufsize);
    self->source = source;
    self->source_arg = source_arg;
    self->bufsize = bufsize;
    self->content_length = 0;
    self->content_remaining = 0;
    self->rdpos = self->wrpos = self->buf;
    return self;
}


void
lr_free(line_reader_t *self) {
    free(self);
}


static size_t
read_some(line_reader_t *self) {
    return self->source(self->source_arg, self->wrpos, self->buf + self->bufsize - self->wrpos);
}

unsigned char *
lr_read_line(line_reader_t *self) {
    unsigned char *eol;

    unsigned char *
    terminate_line(void) { // helper: terminate a line, move rdpos over it, and return its start
        eol[((self->buf < eol) && (eol[-1] == '\r')) ? -1 : 0] = '\0'; // replace (cr)lf with nul
        unsigned char *result = self->rdpos;
        self->content_remaining -= eol + 1 - self->rdpos;
        self->rdpos = eol + 1;
        return result;
    }

    if (self->rdpos != self->wrpos) { // there is unread data in the buffer
        eol = (unsigned char*)memchr(self->rdpos, '\n', self->wrpos - self->rdpos);
        if (eol) { // it contains a terminated line
            return terminate_line();
        }
        // no eol in the buffer, we'll need to read some more data, so let's make space for that
        if (self->rdpos != self->buf) {
            memmove(self->buf, self->rdpos, self->wrpos - self->rdpos);
            self->wrpos -= (self->rdpos - self->buf);
            self->rdpos = self->buf;
        }
    }
    else { // buffer is empty, so make it empty at the start position
        self->rdpos = self->wrpos = self->buf;
    }

    while (self->wrpos < (self->buf + self->bufsize)) {
        ssize_t ret = read_some(self);
        if (ret < 0) { // error
            return NULL;
        }
        if (ret <= 0) { // eof before eol
            return NULL;
        }
        eol = (unsigned char*)memchr(self->wrpos, '\n', ret); // search only in the data we read now
        self->wrpos += ret; // move wrpos over this chunk
        if (eol) {
            return terminate_line();
        }
    }
    return NULL; // error: haven't returned yet -> buffer was too short for a line
}


bool
lr_read_chunk(line_reader_t *self, unsigned char **data, size_t *datalen) {
    if (self->content_remaining == 0) {
        return false;
    }
    if (self->rdpos < self->wrpos) {
        *data = self->rdpos;
        *datalen = self->wrpos - self->rdpos;
        if (*datalen > self->content_remaining) {
            *datalen = self->content_remaining;
        }
        self->content_remaining -= *datalen;
        self->rdpos = self->wrpos = self->buf;
        return true;
    }

    ssize_t ret = read_some(self);
    if (ret <= 0) { // eof or error
        return false;
    }
 
    *data = self->wrpos;
    *datalen = ret;
    self->content_remaining -= ret;
    return true;
}

