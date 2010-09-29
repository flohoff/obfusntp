
int socket_open(char *addr, int port);
int socket_set_ttl(int sock, int ttl);
int socket_join_mcast(int sock, char *addr);
int socket_set_nonblock(int sock);
