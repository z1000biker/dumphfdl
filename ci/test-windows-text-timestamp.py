"""Execute the repository's timestamp formatter using the native Windows CRT."""
import datetime as dt
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path.cwd()
source = (ROOT / 'src/fmtr-text.c').read_text(encoding='utf-8')
start = source.index('#ifdef _WIN32')
end = source.index('static struct octet_string *fmtr_text_format_decoded_msg')
formatter = source[start:end]
prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <libacars/vstring.h>
#include "win32_compat.h"
#ifndef _WIN32
#error This test must use the native Windows compiler and CRT
#endif
#define ASSERT(x) assert(x)
static struct { bool utc; bool milliseconds; } Config;
static int fail_conversion;
static struct tm *test_gmtime_r(const time_t *t, struct tm *result) {
    return fail_conversion == 2 ? NULL : dumphfdl_gmtime_r(t, result);
}
static struct tm *test_localtime_r(const time_t *t, struct tm *result) {
    return fail_conversion == 1 ? NULL : dumphfdl_localtime_r(t, result);
}
#undef gmtime_r
#undef localtime_r
#define gmtime_r test_gmtime_r
#define localtime_r test_localtime_r
'''
suffix = r'''
int main(void) {
    char tz[64];
    long sec, usec;
    int utc, millis, fail;
    while(scanf("%63s %ld %ld %d %d %d", tz, &sec, &usec, &utc, &millis, &fail) == 6) {
        if(_putenv_s("TZ", tz) != 0) return 2;
        _tzset();
        Config.utc = utc != 0;
        Config.milliseconds = millis != 0;
        fail_conversion = fail;
        struct timeval tv = { sec, usec };
        la_vstring *v = format_timestamp(tv);
        puts(v->str);
        la_vstring_destroy(v, true);
    }
    return ferror(stdin) ? 3 : 0;
}
'''

def expected(sec, usec, utc, millis, offset):
    ms = (usec + 500) // 1000 if millis else 0
    if ms == 1000:
        sec += 1
        ms = 0
    actual_offset = 0 if utc else offset
    instant = dt.datetime.fromtimestamp(sec, dt.timezone.utc) + dt.timedelta(seconds=actual_offset)
    label = 'UTC'
    if actual_offset:
        sign = '+' if actual_offset > 0 else '-'
        hh, rem = divmod(abs(actual_offset), 3600)
        mm, ss = divmod(rem, 60)
        label += f'{sign}{hh:02}:{mm:02}'
        if ss:
            label += f':{ss:02}'
    return instant.strftime('%Y-%m-%d %H:%M:%S') + (f'.{ms:03}' if millis else '') + ' ' + label

cases = []
dates = ['1970-01-01T00:00:00', '1999-12-31T23:59:59', '2000-02-28T23:59:59',
         '2000-02-29T23:59:59', '2026-01-01T00:00:00', '2026-07-15T12:00:00',
         '2026-12-31T23:59:59', '2037-12-31T23:59:59']
zones = [('UTC0', 0), ('CET-1', 3600), ('GST-3', 10800), ('NST3:30', -12600),
         ('NPT-5:45', 20700), ('LHT-10:30', 37800), ('LDT-11', 39600),
         ('LIN-14', 50400), ('WST12', -43200), ('SEC-0:00:30', 30), ('SEC0:00:30', -30)]

def add(tz, date, offset, usecs=(0, 499, 500, 123499, 123500, 999499, 999500, 999999)):
    sec = int(dt.datetime.fromisoformat(date).replace(tzinfo=dt.timezone.utc).timestamp())
    for usec in usecs:
        for utc in (False, True):
            for millis in (False, True):
                cases.append((f'{tz} {sec} {usec} {int(utc)} {int(millis)} 0',
                              expected(sec, usec, utc, millis, offset)))

for tz, offset in zones:
    for date in dates:
        add(tz, date, offset)

# Native CRT's documented TZ DST rules are US rules, not European rules.
# Rounding at the spring jump and fall rollback changes the applicable offset.
for date, offset in [('2026-03-08T06:59:58', -18000), ('2026-03-08T07:00:00', -14400),
                     ('2026-11-01T05:30:00', -14400), ('2026-11-01T06:30:00', -18000)]:
    add('EST5EDT', date, offset)
for date, offset_before, offset_after in [('2026-03-08T06:59:59', -18000, -14400),
                                         ('2026-11-01T05:59:59', -14400, -18000)]:
    sec = int(dt.datetime.fromisoformat(date).replace(tzinfo=dt.timezone.utc).timestamp())
    for usec in (999499, 999500, 999999):
        for utc in (False, True):
            for millis in (False, True):
                offset = offset_after if millis and usec >= 999500 else offset_before
                cases.append((f'EST5EDT {sec} {usec} {int(utc)} {int(millis)} 0',
                              expected(sec, usec, utc, millis, offset)))

for utc, failure in [(0, 1), (0, 2), (1, 2)]:
    cases.append((f'UTC0 0 0 {utc} 1 {failure}', 'invalid timestamp'))

flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libacars-2'], text=True))
with tempfile.TemporaryDirectory(prefix='timestamp-crt-') as temp:
    temp = Path(temp)
    test_c = temp / 'timestamp-test.c'
    test_c.write_text(prefix + formatter + suffix, encoding='utf-8')
    for mode, defines in [('debug', []), ('release', ['-DNDEBUG'])]:
        exe = temp / f'timestamp-{mode}.exe'
        subprocess.run(['gcc', '-std=c11', '-Wall', '-Wextra', '-O2', *defines,
                        '-I', str(ROOT / 'src'), str(test_c), *flags, '-lm', '-o', str(exe)], check=True)
        result = subprocess.run([str(exe)], input='\n'.join(case[0] for case in cases) + '\n',
                                text=True, capture_output=True, check=True)
        lines = result.stdout.splitlines()
        if len(lines) != len(cases):
            raise SystemExit(f'{mode}: expected {len(cases)} lines, got {len(lines)}; stderr={result.stderr}')
        for (arguments, want), got in zip(cases, lines):
            if got != want:
                raise SystemExit(f'{mode}: {arguments}: got {got!r}, expected {want!r}')
        print(f'PASS: {mode}: {len(cases)} cases executed using native Windows UCRT and real libacars')
    print('PASS: UTC/local, zero/sub-minute/fractional offsets, epoch/year/leap-day boundaries, milliseconds, DST fold/gap, conversion failures')
