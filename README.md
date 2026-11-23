# 各层次网络协议数据单元封装与解封装

计算机网络课程实验。用 C 语言在内存中逐个构造 UDP 用户数据报、IPv4 数据报和
Ethernet II 帧，再把同一段字节流按相反方向逐层解析，打印每层字段、校验结果和
还原出的应用消息。程序不依赖第三方库，也不调用操作系统协议栈。

```text
封装     应用消息 → UDP 数据报 → IPv4 数据报 → Ethernet II 帧
解封装   Ethernet II 帧 → IPv4 数据报 → UDP 数据报 → 应用消息
```

## 1. 模块划分

每层的封装与解封装各由一个源文件实现，公共声明集中在 `pdu.h`，字节序和校验计算
集中在 `wire.c`。模块之间只通过 `pdu.h` 里的函数和 `ByteView` 传递数据，不共享
全局状态。

| 文件 | 职责 | 负责人 |
| --- | --- | --- |
| `pdu.h` | 公共数据结构、六个函数声明、wire 辅助函数声明 | A |
| `wire.c` | 大端 16 位与小端 32 位读写、Internet 校验和、UDP 校验和、CRC-32 | A |
| `udp_encode.c` | 构造 UDP 首部，计算含伪首部的校验和 | A |
| `ipv4_decode.c` | 解析 IPv4 首部、验证校验和、划定 UDP 范围 | A |
| `ethernet_encode.c` | 构造 Ethernet II 首部，填充到最小帧长，写入 FCS | A |
| `ipv4_encode.c` | 构造 20 字节 IPv4 首部，写入总长度、TTL、协议号和校验和 | B |
| `ethernet_decode.c` | 校验帧长与 FCS，解析 MAC 和 EtherType，交出数据区 | B |
| `udp_decode.c` | 解析 UDP 首部，验证校验和，交出应用数据 | B |
| `main.c` | 演示入口：准备数据、串联三层封装与解封装、打印长度和完整帧 | B |

## 2. 调用链

```text
封装    main → udp_encode → ipv4_encode → ethernet_encode
解封装  main → ethernet_decode → ipv4_decode → udp_decode → 原消息
```

封装自上而下，每层把上一层的结果当作自己的载荷；解封装自下而上，每层只把自己
的载荷交给上一层。任一环节返回负值，`main.c` 立即停止当前流程并以非零状态退出。

## 3. 公共接口

```c
typedef struct { const uint8_t *data; size_t len; } ByteView;  /* 一段字节及其长度 */

typedef struct {                                                /* 演示用的地址与端口 */
    uint8_t src_mac[6], dst_mac[6];
    uint8_t src_ip[4], dst_ip[4];
    uint16_t src_port, dst_port;
} PacketConfig;

typedef struct { ByteView udp; uint8_t src_ip[4], dst_ip[4]; } Ipv4View;  /* IPv4 交给 UDP 的内容 */
```

| 函数 | 说明 |
| --- | --- |
| `udp_encode(payload, config, out, capacity, written)` | 组装 UDP 数据报 |
| `ipv4_encode(udp, config, out, capacity, written)` | 组装 IPv4 数据报 |
| `ethernet_encode(ip, config, out, capacity, written)` | 组装以太网帧（含填充与 FCS） |
| `ethernet_decode(frame, ip_with_padding)` | 校验并交出以太网数据区（可能含填充） |
| `ipv4_decode(ip_with_padding, ip)` | 解析首部，交出不带填充的 UDP 范围和 IP 地址 |
| `udp_decode(ip, payload)` | 验证校验和，交出应用数据 |

约定：

- 成功返回 `0`，失败返回负值，并在 `stderr` 打印中文原因；成功时 `written` 填写
  实际写出的字节数。
- 解码输出的 `ByteView` 指回调用方传入的原缓冲区，不另做拷贝。
- 每个函数先检查指针、长度和容量，再读写缓冲区。

## 4. 各层格式

### 4.1 UDP 用户数据报

| 字段 | 长度 | 取值 |
| --- | --- | --- |
| 源端口 | 2 | `PacketConfig.src_port` |
| 目的端口 | 2 | `PacketConfig.dst_port` |
| 长度 | 2 | 8 字节首部 + 应用数据长度 |
| 校验和 | 2 | 见下 |

