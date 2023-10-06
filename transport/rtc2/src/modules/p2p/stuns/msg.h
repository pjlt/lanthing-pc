/* Copyright (c) 2014 Guilherme Balena Versiani.
 *
 * I dedicate any and all copyright interest in this software to the
 * public domain. I make this dedication for the benefit of the public at
 * large and to the detriment of my heirs and successors. I intend this
 * dedication to be an overt act of relinquishment in perpetuity of all
 * present and future rights to this software under copyright law.
 */

#ifndef __STUNMSG_H__
#define __STUNMSG_H__

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward the sockaddr declaration */
struct sockaddr;

/* Used to demultiplex STUN and RTP */
#define STUN_CHECK(pkt) \
  ((((uint8_t *) pkt)[0] & 0xC0) == 0x00)

/* STUN magic cookie */
#define STUN_MAGIC_COOKIE 0x2112A442ul

/* STUN XOR fingerprint */
#define STUN_XOR_FINGERPRINT 0x5354554euL

/* Retrieve the STUN method from the message-type field of the STUN message */
#define STUN_GET_METHOD(msg_type) ((msg_type) & 0xFEEF)

/* Determine if the message type is a request */
#define STUN_IS_REQUEST(msg_type) (((msg_type) & 0x0110) == 0x0000)

/* Determine if the message type is a successful response */
#define STUN_IS_SUCCESS_RESPONSE(msg_type) (((msg_type) & 0x0110) == 0x0100)

/* Determine if the message type is an error response */
#define STUN_IS_ERROR_RESPONSE(msg_type) (((msg_type) & 0x0110) == 0x0110)

/* Determine if the message type is a response */
#define STUN_IS_RESPONSE(msg_type) (((msg_type) & 0x0100) == 0x0100)

/* Determine if the message type is an indication message */
#define STUN_IS_INDICATION(msg_type) (((msg_type) & 0x0110) == 0x0010)

enum stun_msg_type {
/* Type                                  | Value    | Reference */
  STUN_BINDING_REQUEST                   = 0x0001, /* RFC 5389  */
  STUN_BINDING_RESPONSE                  = 0x0101, /* RFC 5389  */
  STUN_BINDING_ERROR_RESPONSE            = 0x0111, /* RFC 5389  */
  STUN_BINDING_INDICATION                = 0x0011, /* RFC 5389  */
  STUN_SHARED_SECRET_REQUEST             = 0x0002, /* RFC 5389  */
  STUN_SHARED_SECRET_RESPONSE            = 0x0102, /* RFC 5389  */
  STUN_SHARED_SECRET_ERROR_RESPONSE      = 0x0112, /* RFC 5389  */
  STUN_ALLOCATE_REQUEST                  = 0x0003, /* RFC 5766  */
  STUN_ALLOCATE_RESPONSE                 = 0x0103, /* RFC 5766  */
  STUN_ALLOCATE_ERROR_RESPONSE           = 0x0113, /* RFC 5766  */
  STUN_REFRESH_REQUEST                   = 0x0004, /* RFC 5766  */
  STUN_REFRESH_RESPONSE                  = 0x0104, /* RFC 5766  */
  STUN_REFRESH_ERROR_RESPONSE            = 0x0114, /* RFC 5766  */
  STUN_SEND_INDICATION                   = 0x0016, /* RFC 5766  */
  STUN_DATA_INDICATION                   = 0x0017, /* RFC 5766  */
  STUN_CREATE_PERM_REQUEST               = 0x0008, /* RFC 5766  */
  STUN_CREATE_PERM_RESPONSE              = 0x0108, /* RFC 5766  */
  STUN_CREATE_PERM_ERROR_RESPONSE        = 0x0118, /* RFC 5766  */
  STUN_CHANNEL_BIND_REQUEST              = 0x0009, /* RFC 5766  */
  STUN_CHANNEL_BIND_RESPONSE             = 0x0109, /* RFC 5766  */
  STUN_CHANNEL_BIND_ERROR_RESPONSE       = 0x0119, /* RFC 5766  */
  STUN_CONNECT_REQUEST                   = 0x000A, /* RFC 6062  */
  STUN_CONNECT_RESPONSE                  = 0x010A, /* RFC 6062  */
  STUN_CONNECT_ERROR_RESPONSE            = 0x011A, /* RFC 6062  */
  STUN_CONNECTION_BIND_REQUEST           = 0x000B, /* RFC 6062  */
  STUN_CONNECTION_BIND_RESPONSE          = 0x010B, /* RFC 6062  */
  STUN_CONNECTION_BIND_ERROR_RESPONSE    = 0x011B, /* RFC 6062  */
  STUN_CONNECTION_ATTEMPT_REQUEST        = 0x000C, /* RFC 6062  */
  STUN_CONNECTION_ATTEMPT_RESPONSE       = 0x010C, /* RFC 6062  */
  STUN_CONNECTION_ATTEMPT_ERROR_RESPONSE = 0x011C, /* RFC 6062  */
};

