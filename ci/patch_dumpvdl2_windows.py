from pathlib import Path
import re

p = Path("src/dumpvdl2.c")
s = p.read_text(encoding="utf-8")

helper = r'''
#ifdef _WIN32
static char *win_strsep(char **stringp, const char *delim) {
    char *start, *p;
    if(stringp == NULL || *stringp == NULL) {
        return NULL;
    }
    start = *stringp;
    p = start + strcspn(start, delim);
    if(*p != '\0') {
        *p = '\0';
        *stringp = p + 1;
    } else {
        *stringp = NULL;
    }
    return start;
}

static char *win_strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    if(len > n) {
        len = n;
    }
    char *copy = malloc(len + 1);
    if(copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

#define strsep win_strsep
#define strndup win_strndup
#endif
'''

marker = '#include "gs_data.h"\n'
if helper.strip() not in s:
    if marker not in s:
        raise SystemExit("gs_data include marker not found")
    s = s.replace(marker, marker + helper + "\n", 1)

pattern = re.compile(
    r'static void setup_signals\(\) \{\n'
    r'.*?'
    r'\n\}\n\nstatic void pthread_barrier_new',
    re.S,
)
replacement = r'''static void setup_signals() {
#ifdef _WIN32
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
#else
    struct sigaction sigact, pipeact;

    memset(&sigact, 0, sizeof(sigact));
    memset(&pipeact, 0, sizeof(pipeact));
    pipeact.sa_handler = SIG_IGN;
    sigact.sa_handler = &sighandler;
    sigaction(SIGPIPE, &pipeact, NULL);
    sigaction(SIGHUP, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
#endif
}

static void pthread_barrier_new'''

s, n = pattern.subn(replacement, s, count=1)
if n != 1:
    raise SystemExit(f"setup_signals replacement count={n}")

p.write_text(s, encoding="utf-8", newline="\n")
print("Applied dumpvdl2 Windows compatibility rewrite")
