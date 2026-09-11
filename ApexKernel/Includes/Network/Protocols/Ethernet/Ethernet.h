// ApexKernel/Includes/Network/Protocols/Ethernet/Ethernet.h
// ApexOS -- Ethernet II framing (IEEE 802.3, EtherType variant)
//
// Sits directly on top of Drivers/Network/Vendors/Intel/E1000.c: that
// driver only knows how to send/receive opaque byte blobs (whole
// frames), it has no idea what's inside them. This layer adds the
// structure -- prepending/parsing the 14-byte header (destination
// MAC, source MAC, EtherType) so upper layers (ARP, IPv4, see
// Network/Protocols/ARP/, Network/Protocols/IPv4/) can work in terms
// of "send this payload to this MAC as this EtherType" / "I got an
// ARP packet from this MAC" instead of raw frames.
//
// Endianness note: x86 is little-endian, but every multi-byte field
// on the wire in Ethernet/ARP/IPv4/ICMP is big-endian ("network byte
// order") -- htons/ntohs below convert between the two. Defined here
// rather than in Core/Types.h since Ethernet is the first layer that
// needs them, and every protocol layered on top of it (ARP, IPv4,
// ICMP, ...) includes this header anyway.

#ifndef APEX_NETWORK_PROTOCOLS_ETHERNET_H
#define APEX_NETWORK_PROTOCOLS_ETHERNET_H

#include "Core/Types.h"

#define ETH_MAC_LEN 6
#define ETH_HEADER_LEN 14

// Standard Ethernet MTU (payload only, not counting this header) --
// matches E1000_RX_BUFFER_SIZE/E1000_TX_BUFFER_SIZE's headroom (see
// Drivers/Network/Vendors/Intel/E1000.h), so eth_send() never needs
// to fragment.
#define ETH_MTU 1500

// EtherType values this project cares about -- ARP now, IPv4 once
// that layer exists (see the project roadmap: Ethernet -> ARP ->
// IPv4 -> ICMP). Values are the well-known IANA-assigned constants,
// already in host byte order here; eth_send()/callers compare against
// the host-order field this header's parsing already converts (see
// eth_frame_t.ethertype below), never against raw wire bytes.
#define ETH_TYPE_IPV4 0x0800
#define ETH_TYPE_ARP 0x0806

// Broadcast destination -- all-FF, used by ARP requests (see
// Network/Protocols/ARP/) before the destination's MAC is known.
extern const uint8_t ETH_BROADCAST_MAC[ETH_MAC_LEN];

// Converts a 16-bit value between host (little-endian, x86) and
// network (big-endian) byte order. Its own inverse -- same
// implementation covers both directions, matching the well-known
// BSD sockets htons()/ntohs() naming so the intent is obvious to
// anyone who's touched networking code before.
uint16_t eth_htons(uint16_t host_value);
#define eth_ntohs eth_htons

// 32-bit counterpart of eth_htons() -- built from two eth_htons()
// calls on the high/low 16-bit halves, with the halves themselves
// swapped into the other's position, which together reverse all 4
// bytes (the definition of a 32-bit byte-order swap). Needed by TCP
// (Network/Protocols/TCP/) for its 32-bit sequence/acknowledgment
// numbers -- Ethernet/ARP/IPv4/ICMP/UDP only ever needed 16-bit
// fields, so this didn't exist until TCP did. Also its own inverse,
// same as eth_htons().
uint32_t eth_htonl(uint32_t host_value);
#define eth_ntohl eth_htonl

// Wire layout of the 14-byte Ethernet II header -- matches the
// on-the-wire byte order exactly (dest MAC, then src MAC, then
// EtherType, the latter big-endian -- see this file's header comment
// on endianness). Packed since this is read/written directly against
// frame buffers, not just an in-memory convenience struct.
typedef struct __attribute__((packed))
{
    uint8_t dest_mac[ETH_MAC_LEN];
    uint8_t src_mac[ETH_MAC_LEN];
    uint16_t ethertype; // big-endian on the wire
} eth_header_t;

// A received frame, already parsed and host-byte-order-normalized --
// what eth_poll_receive() hands back to callers. `payload` points
// inside the caller-owned buffer passed to eth_poll_receive() (right
// after the 14-byte header), not a separate copy.
typedef struct
{
    uint8_t dest_mac[ETH_MAC_LEN];
    uint8_t src_mac[ETH_MAC_LEN];
    uint16_t ethertype; // host byte order (already converted)
    const uint8_t *payload;
    uint16_t payload_len;
} eth_frame_t;

// Must be called once before eth_send()/eth_poll_receive() are used
// (after e1000_init() has already brought the NIC up) -- caches this
// NIC's own MAC address (via e1000_get_mac()) so eth_send() doesn't
// need it passed in on every call.
void eth_init(void);

// Returns this NIC's own MAC address (cached at eth_init() time).
// Used by ARP (Network/Protocols/ARP/) to fill in its own hardware
// address field.
void eth_get_mac(uint8_t out_mac[ETH_MAC_LEN]);

// Builds a full Ethernet frame (header + payload) and hands it to
// e1000_send_packet(). `ethertype` must be given in host byte order
// (e.g. ETH_TYPE_ARP) -- this function converts it to network order
// itself. Returns false if payload_len exceeds ETH_MTU or the
// underlying e1000_send_packet() call fails (TX ring full).
bool eth_send(const uint8_t dest_mac[ETH_MAC_LEN], uint16_t ethertype,
              const void *payload, uint16_t payload_len);

// Polls the NIC (via e1000_poll_receive()) for one frame and, if one
// arrived, parses its header into `out_frame` -- out_frame->payload
// points into `buffer`, so `buffer` must stay valid as long as
// out_frame is used, and must be at least E1000_RX_BUFFER_SIZE bytes
// (same requirement e1000_poll_receive() itself has). Returns false
// if no frame is currently available (nothing to do this poll) or the
// frame is shorter than ETH_HEADER_LEN (malformed/truncated -- silently
// dropped, not something the network stack's main loop can act on).
bool eth_poll_receive(void *buffer, eth_frame_t *out_frame);

#endif // APEX_NETWORK_PROTOCOLS_ETHERNET_H
