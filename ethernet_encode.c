#include "pdu.h"

#include <stdio.h>
#include <string.h>

enum {
    ETHERNET_HEADER_LEN = 14,
    ETHERNET_FCS_LEN = 4,
    ETHERNET_MIN_FRAME_LEN = 64,
    ETHERNET_MAX_FRAME_LEN = 1518,
    /* 去掉首部和 FCS 之后，数据区至少要补到 46 字节才能凑满 64 字节最小帧。 */
    ETHERNET_MIN_PAYLOAD_LEN = ETHERNET_MIN_FRAME_LEN - ETHERNET_HEADER_LEN - ETHERNET_FCS_LEN,
    ETHER_TYPE_IPV4 = 0x0800
};

int ethernet_encode(ByteView ip, const PacketConfig *config,
                    uint8_t *out, size_t capacity, size_t *written)
{
    if (ip.data == NULL || config == NULL || out == NULL || written == NULL) {
        fputs("Ethernet II 封装失败：参数为空。\n", stderr);
        return -1;
    }

    size_t payload_len = ip.len < ETHERNET_MIN_PAYLOAD_LEN ? ETHERNET_MIN_PAYLOAD_LEN : ip.len;
    size_t frame_len = ETHERNET_HEADER_LEN + payload_len + ETHERNET_FCS_LEN;

    if (frame_len > ETHERNET_MAX_FRAME_LEN) {
        fprintf(stderr, "Ethernet II 封装失败：帧长 %zu 字节，超过 1518 字节上限。\n",
                frame_len);
        return -1;
    }

    if (capacity < frame_len) {
        fputs("Ethernet II 封装失败：输出缓冲区容量不足。\n", stderr);
        return -1;
    }

    memmove(out + ETHERNET_HEADER_LEN, ip.data, ip.len);
    memcpy(out, config->dst_mac, sizeof config->dst_mac);      /* 目的 MAC。 */
    memcpy(out + 6, config->src_mac, sizeof config->src_mac);  /* 源 MAC。 */
    wire_write_be16(out + 12, ETHER_TYPE_IPV4);

    if (payload_len > ip.len) {
        /* 数据区不足 46 字节时补零，补齐的字节同样参与 FCS 计算。 */
        memset(out + ETHERNET_HEADER_LEN + ip.len, 0, payload_len - ip.len);
    }

    size_t fcs_offset = ETHERNET_HEADER_LEN + payload_len;
    uint32_t fcs = wire_crc32((ByteView){out, fcs_offset});
    /* FCS 从目的 MAC 起算，低字节在前写入帧尾。 */
    out[fcs_offset] = (uint8_t)(fcs & 0xFFu);
    out[fcs_offset + 1] = (uint8_t)((fcs >> 8) & 0xFFu);
    out[fcs_offset + 2] = (uint8_t)((fcs >> 16) & 0xFFu);
    out[fcs_offset + 3] = (uint8_t)((fcs >> 24) & 0xFFu);

    *written = frame_len;
    return 0;
}
