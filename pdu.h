#ifndef PDU_H
#define PDU_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;
    size_t len;
} ByteView;

typedef struct {
    uint8_t src_mac[6];
    uint8_t dst_mac[6];
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
    uint16_t src_port;
    uint16_t dst_port;
} PacketConfig;

typedef struct {
    ByteView udp;
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
} Ipv4View;

/* PDU routines return 0 on success and a negative value on error. */
int udp_encode(ByteView payload, const PacketConfig *config,
               uint8_t *out, size_t capacity, size_t *written);
int ipv4_encode(ByteView udp, const PacketConfig *config,
                uint8_t *out, size_t capacity, size_t *written);
int ethernet_encode(ByteView ip, const PacketConfig *config,
                    uint8_t *out, size_t capacity, size_t *written);

/* Decoded byte ranges refer to the original buffer. */
int ethernet_decode(ByteView frame, ByteView *ip_with_padding);
int ipv4_decode(ByteView ip_with_padding, Ipv4View *ip);
int udp_decode(const Ipv4View *ip, ByteView *payload);

uint16_t wire_read_be16(const uint8_t bytes[2]);
void wire_write_be16(uint8_t bytes[2], uint16_t value);
uint32_t wire_read_le32(const uint8_t bytes[4]);

/* Internet checksums return 0 when computed over valid checksummed bytes.
 * UDP includes the IPv4 pseudo-header. The UDP sender encodes a computed
 * zero as 0xffff. An odd final byte is padded with zero for the sum.
 */
uint16_t wire_checksum(ByteView bytes);
uint16_t wire_udp_checksum(ByteView udp, const uint8_t src_ip[4],
                           const uint8_t dst_ip[4]);

/* Standard Ethernet CRC-32, as a number before FCS serialization. */
uint32_t wire_crc32(ByteView bytes);

#endif
