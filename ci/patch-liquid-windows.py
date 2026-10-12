from pathlib import Path

p = Path("src/core/src/timer.c")
s = p.read_text(encoding="utf-8")
s = s.replace("#include <sys/resource.h>\n", "#ifndef _WIN32\n#include <sys/resource.h>\n#endif\n")
s = s.replace("    struct rusage tic_rusage;\n", "#ifndef _WIN32\n    struct rusage tic_rusage;\n#endif\n")
s = s.replace("""    } else if (_q->type == LIQUID_TIMER_RUSAGE) {
        getrusage(RUSAGE_SELF, &_q->tic_rusage);
""", """    } else if (_q->type == LIQUID_TIMER_RUSAGE) {
#ifdef _WIN32
        return liquid_error(LIQUID_EINT,"liquid_timer_tic(), RUSAGE timer is unavailable on Windows");
#else
        getrusage(RUSAGE_SELF, &_q->tic_rusage);
#endif
""")
s = s.replace("""    } else if (_q->type == LIQUID_TIMER_RUSAGE) {
        struct rusage toc;
        getrusage(RUSAGE_SELF, &toc);
        // compute run time
        float time_s  = toc.ru_utime.tv_sec - _q->tic_rusage.ru_utime.tv_sec
                      + toc.ru_stime.tv_sec - _q->tic_rusage.ru_stime.tv_sec;
        float time_us = toc.ru_utime.tv_usec - _q->tic_rusage.ru_utime.tv_usec
                      + toc.ru_stime.tv_usec - _q->tic_rusage.ru_stime.tv_usec;
        return time_s + 1e-6f*time_us;
""", """    } else if (_q->type == LIQUID_TIMER_RUSAGE) {
#ifdef _WIN32
        return liquid_error(LIQUID_EINT,"liquid_timer_toc(), RUSAGE timer is unavailable on Windows");
#else
        struct rusage toc;
        getrusage(RUSAGE_SELF, &toc);
        // compute run time
        float time_s  = toc.ru_utime.tv_sec - _q->tic_rusage.ru_utime.tv_sec
                      + toc.ru_stime.tv_sec - _q->tic_rusage.ru_stime.tv_sec;
        float time_us = toc.ru_utime.tv_usec - _q->tic_rusage.ru_utime.tv_usec
                      + toc.ru_stime.tv_usec - _q->tic_rusage.ru_stime.tv_usec;
        return time_s + 1e-6f*time_us;
#endif
""")
p.write_text(s, encoding="utf-8", newline="\n")
print("Patched liquid-dsp timer.c for Windows")
