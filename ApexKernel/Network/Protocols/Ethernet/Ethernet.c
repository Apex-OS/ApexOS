// ApexKernel/Network/Protocols/Ethernet/Ethernet.c
// ApexOS -- Ethernet II framing (see Ethernet.h)

#include "Network/Protocols/Ethernet/Ethernet.h"
#include "Drivers/Network/Vendors/Intel/E1000.h"

const uint8_t ETH_BROADCAST_MAC[ETH_MAC_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint8_t g_own_mac[ETH_MAC_LEN];

// Scratch buffer for the frame being assembled in eth_send() -- one
// header's worth plus the largest payload this project sends
// (ETH_MTU), static rather than stack-allocated since this file's
// only caller (the network stack's main loop, see the project
// roadmap) never needs re-entrant sends and a ~1514-byte stack buffer
// per call is needless stack pressure in a kernel with no guard pages
// on its stacks yet.
static uint8_t g_tx_frame[ETH_HEADER_LEN + ETH_MTU];

uint16_t eth_htons(uint16_t host_value)
{
    return (uint16_t)((host_value << 8) | (host_value >> 8));
}

uint32_t eth_htonl(uint32_t host_value)
{
    return ((uint32_t)eth_htons((uint16_t)(host_value & 0xFFFF)) << 16) |
           eth_htons((uint16_t)(host_value >> 16));
}

void eth_init(void)
{
    e1000_get_mac(g_own_mac);
}

void eth_get_mac(uint8_t out_mac[ETH_MAC_LEN])
{
    for (int i = 0; i < ETH_MAC_LEN; i++)
        out_mac[i] = g_own_mac[i];
}

bool eth_send(const uint8_t dest_mac[ETH_MAC_LEN], uint16_t ethertype,
              const void *payload, uint16_t payload_len)
{
    if (payload_len > ETH_MTU)
        return false;

    eth_header_t *hdr = (eth_header_t *)g_tx_frame;
    for (int i = 0; i < ETH_MAC_LEN; i++)
    {
        hdr->dest_mac[i] = dest_mac[i];
        hdr->src_mac[i] = g_own_mac[i];
    }
    hdr->ethertype = eth_htons(ethertype);

    uint8_t *dst = g_tx_frame + ETH_HEADER_LEN;
    const uint8_t *src = (const uint8_t *)payload;
    for (uint16_t i = 0; i < payload_len; i++)
        dst[i] = src[i];

    return e1000_send_packet(g_tx_frame, (uint16_t)(ETH_HEADER_LEN + payload_len));
}

bool eth_poll_receive(void *buffer, eth_frame_t *out_frame)
{
    uint16_t length = e1000_poll_receive(buffer);
    if (length == 0)
        return false;

    if (length < ETH_HEADER_LEN)
        return false; // truncated/malformed -- not actionable, drop

    const eth_header_t *hdr = (const eth_header_t *)buffer;
    for (int i = 0; i < ETH_MAC_LEN; i++)
    {
        out_frame->dest_mac[i] = hdr->dest_mac[i];
        out_frame->src_mac[i] = hdr->src_mac[i];
    }
    out_frame->ethertype = eth_ntohs(hdr->ethertype);
    out_frame->payload = (const uint8_t *)buffer + ETH_HEADER_LEN;
    out_frame->payload_len = (uint16_t)(length - ETH_HEADER_LEN);

    return true;
}
