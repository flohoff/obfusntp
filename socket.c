
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int socket_open(char *addr, int port) {
	struct sockaddr_in	lsin;
	int			sock;

	memset(&lsin, 0, sizeof(struct sockaddr_in));

	sock=socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (sock<0)
		return sock;

	lsin.sin_family=AF_INET;
	lsin.sin_addr.s_addr=INADDR_ANY;
	if (addr)
		inet_aton(addr, &lsin.sin_addr);
	lsin.sin_port=htons(port);

	if (bind(sock, (struct sockaddr *) &lsin,
			sizeof(struct sockaddr_in)) != 0) {
		close(sock);
		return -1;
	}

	return sock;
}

int socket_set_ttl(int sock, int ttl) {
	if (ttl)
		return setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
	return 0;
}

int socket_set_nonblock(int sock) {
	unsigned long int	flags;
	/* Set new socket O_NONBLOCK */
	flags=fcntl(sock, F_GETFL);
	return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int socket_join_mcast(int sock, char *addr) {
	struct ip_mreq		mreq;

	memset(&mreq, 0, sizeof(struct ip_mreq));

	/* Its not an ip address ? */
	if (!inet_aton(addr, &mreq.imr_multiaddr))
		return 0;

	//mreq.imr_multiaddr.s_addr=inet_addr(s->remoteaddr);
	mreq.imr_interface.s_addr=INADDR_ANY;

	return setsockopt (sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
}
