#include <endian.h>
#include <typeinfo>
#include <iostream>
#include <strings.h>

#include <NetAddress.h>

moxie::NetAddress::NetAddress(ushort sa_family, int sa_port, const char *sa_ip):
    family_(sa_family),
    ip_(sa_ip),
    port_(sa_port) {
    if (family_ == AF_INET) {
        ::bzero(&addr_, sizeof(struct sockaddr_in));
        addr_.sin_family = family_;
        addr_.sin_port = htobe16(sa_port);
        ::inet_pton(family_, sa_ip, &(addr_.sin_addr));
    } else {
        ::bzero(&addr6_, sizeof(struct sockaddr_in6));
        addr6_.sin6_family = family_;
        addr6_.sin6_port = htobe16(sa_port);
        ::inet_pton(family_, sa_ip, &(addr6_.sin6_addr));
    }
}

const std::string& moxie::NetAddress::getIp() const {
    return ip_;
}

int moxie::NetAddress::getPort() const {
    return port_;
}

struct sockaddr *moxie::NetAddress::addrPtr() {
    if (family_ == AF_INET) {
        return static_cast<struct sockaddr *>(static_cast<void*>(&addr_));
    } else if (family_ == AF_INET6) {
        return static_cast<struct sockaddr *>(static_cast<void*>(&addr6_));
    }
    return static_cast<struct sockaddr *>(static_cast<void*>(&addr6_));
}

const struct sockaddr *moxie::NetAddress::addrPtr() const {
    if (family_ == AF_INET) {
        return static_cast<const struct sockaddr *>(static_cast<const void*>(&addr_));
    } else if (family_ == AF_INET6) {
        return static_cast<const struct sockaddr *>(static_cast<const void*>(&addr6_));
    }
    return static_cast<const struct sockaddr *>(static_cast<const void*>(&addr6_));
}

socklen_t moxie::NetAddress::addrLen() const{
    if (family_ == AF_INET) {
        return sizeof(addr_);
    } else if (family_ == AF_INET6) {
        return sizeof(addr6_);
    }
    return sizeof(addr6_);
}

