/* Copyright (c) 2014 Guilherme Balena Versiani.
 *
 * I dedicate any and all copyright interest in this software to the
 * public domain. I make this dedication for the benefit of the public at
 * large and to the detriment of my heirs and successors. I intend this
 * dedication to be an overt act of relinquishment in perpetuity of all
 * present and future rights to this software under copyright law.
 */

#include <modules/p2p/stuns/msg.h>

#include <modules/p2p/stuns/crc32.h>
#include <modules/p2p/stuns/hmac_sha1.h>
#include <modules/p2p/stuns/md5.h>
#include <modules/p2p/stuns/sha1.h>

#include <memory.h>

/* Include these for sockaddr_in and sockaddr_in6 */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define UNUSED(x) ((void)(x))

#if !defined(__APPLE__) && !defined(_WIN32)
static uint64_t htonll(uint64_t value) {
    const int num = 42;
    if (*(char*)&num == 42) { /* test little endian */
        return (((uint64_t)htonl((uint32_t)value)) << 32) | htonl((uint32_t)(value >> 32));
    }
    else {
        return value;
    }
}

static uint64_t ntohll(uint64_t value) {
    return htonll(value);
}
#endif

static void store_padding(uint8_t* p, size_t n, uint8_t pad) {
    if ((n & 0x03) > 0) {
        memset(p, pad, 4 - (n & 0x03));
    }
}

static struct {
    int err_code;
    const char* err_msg;
} err_msg_map[] = {
    {STUN_ERROR_TRY_ALTERNATE, "Try Alternate"},
    {STUN_ERROR_BAD_REQUEST, "Bad Request"},
    {STUN_ERROR_UNAUTHORIZED, "Unauthorized"},
    {STUN_ERROR_FORBIDDEN, "Forbidden"},
    {STUN_ERROR_UNKNOWN_ATTRIBUTE, "Unknown Attribute"},
    {STUN_ERROR_ALLOCATION_MISMATCH, "Allocation Mismatch"},
    {STUN_ERROR_STALE_NONCE, "Stale Nonce"},
    {STUN_ERROR_ADDR_FAMILY_NOT_SUPP, "Address Family Not Supported"},
    {STUN_ERROR_WRONG_CREDENTIALS, "Wrong Credentials"},
    {STUN_ERROR_UNSUPP_TRANSPORT_PROTO, "Unsupported Transport Protocol"},
    {STUN_ERROR_PEER_ADD_FAMILY_MISMATCH, "Peer Address Family Mismatch"},
    {STUN_ERROR_CONNECTION_ALREADY_EXISTS, "Connection Already Exists"},
    {STUN_ERROR_CONNECTION_FAILURE, "Connection Failure"},
    {STUN_ERROR_ALLOCATION_QUOTA_REACHED, "Allocation Quota Reached"},
    {STUN_ERROR_ROLE_CONFLICT, "Role Conflict"},
    {STUN_ERROR_SERVER_ERROR, "Server Error"},
    {STUN_ERROR_INSUFFICIENT_CAPACITY, "Insufficient Capacity"},
};

static const char* method_map[] = {
    /* 0 */ NULL,
    /* 1 */ "Binding",
    /* 2 */ "SharedSecret",
    /* 3 */ "Allocate",
    /* 4 */ "Refresh",
    /* 5 */ NULL,
    /* 6 */ "Send",
    /* 7 */ "Data",
    /* 8 */ "CreatePermission",
    /* 9 */ "ChannelBind",
    /* A */ "Connect",
    /* B */ "ConnectionBind",
    /* C */ "ConnectionAttempt",
};

const char* stun_err_reason(int err_code) {
    size_t i;
    const char* msg = NULL;
    for (i = 0; i < sizeof(err_msg_map) / sizeof(err_msg_map[0]); i++) {
        if (err_msg_map[i].err_code == err_code) {
            msg = err_msg_map[i].err_msg;
            break;
        }
    }
    /* Avoiding to return NULL for unknown codes */
    return (msg == NULL) ? "???" : msg;
}

