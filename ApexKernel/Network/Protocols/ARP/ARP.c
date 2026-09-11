// ApexKernel/Network/Protocols/ARP/ARP.c
// ApexOS -- Address Resolution Protocol (see ARP.h)

#include "Network/Protocols/ARP/ARP.h"
#include "Core/Time/PITCounter.h"
#include "Drivers/Network/Vendors/Intel/E1000.h"

// Fixed-size cache, linear scan -- this project's traffic volumes
// (ping to a handful of hosts, see the roadmap) never come close to
// needing anything fancier than a flat array. Entries never expire
// (no RFC 826 aging timer) -- V1 scope, fine for a single ping-test
// session; a long-running network stack would want that eventually.
#define ARP_CACHE_SIZE 16

typedef struct
{
    bool valid;
    uint8_t ip[ARP_IPV4_LEN];
    uint8_t mac[ETH_MAC_LEN];
} arp_cache_entry_t;

static arp_cache_entry_t g_cache[ARP_CACHE_SIZE];
static uint8_t g_own_ip[ARP_IPV4_LEN];

// Scratch buffer eth_poll_receive() writes each received frame into
// while arp_resolve() spins waiting for its reply -- same
// static-buffer reasoning as Ethernet.c's g_tx_frame (see that file):
// this project's polling loop is not re-entrant, so one shared buffer
// is enough and avoids a page-sized stack allocation per resolve call.
static uint8_t g_rx_buffer[2048];

static bool ip_equal(const uint8_t a[ARP_IPV4_LEN], const uint8_t b[ARP_IPV4_LEN])
{
    for (int i = 0; i < ARP_IPV4_LEN; i++)
        if (a[i] != b[i])
            return false;
    return true;
}

static void cache_insert(const uint8_t ip[ARP_IPV4_LEN], const uint8_t mac[ETH_MAC_LEN])
{
    // Update in place if this IP is already cached (a fresher MAC for
    // the same IP, e.g. after the peer's NIC changed) -- checked
    // first so re-resolving the same host doesn't fill the cache with
    // duplicate entries.
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (g_cache[i].valid && ip_equal(g_cache[i].ip, ip))
        {
            for (int j = 0; j < ETH_MAC_LEN; j++)
                g_cache[i].mac[j] = mac[j];
            return;
        }
    }

    // Otherwise take the first free slot. If the cache is full, the
    // entry is simply not cached (next resolve for it just does
    // another round trip) -- acceptable at this project's current
    // scale (see this file's header comment), rather than adding
    // eviction logic for a problem that doesn't exist yet.
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (!g_cache[i].valid)
        {
            g_cache[i].valid = true;
            for (int j = 0; j < ARP_IPV4_LEN; j++)
                g_cache[i].ip[j] = ip[j];
            for (int j = 0; j < ETH_MAC_LEN; j++)
                g_cache[i].mac[j] = mac[j];
            return;
        }
    }
}

static bool cache_lookup(const uint8_t ip[ARP_IPV4_LEN], uint8_t out_mac[ETH_MAC_LEN])
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
    {
        if (g_cache[i].valid && ip_equal(g_cache[i].ip, ip))
        {
            for (int j = 0; j < ETH_MAC_LEN; j++)
                out_mac[j] = g_cache[i].mac[j];
            return true;
        }
    }
    return false;
}

static void send_arp(uint16_t oper, const uint8_t target_mac[ETH_MAC_LEN],
                     const uint8_t target_ip[ARP_IPV4_LEN])
{
    arp_packet_t pkt;
    pkt.htype = eth_htons(ARP_HTYPE_ETHERNET);
    pkt.ptype = eth_htons(ARP_PTYPE_IPV4);
    pkt.hlen = ETH_MAC_LEN;
    pkt.plen = ARP_IPV4_LEN;
    pkt.oper = eth_htons(oper);

    uint8_t own_mac[ETH_MAC_LEN];
    eth_get_mac(own_mac);
    for (int i = 0; i < ETH_MAC_LEN; i++)
        pkt.sender_mac[i] = own_mac[i];
    for (int i = 0; i < ARP_IPV4_LEN; i++)
        pkt.sender_ip[i] = g_own_ip[i];

    for (int i = 0; i < ETH_MAC_LEN; i++)
        pkt.target_mac[i] = target_mac[i];
    for (int i = 0; i < ARP_IPV4_LEN; i++)
        pkt.target_ip[i] = target_ip[i];

    // REQUEST is sent broadcast (target MAC not known yet -- that's
    // the whole point of asking), REPLY goes straight back to the
    // requester's own MAC, already known from the frame we're
    // replying to.
    const uint8_t *dest = (oper == ARP_OP_REQUEST) ? ETH_BROADCAST_MAC : target_mac;
    eth_send(dest, ETH_TYPE_ARP, &pkt, sizeof(pkt));
}

void arp_init(const uint8_t own_ip[ARP_IPV4_LEN])
{
    for (int i = 0; i < ARP_IPV4_LEN; i++)
        g_own_ip[i] = own_ip[i];

    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        g_cache[i].valid = false;
}

bool arp_resolve(const uint8_t target_ip[ARP_IPV4_LEN], uint8_t out_mac[ETH_MAC_LEN],
                 uint32_t timeout_ms)
{
    if (cache_lookup(target_ip, out_mac))
        return true;

    send_arp(ARP_OP_REQUEST, ETH_BROADCAST_MAC, target_ip);

    // Same pit_get_ticks()-based timeout pattern as AHCI's
    // ahci_wait_clear() (see Drivers/Storage/AHCI/PortControl.c):
    // PIT ticks at 100Hz (10ms/tick, see Core/Time/PITCounter.h), so
    // timeout_ms converts to ticks by dividing by 10, +1 to avoid a
    // zero-tick timeout rounding a small timeout_ms down to "expires
    // immediately".
    uint64_t start = pit_get_ticks();
    uint64_t timeout_ticks = (timeout_ms / 10) + 1;

    while (pit_get_ticks() - start <= timeout_ticks)
    {
        eth_frame_t frame;
        if (eth_poll_receive(g_rx_buffer, &frame) && frame.ethertype == ETH_TYPE_ARP)
        {
            arp_handle_packet(&frame);

            // The packet just handled may be the reply we're waiting
            // for -- check the cache again immediately rather than
            // waiting for the next loop iteration.
            if (cache_lookup(target_ip, out_mac))
                return true;
        }
    }

    return false; // timed out -- target not present, or not answering
}

void arp_handle_packet(const eth_frame_t *frame)
{
    if (frame->payload_len < sizeof(arp_packet_t))
        return; // truncated/malformed -- not actionable, drop

    const arp_packet_t *pkt = (const arp_packet_t *)frame->payload;

    // Only Ethernet+IPv4 ARP is handled (see ARP.h's header comment
    // on V1 scope) -- anything else (e.g. a hardware type this
    // project never uses) is silently ignored rather than
    // misinterpreted.
    if (eth_ntohs(pkt->htype) != ARP_HTYPE_ETHERNET || eth_ntohs(pkt->ptype) != ARP_PTYPE_IPV4)
        return;

    // Cache the sender's mapping regardless of opcode -- RFC 826's
    // suggested behavior (see ARP.h's header comment), saves a future
    // arp_resolve() call for this host a round trip.
    cache_insert(pkt->sender_ip, pkt->sender_mac);

    uint16_t oper = eth_ntohs(pkt->oper);
    if (oper == ARP_OP_REQUEST && ip_equal(pkt->target_ip, g_own_ip))
        send_arp(ARP_OP_REPLY, pkt->sender_mac, pkt->sender_ip);
}
