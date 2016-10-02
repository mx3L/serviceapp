// dummied source from openpli-enigma2
#ifndef __wrappers_h
#define __wrappers_h

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <vector>
#include <string>
#include <sys/select.h>

ssize_t singleRead(SSL *ssl, int fd, void *buf, size_t count);
ssize_t timedRead(SSL *ssl, int fd, void *buf, size_t count, int initialtimeout, int interbytetimeout);
ssize_t readLine(SSL *ssl, int fd, char** buffer, size_t* bufsize);
ssize_t writeAll(SSL *ssl, int fd, const void *buf, size_t count);
int Select(int maxfd, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int Connect(const char *hostname, int port, int timeoutsec);
int SSLConnect(const char *hostname, int fd, SSL **ssl, SSL_CTX **ctx);

#endif
