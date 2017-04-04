#ifndef __UDP_H
#define __UDP_H

#include "sock.h"

struct udp {
	unsigned short src;	/* source port */
	unsigned short dst;	/* destination port */
	unsigned short length;	/* udp head and data */
	unsigned short checksum;
	unsigned char data[0];
} __attribute__((packed));

struct udp_sock {
	struct sock sk;
	unsigned int family;
};

#define UDP_HRD_SZ sizeof(struct udp)
#define ip2udp(ip) ((struct udp *)ipdata(ip))
#define UDP_MAX_BUFSZ (0xffff - UDP_HRD_SZ)
#define UDP_DEFAULT_TTL 64

extern struct sock *udp_lookup_sock(unsigned short port);
extern void udp_in(struct pkbuf *pkb);
extern void udp_init(void);
extern struct sock *udp_alloc_sock(unsigned int, int);

#endif	/* udp.h */
