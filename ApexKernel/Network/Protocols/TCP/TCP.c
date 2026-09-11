// ApexKernel/Network/Protocols/TCP/TCP.c
// ApexOS -- TCP (see TCP.h)

#include "Network/Protocols/TCP/TCP.h"
#include "Network/Protocols/Ethernet/Ethernet.h"
#include "Core/Time/PITCounter.h"

// The single implicit connection's control block -- see TCP.h's
// header comment on why this stack has no connection table. Holds
// everything needed to build the next outgoing segment and validate
// the next incoming one.
typedef struct
{
    tcp_state_t state;
    uint8_t peer_ip[IPV4_ADDR_LEN];
    uint16_t local_port;
    uint16_t peer_port;
    uint32_t send_next; // next sequence number this side will send (SND.NXT)
    uint32_t recv_next; // next sequence number expected from the peer (RCV.NXT), i.e. our ACK number
} tcp_connection_t;

static tcp_connection_t g_conn;

// Scratch buffers -- same static-buffer reasoning as every other
// layer in this stack (see Ethernet.c/IPv4.c/UDP.c's own comments).
static uint8_t g_tx_segment[TCP_HEADER_LEN + ETH_MTU];
static uint8_t g_checksum_scratch[12 + TCP_HEADER_LEN + ETH_MTU]; // 12 = pseudo-header length, same layout as UDP's

// Same pseudo-header scheme as UDP (RFC 793 section 3.1's checksum
// definition explicitly reuses RFC 768's) -- see UDP.c's
// udp_pseudo_header_t for the identical layout, duplicated here
// rather than shared since the two protocols' checksum call sites
// differ enough (TCP's "length" field covers the TCP segment, UDP's
// covers the UDP datagram) that sharing the struct would need a
// generic-sounding name neither protocol's RFC actually uses.
typedef struct __attribute__((packed))
{
    uint8_t src_ip[IPV4_ADDR_LEN];
    uint8_t dest_ip[IPV4_ADDR_LEN];
    uint8_t zero;
    uint8_t protocol;
    uint16_t tcp_length;
} tcp_pseudo_header_t;

#define TCP_PSEUDO_HEADER_LEN 12
#define IPV4_PROTO_TCP_LOCAL 6 // same value as IPV4_PROTO_TCP (IPv4.h) -- avoids depending on that name here

static uint16_t compute_tcp_checksum(const uint8_t src_ip[IPV4_ADDR_LEN],
                                     const uint8_t dest_ip[IPV4_ADDR_LEN],
                                     const void *tcp_segment, uint16_t segment_len)
{
    tcp_pseudo_header_t pseudo;
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
    {
        pseudo.src_ip[i] = src_ip[i];
        pseudo.dest_ip[i] = dest_ip[i];
    }
    pseudo.zero = 0;
    pseudo.protocol = IPV4_PROTO_TCP_LOCAL;
    pseudo.tcp_length = eth_htons(segment_len);

    uint8_t *scratch = g_checksum_scratch;
    for (int i = 0; i < TCP_PSEUDO_HEADER_LEN; i++)
        scratch[i] = ((const uint8_t *)&pseudo)[i];

    const uint8_t *seg = (const uint8_t *)tcp_segment;
    for (uint16_t i = 0; i < segment_len; i++)
        scratch[TCP_PSEUDO_HEADER_LEN + i] = seg[i];

    return ipv4_checksum(scratch, (uint16_t)(TCP_PSEUDO_HEADER_LEN + segment_len));
}

// Builds and sends one TCP segment with the given flags/seq/ack and
// optional data -- the single choke point every handshake/data/close
// step in this file funnels through, so the header fields that never
// vary (ports, data offset, window size, urgent pointer) are only
// set in one place.
static bool send_segment(uint8_t flags, uint32_t seq, uint32_t ack,
                         const void *data, uint16_t data_len)
{
    tcp_header_t *hdr = (tcp_header_t *)g_tx_segment;
    hdr->src_port = eth_htons(g_conn.local_port);
    hdr->dest_port = eth_htons(g_conn.peer_port);
    hdr->seq_num = eth_htonl(seq);
    hdr->ack_num = eth_htonl(ack);
    hdr->data_offset = (5u << 4); // header length = 5 * 4 = 20 bytes, no options
    hdr->flags = flags;
    hdr->window_size = eth_htons(4096); // fixed, generous-enough window -- no flow control tuning at this stage
    hdr->checksum = 0;
    hdr->urgent_ptr = 0;

    uint8_t *dst = g_tx_segment + TCP_HEADER_LEN;
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < data_len; i++)
        dst[i] = src[i];

    uint16_t total_len = (uint16_t)(TCP_HEADER_LEN + data_len);

    uint8_t own_ip[IPV4_ADDR_LEN];
    ipv4_get_own_ip(own_ip);
    hdr->checksum = eth_htons(compute_tcp_checksum(own_ip, g_conn.peer_ip, g_tx_segment, total_len));

    return ipv4_send(g_conn.peer_ip, IPV4_PROTO_TCP_LOCAL, g_tx_segment, total_len);
}

