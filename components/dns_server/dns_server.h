#ifndef DNS_SERVER_H
#define DNS_SERVER_H

#include <esp_system.h>
#include <esp_log.h>

#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

typedef enum {
    DNS_FLAG_IS_RESPONSE        = 0x8000,
    DNS_FLAG_OPCODE_MASK        = 0x7800,
    DNS_FLAG_IS_AUTHORITATIVE   = 0x0400,
    DNS_FLAG_IS_TRUNCATED       = 0x0200,
    DNS_FLAG_WANT_RECURSION     = 0x0100,
    DNS_FLAG_CAN_RECURSION      = 0x0080,
    DNS_FLAG_IS_AUTHENTICATED   = 0x0020,
    DNS_FLAG_CHECKING_DISABLED  = 0x0010,
    DNS_FLAG_RETCODE_MASK       = 0x000f,
} dns_flags_t;
#define DNS_FLAG_OPCODE_SHIFT   11
#define DNS_FLAG_RETCODE_SHIFT  0


typedef enum {
    DNS_OPCODE_QUERY    = 0 << DNS_FLAG_OPCODE_SHIFT,
    DNS_OPCODE_IQUERY   = 1 << DNS_FLAG_OPCODE_SHIFT,
    DNS_OPCODE_STATUS   = 2 << DNS_FLAG_OPCODE_SHIFT,
    DNS_OPCODE_NOTIFY   = 4 << DNS_FLAG_OPCODE_SHIFT,
    DNS_OPCODE_UPDATE   = 5 << DNS_FLAG_OPCODE_SHIFT,
} dns_opcode_t;


typedef enum {
    DNS_RETCODE_NO_ERROR        = 0 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_FORMAT_ERROR    = 1 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_SERVER_FAILURE  = 2 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_NAME_ERROR      = 3 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_NOT_IMPLEMENTED = 4 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_REFUSED         = 5 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_YXDOMAIN        = 6 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_YXRRSET         = 7 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_NXRRSET         = 8 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_NOT_AUTH        = 9 << DNS_FLAG_RETCODE_SHIFT,
    DNS_RETCODE_NOT_ZONE        = 10 << DNS_FLAG_RETCODE_SHIFT,
} dns_retcode_t;


typedef enum {
    DNS_TYPE_A          = 1,
    DNS_TYPE_NS         = 2,
    DNS_TYPE_MD         = 3,
    DNS_TYPE_MF         = 4,
    DNS_TYPE_CNAME      = 5,
    DNS_TYPE_SOA        = 6,
    DNS_TYPE_MB         = 7,
    DNS_TYPE_MG         = 8,
    DNS_TYPE_MR         = 9,
    DNS_TYPE_NULL       = 10,
    DNS_TYPE_WKS        = 11,
    DNS_TYPE_PTR        = 12,
    DNS_TYPE_HINFO      = 13,
    DNS_TYPE_MINFO      = 14,
    DNS_TYPE_MX         = 15,
    DNS_TYPE_TXT        = 16,
    DNS_TYPE_RP         = 17,
    DNS_TYPE_AFSDB      = 18,
    DNS_TYPE_X25        = 19,
    DNS_TYPE_ISDN       = 20,
    DNS_TYPE_RT         = 21,
    DNS_TYPE_NSAP       = 22,
    DNS_TYPE_NSAP_PTR   = 23,
    DNS_TYPE_SIG        = 24,
    DNS_TYPE_KEY        = 25,
    DNS_TYPE_PX         = 26,
    DNS_TYPE_GPOS       = 27,
    DNS_TYPE_AAAA       = 28,
    DNS_TYPE_LOC        = 29,
    DNS_TYPE_NXT        = 30,
    DNS_TYPE_EID        = 31,
    DNS_TYPE_NB         = 32,
    DNS_TYPE_SRV        = 33,
    DNS_TYPE_ATMA       = 34,
    DNS_TYPE_NAPTR      = 35,
    DNS_TYPE_KX         = 36,
    DNS_TYPE_CERT       = 37,
    DNS_TYPE_A6         = 38,
    DNS_TYPE_DNAME      = 39,
    DNS_TYPE_SINK       = 40,
    DNS_TYPE_OPT        = 41,
    DNS_TYPE_APL        = 42,
    DNS_TYPE_DS         = 43,
    DNS_TYPE_SSHFP      = 44,
    DNS_TYPE_IPSECKEY   = 45,
    DNS_TYPE_RRSIG      = 46,
    DNS_TYPE_NSEC       = 47,
    DNS_TYPE_DNSKEY     = 48,
    DNS_TYPE_DHCID      = 49,
    DNS_TYPE_NSEC3      = 50,
    DNS_TYPE_NSEC3PARAM = 51,
    DNS_TYPE_TLSA       = 52,
    DNS_TYPE_HIP        = 55,
    DNS_TYPE_NINFO      = 56,
    DNS_TYPE_RKEY       = 57,
    DNS_TYPE_TALINK     = 58,
    DNS_TYPE_CHILD_DS   = 59,
    DNS_TYPE_SPF        = 99,
    DNS_TYPE_UINFO      = 100,
    DNS_TYPE_UID        = 101,
    DNS_TYPE_GID        = 102,
    DNS_TYPE_UNSPEC     = 103,
    DNS_TYPE_TKEY       = 249,
    DNS_TYPE_TSIG       = 250,
    DNS_TYPE_IXFR       = 251,
    DNS_TYPE_AXFR       = 252,
    DNS_TYPE_MAILB      = 253,
    DNS_TYPE_MAILA      = 254,
    DNS_TYPE_STAR       = 255,
    DNS_TYPE_URI        = 256,
    DNS_TYPE_CAA        = 257,
    DNS_TYPE_DNSSEC_TA  = 32768,
    DNS_TYPE_DNSSEC_LV  = 32769,
} dns_type_t;


