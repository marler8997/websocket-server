#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Prevents need to type "struct sockaddr"
typedef struct sockaddr     sockaddr;
typedef struct sockaddr_in  sockaddr_in;
typedef struct sockaddr_in6 sockaddr_in6;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef union {
  sockaddr base;
  sockaddr_in ip4;
  sockaddr_in6 ip6;
} Addr;

// A pstring is a string that uses pointers for the
// start and end. It's a good string to use when parsing.
typedef struct {
  const char* ptr;
  const char* limit;
} pstring;
#define pstringEmpty(s) (s.ptr >= s.limit)
int pstringCmp(pstring s, const char* cmp);
#define pstringEqualsLiteral(s,lit) (((s.limit-s.ptr)==sizeof(lit)-1) && pstringCmp(s,lit) == 0)

const char* addrFamilyString(int af);
const char* sockTypeString(int type);
const char* sockProtocolString(int proto);

// Max ipv6 address is 39 chars, the max port is 6 chars (":65535") and need to add 1 for '\0'
#define MAX_ADDR_CHARS 46

int ipv6ToString(char* output, struct in6_addr addr);
int addrToString(char* output, const sockaddr* name);

// Returns: size of the string successfully parsed and the ip address in HOST ORDER
uint8 ipv4StringToAddr(pstring s, uint32* outIP);
// Returns: size of the string successfully parsed and the socket address in NETWORK ORDER
uint8 stringToAddr(pstring s, Addr* addr);

//#define MAX_IOCTL_ARG_STRING 16
//const char* GetIoctlInfo(long cmd, uint32 FAR* argp, char* argString, unsigned argStringSize);

#ifdef __cplusplus
}
#endif