// Polls once for a TCP segment belonging to the current connection
// (right IP + right ports) -- returns true and fills out_hdr/
// out_data/out_data_len if one arrived, false otherwise (nothing
// available, checksum invalid, or it's not part of this connection).
// The shared low-level poll every handshake-wait-loop and
// tcp_poll_receive() itself is built on.
static bool poll_segment(void *buffer, const tcp_header_t **out_hdr,
                         const uint8_t **out_data, uint16_t *out_data_len)
{
    ipv4_packet_t packet;
    if (!ipv4_poll_receive(buffer, &packet))
        return false;

    if (packet.protocol != IPV4_PROTO_TCP_LOCAL)
        return false;

    if (packet.payload_len < TCP_HEADER_LEN)
        return false; // truncated -- not actionable, drop

    if (compute_tcp_checksum(packet.src_ip, packet.dest_ip, packet.payload, packet.payload_len) != 0x0000)
        return false; // corrupted in transit -- see IPv4.h's header comment on this same 0x0000 property

    const tcp_header_t *hdr = (const tcp_header_t *)packet.payload;

    bool from_peer = true;
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
        if (packet.src_ip[i] != g_conn.peer_ip[i])
            from_peer = false;

    if (!from_peer || eth_ntohs(hdr->src_port) != g_conn.peer_port ||
        eth_ntohs(hdr->dest_port) != g_conn.local_port)
        return false; // not part of this connection -- ignore, this stack has no connection table to route it to anyway

    uint16_t header_len = (uint16_t)((hdr->data_offset >> 4) * 4);
    if (header_len < TCP_HEADER_LEN || header_len > packet.payload_len)
        return false; // malformed data offset -- not actionable, drop

    *out_hdr = hdr;
    *out_data = packet.payload + header_len;
    *out_data_len = (uint16_t)(packet.payload_len - header_len);

    return true;
}

void tcp_init(void)
{
    g_conn.state = TCP_STATE_CLOSED;
}

tcp_state_t tcp_get_state(void)
{
    return g_conn.state;
}

bool tcp_connect(const uint8_t dest_ip[IPV4_ADDR_LEN], uint16_t dest_port, uint16_t src_port,
                 uint32_t timeout_ms)
{
    for (int i = 0; i < IPV4_ADDR_LEN; i++)
        g_conn.peer_ip[i] = dest_ip[i];
    g_conn.peer_port = dest_port;
    g_conn.local_port = src_port;
    g_conn.send_next = 0x1000; // arbitrary fixed ISN -- no real randomization yet (see this file's header comment on V1 scope; a production stack would randomize this per RFC 6528 to resist sequence-prediction attacks, not a concern for this project's own test traffic)
    g_conn.recv_next = 0;

    g_conn.state = TCP_STATE_SYN_SENT;
    if (!send_segment(TCP_FLAG_SYN, g_conn.send_next, 0, NULL, 0))
    {
        g_conn.state = TCP_STATE_CLOSED;
        return false;
    }
    g_conn.send_next++; // SYN consumes one sequence number, per RFC 793

    static uint8_t rx_buffer[2048];
    uint64_t start = pit_get_ticks();
    uint64_t timeout_ticks = (timeout_ms / 10) + 1;

    while (pit_get_ticks() - start <= timeout_ticks)
    {
        const tcp_header_t *hdr;
        const uint8_t *data;
        uint16_t data_len;
        if (!poll_segment(rx_buffer, &hdr, &data, &data_len))
            continue;

        if (hdr->flags & TCP_FLAG_RST)
        {
            g_conn.state = TCP_STATE_CLOSED;
            return false; // connection refused
        }

        if ((hdr->flags & TCP_FLAG_SYN) && (hdr->flags & TCP_FLAG_ACK))
        {
            uint32_t peer_seq = eth_ntohl(hdr->seq_num);
            g_conn.recv_next = peer_seq + 1; // SYN consumes one sequence number on their side too

            if (!send_segment(TCP_FLAG_ACK, g_conn.send_next, g_conn.recv_next, NULL, 0))
            {
                g_conn.state = TCP_STATE_CLOSED;
                return false;
            }

            g_conn.state = TCP_STATE_ESTABLISHED;
            return true;
        }
    }

    g_conn.state = TCP_STATE_CLOSED;
    return false; // timed out -- peer never completed the handshake
}