校验和覆盖 **IPv4 伪首部 + UDP 首部 + 应用数据**。伪首部 12 字节，依次为源 IP、
目的 IP、一个零字节、协议号 17 和 UDP 长度；伪首部只参与计算，不写入数据报。
计算前先把校验和字段置零；长度为奇数时末尾补一个零字节，同样只参与计算。算出的
结果是 `0x0000` 时写入 `0xFFFF`，把 `0x0000` 留给“发送端未计算校验和”的含义。

### 4.2 IPv4 数据报

首部固定 20 字节，不使用选项字段，不产生分片。

| 字段 | 取值 |
| --- | --- |
| 版本 / 首部长度 | `0x45`（IPv4，5 个 32 位字） |
| 区分服务 | `0x00` |
| 总长度 | 20 + UDP 长度 |
| 标识 | `0x0000` |
| 标志与片偏移 | `0x4000`（DF=1，MF=0，片偏移 0） |
| 生存时间 | 64 |
| 协议号 | 17（UDP） |
| 首部校验和 | 按 Internet 校验和计算，只覆盖 20 字节首部 |
| 源 / 目的地址 | `PacketConfig` 中的地址 |

解封装依次核对版本号、IHL、总长度与可用字节、首部校验和、保留位、分片标志和协议号，
再用 `总长度 - IHL` 划定 UDP 范围，**把以太网填充排除在外**。

### 4.3 Ethernet II 帧

采用带 4 字节 FCS 的演示格式，从目的 MAC 开始，到 FCS 结束。

| 字段 | 长度 | 取值 |
| --- | --- | --- |
| 目的 MAC | 6 | `PacketConfig.dst_mac` |
| 源 MAC | 6 | `PacketConfig.src_mac` |
| EtherType | 2 | `0x0800`（IPv4） |
| 数据区 | 46 ~ 1500 | IPv4 数据报；不足 46 字节时补零 |
| FCS | 4 | CRC-32，低字节在前 |

帧长限制在 64 ~ 1518 字节。数据区不足 46 字节时补零凑满最小帧，填充字节同样参与
FCS 计算；接收端按 IPv4 的 `总长度` 丢弃填充，因此填充不影响还原结果。

## 5. 构建与运行

源文件按 UTF-8 保存。建议同时开启 `-Wconversion -Werror`，避免字段赋值时的隐式截断。

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror \
    -finput-charset=UTF-8 -fexec-charset=UTF-8 \
    wire.c udp_encode.c ipv4_encode.c ethernet_encode.c \
    ethernet_decode.c ipv4_decode.c udp_decode.c main.c \
    -o pdu_demo

./pdu_demo                     # 默认消息 "Hello, UDP!"
./pdu_demo "任意一段消息"       # 最长 1472 字节（1500 MTU − 20 − 8）
```

演示使用固定配置：`02:00:00:00:00:01 → 02:00:00:00:00:02`，
`192.0.2.1 → 198.51.100.2`，端口 `12345 → 9000`。

## 6. 运行结果

默认消息 `Hello, UDP!`（11 字节）：

```text
=== 封装 ===
原始消息（11 字节）  Hello, UDP!
UDP 数据报 19 字节，IPv4 数据报 39 字节，以太网帧 64 字节。
以太网帧十六进制
02 00 00 00 00 02 02 00 00 00 00 01 08 00 45 00
00 27 00 00 40 00 40 11 4E 8F C0 00 02 01 C6 33
64 02 30 39 23 28 00 13 16 8C 48 65 6C 6C 6F 2C
20 55 44 50 21 00 00 00 00 00 00 00 A6 0E F0 E7
```

解封装逐层核对后输出，三层校验均通过，还原消息与原文比对一致：

```text
Ethernet II（帧长 64 字节）
  目的 MAC  02:00:00:00:00:02
  源 MAC  02:00:00:00:00:01
  EtherType  0x0800（IPv4）
  数据区     46 字节（可能包含填充）
  FCS        0xE7F00EA6（CRC-32 校验通过）
IPv4（首部 20 字节，总长度 39 字节）
  版本       4
  首部长度   20 字节（IHL=5，可选字段 0 字节）
  DSCP       0（区分服务代码点）
  ECN        0（显式拥塞通知）
  总长度     39 字节（数据区 19 字节）
  标识       0x0000
  标志位     保留位=0，DF（禁止分片）=1，MF（后续分片）=0
  片偏移     0（每单位 8 字节）
  TTL        64（剩余生存时间）
  协议号     17（UDP）
  首部校验和 0x4E8F（校验通过）
  源 IP      192.0.2.1
  目的 IP    198.51.100.2
  以太网填充 7 字节（IPv4 总长度之外）
  交给 UDP   19 字节
