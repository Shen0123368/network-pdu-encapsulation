#include "pdu.h"

/* 多字节字段的读写和三层校验计算都由本文件负责，其他模块只通过 pdu.h 调用。
 * 端口、长度、IPv4 与 UDP 的首部字段使用网络字节序（大端），
 * 以太网 FCS 按低字节在前写入帧尾。
 */

/* 把一段字节按大端 16 位字累加到校验和中。末尾若剩单个字节，补一个零字节参与求和，
 * 这个零字节只用于计算，不会写回缓冲区。 */
static uint32_t add_words(uint32_t sum, const uint8_t *bytes, size_t len)
{
    size_t i = 0;

    for (; i + 1 < len; i += 2) {
        sum += ((uint32_t)bytes[i] << 8) | (uint32_t)bytes[i + 1];
    }
    if (i < len) {
        sum += (uint32_t)bytes[i] << 8;
    }
    return sum;
}

/* 把高位的进位折回低 16 位，得到单字反码和。 */
static uint16_t fold(uint32_t sum)
{
    while (sum > 0xFFFFu) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)sum;
}

uint16_t wire_read_be16(const uint8_t bytes[2])
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}

void wire_write_be16(uint8_t bytes[2], uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)(value & 0xFFu);
}

uint32_t wire_read_le32(const uint8_t bytes[4])
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

/* 对整段字节（含已经写入的校验和字段）求 Internet 校验和。
 * 校验和正确时返回值折叠后为 0，因此解封装可直接用 != 0 判断失败。 */
uint16_t wire_checksum(ByteView bytes)
{
    return (uint16_t)~fold(add_words(0, bytes.data, bytes.len));
}

/* UDP 校验和覆盖伪首部、UDP 首部和数据。
 * 伪首部为源 IP、目的 IP、一个零字节、协议号 17 和 UDP 长度，共 12 字节。
 * 调用时 UDP 长度字段必须已经写好，校验和字段按约定先置零。 */
uint16_t wire_udp_checksum(ByteView udp, const uint8_t src_ip[4],
                           const uint8_t dst_ip[4])
{
    uint8_t pseudo_tail[4];
    uint32_t sum = 0;

    sum = add_words(sum, src_ip, 4);
    sum = add_words(sum, dst_ip, 4);
    pseudo_tail[0] = 0;                                   /* 填充位。 */
    pseudo_tail[1] = 17;                                  /* 协议号 UDP。 */
    pseudo_tail[2] = (uint8_t)((udp.len >> 8) & 0xFFu);
    pseudo_tail[3] = (uint8_t)(udp.len & 0xFFu);
    sum = add_words(sum, pseudo_tail, sizeof pseudo_tail);
    sum = add_words(sum, udp.data, udp.len);

    return (uint16_t)~fold(sum);
}

/* 标准以太网 CRC-32：生成多项式 0x04C11DB7，反射实现用 0xEDB88320，
 * 初值全 1，最后取反。返回的是写入 FCS 字段之前的数值。 */
uint32_t wire_crc32(ByteView bytes)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < bytes.len; ++i) {
        crc ^= bytes.data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) != 0 ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
        }
    }
    return ~crc;
}
