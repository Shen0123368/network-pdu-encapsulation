#include "pdu.h"

#include <inttypes.h>
#include <stdio.h>

enum {
    ETHERNET_HEADER_LEN = 14,
    ETHERNET_FCS_LEN = 4,
    ETHERNET_MIN_FRAME_LEN = 64,
    ETHERNET_MAX_FRAME_LEN = 1518,
    ETHER_TYPE_IPV4 = 0x0800
};

static void print_mac(const char *label, const uint8_t mac[6])
{
    printf("  %s  %02X:%02X:%02X:%02X:%02X:%02X\n", label,
           (unsigned)mac[0], (unsigned)mac[1], (unsigned)mac[2],
           (unsigned)mac[3], (unsigned)mac[4], (unsigned)mac[5]);
}

int ethernet_decode(ByteView frame, ByteView *ip_with_padding)
{
    if (frame.data == NULL || ip_with_padding == NULL) {
        fputs("Ethernet II 解封装失败：参数为空。\n", stderr);
        return -1;
    }

    if (frame.len < ETHERNET_MIN_FRAME_LEN || frame.len > ETHERNET_MAX_FRAME_LEN) {
        fprintf(stderr, "Ethernet II 解封装失败：帧长 %zu 字节，预期 64 到 1518 字节。\n",
                frame.len);
        return -1;
    }

    size_t fcs_offset = frame.len - ETHERNET_FCS_LEN;
    uint32_t fcs = wire_read_le32(frame.data + fcs_offset);
    uint32_t expected = wire_crc32((ByteView){frame.data, fcs_offset});
    if (fcs != expected) {
        fprintf(stderr, "Ethernet II 解封装失败：FCS 为 0x%08" PRIX32
                "，计算值为 0x%08" PRIX32 "。\n", fcs, expected);
        return -1;
    }

    uint16_t ether_type = wire_read_be16(frame.data + 12);
    if (ether_type != ETHER_TYPE_IPV4) {
        fprintf(stderr, "Ethernet II 解封装失败：EtherType 0x%04X，预期 0x0800（IPv4）。\n",
                (unsigned)ether_type);
        return -1;
    }

    *ip_with_padding = (ByteView){
        frame.data + ETHERNET_HEADER_LEN,
        fcs_offset - ETHERNET_HEADER_LEN
    };

    printf("Ethernet II（帧长 %zu 字节）\n", frame.len);
    print_mac("目的 MAC", frame.data);
    print_mac("源 MAC", frame.data + 6);
    printf("  EtherType  0x%04X（IPv4）\n", (unsigned)ether_type);
    printf("  数据区     %zu 字节（可能包含填充）\n", ip_with_padding->len);
    printf("  FCS        0x%08" PRIX32 "（CRC-32 校验通过）\n", fcs);
    return 0;
}
