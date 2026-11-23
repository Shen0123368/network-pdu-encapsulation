#include "pdu.h"

#include <stdio.h>
#include <string.h>

enum { UDP_HEADER_LEN = 8 };

int udp_encode(ByteView payload, const PacketConfig *config,
               uint8_t *out, size_t capacity, size_t *written)
{
    if (payload.data == NULL || config == NULL || out == NULL || written == NULL) {
        fputs("UDP 封装失败：参数为空。\n", stderr);
        return -1;
    }

    if (payload.len > (size_t)(UINT16_MAX - UDP_HEADER_LEN)) {
        fputs("UDP 封装失败：应用数据超出一个数据报能表示的长度。\n", stderr);
        return -1;
    }

    size_t total_len = UDP_HEADER_LEN + payload.len;
    if (capacity < total_len) {
        fputs("UDP 封装失败：输出缓冲区容量不足。\n", stderr);
        return -1;
    }

    memmove(out + UDP_HEADER_LEN, payload.data, payload.len);
    wire_write_be16(out, config->src_port);
    wire_write_be16(out + 2, config->dst_port);
    wire_write_be16(out + 4, (uint16_t)total_len);   /* 长度包含 8 字节首部。 */
    wire_write_be16(out + 6, 0);                     /* 计算校验和时本字段先置零。 */

    uint16_t checksum = wire_udp_checksum((ByteView){out, total_len},
                                          config->src_ip, config->dst_ip);
    /* 发送端把算出的 0x0000 写成 0xFFFF，0x0000 留给“不携带校验和”的含义。 */
    wire_write_be16(out + 6, checksum == 0 ? (uint16_t)0xFFFFu : checksum);

    *written = total_len;
    return 0;
}
