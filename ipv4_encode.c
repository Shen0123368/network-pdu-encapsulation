#include "pdu.h"

#include <stdio.h>
#include <string.h>

enum {
    IPV4_HEADER_LEN = 20,
    IPV4_UDP_PROTOCOL = 17,
    IPV4_DONT_FRAGMENT = 0x4000,
    UDP_HEADER_LEN = 8
};

int ipv4_encode(ByteView udp, const PacketConfig *config,
                uint8_t *out, size_t capacity, size_t *written)
{
    if (udp.data == NULL || config == NULL || out == NULL || written == NULL) {
        fputs("IPv4 封装失败：参数为空。\n", stderr);
        return -1;
    }

    if (udp.len < UDP_HEADER_LEN || udp.len > UINT16_MAX - IPV4_HEADER_LEN ||
        wire_read_be16(udp.data + 4) != udp.len) {
        fputs("IPv4 封装失败：UDP 长度无效。\n", stderr);
        return -1;
    }

    size_t total_len = IPV4_HEADER_LEN + udp.len;
    if (capacity < total_len) {
        fputs("IPv4 封装失败：输出缓冲区容量不足。\n", stderr);
        return -1;
    }

    memmove(out + IPV4_HEADER_LEN, udp.data, udp.len);
    memset(out, 0, IPV4_HEADER_LEN);
    out[0] = 0x45;                        /* IPv4, five 32-bit header words. */
    wire_write_be16(out + 2, (uint16_t)total_len);
    wire_write_be16(out + 6, IPV4_DONT_FRAGMENT);
    out[8] = 64;                          /* TTL. */
    out[9] = IPV4_UDP_PROTOCOL;
    memcpy(out + 12, config->src_ip, sizeof config->src_ip);
    memcpy(out + 16, config->dst_ip, sizeof config->dst_ip);

    uint16_t checksum = wire_checksum((ByteView){out, IPV4_HEADER_LEN});
    wire_write_be16(out + 10, checksum);

    *written = total_len;
    return 0;
}
