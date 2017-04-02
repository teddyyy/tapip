#include "netif.h"
#include "lib.h"
#include "ip6.h"

int inet_pton(const char *src, void *dst)
{
	unsigned short int ip[8];
	unsigned char *p = dst;
	int i, j, v, d, brk =- 1;

	if (*src == ':' && *++src != ':')
		return 0;

	for (i = 0; ; i++) {
		if (src[0] == ':' && brk < 0) {
			brk = i;
			ip[i&7] = 0;

			if (!*++src) break;
			if (i == 7)  return 0;

			continue;
		}

		for (v = j = 0; j < 4 && (d = hexval(src[j])) >= 0; j++)
			v = 16 * v + d;

		if (j == 0) return 0;

		ip[i&7] = v;
		if (!src[j] && (brk >= 0 || i == 7)) break;
		if (i == 7) return 0;

		if (src[j] != ':') {
			if (src[j] != '.' || (i < 6 && brk < 0))
				return 0;
			i++;
			break;
		}
		src += j + 1;
	}

	if (brk >= 0) {
		memmove(ip + brk + 7 - i, ip + brk, 2 *(i + 1 - brk));
		for (j = 0; j < 7 - i; j++)
			ip[brk + j] = 0;
	}

	for (j = 0; j < 8; j++) {
		*p++ = ip[j] >> 8;
		*p++ = ip[j];
	}

	return 1;
}

const char *inet_ntop(const void *cp, char *buf, size_t len)
{
	size_t xlen;

	const struct in6_addr *s = (const struct in6_addr *)cp;

	xlen = snprintf(buf, len, "%x:%x:%x:%x:%x:%x:%x:%x",
			_ntohs(s->s6_addr16[0]),
			_ntohs(s->s6_addr16[1]),
			_ntohs(s->s6_addr16[2]),
			_ntohs(s->s6_addr16[3]),
			_ntohs(s->s6_addr16[4]),
			_ntohs(s->s6_addr16[5]),
			_ntohs(s->s6_addr16[6]),
			_ntohs(s->s6_addr16[7]));

	if (xlen > len)
		return NULL;

	return buf;
}
