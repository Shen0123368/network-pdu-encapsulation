#include "pdu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    UDP_HEADER_LEN = 8,
    IPV4_HEADER_LEN = 20,
    ETHERNET_MTU = 1500,
    ETHERNET_MAX_FRAME_LEN = 1518,
    MAX_MESSAGE_LEN = ETHERNET_MTU - IPV4_HEADER_LEN - UDP_HEADER_LEN
};

static void print_hex(ByteView bytes)
{
    for (size_t i = 0; i < bytes.len; ++i) {
        printf("%02X", (unsigned)bytes.data[i]);
        putchar((i + 1) % 16 == 0 || i + 1 == bytes.len ? '\n' : ' ');
    }
}

int main(int argc, char *argv[])
{
    if (argc > 2) {
        fprintf(stderr, "用法：%s [消息]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *message = argc == 2 ? argv[1] : "Hello, UDP!";
    ByteView input = {(const uint8_t *)message, strlen(message)};
    if (input.len > MAX_MESSAGE_LEN) {
        fprintf(stderr, "消息长 %zu 字节，最多允许 %d 字节。\n", input.len, MAX_MESSAGE_LEN);
        return EXIT_FAILURE;
    }

    const PacketConfig config = {
        .src_mac = {0x02, 0, 0, 0, 0, 1},
        .dst_mac = {0x02, 0, 0, 0, 0, 2},
        .src_ip = {192, 0, 2, 1},
        .dst_ip = {198, 51, 100, 2},
        .src_port = 12345,
        .dst_port = 9000
    };

    uint8_t udp_bytes[ETHERNET_MTU - IPV4_HEADER_LEN];
    uint8_t ip_bytes[ETHERNET_MTU];
    uint8_t frame_bytes[ETHERNET_MAX_FRAME_LEN];
    size_t udp_len, ip_len, frame_len;

    puts("=== 封装 ===");
    printf("原始消息（%zu 字节）  %s\n", input.len, message);
    if (udp_encode(input, &config, udp_bytes, sizeof udp_bytes, &udp_len) != 0 ||
        ipv4_encode((ByteView){udp_bytes, udp_len}, &config,
                    ip_bytes, sizeof ip_bytes, &ip_len) != 0 ||
        ethernet_encode((ByteView){ip_bytes, ip_len}, &config,
                        frame_bytes, sizeof frame_bytes, &frame_len) != 0) {
        return EXIT_FAILURE;
    }

    printf("UDP 数据报 %zu 字节，IPv4 数据报 %zu 字节，以太网帧 %zu 字节。\n",
           udp_len, ip_len, frame_len);
    puts("以太网帧十六进制");
    ByteView frame = {frame_bytes, frame_len};
    print_hex(frame);

    ByteView ip_with_padding, decoded_message;
    Ipv4View ip;

    puts("\n=== 解封装 ===");
    if (ethernet_decode(frame, &ip_with_padding) != 0 ||
        ipv4_decode(ip_with_padding, &ip) != 0 ||
        udp_decode(&ip, &decoded_message) != 0) {
        return EXIT_FAILURE;
    }

    printf("还原消息（%zu 字节）  ", decoded_message.len);
    fwrite(decoded_message.data, 1, decoded_message.len, stdout);
    putchar('\n');

    if (decoded_message.len != input.len ||
        memcmp(decoded_message.data, input.data, input.len) != 0) {
        fputs("原文比对失败。\n", stderr);
        return EXIT_FAILURE;
    }

    puts("原文比对通过。");
    return EXIT_SUCCESS;
}
