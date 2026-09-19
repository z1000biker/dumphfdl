/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>                      // fprintf
#include <string.h>                     // strdup, strerror
#include <unistd.h>                     // close
#include <errno.h>                      // errno
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>                  // socket, connect
#include <sys/socket.h>                 // socket, connect
#include <netdb.h>                      // getaddrinfo
#endif
#include "output-common.h"              // output_descriptor_t, output_qentry_t, output_queue_drain
#include "kvargs.h"                     // kvargs, option_descr_t
#include "util.h"                       // ASSERT

#ifdef _WIN32
typedef SOCKET output_socket_t;
#define OUTPUT_INVALID_SOCKET INVALID_SOCKET
#define output_socket_close closesocket
static int output_socket_send(output_socket_t sock, void const *buf, size_t len) {
	return send(sock, (char const *)buf, (int)len, 0);
}
#else
typedef int output_socket_t;
#define OUTPUT_INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define output_socket_close close
static ssize_t output_socket_send(output_socket_t sock, void const *buf, size_t len) {
	return write(sock, buf, len);
}
#endif

typedef struct {
	char *address;
	char *port;
	output_socket_t sockfd;
} out_udp_ctx_t;

static bool out_udp_supports_format(output_format_t format) {
	return(format == OFMT_TEXT || format == OFMT_BASESTATION || format == OFMT_JSON);
}

static void *out_udp_configure(kvargs *kv) {
	ASSERT(kv != NULL);
	NEW(out_udp_ctx_t, cfg);
	if(kvargs_get(kv, "address") == NULL) {
		fprintf(stderr, "output_udp: IP address not specified\n");
		goto fail;
	}
	cfg->address = strdup(kvargs_get(kv, "address"));
	if(kvargs_get(kv, "port") == NULL) {
		fprintf(stderr, "output_udp: UDP port not specified\n");
		goto fail;
	}
	cfg->port = strdup(kvargs_get(kv, "port"));
	cfg->sockfd = OUTPUT_INVALID_SOCKET;
	return cfg;
fail:
	XFREE(cfg);
	return NULL;
}

static int out_udp_init(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_udp_ctx_t *self = selfptr;

#ifdef _WIN32
	if(dumphfdl_winsock_init() != 0) {
		fprintf(stderr, "output_udp: WSAStartup failed\n");
		return -1;
	}
#endif

	struct addrinfo hints, *result, *rptr;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	int ret = getaddrinfo(self->address, self->port, &hints, &result);
	if(ret != 0) {
		fprintf(stderr, "output_udp: could not resolve %s: %s\n", self->address, gai_strerror(ret));
		return -1;
	}
	for (rptr = result; rptr != NULL; rptr = rptr->ai_next) {
		self->sockfd = socket(rptr->ai_family, rptr->ai_socktype, rptr->ai_protocol);
		if(self->sockfd == OUTPUT_INVALID_SOCKET) {
			continue;
		}
		if(connect(self->sockfd, rptr->ai_addr, (int)rptr->ai_addrlen) != SOCKET_ERROR) {
			break;
		}
		output_socket_close(self->sockfd);
		self->sockfd = OUTPUT_INVALID_SOCKET;
	}
	if (rptr == NULL) {
		fprintf(stderr, "output_udp: Could not set up UDP socket to %s:%s: all addresses failed\n",
				self->address, self->port);
		self->sockfd = OUTPUT_INVALID_SOCKET;
		return -1;
	}
	freeaddrinfo(result);
	return 0;
}

static int out_udp_produce_text(out_udp_ctx_t *self, struct metadata *metadata, struct octet_string *msg) {
	UNUSED(metadata);
	ASSERT(msg != NULL);
	ASSERT(self->sockfd != OUTPUT_INVALID_SOCKET);
	if(msg->len < 2) {
		return 0;
	}
	if(output_socket_send(self->sockfd, msg->buf, msg->len) < 0) {
		return -1;
	}
	return 0;
}

static int out_udp_produce(void *selfptr, output_format_t format, struct metadata *metadata, struct octet_string *msg) {
	ASSERT(selfptr != NULL);
	out_udp_ctx_t *self = selfptr;
	int32_t result = 0;
	if(format == OFMT_TEXT || format == OFMT_JSON || format == OFMT_BASESTATION) {
		result = out_udp_produce_text(self, metadata, msg);
	}
	if(result < 0) {
		// UDP output is fire-and-forget by definition.
		// Return 0 regardless of whether the send succeeded or not,
		// but print an error to help with diagnosing common issues,
		// (eg. nothing listening on the receiver port).
		fprintf(stderr, "output_udp(%s:%s): send error: %s\n", self->address,
			self->port,	strerror(errno));
	}
	return 0;
}

static void out_udp_handle_shutdown(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_udp_ctx_t *self = selfptr;
	fprintf(stderr, "output_udp(%s:%s): shutting down\n", self->address, self->port);
	if(self->sockfd != OUTPUT_INVALID_SOCKET) {
		output_socket_close(self->sockfd);
	}
}

static void out_udp_handle_failure(void *selfptr) {
	ASSERT(selfptr != NULL);
	out_udp_ctx_t *self = selfptr;
	fprintf(stderr, "output_udp: can't connect to %s:%s, deactivating output\n",
			self->address, self->port);
	if(self->sockfd != OUTPUT_INVALID_SOCKET) {
		output_socket_close(self->sockfd);
	}
}

static const option_descr_t out_udp_options[] = {
	{
		.name = "address",
		.description = "Destination host name or IP address (required)"
	},
	{
		.name = "port",
		.description = "Destination UDP port (required)"
	},
	{
		.name = NULL,
		.description = NULL
	}
};

output_descriptor_t out_DEF_udp = {
	.name = "udp",
	.description = "Output to a remote host via UDP",
	.options = out_udp_options,
	.supports_format = out_udp_supports_format,
	.configure = out_udp_configure,
	.init = out_udp_init,
	.produce = out_udp_produce,
	.handle_shutdown = out_udp_handle_shutdown,
	.handle_failure = out_udp_handle_failure
};
