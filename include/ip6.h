#ifndef __IP6_H
#define __IP6_H

struct in6_addr {
	union {
		unsigned char	u6_addr8[16];
		unsigned short int  u6_addr16[8];
		unsigned int  u6_addr32[4];
	} u6_addr;	/* 128-bit IP6 address */

#define	s6_addr			n6_u.u6_addr8
#define	s6_addr16		in6_u.u6_addr16
#define	s6_addr32		in6_u.u6_addr32
};

#endif	/* ip */