#define TCP_RETRY_TIMEOUT_MS 1000 // fixed RTO -- no RTT-based adaptive timing yet, see TCP.h's header comment on scope
#define TCP_MAX_RETRIES 5

bool tcp_send(const void *data, uint16_t len)
{
    if (g_conn.state != TCP_STATE_ESTABLISHED)
        return false;

    if ((uint32_t)TCP_HEADER_LEN + len > ETH_MTU)
        return false; // no segmentation yet -- see TCP.h's header comment on V1 scope

    uint32_t this_seq = g_conn.send_next;
    uint32_t seq_after = this_seq + len; // what the peer's ack_num must reach or exceed to fully acknowledge this segment

    static uint8_t rx_buffer[2048];

    for (int attempt = 0; attempt < TCP_MAX_RETRIES; attempt++)
    {
        if (!send_segment(TCP_FLAG_ACK | TCP_FLAG_PSH, this_seq, g_conn.recv_next, data, len))
            return false;

        uint64_t start = pit_get_ticks();
        uint64_t timeout_ticks = (TCP_RETRY_TIMEOUT_MS / 10) + 1;

        while (pit_get_ticks() - start <= timeout_ticks)
        {
            const tcp_header_t *hdr;
            const uint8_t *seg_data;
            uint16_t seg_data_len;
            if (!poll_segment(rx_buffer, &hdr, &seg_data, &seg_data_len))
                continue;

            if (hdr->flags & TCP_FLAG_RST)
            {
                g_conn.state = TCP_STATE_CLOSED;
                return false;
            }

            // Any segment whose ACK number reaches past this one's
            // last byte confirms delivery -- doesn't need to be a
            // bare ACK specifically (RFC 793's ACK field is valid on
            // every segment once a connection is established, so a
            // data segment or FIN from the peer piggybacking the same
            // acknowledgment counts too).
            if (hdr->flags & TCP_FLAG_ACK)
            {
                uint32_t peer_ack = eth_ntohl(hdr->ack_num);
                // Sequence numbers wrap around (RFC 793 section 3.3) --
                // comparing with subtraction rather than a plain >=
                // handles that wraparound correctly, the same trick
                // RFC 793 itself specifies for sequence comparisons.
                if ((int32_t)(peer_ack - seq_after) >= 0)
                {
                    g_conn.send_next = seq_after;

                    // This segment may have arrived carrying its own
                    // data/FIN worth handling -- tcp_poll_receive()
                    // won't see this exact packet again (it's already
                    // been consumed off the NIC's RX ring by
                    // poll_segment() above), so anything it carried
                    // beyond the ACK would otherwise be silently
                    // lost. Rare in practice for a request/response
                    // pattern (the peer is still processing our
                    // request at this point), so not specially
                    // handled -- see TCP.h's header comment on this
                    // being a stop-and-wait model, not a general
                    // pipelined one.
                    return true;
                }
            }
        }
        // Timed out waiting for this attempt's ACK -- loop retries
        // with the same this_seq (a genuine retransmission, not a
        // new segment), per RFC 793's retransmission model.
    }

    return false; // exhausted all retries -- peer unreachable or persistently not acknowledging
}

