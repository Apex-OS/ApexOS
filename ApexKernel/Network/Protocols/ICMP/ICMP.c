// ApexKernel/Network/Protocols/ICMP/ICMP.c
// ApexOS -- ICMP Echo Request/Reply (see ICMP.h)

#include "Network/Protocols/ICMP/ICMP.h"
#include "Network/Protocols/Ethernet/Ethernet.h"

// Scratch buffer for the message being assembled in
// icmp_send_echo_request() -- same static-buffer reasoning as every
// other layer in this stack (see Ethernet.c/ARP.c/IPv4.c's own
// g_tx_*/g_rx_buffer comments). Sized to the largest payload IPv4 can
// carry in one unfragmented Ethernet frame.
static uint8_t g_tx_message[ETH_MTU];

bool icmp_send_echo_request(const uint8_t dest_ip[IPV4_ADDR_LEN], uint16_t identifier,
                            uint16_t sequence, const void *data, uint16_t data_len)
{
    if ((uint32_t)ICMP_HEADER_LEN + data_len > ETH_MTU)
        return false;

    icmp_header_t *hdr = (icmp_header_t *)g_tx_message;
    hdr->type = ICMP_TYPE_ECHO_REQUEST;
    hdr->code = ICMP_CODE_ECHO;
    hdr->identifier = eth_htons(identifier);
    hdr->sequence = eth_htons(sequence);
    hdr->checksum = 0; // zeroed before computing -- same convention as IPv4's header checksum

    uint8_t *dst = g_tx_message + ICMP_HEADER_LEN;
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < data_len; i++)
        dst[i] = src[i];

    // ICMP's checksum (RFC 792) covers the whole ICMP message --
    // header plus data -- not just the header the way IPv4's own
    // checksum only covers its 20-byte header. Same algorithm either
    // way (RFC 792 explicitly reuses RFC 791's), see
    // ipv4_checksum()'s header comment for why it's exposed for
    // exactly this reuse.
    uint16_t total_len = (uint16_t)(ICMP_HEADER_LEN + data_len);
    hdr->checksum = eth_htons(ipv4_checksum(g_tx_message, total_len));

    return ipv4_send(dest_ip, IPV4_PROTO_ICMP, g_tx_message, total_len);
}

bool icmp_poll_receive(void *buffer, icmp_echo_reply_t *out_reply)
{
    ipv4_packet_t packet;
    if (!ipv4_poll_receive(buffer, &packet))
        return false;

    if (packet.protocol != IPV4_PROTO_ICMP)
        return false;

    if (packet.payload_len < ICMP_HEADER_LEN)
        return false; // truncated -- not actionable, drop

    const icmp_header_t *hdr = (const icmp_header_t *)packet.payload;

    if (ipv4_checksum(packet.payload, packet.payload_len) != 0x0000)
        return false; // corrupted in transit -- valid checksum sums to 0x0000, see IPv4.h's header comment

    if (hdr->type != ICMP_TYPE_ECHO_REPLY)
        return false; // not a reply (e.g. someone else's Echo Request) -- see ICMP.h's header comment on V1 scope

    for (int i = 0; i < IPV4_ADDR_LEN; i++)
        out_reply->src_ip[i] = packet.src_ip[i];
    out_reply->identifier = eth_ntohs(hdr->identifier);
    out_reply->sequence = eth_ntohs(hdr->sequence);
    out_reply->data = packet.payload + ICMP_HEADER_LEN;
    out_reply->data_len = (uint16_t)(packet.payload_len - ICMP_HEADER_LEN);

    return true;
}
