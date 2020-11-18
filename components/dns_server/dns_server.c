#include "dns_server.h"

#include <string.h>
#include <machine/endian.h>

static const char *TAG = "dns_server";

#if _BYTE_ORDER == _BIG_ENDIAN

#define htobe16(x)  (x)
#define be16toh(x)  (x)
#define htobe32(x)  (x)
#define be32toh(x)  (x)
#define htobe64(x)  (x)
#define be64toh(x)  (x)

#define htole16(x)  __builtin_bswap16(x)
#define le16toh(x)  __builtin_bswap16(x)
#define htole32(x)  __builtin_bswap32(x)
#define le32toh(x)  __builtin_bswap32(x)
#define htole64(x)  __builtin_bswap64(x)
#define le64toh(x)  __builtin_bswap64(x)

#else

#define htobe16(x)  __builtin_bswap16(x) 
#define be16toh(x)  __builtin_bswap16(x) 
#define htobe32(x)  __builtin_bswap32(x) 
#define be32toh(x)  __builtin_bswap32(x) 
#define htobe64(x)  __builtin_bswap64(x) 
#define be64toh(x)  __builtin_bswap64(x) 

#define htole16(x)  (x)
#define le16toh(x)  (x)
#define htole32(x)  (x)
#define le32toh(x)  (x)
#define htole64(x)  (x)
#define le64toh(x)  (x)

#endif

void
dns_buf_init_alloc(dns_buf_t *self, size_t alloc_length) {
    if (alloc_length < 16)
        alloc_length = 16;
    self->data = (uint8_t*)malloc(alloc_length);
    self->alloc_length = alloc_length;
    self->rdpos = self->wrpos = 0;
    self->owns_data = true;
}

void
dns_buf_init_use_data(dns_buf_t *self, uint8_t *data, size_t data_length) {
    self->data = data;
    self->wrpos = self->alloc_length = data_length;
    self->rdpos = 0;
    self->owns_data = false;
}

void
dns_buf_init_use_buffer(dns_buf_t *self, uint8_t *data, size_t data_length) {
    self->data = data;
    self->alloc_length = data_length;
    self->rdpos = self->wrpos = 0;
    self->owns_data = false;
}


void
dns_buf_destroy(dns_buf_t *self) {
    if (self->owns_data && (self->data != NULL)) {
        free(self->data);
        self->data = NULL;
    }
    self->rdpos = self->wrpos = self->alloc_length = 0;
}


void
dns_buf_log(dns_buf_t *self, const char *prefix) {
    ESP_LOGD(TAG, "%s; rdpos=0x%04x, wrpos=0x%04x, alloc_length=0x%04x, owns_data=%d", prefix, self->rdpos, self->wrpos, self->alloc_length, self->owns_data);
    if (self->wrpos > 0) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, self->data, self->wrpos, ESP_LOG_DEBUG);
    }
}

void
dns_buf_grow(dns_buf_t *self, size_t additional_bytes) {
    size_t new_pos = self->wrpos + additional_bytes;
    if (new_pos > self->alloc_length) {
        assert(self->owns_data);
        while (new_pos > self->alloc_length) {
            // NOTE: alloc_length must be >= 4 for this to work
            self->alloc_length = self->alloc_length * 33 / 25;
        }
        self->data = (uint8_t*)realloc(self->data, self->alloc_length);
        assert(self->data != NULL);
    }
}

size_t
dns_buf_available(dns_buf_t *self) {
    return self->wrpos - self->rdpos;
}


bool
dns_parse_u8(dns_buf_t *self, uint8_t *dest) {
    if (dns_buf_available(self) < 1)
        return false;

    *dest = *(self->data + self->rdpos);
    self->rdpos += 1;
    return true;
}

void
dns_write_u8(dns_buf_t *self, uint8_t src) {
    dns_buf_grow(self, 1);
    *(self->data + self->wrpos) = src;
    self->wrpos += 1;
}

