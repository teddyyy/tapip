#ifndef __IP6_H
#define __IP6_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define INET6_ADDRSTRLEN	46

struct in6_addr {
	union {
		unsigned char	u6_addr8[16];
		unsigned short int  u6_addr16[8];
		unsigned int  u6_addr32[4];
	} u6_addr;	/* 128-bit IP6 address */

#define	s6_addr			u6_addr.u6_addr8
#define	s6_addr16		u6_addr.u6_addr16
#define	s6_addr32		u6_addr.u6_addr32
};

static inline int _isxdigit(int ch)
{
	if (ch >= '0' && ch <= '9')
		return true;

	if (ch >= 'a' && ch <= 'f')
		return true;

	if (ch >= 'A' && ch <= 'F')
		return true;

	return 0;
}

static inline int hexval(unsigned ch)
{
	if (ch - '0' < 10) return ch - '0';
	ch |= 32;
	if (ch - 'a' < 6) return ch - 'a' + 10;

	return -1;
}

extern int inet_pton(const char *src, void *dst);
extern const char *inet_ntop(const void *cp, char *buf, size_t len);

#endif	/* ip */
