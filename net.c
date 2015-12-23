#include <stdio.h>

#include "net.h"

int pstringCmp(pstring s, const char* cmp) {
  return memcmp(s.ptr, cmp, s.limit-s.ptr);
}

const char* addrFamilyString(int af)
{
  switch(af) {
  case AF_UNSPEC:   return "AF_UNSPEC";
  case AF_INET:     return "AF_INET";
  case AF_INET6:    return "AF_INET6";
  default:          return "UNKNOWN";
  }
}
const char* sockTypeString(type)
{
  switch(type) {
  case SOCK_STREAM: return "SOCK_STREAM";
  case SOCK_DGRAM:  return "SOCK_DGRAM";
  default:          return "UNKNOWN";
  }
}
const char* sockProtocolString(proto)
{
  switch(proto) {
  case IPPROTO_TCP: return "TCP";
  case IPPROTO_UDP: return "UDP";
  default:          return "UNKNOWN";
  }
}

// Returns: number of chars written
int ipv6ToString(char* output, struct in6_addr addr)
{
  return sprintf(output, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                 addr.s6_addr[0] , addr.s6_addr[1] , addr.s6_addr[2] , addr.s6_addr[3],
                 addr.s6_addr[4] , addr.s6_addr[5] , addr.s6_addr[6] , addr.s6_addr[7],
                 addr.s6_addr[8] , addr.s6_addr[9] , addr.s6_addr[10], addr.s6_addr[11],
                 addr.s6_addr[12], addr.s6_addr[13], addr.s6_addr[14], addr.s6_addr[15]);
}

// Convert sockaddr to string
// Returns: number of chars written
// Assumption: CheckAddr has already been used to verify the address
// Assumption: length of output is >= MAX_ADDR_CHAR_LENGTH
int addrToString(char* output, const sockaddr* name)
{
  if(name->sa_family == AF_INET) {

    uint32 ip   = ntohl(((const sockaddr_in*)name)->sin_addr.s_addr);
    uint16 port = ntohs(((const sockaddr_in*)name)->sin_port);
    return sprintf(output, "%u.%u.%u.%u:%u",
                   ip >> 24, 0xFF & (ip >> 16), 0xFF & (ip >> 8), 0xFF & ip, port);

  } else if(name->sa_family == AF_INET6) {

    int addrCharLength = ipv6ToString(output, ((const sockaddr_in6*)name)->sin6_addr);
    return addrCharLength + sprintf(output + addrCharLength, ":%u", ntohs(((const sockaddr_in6*)name)->sin6_port));

  } else {
    return sprintf(output, "UnknownAddress(family=%d)", name->sa_family);
  }
}

// Returns: size of the string successfully parsed
uint8 ipv4StringToAddr(pstring s, uint32* outIP)
{
  uint32 ip = 0;
  uint8 shift = 24;
  const char* p = s.ptr;

  while(1) {
    uint32 bval;
    char c;

    if(p >= s.limit) {
      return 0; // Fail (input ended too early)
    }

    // Parse one of the ip numbers
    c = *p;
    if(c > '9' || c < '0') {
      return 0;  // Fail (found invalid character)
    }

    bval = c - '0';
    while(1) {
      p++;
      if(p >= s.limit)
    break;
      c = *p;
      if(c > '9' || c < '0') {
    break;
      }
      bval *= 10;
      bval += c - '0';
      if(bval > 255) {
    return 0; // Fail (one of the numbers in the ip is too large)
      }
    }

    // Add the number to the ip and return if done
    ip |= (bval << shift);
    if(shift == 0) {
      *outIP = ip;
      return p - s.ptr;
    }

    if(c != '.') {
      return 0; // Fail (ip byte num not separated by '.')
    }
    p++; // Skip the '.'
    shift -= 8;
  }
}

// Returns: size of the string successfully parsed
uint8 stringToAddr(pstring s, Addr* addr)
{
  uint8 len;
  uint32 ipv4Addr;

  // First try to parse it as an ipv4
  len = ipv4StringToAddr(s, &ipv4Addr);
  if(len > 0) {
    addr->ip4.sin_family      = AF_INET;
    addr->ip4.sin_addr.s_addr = htonl(ipv4Addr);
    return len;
  }

  // IPv6 not implemented
  return 0;
}

/*
const char* GetIoctlInfo(long cmd, uint32 FAR* argp, char* argString, unsigned argStringSize)
{
  switch(cmd) {
  case FIONBIO:
    _snprintf(argString, argStringSize, "%lu", *argp);
    return "FIONBIO";
  case FIONREAD:
    sprintf(argString, "?");
    return "FIONREAD";
  case HP_WS2_SHIM_IOCTL_GET_STACK:
    argString[0] = 0;
    return "HP_WS2_SHIM_IOCTL_GET_STACK";
  default:
    sprintf(argString, "?");
    return "UNKNOWN";
  }
}
*/