enum stun_attr_type {
/* Attribute                    | Value  | Type                     | Reference */
  STUN_ATTR_MAPPED_ADDRESS      = 0x0001, /* stun_attr_sockaddr     | RFC 5389  */
  STUN_ATTR_RESPONSE_ADDRESS    = 0x0002, /* stun_attr_sockaddr     | RFC 5389  */
  STUN_ATTR_CHANGE_REQUEST      = 0x0003, /* stun_attr_uint32       | RFC 5780  */
  STUN_ATTR_SOURCE_ADDRESS      = 0x0004, /* stun_attr_sockaddr     | RFC 5389  */
  STUN_ATTR_CHANGED_ADDRESS     = 0x0005, /* stun_attr_sockaddr     | RFC 5389  */
  STUN_ATTR_USERNAME            = 0x0006, /* stun_attr_varsize      | RFC 5389  */
  STUN_ATTR_PASSWORD            = 0x0007, /* stun_attr_varsize      | RFC 5389  */
  STUN_ATTR_MESSAGE_INTEGRITY   = 0x0008, /* stun_attr_msgint       | RFC 5389  */
  STUN_ATTR_ERROR_CODE          = 0x0009, /* stun_attr_errcode      | RFC 5389  */
  STUN_ATTR_UNKNOWN_ATTRIBUTES  = 0x000A, /* stun_attr_unknown      | RFC 5389  */
  STUN_ATTR_REFLECTED_FROM      = 0x000B, /* stun_attr_sockaddr     | RFC 5389  */
  STUN_ATTR_CHANNEL_NUMBER      = 0x000C, /* stun_attr_uint32       | RFC 5766  */
  STUN_ATTR_LIFETIME            = 0x000D, /* stun_attr_uint32       | RFC 5766  */
  STUN_ATTR_BANDWIDTH           = 0x0010, /* stun_attr_uint32       | RFC 5766  */
  STUN_ATTR_XOR_PEER_ADDRESS    = 0x0012, /* stun_attr_xor_sockaddr | RFC 5766  */
  STUN_ATTR_DATA                = 0x0013, /* stun_attr_varsize      | RFC 5766  */
  STUN_ATTR_REALM               = 0x0014, /* stun_attr_varsize      | RFC 5389  */
  STUN_ATTR_NONCE               = 0x0015, /* stun_attr_varsize      | RFC 5389  */
  STUN_ATTR_XOR_RELAYED_ADDRESS = 0x0016, /* stun_attr_xor_sockaddr | RFC 5766  */
  STUN_ATTR_REQ_ADDRESS_FAMILY  = 0x0017, /* stun_attr_uint8        | RFC 6156  */
  STUN_ATTR_EVEN_PORT           = 0x0018, /* stun_attr_uint8_pad    | RFC 5766  */
  STUN_ATTR_REQUESTED_TRANSPORT = 0x0019, /* stun_attr_uint32       | RFC 5766  */
  STUN_ATTR_DONT_FRAGMENT       = 0x001A, /* empty                  | RFC 5766  */
  STUN_ATTR_XOR_MAPPED_ADDRESS  = 0x0020, /* stun_attr_xor_sockaddr | RFC 5389  */
  STUN_ATTR_TIMER_VAL           = 0x0021, /* stun_attr_uint32       | RFC 5766  */
  STUN_ATTR_RESERVATION_TOKEN   = 0x0022, /* stun_attr_uint64       | RFC 5766  */
  STUN_ATTR_PRIORITY            = 0x0024, /* stun_attr_uint32       | RFC 5245  */
  STUN_ATTR_USE_CANDIDATE       = 0x0025, /* empty                  | RFC 5245  */
  STUN_ATTR_PADDING             = 0x0026, /* stun_attr_varsize      | RFC 5780  */
  STUN_ATTR_RESPONSE_PORT       = 0x0027, /* stun_attr_uint16_pad   | RFC 5780  */
  STUN_ATTR_CONNECTION_ID       = 0x002A, /* stun_attr_uint32       | RFC 6062  */
  STUN_ATTR_SOFTWARE            = 0x8022, /* stun_attr_varsize      | RFC 5389  */
  STUN_ATTR_ALTERNATE_SERVER    = 0x8023, /* stun_attr_sockaddr     | RFC 5389  */
  STUN_ATTR_FINGERPRINT         = 0x8028, /* stun_attr_uint32       | RFC 5389  */
  STUN_ATTR_ICE_CONTROLLED      = 0x8029, /* stun_attr_uint64       | RFC 5245  */
  STUN_ATTR_ICE_CONTROLLING     = 0x802A, /* stun_attr_uint64       | RFC 5245  */
  STUN_ATTR_RESPONSE_ORIGIN     = 0x802B, /* stun_attr_sockaddr     | RFC 5780  */
  STUN_ATTR_OTHER_ADDRESS       = 0x802C, /* stun_attr_sockaddr     | RFC 5780  */
};

