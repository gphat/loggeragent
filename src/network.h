#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_NAMESER_H
#include <nameser.h>
#endif

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <resolv.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include <sys/socket.h>
#include <sys/types.h>

int open_listener(int);

int open_connection(char *, int);

SSL_CTX* init_context(void);

int load_certificates(SSL_CTX *);

int show_certs(SSL *ssl);

void servlet(SSL *);

int ssl_send(SSL *, char *);
