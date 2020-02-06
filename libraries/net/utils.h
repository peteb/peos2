// -*- c++ -*-

#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stddef.h>
#include <stdint.h>

#include <support/format.h>
#include <support/string.h>

p2::string<32> hwaddr_str(const uint8_t *octets);
int read(int fd, char *buf, size_t length);
p2::string<16> ipaddr_str(uint32_t octets);
uint32_t parse_ipaddr(const char *str);
uint64_t sum_words(const char *data, size_t length);

inline uint16_t ntohs(uint16_t net_bytes)
{
  return __builtin_bswap16(net_bytes);
}

inline uint32_t ntohl(uint32_t net_bytes)
{
  return __builtin_bswap32(net_bytes);
}

inline uint16_t htons(uint16_t host_bytes)
{
  return ntohs(host_bytes);
}

inline uint32_t htonl(uint32_t host_bytes)
{
  return ntohl(host_bytes);
}

#endif // !NET_UTILS_H
