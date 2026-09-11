// ApexKernel/Includes/Core/Shell/Commands/DiagnosticCommands.h
// ApexOS -- Shell hardware diagnostic/test commands

#ifndef APEX_CORE_SHELL_COMMANDS_DIAGNOSTIC_H
#define APEX_CORE_SHELL_COMMANDS_DIAGNOSTIC_H

void cmd_gfxtest(void);
void cmd_disktest(void);
void cmd_apexui(void);

// TEST TEMPORAIRE (réseau) -- valide Ethernet+ARP de bout en bout
// avant que IPv4/ICMP existent (voir la roadmap réseau du projet).
// `args` : une IP en notation décimale pointée ("A.B.C.D"), ou une
// chaîne vide pour la valeur par défaut (10.0.2.2, la gateway que
// QEMU expose toujours en mode "-netdev user"). A retirer une fois
// que `ping` (au-dessus d'ICMP) existe et rend ce test redondant.
void cmd_arptest(const char *args);

// Sends ICMP Echo Requests to an IPv4 address and reports round-trip
// time for each reply, like the standard Unix `ping` tool -- see
// Network/Protocols/ICMP/ICMP.h. `args`: an IP in "A.B.C.D" notation
// optionally followed by a space and a request count (default 4), or
// an empty string for the default target (10.0.2.2, the QEMU
// user-mode gateway -- same default as cmd_arptest, always present
// under `make run`).
void cmd_ping(const char *args);

// TEST TEMPORAIRE (réseau) -- valide UDP end to end (construction,
// pseudo-header checksum, parsing) against a real peer: sends a
// minimal DNS query (RFC 1035) to QEMU's user-mode built-in resolver
// at 10.0.2.3 and prints the resolved IPv4 address from the reply.
// See Network/Protocols/UDP/UDP.h. `args`: a hostname (default
// "example.com" if empty).
void cmd_dnstest(const char *args);

// Full TCP stack test -- resolves `args` (a hostname, default
// "example.com" if empty) via DNS, opens a TCP connection to port 80
// (three-way handshake, see Network/Protocols/TCP/TCP.h), sends a
// minimal HTTP/1.1 GET request, and prints whatever the server sends
// back before closing the connection. Exercises the entire stack in
// one command: Ethernet + ARP + IPv4 + UDP (for the DNS lookup) + TCP
// (handshake, data transfer, close) -- the project roadmap's final
// networking milestone.
void cmd_tcptest(const char *args);

// Toggles mirroring of every character shown on screen to the COM1
// serial port (see Drivers/Display/VGA.c's vga_set_serial_mirror()) --
// off by default so a normal interactive session doesn't spam
// whatever's attached to COM1, but very useful when debugging under
// QEMU's `-serial stdio` (see the Makefile's `run` target): lets
// verbose diagnostic output (e.g. from ping/arptest/tcptest) be
// copy-pasted straight from the host terminal instead of read off
// the framebuffer, and avoids the on-screen console's rendering
// glitches on very long/fast output. `args`: "on" or "off".
void cmd_serialmirror(const char *args);

#endif // APEX_CORE_SHELL_COMMANDS_DIAGNOSTIC_H