enum stun_error_code_type {
/* Code                                | Value | Reference */
  STUN_ERROR_TRY_ALTERNATE             = 300, /* RFC 5389  */
  STUN_ERROR_BAD_REQUEST               = 400, /* RFC 5389  */
  STUN_ERROR_UNAUTHORIZED              = 401, /* RFC 5389  */
  STUN_ERROR_FORBIDDEN                 = 403, /* RFC 5766  */
  STUN_ERROR_UNKNOWN_ATTRIBUTE         = 420, /* RFC 5389  */
  STUN_ERROR_ALLOCATION_MISMATCH       = 437, /* RFC 5766  */
  STUN_ERROR_STALE_NONCE               = 438, /* RFC 5389  */
  STUN_ERROR_ADDR_FAMILY_NOT_SUPP      = 440, /* RFC 6156  */
  STUN_ERROR_WRONG_CREDENTIALS         = 441, /* RFC 5766  */
  STUN_ERROR_UNSUPP_TRANSPORT_PROTO    = 442, /* RFC 5766  */
  STUN_ERROR_PEER_ADD_FAMILY_MISMATCH  = 443, /* RFC 6156  */
  STUN_ERROR_CONNECTION_ALREADY_EXISTS = 446, /* RFC 6062  */
  STUN_ERROR_CONNECTION_FAILURE        = 447, /* RFC 6062  */
  STUN_ERROR_ALLOCATION_QUOTA_REACHED  = 486, /* RFC 5766  */
  STUN_ERROR_ROLE_CONFLICT             = 487, /* RFC 5245  */
  STUN_ERROR_SERVER_ERROR              = 500, /* RFC 5389  */
  STUN_ERROR_INSUFFICIENT_CAPACITY     = 508, /* RFC 5766  */
};

/* STUN address families */
enum stun_addr_family {
  STUN_IPV4 = 0x01,
  STUN_IPV6 = 0x02
};

#pragma pack(1)

typedef struct _stun_msg_hdr {
  uint16_t type;                               /* message type */
  uint16_t length;                             /* message length */
  uint32_t magic;                              /* magic cookie */
  uint8_t tsx_id[12];                          /* transaction id */
} stun_msg_hdr;

typedef struct _stun_attr_hdr {
  uint16_t type;                               /* attribute type */
  uint16_t length;                             /* length, no padding */
} stun_attr_hdr;

