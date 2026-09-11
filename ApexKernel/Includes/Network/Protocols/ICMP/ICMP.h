// ApexKernel/Includes/Network/Protocols/ICMP/ICMP.h
// ApexOS -- ICMP Echo Request/Reply (RFC 792), the "ping" protocol
//
// Sits on top of Network/Protocols/IPv4/IPv4.h -- the last layer in
// the project roadmap's networking stack (e1000 -> Ethernet -> ARP ->
// IPv4 -> ICMP). Everything below this already handles addressing
// (ARP), routing to a MAC (Ethernet), and delivery to an IP
// (IPv4/ipv4_send() and ipv4_poll_receive()) -- ICMP just adds the
// specific 8-byte header and type/code values that make a packet a
// ping request or reply, and hands its payload straight to
// ipv4_send()/reads it straight from ipv4_poll_receive().
//
// V1 scope: only Echo Request (type 8) and Echo Reply (type 0) --
// this project's whole goal here is `ping` working end to end, not a
// general ICMP implementation (no Destination Unreachable, no Time
// Exceeded, no Redirect -- those are host/router error-reporting
// messages this project's kernel doesn't need to originate or handle
// yet).

#ifndef APEX_NETWORK_PROTOCOLS_ICMP_H
#define APEX_NETWORK_PROTOCOLS_ICMP_H

#include "Core/Types.h"
#include "Network/Protocols/IPv4/IPv4.h"

#define ICMP_HEADER_LEN 8

// ICMP message types (RFC 792) this project originates/handles --
// already host byte order here; icmp_send_echo_request() writes the
// type field directly since it's a single byte (no endianness to
// convert, unlike the 16-bit id/sequence/checksum fields).
#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_ECHO_REQUEST 8
#define ICMP_CODE_ECHO 0 // the only defined code for both echo types

// Wire layout of an ICMP Echo Request/Reply header (RFC 792) --
// identical 8-byte shape for both types, only the `type` field
// differs (ICMP_TYPE_ECHO_REQUEST vs ICMP_TYPE_ECHO_REPLY). Packed
// since this is read/written directly against IPv4 packet payloads.
// Any bytes after this header (up to whatever length the caller
// sent/received) are opaque echo data -- RFC 792 says a reply must
// echo back whatever data the request carried, unexamined.
typedef struct __attribute__((packed))
{
    uint8_t type;
    uint8_t code;
    uint16_t checksum;   // big-endian: RFC 791 checksum over this whole message (header + data)
    uint16_t identifier; // big-endian: caller-chosen, matches requests to replies
    uint16_t sequence;   // big-endian: caller-chosen, typically incremented per request
} icmp_header_t;

// A received Echo Reply, already parsed -- what icmp_poll_receive()
// hands back to callers. `data` points inside the caller-owned buffer
// passed down to icmp_poll_receive() (right after the 8-byte ICMP
// header), not a separate copy.
typedef struct
{
    uint8_t src_ip[IPV4_ADDR_LEN];
    uint16_t identifier;
    uint16_t sequence;
    const uint8_t *data;
    uint16_t data_len;
} icmp_echo_reply_t;

// Sends an ICMP Echo Request to dest_ip -- `data`/`data_len` is
// arbitrary payload the peer is expected to echo back unchanged (RFC
// 792), commonly a timestamp or pattern bytes for round-trip timing
// (see the shell's `ping` command, Core/Shell/Commands/
// NetworkCommands.c, for how this project uses it). Blocks for up to
// 2 seconds resolving dest_ip's MAC if it isn't already ARP-cached
// (see ipv4_send()/arp_resolve()) -- does NOT wait for the reply
// itself, that's icmp_poll_receive()'s job, called separately by the
// ping command's own loop so it can time the round trip and/or send
// several requests without each one blocking on the last one's reply.
// Returns false if the underlying ipv4_send() fails (ARP timeout,
// packet too large, etc).
bool icmp_send_echo_request(const uint8_t dest_ip[IPV4_ADDR_LEN], uint16_t identifier,
                            uint16_t sequence, const void *data, uint16_t data_len);

// Polls the network (via ipv4_poll_receive()) for one ICMP Echo Reply
// and, if one arrived, parses it into `out_reply` -- out_reply->data
// points into `buffer`, so `buffer` must stay valid as long as
// out_reply is used, and must be at least E1000_RX_BUFFER_SIZE bytes
// (same requirement ipv4_poll_receive()/eth_poll_receive() have).
// Returns false if no IPv4 packet is currently available, it isn't
// ICMP, its checksum doesn't validate, or its type isn't
// ICMP_TYPE_ECHO_REPLY (e.g. it's an Echo Request from someone else
// pinging us -- silently ignored, not answered: this project's ping
// command doesn't also implement being a ping responder, see this
// file's header comment on V1 scope).
bool icmp_poll_receive(void *buffer, icmp_echo_reply_t *out_reply);

#endif // APEX_NETWORK_PROTOCOLS_ICMP_H
