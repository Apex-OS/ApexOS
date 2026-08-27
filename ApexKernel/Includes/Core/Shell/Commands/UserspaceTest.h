// ApexKernel/Includes/Core/Shell/Commands/UserspaceTest.h
// ApexOS -- Ring 3 smoke-test command

#ifndef APEX_CORE_SHELL_COMMANDS_USERSPACE_TEST_H
#define APEX_CORE_SHELL_COMMANDS_USERSPACE_TEST_H

// Jumps user_main into Ring 3 as a quick smoke test of the
// userspace/syscall path, independent of the AXF loader. Manipulates
// the shell_return_bypass jump mechanism directly -- the same one
// axf_load_and_run() relies on (see
// ApexKernel/Formats/AXF/AXP/ProcessLaunch.c).
void cmd_ring3(void);

#endif // APEX_CORE_SHELL_COMMANDS_USERSPACE_TEST_H