const char* stun_method_name(uint16_t type) {
    const char* name = NULL;
    size_t method = STUN_GET_METHOD(type);
    if (method < sizeof(method_map) / sizeof(method_map[0]))
        name = method_map[method];
    return (name == NULL) ? "???" : name;
}

const char* stun_class_name(uint16_t type) {
    if (STUN_IS_REQUEST(type))
        return "Request";
    else if (STUN_IS_SUCCESS_RESPONSE(type))
        return "Success Response";
    else if (STUN_IS_ERROR_RESPONSE(type))
        return "Error Response";
    else if (STUN_IS_INDICATION(type))
        return "Indication";
    else
        return "???";
}

void stun_msg_hdr_init(stun_msg_hdr* msg_hdr, uint16_t type, const uint8_t tsx_id[12]) {
    memset(msg_hdr, 0, sizeof(stun_msg_hdr));
    msg_hdr->type = htons(type);
    msg_hdr->magic = htonl(STUN_MAGIC_COOKIE);
    memcpy(&msg_hdr->tsx_id, tsx_id, sizeof(msg_hdr->tsx_id));
}

size_t stun_msg_len(const stun_msg_hdr* msg_hdr) {
    return sizeof(stun_msg_hdr) + ntohs(msg_hdr->length);
}

uint16_t stun_msg_type(const stun_msg_hdr* msg_hdr) {
    return ntohs(msg_hdr->type);
}

const uint8_t* stun_msg_end(const stun_msg_hdr* msg_hdr) {
    uint8_t* begin = (uint8_t*)msg_hdr;
    return begin + stun_msg_len(msg_hdr);
}

void stun_attr_hdr_init(stun_attr_hdr* hdr, uint16_t type, uint16_t length) {
    hdr->type = htons(type);
    hdr->length = htons(length);
}

uint8_t* stun_attr_end(stun_attr_hdr* attr_hdr) {
    uint8_t* begin = (uint8_t*)attr_hdr;
    return begin + stun_attr_block_len(attr_hdr);
}

int stun_attr_sockaddr_init(stun_attr_sockaddr* attr, uint16_t type, const struct sockaddr* addr) {
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in* sin = (struct sockaddr_in*)addr;
        stun_attr_hdr_init(&attr->hdr, type,
                           sizeof(attr->unused) + sizeof(attr->family) + sizeof(attr->port) +
                               sizeof(attr->addr.v4));
        attr->unused = 0;
        attr->family = STUN_IPV4;
        attr->port = sin->sin_port;
        memcpy(&attr->addr.v4, &sin->sin_addr, sizeof(sin->sin_addr));
        return STUN_OK;
    }
    else if (addr->sa_family == AF_INET6) {
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*)addr;
        stun_attr_hdr_init(&attr->hdr, type,
                           sizeof(attr->unused) + sizeof(attr->family) + sizeof(attr->port) +
                               sizeof(attr->addr.v6));
        attr->unused = 0;
        attr->family = STUN_IPV6;
        attr->port = sin6->sin6_port;
        memcpy(&attr->addr.v6, &sin6->sin6_addr, sizeof(sin6->sin6_addr));
        return STUN_OK;
    }
    else {
        return STUN_ERR_NOT_SUPPORTED;
    }
}

int stun_attr_xor_sockaddr_init(stun_attr_xor_sockaddr* attr, uint16_t type,
                                const struct sockaddr* addr, const stun_msg_hdr* hdr) {
    uint8_t* p;
    uint8_t* begin = (uint8_t*)attr;
    int status = stun_attr_sockaddr_init((stun_attr_sockaddr*)attr, type, addr);
    if (status != STUN_OK)
        return status;
    /* advance to the port */
    p = begin + sizeof(attr->hdr) + sizeof(attr->unused) + sizeof(attr->family);
    *(uint16_t*)p ^= htons((uint16_t)(STUN_MAGIC_COOKIE >> 16));
    p += sizeof(attr->port); /* advance the port */
    *(uint32_t*)p ^= htonl(STUN_MAGIC_COOKIE);
    p += sizeof(attr->addr.v4); /* advance the IPv4 address */
    if (attr->family == STUN_IPV6) {
        /* rest of IPv6 address has to be XOR'ed with the transaction id */
        *p++ ^= hdr->tsx_id[0];
        *p++ ^= hdr->tsx_id[1];
        *p++ ^= hdr->tsx_id[2];
        *p++ ^= hdr->tsx_id[3];
        *p++ ^= hdr->tsx_id[4];
        *p++ ^= hdr->tsx_id[5];
        *p++ ^= hdr->tsx_id[6];
        *p++ ^= hdr->tsx_id[7];
        *p++ ^= hdr->tsx_id[8];
        *p++ ^= hdr->tsx_id[9];
        *p++ ^= hdr->tsx_id[10];
        *p++ ^= hdr->tsx_id[11];
    }
    return STUN_OK;
}

