#include "kernel_backend.hpp"

#include <cstdint>
#include <sstream>

namespace shay {
namespace {
void emit_bytes(std::ostringstream &out, const std::string &text) {
    out << "    .byte ";
    bool first = true;
    for (unsigned char byte : text) {
        if (!first) out << ", ";
        out << static_cast<unsigned int>(byte);
        first = false;
    }
    if (!first) out << ", ";
    out << "13, 10, 0\n";
}
}

bool emit_x86_64_kernel_assembly(const Program &program, std::string &assembly,
                                std::string &error) {
    if (!program.errors.empty()) {
        error = "source contains parser errors";
        return false;
    }
    for (const auto &statement : program.statements) {
        if (statement.kind != StatementKind::OutputText) {
            error = "x86_64 kernel preview supports only top-level instxt statements (line " +
                    std::to_string(statement.line) + "); variables, functions, imports, and control flow are not supported yet";
            return false;
        }
        if (statement.value.find('\0') != std::string::npos) {
            error = "kernel serial output cannot contain a NUL byte (line " +
                    std::to_string(statement.line) + ")";
            return false;
        }
    }

    std::ostringstream out;
    out << R"ASM(.section .limine_requests_start,"a"
.balign 8
.global __limine_requests_start
__limine_requests_start:
    .quad 0xf6b8f4b39de7d1ae
    .quad 0xfab91a6940fcb9cf
    .quad 0x785c6ed015d3e316
    .quad 0x181e920a7852b9d9

.section .limine_requests,"a"
.balign 8
.global __limine_base_revision
__limine_base_revision:
    .quad 0xf9562b2d5c95a6c8
    .quad 0x6a7b384944536bdc
    .quad 3

.section .limine_requests_end,"a"
.balign 8
.global __limine_requests_end
__limine_requests_end:
    .quad 0xadc0e0531bb10d03
    .quad 0x9572709f31764c62

.section .text
.code64
.global _start
.type _start,@function
_start:
    cli
    cld

    # Initialize COM1 (115200 baud, 8N1).
    movw $0x3f9, %dx
    xorb %al, %al
    outb %al, %dx
    movw $0x3fb, %dx
    movb $0x80, %al
    outb %al, %dx
    movw $0x3f8, %dx
    movb $0x01, %al
    outb %al, %dx
    movw $0x3f9, %dx
    xorb %al, %al
    outb %al, %dx
    movw $0x3fb, %dx
    movb $0x03, %al
    outb %al, %dx
    movw $0x3fa, %dx
    movb $0xc7, %al
    outb %al, %dx
    movw $0x3fc, %dx
    movb $0x0b, %al
    outb %al, %dx

)ASM";
    for (std::size_t i = 0; i < program.statements.size(); ++i) {
        out << "    lea .Lshay_message_" << i << "(%rip), %rdi\n"
            << "    call .Lshay_serial_write\n";
    }
    out << R"ASM(
.Lshay_halt:
    cli
    hlt
    jmp .Lshay_halt

# Write a NUL-terminated string to the COM1 UART.
.Lshay_serial_write:
    movb (%rdi), %r8b
    testb %r8b, %r8b
    je .Lshay_serial_done
.Lshay_wait_uart:
    movw $0x3fd, %dx
    inb %dx, %al
    testb $0x20, %al
    jz .Lshay_wait_uart
    movw $0x3f8, %dx
    movb %r8b, %al
    outb %al, %dx
    incq %rdi
    jmp .Lshay_serial_write
.Lshay_serial_done:
    ret

.section .rodata
)ASM";
    for (std::size_t i = 0; i < program.statements.size(); ++i) {
        out << ".balign 1\n.Lshay_message_" << i << ":\n";
        emit_bytes(out, program.statements[i].value);
    }
    out << R"ASM(
.section .note.GNU-stack,"",@progbits
)";
    assembly = out.str();
    return true;
}

const char *x86_64_kernel_linker_script() {
    return R"LD(OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x100000;
    .limine_requests : ALIGN(8) {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :text
    .text : ALIGN(0x1000) { *(.text .text.*) } :text
    .rodata : ALIGN(0x1000) { *(.rodata .rodata.*) } :text
    .data : ALIGN(0x1000) { *(.data .data.*) } :data
    .bss : ALIGN(0x1000) { *(COMMON) *(.bss .bss.*) } :data

    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
)LD";
}
}
