// ApexKernel/Network/Protocols/UDP/UDP.c
// ApexOS -- UDP (see UDP.h)

#include "Network/Protocols/UDP/UDP.h"
#include "Network/Protocols/Ethernet/Ethernet.h"

// Scratch buffer for the datagram being assembled in udp_send() --
// same static-buffer reasoning as every other layer in this stack
// (see Ethernet.c/ARP.c/IPv4.c/ICMP.c's own g_tx_*/g_rx_buffer
// comments).
static uint8_t g_tx_datagram[UDP_HEADER_LEN + ETH_MTU];

// Pseudo-header layout (RFC 768) used only for the checksum
// computation -- never sent on the wire itself, hence not exposed in
// UDP.h alongside udp_header_t (which IS the wire format). Built into
// a scratch buffer just ahead of the real UDP header/data so the
// whole thing can be checksummed in one contiguous pass (see
// ipv4_checksum()'s signature -- it takes one buffer, not multiple
// pieces to sum across separately).
typedef struct __attribute__((packed))
{
    uint8_t src_ip[IPV4_ADDR_LEN];
    uint8_t dest_ip[IPV4_ADDR_LEN];
    uint8_t zero;
    uint8_t protocol;
    uint16_t udp_length; // big-endian, same value as udp_header_t.length
} udp_pseudo_header_t;

#define UDP_PSEUDO_HEADER_LEN 12

// Scratch buffer holding pseudo-header + UDP header + data
// contiguously, purely for checksum computation -- built and
// discarded within udp_send()/udp_poll_receive(), never itself sent
// or received (the pseudo-header has no wire representation, see its
// struct's comment above).
static uint8_t g_checksum_scratch[UDP_PSEUDO_HEADER_LEN + UDP_HEADER_LEN + ETH_MTU];

static uint16_t compute_udp_checksum(const uint8_t src_ip[IPV4_ADDR_LEN],
                                     const uint8_t dest_ip[IPV4_ADDR_LEN],
                                     const void *udp_message, uint16_t udp_message_len)
{
    udp_pseudo_header_t pseudo;
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
    {
        pseudo.src_ip[i] = src_ip[i];
        pseudo.dest_ip[i] = dest_ip[i];
    }
    pseudo.zero = 0;
    pseudo.protocol = IPV4_PROTO_UDP;
    pseudo.udp_length = eth_htons(udp_message_len);

    uint8_t *scratch = g_checksum_scratch;
    for (int i = 0; i < UDP_PSEUDO_HEADER_LEN; i++)
        scratch[i] = ((const uint8_t *)&pseudo)[i];

    const uint8_t *msg = (const uint8_t *)udp_message;
    for (uint16_t i = 0; i < udp_message_len; i++)
        scratch[UDP_PSEUDO_HEADER_LEN + i] = msg[i];

    return ipv4_checksum(scratch, (uint16_t)(UDP_PSEUDO_HEADER_LEN + udp_message_len));
}

bool udp_send(const uint8_t dest_ip[IPV4_ADDR_LEN], uint16_t dest_port, uint16_t src_port,
              const void *payload, uint16_t payload_len)
{
    if ((uint32_t)UDP_HEADER_LEN + payload_len > ETH_MTU)
        return false;

    udp_header_t *hdr = (udp_header_t *)g_tx_datagram;
    hdr->src_port = eth_htons(src_port);
    hdr->dest_port = eth_htons(dest_port);
    uint16_t total_len = (uint16_t)(UDP_HEADER_LEN + payload_len);
    hdr->length = eth_htons(total_len);
    hdr->checksum = 0; // zeroed before computing -- same convention as every other checksum in this stack

    uint8_t *dst = g_tx_datagram + UDP_HEADER_LEN;
    const uint8_t *src = (const uint8_t *)payload;
    for (uint16_t i = 0; i < payload_len; i++)
        dst[i] = src[i];

    uint8_t own_ip[IPV4_ADDR_LEN];
    ipv4_get_own_ip(own_ip);
    uint16_t checksum = compute_udp_checksum(own_ip, dest_ip, g_tx_datagram, total_len);

    // RFC 768: a computed checksum of exactly 0x0000 is sent as
    // all-ones (0xFFFF) instead, since 0x0000 on the wire means "no
    // checksum was computed" (see UDP.h's header comment on
    // udp_poll_receive() accepting that convention on the receive
    // side too). An all-ones checksum this way is still valid --
    // this is the one case where the field isn't the literal
    // complement of the sum.
    hdr->checksum = eth_htons(checksum == 0x0000 ? 0xFFFF : checksum);

    return ipv4_send(dest_ip, IPV4_PROTO_UDP, g_tx_datagram, total_len);
}

bool udp_poll_receive(void *buffer, udp_datagram_t *out_datagram)
{
    ipv4_packet_t packet;
    if (!ipv4_poll_receive(buffer, &packet))
        return false;

    if (packet.protocol != IPV4_PROTO_UDP)
        return false;

    if (packet.payload_len < UDP_HEADER_LEN)
        return false; // truncated -- not actionable, drop

    const udp_header_t *hdr = (const udp_header_t *)packet.payload;

    // RFC 768: an all-zero checksum field means the sender chose not
    // to compute one -- treated as valid rather than rejected (see
    // UDP.h's header comment on this exact behavior). Any other
    // value is verified against a freshly recomputed checksum over
    // the same pseudo-header + message this sender would have used.
    if (hdr->checksum != 0)
    {
        uint16_t check = compute_udp_checksum(packet.src_ip, packet.dest_ip,
                                              packet.payload, packet.payload_len);
        if (check != 0x0000)
            return false; // corrupted in transit -- see IPv4.h's header comment on this same 0x0000 property
    }

    out_datagram->src_port = eth_ntohs(hdr->src_port);
    out_datagram->dest_port = eth_ntohs(hdr->dest_port);
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
        out_datagram->src_ip[i] = packet.src_ip[i];

    uint16_t length = eth_ntohs(hdr->length);
    if (length < UDP_HEADER_LEN || length > packet.payload_len)
        return false; // malformed length field -- not actionable, drop

    out_datagram->data = packet.payload + UDP_HEADER_LEN;
    out_datagram->data_len = (uint16_t)(length - UDP_HEADER_LEN);

    return true;
}
