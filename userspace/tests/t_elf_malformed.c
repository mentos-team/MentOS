/// @file t_elf_malformed.c
/// @brief Regression coverage for malformed ELF program-header validation.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#define FILE_PATH "/home/user/t_elf_malformed.elf"
#define PROCAREA_END 0xC0000000U

typedef struct elf_header_fixture {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf_header_fixture_t;

typedef struct elf_program_fixture {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} elf_program_fixture_t;

typedef struct elf_fixture {
    elf_header_fixture_t header;
    elf_program_fixture_t program;
} elf_fixture_t;

/// @brief Build the smallest valid ELF fixture containing one PT_LOAD.
static elf_fixture_t make_fixture(void)
{
    elf_fixture_t fixture;
    memset(&fixture, 0, sizeof(fixture));
    fixture.header.ident[0] = 0x7f;
    fixture.header.ident[1] = 'E';
    fixture.header.ident[2] = 'L';
    fixture.header.ident[3] = 'F';
    fixture.header.ident[4] = 1;
    fixture.header.ident[5] = 1;
    fixture.header.ident[6] = 1;
    fixture.header.type = 2;
    fixture.header.machine = 3;
    fixture.header.version = 1;
    fixture.header.entry = 0x08048000;
    fixture.header.phoff = sizeof(elf_header_fixture_t);
    fixture.header.ehsize = sizeof(elf_header_fixture_t);
    fixture.header.phentsize = sizeof(elf_program_fixture_t);
    fixture.header.phnum = 1;

    fixture.program.type = 1;
    fixture.program.vaddr = 0x08048000;
    fixture.program.filesz = 0;
    fixture.program.memsz = 1;
    fixture.program.flags = 5;
    fixture.program.align = 0x1000;
    return fixture;
}

/// @brief Write a fixture and mark it executable.
static int write_fixture(const elf_fixture_t *fixture)
{
    int fd = creat(FILE_PATH, 0755);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_elf_malformed] creat: %s", strerror(errno));
        return -1;
    }
    ssize_t written = write(fd, fixture, sizeof(*fixture));
    int close_result = close(fd);
    if (written != (ssize_t)sizeof(*fixture) || close_result < 0) {
        syslog(LOG_ERR, "[t_elf_malformed] failed to write fixture: %s", strerror(errno));
        unlink(FILE_PATH);
        return -1;
    }
    return 0;
}

/// @brief Execute the fixture in a child and return its errno or signal.
static int run_fixture(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        return -2;
    }
    if (pid == 0) {
        char *argv[] = {(char *)FILE_PATH, NULL};
        char *envp[] = {NULL};
        execve(FILE_PATH, argv, envp);
        exit(errno);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -2;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

/// @brief Check one malformed fixture is rejected without killing its parent.
static int check_rejected(const char *label, elf_fixture_t *fixture)
{
    if (write_fixture(fixture) < 0) {
        return -1;
    }
    int result = run_fixture();
    unlink(FILE_PATH);
    if (result != ENOEXEC) {
        syslog(LOG_ERR, "[t_elf_malformed] %s: expected ENOEXEC, got %d", label, result);
        return -1;
    }
    return 0;
}

static void mutate_phentsize(elf_fixture_t *fixture) { fixture->header.phentsize = 0; }
static void mutate_phoff(elf_fixture_t *fixture) { fixture->header.phoff = 0x1000; }
static void mutate_phnum_overflow(elf_fixture_t *fixture) { fixture->header.phnum = 0xffff; }
static void mutate_zero_mem(elf_fixture_t *fixture) { fixture->program.memsz = 0; }
static void mutate_files_larger(elf_fixture_t *fixture)
{
    fixture->program.filesz = 2;
    fixture->program.memsz = 1;
}
static void mutate_file_range(elf_fixture_t *fixture)
{
    fixture->program.offset = 0x1000;
    fixture->program.filesz = 1;
}
static void mutate_zero_vaddr(elf_fixture_t *fixture) { fixture->program.vaddr = 0; }
static void mutate_kernel_vaddr(elf_fixture_t *fixture) { fixture->program.vaddr = PROCAREA_END; }
static void mutate_cross_kernel(elf_fixture_t *fixture)
{
    fixture->program.vaddr = PROCAREA_END - 0x1000;
    fixture->program.memsz = 0x2000;
}

int main(void)
{
    struct test_case {
        const char *label;
        void (*mutate)(elf_fixture_t *fixture);
    };

    static const struct test_case cases[] = {
        {"program-header entry size", mutate_phentsize},
        {"program-header offset", mutate_phoff},
        {"program-header count overflow", mutate_phnum_overflow},
        {"zero memory size", mutate_zero_mem},
        {"file size larger than memory size", mutate_files_larger},
        {"segment past end of file", mutate_file_range},
        {"zero virtual address", mutate_zero_vaddr},
        {"kernel virtual address", mutate_kernel_vaddr},
        {"segment crosses kernel boundary", mutate_cross_kernel},
    };

    int failures = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        elf_fixture_t fixture = make_fixture();
        cases[i].mutate(&fixture);
        if (check_rejected(cases[i].label, &fixture) < 0) {
            ++failures;
        }
    }
    syslog(LOG_DEBUG, "[t_elf_malformed] checked %u malformed program-header cases", (unsigned)(sizeof(cases) / sizeof(cases[0])));
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
