/* rtl8139.cpp — Realtek RTL8139 网卡驱动
 * 访问方式: IO 端口 (BAR1), 轮询收发 (不使用中断)。
 * 接收缓冲区: 8K+16 字节环形缓冲, 由 RBSTART 指定物理地址。
 */
#include "rtl8139.h"
#include "pci.h"
#include "port.h"
#include "net.h"
#include "vga.h"

#define RX_BUF_LEN (8192 + 16)

namespace RTL8139 {

    static bool    gPresent = false;
    static bool    gDbgTx = true;
    static int     gDbgRx = 4;
    static uint32_t gIO = 0;
    static uint8_t  gMAC[6];
    static uint8_t  gRxBuf[RX_BUF_LEN] __attribute__((aligned(16)));
    static uint8_t  gTxBuf[1536] __attribute__((aligned(16)));
    static uint32_t gCapr;      // 上次消费到的 ring 偏移
    static uint32_t gTxSlot;    // 当前使用的发送描述符

    // 寄存器偏移 (相对 IO base)
    enum : uint16_t {
        REG_IDR0   = 0x00,   // MAC (6)
        REG_TSD0   = 0x10,   // 发送状态
        REG_TSAD0  = 0x20,   // 发送地址
        REG_RBSTART= 0x30,   // 接收缓冲物理地址
        REG_CR     = 0x37,
        REG_CAPR   = 0x38,
        REG_CBR    = 0x3A,
        REG_ISR    = 0x3E,
        REG_TCR    = 0x40,
        REG_RCR    = 0x44,
    };

    static inline void outb(uint16_t r, uint8_t  v) { Port::outb(gIO + r, v); }
    static inline void outw(uint16_t r, uint16_t v) { Port::outw(gIO + r, v); }
    static inline void outl(uint16_t r, uint32_t v) { Port::outd(gIO + r, v); }
    static inline uint8_t  inb(uint16_t r) { return Port::inb(gIO + r); }
    static inline uint16_t inw(uint16_t r) { return Port::inw(gIO + r); }
    static inline uint32_t inl(uint16_t r) { return Port::ind(gIO + r); }

    // 从环形缓冲读取 len 字节到 dst, 自动处理回绕
    static void ringRead(uint32_t off, uint8_t* dst, int len) {
        for (int i = 0; i < len; i++) {
            dst[i] = gRxBuf[(off + i) % RX_BUF_LEN];
        }
    }

    bool init() {
        uint16_t bdf = PCI::findDevice(PCI_VENDOR_REALTEK, PCI_DEV_RTL8139);
        if (bdf == 0xFFFF) return false;

        uint8_t dev = (uint8_t)(bdf >> 8);
        // 开启 IO 空间 + 内存空间 + Bus Master (PCI CMD 位 0/1/2)
        uint32_t cmd = PCI::readConfig(0, dev, 0, 0x04);
        PCI::writeConfig(0, dev, 0, 0x04, cmd | 0x07);

        // QEMU RTL8139: BAR0 = IO 端口, BAR1 = MMIO; 本驱动走 IO 模式
        uint32_t bar0 = PCI::readConfig(0, dev, 0, 0x10);
        gIO = bar0 & ~3;
        if (gIO == 0) return false;

        // 读取 MAC
        for (int i = 0; i < 6; i++) gMAC[i] = inb(REG_IDR0 + i);

        // 软复位
        outb(REG_CR, 0x10);
        for (int t = 0; t < 1000; t++) {
            if (!(inb(REG_CR) & 0x10)) break;
        }

        // 接收: 设置 ring 起始地址, 重置消费指针
        outl(REG_RBSTART, (uint32_t)(uintptr_t)gRxBuf);
        gCapr = 0;
        outw(REG_CAPR, 0);

        // 清中断
        outw(REG_ISR, 0xFFFF);

        // 发送配置 (寄存任务默认), 接收配置: 接受广播/多播/物理匹配, 8K+16
        // RCR: WRAP(7) | AR(6) | AM(3) | AB(2) | RER(1) | AER(0) = 0xCF
        outl(REG_TCR, 0);
        outl(REG_RCR, (1u << 7) | (1u << 6) | (1u << 3) | (1u << 2) | (1u << 1) | (1u << 0));

        // 启动收发
        outb(REG_CR, 0x0C);

        gTxSlot = 0;
        gPresent = true;
        return true;
    }

    bool present() { return gPresent; }

    const uint8_t* mac() { return gMAC; }
    uint32_t ioBase() { return gIO; }

