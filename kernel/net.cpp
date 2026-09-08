/* net.cpp — WHNos 网络协议基础: 以太网 + ARP + IPv4 + ICMP
 *
 * 目标: 让 WHNos 能通过网卡联网。当前实现:
 *   - PCI 枚举 -> RTL8139 驱动 (轮询)
 *   - 静态 IP 配置 (QEMU user 网络: 10.0.2.15/24, 网关 10.0.2.2)
 *   - ARP 解析邻居 MAC
 *   - ICMP echo 收发 (可被 ping, 也可 ping 网关验证连通)
 * 后续扩展: DHCP / UDP / TCP / HTTP, 进而实现 apt 拉包。
 */
#include "net.h"
#include "vga.h"
#include "serial.h"
#include "pit.h"
#include "pci.h"
#include "rtl8139.h"
#include "e1000.h"

namespace Net {

    // 网卡选择: true = Intel e1000 (VMware E1000 / QEMU e1000)
    //           false = RTL8139 (QEMU 老型号 / 实机老网卡)
    static bool gUseE1000 = false;

    uint8_t  myMAC[6] = {0};
    uint32_t myIP  = 0x0F02000A;   // 10.0.2.15  内存中的网络序字节 [0A 00 02 0F]
    uint32_t myMask= 0x00FFFFFF;   // 255.255.255.0
    uint32_t myGW  = 0x0202000A;   // 10.0.2.2   网关
    uint8_t  gwMAC[6] = {0};
    bool     gwKnown = false;

    static uint32_t gRx = 0, gTx = 0, gErr = 0;
    static bool gUp = false;

    // ARP 单条目缓存 (最近解析的邻居)
    static uint32_t gArpIP = 0;
    static uint8_t  gArpMAC[6] = {0};
    static bool     gArpCached = false;

    // ICMP echo 等待
    static uint16_t gEchoId = 0, gEchoSeq = 0;
    static bool gEchoDone = false, gEchoOk = false;

    static uint8_t gTxFrame[NET_MTU];
    static uint8_t gRxFrame[NET_MTU];

    // 前向声明 (init 中在定义前调用)
    static bool arpResolve(uint32_t ip);
    static int  pollOnce();

    // ---- 内部工具 ----

