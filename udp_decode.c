#include "pdu.h"

#include <stdio.h>

enum { UDP_HEADER_LEN = 8 };

int udp_decode(const Ipv4View *ip, ByteView *payload)
{
    if (ip == NULL || payload == NULL || ip->udp.data == NULL) {
        fputs("UDP 解封装失败：参数为空。\n", stderr);
        return -1;
    }

    ByteView udp = ip->udp;
    if (udp.len < UDP_HEADER_LEN) {
        fputs("UDP 解封装失败：数据不足 8 字节，无法读取完整首部。\n", stderr);
        return -1;
    }

    uint16_t length = wire_read_be16(udp.data + 4);
    if (length < UDP_HEADER_LEN || length != udp.len) {
        fprintf(stderr, "UDP 解封装失败：长度字段为 %u，IPv4 数据区有 %zu 字节。\n",
                (unsigned)length, udp.len);
        return -1;
    }

    uint16_t checksum = wire_read_be16(udp.data + 6);
    if (checksum != 0 && wire_udp_checksum(udp, ip->src_ip, ip->dst_ip) != 0) {
        fprintf(stderr, "UDP 解封装失败：校验和 0x%04X 验证失败。\n", (unsigned)checksum);
        return -1;
    }

    *payload = (ByteView){udp.data + UDP_HEADER_LEN, length - UDP_HEADER_LEN};

    puts("UDP（首部固定为 8 字节）");
    printf("  源端口     %u\n", (unsigned)wire_read_be16(udp.data));
    printf("  目的端口   %u\n", (unsigned)wire_read_be16(udp.data + 2));
    printf("  长度       %u 字节（应用数据 %zu 字节）\n",
           (unsigned)length, payload->len);
    printf("  校验和     0x%04X（%s）\n", (unsigned)checksum,
           checksum == 0 ? "未携带校验和，IPv4 允许" : "校验通过");
    return 0;
}