    void sendFrame(const uint8_t* data, int len) {
        if (!gPresent || len <= 0 || len > 1536) return;

        // 拷贝到发送缓冲 (线性透传, 无分页故物理地址即线性地址)
        for (int i = 0; i < len; i++) gTxBuf[i] = data[i];
        if (len < 60) {                       // 以太网最小帧 60 字节(不含FCS)
            for (int i = len; i < 60; i++) gTxBuf[i] = 0;
            len = 60;
        }

        uint32_t tsad = REG_TSAD0 + gTxSlot * 4;
        uint32_t tsd  = REG_TSD0  + gTxSlot * 4;
        outl(tsad, (uint32_t)(uintptr_t)gTxBuf);   // 发送缓冲物理地址
        // QEMU/硬件语义: TSD 写值不能带 OWN(bit13). 写 TSD 触发发送;
        // 发送完成时硬件回置 OWN=1 (host 重新拥有描述符)
        outl(tsd, (uint32_t)len);

        bool txDone = false;
        for (int t = 0; t < 10000; t++) {
            if (inl(tsd) & (1u << 13)) { txDone = true; break; }   // OWN 置1 -> 完成
        }
        if (gDbgTx) {
            gDbgTx = false;
            static const char* hx = "0123456789ABCDEF";
            char hb[9]; uint32_t v;
            VGA::write("TX# txbuf=");
            v = (uint32_t)(uintptr_t)gTxBuf;
            for (int i = 7; i >= 0; i--) hb[7 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[8] = 0; VGA::write(hb);
            VGA::write(" tsd=");
            v = inl(tsd) & 0x00FFFFu;
            for (int i = 5; i >= 0; i--) hb[5 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[6] = 0; VGA::write(hb);
            if (txDone) VGA::writeLine(" done"); else VGA::writeLine(" TIMEOUT");
        }
        if (txDone) gTxSlot = (gTxSlot + 1) & 3;   // 与 QEMU currTxDesc 同步推进
    }

    int pollFrame(uint8_t* out, int maxlen) {
        if (!gPresent) return 0;

        uint16_t cbr = inw(REG_CBR);              // 硬件写指针
        if (cbr != (uint16_t)gCapr && gDbgRx > 0) {
            static const char* hx = "0123456789ABCDEF";
            char hb[5];
            uint16_t v;
            uint16_t hd[2];
            uint8_t pk[12];
            ringRead(gCapr, (uint8_t*)hd, 4);
            ringRead((gCapr + 4) % RX_BUF_LEN, pk, 12);
            VGA::write("RX#");
            {
                char nbuf[8];
                int nn = gDbgRx;
                if (nn >= 10) nbuf[0] = (char)('0' + nn / 10); else nbuf[0] = ' ';
                nbuf[1] = (char)('0' + nn % 10);
                nbuf[2] = 0;
                VGA::write(nbuf);
            }
            VGA::write(" cbr=");
            v = cbr;
            for (int i = 3; i >= 0; i--) hb[3 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[4] = 0; VGA::write(hb);
            VGA::write(" capr=");
            v = (uint16_t)gCapr;
            for (int i = 3; i >= 0; i--) hb[3 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[4] = 0; VGA::write(hb);
            VGA::write(" h0=");
            v = hd[0];
            for (int i = 3; i >= 0; i--) hb[3 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[4] = 0; VGA::write(hb);
            VGA::write(" h1=");
            v = hd[1];
            for (int i = 3; i >= 0; i--) hb[3 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[4] = 0; VGA::write(hb);
            VGA::write(" len=");
            v = hd[1] - 4;
            for (int i = 3; i >= 0; i--) hb[3 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[4] = 0; VGA::write(hb);
            VGA::write(" data=");
            for (int i = 0; i < 12; i++) {
                hb[0] = hx[pk[i] >> 4];
                hb[1] = hx[pk[i] & 0xF];
                hb[2] = 0;
                VGA::write(hb);
            }
            VGA::writeLine("");
            gDbgRx--;
        }
        if (cbr == (uint16_t)gCapr) return 0;     // 无新数据

        uint16_t header[2];
        ringRead(gCapr, (uint8_t*)header, 4);
        // QEMU/硬件 ring mode 头: 32 位小端。
        //   低 16 位 = 状态 (RxStatusOK 等)
        //   高 16 位 = 帧长度 + 4
        // 每条目布局: [4B 头][帧数据 size][4B CRC], 整体按 4 字节对齐
        uint32_t hdr = (uint32_t)header[0] | ((uint32_t)header[1] << 16);
        int pktLen = (int)((hdr >> 16) - 4);
        if (pktLen <= 0 || pktLen > 6000) {
            // 坏描述符: 直接跳到硬件写指针
            gCapr = cbr;
            outw(REG_CAPR, (uint16_t)((gCapr + RX_BUF_LEN - 16) % RX_BUF_LEN));
            return 0;
        }

        // 拷贝整个报文 (去掉 4B 头; 4B CRC 忽略)
        uint32_t payloadOff = (gCapr + 4) % RX_BUF_LEN;
        if (pktLen > maxlen) pktLen = maxlen;
        ringRead(payloadOff, out, pktLen);

        // 更新消费指针: 跳过 (4B头 + 帧 + 4B CRC), 并对齐到 4 字节。
        // 注意: QEMU 的 rtl8139 对 CAPR 写入有 +16 的偏移处理(off-by-16),
        // 因此写入时需回减 16, 才能让 QEMU 内部 RxBufPtr 等于真实消费位置,
        // 否则 can_receive 判定可用空间恒小于 1514, 后续帧全部被拒收。
        uint32_t consumed = (4 + ((int)((hdr >> 16) - 4)) + 4 + 3) & ~3u;
        gCapr = (gCapr + consumed) % RX_BUF_LEN;
        outw(REG_CAPR, (uint16_t)((gCapr + RX_BUF_LEN - 16) % RX_BUF_LEN));
        return pktLen;
    }
}
