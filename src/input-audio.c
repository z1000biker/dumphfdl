/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef _WIN32

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>
#include <mmsystem.h>
#include <liquid/liquid.h>      // cbuffercf_*
#include "block.h"              // block_*
#include "globals.h"            // do_exit, exitcode
#include "input-common.h"       // struct input, input_vtable
#include "input-helpers.h"      // get_sample_*, complex_samples_produce
#include "input-audio.h"
#include "util.h"               // ASSERT, NEW, XCALLOC, XFREE

#define AUDIO_BUFFER_COUNT 4
#define AUDIO_BUFFER_MS 100
#define AUDIO_DEFAULT_SAMPLE_RATE 48000

struct audio_input {
	struct input input;
	HWAVEIN wavein;
	HANDLE event;
	WAVEHDR headers[AUDIO_BUFFER_COUNT];
	int16_t *buffers[AUDIO_BUFFER_COUNT];
	bool prepared[AUDIO_BUFFER_COUNT];
	DWORD frames_per_buffer;
	UINT device_id;
};

static void print_mm_error(char const *what, MMRESULT result) {
	char msg[MAXERRORLENGTH] = {0};
	if(waveInGetErrorTextA(result, msg, sizeof(msg)) == MMSYSERR_NOERROR) {
		fprintf(stderr, "%s: %s (0x%08lx)\n", what, msg, (unsigned long)result);
	} else {
		fprintf(stderr, "%s: WinMM error 0x%08lx\n", what, (unsigned long)result);
	}
}

static bool str_contains_ci(char const *haystack, char const *needle) {
	if(haystack == NULL || needle == NULL || needle[0] == '\0') {
		return false;
	}
	size_t nlen = strlen(needle);
	for(char const *h = haystack; *h != '\0'; h++) {
		size_t i = 0;
		while(i < nlen && h[i] != '\0' &&
				tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
			i++;
		}
		if(i == nlen) {
			return true;
		}
	}
	return false;
}

void audio_input_list_devices(void) {
	UINT count = waveInGetNumDevs();
	fprintf(stderr, "Windows recording devices (%u):\n", (unsigned)count);
	if(count == 0) {
		fprintf(stderr, "  (none found)\n");
		return;
	}
	for(UINT i = 0; i < count; i++) {
		WAVEINCAPSA caps = {0};
		MMRESULT result = waveInGetDevCapsA(i, &caps, sizeof(caps));
		if(result == MMSYSERR_NOERROR) {
			fprintf(stderr, "  %u: %s\n", (unsigned)i, caps.szPname);
		} else {
			fprintf(stderr, "  %u: <unable to query device>\n", (unsigned)i);
		}
	}
	fprintf(stderr, "Use --audio-device <index> for an unambiguous selection.\n");
}

