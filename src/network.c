#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "log.h"
#include "network.h"
#include "script.h"
#include "util.h"

static SSL_CTX *ctx = NULL;

int open_listener(int port) {
	int sd;
	struct sockaddr_in addr;

	sd = socket(PF_INET, SOCK_STREAM, 0);
	if(sd == -1) {
		llog(LOG_ERR, "Could not open socket.");
		return -1;
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		llog(LOG_ERR, "Can't bind to port.");
		return -1;
	}
	if(listen(sd, 10) != 0) {
		llog(LOG_ERR, "Can't configure listening port.");
		return -1;
	}
	return sd;
}

int open_connection(char *hostname, int port) {
	int sd;
	struct hostent *host;
	struct sockaddr_in addr;

	if((host = gethostbyname(hostname)) == NULL) {
		llog(LOG_ERR, "Error resolving hostname '%s'.", hostname);
		return -1;
	}

	sd = socket(PF_INET, SOCK_STREAM, 0);
	if(sd == -1) {
		llog(LOG_ERR, "Error opening socket.");
		return -1;
	}

	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = *(long *)(host->h_addr);
	if(connect(sd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		close(sd);
		llog(LOG_ERR, "Error connecting to host '%s'.", hostname);
		return -1;
	}

	return sd;
}

SSL_CTX* init_context(void) {
	char *seed = NULL;

	if(ctx == NULL) {
		llog(LOG_DEBUG, "Seeding PRNG.");
		seed = gen_random(2048);
		RAND_seed(seed, 2048);
		free(seed);

		llog(LOG_DEBUG, "Generating Context.");
		SSL_library_init();
		OpenSSL_add_all_algorithms();
		SSL_load_error_strings();

		ctx = SSL_CTX_new(SSLv3_method());
		if(ctx == NULL) {
			llog(LOG_ERR, "Error creating SSL context: %s",
				(char *) ERR_error_string(ERR_get_error(), NULL)
			);
			return NULL;
		}

		SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_AUTO_RETRY);

		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
		if(SSL_CTX_load_verify_locations(ctx, CA_CERT, NULL) == 0) {
			SSL_CTX_free(ctx);
			llog(LOG_ERR, "SSL_CTX_load_verify_locations failed: %s",
				ERR_error_string(ERR_get_error(), NULL)
			);
			return NULL;
		}
		SSL_CTX_set_client_CA_list(ctx, SSL_load_client_CA_file(CA_CERT));

		if(load_certificates(ctx) < 0) {
			llog(LOG_ERR, "Error loading certificates.");
			SSL_CTX_free(ctx);
			return NULL;
		}
	} else {
		llog(LOG_DEBUG, "Skipped context creation.");
	}
	
	return ctx;
}

int load_certificates(SSL_CTX* ctx) {

	llog(LOG_DEBUG, "Attempting to load '%s' and '%s'.", AGENT_KEY, AGENT_CERT);

	if(SSL_CTX_use_certificate_file(ctx, AGENT_CERT, SSL_FILETYPE_PEM) <= 0) {
		llog(LOG_ERR, "Error loading certificate '%s' : %s", AGENT_CERT,
			(char *) ERR_error_string(ERR_get_error(), NULL)
		);
		return -1;
	}
	llog(LOG_DEBUG, "Loaded certificate '%s'.", AGENT_CERT);

	if(SSL_CTX_use_PrivateKey_file(ctx, AGENT_KEY, SSL_FILETYPE_PEM) <= 0) {
		llog(LOG_ERR, "Error loading key '%s' : %s", AGENT_KEY,
			(char *) ERR_error_string(ERR_get_error(), NULL)
		);
		return -1;
	}
	llog(LOG_DEBUG, "Loaded private key '%s'.", AGENT_KEY);

	if(!SSL_CTX_check_private_key(ctx)) {
		llog(LOG_ERR, "Private key does not match the public certificate");
		return -1;
	}

	return 1;
}

int show_certs(SSL *ssl) {
	X509 *cert;
	char *line;

	cert = SSL_get_peer_certificate(ssl);
	if(cert != NULL) {
		llog(LOG_DEBUG, "---- Client Certificates ----");
		line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
		llog(LOG_DEBUG, "Subject: %s", line);
		free(line);
		line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
		llog(LOG_DEBUG, "Issuer: %s", line);
		free(line);
		X509_free(cert);
	} else {
		llog(LOG_DEBUG, "No certificates.");
		return -1;
	}
	return 1;
}

