/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "input-common.h"       // struct input_vtable

extern struct input_vtable audio_input_vtable;

/* Print Windows recording devices in a stable index:name form. */
void audio_input_list_devices(void);
