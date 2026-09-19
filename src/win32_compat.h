/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#ifdef _WIN32
#include <stddef.h>
#include <complex.h>
#include <time.h>
#include <sys/time.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#ifndef CMPLXF
#define CMPLXF(re, im) ((float complex)((float)(re) + I * (float)(im)))
#endif

char *dumphfdl_strsep(char **stringp, const char *delim);
int dumphfdl_winsock_init(void);

#ifndef strsep
#define strsep dumphfdl_strsep
#endif

#ifndef timersub
#define timersub(a, b, result) do { \
	(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
	(result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
	if((result)->tv_usec < 0) { \
		--(result)->tv_sec; \
		(result)->tv_usec += 1000000; \
	} \
} while(0)
#endif

static inline struct tm *dumphfdl_gmtime_r(const time_t *timer, struct tm *result) {
	return gmtime_s(result, timer) == 0 ? result : NULL;
}

static inline struct tm *dumphfdl_localtime_r(const time_t *timer, struct tm *result) {
	return localtime_s(result, timer) == 0 ? result : NULL;
}

#define gmtime_r dumphfdl_gmtime_r
#define localtime_r dumphfdl_localtime_r
#define timegm _mkgmtime64

#endif
