#include "shay_runtime.h"

#include <stdio.h>

void shay_print_text(const char *text) {
    if (text == NULL) {
        fputs("(null)", stdout);
        fputc('\n', stdout);
        return;
    }
    fputs(text, stdout);
    fputc('\n', stdout);
}

void shay_print_value(const char *text) {
    shay_print_text(text);
}