typedef struct _stun_attr_sockaddr {
  stun_attr_hdr hdr;
  uint8_t unused;
  uint8_t family;                              /* IPv4 = 1, IPv6 = 2 */
  uint16_t port;
  union {
    uint8_t v4[4];
    uint8_t v6[16];
  } addr;
} stun_attr_sockaddr;

typedef struct _stun_attr_varsize {
  stun_attr_hdr hdr;
  uint8_t value[1];                            /* variable size value */
} stun_attr_varsize;

typedef struct _stun_attr_uint8 {
  stun_attr_hdr hdr;
  uint8_t value;                               /* single 8-bit value */
  uint8_t unused[3];
} stun_attr_uint8;

typedef struct _stun_attr_uint16 {
  stun_attr_hdr hdr;
  uint16_t value;                              /* single 16-bit value */
  uint8_t unused[2];
} stun_attr_uint16;

typedef struct _stun_attr_uint32 {
  stun_attr_hdr hdr;
  uint32_t value;                              /* single 32-bit value */
} stun_attr_uint32;

typedef struct _stun_attr_uint64 {
  stun_attr_hdr hdr;
  uint64_t value;                              /* single 64-bit value */
} stun_attr_uint64;

typedef struct _stun_attr_msgint {
  stun_attr_hdr hdr;
  uint8_t hmac[20];                            /* HMAC-SHA1 hash */
} stun_attr_msgint;

typedef struct _stun_attr_errcode {
  stun_attr_hdr hdr;
  uint16_t unused;
  uint8_t err_class;                           /* code / 100 */
  uint8_t err_code;                            /* code % 100 */
  char err_reason[1];
} stun_attr_errcode;

typedef struct _stun_attr_unknown {
  stun_attr_hdr hdr;
  uint16_t attrs[1];                           /* list of 16-bit values */
} stun_attr_unknown;

#pragma pack()

/* Gets the size of a sockaddr attribute, given the address type */
#define STUN_ATTR_SOCKADDR_SIZE(x) (4 + 4 + ((x) == STUN_IPV4 ? 4 : 16))

/* Gets the size of a varsize attribute, given the string/payload length */
#define STUN_ATTR_VARSIZE_SIZE(x) (4 + (((x) + 3) & (~3)))

/* Gets the size of an ERROR-CODE attribute, given the reason phrase length */
#define STUN_ATTR_ERROR_CODE_SIZE(x) (4 + 4 + (((x) + 3) & (~3)))

/* Gets the size of a UNKNOWN attribute, given the number of attributes */
#define STUN_ATTR_UNKNOWN_SIZE(x) (4 + ((((x) << 1) + 3) & (~3)))

/* Gets the size of a 8-bit attribute */
#define STUN_ATTR_UINT8_SIZE (4 + 4)

/* Gets the size of a 16-bit attribute */
#define STUN_ATTR_UINT16_SIZE (4 + 4)

/* Gets the size of a 32-bit attribute */
#define STUN_ATTR_UINT32_SIZE (4 + 4)

/* Gets the size of a 64-bit attribute */
#define STUN_ATTR_UINT64_SIZE (4 + 8)

/* Gets the size of a MESSAGE-INTEGRITY attribute */
#define STUN_ATTR_MSGINT_SIZE (4 + 20)

/* Gets the size of a FINGERPRINT attribute */
#define STUN_ATTR_FINGERPRINT_SIZE STUN_ATTR_UINT32_SIZE

/* Equivalent types */
typedef stun_attr_sockaddr stun_attr_xor_sockaddr;
typedef stun_attr_uint8 stun_attr_uint8_pad;
typedef stun_attr_uint16 stun_attr_uint16_pad;