void stun_attr_varsize_init(stun_attr_varsize* attr, uint16_t type, const void* buf,
                            size_t buf_size, uint8_t pad) {
    uint8_t* p = (uint8_t*)attr;
    stun_attr_hdr_init(&attr->hdr, type, (uint16_t)buf_size);
    memcpy(attr->value, buf, buf_size);
    store_padding(p + sizeof(attr->hdr) + buf_size, buf_size, pad);
}

void stun_attr_uint8_init(stun_attr_uint8* attr, uint16_t type, uint8_t value) {
    stun_attr_hdr_init(&attr->hdr, type, 4);
    attr->value = value;
    memset(attr->unused, 0, sizeof(attr->unused));
}

void stun_attr_uint8_pad_init(stun_attr_uint8_pad* attr, uint16_t type, uint8_t value,
                              uint8_t pad) {
    stun_attr_hdr_init(&attr->hdr, type, 1);
    attr->value = value;
    memset(attr->unused, pad, sizeof(attr->unused));
}

void stun_attr_uint16_init(stun_attr_uint16* attr, uint16_t type, uint16_t value) {
    stun_attr_hdr_init(&attr->hdr, type, 4);
    attr->value = htons(value);
    memset(attr->unused, 0, sizeof(attr->unused));
}

void stun_attr_uint16_pad_init(stun_attr_uint16_pad* attr, uint16_t type, uint16_t value,
                               uint8_t pad) {
    stun_attr_hdr_init(&attr->hdr, type, 2);
    attr->value = htons(value);
    memset(attr->unused, pad, sizeof(attr->unused));
}

void stun_attr_uint32_init(stun_attr_uint32* attr, uint16_t type, uint32_t value) {
    stun_attr_hdr_init(&attr->hdr, type, sizeof(attr->value));
    attr->value = htonl(value);
}

void stun_attr_uint64_init(stun_attr_uint64* attr, uint16_t type, uint64_t value) {
    stun_attr_hdr_init(&attr->hdr, type, sizeof(attr->value));
    attr->value = htonll(value);
}

void stun_attr_errcode_init(stun_attr_errcode* attr, int err_code, const char* err_reason,
                            uint8_t pad) {
    size_t reason_len = strlen(err_reason);
    uint8_t* p = (uint8_t*)attr;
    uint16_t attr_len = (uint16_t)(sizeof(attr->unused) + sizeof(attr->err_class) +
                                   sizeof(attr->err_code) + reason_len);
    stun_attr_hdr_init(&attr->hdr, STUN_ATTR_ERROR_CODE, attr_len);
    attr->unused = 0;
    attr->err_class = (uint8_t)(err_code / 100);
    attr->err_code = err_code % 100;
    memcpy(attr->err_reason, err_reason, reason_len);
    store_padding(p + sizeof(attr->hdr) + attr_len, attr_len, pad);
}

void stun_attr_unknown_init(stun_attr_unknown* attr, const uint16_t* unknown_codes, size_t count,
                            uint8_t pad) {
    size_t i;
    uint8_t* p = (uint8_t*)attr;
    uint16_t attr_len = (uint16_t)(count << 1);
    stun_attr_hdr_init(&attr->hdr, STUN_ATTR_UNKNOWN_ATTRIBUTES, attr_len);
    for (i = 0; i < count; i++)
        attr->attrs[i] = htons(unknown_codes[i]);
    store_padding(p + sizeof(attr->hdr) + attr_len, attr_len, pad);
}

