#include "netif.h"
#include "ip6.h"

int inet_pton(const char *src, void *dst)
{
	struct in6_addr *d = (struct in6_addr *)dst;
	int colons = 0, dcolons = 0;
	int i;
	const char *p;

	for (p = dst; *p; p++) {
		if (p[0] == ':') {
			colons++;
			if (p[1] == ':') 
				dcolons++;
		} else if (!_isxdigit(*p))
				return 0;	// Invalid address
	}

	if (colons > 7 || dcolons > 1
		|| (!dcolons && colons != 7))
			return 0;	// Invalid address

	memset(d, 0, sizeof(struct in6_addr));

	i = 0;
	for (p = dst; *p; p++) {
		if (*p == ':') {
			if (p[1] == ':') 
				i += (8 - colons);
			else
				i++;
		} else {
			d->s6_addr16[i] = _htons((_ntohs(d->s6_addr16[i]) << 4) + hexval(*p));
		}
	}
	
	return 1;
}
			