/* The returned values from the below functions */
enum stun_status_type {
  STUN_OK                    = 0,
  STUN_ERR_NOT_SUPPORTED     = -1,
  STUN_ERR_NO_MEMORY         = -2,
  STUN_ERR_INVALID_ARG       = -3,
  STUN_ERR_UNKNOWN_ATTRIBUTE = -4,
  STUN_ERR_TOO_SMALL         = -5,
  STUN_ERR_BAD_TYPE          = -6,
  STUN_ERR_TRAIL_ATTRIBUTES  = -7,
  STUN_ERR_BAD_MSGINT        = -8,
  STUN_ERR_BAD_FINGERPRINT   = -9,
  STUN_ERR_PWD_NOTAVAIL      = -10,
  STUN_ERR_BAD_ADDR_FAMILY   = -11,
};

/* Get STUN standard reason phrase for the specified error code. NULL is
 * returned for unknown error codes.
 */
const char *stun_err_reason(int err_code);

/* Get STUN message type name. */
const char *stun_method_name(uint16_t type);

/* Get STUN message class name. */
const char *stun_class_name(uint16_t type);

/* Initializes a STUN message. */
void stun_msg_hdr_init(stun_msg_hdr *msg_hdr, uint16_t type,
                       const uint8_t tsx_id[12]);

/* Gets the STUN message type. */
uint16_t stun_msg_type(const stun_msg_hdr *msg_hdr);

/* Gets the STUN message length (including header). */
size_t stun_msg_len(const stun_msg_hdr *msg_hdr);

/* Gets the STUN message end. */
const uint8_t *stun_msg_end(const stun_msg_hdr *msg_hdr);

/* Initializes a generic attribute header */
void stun_attr_hdr_init(stun_attr_hdr *attr_hdr, uint16_t type, uint16_t len);

/* Gets the STUN attribute end. */
uint8_t *stun_attr_end(stun_attr_hdr *attr_hdr);

/* Initializes a sockaddr attribute */
int stun_attr_sockaddr_init(stun_attr_sockaddr *sockaddr_attr, uint16_t type,
                            const struct sockaddr *addr);

/* Initializes a XOR'ed sockaddr attribute */
int stun_attr_xor_sockaddr_init(stun_attr_xor_sockaddr *sockaddr_attr,
                                uint16_t type, const struct sockaddr *addr,
                                const stun_msg_hdr *msg_hdr);

/* Initializes a varsize attribute. Check macro STUN_ATTR_VARSIZE_SIZE for
 * the correct attribute size.
 */
void stun_attr_varsize_init(stun_attr_varsize *attr, uint16_t type,
                            const void *buf, size_t buf_size, uint8_t pad);

/* Initializes an 8-bit attribute. Length will be 4 followed by 3 zeroed
 * bytes (normally used as RFFU = Reserved For Future Use), like the below:
 *
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |         type                  |            Length=4           |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |     value     |                   RFFU=0                      |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
void stun_attr_uint8_init(stun_attr_uint8 *attr, uint16_t type, uint8_t value);

/* Initializes an 8-bit attribute with padding. Length will be 1 followed by
 * 3 padding bytes, like the below:
 *
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |         type                  |            Length=1           |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |     value     |                   padding                     |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
void stun_attr_uint8_pad_init(stun_attr_uint8_pad *attr, uint16_t type,
                              uint8_t value, uint8_t pad);

/* Initializes a 16-bit attribute. Length will be 4 followed by 2 zeroed
 * bytes (normally used as RFFU = Reserved For Future Use), like the below:
 *
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |         type                  |            Length=4           |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |         value                 |             RFFU=0            |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
void stun_attr_uint16_init(stun_attr_uint16 *attr, uint16_t type,
                           uint16_t value);

/* Initializes a 16-bit attribute with padding. Length will be 4 followed by
 * 2 zeroed bytes (normally used as RFFU = Reserved For Future Use), like the
 * below:
 *
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |         type                  |            Length=2           |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |         value                 |            padding            |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
void stun_attr_uint16_pad_init(stun_attr_uint16_pad *attr, uint16_t type,
                               uint16_t value, uint8_t pad);

/* Initializes a 32-bit attribute */
void stun_attr_uint32_init(stun_attr_uint32 *attr, uint16_t type,
                           uint32_t value);

/* Initializes a 64-bit attribute */
void stun_attr_uint64_init(stun_attr_uint64 *attr, uint16_t type,
                           uint64_t value);