    static uint16_t in16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
    static void     packs(uint8_t* p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
    static uint32_t in32(const uint8_t* p) { return ((uint32_t)in16(p) << 16) | in16(p + 2); }

    static uint16_t checksum(const uint8_t* data, int len) {
        uint32_t sum = 0;
        int i = 0;
        for (; i + 1 < len; i += 2) sum += (uint16_t)((data[i] << 8) | data[i + 1]);
        if (i < len) sum += (uint16_t)(data[i] << 8);
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        return (uint16_t)~sum;
    }

    static void ethSwappedMAC(const uint8_t* theirMAC) {
        for (int i = 0; i < 6; i++) {
            gTxFrame[i] = theirMAC[i];       // dst
            gTxFrame[6 + i] = myMAC[i];      // src
        }
    }

    // ---- 帧构造 ----

    static void sendARPFrame(bool isReq, const uint8_t* peerMAC, uint32_t peerIP) {
        const uint8_t* dstMAC = isReq ? (const uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF" : peerMAC;
        ethSwappedMAC(dstMAC);
        packs(gTxFrame + 12, 0x0806);        // ethertype = ARP
        uint8_t* a = gTxFrame + 14;
        packs(a + 0, 1); a[2] = 0x08; a[3] = 0x00;      // htype=1, ptype=IP
        a[4] = 6; a[5] = 4;                             // hlen, plen
        packs(a + 6, isReq ? 1 : 2);                    // op
        for (int i = 0; i < 6; i++) { a[8 + i] = myMAC[i]; a[18 + i] = isReq ? 0 : peerMAC[i]; }
        for (int i = 0; i < 4; i++) { a[14 + i] = ((const uint8_t*)&myIP)[i]; a[24 + i] = ((const uint8_t*)&peerIP)[i]; }
        sendRaw(gTxFrame, 14 + 28);
    }

    // 发送 ICMP 报文 (通过 dstMAC 指定下一跳)
    static void sendICMP(bool req, uint32_t dstIP, const uint8_t* dstMAC,
                         uint16_t id, uint16_t seq, const uint8_t* payload, int plen) {
        ethSwappedMAC(dstMAC);
        packs(gTxFrame + 12, 0x0800);        // ethertype = IPv4
        uint8_t* ip = gTxFrame + 14;
        ip[0] = 0x45; ip[1] = 0;
        int totalLen = 20 + 8 + plen;
        packs(ip + 2, totalLen);
        packs(ip + 4, (uint16_t)(0x0100 + (gTx & 0x7FFF)));
        packs(ip + 6, 0x0000);
        ip[8] = 64; ip[9] = 1;               // ttl, proto=ICMP
        packs(ip + 10, 0);
        for (int i = 0; i < 4; i++) { ip[12 + i] = ((const uint8_t*)&myIP)[i]; ip[16 + i] = ((const uint8_t*)&dstIP)[i]; }
        packs(ip + 10, checksum(ip, 20));

        uint8_t* ic = ip + 20;
        ic[0] = req ? 8 : 0; ic[1] = 0;
        packs(ic + 2, 0);
        packs(ic + 4, id);
        packs(ic + 6, seq);
        for (int i = 0; i < plen; i++) ic[8 + i] = payload[i];
        packs(ic + 2, checksum(ic, 8 + plen));

        sendRaw(gTxFrame, 14 + totalLen);
    }

    // ---- 对外接口 ----

    void init() {
        VGA::write("Net: scanning PCI... ");

        // 优先尝试 Intel e1000 (VMware E1000 / QEMU e1000)
        if (E1000::init()) {
            gUseE1000 = true;
            VGA::writeLine("found Intel Pro/1000 (e1000)");
        } else {
            VGA::writeLine("no e1000, trying RTL8139...");
            uint16_t bdf = PCI::findDevice(PCI_VENDOR_REALTEK, PCI_DEV_RTL8139);
            if (bdf == 0xFFFF) { VGA::writeLine("no supported NIC found"); return; }
            VGA::write("found RTL8139 (bus0 dev ");
            char b[8];
            b[0] = '0' + ((bdf >> 8) / 10);
            b[1] = '0' + ((bdf >> 8) % 10);
            b[2] = '\0';
            VGA::write(b);
            VGA::writeLine(")");

            if (!RTL8139::init()) { VGA::writeLine("Net: RTL8139 init failed"); return; }
            VGA::write("  IO 0x");
            {
                static const char* hx = "0123456789ABCDEF";
                uint32_t io = RTL8139::ioBase();
                char hb[9];
                for (int i = 7; i >= 0; i--) hb[7 - i] = hx[(io >> (i * 4)) & 0xF];
                hb[8] = '\0';
                VGA::write(hb);
            }
            VGA::writeLine("");
        }

        const uint8_t* m = gUseE1000 ? E1000::mac() : RTL8139::mac();
        for (int i = 0; i < 6; i++) myMAC[i] = m[i];
        gUp = true;

        char s[32];
        static const char* hx = "0123456789ABCDEF";
        VGA::write("Net: MAC ");
        macStr(s); VGA::write(s);
        Serial::write("Net: MAC "); Serial::writeln(s);
        VGA::write("  IP ");
        ipStr(s); VGA::write(s);
        Serial::write("Net: IP "); Serial::writeln(s);
        VGA::write("  GW ");
        gatewayStr(s); VGA::writeLine("");
        Serial::write("Net: GW "); Serial::writeln(s);

        VGA::write("Net: resolving gateway... ");
        Serial::write("Net: resolving gateway... ");
        if (arpResolve(myGW)) {
            VGA::write("ok  gw MAC ");
            Serial::write("ok  gw MAC ");
            char buf[20];
            for (int i = 0; i < 6; i++) {
                buf[i * 3]     = hx[gwMAC[i] >> 4];
                buf[i * 3 + 1] = hx[gwMAC[i] & 0xF];
                buf[i * 3 + 2] = ':';
            }
            buf[17] = '\0';
            VGA::writeLine(buf);
            Serial::writeln(buf);
        } else {
            VGA::writeLine("timed out (will retry on demand)");
            Serial::writeln("timed out (will retry on demand)");
        }
    }

    bool isUp() { return gUp; }

    const char* nicName() {
        if (!gUp) return "none";
        return gUseE1000 ? "Intel e1000 (MMIO)" : "RTL8139 (IO)";
    }
    uint32_t rxCount() { return gRx; }
    uint32_t txCount() { return gTx; }
    uint32_t errCount() { return gErr; }

    void macStr(char* out) {
        static const char* hx = "0123456789ABCDEF";
        for (int i = 0; i < 6; i++) {
            out[i * 3]     = hx[myMAC[i] >> 4];
            out[i * 3 + 1] = hx[myMAC[i] & 0xF];
            out[i * 3 + 2] = ':';
        }
        out[17] = '\0';
    }

    void ipToStr(uint32_t ip, char* out) {
        const uint8_t* p = (const uint8_t*)&ip;
        auto wr = [&](int v) {
            if (v >= 100) *out++ = (char)('0' + v / 100);
            if (v >= 10)  *out++ = (char)('0' + (v / 10) % 10);
            *out++ = (char)('0' + v % 10);
        };
        wr(p[0]); *out++ = '.';
        wr(p[1]); *out++ = '.';
        wr(p[2]); *out++ = '.';
        wr(p[3]); *out = '\0';
    }

    void ipStr(char* out) { ipToStr(myIP, out); }
    void maskStr(char* out) { ipToStr(myMask, out); }
    void gatewayStr(char* out) { ipToStr(myGW, out); }

    uint32_t parseIP(const char* s) {
        uint8_t o[4] = {0, 0, 0, 0};
        int n = 0, v = 0;
        if (!s) return 0xFFFFFFFF;
        for (const char* p = s;; p++) {
            if (*p >= '0' && *p <= '9') v = v * 10 + (*p - '0');
            else if (*p == '.') {
                if (n > 3 || v > 255) return 0xFFFFFFFF;
                o[n++] = (uint8_t)v; v = 0;
            }
            else if (*p == '\0') {
                if (n != 3 || v > 255) return 0xFFFFFFFF;
                o[3] = (uint8_t)v;
                uint32_t ip = 0;
                for (int i = 0; i < 4; i++) ((uint8_t*)&ip)[i] = o[i];
                return ip;
            }
            else return 0xFFFFFFFF;
        }
    }

    void sendRaw(const uint8_t* frame, int len) {
        if (!gUp) return;
        if (gUseE1000) E1000::sendFrame(frame, len);
        else           RTL8139::sendFrame(frame, len);
        gTx++;
    }

    // ---- ARP 解析 (带阻塞轮询) ----

    static bool arpResolve(uint32_t ip) {
        if (ip == myGW && gwKnown) return true;
        if (gArpCached && gArpIP == ip) return true;

        sendARPFrame(true, 0, ip);
        uint32_t start = PIT::ticks();
        while (PIT::ticks() - start < 3000) {
            pollOnce();
            if (ip == myGW && gwKnown) return true;
            if (gArpCached && gArpIP == ip) return true;
        }
        return (ip == myGW) ? gwKnown : (gArpCached && gArpIP == ip);
    }

    // ---- 收包 ----

    static int pollOnce() {
        int n;
        if (gUseE1000) n = E1000::pollFrame(gRxFrame, NET_MTU);
        else           n = RTL8139::pollFrame(gRxFrame, NET_MTU);
        if (n <= 0) return 0;
        onRx(gRxFrame, n);
        return 1;
    }

    void poll() {
        for (int i = 0; i < 8; i++) {
            if (pollOnce() == 0) break;
        }
    }

    void onRx(const uint8_t* frame, int len) {
        if (len < ETH_HDR_LEN + 4) return;
        gRx++;
        uint16_t type = in16(frame + 12);
        if (type == 0x0806)       { handleArp(frame + 14); }
        else if (type == 0x0800)  { handleIPv4(frame + 14, frame + 6); }
    }

    void handleArp(const uint8_t* a) {
        uint16_t op = in16(a + 6);
        uint32_t spa = 0, tpa = 0;
        for (int i = 0; i < 4; i++) {
            ((uint8_t*)&spa)[i] = a[14 + i];
            ((uint8_t*)&tpa)[i] = a[24 + i];
        }
        if (!gwKnown && spa == myGW) {
            for (int i = 0; i < 6; i++) gwMAC[i] = a[8 + i];
            gwKnown = true;
        }
        if (op == 1 && tpa == myIP) {          // ARP request -> 应答
            uint8_t mac[6];
            for (int i = 0; i < 6; i++) mac[i] = a[8 + i];
            sendARPFrame(false, mac, spa);
        } else if (op == 2) {                  // ARP reply -> 学习
            gArpIP = spa;
            for (int i = 0; i < 6; i++) gArpMAC[i] = a[8 + i];
            gArpCached = true;
        }
    }

    void handleIPv4(const uint8_t* ip, const uint8_t* srcMAC) {
        if ((ip[0] >> 4) != 4) return;
        int ihl = (ip[0] & 0xF) * 4;
        uint32_t dst = 0;
        for (int i = 0; i < 4; i++) ((uint8_t*)&dst)[i] = ip[16 + i];
        if (dst != myIP && dst != 0xFFFFFFFF) return;   // 仅本机/广播

        if (ip[9] == 1) {                       // ICMP
            const uint8_t* ic = ip + ihl;
            if (ic[0] == 8) {                   // echo request -> 回显应答
                uint16_t id = in16(ic + 4), seq = in16(ic + 6);
                int plen = (int)in16(ip + 2) - ihl - 8;
                if (plen < 0) plen = 0;
                // 应答目标 MAC: 优先用 ARP 缓存中的来源 (QEMU 中通常即网关)
                const uint8_t* dmac = gwMAC;
                if (gArpCached && gArpIP == dst) dmac = gArpMAC;
                else if (srcMAC) dmac = srcMAC;
                sendICMP(false, dst, dmac, id, seq, ic + 8, plen);
            } else if (ic[0] == 0) {            // echo reply
                uint16_t id = in16(ic + 4), seq = in16(ic + 6);
                VGA::writeLine("icmp reply rx");
                if (id == gEchoId && seq == gEchoSeq) { gEchoDone = true; gEchoOk = true; }
            }
        } else if (ip[9] == 6) {                // TCP
            handleTCP(ip, ip + ihl, (int)in16(ip + 2) - ihl);
        }
    }

    // ---- TCP (单连接, 轮询) ----
    // 为 apt 拉包打地基: 一次只维护一个主动连接。
    enum { TCP_CLOSED = 0, TCP_SYN_SENT = 1, TCP_ESTABLISHED = 2 };
    static int gTcpState = TCP_CLOSED;
    static uint32_t gTcpSnd = 0;     // 本端下一个待发送的序列号
    static uint32_t gTcpRcv = 0;     // 期望接收的序列号 (RCV.NXT)
    static uint32_t gTcpPeerIP = 0;
    static uint16_t gTcpLPort = 0;
    static uint16_t gTcpRPort = 0;
    static uint8_t  gTcpPeerMAC[6];
    static uint8_t* gTcpBuf = 0;  // 接收缓冲 (调用方提供)
    static int  gTcpBufCap = 0, gTcpBufLen = 0;
    static bool gTcpDone = false, gTcpOk = false;
    static uint32_t gTcpLastRx = 0;  // 最近一次有效收包时刻 (空闲判定)

    static void packs32(uint8_t* p, uint32_t v) {
        p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
    }

    // TCP 段校验和 = 伪头(源IP+目的IP+0+proto6+段长) + 段 (校验和字段须为 0)
    static uint16_t tcpSum(const uint8_t* seg, int len) {
        uint32_t sum = 0;
        const uint8_t* s = (const uint8_t*)&myIP;
        const uint8_t* d = (const uint8_t*)&gTcpPeerIP;
        for (int i = 0; i < 4; i += 2) sum += (uint16_t)((s[i] << 8) | s[i + 1]);
        for (int i = 0; i < 4; i += 2) sum += (uint16_t)((d[i] << 8) | d[i + 1]);
        sum += 0x0006;                        // protocol = TCP
        sum += (uint16_t)len;                 // TCP length
        for (int i = 0; i + 1 < len; i += 2) sum += (uint16_t)((seg[i] << 8) | seg[i + 1]);
        if (len & 1) sum += (uint16_t)(seg[len - 1] << 8);
        while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
        return (uint16_t)~sum;
    }

    // 发送一个 TCP 段 (封装成 IPv4 + 以太网)
    static void sendTCP(uint8_t flags, uint32_t seq, uint32_t ack,
                        const uint8_t* payload, int plen) {
        ethSwappedMAC(gTcpPeerMAC);
        packs(gTxFrame + 12, 0x0800);
        uint8_t* ip = gTxFrame + 14;
        ip[0] = 0x45; ip[1] = 0;
        int totalLen = 20 + 20 + plen;
        packs(ip + 2, totalLen);
        packs(ip + 4, (uint16_t)(0x0100 + (gTx & 0x7FFF)));
        packs(ip + 6, 0x0000);
        ip[8] = 64; ip[9] = 6;                // ttl, proto = TCP
        packs(ip + 10, 0);
        for (int i = 0; i < 4; i++) {
            ip[12 + i] = ((const uint8_t*)&myIP)[i];
            ip[16 + i] = ((const uint8_t*)&gTcpPeerIP)[i];
        }
        packs(ip + 10, checksum(ip, 20));

        uint8_t* t = ip + 20;
        packs(t + 0, gTcpLPort);
        packs(t + 2, gTcpRPort);
        packs32(t + 4, seq);
        packs32(t + 8, ack);
        t[12] = 0x50;                 // 数据偏移 = 5 words (无选项)
        t[13] = flags;
        packs(t + 14, 0x2000);        // 窗口 8192
        packs(t + 16, 0);             // 校验和占位
        packs(t + 18, 0);             // urgent pointer
        for (int i = 0; i < plen; i++) t[20 + i] = payload[i];
        packs(t + 16, tcpSum(t, 20 + plen));

        sendRaw(gTxFrame, 14 + totalLen);
    }

    // 处理属于当前连接的一个 TCP 段; 驱动握手/数据/Ack/FIN 状态机
    static void tcpOnSeg(const uint8_t* t, int segLen) {
        if (segLen < 20) return;
        if (in16(t) != gTcpRPort) return;     // 源端口必须匹配
        uint8_t flags = t[13];
        int doff = ((t[12] >> 4) & 0xF) * 4;
        if (doff < 20 || doff > segLen) return;
        int dataLen = segLen - doff;

        if (flags & 0x04) {                   // RST
            gTcpDone = true; gTcpOk = false;
            return;
        }

        if (gTcpState == TCP_SYN_SENT && (flags & 0x02)) {
            // 收到 SYN-ACK: 完成三次握手
            gTcpRcv = in32(t + 4) + 1;        // RCV.NXT = 对端 ISN + 1
            gTcpSnd = in32(t + 8);            // ack = 确认我方 SYN
            gTcpState = TCP_ESTABLISHED;
            gTcpLastRx = PIT::ticks();
            sendTCP(0x10, gTcpSnd, gTcpRcv, 0, 0);   // 回 ACK
        } else if (gTcpState == TCP_ESTABLISHED) {
            bool hasData = false;
            if (dataLen > 0 ) {
                const uint8_t* p = t + doff;
                int n = dataLen;
                int room = gTcpBufCap - gTcpBufLen;
                if (n > room) n = room;
                for (int i = 0; i < n; i++) gTcpBuf[gTcpBufLen + i] = p[i];
                gTcpBufLen += n;
                gTcpRcv += dataLen;           // 简化: 假定按序到达
                hasData = true;
            }
            if (hasData || (flags & 0x01)) {
                gTcpLastRx = PIT::ticks();
                sendTCP(0x10, gTcpSnd, gTcpRcv, 0, 0);   // 回 ACK
            }
            if (flags & 0x01) {               // FIN: 对端发完
                gTcpDone = true;
                gTcpOk = gTcpBufLen > 0;
            }
        }
    }

    static void handleTCP(const uint8_t* ip, const uint8_t* t, int segLen) {
        if (segLen < 20) return;
        if (in16(t + 2) != gTcpLPort) return; // 目标端口必须匹配当前连接
        tcpOnSeg(t, segLen);
    }

    // ---- ping ----

    bool pingOnce(uint32_t ip, int seq) {
        if (!gUp) return false;

        // 同子网直连, 否则走网关
        uint32_t nextHop;
        if ((ip & myMask) == (myIP & myMask)) nextHop = ip;
        else nextHop = myGW;

        // 解析下一跳 MAC
        const uint8_t* dmac = 0;
        if (nextHop == myGW) {
            if (!gwKnown && !arpResolve(myGW)) return false;
            dmac = gwMAC;
        } else {
            if (!gArpCached || gArpIP != ip) {
                if (!arpResolve(ip)) return false;
            }
            dmac = gArpMAC;
        }

        uint8_t payload[32];
        for (int i = 0; i < 32; i++) payload[i] = (uint8_t)i;
        gEchoId = (uint16_t)(gTx & 0xFFFF);
        gEchoSeq = (uint16_t)seq;
        gEchoDone = false; gEchoOk = false;

        sendICMP(true, ip, dmac, gEchoId, gEchoSeq, payload, 32);

        uint32_t start = PIT::ticks();
        while (PIT::ticks() - start < 3000) {
            pollOnce();
            if (gEchoDone) break;
        }
        return gEchoOk;
    }

    // ---- http ----

    int httpGet(uint32_t dstIP, uint16_t dstPort,
                const char* host, const char* path,
                uint8_t* buf, int cap) {
        if (!gUp || !buf || cap <= 0) return -1;

        // 1. 解析下一跳 MAC
        if (dstIP == myGW) {
            if (!gwKnown && !arpResolve(dstIP)) return -1;
            for (int i = 0; i < 6; i++) gTcpPeerMAC[i] = gwMAC[i];
        } else {
            if (!gArpCached || gArpIP != dstIP) {
                if (!arpResolve(dstIP)) return -1;
            }
            if (!gArpCached || gArpIP != dstIP) return -1;
            for (int i = 0; i < 6; i++) gTcpPeerMAC[i] = gArpMAC[i];
        }

        gTcpPeerIP = dstIP;
        gTcpLPort  = (uint16_t)(40000 + (gTx & 0x3FFF));
        gTcpRPort  = dstPort;
        gTcpSnd    = 0x10000000 + (gTx & 0x0FFFFFFF);   // 本端 ISN (伪随机)
        gTcpRcv    = 0;
        gTcpState  = TCP_SYN_SENT;
        gTcpDone   = false; gTcpOk = false;
        gTcpBuf    = buf; gTcpBufCap = cap; gTcpBufLen = 0;
        gTcpLastRx = 0;

        sendTCP(0x02, gTcpSnd, 0, 0, 0);             // SYN

        // 2. 等 SYN-ACK, 完成三次握手
        {
            uint32_t start = PIT::ticks();
            while (PIT::ticks() - start < 5000 && gTcpState != TCP_ESTABLISHED) {
                pollOnce();
            }
        }
        if (gTcpState != TCP_ESTABLISHED) { gTcpState = TCP_CLOSED; return -1; }
        Serial::writeln("tcp: established");

        // 3. 发送 HTTP GET (PSH+ACK)
        {
            char req[320]; int rl = 0;
            const char* s;
            s = "GET ";              while (*s) req[rl++] = *s++;
            s = path;                while (*s) req[rl++] = *s++;
            s = " HTTP/1.1\r\nHost: "; while (*s) req[rl++] = *s++;
            s = host;                while (*s) req[rl++] = *s++;
            s = "\r\nConnection: close\r\n\r\n"; while (*s) req[rl++] = *s++;
            sendTCP(0x18, gTcpSnd, gTcpRcv, (const uint8_t*)req, rl);  // PSH+ACK
            gTcpSnd += rl;
        }

        // 4. 收响应: 对端 FIN 结束, 或已有数据后空闲 3s, 或总超时 10s
        {
            uint32_t start = PIT::ticks();
            while (!gTcpDone) {
                pollOnce();
                if (PIT::ticks() - start > 10000) break;
                if (gTcpBufLen > 0 && PIT::ticks() - gTcpLastRx > 3000) break;
            }
        }

        int result = gTcpOk ? gTcpBufLen : -1;
        gTcpState = TCP_CLOSED;
        return result;
    }
}
