// Copyright (c) 2014 Guilherme Balena Versiani.
//
// I dedicate any and all copyright interest in this software to the
// public domain. I make this dedication for the benefit of the public at
// large and to the detriment of my heirs and successors. I intend this
// dedication to be an overt act of relinquishment in perpetuity of all
// present and future rights to this software under copyright law.

#ifndef STUNXX_MESSAGE_H_
#define STUNXX_MESSAGE_H_

#include <cstring>
#include <modules/p2p/stuns/msg.h>
#include <string>
#include <vector>

// Forward sockaddr declarations;
struct sockaddr_in;
struct sockaddr_in6;

namespace stun {
namespace attribute {
namespace type {

enum attribute_type {
#define STUNXX_ATTRIBUTE_DEF(d, a) a = d,
#include <modules/p2p/stuns/attributes_template.h>
#undef STUNXX_ATTRIBUTE_DEF
};

} // namespace type

namespace decoding_bits {

template <typename AttributeType> class attribute_base {
public:
    attribute_base(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : msg_hdr_(msg_hdr)
        , attr_(reinterpret_cast<const AttributeType*>(attr_hdr)) {}

protected:
    const stun_msg_hdr* msg_hdr_;
    const AttributeType* attr_;
};

class empty : public attribute_base<stun_attr_hdr> {
public:
    empty(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    bool exists() const { return true; }
};

class socket_address : public attribute_base<stun_attr_sockaddr> {
public:
    socket_address(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    bool to_sockaddr(sockaddr* addr) {
        return stun_attr_sockaddr_read(attr_, addr) == STUN_OK ? true : false;
    }
};

class xor_socket_address : public attribute_base<stun_attr_xor_sockaddr> {
public:
    xor_socket_address(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    bool to_sockaddr(sockaddr* addr) {
        return stun_attr_xor_sockaddr_read(attr_, msg_hdr_, addr) == STUN_OK ? true : false;
    }
};

class string : public attribute_base<stun_attr_varsize> {
public:
    string(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    std::string to_string() const {
        return std::string(reinterpret_cast<const char*>(stun_attr_varsize_read(attr_)),
                           stun_attr_len(&attr_->hdr));
    }
};

class data_type : public attribute_base<stun_attr_varsize> {
public:
    data_type(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    size_t size() const { return stun_attr_len(&attr_->hdr); }
    const uint8_t* data() const {
        return reinterpret_cast<const uint8_t*>(attr_) + sizeof(stun_attr_hdr);
    }
};

class u8 : public attribute_base<stun_attr_uint8> {
public:
    u8(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    uint8_t value() const { return stun_attr_uint8_read(attr_); }
};

class u16 : public attribute_base<stun_attr_uint16> {
public:
    u16(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    uint16_t value() const { return stun_attr_uint16_read(attr_); }
};

class u32 : public attribute_base<stun_attr_uint32> {
public:
    u32(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    uint32_t value() const { return stun_attr_uint32_read(attr_); }
};

class u64 : public attribute_base<stun_attr_uint64> {
public:
    u64(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    uint64_t value() const { return stun_attr_uint64_read(attr_); }
};

class errcode : public attribute_base<stun_attr_errcode> {
public:
    errcode(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    int status_code() { return stun_attr_errcode_status(attr_); }
    std::string reason_phrase() const {
        const char* str = stun_attr_errcode_reason(attr_);
        size_t str_len = stun_attr_errcode_reason_len(attr_);
        return std::string(str, str_len);
    }
};

class unknown : public attribute_base<stun_attr_unknown> {
public:
    unknown(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    size_t size() const { return stun_attr_unknown_count(attr_); }
    uint16_t operator[](size_t n) const { return stun_attr_unknown_get(attr_, n); }
};

class msgint : public attribute_base<stun_attr_msgint> {
public:
    msgint(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    template <typename char_type>
    bool check_integrity(const char_type* key_begin, const char_type* key_end) const {
        return stun_attr_msgint_check(attr_, msg_hdr_, reinterpret_cast<const uint8_t*>(key_begin),
                                      key_end - key_begin)
                   ? true
                   : false;
    }
    bool check_integrity(const std::string& key) const {
        return check_integrity(key.data(), key.data() + key.size());
    }
};

class fingerprint : public attribute_base<stun_attr_uint32> {
public:
    fingerprint(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : attribute_base(msg_hdr, attr_hdr) {}
    bool check_integrity() const {
        return stun_attr_fingerprint_check(attr_, msg_hdr_) == 0 ? false : true;
    }
};

} // namespace decoding_bits

namespace decoding {

#define STUNXX_ATTRIBUTE_SPECIAL(x, name)
#define STUNXX_ATTRIBUTE_EMPTY(x, name) typedef decoding_bits::empty name;
#define STUNXX_ATTRIBUTE_STRING(x, name) typedef decoding_bits::string name;
#define STUNXX_ATTRIBUTE_DATA(x, name) typedef decoding_bits::data_type name;
#define STUNXX_ATTRIBUTE_SOCKADDR(x, name) typedef decoding_bits::socket_address name;
#define STUNXX_ATTRIBUTE_XOR_SOCKADDR(x, name) typedef decoding_bits::xor_socket_address name;
#define STUNXX_ATTRIBUTE_UINT8(x, name) typedef decoding_bits::u8 name;
#define STUNXX_ATTRIBUTE_UINT8_PAD(x, name) typedef decoding_bits::u8 name;
#define STUNXX_ATTRIBUTE_UINT16(x, name) typedef decoding_bits::u16 name;
#define STUNXX_ATTRIBUTE_UINT16_PAD(x, name) typedef decoding_bits::u16 name;
#define STUNXX_ATTRIBUTE_UINT32(x, name) typedef decoding_bits::u32 name;
#define STUNXX_ATTRIBUTE_UINT64(x, name) typedef decoding_bits::u64 name;
#include <modules/p2p/stuns/attributes_template.h>
#undef STUNXX_ATTRIBUTE_SPECIAL
#undef STUNXX_ATTRIBUTE_EMPTY
#undef STUNXX_ATTRIBUTE_STRING
#undef STUNXX_ATTRIBUTE_DATA
#undef STUNXX_ATTRIBUTE_SOCKADDR
#undef STUNXX_ATTRIBUTE_XOR_SOCKADDR
#undef STUNXX_ATTRIBUTE_UINT8
#undef STUNXX_ATTRIBUTE_UINT8_PAD
#undef STUNXX_ATTRIBUTE_UINT16
#undef STUNXX_ATTRIBUTE_UINT16_PAD
#undef STUNXX_ATTRIBUTE_UINT32
#undef STUNXX_ATTRIBUTE_UINT64

// Special types are defined below:
typedef decoding_bits::msgint message_integrity;
typedef decoding_bits::errcode error_code;
typedef decoding_bits::unknown unknown_attributes;
typedef decoding_bits::fingerprint fingerprint;

} // namespace decoding

namespace decoding_bits {

template <type::attribute_type> struct traits;

#define STUNXX_ATTRIBUTE_DEF(d, name)                                                              \
    template <> struct traits<type::name> {                                                        \
        typedef decoding::name decoding_type;                                                      \
    };
#include <modules/p2p/stuns/attributes_template.h>
#undef STUNXX_ATTRIBUTE_DEF

} // namespace decoding_bits

class decoded {
public:
    decoded(const stun_msg_hdr* msg_hdr, const stun_attr_hdr* attr_hdr)
        : msg_hdr_(msg_hdr)
        , attr_hdr_(attr_hdr) {}

    uint16_t type() const { return stun_attr_type(attr_hdr_); }

    decoded next() const { return decoded(msg_hdr_, stun_msg_next_attr(msg_hdr_, attr_hdr_)); }

    const stun_msg_hdr* msg_hdr() const { return msg_hdr_; }

    const stun_attr_hdr* attr_ptr() const { return attr_hdr_; }

    template <type::attribute_type T> typename decoding_bits::traits<T>::decoding_type to() {
        return typename decoding_bits::traits<T>::decoding_type(msg_hdr_, attr_hdr_);
    }

private:
    const stun_msg_hdr* msg_hdr_;
    const stun_attr_hdr* attr_hdr_;
};

namespace bits {

struct empty {
    empty(uint16_t type)
        : type_(type) {}
    size_t size() const { return sizeof(stun_attr_hdr); }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_empty_add(msg_hdr, type_); }
    uint16_t type_;
};

struct socket_address {
    socket_address(uint16_t type, const sockaddr* addr)
        : type_(type)
        , addr_(addr) {}
    size_t size() const {
        return STUN_ATTR_SOCKADDR_SIZE(STUN_IPV6); // worst case
    }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_sockaddr_add(msg_hdr, type_, addr_); }
    uint16_t type_;
    const sockaddr* addr_;
};

struct xor_socket_address {
    xor_socket_address(uint16_t type, const sockaddr* addr)
        : type_(type)
        , addr_(addr) {}
    size_t size() const { return STUN_ATTR_SOCKADDR_SIZE(STUN_IPV6); }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_xor_sockaddr_add(msg_hdr, type_, addr_); }
    uint16_t type_;
    const sockaddr* addr_;
};

template <typename char_type> struct varsize {
    varsize(uint16_t type, const char_type* begin, const char_type* end, uint8_t pad)
        : type_(type)
        , begin_(begin)
        , end_(end)
        , pad_(pad) {}
    size_t size() const { return STUN_ATTR_VARSIZE_SIZE(end_ - begin_); }
    void append(stun_msg_hdr* msg_hdr) const {
        stun_attr_varsize_add(msg_hdr, type_, reinterpret_cast<const uint8_t*>(begin_),
                              end_ - begin_, pad_);
    }
    uint16_t type_;
    const char_type* begin_;
    const char_type* end_;
    uint8_t pad_;
};

struct u8 {
    u8(uint16_t type, uint8_t value)
        : type_(type)
        , value_(value) {}
    size_t size() const { return STUN_ATTR_UINT8_SIZE; }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_uint8_add(msg_hdr, type_, value_); }
    uint16_t type_;
    uint8_t value_;
};

struct u8_pad {
    u8_pad(uint16_t type, uint8_t value, uint8_t pad)
        : type_(type)
        , value_(value)
        , pad_(pad) {}
    size_t size() const { return STUN_ATTR_UINT8_SIZE; }
    void append(stun_msg_hdr* msg_hdr) const {
        stun_attr_uint8_pad_add(msg_hdr, type_, value_, pad_);
    }
    uint16_t type_;
    uint8_t value_;
    uint8_t pad_;
};

struct u16 {
    u16(uint16_t type, uint16_t value)
        : type_(type)
        , value_(value) {}
    size_t size() const { return STUN_ATTR_UINT16_SIZE; }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_uint16_add(msg_hdr, type_, value_); }
    uint16_t type_;
    uint16_t value_;
};

struct u16_pad {
    u16_pad(uint16_t type, uint16_t value, uint8_t pad)
        : type_(type)
        , value_(value)
        , pad_(pad) {}
    size_t size() const { return STUN_ATTR_UINT16_SIZE; }
    void append(stun_msg_hdr* msg_hdr) const {
        stun_attr_uint16_pad_add(msg_hdr, type_, value_, pad_);
    }
    uint16_t type_;
    uint16_t value_;
    uint8_t pad_;
};

struct u32 {
    u32(uint16_t type, uint32_t value)
        : type_(type)
        , value_(value) {}
    size_t size() const { return STUN_ATTR_UINT32_SIZE; }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_uint32_add(msg_hdr, type_, value_); }
    uint16_t type_;
    uint32_t value_;
};

struct u64 {
    u64(uint16_t type, uint64_t value)
        : type_(type)
        , value_(value) {}
    size_t size() const { return STUN_ATTR_UINT64_SIZE; }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_uint64_add(msg_hdr, type_, value_); }
    uint16_t type_;
    uint64_t value_;
};

struct errcode {
    errcode(int status_code, const char* reason, uint8_t pad)
        : status_code_(status_code)
        , reason_(reason)
        , pad_(pad) {}
    size_t size() const { return STUN_ATTR_ERROR_CODE_SIZE(strlen(reason_)); }
    void append(stun_msg_hdr* msg_hdr) const {
        stun_attr_errcode_add(msg_hdr, status_code_, reason_, pad_);
    }
    int status_code_;
    const char* reason_;
    uint8_t pad_;
};

struct unknown {
    unknown(const uint16_t* begin, const uint16_t* end, uint8_t pad)
        : begin_(begin)
        , end_(end)
        , pad_(pad) {}
    size_t size() const { return STUN_ATTR_UNKNOWN_SIZE(end_ - begin_); }
    void append(stun_msg_hdr* msg_hdr) const {
        stun_attr_unknown_add(msg_hdr, begin_, end_ - begin_, pad_);
    }
    const uint16_t* begin_;
    const uint16_t* end_;
    uint8_t pad_;
};

struct msgint {
    msgint(const uint8_t* key, size_t key_len)
        : key_(key)
        , key_len_(key_len) {}
    size_t size() const { return STUN_ATTR_MSGINT_SIZE; }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_msgint_add(msg_hdr, key_, key_len_); }
    const uint8_t* key_;
    size_t key_len_;
};

struct fingerprint {
    fingerprint() {}
    size_t size() const { return STUN_ATTR_FINGERPRINT_SIZE; }
    void append(stun_msg_hdr* msg_hdr) const { stun_attr_fingerprint_add(msg_hdr); }
};

} // namespace bits

#define STUNXX_ATTRIBUTE_SPECIAL(x, name) // defined below

#define STUNXX_ATTRIBUTE_STRING(x, name)                                                           \
    bits::varsize<char> name(const char* data, uint8_t pad = 0) {                                  \
        return bits::varsize<char>(attribute::type::name, data, data + strlen(data), pad);         \
    }                                                                                              \
    bits::varsize<char> name(const std::string& s, uint8_t pad = 0) {                              \
        return bits::varsize<char>(attribute::type::name, s.data(), s.data() + s.size(), pad);     \
    }                                                                                              \
    bits::varsize<char> name(const char* begin, const char* end, uint8_t pad = 0) {                \
        return bits::varsize<char>(attribute::type::name, begin, end, pad);                        \
    }

#define STUNXX_ATTRIBUTE_DATA(x, name)                                                             \
    bits::varsize<uint8_t> name(const uint8_t* data, size_t data_len, uint8_t pad = 0) {           \
        return bits::varsize<uint8_t>(attribute::type::name, data, data + data_len, pad);          \
    }                                                                                              \
    bits::varsize<uint8_t> name(const uint8_t* begin, const uint8_t* end, uint8_t pad = 0) {       \
        return bits::varsize<uint8_t>(attribute::type::name, begin, end, pad);                     \
    }

#define STUNXX_ATTRIBUTE_SOCKADDR(x, name)                                                         \
    bits::socket_address name(const sockaddr* addr) {                                              \
        return bits::socket_address(attribute::type::name, addr);                                  \
    }                                                                                              \
    bits::socket_address name(const sockaddr& addr) {                                              \
        return bits::socket_address(attribute::type::name, &addr);                                 \
    }                                                                                              \
    bits::socket_address name(const sockaddr_in* addr) {                                           \
        return bits::socket_address(attribute::type::name, (sockaddr*)addr);                       \
    }                                                                                              \
    bits::socket_address name(const sockaddr_in& addr) {                                           \
        return bits::socket_address(attribute::type::name, (sockaddr*)&addr);                      \
    }                                                                                              \
    bits::socket_address name(const sockaddr_in6* addr) {                                          \
        return bits::socket_address(attribute::type::name, (sockaddr*)addr);                       \
    }                                                                                              \
    bits::socket_address name(const sockaddr_in6& addr) {                                          \
        return bits::socket_address(attribute::type::name, (sockaddr*)&addr);                      \
    }

#define STUNXX_ATTRIBUTE_XOR_SOCKADDR(x, name)                                                     \
    bits::xor_socket_address name(const sockaddr* addr) {                                          \
        return bits::xor_socket_address(attribute::type::name, addr);                              \
    }                                                                                              \
    bits::xor_socket_address name(const sockaddr& addr) {                                          \
        return bits::xor_socket_address(attribute::type::name, &addr);                             \
    }                                                                                              \
    bits::xor_socket_address name(const sockaddr_in* addr) {                                       \
        return bits::xor_socket_address(attribute::type::name, (sockaddr*)addr);                   \
    }                                                                                              \
    bits::xor_socket_address name(const sockaddr_in& addr) {                                       \
        return bits::xor_socket_address(attribute::type::name, (sockaddr*)&addr);                  \
    }                                                                                              \
    bits::xor_socket_address name(const sockaddr_in6* addr) {                                      \
        return bits::xor_socket_address(attribute::type::name, (sockaddr*)addr);                   \
    }                                                                                              \
    bits::xor_socket_address name(const sockaddr_in6& addr) {                                      \
        return bits::xor_socket_address(attribute::type::name, (sockaddr*)&addr);                  \
    }

#define STUNXX_ATTRIBUTE_EMPTY(x, name)                                                            \
    bits::empty name() {                                                                           \
        return bits::empty(attribute::type::name);                                                 \
    }

#define STUNXX_ATTRIBUTE_UINT8(x, name)                                                            \
    bits::u8 name(uint8_t value) {                                                                 \
        return bits::u8(attribute::type::name, value);                                             \
    }

#define STUNXX_ATTRIBUTE_UINT8_PAD(x, name)                                                        \
    bits::u8_pad name(uint8_t value, uint8_t pad = 0) {                                            \
        return bits::u8_pad(attribute::type::name, value, pad);                                    \
    }

#define STUNXX_ATTRIBUTE_UINT16(x, name)                                                           \
    bits::u16 name(uint16_t value) {                                                               \
        return bits::u16(attribute::type::name, value);                                            \
    }

#define STUNXX_ATTRIBUTE_UINT16_PAD(x, name)                                                       \
    bits::u16_pad name(uint16_t value, uint8_t pad = 0) {                                          \
        return bits::u16_pad(attribute::type::name, value, pad);                                   \
    }

#define STUNXX_ATTRIBUTE_UINT32(x, name)                                                           \
    bits::u32 name(uint32_t value) {                                                               \
        return bits::u32(attribute::type::name, value);                                            \
    }

#define STUNXX_ATTRIBUTE_UINT64(x, name)                                                           \
    bits::u64 name(uint64_t value) {                                                               \
        return bits::u64(attribute::type::name, value);                                            \
    }

#include <modules/p2p/stuns/attributes_template.h>

#undef STUNXX_ATTRIBUTE_SPECIAL
#undef STUNXX_ATTRIBUTE_STRING
#undef STUNXX_ATTRIBUTE_DATA
#undef STUNXX_ATTRIBUTE_SOCKADDR
#undef STUNXX_ATTRIBUTE_XOR_SOCKADDR
#undef STUNXX_ATTRIBUTE_EMPTY
#undef STUNXX_ATTRIBUTE_UINT8
#undef STUNXX_ATTRIBUTE_UINT8_PAD
#undef STUNXX_ATTRIBUTE_UINT16
#undef STUNXX_ATTRIBUTE_UINT16_PAD
#undef STUNXX_ATTRIBUTE_UINT32
#undef STUNXX_ATTRIBUTE_UINT64

// Special attributes define below:

bits::errcode error_code(int status_code, const char* reason, uint8_t pad = 0) {
    return bits::errcode(status_code, reason, pad);
}

bits::unknown unknown_attributes(const uint16_t* data, size_t count, uint8_t pad = 0) {
    return bits::unknown(data, data + count, pad);
}

bits::unknown unknown_attributes(const uint16_t* begin, const uint16_t* end, uint8_t pad = 0) {
    return bits::unknown(begin, end, pad);
}

bits::msgint message_integrity(const char* key) {
    return bits::msgint(reinterpret_cast<const uint8_t*>(key), strlen(key));
}

bits::msgint message_integrity(const std::string& key) {
    return bits::msgint(reinterpret_cast<const uint8_t*>(key.c_str()), key.size());
}

bits::msgint message_integrity(const uint8_t* key, size_t key_len) {
    return bits::msgint(key, key_len);
}

bits::fingerprint fingerprint() {
    return bits::fingerprint();
}

} // namespace attribute

template <class Allocator> class base_message {
public:
    class iterator {
    public:
        typedef iterator self_type;
        typedef size_t difference_type;
        typedef size_t size_type;
        typedef attribute::decoded value_type;
        typedef attribute::decoded* pointer;
        typedef attribute::decoded& reference;
        typedef std::forward_iterator_tag iterator_category;

        iterator(const stun_msg_hdr* msg_hdr, const uint8_t* ptr)
            : attr_(msg_hdr, reinterpret_cast<const stun_attr_hdr*>(ptr)) {}

        self_type operator++() {
            attr_ = attr_.next();
            return *this;
        }

        self_type operator++(int) {
            self_type it = *this;
            attr_ = attr_.next();
            return it;
        }

        const reference operator*() { return attr_; }
        pointer operator->() { return &attr_; }

        bool operator==(const self_type& rhs) { return attr_.attr_ptr() == rhs.attr_.attr_ptr(); }
        bool operator!=(const self_type& rhs) { return attr_.attr_ptr() != rhs.attr_.attr_ptr(); }

    private:
        attribute::decoded attr_;
    };

    enum type {
        binding_request = STUN_BINDING_REQUEST,
        binding_response = STUN_BINDING_RESPONSE,
        binding_error_response = STUN_BINDING_ERROR_RESPONSE,
        binding_indication = STUN_BINDING_INDICATION,
        shared_secret_request = STUN_SHARED_SECRET_REQUEST,
        shared_secret_response = STUN_SHARED_SECRET_RESPONSE,
        shared_secret_error_response = STUN_SHARED_SECRET_ERROR_RESPONSE,
        allocate_request = STUN_ALLOCATE_REQUEST,
        allocate_response = STUN_ALLOCATE_RESPONSE,
        allocate_error_response = STUN_ALLOCATE_ERROR_RESPONSE,
        refresh_request = STUN_REFRESH_REQUEST,
        refresh_response = STUN_REFRESH_RESPONSE,
        refresh_error_response = STUN_REFRESH_ERROR_RESPONSE,
        send_indication = STUN_SEND_INDICATION,
        data_indication = STUN_DATA_INDICATION,
        create_perm_request = STUN_CREATE_PERM_REQUEST,
        create_perm_response = STUN_CREATE_PERM_RESPONSE,
        create_perm_error_response = STUN_CREATE_PERM_ERROR_RESPONSE,
        channel_bind_request = STUN_CHANNEL_BIND_REQUEST,
        channel_bind_response = STUN_CHANNEL_BIND_RESPONSE,
        channel_bind_error_response = STUN_CHANNEL_BIND_ERROR_RESPONSE,
        connect_request = STUN_CONNECT_REQUEST,
        connect_response = STUN_CONNECT_RESPONSE,
        connect_error_response = STUN_CONNECT_ERROR_RESPONSE,
        connection_bind_request = STUN_CONNECTION_BIND_REQUEST,
        connection_bind_response = STUN_CONNECTION_BIND_RESPONSE,
        connection_bind_error_response = STUN_CONNECTION_BIND_ERROR_RESPONSE,
        connection_attempt_request = STUN_CONNECTION_ATTEMPT_REQUEST,
        connection_attempt_response = STUN_CONNECTION_ATTEMPT_RESPONSE,
        connection_attempt_error_response = STUN_CONNECTION_ATTEMPT_ERROR_RESPONSE,
    };

    static const size_t header_size = sizeof(stun_msg_hdr);

    base_message()
        : buffer_(header_size, 0) {}

    base_message(size_t n)
        : buffer_(n < header_size ? header_size : n, 0) {}

    base_message(const base_message& msg)
        : buffer_(msg.buffer_) {}

    base_message(const uint8_t* buf, size_t buf_len)
        : buffer_(buf, buf_len) {}

    template <class InputIterator>
    base_message(InputIterator first, InputIterator last)
        : buffer_(first, last) {}

    base_message(uint16_t type, const uint8_t tsx_id[12])
        : buffer_(header_size, 0) {
        stun_msg_hdr_init(hdr(), type, tsx_id);
    }

    ~base_message() {}

    void resize(size_t size) { buffer_.resize(size, 0); }
    size_t capacity() const { return buffer_.size(); }

    uint8_t* data() { return buffer_.data(); }
    const uint8_t* data() const { return buffer_.data(); }

    std::vector<uint8_t> id() const {
        std::vector<uint8_t> tid(12);
        memcpy(tid.data(), hdr()->tsx_id, 12);
        return tid;
    }

    size_t size() const { return stun_msg_len(hdr()); }

    bool verify() const { return stun_msg_verify(hdr(), capacity()) == 0 ? false : true; }

    uint16_t type() const { return stun_msg_type(hdr()); }

    template <typename AttributeType> void push_back(const AttributeType& attr) {
        buffer_.resize(size() + attr.size());
        attr.append(reinterpret_cast<stun_msg_hdr*>(buffer_.data()));
    }

    iterator begin() const {
        return iterator(hdr(), reinterpret_cast<const uint8_t*>(stun_msg_next_attr(hdr(), NULL)));
    }

    iterator end() const { return iterator(hdr(), NULL); }

private:
    std::vector<uint8_t, Allocator> buffer_;

    stun_msg_hdr* hdr() { return reinterpret_cast<stun_msg_hdr*>(buffer_.data()); }
    const stun_msg_hdr* hdr() const {
        return reinterpret_cast<const stun_msg_hdr*>(buffer_.data());
    }
};

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::empty& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::socket_address& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::xor_socket_address& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::varsize<char>& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::varsize<uint8_t>& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg, const attribute::bits::u8& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::u8_pad& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::u16& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::u16_pad& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::u32& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::u64& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::errcode& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::unknown& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::msgint& attr) {
    msg.push_back(attr);
    return msg;
}

template <class Allocator>
base_message<Allocator>& operator<<(base_message<Allocator>& msg,
                                    const attribute::bits::fingerprint& attr) {
    msg.push_back(attr);
    return msg;
}

typedef base_message<std::allocator<uint8_t>> message;

class message_piece {
public:
    typedef message::iterator iterator;

    message_piece()
        : ptr_(NULL)
        , length_(0) {}

    message_piece(const message& msg)
        : ptr_(msg.data())
        , length_(msg.size()) {}

    message_piece(const uint8_t* buf, size_t buf_len)
        : ptr_(buf)
        , length_(buf_len) {}

    template <class InputIterator>
    message_piece(InputIterator first, InputIterator last)
        : ptr_((last > first) ? &(*first) : NULL)
        , length_((last > first) ? (size_t)(last - first) : 0) {}

    ~message_piece() {}

    const uint8_t* data() const { return ptr_; }
    size_t size() const { return length_; }

    bool verify() const { return stun_msg_verify(hdr(), length_) == 0 ? false : true; }

    uint16_t type() const { return stun_msg_type(hdr()); }

    iterator begin() const {
        return iterator(hdr(), reinterpret_cast<const uint8_t*>(stun_msg_next_attr(hdr(), NULL)));
    }

    iterator end() const { return iterator(hdr(), NULL); }

private:
    const uint8_t* ptr_;
    size_t length_;

    const stun_msg_hdr* hdr() const { return reinterpret_cast<const stun_msg_hdr*>(ptr_); }
};

} // namespace stun

#endif // STUNXX_MESSAGE_H_