/* Initializes an ERROR-CODE attribute */
void stun_attr_errcode_init(stun_attr_errcode *attr, int err_code,
                            const char *err_reason, uint8_t pad);

/* Initializes an UNKNOWN-ATTRIBUTES attribute */
void stun_attr_unknown_init(stun_attr_unknown *attr,
                            const uint16_t *unknown_codes, size_t count,
                            uint8_t pad);

/* Initializes a MESSAGE-INTEGRITY attribute. Note that this attribute must be
 * the next to last one in a STUN message, before FINGERPRINT. It also expects
 * that you already have added the provided attribute to the message header.
 *
 * This function will calculate the HMAC-SHA1 digest from the message using the
 * supplied password. If the message contains USERNAME and REALM attributes,
 * then the key will be MD5(username ":" realm ":" password). Otherwise the
 * hash key is simply the input password.
 */
void stun_attr_msgint_init(stun_attr_msgint *attr, const stun_msg_hdr *msg_hdr,
                           const void *key, size_t key_len);

/* Initializes a FINGERPRINT attribute. Note that this attribute must be the
 * last one in a STUN message, right after the MESSAGE-INTEGRITY. It also
 * expects that you already have added the provided attribute to the message
 * header.
 */
void stun_attr_fingerprint_init(stun_attr_uint32 *attr,
                                const stun_msg_hdr *msg_hdr);

/* Appends an attribute to the STUN message header. */
void stun_msg_add_attr(stun_msg_hdr *msg_hdr, const stun_attr_hdr *attr);

/* Adds an empty attribute to the message end */
void stun_attr_empty_add(stun_msg_hdr *msg_hdr, uint16_t type);

/* Adds a sockaddr attribute to the message end */
int stun_attr_sockaddr_add(stun_msg_hdr *msg_hdr, uint16_t type,
                           const struct sockaddr *addr);

/* Adds a XOR'ed sockaddr attribute to the message end */
int stun_attr_xor_sockaddr_add(stun_msg_hdr *msg_hdr, uint16_t type,
                               const struct sockaddr *addr);

/* Adds a varsize attribute to the message end */
void stun_attr_varsize_add(stun_msg_hdr *msg_hdr, uint16_t type,
                           const void *buf, size_t buf_size, uint8_t pad);

/* Adds an 8-bit attribute to the message end */
void stun_attr_uint8_add(stun_msg_hdr *msg_hdr, uint16_t type, uint8_t value);

/* Adds an 8-bit attribute with padding to the message end */
void stun_attr_uint8_pad_add(stun_msg_hdr *msg_hdr, uint16_t type,
                             uint8_t value, uint8_t pad);

/* Adds a 16-bit attribute to the message end */
void stun_attr_uint16_add(stun_msg_hdr *msg_hdr, uint16_t type,
                          uint16_t value);

/* Adds a 16-bit attribute with padding to the message end */
void stun_attr_uint16_pad_add(stun_msg_hdr *msg_hdr, uint16_t type,
                              uint16_t value, uint8_t pad);

/* Adds a 32-bit attribute to the message end */
void stun_attr_uint32_add(stun_msg_hdr *msg_hdr, uint16_t type,
                          uint32_t value);

/* Adds a 64-bit attribute to the message end */
void stun_attr_uint64_add(stun_msg_hdr *msg_hdr, uint16_t type,
                          uint64_t value);

/* Adds an ERROR-CODE attribute to the message end */
void stun_attr_errcode_add(stun_msg_hdr *msg_hdr, int err_code,
                           const char *err_reason, uint8_t pad);

/* Adds an UNKNOWN-ATTRIBUTES attribute to the message end */
void stun_attr_unknown_add(stun_msg_hdr *msg_hdr,
                           const uint16_t *unknown_codes, size_t count,
                           uint8_t pad);

/* Adds a MESSAGE-INTEGRITY to the message end */
void stun_attr_msgint_add(stun_msg_hdr *msg_hdr,
                          const void *key, size_t key_len);