typedef enum {
    DNS_CLASS_IN        = 1,
    DNS_CLASS_CH        = 3,
    DNS_CLASS_HS        = 4,
    DNS_CLASS_NONE      = 254,
    DNS_CLASS_ANY       = 255,
} dns_class_t;


typedef struct {
    uint8_t *data;
    size_t rdpos;
    size_t wrpos;
    size_t alloc_length;
    bool owns_data;
} dns_buf_t;

void dns_buf_init_alloc(dns_buf_t *self, size_t alloc_length);
void dns_buf_init_use_data(dns_buf_t *self, uint8_t *data, size_t data_length);
void dns_buf_init_use_buffer(dns_buf_t *self, uint8_t *data, size_t data_length);
void dns_buf_log(dns_buf_t *self, const char *prefix);
void dns_buf_grow(dns_buf_t *self, size_t additional_bytes);
size_t dns_buf_available(dns_buf_t *self);
void dns_buf_destroy(dns_buf_t *self);

bool dns_parse_u8(dns_buf_t *self, uint8_t *dest);
void dns_write_u8(dns_buf_t *self, uint8_t src);
void dns_write_u8s(dns_buf_t *self, const uint8_t *src, size_t src_length);

bool dns_parse_u16n(dns_buf_t *self, uint16_t *dest);
bool dns_parse_u16le(dns_buf_t *self, uint16_t *dest);
bool dns_parse_u16be(dns_buf_t *self, uint16_t *dest);
void dns_write_u16n(dns_buf_t *self, uint16_t src);
void dns_write_u16le(dns_buf_t *self, uint16_t src);
void dns_write_u16be(dns_buf_t *self, uint16_t src);

bool dns_parse_u32n(dns_buf_t *self, uint32_t *dest);
bool dns_parse_u32le(dns_buf_t *self, uint32_t *dest);
bool dns_parse_u32be(dns_buf_t *self, uint32_t *dest);
void dns_write_u32n(dns_buf_t *self, uint32_t src);
void dns_write_u32le(dns_buf_t *self, uint32_t src);
void dns_write_u32be(dns_buf_t *self, uint32_t src);

bool dns_parse_dns_name(dns_buf_t *self, dns_buf_t* dest);
void dns_write_dns_name(dns_buf_t* self, const char *src);


typedef bool (*dns_policy_t)(dns_buf_t *out, const char *name, dns_type_t type, dns_class_t _class, uint32_t *ttl);

esp_err_t dns_server_start(dns_policy_t fn);


#endif // DNS_SERVER_H
// vim: set sw=4 ts=4 indk= et si:
