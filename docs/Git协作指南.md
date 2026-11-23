# Git 协作指南

仓库采用 `main` 汇总，分工文件见[双人分工](双人分工.md)。在 GitHub 接受协作者邀请后，队友可用自己的 GitHub 账号克隆和推送分支。

## 第一次开工

```bash
git clone https://github.com/Shen0123368/network-pdu-encapsulation.git
cd network-pdu-encapsulation
```

A 和 B 确认[接口与联调约定](接口与联调约定.md)。A 先在 `feat/common-interface` 分支提交 `pdu.h`，可一并提交 `wire.c`。在 GitHub 开一个目标为 `main` 的 Pull Request，B 检查接口，A 合并。之后两人从更新后的 `main` 建分支。

```bash
# A、B 均执行
git switch main
git pull --ff-only origin main

# A 执行
git switch -c feat/member-a

# B 执行
git switch -c feat/member-b
```

## 提交与合并

1. 每人只暂存自己这次改动涉及的文件。提交前用 `git status --short` 核对。例如，A 的 UDP 封装完成后执行如下命令。

   ```bash
   git add udp_encode.c
   git commit -m "实现 UDP 封装"
   git push -u origin feat/member-a
   ```

2. B 用同样的方式提交自己负责的文件，并推送到 `feat/member-b`。双方在 GitHub 建 Pull Request，写明改动和跑过的用例，把样例输出贴到描述里给对方核对。
3. 建议先合并 A 的模块分支。开发过程中可分别用小驱动检查模块，完整程序等两个分支的六个函数都齐全后编译。
4. B 在自己的分支拉取 `main` 的新提交，再编译并跑完整流程。若接口有变，两人先对齐头文件和调用参数，再继续合并。

   ```bash
   git switch feat/member-b
   git fetch origin
   git merge origin/main
   git push
   ```

5. 两人核对正常帧和错误帧的实际输出，合并 B 的分支。以合并后 `main` 上的运行输出制作 PDF。

接口改动由双方先确认，再由 A 更新 `pdu.h`。B 负责更新 `main.c` 中的调用。交换样例时附上源、目的地址、长度和预期字段，方便对方直接验证自己的模块。
