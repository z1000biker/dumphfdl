/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdint.h>                     // int64_t
#include <stdio.h>                      // snprintf
#include <math.h>                       // round
#include <time.h>                       // strftime, gmtime, localtime, gmtime_r, localtime_r
#include <sys/time.h>                   // struct timeval
#include <libacars/libacars.h>          // la_proto_node
#include <libacars/vstring.h>           // la_vstring
#include "fmtr-text.h"
#include "output-common.h"              // fmtr_descriptor_t
#include "util.h"                       // struct octet_string, Config, EOL
#include "pdu.h"                        // struct hfdl_pdu_metadata

static bool fmtr_text_supports_data_type(fmtr_input_type_t type) {
	return(type == FMTR_INTYPE_DECODED_FRAME);
}

#ifdef _WIN32
/* Calendar arithmetic avoids _mkgmtime64's 1970..3000 range restriction.
 * The inputs are normalized results from localtime_r/gmtime_r. DST is already
 * reflected in their calendar fields and must not be applied a second time.
 */
static int64_t windows_calendar_seconds(struct tm const *tmstruct) {
	int64_t year = (int64_t)tmstruct->tm_year + 1899;
	int64_t days = 365 * year + year / 4 - year / 100 + year / 400
			+ tmstruct->tm_yday;
	return days * 86400 + tmstruct->tm_hour * 3600
			+ tmstruct->tm_min * 60 + tmstruct->tm_sec;
}

static void format_windows_timestamp(char *tbuf, size_t tbuf_len, char *tzbuf, size_t tzbuf_len,
		struct tm const *tmstruct, struct tm const *utc_tm) {
	ASSERT(tbuf != NULL);
	ASSERT(tzbuf != NULL);
	ASSERT(tmstruct != NULL);

	/*
	 * Do not use locale-dependent %x/%X here. Older Windows CRTs used by some
	 * MinGW toolchains also do not reliably support the POSIX %F/%T forms.
	 * Build the Unix-style timestamp explicitly instead.
	 */
	snprintf(tbuf, tbuf_len, "%04d-%02d-%02d %02d:%02d:%02d",
			tmstruct->tm_year + 1900,
			tmstruct->tm_mon + 1,
			tmstruct->tm_mday,
			tmstruct->tm_hour,
			tmstruct->tm_min,
			tmstruct->tm_sec);

	if(Config.utc == true) {
		/*
		 * strftime("%Z", gmtime(...)) may produce an empty string on Windows.
		 * UTC is known here, so emit it explicitly.
		 */
		snprintf(tzbuf, tzbuf_len, "UTC");
		return;
	}

	ASSERT(utc_tm != NULL);
	int64_t offset_seconds = windows_calendar_seconds(tmstruct)
			- windows_calendar_seconds(utc_tm);
	if(offset_seconds == 0) {
		snprintf(tzbuf, tzbuf_len, "UTC");
		return;
	}

	char sign = '+';
	if(offset_seconds < 0) {
		sign = '-';
		offset_seconds = -offset_seconds;
	}
	long offset_minutes = (long)(offset_seconds / 60);
	int remaining_seconds = (int)(offset_seconds % 60);
	if(remaining_seconds != 0) {
		snprintf(tzbuf, tzbuf_len, "UTC%c%02ld:%02ld:%02d",
				sign, offset_minutes / 60, offset_minutes % 60, remaining_seconds);
	} else {
		snprintf(tzbuf, tzbuf_len, "UTC%c%02ld:%02ld",
				sign, offset_minutes / 60, offset_minutes % 60);
	}
}
#endif

static la_vstring *format_timestamp(struct timeval tv) {
	int32_t millis = 0;
	if(Config.milliseconds == true) {
		millis = round(tv.tv_usec / 1000.0);
		if(millis > 999) {
		    millis -= 1000;
		    tv.tv_sec++;
		}
	}
	time_t seconds = (time_t)tv.tv_sec;
	char tbuf[32], tzbuf[16];
#ifdef _WIN32
	struct tm tmstruct, utc_tm;
	if((Config.utc == true ? gmtime_r(&seconds, &tmstruct)
			: localtime_r(&seconds, &tmstruct)) == NULL ||
			(Config.utc == false && gmtime_r(&seconds, &utc_tm) == NULL)) {
		la_vstring *invalid = la_vstring_new();
		la_vstring_append_sprintf(invalid, "invalid timestamp");
		return invalid;
	}
	format_windows_timestamp(tbuf, sizeof(tbuf), tzbuf, sizeof(tzbuf), &tmstruct,
			Config.utc == true ? NULL : &utc_tm);
#else
	struct tm *tmstruct = (Config.utc == true ? gmtime(&seconds) : localtime(&seconds));
	strftime(tbuf, sizeof(tbuf), "%F %T", tmstruct);
	strftime(tzbuf, sizeof(tzbuf), "%Z", tmstruct);
#endif

	la_vstring *vstr = la_vstring_new();
	la_vstring_append_sprintf(vstr, "%s", tbuf);
	if(Config.milliseconds == true) {
		la_vstring_append_sprintf(vstr, ".%03d", millis);
	}
	la_vstring_append_sprintf(vstr, " %s", tzbuf);
	return vstr;
}

static struct octet_string *fmtr_text_format_decoded_msg(struct metadata *metadata, la_proto_node *root) {
	ASSERT(metadata != NULL);
	ASSERT(root != NULL);

	struct hfdl_pdu_metadata *hm = container_of(metadata, struct hfdl_pdu_metadata,
			metadata);
	la_vstring *timestamp = format_timestamp(metadata->rx_timestamp);
	la_vstring *vstr = la_vstring_new();

	la_vstring_append_sprintf(vstr, "[%s] [%.1f kHz] [%.1f Hz] [%.1f/%.1f dBFS] [%.1f dB] [%d bps] [%c]",
			timestamp->str,
			(float)hm->freq / 1000.f,
			hm->freq_err_hz,
			hm->rssi,
			hm->noise_floor,
			hm->rssi - hm->noise_floor,
			hm->bit_rate,
			hm->slot
			);
	la_vstring_destroy(timestamp, true);

	EOL(vstr);

	vstr = la_proto_tree_format_text(vstr, root);
	EOL(vstr);
	struct octet_string *ret = octet_string_new(vstr->str, vstr->len);
	la_vstring_destroy(vstr, false);
	return ret;
}

fmtr_descriptor_t fmtr_DEF_text = {
	.name = "text",
	.description = "Human readable text",
	.format_decoded_msg = fmtr_text_format_decoded_msg,
	.format_raw_msg = NULL,
	.supports_data_type = fmtr_text_supports_data_type,
	.output_format = OFMT_TEXT,
};
