/* pkg.h — WHNos 简易包管理器 (类 apt 最小实现)
 *
 * 由于 WHNos 尚无文件系统，包管理采用内存模式:
 *   - 源: HTTP 服务器 (QEMU user 网络下为 10.0.2.2)
 *   - 索引文件: /Packages, 每行一个包:
 *         name version size checksum path
 *     其中 size/checksum 为十进制/十六进制数字, path 为包在源上的绝对路径;
 *     '#' 开头或空行为注释/空行。
 *   - "安装" = 通过 HTTP 拉取包内容到内存, 校验 size+checksum 后登记为已安装。
 */
#ifndef PKG_H
#define PKG_H

#include <stdint.h>

#define PKG_UPDATE_OK   0
#define PKG_UPDATE_FAIL -1

namespace Pkg {

    // 包名字段容量 (供 shell 分配缓冲区)
    enum { MAX_NAME = 32 };

    // 从源 (host:port) 拉取 /Packages 索引并解析到内存包表。
    // 返回 0 成功, 非 0 失败。失败原因可用 lastError() 查看。
    int  update(const char* host, uint16_t port);

    // 列出包表; onlyInstalled=true 只列已安装。
    void list(bool onlyInstalled);

    // 安装指定名字的包; 成功返回 0。
    int  install(const char* name, const char* host, uint16_t port);

    // 查看已安装包的内容 (打印前 amount 字节); 未安装返回 -1。
    int  cat(const char* name, int amount);

    // 最近一次操作失败原因 (ASCII, 内部静态缓冲)
    const char* lastError();

    // 已加载包数
    int  count();
}

#endif