void
dns_write_u8s(dns_buf_t *self, const uint8_t *src, size_t src_length) {
    dns_buf_grow(self, src_length);
    memcpy(self->data + self->wrpos, src, src_length);
    self->wrpos += src_length;
}


bool
dns_parse_u16n(dns_buf_t *self, uint16_t *dest) {
    if (dns_buf_available(self) < 2)
        return false;

    *dest = ntohs(*(uint16_t*)(self->data + self->rdpos));
    self->rdpos += 2;
    return true;
}

bool
dns_parse_u16le(dns_buf_t *self, uint16_t *dest) {
    if (dns_buf_available(self) < 2)
        return false;

    *dest = le16toh(*(uint16_t*)(self->data + self->rdpos));
    self->rdpos += 2;
    return true;
}

bool
dns_parse_u16be(dns_buf_t *self, uint16_t *dest) {
    if (dns_buf_available(self) < 2)
        return false;

    *dest = be16toh(*(uint16_t*)(self->data + self->rdpos));
    self->rdpos += 2;
    return true;
}

void
dns_write_u16n(dns_buf_t *self, uint16_t src) {
    dns_buf_grow(self, 2);
    *(uint16_t*)(self->data + self->wrpos) = htons(src);
    self->wrpos += 2;
}

void
dns_write_u16le(dns_buf_t *self, uint16_t src) {
    dns_buf_grow(self, 2);
    *(uint16_t*)(self->data + self->wrpos) = htole16(src);
    self->wrpos += 2;
}

void
dns_write_u16be(dns_buf_t *self, uint16_t src) {
    dns_buf_grow(self, 2);
    *(uint16_t*)(self->data + self->wrpos) = htobe16(src);
    self->wrpos += 2;
}


bool
dns_parse_u32n(dns_buf_t *self, uint32_t *dest) {
    if (dns_buf_available(self) < 4)
        return false;

    *dest = ntohl(*(uint32_t*)(self->data + self->rdpos));
    self->rdpos += 4;
    return true;
}

bool
dns_parse_u32le(dns_buf_t *self, uint32_t *dest) {
    if (dns_buf_available(self) < 4)
        return false;

    *dest = le32toh(*(uint32_t*)(self->data + self->rdpos));
    self->rdpos += 4;
    return true;
}

bool
dns_parse_u32be(dns_buf_t *self, uint32_t *dest) {
    if (dns_buf_available(self) < 4)
        return false;

    *dest = be32toh(*(uint32_t*)(self->data + self->rdpos));
    self->rdpos += 4;
    return true;
}

void
dns_write_u32n(dns_buf_t *self, uint32_t src) {
    dns_buf_grow(self, 4);
    *(uint32_t*)(self->data + self->wrpos) = htonl(src);
    self->wrpos += 4;
}

void
dns_write_u32le(dns_buf_t *self, uint32_t src) {
    dns_buf_grow(self, 4);
    *(uint32_t*)(self->data + self->wrpos) = htole32(src);
    self->wrpos += 4;
}

void
dns_write_u32be(dns_buf_t *self, uint32_t src) {
    dns_buf_grow(self, 4);
    *(uint32_t*)(self->data + self->wrpos) = htobe32(src);
    self->wrpos += 4;
}




bool
dns_parse_dns_name(dns_buf_t *self, dns_buf_t* dest) {
    while (true) {
        uint8_t label_len;
        uint16_t label_pos = self->rdpos;

        if (!dns_parse_u8(self, &label_len))
            return false;
        if (label_len == 0)
            return true;

        switch (label_len & 0xc0) {
            case 0x80:
            case 0x40:
            return false;

            case 0xc0: {
                uint16_t backref_pos = ((uint16_t)(label_len & 0x3f)) << 8;
                if (!dns_parse_u8(self, &label_len))
                    return false;
                backref_pos += label_len;
                if (backref_pos >= label_pos)
                    return false; // endless loops, huh?
                uint16_t in_pos_before = self->rdpos;
                self->rdpos = backref_pos;
                bool result = dns_parse_dns_name(self, dest);
                self->rdpos = in_pos_before;
                return result;
            }

            case 0x00:
            if (self->rdpos + label_len > self->wrpos)
                return false;
            dns_write_u8s(dest, self->data + self->rdpos, label_len);
            dns_write_u8(dest, '.');
            self->rdpos += label_len;
            break;
        }
    }
}


