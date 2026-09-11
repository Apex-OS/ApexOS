// ApexKernel/Network/Protocols/IPv4/IPv4.c
// ApexOS -- IPv4 (see IPv4.h)

#include "Network/Protocols/IPv4/IPv4.h"
#include "Network/Protocols/Ethernet/Ethernet.h"
#include "Network/Protocols/ARP/ARP.h"

// Scratch buffer for the packet being assembled in ipv4_send() --
// same static-buffer reasoning as Ethernet.c's g_tx_frame and
// ARP.c's g_rx_buffer (see those files): this project's polling loop
// isn't re-entrant, so one shared buffer per direction is enough and
// avoids a page-sized stack allocation on every send.
static uint8_t g_tx_packet[IPV4_HEADER_LEN + ETH_MTU];

static uint8_t g_own_ip[IPV4_ADDR_LEN];
static uint8_t g_gateway_ip[IPV4_ADDR_LEN];
static bool g_gateway_set = false;

// Fixed /24 mask -- see IPv4.h's header comment on this file's
// routing scope. Not configurable yet since this project's only
// network environment so far (QEMU user-mode, 10.0.2.0/24) always
// uses one.
#define IPV4_SUBNET_MASK 0xFFFFFF00u

static uint32_t ip_to_u32(const uint8_t ip[IPV4_ADDR_LEN])
{
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) | ((uint32_t)ip[2] << 8) | ip[3];
}

static bool same_subnet(const uint8_t a[IPV4_ADDR_LEN], const uint8_t b[IPV4_ADDR_LEN])
{
    return (ip_to_u32(a) & IPV4_SUBNET_MASK) == (ip_to_u32(b) & IPV4_SUBNET_MASK);
}

// Fragmentation ID counter -- incremented per packet sent (RFC 791's
// suggested behavior for identifying a datagram, even though this
// project never actually fragments -- see IPv4.h's header comment on
// V1 scope). A receiver is still allowed to see this field and
// harmless duplicates would only matter if fragmentation were in
// play, but incrementing it costs nothing and matches what a real
// stack does.
static uint16_t g_next_id = 1;

static bool ip_equal(const uint8_t a[IPV4_ADDR_LEN], const uint8_t b[IPV4_ADDR_LEN])
{
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
        if (a[i] != b[i])
            return false;
    return true;
}

uint16_t ipv4_checksum(const void *data, uint16_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0;

    // Sums 16-bit big-endian words -- reconstructed manually from
    // byte pairs rather than casting to uint16_t* to avoid any
    // assumption about this buffer's alignment (it's often a field
    // partway through a packed struct, not necessarily 2-byte
    // aligned).
    while (length > 1)
    {
        sum += ((uint32_t)bytes[0] << 8) | bytes[1];
        bytes += 2;
        length -= 2;
    }

    // Odd trailing byte (never happens for a 20-byte header, but this
    // function is also used by ICMP on payloads of arbitrary length,
    // see IPv4.h's header comment) -- padded with a zero low byte per
    // RFC 791.
    if (length == 1)
        sum += ((uint32_t)bytes[0] << 8);

    // Folds any carry out of the 16-bit range back in -- a one's
    // complement sum can overflow past 16 bits, and RFC 791 requires
    // wrapping that overflow back into the low 16 bits rather than
    // discarding it.
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)~sum;
}

void ipv4_init(const uint8_t own_ip[IPV4_ADDR_LEN])
{
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
        g_own_ip[i] = own_ip[i];
}

void ipv4_set_gateway(const uint8_t gateway_ip[IPV4_ADDR_LEN])
{
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
        g_gateway_ip[i] = gateway_ip[i];
    g_gateway_set = true;
}

void ipv4_get_own_ip(uint8_t out_ip[IPV4_ADDR_LEN])
{
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
        out_ip[i] = g_own_ip[i];
}

