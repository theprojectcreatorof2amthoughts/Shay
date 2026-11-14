#include "runtime.h"

#include <stdio.h>

void shay_runtime_write(const char *text) {
    if (text != NULL) {
        fputs(text, stdout);
        fflush(stdout);
    }
}

void shay_runtime_error(const char *text) {
    if (text != NULL) {
        fputs(text, stderr);
        fputc('\n', stderr);
        fflush(stderr);
    }
}