void stun_attr_msgint_init(stun_attr_msgint* attr, const stun_msg_hdr* msg_hdr, const void* key,
                           size_t key_len) {
    uint8_t* p = (uint8_t*)msg_hdr;
    uint8_t* p_end = p + stun_msg_len(msg_hdr) - sizeof(*attr);
    HMAC_SHA1_CTX ctx;
    HMAC_SHA1_Init(&ctx, (const uint8_t*)key, key_len);
    HMAC_SHA1_Update(&ctx, p, p_end - p);
    HMAC_SHA1_Final(attr->hmac, &ctx);
}

void stun_attr_fingerprint_init(stun_attr_uint32* attr, const stun_msg_hdr* msg_hdr) {
    uint8_t* p = (uint8_t*)msg_hdr;
    uint8_t* p_end = p + stun_msg_len(msg_hdr) - sizeof(*attr);
    uint32_t value = crc32(0, p, p_end - p) ^ STUN_XOR_FINGERPRINT;
    attr->value = htonl(value);
}

void stun_msg_add_attr(stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr) {
    size_t attr_len = stun_attr_block_len(attr_hdr);
    msg_hdr->length = htons(ntohs(msg_hdr->length) + (uint16_t)attr_len);
}

void stun_attr_empty_add(stun_msg_hdr* msg_hdr, uint16_t type) {
    stun_attr_hdr* attr = (stun_attr_hdr*)stun_msg_end(msg_hdr);
    stun_attr_hdr_init(attr, type, 0);
    stun_msg_add_attr(msg_hdr, attr);
}

int stun_attr_sockaddr_add(stun_msg_hdr* msg_hdr, uint16_t type, const struct sockaddr* addr) {
    stun_attr_sockaddr* attr = (stun_attr_sockaddr*)stun_msg_end(msg_hdr);
    int status = stun_attr_sockaddr_init(attr, type, addr);
    if (status != STUN_OK)
        return status;
    stun_msg_add_attr(msg_hdr, &attr->hdr);
    return STUN_OK;
}

int stun_attr_xor_sockaddr_add(stun_msg_hdr* msg_hdr, uint16_t type, const struct sockaddr* addr) {
    stun_attr_xor_sockaddr* attr = (stun_attr_xor_sockaddr*)stun_msg_end(msg_hdr);
    int status = stun_attr_xor_sockaddr_init(attr, type, addr, msg_hdr);
    if (status != STUN_OK)
        return status;
    stun_msg_add_attr(msg_hdr, &attr->hdr);
    return STUN_OK;
}

