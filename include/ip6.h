#ifndef __IP6_H
#define __IP6_H

#include <string.h>
#include <stdbool.h>

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

	return (ch >= 'A') && (ch <= 'F');
}

static inline int hexval(int ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	else
		return -1;
}

extern int inet_pton(const char *src, void *dst);

#endif	/* ip */
