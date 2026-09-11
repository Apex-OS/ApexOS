// ApexKernel/Includes/Network/Protocols/TCP/TCP.h
// ApexOS -- TCP (RFC 793), client-only, single connection
//
// Sits on top of Network/Protocols/IPv4/IPv4.h, same as UDP. Unlike
// every other protocol in this stack, TCP is connection-oriented and
// stateful -- tcp_connect() must complete (the three-way handshake)
// before tcp_send()/tcp_poll_receive() do anything meaningful, and
// tcp_close() should be called when done. There is no "just send a
// packet" primitive here the way icmp_send_echo_request() or
// udp_send() are one-shot calls.
//
// V1 scope, deliberately narrow -- this is the last and most complex
// step in the project roadmap's networking arc, built incrementally
// rather than all at once:
//   1. Handshake (tcp_connect() brings a connection to ESTABLISHED,
//      tcp_close() tears it down).
//   2. Data transfer (tcp_send()/tcp_poll_receive() move bytes over
//      an ESTABLISHED connection, cumulative ACKs).
//   3. Retransmission: tcp_send() is stop-and-wait with a fixed
//      timeout and a bounded retry count (see tcp_send()'s own
//      comment) -- not a full sliding-window send buffer with
//      adaptive RTT-based RTO the way a production TCP stack has,
//      but a dropped segment is no longer silently lost.
//
// Still not modeled: incoming data loss recovery beyond what falls
// out naturally (an out-of-order/dropped incoming segment is
// silently ignored, relying on the peer's own retransmission timer
// to resend it -- see tcp_poll_receive()'s comment on this), no
// congestion control, no window scaling, no selective ACK.
//
// Client-only: this stack never listens for/accepts incoming
// connections (no TCP server role) -- tcp_connect() always initiates.
// Single connection: there is one implicit TCP control block, not a
// table of connections -- calling tcp_connect() again before
// tcp_close() on the current one abandons it. Good enough for this
// project's current use (testing TCP against one real server at a
// time, the same role cmd_ping()/cmd_dnstest() played for
// ICMP/UDP) -- a real multi-connection stack is future work once
// something actually needs concurrent TCP connections.

#ifndef APEX_NETWORK_PROTOCOLS_TCP_H
#define APEX_NETWORK_PROTOCOLS_TCP_H

#include "Core/Types.h"
#include "Network/Protocols/IPv4/IPv4.h"

#define TCP_HEADER_LEN 20 // no options, data offset always 5 (see tcp_header_t below)

// TCP flag bits (RFC 793 section 3.1) -- the ones this V1
// implementation actually sets/checks. URG and ECN-related bits are
// never used by this project.
#define TCP_FLAG_FIN (1u << 0)
#define TCP_FLAG_SYN (1u << 1)
#define TCP_FLAG_RST (1u << 2)
#define TCP_FLAG_PSH (1u << 3)
#define TCP_FLAG_ACK (1u << 4)

// Connection states this implementation actually models -- a small
// subset of RFC 793's full state machine (figure 6), enough for a
// client-only, single-connection stack. No LISTEN (never a server),
// no SYN_RECEIVED (never accepts), no CLOSE_WAIT/LAST_ACK (the peer
// closing first isn't handled specially yet -- see tcp_poll_receive()
// for current behavior).
typedef enum
{
    TCP_STATE_CLOSED,
    TCP_STATE_SYN_SENT,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT,
    TCP_STATE_CLOSED_BY_PEER // peer sent FIN -- see tcp_poll_receive()'s header comment
} tcp_state_t;

// Wire layout of a 20-byte TCP header (RFC 793 section 3.1), no
// options -- matches on-the-wire byte order exactly, every
// multi-byte field big-endian (see Ethernet.h's header comment on
// why eth_htons()/eth_ntohs() exist). Packed since this is read/
// written directly against IPv4 packet payloads.
typedef struct __attribute__((packed))
{
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset; // high 4 bits = header length in 32-bit words (always 5 here); low 4 bits reserved, always 0
    uint8_t flags;       // TCP_FLAG_* above (only the low 6 bits are used)
    uint16_t window_size;
    uint16_t checksum;   // same pseudo-header scheme as UDP -- see UDP.h's header comment
    uint16_t urgent_ptr; // never used by this project -- always 0 on send
} tcp_header_t;

