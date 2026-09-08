/* net.h — WHNos 网络子系统 (TCP/IP 协议基础)
 * 目标: 让系统能通过网卡联网，为后续 "apt 拉包" 提供传输基础。
 * 当前实现: 以太网 + ARP + IPv4 + ICMP(echo) 轮询模型。
 */
#ifndef NET_H
#define NET_H

#include <stdint.h>

#define NET_MTU 1514        // 最大帧长 (14 头 + 1500 payload 略留余量)
#define ETH_HDR_LEN 14

namespace Net {

    // ---- 初始化 / 轮询 ----
    void init();        // PCI 枚举 → 网卡驱动 → 静态配置 IP → ARP 解析网关
    void poll();        // 处理一个到达帧 (在 shell 空闲/等待时调用)
    bool isUp();

    // ---- 协议 ----
    // 发送 ICMP echo request 到 ip; 阻塞等待 reply 或超时(约600ms)
    bool pingOnce(uint32_t ip, int seq);

    // ---- HTTP (基于 TCP/80, 为 apt 拉包打地基) ----
    // 对 dstIP:dstPort 发起 TCP 三次握手, 发送 HTTP GET, 阻塞收响应.
    // host = Host 头内容, path = 请求路径 (如 "/"), 响应(含 header)拷入 buf.
    // 返回收到的字节数; 失败返回 -1.
    int httpGet(uint32_t dstIP, uint16_t dstPort,
                const char* host, const char* path,
                uint8_t* buf, int cap);

    // ---- 状态查询 (给 shell) ----
    const char* nicName();
    void macStr(char* out);
    void ipStr(char* out);
    void maskStr(char* out);
    void gatewayStr(char* out);
    uint32_t rxCount();
    uint32_t txCount();
    uint32_t errCount();

    // ---- 工具函数 ----
    // "10.0.2.2" -> uint32 (网络序); 失败返回 0xFFFFFFFF
    uint32_t parseIP(const char* s);
    void ipToStr(uint32_t ip, char* out);

    // 供 rtl8139 / 内部使用
    void onRx(const uint8_t* frame, int len);       // 以太网帧入口
    void handleArp(const uint8_t* frame);           // frame 指向 ARP 报文
    void handleIPv4(const uint8_t* frame, const uint8_t* srcMAC); // frame 指向 IP 报文
    void sendRaw(const uint8_t* frame, int len);

    extern uint8_t  myMAC[6];
    extern uint32_t myIP;       // 网络序
    extern uint32_t myMask;
    extern uint32_t myGW;
    extern uint8_t  gwMAC[6];
    extern bool     gwKnown;
}

#endif
