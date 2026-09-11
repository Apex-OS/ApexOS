// ApexKernel/Includes/Network/Protocols/ARP/ARP.h
// ApexOS -- Address Resolution Protocol (RFC 826), IPv4-over-Ethernet only
//
// Sits on top of Network/Protocols/Ethernet/Ethernet.h. Its whole job:
// given an IPv4 address, find the MAC address of the machine that
// owns it, so IPv4 (Network/Protocols/IPv4/, next in the project
// roadmap) knows which MAC to hand Ethernet when it wants to send a
// packet. Without this, IPv4 would have no way to address anything on
// the local network -- IP addresses mean nothing to the NIC/switch,
// only MAC addresses do.
//
// V1 scope: only handles the two opcodes this project's ping/IPv4
// traffic actually needs -- REQUEST (ask "who has this IP") and REPLY
// (answer "I do, here's my MAC"). No gratuitous ARP, no probing, no
// RARP.

#ifndef APEX_NETWORK_PROTOCOLS_ARP_H
#define APEX_NETWORK_PROTOCOLS_ARP_H

#include "Core/Types.h"
#include "Network/Protocols/Ethernet/Ethernet.h"

#define ARP_IPV4_LEN 4

// ARP opcodes (RFC 826) -- already host byte order here; arp_send()
// converts to network order internally, same convention
// Ethernet.h's ETH_TYPE_* constants follow.
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2

// ARP hardware/protocol type constants (RFC 826) -- fixed values for
// this project's only combination (Ethernet + IPv4), never anything
// else, so callers never need to touch these directly.
#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4 0x0800 // same value as ETH_TYPE_IPV4, different field

// Wire layout of an ARP packet for the Ethernet+IPv4 case (RFC 826
// section on the packet format) -- fixed 28 bytes, all multi-byte
// fields big-endian on the wire except the address bytes themselves
// (already byte arrays, no endianness to convert). Packed since this
// is read/written directly against Ethernet frame payloads.
typedef struct __attribute__((packed))
{
    uint16_t htype; // hardware type, ARP_HTYPE_ETHERNET
    uint16_t ptype; // protocol type, ARP_PTYPE_IPV4
    uint8_t hlen;   // hardware address length, 6
    uint8_t plen;   // protocol address length, 4
    uint16_t oper;  // ARP_OP_REQUEST / ARP_OP_REPLY
    uint8_t sender_mac[ETH_MAC_LEN];
    uint8_t sender_ip[ARP_IPV4_LEN];
    uint8_t target_mac[ETH_MAC_LEN];
    uint8_t target_ip[ARP_IPV4_LEN];
} arp_packet_t;

// Must be called once before arp_resolve() is used -- after
// eth_init(), needs this NIC's own MAC (via eth_get_mac()) and takes
// this machine's own IPv4 address (used to fill in the sender fields
// of every ARP request this code sends). No DHCP in this project yet
// (see the roadmap), so the address is simply assigned here rather
// than discovered.
void arp_init(const uint8_t own_ip[ARP_IPV4_LEN]);

// Resolves an IPv4 address to a MAC address. Checks the internal
// cache first; on a miss, sends an ARP request (broadcast) and
// blocks -- polling the NIC via eth_poll_receive() in a loop, see
// this file's implementation -- until a matching reply arrives or
// timeout_ms elapses (same pit_get_ticks()-based pattern as AHCI's
// ahci_wait_clear(), see Drivers/Storage/AHCI/PortControl.c). Returns
// true and fills out_mac on success, false on timeout (target not
// present on the network, or not answering).
//
// This blocks the calling context for up to timeout_ms -- acceptable
// for this project's current polling-only, single-threaded-ish
// network use (see E1000.h's header comment on why V1 has no
// interrupt/event-loop path yet), but callers on a real scheduled
// task should be aware a slow/absent target stalls them for the
// full timeout.
bool arp_resolve(const uint8_t target_ip[ARP_IPV4_LEN], uint8_t out_mac[ETH_MAC_LEN],
                 uint32_t timeout_ms);

// Called from the network stack's main poll loop for every received
// frame whose ethertype is ETH_TYPE_ARP (see eth_poll_receive()).
// Handles both directions: if it's a REQUEST for our own IP, sends
// back a REPLY; if it's a REPLY (to a request we sent), records the
// sender's IP->MAC mapping in the cache so a blocked arp_resolve()
// call picks it up on its next poll. Every valid ARP packet's
// sender IP/MAC is cached opportunistically either way (RFC 826's
// suggested behavior), not just replies to our own requests -- free
// information that saves a future arp_resolve() a round trip.
void arp_handle_packet(const eth_frame_t *frame);

#endif // APEX_NETWORK_PROTOCOLS_ARP_H
