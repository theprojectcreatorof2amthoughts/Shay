#ifndef SHAY_KERNEL_BACKEND_HPP
#define SHAY_KERNEL_BACKEND_HPP

#include "ast.hpp"
#include <string>

namespace shay {
bool emit_x86_64_kernel_assembly(const Program &program, std::string &assembly,
                                std::string &error);
const char *x86_64_kernel_linker_script();
}

#endif