bool ipv4_send(const uint8_t dest_ip[IPV4_ADDR_LEN], uint8_t protocol,
               const void *payload, uint16_t payload_len)
{
    if ((uint32_t)IPV4_HEADER_LEN + payload_len > ETH_MTU)
        return false;

    // Routing decision: ARP only works within one Ethernet broadcast
    // domain, so a destination outside our own /24 must be sent to
    // the gateway's MAC instead -- the gateway is the one that
    // actually knows how to reach it. The IP header itself always
    // carries the real destination IP either way (see below); only
    // the Ethernet-layer destination MAC changes here, exactly how
    // real IP routing works (the IP layer doesn't know or care that
    // the frame is being handed to a router rather than the final
    // host).
    const uint8_t *arp_target = dest_ip;
    if (!same_subnet(dest_ip, g_own_ip))
    {
        if (!g_gateway_set)
            return false; // off-subnet destination with no gateway configured -- unreachable
        arp_target = g_gateway_ip;
    }

    uint8_t dest_mac[ETH_MAC_LEN];
    if (!arp_resolve(arp_target, dest_mac, 2000))
        return false; // destination/gateway unreachable / not answering ARP

    ipv4_header_t *hdr = (ipv4_header_t *)g_tx_packet;
    hdr->version_ihl = IPV4_VERSION_IHL;
    hdr->dscp_ecn = 0;
    hdr->total_length = eth_htons((uint16_t)(IPV4_HEADER_LEN + payload_len));
    hdr->identification = eth_htons(g_next_id++);
    hdr->flags_fragment = eth_htons(0); // no flags, no fragmentation (see IPv4.h's header comment)
    hdr->ttl = IPV4_DEFAULT_TTL;
    hdr->protocol = protocol;
    hdr->checksum = 0; // zeroed before computing -- see ipv4_checksum()'s header comment
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
    {
        hdr->src_ip[i] = g_own_ip[i];
        hdr->dest_ip[i] = dest_ip[i];
    }
    hdr->checksum = eth_htons(ipv4_checksum(hdr, IPV4_HEADER_LEN));

    uint8_t *dst = g_tx_packet + IPV4_HEADER_LEN;
    const uint8_t *src = (const uint8_t *)payload;
    for (uint16_t i = 0; i < payload_len; i++)
        dst[i] = src[i];

    return eth_send(dest_mac, ETH_TYPE_IPV4, g_tx_packet, (uint16_t)(IPV4_HEADER_LEN + payload_len));
}

bool ipv4_poll_receive(void *buffer, ipv4_packet_t *out_packet)
{
    eth_frame_t frame;
    if (!eth_poll_receive(buffer, &frame))
        return false;

    if (frame.ethertype != ETH_TYPE_IPV4)
        return false;

    if (frame.payload_len < IPV4_HEADER_LEN)
        return false; // truncated -- not actionable, drop

    const ipv4_header_t *hdr = (const ipv4_header_t *)frame.payload;

    // Only version 4, no-options headers are handled (see IPv4.h's
    // header comment on V1 scope) -- anything else is silently
    // ignored rather than misinterpreted (e.g. reading past the
    // fixed 20-byte header as if it were payload when IHL > 5).
    if (hdr->version_ihl != IPV4_VERSION_IHL)
        return false;

    // Checksum validation: summing the whole header (including the
    // checksum field as received, not zeroed) with the same
    // algorithm used to compute it must yield exactly 0x0000 if
    // nothing was corrupted in transit -- a standard property of the
    // one's complement checksum, cheaper to check this way than
    // re-zeroing the field and comparing against a freshly computed
    // value.
    if (ipv4_checksum(hdr, IPV4_HEADER_LEN) != 0x0000)
        return false;

    if (!ip_equal(hdr->dest_ip, g_own_ip))
        return false; // not addressed to us -- see IPv4.h's header comment

    for (int i = 0; i < IPV4_ADDR_LEN; i++)
    {
        out_packet->src_ip[i] = hdr->src_ip[i];
        out_packet->dest_ip[i] = hdr->dest_ip[i];
    }
    out_packet->protocol = hdr->protocol;

    uint16_t total_length = eth_ntohs(hdr->total_length);
    if (total_length < IPV4_HEADER_LEN || total_length > frame.payload_len)
        return false; // malformed length field -- not actionable, drop

    out_packet->payload = frame.payload + IPV4_HEADER_LEN;
    out_packet->payload_len = (uint16_t)(total_length - IPV4_HEADER_LEN);

    return true;
}
