#include "pdu.h"

#include <stdio.h>
#include <string.h>

enum {
    IPV4_MIN_HEADER_LEN = 20,
    IPV4_UDP_PROTOCOL = 17,
    IPV4_FLAG_RESERVED = 0x8000,
    IPV4_FLAG_DONT_FRAGMENT = 0x4000,
    IPV4_FLAG_MORE_FRAGMENTS = 0x2000,
    IPV4_FRAGMENT_OFFSET_MASK = 0x1FFF
};

static void print_address(const char *label, const uint8_t address[4])
{
    printf("  %s  %u.%u.%u.%u\n", label, (unsigned)address[0], (unsigned)address[1],
           (unsigned)address[2], (unsigned)address[3]);
}

int ipv4_decode(ByteView ip_with_padding, Ipv4View *ip)
{
    if (ip_with_padding.data == NULL || ip == NULL) {
        fputs("IPv4 解封装失败：参数为空。\n", stderr);
        return -1;
    }

    if (ip_with_padding.len < IPV4_MIN_HEADER_LEN) {
        fprintf(stderr, "IPv4 解封装失败：数据只有 %zu 字节，不足 20 字节基本首部。\n",
                ip_with_padding.len);
        return -1;
    }

    const uint8_t *bytes = ip_with_padding.data;
    unsigned version = bytes[0] >> 4;
    size_t header_len = (size_t)(bytes[0] & 0x0Fu) * 4;
    uint16_t total_len = wire_read_be16(bytes + 2);

    if (version != 4) {
        fprintf(stderr, "IPv4 解封装失败：版本号为 %u，预期 4。\n", version);
        return -1;
    }

    if (header_len < IPV4_MIN_HEADER_LEN) {
        fprintf(stderr, "IPv4 解封装失败：IHL 为 %zu 字节，小于 20 字节基本首部。\n",
                header_len);
        return -1;
    }

    if (header_len > ip_with_padding.len) {
        fprintf(stderr, "IPv4 解封装失败：首部长度 %zu 字节，但可用数据只有 %zu 字节。\n",
                header_len, ip_with_padding.len);
        return -1;
    }

    if (total_len < header_len || total_len > ip_with_padding.len) {
        fprintf(stderr, "IPv4 解封装失败：总长度为 %u 字节，与首部 %zu 字节、可用 %zu 字节不符。\n",
                (unsigned)total_len, header_len, ip_with_padding.len);
        return -1;
    }

    if (wire_checksum((ByteView){bytes, header_len}) != 0) {
        fprintf(stderr, "IPv4 解封装失败：首部校验和 0x%04X 验证失败。\n",
                (unsigned)wire_read_be16(bytes + 10));
        return -1;
    }

    uint16_t flags_and_offset = wire_read_be16(bytes + 6);
    if ((flags_and_offset & IPV4_FLAG_RESERVED) != 0) {
        fputs("IPv4 解封装失败：标志位中的保留位应为 0。\n", stderr);
        return -1;
    }
    if ((flags_and_offset & IPV4_FLAG_MORE_FRAGMENTS) != 0 ||
        (flags_and_offset & IPV4_FRAGMENT_OFFSET_MASK) != 0) {
        fputs("IPv4 解封装失败：本演示只处理未分片的数据报。\n", stderr);
        return -1;
    }

    if (bytes[9] != IPV4_UDP_PROTOCOL) {
        fprintf(stderr, "IPv4 解封装失败：协议号为 %u，预期 17（UDP）。\n",
                (unsigned)bytes[9]);
        return -1;
    }

    /* 用总长度和 IHL 划出 UDP 范围，以太网填充被排除在外。 */
    ip->udp = (ByteView){bytes + header_len, (size_t)total_len - header_len};
    memcpy(ip->src_ip, bytes + 12, sizeof ip->src_ip);
    memcpy(ip->dst_ip, bytes + 16, sizeof ip->dst_ip);

    printf("IPv4（首部 %zu 字节，总长度 %u 字节）\n", header_len, (unsigned)total_len);
    printf("  版本       %u\n", version);
    printf("  首部长度   %zu 字节（IHL=%u，可选字段 %zu 字节）\n",
           header_len, (unsigned)(header_len / 4), header_len - IPV4_MIN_HEADER_LEN);
    printf("  DSCP       %u（区分服务代码点）\n", (unsigned)bytes[1] >> 2);
    printf("  ECN        %u（显式拥塞通知）\n", (unsigned)bytes[1] & 0x03u);
    printf("  总长度     %u 字节（数据区 %zu 字节）\n",
           (unsigned)total_len, ip->udp.len);
    printf("  标识       0x%04X\n", (unsigned)wire_read_be16(bytes + 4));
    printf("  标志位     保留位=0，DF（禁止分片）=%u，MF（后续分片）=%u\n",
           (unsigned)((flags_and_offset & IPV4_FLAG_DONT_FRAGMENT) != 0),
           (unsigned)((flags_and_offset & IPV4_FLAG_MORE_FRAGMENTS) != 0));
    printf("  片偏移     %u（每单位 8 字节）\n",
           (unsigned)(flags_and_offset & IPV4_FRAGMENT_OFFSET_MASK));
    printf("  TTL        %u（剩余生存时间）\n", (unsigned)bytes[8]);
    printf("  协议号     %u（UDP）\n", (unsigned)bytes[9]);
    printf("  首部校验和 0x%04X（校验通过）\n",
           (unsigned)wire_read_be16(bytes + 10));
    print_address("源 IP    ", ip->src_ip);
    print_address("目的 IP  ", ip->dst_ip);
    printf("  以太网填充 %zu 字节（IPv4 总长度之外）\n",
           ip_with_padding.len - total_len);
    printf("  交给 UDP   %zu 字节\n", ip->udp.len);
    return 0;
}