/* Adds a FINGERPRINT attribute to the message end */
void stun_attr_fingerprint_add(stun_msg_hdr *msg_hdr);

/* Check the validity of an incoming STUN packet. Peforms several checks,
 * including the MESSAGE-INTEGRITY, if available.
 */
int stun_msg_verify(const stun_msg_hdr *msg_hdr, size_t msg_size);

/* Gets the attribute length (inner length, no padding) */
size_t stun_attr_len(const stun_attr_hdr *attr_hdr);

/* Gets the attribute block length (with padding) */
size_t stun_attr_block_len(const stun_attr_hdr *attr_hdr);

/* Gets the attribute type */
uint16_t stun_attr_type(const stun_attr_hdr *attr_hdr);

/* Iterates over the existing STUN message attributes. Passing a NULL
 * current attribute, you point to the first attribute.
 *
 * Returns the next STUN attribute, or NULL past the last one.
 */
const stun_attr_hdr *stun_msg_next_attr(const stun_msg_hdr *msg_hdr,
                                        const stun_attr_hdr *attr_hdr);

/* Finds a specific STUN attribute in the provided message. It will perform
 * a linear search over the available attributes.
 */
const stun_attr_hdr *stun_msg_find_attr(const stun_msg_hdr *msg_hdr,
                                        uint16_t type);

/* Reads a sockaddr attribute. Returns error case the address family
 * is unknown (should be STUN_IPV4 or STUN_IPV6).
 */
int stun_attr_sockaddr_read(const stun_attr_sockaddr *attr,
                            struct sockaddr *addr);

/* Reads a XOR'red sockaddr attribute. Returns error case the address family
 * is unknown (should be STUN_IPV4 or STUN_IPV6).
 */
int stun_attr_xor_sockaddr_read(const stun_attr_xor_sockaddr *attr,
                                const stun_msg_hdr *msg_hdr,
                                struct sockaddr *addr);

/* Reads a varsize attribute. The length is returned by stun_attr_len */
const void *stun_attr_varsize_read(const stun_attr_varsize *attr);

/* Reads an 8-bit attribute */
uint8_t stun_attr_uint8_read(const stun_attr_uint8 *attr);

/* Reads a 16-bit attribute */
uint16_t stun_attr_uint16_read(const stun_attr_uint16 *attr);

/* Reads a 32-bit attribute */
uint32_t stun_attr_uint32_read(const stun_attr_uint32 *attr);

/* Reads a 64-bit attribute */
uint64_t stun_attr_uint64_read(const stun_attr_uint64 *attr);

/* Gets the status code from the ERROR-CODE attribute */
int stun_attr_errcode_status(const stun_attr_errcode *attr);

/* Gets the reason phrase from the ERROR-CODE attribute */
const char *stun_attr_errcode_reason(const stun_attr_errcode *attr);

/* Gets the reason phrase length from the ERROR-CODE attribute */
size_t stun_attr_errcode_reason_len(const stun_attr_errcode *attr);

/* Gets the number of unknown attributes contained in a UNKNOWN-ATTRIBUTES
 * attribute.
 */
size_t stun_attr_unknown_count(const stun_attr_unknown *attr);

/* Gets the nth occurrence of unknown attributes.
 */
uint16_t stun_attr_unknown_get(const stun_attr_unknown *attr, size_t n);

/* Checks the MESSAGE-INTEGRITY attribute.
 */
int stun_attr_msgint_check(const stun_attr_msgint *msgint,
                           const stun_msg_hdr *msg_hdr,
                           const uint8_t *key, size_t key_len);

/* Calculates the key used for long term credentials for using with the
 * MESSAGE-INTEGRITY attribute; MD5(user:realm:pass).
 */
void stun_genkey(const void *username, size_t username_len,
                 const void *realm, size_t realm_len,
                 const void *password, size_t password_len,
                 uint8_t key[16]);

/* Checks the FINGERPRINT attribute.
 */
int stun_attr_fingerprint_check(const stun_attr_uint32 *fingerprint,
                                const stun_msg_hdr *msg_hdr);

#ifdef __cplusplus
};
#endif

#endif // __STUNMSG_H__
