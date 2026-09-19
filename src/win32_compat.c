/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include <pthread.h>
#include "win32_compat.h"

#ifdef _WIN32
static pthread_once_t winsock_once = PTHREAD_ONCE_INIT;
static int winsock_result = WSASYSNOTREADY;

static void winsock_init_once(void) {
	WSADATA data;
	winsock_result = WSAStartup(MAKEWORD(2, 2), &data);
}

int dumphfdl_winsock_init(void) {
	pthread_once(&winsock_once, winsock_init_once);
	return winsock_result;
}

char *dumphfdl_strsep(char **stringp, const char *delim) {
	char *start = *stringp;
	if(start == NULL) {
		return NULL;
	}
	char *p = start;
	while(*p != '\0') {
		if(strchr(delim, *p) != NULL) {
			*p = '\0';
			*stringp = p + 1;
			return start;
		}
		p++;
	}
	*stringp = NULL;
	return start;
}
#endif