// Must be called before tcp_connect() -- after ipv4_init() (needs
// ipv4_get_own_ip() to build the pseudo-header checksum, same as
// UDP). Resets the connection state to TCP_STATE_CLOSED.
void tcp_init(void);

// Returns the current connection state -- callers (e.g. a shell test
// command) use this to know whether tcp_connect() finished, whether
// the peer closed first, etc. rather than tcp_connect()/
// tcp_poll_receive() needing extra out-parameters for it.
tcp_state_t tcp_get_state(void);

// Initiates a TCP connection to dest_ip:dest_port from src_port on
// this machine -- performs the full three-way handshake (send SYN,
// wait for SYN+ACK, send ACK) and blocks until it completes or
// timeout_ms elapses, same blocking-with-timeout pattern as
// arp_resolve() (see ARP.h's header comment on that same tradeoff).
// Returns true and leaves the connection in TCP_STATE_ESTABLISHED on
// success; false on timeout (leaves it TCP_STATE_CLOSED) or if the
// peer responds with RST (port closed/refused -- also leaves it
// TCP_STATE_CLOSED).
bool tcp_connect(const uint8_t dest_ip[IPV4_ADDR_LEN], uint16_t dest_port, uint16_t src_port,
                 uint32_t timeout_ms);

// Sends `len` bytes of data on the current ESTABLISHED connection --
// returns false immediately (no-op) if the connection isn't
// ESTABLISHED. Blocks until the peer ACKs this data or retransmission
// gives up: resends the same segment (same sequence number, so the
// peer's own duplicate-detection handles a retransmit that arrives
// after the original did) on a fixed timeout if no ACK covering it
// arrives, up to TCP_MAX_RETRIES attempts total. This is a simplified
// stop-and-wait model -- one unacknowledged segment at a time, no
// sliding window of multiple in-flight segments -- appropriate for
// this project's current single-request use (see TCP.h's header
// comment on V1 scope) rather than a full RFC 793 send buffer with
// cumulative ACK tracking across many in-flight segments.
bool tcp_send(const void *data, uint16_t len);

// Polls the network (via ipv4_poll_receive()) for one TCP segment
// belonging to the current connection and, if one carries data,
// copies it into `out_buffer` (up to `out_buffer_size` bytes) and
// returns the number of bytes copied (0 if the segment was
// control-only, e.g. a bare ACK, or if nothing arrived this poll).
// Automatically ACKs received data and handles the peer initiating
// close (a FIN moves the connection to TCP_STATE_CLOSED_BY_PEER after
// ACKing it) -- tcp_get_state() reflects that so the caller's loop
// can stop reading. `scratch_buffer` is the underlying Ethernet
// receive buffer (same E1000_RX_BUFFER_SIZE sizing requirement as
// every other *_poll_receive() in this stack) and is distinct from
// out_buffer since a segment's on-wire size and the data actually
// delivered to the caller aren't the same buffer.
uint16_t tcp_poll_receive(void *scratch_buffer, uint8_t *out_buffer, uint16_t out_buffer_size);

// Initiates connection teardown -- sends FIN, waits for the peer's
// ACK and its own FIN in turn (best-effort: RFC 793's full
// four-way close with FIN_WAIT_1/2, TIME_WAIT etc. is not modeled,
// see this file's header comment on V1 scope), then sets the
// connection state to TCP_STATE_CLOSED regardless of whether the
// peer's side of the close was observed within timeout_ms. Safe to
// call even if the connection is already TCP_STATE_CLOSED_BY_PEER
// (skips straight to sending this side's own FIN/ACK) or
// TCP_STATE_CLOSED (no-op).
void tcp_close(uint32_t timeout_ms);

#endif // APEX_NETWORK_PROTOCOLS_TCP_H