void stun_attr_varsize_add(stun_msg_hdr* msg_hdr, uint16_t type, const void* buf, size_t buf_size,
                           uint8_t pad) {
    stun_attr_varsize* attr = (stun_attr_varsize*)stun_msg_end(msg_hdr);
    stun_attr_varsize_init(attr, type, buf, buf_size, pad);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_uint8_add(stun_msg_hdr* msg_hdr, uint16_t type, uint8_t value) {
    stun_attr_uint8* attr = (stun_attr_uint8*)stun_msg_end(msg_hdr);
    stun_attr_uint8_init(attr, type, value);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_uint8_pad_add(stun_msg_hdr* msg_hdr, uint16_t type, uint8_t value, uint8_t pad) {
    stun_attr_uint8_pad* attr = (stun_attr_uint8_pad*)stun_msg_end(msg_hdr);
    stun_attr_uint8_pad_init(attr, type, value, pad);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_uint16_add(stun_msg_hdr* msg_hdr, uint16_t type, uint16_t value) {
    stun_attr_uint16* attr = (stun_attr_uint16*)stun_msg_end(msg_hdr);
    stun_attr_uint16_init(attr, type, value);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_uint16_pad_add(stun_msg_hdr* msg_hdr, uint16_t type, uint16_t value, uint8_t pad) {
    stun_attr_uint16_pad* attr = (stun_attr_uint16_pad*)stun_msg_end(msg_hdr);
    stun_attr_uint16_pad_init(attr, type, value, pad);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_uint32_add(stun_msg_hdr* msg_hdr, uint16_t type, uint32_t value) {
    stun_attr_uint32* attr = (stun_attr_uint32*)stun_msg_end(msg_hdr);
    stun_attr_uint32_init(attr, type, value);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_uint64_add(stun_msg_hdr* msg_hdr, uint16_t type, uint64_t value) {
    stun_attr_uint64* attr = (stun_attr_uint64*)stun_msg_end(msg_hdr);
    stun_attr_uint64_init(attr, type, value);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_errcode_add(stun_msg_hdr* msg_hdr, int err_code, const char* err_reason,
                           uint8_t pad) {
    stun_attr_errcode* attr = (stun_attr_errcode*)stun_msg_end(msg_hdr);
    stun_attr_errcode_init(attr, err_code, err_reason, pad);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_unknown_add(stun_msg_hdr* msg_hdr, const uint16_t* unknown_codes, size_t count,
                           uint8_t pad) {
    stun_attr_unknown* attr = (stun_attr_unknown*)stun_msg_end(msg_hdr);
    stun_attr_unknown_init(attr, unknown_codes, count, pad);
    stun_msg_add_attr(msg_hdr, &attr->hdr);
}

void stun_attr_msgint_add(stun_msg_hdr* msg_hdr, const void* key, size_t key_len) {
    stun_attr_msgint* attr = (stun_attr_msgint*)stun_msg_end(msg_hdr);
    stun_attr_hdr_init(&attr->hdr, STUN_ATTR_MESSAGE_INTEGRITY, sizeof(attr->hmac));
    stun_msg_add_attr(msg_hdr, &attr->hdr);
    stun_attr_msgint_init(attr, msg_hdr, key, key_len);
}

void stun_attr_fingerprint_add(stun_msg_hdr* msg_hdr) {
    stun_attr_uint32* attr = (stun_attr_uint32*)stun_msg_end(msg_hdr);
    stun_attr_hdr_init(&attr->hdr, STUN_ATTR_FINGERPRINT, sizeof(attr->value));
    stun_msg_add_attr(msg_hdr, &attr->hdr);
    stun_attr_fingerprint_init(attr, msg_hdr);
}

int stun_msg_verify(const stun_msg_hdr* msg_hdr, size_t msg_size) {
    size_t msg_len;
    const uint8_t* p = (const uint8_t*)msg_hdr;
    const uint8_t* p_end;
    const stun_attr_hdr* attr_hdr;

    /* First byte of STUN message is always 0x00 or 0x01. */
    if (*p != 0x00 && *p != 0x01)
        return 0;

    /* Check the length, it cannot exceed the message size. */
    msg_len = stun_msg_len(msg_hdr);
    if (msg_len > msg_size)
        return 0;

    /* STUN message is always padded to the nearest 4 bytes, thus
     * the last two bits of the length field are always zero.
     */
    if ((msg_len & 0x03) != 0)
        return 0;

    /* Check if the attribute lengths don't exceed the message length. */
    p_end = p + msg_len;
    p += sizeof(stun_msg_hdr);
    if (p == p_end)
        return 1; /* It's an empty message, nothing else to check */
    do {
        attr_hdr = (const stun_attr_hdr*)p;
        p += stun_attr_block_len(attr_hdr);
    } while (p < p_end);
    if (p != p_end)
        return 0;

    /* If FINGERPRINT is the last attribute, check if is valid */
    if (ntohs(attr_hdr->type) == STUN_ATTR_FINGERPRINT) {
        const stun_attr_uint32* fingerprint = (const stun_attr_uint32*)attr_hdr;
        if (!stun_attr_fingerprint_check(fingerprint, msg_hdr))
            return 0;
    }

    return 1; /* all is well */
}

size_t stun_attr_len(const stun_attr_hdr* attr_hdr) {
    return ntohs(attr_hdr->length);
}

size_t stun_attr_block_len(const stun_attr_hdr* attr_hdr) {
    return sizeof(*attr_hdr) + ((stun_attr_len(attr_hdr) + 3) & (~3));
}

uint16_t stun_attr_type(const stun_attr_hdr* attr_hdr) {
    return ntohs(attr_hdr->type);
}

const stun_attr_hdr* stun_msg_next_attr(const stun_msg_hdr* msg_hdr,
                                        const stun_attr_hdr* attr_hdr) {
    uint8_t* p;
    const uint8_t* p_end = stun_msg_end(msg_hdr);
    if (!attr_hdr) {
        p = ((uint8_t*)msg_hdr) + sizeof(stun_msg_hdr);
    }
    else {
        p = ((uint8_t*)attr_hdr) + stun_attr_block_len(attr_hdr);
    }
    if (p >= p_end)
        return NULL;
    return (stun_attr_hdr*)p;
}

const stun_attr_hdr* stun_msg_find_attr(const stun_msg_hdr* msg_hdr, uint16_t type) {
    const stun_attr_hdr* it = NULL;
    while ((it = stun_msg_next_attr(msg_hdr, it)) != NULL) {
        if (stun_attr_type(it) == type)
            break;
    }
    return it;
}

int stun_attr_sockaddr_read(const stun_attr_sockaddr* attr, struct sockaddr* addr) {
    if (attr->family == STUN_IPV4) {
        struct sockaddr_in* sin = (struct sockaddr_in*)addr;
        sin->sin_family = AF_INET;
        sin->sin_port = attr->port;
        memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
        memcpy(&sin->sin_addr, &attr->addr.v4, sizeof(attr->addr.v4));
        return STUN_OK;
    }
    else if (attr->family == STUN_IPV6) {
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*)addr;
        memset(sin6, 0, sizeof(struct sockaddr_in6));
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = attr->port;
        memcpy(&sin6->sin6_addr, &attr->addr.v6, sizeof(attr->addr.v6));
        return STUN_OK;
    }
    else {
        return STUN_ERR_BAD_ADDR_FAMILY;
    }
}

int stun_attr_xor_sockaddr_read(const stun_attr_xor_sockaddr* attr, const stun_msg_hdr* msg_hdr,
                                struct sockaddr* addr) {
    int status = stun_attr_sockaddr_read((const stun_attr_sockaddr*)attr, addr);
    if (status < STUN_OK)
        return status;
    if (addr->sa_family == AF_INET) {
        struct sockaddr_in* sin = (struct sockaddr_in*)addr;
        sin->sin_port ^= htons((uint16_t)(STUN_MAGIC_COOKIE >> 16));
        *((uint32_t*)&sin->sin_addr) ^= htonl(STUN_MAGIC_COOKIE);
    }
    else {
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*)addr;
        uint8_t* p = (uint8_t*)&sin6->sin6_addr;
        sin6->sin6_port ^= htons((uint16_t)(STUN_MAGIC_COOKIE >> 16));
        *((uint32_t*)p) ^= htonl(STUN_MAGIC_COOKIE);
        p += 4;
        /* rest of IPv6 address has to be XOR'ed with the transaction id */
        *p++ ^= msg_hdr->tsx_id[0];
        *p++ ^= msg_hdr->tsx_id[1];
        *p++ ^= msg_hdr->tsx_id[2];
        *p++ ^= msg_hdr->tsx_id[3];
        *p++ ^= msg_hdr->tsx_id[4];
        *p++ ^= msg_hdr->tsx_id[5];
        *p++ ^= msg_hdr->tsx_id[6];
        *p++ ^= msg_hdr->tsx_id[7];
        *p++ ^= msg_hdr->tsx_id[8];
        *p++ ^= msg_hdr->tsx_id[9];
        *p++ ^= msg_hdr->tsx_id[10];
        *p++ ^= msg_hdr->tsx_id[11];
    }
    return STUN_OK;
}

const void* stun_attr_varsize_read(const stun_attr_varsize* attr) {
    return attr->value;
}

uint8_t stun_attr_uint8_read(const stun_attr_uint8* attr) {
    return attr->value;
}

uint16_t stun_attr_uint16_read(const stun_attr_uint16* attr) {
    return ntohs(attr->value);
}

uint32_t stun_attr_uint32_read(const stun_attr_uint32* attr) {
    return ntohl(attr->value);
}

uint64_t stun_attr_uint64_read(const stun_attr_uint64* attr) {
    return ntohll(attr->value);
}

int stun_attr_errcode_status(const stun_attr_errcode* attr) {
    return attr->err_class * 100 + attr->err_code;
}

const char* stun_attr_errcode_reason(const stun_attr_errcode* attr) {
    return attr->err_reason;
}

size_t stun_attr_errcode_reason_len(const stun_attr_errcode* attr) {
    return stun_attr_len(&attr->hdr) - sizeof(stun_attr_hdr);
}

size_t stun_attr_unknown_count(const stun_attr_unknown* attr) {
    return ntohs(attr->hdr.length) >> 1;
}

uint16_t stun_attr_unknown_get(const stun_attr_unknown* attr, size_t n) {
    if (n >= stun_attr_unknown_count(attr))
        return 0;
    return ntohs(attr->attrs[n]);
}

uint16_t* stun_attr_unknown_next(const stun_attr_unknown* attr, uint16_t* unk_it) {
    uint8_t* p;
    uint8_t* p_end = stun_attr_end((stun_attr_hdr*)attr);
    if (!unk_it) {
        p = ((uint8_t*)attr) + sizeof(stun_attr_hdr);
    }
    else {
        p = ((uint8_t*)unk_it) + sizeof(uint16_t);
    }
    if (p >= p_end)
        return NULL;
    return (uint16_t*)p;
}

int stun_attr_msgint_check(const stun_attr_msgint* msgint, const stun_msg_hdr* msg_hdr,
                           const uint8_t* key, size_t key_len) {
    uint8_t* p = (uint8_t*)msg_hdr;
    const uint8_t* p_end = stun_msg_end(msg_hdr) - STUN_ATTR_MSGINT_SIZE;
    uint16_t length;
    uint8_t digest[20];
    HMAC_SHA1_CTX ctx;
    const stun_attr_hdr* fingerprint = stun_msg_find_attr(msg_hdr, STUN_ATTR_FINGERPRINT);
    if (fingerprint) {
        length = htons(ntohs(msg_hdr->length) - STUN_ATTR_UINT32_SIZE);
        p_end -= STUN_ATTR_UINT32_SIZE;
    }
    else {
        length = msg_hdr->length;
    }
    HMAC_SHA1_Init(&ctx, key, key_len);
    HMAC_SHA1_Update(&ctx, p, sizeof(msg_hdr->type));
    HMAC_SHA1_Update(&ctx, (uint8_t*)&length, sizeof(length));
    p += sizeof(msg_hdr->type) + sizeof(msg_hdr->length);
    HMAC_SHA1_Update(&ctx, p, p_end - p);
    HMAC_SHA1_Final(digest, &ctx);
    return memcmp(digest, msgint->hmac, sizeof(digest)) == 0 ? 1 : 0;
}

void stun_genkey(const void* username, size_t username_len, const void* realm, size_t realm_len,
                 const void* password, size_t password_len, uint8_t key[16]) {
    MD5_CTX ctx;
    rtc2_MD5_Init(&ctx);
    rtc2_MD5_Update(&ctx, username, username_len);
    rtc2_MD5_Update(&ctx, ":", 1);
    rtc2_MD5_Update(&ctx, realm, realm_len);
    rtc2_MD5_Update(&ctx, ":", 1);
    rtc2_MD5_Update(&ctx, password, password_len);
    rtc2_MD5_Final(key, &ctx);
}

int stun_attr_fingerprint_check(const stun_attr_uint32* fingerprint, const stun_msg_hdr* msg_hdr) {
    uint32_t value;
    uint8_t *p, *p_end;
    p_end = (uint8_t*)fingerprint;
    p = (uint8_t*)msg_hdr;
    value = crc32(0, p, p_end - p) ^ STUN_XOR_FINGERPRINT;
    return ntohl(fingerprint->value) != value ? 0 : 1;
}