UDP（首部固定为 8 字节）
  源端口     12345
  目的端口   9000
  长度       19 字节（应用数据 11 字节）
  校验和     0x168C（校验通过）
还原消息（11 字节）  Hello, UDP!
原文比对通过。
```

各层长度随消息变化，可直接对照：

| 应用消息 | UDP | IPv4 | 数据区 | 填充 | 帧长 |
| --- | --- | --- | --- | --- | --- |
| `Hello, UDP!`（11 字节） | 19 | 39 | 46 | 7 | 64 |
| 32 字节 ASCII | 40 | 60 | 60 | 0 | 78 |
| 1472 字节 | 1480 | 1500 | 1500 | 0 | 1518 |

## 7. 测试用例

| 用例 | 输入 | 预期结果 | 实际 |
| --- | --- | --- | --- |
| 短消息 | `Hello, UDP!` | 19 / 39 / 64 字节，填充 7 字节，三层校验通过 | 通过 |
| 无填充 | 32 字节消息 | 40 / 60 / 78 字节，无需填充 | 通过 |
| 最大帧 | 1472 字节消息 | 1480 / 1500 / 1518 字节 | 通过 |
| 超长消息 | 1473 字节消息 | 拒绝，提示最多 1472 字节 | 通过 |
| FCS 错误 | 翻转数据区一位，保留原 FCS | 以太网层报告 FCS 校验失败 | 通过 |
| 首部校验和错误 | 改动 IPv4 首部一位 | IPv4 层报告校验和失败 | 通过 |
| 截短首部 | 传入 63 字节帧 / 19 字节 IPv4 / 7 字节 UDP | 对应层报告长度不足 | 通过 |
| 容量不足 | 传入小于实际长度的输出缓冲区 | 封装函数返回负值，不越界写入 | 通过 |
| 校验和为 0 | 构造使 UDP 校验和算得 `0x0000` 的数据 | 写入 `0xFFFF`，接收端校验通过 | 通过 |

## 8. 错误处理

所有检查都在读写缓冲区之前完成，失败时向 `stderr` 输出中文原因并返回负值。
典型消息：

| 层 | 检查项 | 消息示例 |
| --- | --- | --- |
| UDP 封装 | 输出容量 | `UDP 封装失败：输出缓冲区容量不足。` |
| IPv4 封装 | UDP 长度字段与实参不符 | `IPv4 封装失败：UDP 长度无效。` |
| Ethernet 封装 | IPv4 数据报超出数据区上限 | `Ethernet II 封装失败：IPv4 数据报长 1501 字节，超过以太网数据区上限 1500 字节。` |
| Ethernet 解封装 | FCS | `Ethernet II 解封装失败：FCS 为 0x…，计算值为 0x…。` |
| IPv4 解封装 | 首部校验和 | `IPv4 解封装失败：首部校验和 0x4F8F 验证失败。` |
| IPv4 解封装 | 标志位保留位 | `IPv4 解封装失败：标志位中的保留位应为 0。` |
| UDP 解封装 | 长度字段 | `UDP 解封装失败：长度字段为 17733，IPv4 数据区有 40 字节。` |

## 9. 已知限制

- 只处理 IPv4 + UDP 的单帧数据报，不支持分片、IPv6、TCP 和 IPv4 选项字段。
- MAC 与 IP 地址来自固定的 `PacketConfig`，程序不做 ARP 解析，也不访问真实网卡。
- FCS 按课程要求的演示格式显式放在帧尾并参与收发；真实以太网的 FCS 由网卡生成
  和校验，抓包软件通常不显示。
- 应用消息受 1500 字节 MTU 限制，最多 1472 字节。

## 10. 分支说明

| 分支 | 内容 |
| --- | --- |
| `main` | 汇总分支，报告以该分支代码的运行输出为准 |
| `feat/common-interface` | 公共头文件 `pdu.h` |
| `feat/member-a` | UDP 封装、IPv4 解封装、Ethernet 封装、`wire.c` |
| `feat/member-b` | IPv4 封装、Ethernet 解封装、UDP 解封装、`main.c` |

分工与接口细节见 [docs/双人分工.md](docs/双人分工.md)、
[docs/接口与联调约定.md](docs/接口与联调约定.md)，协作流程见
[docs/Git协作指南.md](docs/Git协作指南.md)。
