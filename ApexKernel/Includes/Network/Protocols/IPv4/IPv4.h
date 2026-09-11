// ApexKernel/Includes/Network/Protocols/IPv4/IPv4.h
// ApexOS -- IPv4 (RFC 791), no fragmentation, no options
//
// Sits on top of Network/Protocols/Ethernet/Ethernet.h and
// Network/Protocols/ARP/ARP.h: given a destination IPv4 address,
// ipv4_send() resolves its MAC via arp_resolve() and hands the
// finished packet to eth_send(). ICMP (ping, next and last step in
// the project roadmap) is built on top of this the same way ARP is
// built on top of Ethernet -- ICMP doesn't touch the NIC or MAC
// addresses at all, it just asks this layer to deliver a payload to
// an IP address.
//
// V1 scope: single local IPv4 address (no interfaces table), no
// fragmentation (every packet this project sends/receives fits in one
// Ethernet frame -- true for ICMP echo, all this needs for `ping`),
// no IP options (20-byte header only, IHL always 5). Routing is
// minimal but real: a fixed /24 subnet mask (this project's whole
// network setup is QEMU user-mode's default 10.0.2.0/24, see the
// Makefile's `run` target) and a single configurable default
// gateway (see ipv4_set_gateway() below) -- destinations outside the
// local /24 are sent to the gateway's MAC instead of trying (and
// failing) to ARP-resolve a host that's actually behind a router.
// No real routing table beyond that one gateway entry.

#ifndef APEX_NETWORK_PROTOCOLS_IPV4_H
#define APEX_NETWORK_PROTOCOLS_IPV4_H

#include "Core/Types.h"

#define IPV4_ADDR_LEN 4
#define IPV4_HEADER_LEN 20    // no options, IHL=5 (5 * 4 bytes)
#define IPV4_VERSION_IHL 0x45 // version 4, IHL 5 -- packed into one byte

#define IPV4_DEFAULT_TTL 64 // conventional default (Linux/BSD use the same)

// Protocol field values (IANA-assigned) this project's upper layers
// care about -- ICMP now (the whole point of the roadmap's ping
// goal), UDP/TCP left as future work per the project roadmap.
#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP 6
#define IPV4_PROTO_UDP 17

// Wire layout of a 20-byte IPv4 header (RFC 791 section 3.1), no
// options -- matches on-the-wire byte order exactly except where
// noted; every multi-byte field is big-endian on the wire (see
// Ethernet.h's header comment on why eth_htons()/eth_ntohs() exist).
// Packed since this is read/written directly against Ethernet frame
// payloads.
typedef struct __attribute__((packed))
{
    uint8_t version_ihl;     // version (4 bits) + IHL (4 bits), see IPV4_VERSION_IHL
    uint8_t dscp_ecn;        // unused by this project -- always 0 on send
    uint16_t total_length;   // big-endian: header + payload, in bytes
    uint16_t identification; // big-endian: fragmentation ID, see ipv4_send()'s comment
    uint16_t flags_fragment; // big-endian: flags (3 bits) + fragment offset (13 bits)
    uint8_t ttl;
    uint8_t protocol;  // IPV4_PROTO_* above
    uint16_t checksum; // big-endian: header-only checksum, see ipv4_checksum()
    uint8_t src_ip[IPV4_ADDR_LEN];
    uint8_t dest_ip[IPV4_ADDR_LEN];
} ipv4_header_t;

// A received IPv4 packet, already parsed and validated (checksum
// verified, destination matches our own IP) -- what
// ipv4_poll_receive() hands back to callers. `payload` points inside
// the caller-owned buffer passed down to ipv4_poll_receive() (right
// after the 20-byte header), not a separate copy.
typedef struct
{
    uint8_t src_ip[IPV4_ADDR_LEN];
    uint8_t dest_ip[IPV4_ADDR_LEN];
    uint8_t protocol;
    const uint8_t *payload;
    uint16_t payload_len;
} ipv4_packet_t;

// Computes the RFC 791 Internet checksum (one's complement sum of
// 16-bit words, folded and complemented) over `data`/`length` bytes.
// Used both to fill ipv4_header_t.checksum on send (with that field
// itself zeroed first) and to verify it on receive (summing the
// whole header including the received checksum should yield exactly
// 0x0000 if nothing was corrupted -- see this file's .c for the
// exact check). Exposed here (not static in the .c) because ICMP
// (Network/Protocols/ICMP/, next in the roadmap) needs the same
// algorithm for its own checksum field -- RFC 792 explicitly reuses
// RFC 791's checksum computation.
uint16_t ipv4_checksum(const void *data, uint16_t length);

// Must be called once before ipv4_send()/ipv4_poll_receive() are
// used -- after arp_init() (see ARP.h), stores this machine's own
// IPv4 address (used to fill in the source field of every packet
// this code sends, and to recognize which received packets are
// addressed to us).
void ipv4_init(const uint8_t own_ip[IPV4_ADDR_LEN]);

// Sets the default gateway's IPv4 address -- must be called before
// ipv4_send() is used to reach anything off the local subnet (see
// this file's header comment on the /24 assumption). Without a
// gateway configured, ipv4_send() can only reach addresses on the
// same /24 as this machine's own IP (arp_resolve() would time out
// trying to ARP for a host that's actually behind a router, since
// ARP only works within one Ethernet broadcast domain).
void ipv4_set_gateway(const uint8_t gateway_ip[IPV4_ADDR_LEN]);

// Returns this machine's own IPv4 address (cached at ipv4_init()
// time). Used by UDP/TCP (Network/Protocols/UDP/,
// Network/Protocols/TCP/) to build their pseudo-header checksum
// (RFC 768 / RFC 793 both require the source IP in the checksum
// computation, not just the UDP/TCP header itself).
void ipv4_get_own_ip(uint8_t out_ip[IPV4_ADDR_LEN]);

// Builds a full IPv4 packet (20-byte header + payload) and hands it
// to eth_send() -- resolves dest_ip's MAC via arp_resolve() first
// (see ARP.h), 2-second timeout, same blocking-with-timeout
// reasoning as arp_resolve() itself (see that function's header
// comment). Returns false if: payload_len is too large to fit in one
// Ethernet frame (IPV4_HEADER_LEN + payload_len > ETH_MTU -- no
// fragmentation, see this file's header comment on V1 scope),
// arp_resolve() times out (destination unreachable/not answering),
// or the underlying eth_send() fails.
bool ipv4_send(const uint8_t dest_ip[IPV4_ADDR_LEN], uint8_t protocol,
               const void *payload, uint16_t payload_len);

// Polls the network (via eth_poll_receive()) for one IPv4 packet
// addressed to this machine and, if one arrived, parses it into
// `out_packet` -- out_packet->payload points into `buffer`, so
// `buffer` must stay valid as long as out_packet is used, and must be
// at least E1000_RX_BUFFER_SIZE bytes (same requirement
// eth_poll_receive() itself has). Returns false if: no frame is
// currently available, the frame isn't ETH_TYPE_IPV4, the packet is
// truncated/checksum-invalid (silently dropped -- not something a
// polling caller can act on), or its destination isn't our own IP
// (not addressed to us -- also silently ignored, this project has no
// promiscuous-mode use case).
bool ipv4_poll_receive(void *buffer, ipv4_packet_t *out_packet);

#endif // APEX_NETWORK_PROTOCOLS_IPV4_H