void
dns_write_dns_name(dns_buf_t* self, const char *src) {
    // TODO: compress
    while (*src) {
        const char *dot_pos = strchrnul(src, '.');
        size_t label_length = dot_pos - src;
        assert(label_length < 64);
        dns_write_u8(self, label_length);
        dns_write_u8s(self, (const uint8_t*)src, label_length);
        if (*dot_pos == '\0') {
            break;
        }
        src = dot_pos + 1;
    }
    dns_write_u8(self, 0);
}


static void
dns_server_task(void *pvParameters) {
    dns_policy_t fn = (dns_policy_t)pvParameters;
    uint8_t rx_buffer[512];
    dns_buf_t in;
    dns_buf_t out;

    ESP_LOGI(TAG, "DNS server task; fn=%p", fn);
    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(53);

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created;");

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind; errno=%d", errno);
        }
        ESP_LOGI(TAG, "Socket bound;");

        while (1) {
            ESP_LOGI(TAG, "Waiting for data;");
            struct sockaddr_in source_addr; // Large enough for both IPv4 or IPv6

            socklen_t socklen = sizeof(source_addr);
            ssize_t rx_length = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);
            if (rx_length < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            ESP_LOGI(TAG, "Received %d bytes from 0x%08x:", rx_length, ((struct sockaddr_in *)&source_addr)->sin_addr.s_addr);

            dns_buf_init_use_data(&in, rx_buffer, rx_length);
            dns_buf_log(&in, "Input");
            dns_buf_init_alloc(&out, in.wrpos); // estimate the output size to be the same as the input


            uint16_t id = 0;
            dns_opcode_t opcode = DNS_OPCODE_QUERY;
            dns_retcode_t retcode = DNS_RETCODE_NO_ERROR;
            uint16_t flags;
            uint16_t num_questions;
            uint16_t num_answer_rrs;
            uint16_t num_authority_rrs;
            uint16_t num_additional_rrs;

            do {
                if (!dns_parse_u16n(&in, &id) || !dns_parse_u16n(&in, &flags)) {
                    retcode = DNS_RETCODE_FORMAT_ERROR;
                    break;
                }
                opcode = flags & DNS_FLAG_OPCODE_MASK;
                if ((opcode != DNS_OPCODE_QUERY) && (opcode != DNS_OPCODE_IQUERY)) {
                    retcode = DNS_RETCODE_NOT_IMPLEMENTED;
                    break;
                }

                if (   (flags & DNS_FLAG_IS_RESPONSE)
                    || !dns_parse_u16n(&in, &num_questions)
                    || (num_questions == 0)
                    || !dns_parse_u16n(&in, &num_answer_rrs)
                    || (num_answer_rrs != 0)
                    || !dns_parse_u16n(&in, &num_authority_rrs)
                    || (num_authority_rrs != 0)
                    || !dns_parse_u16n(&in, &num_additional_rrs)
                   ) {
                    retcode = DNS_RETCODE_FORMAT_ERROR;
                    break;
                }
                ESP_LOGD(TAG, "Parsed req header; id=0x%04x, flags=0x%04x, opcode=%d, num_questions=%d, num_additional_rrs=%d", id, flags, opcode, num_questions, num_additional_rrs);

                // we need to write a header as the compression of the answers may need it
                dns_write_u16n(&out, id);
                dns_write_u16n(&out, DNS_FLAG_IS_RESPONSE | opcode | DNS_RETCODE_NO_ERROR);
                dns_write_u16n(&out, 0);
                dns_write_u16n(&out, 0); // num_answers fixup will be injected later
                dns_write_u16n(&out, 0);
                dns_write_u16n(&out, 0);
                
                if (fn) {
                    dns_buf_t name;
                    dns_buf_init_alloc(&name, 256);
                    for (int i = 0; (i < num_questions) && (retcode == DNS_RETCODE_NO_ERROR); ++i) {
                        uint16_t type;
                        uint16_t _class;

                        name.wrpos = 0;

                        if (!dns_parse_dns_name(&in, &name) || !dns_parse_u16n(&in, &type) || !dns_parse_u16n(&in, &_class)) {
                            retcode = DNS_RETCODE_FORMAT_ERROR;
                            break;
                        }
                        dns_write_u8(&name, 0); // name.data is asciiz now
                        ESP_LOGD(TAG, "Parsed question; idx=%d, name='%s', type=%d, class=%d", i, name.data, type, _class);

                        // mark position in case the policy doesn't want to send an answer
                        size_t out_pos_before = out.wrpos;

                        // write the rr header
                        dns_write_dns_name(&out, (const char*)name.data);
                        dns_write_u16n(&out, type);
                        dns_write_u16n(&out, _class);
                        size_t ttl_pos = out.wrpos;
                        dns_write_u32n(&out, 0);    // TTL placeholder
                        size_t rdlength_pos = out.wrpos;
                        dns_write_u16n(&out, 0);    // rdlength placeholder
                        size_t rdata_pos = out.wrpos;

                        uint32_t ttl = 0;
                        // the policy fn shall
                        // - either set ttl, write rdata and return true
                        // - or return false (and may write any junk, it'll be discarded)
                        if (fn(&out, (const char*)name.data, type, _class, &ttl)) {
                            ++num_answer_rrs;
                            ESP_LOGD(TAG, "Policy wrote answer; num_answer_rrs=%d", num_answer_rrs);
                            *(uint32_t*)(out.data + ttl_pos) = htonl(ttl);
                            *(uint16_t*)(out.data + rdlength_pos) = htons(out.wrpos - rdata_pos);
                        }
                        else {
                            out.wrpos = out_pos_before;
                            ESP_LOGD(TAG, "Policy skipped the question;");
                        }
                    }
                    dns_buf_destroy(&name);
                    if (retcode != DNS_RETCODE_NO_ERROR)
                        break;
                    if (in.rdpos != in.wrpos) {
                        retcode = DNS_RETCODE_FORMAT_ERROR;
                        break;
                    }
                }

            } while (0);

            if (retcode != DNS_RETCODE_NO_ERROR) {
                ESP_LOGE(TAG, "Sending error; retcode=%d", retcode);
                out.wrpos = 0;
                dns_write_u16n(&out, id);
                dns_write_u16n(&out, DNS_FLAG_IS_RESPONSE | opcode | DNS_FLAG_IS_AUTHORITATIVE | retcode);
                dns_write_u16n(&out, 0);
                dns_write_u16n(&out, 0);
                dns_write_u16n(&out, 0);
                dns_write_u16n(&out, 0);
            }
            else if (out.wrpos >= 8) {
                *(uint16_t*)(out.data + 6) = htons(num_answer_rrs);
            }

            ESP_LOGD(TAG, "Sending response; len=%d", out.wrpos);
            dns_buf_log(&out, "Output");
            int err = sendto(sock, out.data, out.wrpos, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                //break;
            }
            dns_buf_destroy(&out);

        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

esp_err_t
dns_server_start(dns_policy_t fn) {
    TaskHandle_t dns_task;
    if (xTaskCreate(dns_server_task, TAG, 4096, (void*)fn, 5, &dns_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task; name='%s'", TAG);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// vim: set sw=4 ts=4 indk= et si:
