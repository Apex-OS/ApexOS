// ApexKernel/Includes/Core/Shell/PathResolver.h
// ApexOS -- Shell path resolution helpers

#ifndef APEX_CORE_SHELL_PATH_RESOLVER_H
#define APEX_CORE_SHELL_PATH_RESOLVER_H

#include "Core/Types.h"

#define CWD_MAX_LEN 512

// Current working directory, stored as a full absolute path including
// the volume prefix (e.g. "/@SysDisk/Home/Users/Zeemoud"). Shared with
// Prompt.c (prompt display) and the shell Commands/ (ring3 command
// context).
extern char cwd[CWD_MAX_LEN];

// Builds an absolute path from a user argument: if `arg` starts with
// '/', it is copied as-is; otherwise it is joined to `cwd`. The result
// is normalized (".", ".." resolved) before being written to `out`.
void path_make_absolute(const char *arg, char *out, uint32_t out_size);

// Splits an absolute path into (parent path, final segment name).
// Returns false if the path has no valid final segment.
bool split_path_parent_name(const char *abs_path, char *parent_out,
                            uint32_t parent_size, char *name_out,
                            uint32_t name_size);

// Finds the first space in `args`, NUL-terminates the first token in
// place, and returns a pointer to the remainder. Returns NULL if no
// space is found.
char *split_first_space(char *args);

bool str_eq(const char *a, const char *b);
bool str_startswith(const char *str, const char *prefix);

#endif // APEX_CORE_SHELL_PATH_RESOLVER_H
