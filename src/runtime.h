#ifndef SHAY_RUNTIME_H
#define SHAY_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

void shay_runtime_write(const char *text);
void shay_runtime_error(const char *text);

#ifdef __cplusplus
}
#endif

#endif