static bool audio_device_id_from_string(char const *selector, UINT *result_id) {
	ASSERT(result_id != NULL);
	if(selector == NULL || selector[0] == '\0' || _stricmp(selector, "default") == 0) {
		*result_id = WAVE_MAPPER;
		return true;
	}

	char *endptr = NULL;
	unsigned long numeric = strtoul(selector, &endptr, 10);
	if(endptr != selector && endptr[0] == '\0') {
		UINT count = waveInGetNumDevs();
		if(numeric >= count) {
			fprintf(stderr, "Audio device index %lu is out of range (0..%u)\n",
				numeric, count > 0 ? (unsigned)(count - 1) : 0);
			return false;
		}
		*result_id = (UINT)numeric;
		return true;
	}

	UINT count = waveInGetNumDevs();
	int exact_match = -1;
	int partial_match = -1;
	int partial_match_count = 0;
	for(UINT i = 0; i < count; i++) {
		WAVEINCAPSA caps = {0};
		if(waveInGetDevCapsA(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) {
			continue;
		}
		if(_stricmp(caps.szPname, selector) == 0) {
			exact_match = (int)i;
			break;
		}
		if(str_contains_ci(caps.szPname, selector)) {
			partial_match = (int)i;
			partial_match_count++;
		}
	}
	if(exact_match >= 0) {
		*result_id = (UINT)exact_match;
		return true;
	}
	if(partial_match_count == 1) {
		*result_id = (UINT)partial_match;
		return true;
	}
	if(partial_match_count > 1) {
		fprintf(stderr, "Audio device selector '%s' is ambiguous. Use --list-audio-devices and select by index.\n",
			selector);
	} else {
		fprintf(stderr, "Audio device '%s' not found. Use --list-audio-devices to see available inputs.\n",
			selector);
	}
	return false;
}

static void audio_device_name(UINT device_id, char *buf, size_t buf_len) {
	if(buf == NULL || buf_len == 0) {
		return;
	}
	buf[0] = '\0';
	UINT query_id = device_id;
	if(query_id == WAVE_MAPPER) {
		query_id = 0;
	}
	WAVEINCAPSA caps = {0};
	if(waveInGetDevCapsA(query_id, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
		snprintf(buf, buf_len, "%s", caps.szPname);
	} else if(device_id == WAVE_MAPPER) {
		snprintf(buf, buf_len, "default recording device");
	} else {
		snprintf(buf, buf_len, "device %u", (unsigned)device_id);
	}
}

struct input *audio_input_create(struct input_cfg *cfg) {
	UNUSED(cfg);
	NEW(struct audio_input, audio);
	return &audio->input;
}

static void audio_release(struct audio_input *audio) {
	if(audio == NULL) {
		return;
	}
	if(audio->wavein != NULL) {
		waveInReset(audio->wavein);
		for(size_t i = 0; i < AUDIO_BUFFER_COUNT; i++) {
			if(audio->prepared[i]) {
				waveInUnprepareHeader(audio->wavein, &audio->headers[i], sizeof(WAVEHDR));
				audio->prepared[i] = false;
			}
		}
		waveInClose(audio->wavein);
		audio->wavein = NULL;
	}
	for(size_t i = 0; i < AUDIO_BUFFER_COUNT; i++) {
		XFREE(audio->buffers[i]);
	}
	if(audio->event != NULL) {
		CloseHandle(audio->event);
		audio->event = NULL;
	}
}

void audio_input_destroy(struct input *input) {
	if(input != NULL) {
		struct audio_input *audio = container_of(input, struct audio_input, input);
		audio_release(audio);
		XFREE(audio);
	}
}

int32_t audio_input_init(struct input *input) {
	ASSERT(input != NULL);
	struct audio_input *audio = container_of(input, struct audio_input, input);
	struct input_cfg *cfg = input->config;

	if(cfg->sample_rate <= 0) {
		cfg->sample_rate = AUDIO_DEFAULT_SAMPLE_RATE;
	}
	if(!audio_device_id_from_string(cfg->source, &audio->device_id)) {
		return -1;
	}

	WAVEFORMATEX fmt = {0};
	fmt.wFormatTag = WAVE_FORMAT_PCM;
	fmt.nChannels = 1;
	fmt.nSamplesPerSec = (DWORD)cfg->sample_rate;
	fmt.wBitsPerSample = 16;
	fmt.nBlockAlign = (WORD)(fmt.nChannels * fmt.wBitsPerSample / 8);
	fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;

	audio->event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if(audio->event == NULL) {
		fprintf(stderr, "Unable to create Windows audio event (error %lu)\n",
			(unsigned long)GetLastError());
		return -1;
	}

	MMRESULT result = waveInOpen(&audio->wavein, audio->device_id, &fmt,
			(DWORD_PTR)audio->event, 0, CALLBACK_EVENT);
	if(result != MMSYSERR_NOERROR) {
		print_mm_error("Unable to open Windows audio input", result);
		fprintf(stderr, "Requested format: mono 16-bit PCM at %d Hz\n", cfg->sample_rate);
		return -1;
	}

	audio->frames_per_buffer = (DWORD)((uint64_t)cfg->sample_rate * AUDIO_BUFFER_MS / 1000U);
	if(audio->frames_per_buffer < 256) {
		audio->frames_per_buffer = 256;
	}

	for(size_t i = 0; i < AUDIO_BUFFER_COUNT; i++) {
		audio->buffers[i] = XCALLOC(audio->frames_per_buffer, sizeof(int16_t));
		memset(&audio->headers[i], 0, sizeof(WAVEHDR));
		audio->headers[i].lpData = (LPSTR)audio->buffers[i];
		audio->headers[i].dwBufferLength = audio->frames_per_buffer * sizeof(int16_t);

		result = waveInPrepareHeader(audio->wavein, &audio->headers[i], sizeof(WAVEHDR));
		if(result != MMSYSERR_NOERROR) {
			print_mm_error("waveInPrepareHeader failed", result);
			return -1;
		}
		audio->prepared[i] = true;

		result = waveInAddBuffer(audio->wavein, &audio->headers[i], sizeof(WAVEHDR));
		if(result != MMSYSERR_NOERROR) {
			print_mm_error("waveInAddBuffer failed", result);
			return -1;
		}
	}

	cfg->sfmt = SFMT_S16;
	input->full_scale = get_sample_full_scale_value(cfg->sfmt);
	input->bytes_per_sample = (int32_t)get_sample_size(cfg->sfmt);
	input->block.producer.max_tu = audio->frames_per_buffer;

	char name[MAXPNAMELEN + 32] = {0};
	audio_device_name(audio->device_id, name, sizeof(name));
	fprintf(stderr, "Windows audio input: %s, mono 16-bit PCM, %d Hz\n", name, cfg->sample_rate);
	fprintf(stderr, "Receiver must be in USB mode and tuned to the HFDL channel frequency.\n");
	return 0;
}

void *audio_input_thread(void *ctx) {
	ASSERT(ctx != NULL);
	struct block *block = ctx;
	struct input *input = container_of(block, struct input, block);
	struct audio_input *audio = container_of(input, struct audio_input, input);
	struct circ_buffer *circ_buffer = &block->producer.out->circ_buffer;
	float complex *outbuf = XCALLOC(audio->frames_per_buffer, sizeof(float complex));

	MMRESULT result = waveInStart(audio->wavein);
	if(result != MMSYSERR_NOERROR) {
		print_mm_error("waveInStart failed", result);
		exitcode = EXIT_FAILURE;
		do_exit = 1;
		goto shutdown;
	}

	while(do_exit == 0) {
		DWORD wait_result = WaitForSingleObject(audio->event, 250);
		if(wait_result == WAIT_TIMEOUT) {
			continue;
		}
		if(wait_result != WAIT_OBJECT_0) {
			fprintf(stderr, "Windows audio wait failed (error %lu)\n",
				(unsigned long)GetLastError());
			exitcode = EXIT_FAILURE;
			do_exit = 1;
			break;
		}

		for(size_t i = 0; i < AUDIO_BUFFER_COUNT; i++) {
			WAVEHDR *hdr = &audio->headers[i];
			if((hdr->dwFlags & WHDR_DONE) == 0) {
				continue;
			}
			DWORD bytes = hdr->dwBytesRecorded;
			if(bytes > 0) {
				input->convert_sample_buffer(input, hdr->lpData, bytes, outbuf);
				complex_samples_produce(circ_buffer, outbuf, bytes / input->bytes_per_sample);
			}
			hdr->dwBytesRecorded = 0;
			if(do_exit == 0) {
				result = waveInAddBuffer(audio->wavein, hdr, sizeof(WAVEHDR));
				if(result != MMSYSERR_NOERROR) {
					print_mm_error("waveInAddBuffer failed while capturing", result);
					exitcode = EXIT_FAILURE;
					do_exit = 1;
					break;
				}
			}
		}
	}

shutdown:
	waveInStop(audio->wavein);
	waveInReset(audio->wavein);
	block_connection_one2one_shutdown(block->producer.out);
	block->running = false;
	XFREE(outbuf);
	return NULL;
}

struct input_vtable const audio_input_vtable = {
	.create = audio_input_create,
	.init = audio_input_init,
	.destroy = audio_input_destroy,
	.rx_thread_routine = audio_input_thread
};

#endif /* _WIN32 */
