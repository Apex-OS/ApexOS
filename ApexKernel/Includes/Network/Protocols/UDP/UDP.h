// ApexKernel/Includes/Network/Protocols/UDP/UDP.h
// ApexOS -- UDP (RFC 768)
//
// Sits on top of Network/Protocols/IPv4/IPv4.h, the same way ICMP
// does -- udp_send() hands its datagram straight to ipv4_send(), and
// udp_poll_receive() reads straight from ipv4_poll_receive(). Unlike
// ICMP, UDP is a transport-layer protocol with its own addressing
// concept (ports) on top of IP addresses -- this is the first layer
// in the stack where "who" a packet is for/from is more than just an
// IP address.
//
// V1 scope: no listening/binding model, no multiplexing by port on
// receive -- udp_poll_receive() just hands back whatever UDP
// datagram arrives next addressed to this machine, and the caller
// checks whether dest_port is the one it cares about. Good enough
// for this project's current single-purpose network testing (see the
// project roadmap); a real socket-like API with per-port receive
// queues is future work once there's more than one thing wanting UDP
// traffic at once.
//
// Checksum note: unlike IPv4's own header checksum (which only
// covers the IPv4 header) and ICMP's (which covers the ICMP message
// but nothing from IPv4), UDP's checksum (RFC 768) is computed over
// a "pseudo-header" -- source IP, dest IP, zero byte, protocol,
// UDP length -- prepended in front of the actual UDP header+data.
// This lets a receiver detect a UDP packet delivered to/from the
// wrong IP even though IP addresses aren't literally part of the UDP
// header on the wire. ipv4_get_own_ip() (see IPv4.h) exists
// specifically so this layer can build that pseudo-header.

#ifndef APEX_NETWORK_PROTOCOLS_UDP_H
#define APEX_NETWORK_PROTOCOLS_UDP_H

#include "Core/Types.h"
#include "Network/Protocols/IPv4/IPv4.h"

#define UDP_HEADER_LEN 8

// Wire layout of an 8-byte UDP header (RFC 768) -- matches on-the-wire
// byte order exactly, every multi-byte field big-endian (see
// Ethernet.h's header comment on why eth_htons()/eth_ntohs() exist).
// Packed since this is read/written directly against IPv4 packet
// payloads.
typedef struct __attribute__((packed))
{
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length; // header + data, in bytes (NOT including the pseudo-header)
    uint16_t checksum;
} udp_header_t;

// A received UDP datagram, already parsed and validated (checksum
// verified, destination IP matches our own -- checked one layer down
// by ipv4_poll_receive() already) -- what udp_poll_receive() hands
// back to callers. `data` points inside the caller-owned buffer
// passed down to udp_poll_receive() (right after the 8-byte UDP
// header), not a separate copy.
typedef struct
{
    uint8_t src_ip[IPV4_ADDR_LEN];
    uint16_t src_port;
    uint16_t dest_port;
    const uint8_t *data;
    uint16_t data_len;
} udp_datagram_t;

// Sends a UDP datagram to dest_ip:dest_port, from src_port on this
// machine. Blocks for up to 2 seconds resolving dest_ip's MAC if not
// already ARP-cached (same as icmp_send_echo_request() -- see
// ICMP.h's header comment on that same behavior, inherited here from
// the same underlying ipv4_send() call). Returns false if
// UDP_HEADER_LEN + payload_len doesn't fit in one Ethernet frame (no
// fragmentation, same V1 scope as IPv4 -- see IPv4.h), or the
// underlying ipv4_send() fails.
bool udp_send(const uint8_t dest_ip[IPV4_ADDR_LEN], uint16_t dest_port, uint16_t src_port,
              const void *payload, uint16_t payload_len);

// Polls the network (via ipv4_poll_receive()) for one UDP datagram
// addressed to this machine and, if one arrived, parses it into
// `out_datagram` -- out_datagram->data points into `buffer`, so
// `buffer` must stay valid as long as out_datagram is used, and must
// be at least E1000_RX_BUFFER_SIZE bytes (same requirement
// eth_poll_receive()/ipv4_poll_receive() have). Returns false if no
// IPv4 packet is currently available, it isn't UDP, or its checksum
// doesn't validate (RFC 768 allows an all-zero checksum field to mean
// "no checksum was computed" -- this implementation treats that as
// valid too, matching how most receivers handle it, rather than
// rejecting every packet a peer sends with checksumming disabled).
// Callers that only care about a specific port filter on
// out_datagram->dest_port themselves -- see this file's header
// comment on V1 scope.
bool udp_poll_receive(void *buffer, udp_datagram_t *out_datagram);

#endif // APEX_NETWORK_PROTOCOLS_UDP_H