void servlet(SSL *ssl) {
	char buf[16384];
	char *errors, *output;
	int r, loop = 1;
	xmlDocPtr doc;
	long verify = 0;
	int certCheck = 0;
	int retval = 0;

	bzero(&buf, 16384);
	if((retval = SSL_accept(ssl)) < 1) {
		llog(LOG_ERR, "Error in SSL_accept: %s",
			(char *) ERR_error_string(ERR_get_error(), NULL)
		);
	} else {

		if((verify = SSL_get_verify_result(ssl)) != X509_V_OK) {
			llog(LOG_ERR, "Could not verify client certificate: %s",
				X509_verify_cert_error_string(verify)
			);
			return;
		}

		if(show_certs(ssl) == -1) {
			llog(LOG_ERR, "Error showing certificates.");
			return;
		}

		while(loop == 1) {
			r = SSL_read(ssl, buf, sizeof(buf));
			chomp(buf);
			switch(SSL_get_error(ssl, r)) {
				case SSL_ERROR_NONE:
				case SSL_ERROR_SYSCALL:
					if(r == -1) {
						llog(LOG_ERR, "SSL syscall error: %s", strerror(errno));
					}
					loop = 0;
					break;
				case SSL_ERROR_WANT_READ:
					/* Keep reading */
					llog(LOG_DEBUG, "Calling SSL_read() again for more data.");
					continue;
					break;
				case SSL_ERROR_SSL:
					llog(LOG_ERR, "SSL error: %s", ERR_error_string(ERR_get_error(), NULL));
					/* Fall through */
				case SSL_ERROR_ZERO_RETURN:
					llog(LOG_DEBUG, "Exiting loop, no more data.");
					loop = 0;
					break;
			}
			if(!strncmp("+DONE+", buf, 6)) {
				llog(LOG_DEBUG,"Client says DONE.");
				loop = 0;
				break;
			}
			printf("Client said:\n\"%s\" (%d bytes)\n", buf, r);
			buffer_errors(1);
			doc = xmlParseDoc(buf);
			if(doc == NULL) {
				llog(LOG_ERR, "Document not parsed correctly.");
				ssl_send(ssl, "+NOK+");
				ssl_send(ssl, "Document not parsed correctly.");
				buffer_errors(0);
				clear_output();
				ssl_send(ssl, "+DONE+");
				llog(LOG_DEBUG, "Sent DONE to client.");
				return;
			}
			if(parse_doc(doc) == 0) {
				errors = get_error_strings();
				ssl_send(ssl, "+NOK+");
				ssl_send(ssl, errors);
				xmlFree(doc);
				buffer_errors(0);
				clear_output();
				ssl_send(ssl, "+DONE+");
				llog(LOG_DEBUG, "Sent DONE to client.");
				return;
			}
			errors = get_error_strings();
			output = get_output();
			
			if(errors == NULL) {
				ssl_send(ssl, "+OK+");
				llog(LOG_DEBUG, "Errors is NULL, not sending.");
			} else {
				ssl_send(ssl, "+NOK+");
				ssl_send(ssl, output);
				llog(LOG_DEBUG, "Sent errors to client.");
			}

			if(output != NULL) {
				ssl_send(ssl, output);
				llog(LOG_DEBUG, "Output sent to client.");
			} else {
				llog(LOG_DEBUG, "Output is NULL, not sending.");
			}
			buffer_errors(0);
			clear_output();
			ssl_send(ssl, "+DONE+");
			llog(LOG_DEBUG, "Sent DONE to client.");
			xmlFreeDoc(doc);
		}
	} 
}

int ssl_send(SSL *ssl, char *msg) {
	int n = 0;
	char *msgsend;

	if(msg == NULL) {
		return 0;
	}

	msgsend = lprintf("%s\r\n", msg);

writeloop:
	n = SSL_write(ssl, msgsend, strlen(msgsend));
	switch(SSL_get_error(ssl, n)) {
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			goto writeloop;
		case SSL_ERROR_SYSCALL:
			llog(LOG_ERR, "SSL syscall error");
		case SSL_ERROR_ZERO_RETURN:
			break;
		defalt:
			llog(LOG_ERR, "Unknown SSL error.");
			break;
	}

	free(msgsend);

	return n;
}
