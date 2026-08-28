// ApexKernel/Includes/Core/Shell/Shell.h
// ApexOS -- Kernel shell

#ifndef APEX_CORE_SHELL_H
#define APEX_CORE_SHELL_H

#include "Core/Types.h"

#define SHELL_BUFFER_SIZE 4096 // 4KB au lieu de 256
#define SHELL_PROMPT "ApexOS> "

void shell_init(void);
void shell_run(void); // Tourne comme un processus

#endif // APEX_CORE_SHELL_H
