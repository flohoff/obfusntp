#include <stdlib.h>
#include <event.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "socket.h"

#define MAX_MSG_SIZE	2048


/*
 * convert microseconds to fraction of second * 2^32 (i.e., the lsw of
 * a 64-bit ntp timestamp).  This routine uses the factorization
 * 2^32/10^6 = 4096 + 256 - 1825/32 which results in a max conversion
 * error of 3 * 10^-7 and an average error of half that.
 *
 * Geklaut von http://www.openmash.org/lxr/source/rtp/ntp-time.h
 * 20080215 flo
 */
static inline u_int usec2ntp(u_int usec) {
	u_int t = (usec * 1825) >> 5;
	return ((usec << 12) + (usec << 8) - t);
}

/*
 * Number of seconds between 1-Jan-1900 and 1-Jan-1970
 */

#define GETTIMEOFDAY_TO_NTP_OFFSET 2208988800UL

char		msgbuffer[MAX_MSG_SIZE];

typedef struct {
	u_int32_t upper;        /* more significant 32 bits */
	u_int32_t lower;        /* less significant 32 bits */
} ntp64;

struct ntppkt_s {
	uint8_t		flags;
	uint8_t		stratum;
	uint8_t		poll;
	uint8_t		precision;

	uint32_t	rootdelay;
	uint32_t	rootdisp;
	uint32_t	refid;

	ntp64		referencets;
	ntp64		origints;
	ntp64		receivets;
	ntp64		transmitts;
};

#define NTP_LI_MASK		0xc0

#define NTP_VERSION_MASK	0x38
#define NTP_VERSION_SHIFT	3

#define NTP_MODE_MASK		0x7
#define NTP_MODE_CLIENT		0x3
#define NTP_MODE_SERVER		0x4

#define ntp_version(a) (((a)->flags&NTP_VERSION_MASK)>>NTP_VERSION_SHIFT)
#define ntp_client_request(a) (((a)->flags&NTP_MODE_MASK) == NTP_MODE_CLIENT)

/*
* Return a 64-bit ntp timestamp (UTC time relative to Jan 1, 1970).
* gettimeofday conveniently gives us the correct reference -- we just
* need to convert sec+usec to a 64-bit fixed point (with binary point
* at bit 32).
*/
static inline ntp64 ntp64time(struct timeval tv) {
	ntp64 n;
	n.upper = (u_int)tv.tv_sec + GETTIMEOFDAY_TO_NTP_OFFSET;
	n.lower = usec2ntp((u_int)tv.tv_usec);
	return (n);
}

struct {
	uint32_t	offset;
	int		obfus;
	int		debug;
	char		*bindaddr;
} config;

#define	HASH_MAX	(256<<3)
#define OFFSET_DEFAULT	600

uint32_t randoffset(struct sockaddr_in *sin) {
	uint32_t	orghash;
	uint32_t	ro;
	uint32_t	addr=ntohl(sin->sin_addr.s_addr);

	orghash=((addr>>24)^(addr>>15)^(addr>>6)^(addr<<3))&(HASH_MAX-1);

	ro=(config.offset*orghash)/HASH_MAX;

	if (config.debug) {
		printf("%d.%d.%d.%d %05x %d\n",
			addr>>24&0xff,
			addr>>16&0xff,
			addr>>8&0xff,
			addr&0xff,
			orghash,
			ro);
	}

	return ro;
}

static void ntp_recv(int fd, short event, void *arg) {
	int			len;
	unsigned int		sinlen=sizeof(struct sockaddr_in);
	uint32_t		ro;
	struct sockaddr_in	sin;
	struct ntppkt_s		*np=(void *) &msgbuffer;
	struct timeval		tv;
	ntp64			ntpnow;

	/* len=recv(fd, msgbuffer, MAX_MSG_SIZE, 0); */

	len=recvfrom(fd, &msgbuffer, MAX_MSG_SIZE, 0,
		(struct sockaddr *) &sin, &sinlen);

	if (len <= 0)
		return;

	if (ntp_version(np) <= 2) {
		printf("Received version %d request - dropping\n", ntp_version(np));
		return;
	}

	if (!ntp_client_request(np)) {
		printf("Not ntp client request - dropping\n");
		return;
	}

	/* Set Server reply mode */
	np->flags=(np->flags&~NTP_MODE_MASK)|NTP_MODE_SERVER;
	np->flags=np->flags&~NTP_LI_MASK;

	/* Set BAD stratum */
	np->stratum=4;

	/* 0.000001 sec precision */
	np->precision=0xec;

	np->rootdelay=0x0;
	np->rootdisp=0x0;

	/*
	 * Make it clear that this is a RANDOM time source
	 * although it gets interpreted as an IP Address
	 *
	 */
	memcpy(&np->refid, "RND\000", 4);

	if (gettimeofday(&tv, NULL) < 0)
		return;

	ntpnow=ntp64time(tv);

	if (config.obfus) {
		ro=randoffset((struct sockaddr_in *) &sin);
		ntpnow.upper-=ro;
	}

	ntpnow.upper=htonl(ntpnow.upper);
	ntpnow.lower=htonl(ntpnow.lower);

	/* Copy old transmit timestamp to originate timestamp */
	np->origints=np->transmitts;

	np->receivets=ntpnow;
	np->transmitts=ntpnow;
	np->referencets=ntpnow;

	sendto(fd, msgbuffer, len, 0, (struct sockaddr *) &sin, sinlen);
}

static void usage(void ) {
	printf("obfusntp [-n] [-o offset]\n\n");
	printf("-n		- No obfuscate\n");
	printf("-o offset	- Max offset\n");
	printf("-d		- Debug\n");
	printf("-b addr		- Bind addr\n");
	exit(0);
}

int main(int argc, char **argv) {
	int		s;
	struct event	ntpe;
	char		ch;

	config.offset=OFFSET_DEFAULT;
	config.obfus=1;

	while((ch=getopt(argc, argv, "b:n:o:d")) != -1) {
		switch(ch) {
			case 'o':
				config.offset=strtol(optarg, NULL, 10);
				break;
			case 'b':
				config.bindaddr=strdup(optarg);
				break;
			case 'n':
				config.obfus=0;
				break;
			case 'd':
				config.debug++;
				break;
			default:
				usage();
				break;
		}

	}

	s=socket_open(config.bindaddr, 123);

	if (s<0)
		exit(-1);

	socket_set_nonblock(s);

	event_init();

	event_set(&ntpe, s, EV_READ|EV_PERSIST, ntp_recv, NULL);
	event_add(&ntpe, NULL);

	event_dispatch();

	exit(0);
}