uint16_t tcp_poll_receive(void *scratch_buffer, uint8_t *out_buffer, uint16_t out_buffer_size)
{
    if (g_conn.state != TCP_STATE_ESTABLISHED && g_conn.state != TCP_STATE_FIN_WAIT)
        return 0;

    const tcp_header_t *hdr;
    const uint8_t *data;
    uint16_t data_len;
    if (!poll_segment(scratch_buffer, &hdr, &data, &data_len))
        return 0;

    if (hdr->flags & TCP_FLAG_RST)
    {
        g_conn.state = TCP_STATE_CLOSED;
        return 0;
    }

    // Peer-initiated close: a FIN is acknowledged like any other byte
    // of sequence space (RFC 793 counts FIN as consuming one), and
    // moves this connection to TCP_STATE_CLOSED_BY_PEER so the
    // caller's read loop knows to stop expecting more data. This
    // implementation does not also send its own FIN back
    // automatically here -- tcp_close() does that explicitly, kept
    // as a separate deliberate step rather than an automatic
    // half-close reaction (see TCP.h's header comment on V1 scope:
    // this stack doesn't model CLOSE_WAIT/LAST_ACK as distinct
    // states, tcp_close() just does the practical equivalent).
    if (hdr->flags & TCP_FLAG_FIN)
    {
        uint32_t peer_seq = eth_ntohl(hdr->seq_num);
        g_conn.recv_next = peer_seq + data_len + 1;
        send_segment(TCP_FLAG_ACK, g_conn.send_next, g_conn.recv_next, NULL, 0);
        g_conn.state = TCP_STATE_CLOSED_BY_PEER;

        // A FIN can still carry data in the same segment (rare in
        // practice but valid per RFC 793) -- deliver it before
        // reporting the closed state, rather than dropping the last
        // bytes a well-behaved peer sent.
        if (data_len == 0)
            return 0;
    }
    else if (data_len > 0)
    {
        uint32_t peer_seq = eth_ntohl(hdr->seq_num);

        // Only in-order data is accepted -- this stack has no
        // reassembly buffer for out-of-order segments (see TCP.h's
        // header comment on V1 scope). A segment that doesn't start
        // exactly at recv_next is silently dropped; the peer's own
        // retransmission timer will eventually resend it, same as
        // real TCP relies on for loss recovery.
        if (peer_seq != g_conn.recv_next)
            return 0;

        g_conn.recv_next += data_len;
        send_segment(TCP_FLAG_ACK, g_conn.send_next, g_conn.recv_next, NULL, 0);
    }
    else
    {
        return 0; // bare ACK or other control segment -- nothing to deliver
    }

    uint16_t copy_len = (data_len < out_buffer_size) ? data_len : out_buffer_size;
    for (uint16_t i = 0; i < copy_len; i++)
        out_buffer[i] = data[i];

    return copy_len;
}

void tcp_close(uint32_t timeout_ms)
{
    if (g_conn.state == TCP_STATE_CLOSED)
        return;

    // If the peer already closed their side (TCP_STATE_CLOSED_BY_PEER),
    // only this side's own FIN/ACK needs sending -- otherwise this is
    // a locally-initiated close and the full FIN -> peer's ACK ->
    // peer's own FIN -> our ACK sequence is attempted, best-effort
    // (see TCP.h's header comment on this function's scope).
    bool peer_already_closed = (g_conn.state == TCP_STATE_CLOSED_BY_PEER);

    send_segment(TCP_FLAG_FIN | TCP_FLAG_ACK, g_conn.send_next, g_conn.recv_next, NULL, 0);
    g_conn.send_next++; // FIN consumes one sequence number, per RFC 793
    g_conn.state = TCP_STATE_FIN_WAIT;

    if (peer_already_closed)
    {
        g_conn.state = TCP_STATE_CLOSED;
        return;
    }

    static uint8_t rx_buffer[2048];
    uint64_t start = pit_get_ticks();
    uint64_t timeout_ticks = (timeout_ms / 10) + 1;

    while (pit_get_ticks() - start <= timeout_ticks)
    {
        const tcp_header_t *hdr;
        const uint8_t *data;
        uint16_t data_len;
        if (!poll_segment(rx_buffer, &hdr, &data, &data_len))
            continue;

        if (hdr->flags & TCP_FLAG_FIN)
        {
            uint32_t peer_seq = eth_ntohl(hdr->seq_num);
            g_conn.recv_next = peer_seq + data_len + 1;
            send_segment(TCP_FLAG_ACK, g_conn.send_next, g_conn.recv_next, NULL, 0);
            break;
        }
    }

    g_conn.state = TCP_STATE_CLOSED;
}
