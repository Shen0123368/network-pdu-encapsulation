# 各层次网络协议数据单元封装与解封装

计算机网络课程双人作业。用 C 语言在内存中构造 UDP 用户数据报、IPv4 数据报和 Ethernet II 帧，再用同一段字节流逐层解析、打印字段含义和校验结果，最后输出还原的应用消息。

```text
封装      原始消息 → UDP → IPv4 → Ethernet II
解封装    Ethernet II → IPv4 → UDP → 原始消息
```

## 开工入口

- 确认每人的角色和负责文件。[双人分工](docs/双人分工.md)
- 一起确认函数签名和跨层参数，再由 A 提交 `pdu.h`。[接口与联调约定](docs/接口与联调约定.md)
- 各自建分支，按合并顺序接入主程序。[Git 协作指南](docs/Git协作指南.md)

## 交付内容

- C 源代码与头文件。
- PDF 运行结果报告，包含程序结构、两人分工、封装和解封装的实际运行输出、测试结果。

## 编译与运行

在仓库根目录用 GCC 编译并运行。源文件按 UTF-8 保存。

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror -finput-charset=UTF-8 -fexec-charset=UTF-8 main.c udp_encode.c udp_decode.c ipv4_encode.c ipv4_decode.c ethernet_encode.c ethernet_decode.c wire.c -o pdu-demo.exe
.\pdu-demo.exe
.\pdu-demo.exe "0123456789abcdefghijklmnopqrstuv"
```

默认消息为 `Hello, UDP!`。也可通过一个命令行参数传入消息，最长为 1472 字节。运行时会显示各层长度、完整帧十六进制、解析字段、三层校验和还原消息。
