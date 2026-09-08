/* e1000.cpp — Intel PRO/1000 (e1000/82540EM 系) 网卡驱动
 *
 * 资料参考:
 *   - OSDev Wiki: Intel Ethernet / e1000 教程
 *   - Linux 内核 e1000 驱动 (e1000_main.c / e1000_hw.h)
 *
 * 特性:
 *   - MMIO (BAR0) 访问, 不使用 IO 端口
 *   - 轮询收发, 不使用中断 (PIC 中断线不接)
 *   - legacy 16 字节描述符, 32 描述符环形队列
 *   - 支持 82540EM / 82545EM / 82573L / 82574L 等常见 ID
 *     (VMware E1000 = 0x100E, QEMU e1000 同)
 */
#include "e1000.h"
#include "pci.h"
#include "vga.h"
#include "serial.h"

#include <stddef.h>

// ---- PCI 常量 ----
#define PCI_VENDOR_INTEL 0x8086

// ---- 寄存器偏移 (MMIO, 相对 BAR0) ----
// 通用
#define REG_CTRL    0x0000
#define REG_STATUS  0x0008
#define REG_EERD    0x0014
#define REG_ICR     0x00C0
#define REG_ICS     0x00C8
#define REG_IMS     0x00D0
#define REG_IMR     0x00D4
#define REG_RCTL    0x0100
#define REG_TCTL    0x0400
#define REG_TIPG    0x0410
// 接收
#define REG_RDBAL   0x2800
#define REG_RDBAH   0x2804
#define REG_RDLEN   0x2808
#define REG_RDH     0x2810
#define REG_RDT     0x2818
#define REG_RXDCTL  0x2828
// 发送
#define REG_TDBAL   0x3800
#define REG_TDBAH   0x3804
#define REG_TDLEN   0x3808
#define REG_TDH     0x3810
#define REG_TDT     0x3818
#define REG_TXDCTL  0x3828
// MAC 滤波 (RAL/RAH 只读回存用于自检, 不主动写)
#define REG_RAL0    0x5400
#define REG_RAH0    0x5404

// CTRL
#define CTRL_RST    (1u << 26)
#define CTRL_ASDE   (1u << 5)
#define CTRL_SLU    (1u << 6)     // Set Link Up (仅复位/up后)

// RCTL (对齐 Intel 规范 / QEMU: EN=bit1 SBP=bit2 UPE=bit3 MPE=bit4 LPE=bit5)
#define RCTL_EN     (1u << 1)
#define RCTL_SBP    (1u << 2)
#define RCTL_UPE    (1u << 3)
#define RCTL_MPE    (1u << 4)
#define RCTL_LPE    (1u << 5)
#define RCTL_BAM    (1u << 15)
#define RCTL_SECRC  (1u << 26)

// TCTL
// 注意: EN 是 bit1(0x2); bit0 是 software reset(RST)。
// 之前误写 bit0, 导致 start_xmit 的 EN 检查恒不通过、TDH 永不推进。
#define TCTL_EN     (1u << 1)
#define TCTL_PSP    (1u << 3)
#define TCTL_CT_SHIFT 4
#define TCTL_COLD_SHIFT 12

// 描述符个数
#define QUEUE_SIZE  32

namespace E1000 {

    // ---- MMIO 访问 ----
    static volatile uint32_t* gReg = 0;   // (uint32_t*)BAR0

    static inline uint32_t reg(uint32_t off) { return gReg[off >> 2]; }
    static inline void     regw(uint32_t off, uint32_t v) { gReg[off >> 2] = v; }

    // ---- 状态 ----
    static bool     gPresent = false;
    static uint8_t  gMAC[6];
    static uint32_t gDmaBase = 0;         // 用于调试

    // ---- 描述符 ----
    // legacy 16 字节描述符 (little endian)
    struct Desc {
        uint32_t addr;      // 0-3  : 缓冲物理地址(低32位)
        uint32_t addr_hi;   // 4-7  : 高32位 (legacy 常 0)
        uint16_t len;       // 8-9  : RX=硬件写回的帧长; TX=待发帧长
        uint8_t  cso;       // 10   : TX: checksum offset
        uint8_t  cmd;       // 11   : TX: EOP/IFCS/RS
        uint8_t  status;    // 12   : DD(bit0); RX 另有 EOP(bit1)
        uint8_t  css;       // 13   : TX: checksum start
        uint16_t special;   // 14-15: TX: 保留 / RX: vlan tag
    } __attribute__((packed));

    // 描述符标志
    enum : uint8_t {
        DESC_DD  = 0x01,   // Descriptor Done
        DESC_EOP = 0x02,   // End Of Packet
    };
    // TX cmd
    enum : uint8_t {
        TXCMD_EOP  = 0x01,
        TXCMD_IFCS = 0x02,
        TXCMD_RS   = 0x08,
    };

    // DMA 缓冲区 (静态数组, 因无 MMU 线性地址=物理地址)
    static Desc    gTxDesc[QUEUE_SIZE] __attribute__((aligned(16)));
    static Desc    gRxDesc[QUEUE_SIZE] __attribute__((aligned(16)));
    static uint8_t gTxBuf[QUEUE_SIZE][1536] __attribute__((aligned(16)));
    static uint8_t gRxBuf[QUEUE_SIZE][1536] __attribute__((aligned(16)));

    static int gTxIdx = 0;    // 下一次发送槽
    static int gRxIdx = 0;    // 下一次接收槽

    static bool gDbgTx = true;
    static int  gDbgRx = 4;

    // ---- PCI 探测 ----
    // 常见的 e1000 设备 ID 列表 (含 QEMU/VMware 常用的 0x100E)
    static const uint16_t kDevIds[] = {
        0x100E,  // 82540EM (QEMU e1000, VMware E1000)
        0x100F,  // 82545EM
        0x1018,  // 82547EI
        0x1019,  // 82547EI (mobile)
        0x105E,  // 82571EB
        0x1076,  // 82541GI
        0x107C,  // 82541GI (mobile)
        0x10B9,  // 82572EI
        0x10BA,  // 82572EI (mobile)
        0x10C9,  // 82576
        0x10D3,  // 82574L
        0x10DE,  // 82567LF-2
        0x10E8,  // 82576
        0x1523,  // I350
    };

    static uint16_t probe() {
        for (size_t i = 0; i < sizeof(kDevIds) / sizeof(kDevIds[0]); i++) {
            uint16_t bdf = PCI::findDevice(PCI_VENDOR_INTEL, kDevIds[i]);
            if (bdf != 0xFFFF) return bdf;
        }
        return 0xFFFF;
    }

    // ---- EEPROM 读取 (用于回退取 MAC) ----
    static uint16_t eepromRead(uint8_t addr) {
        regw(REG_EERD, (1u << 0) | ((uint32_t)addr << 8));
        for (int t = 0; t < 10000; t++) {
            uint32_t v = reg(REG_EERD);
            if (v & (1u << 4)) {           // EERD.DONE
                return (uint16_t)(v >> 16);
            }
        }
        return 0xFFFF;
    }

    // ---- 软件复位 ----
    static void reset() {
        regw(REG_CTRL, reg(REG_CTRL) | CTRL_RST);
        for (int t = 0; t < 100000; t++) {
            if (!(reg(REG_CTRL) & CTRL_RST)) break;
        }
    }

    // ---- 读取 MAC ----
    // 优先直接回读 RAL0/RAH0 (复位后硬件自动从 EEPROM 加载);
    // 若为 0 则回退到 EERD。
    static void readMAC() {
        // 先尝试 RAL/RAH
        uint32_t ral = reg(REG_RAL0);
        uint32_t rah = reg(REG_RAH0);
        int nonzero = ral != 0 || (rah & 0xFFFF) != 0;
        if (nonzero) {
            gMAC[0] = (uint8_t)(ral >> 0);
            gMAC[1] = (uint8_t)(ral >> 8);
            gMAC[2] = (uint8_t)(ral >> 16);
            gMAC[3] = (uint8_t)(ral >> 24);
            gMAC[4] = (uint8_t)(rah >> 0);
            gMAC[5] = (uint8_t)(rah >> 8);
        } else {
            uint16_t w0 = eepromRead(0);
            uint16_t w1 = eepromRead(1);
            gMAC[0] = (uint8_t)(w0 >> 0);
            gMAC[1] = (uint8_t)(w0 >> 8);
            gMAC[2] = (uint8_t)(w1 >> 0);
            gMAC[3] = (uint8_t)(w1 >> 8);
            gMAC[4] = (uint8_t)(eepromRead(2) >> 0);
            gMAC[5] = (uint8_t)(eepromRead(2) >> 8);
        }
    }

    // ---- 初始化 ----
    bool init() {
        uint16_t bdf = probe();
        if (bdf == 0xFFFF) return false;

        uint8_t dev = (uint8_t)(bdf >> 8);

        // 找到的 PCI 设备信息保存
        uint16_t vid = (uint16_t)(PCI::readConfig(0, dev, 0, 0) & 0xFFFF);
        (void)vid;
        uint16_t did = (uint16_t)(PCI::readConfig(0, dev, 0, 0) >> 16);
        (void)did;

        // 开启总线主控 + 内存空间 (PCI CMD 位 1=mem, 2=bus master)
        uint32_t cmd = PCI::readConfig(0, dev, 0, 0x04);
        PCI::writeConfig(0, dev, 0, 0x04, cmd | 0x06);

        // BAR0 = MMIO 基址
        uint32_t bar0 = PCI::readConfig(0, dev, 0, 0x10);
        if ((bar0 & 0xFFFFFFFF) == 0) return false;
        gReg = (volatile uint32_t*)(uintptr_t)(bar0 & ~0xF);

        // 复位
        reset();

        // 诊断: 复位后读回 PCI COMMAND, 确认 bus master/内存空间位是否保留
        {
            static const char* hx = "0123456789ABCDEF";
            uint32_t c = PCI::readConfig(0, dev, 0, 0x04);
            VGA::write("E1K CFGCMD=");
            char hb[9];
            for (int i = 7; i >= 0; i--) hb[7 - i] = hx[(c >> (i * 4)) & 0xF];
            hb[8] = 0; VGA::writeLine(hb);
        }

        // 读取 MAC
        readMAC();

        // ---- 接收环 ----
        for (int i = 0; i < QUEUE_SIZE; i++) {
            gRxDesc[i].addr     = (uint32_t)(uintptr_t)gRxBuf[i];
            gRxDesc[i].addr_hi  = 0;
            gRxDesc[i].len   = 0;
            gRxDesc[i].status   = 0;
            gRxDesc[i].css      = 0;
            gRxDesc[i].special  = 0;
        }
        regw(REG_RDBAL, (uint32_t)(uintptr_t)gRxDesc);
        regw(REG_RDBAH, 0);
        regw(REG_RDLEN, (uint32_t)(QUEUE_SIZE * sizeof(Desc)));
        regw(REG_RDH, 0);
        // RDT 初始化为 QUEUE_SIZE-1: RDH(硬件写指针)==RDT 会被 QEMU 判定环满,
        // 必须留出可用描述符, 网卡才会接收帧
        regw(REG_RDT, QUEUE_SIZE - 1);
        gRxIdx = 0;

        // ---- 发送环 ----
        for (int i = 0; i < QUEUE_SIZE; i++) {
            gTxDesc[i].addr     = (uint32_t)(uintptr_t)gTxBuf[i];
            gTxDesc[i].addr_hi  = 0;
            gTxDesc[i].len   = 0;
            gTxDesc[i].cmd   = 0;
            gTxDesc[i].status   = DESC_DD;   // 初始标记为空闲
            gTxDesc[i].css      = 0;
            gTxDesc[i].special  = 0;
        }
        regw(REG_TDBAL, (uint32_t)(uintptr_t)gTxDesc);
        regw(REG_TDBAH, 0);
        regw(REG_TDLEN, (uint32_t)(QUEUE_SIZE * sizeof(Desc)));
        regw(REG_TDH, 0);
        regw(REG_TDT, 0);
        gTxIdx = 0;

        // ---- 描述符阈值 (关键: 新版 QEMU e1000 发送必须配 WTHRESH,
        //      否则硬件不消费发送描述符, TDH 不推进) ----
        // TXDCTL: WTHRESH=1 (回写阈值), GRAN=0
        regw(REG_TXDCTL, 0x01000000);
        // RXDCTL: WTHRESH=1
        regw(REG_RXDCTL, 0x01000000);

        // ---- 接收控制 ----
        // EN + SBP + UPE + MPE + LPE + BAM + SECRC
        regw(REG_RCTL, RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE |
                       RCTL_LPE | RCTL_BAM | RCTL_SECRC);

        // ---- 发送控制 ----
        // EN + PSP + CT=0x10 + COLD=0x40
        regw(REG_TCTL, TCTL_EN | TCTL_PSP |
                       (0x10u << TCTL_CT_SHIFT) |
                       (0x40u << TCTL_COLD_SHIFT));

        // 包间隔 (典型默认值)
        regw(REG_TIPG, 0x0060200A);

        // 开链路 (设 SLU)。注: 链接层流量控制打开, 便于 VMware 稳定起速
        regw(REG_CTRL, reg(REG_CTRL) | CTRL_SLU | CTRL_ASDE);

        // 等待链路 up (STATUS.LU, bit1)。QEMU/VMware 复位后需短暂时间
        for (int t = 0; t < 3000000; t++) {
            if (reg(REG_STATUS) & (1u << 1)) break;
        }

        gDmaBase = (uint32_t)(uintptr_t)gTxDesc;
        gPresent = true;
        return true;
    }

    bool present() { return gPresent; }

    const uint8_t* mac() { return gMAC; }
    const char*    name() { return "Intel Pro/1000 (MMIO)"; }

    // ---- 发送 ----
    void sendFrame(const uint8_t* data, int len) {
        if (!gPresent || len <= 0 || len > 1536) return;

        int idx = gTxIdx;

        // 等待该槽空闲 (上一次发送完成 DD 置位)
        for (int t = 0; t < 100000 && !(gTxDesc[idx].status & DESC_DD); t++) {}

        // 拷贝数据
        for (int i = 0; i < len; i++) gTxBuf[idx][i] = data[i];
        if (len < 60) {
            for (int i = len; i < 60; i++) gTxBuf[idx][i] = 0;
            len = 60;                        // 以太网最小帧 60 字节
        }

        // 写入描述符并提交
        gTxDesc[idx].addr    = (uint32_t)(uintptr_t)gTxBuf[idx];
        gTxDesc[idx].addr_hi = 0;
        gTxDesc[idx].len  = (uint16_t)len;
        gTxDesc[idx].cmd  = TXCMD_EOP | TXCMD_IFCS | TXCMD_RS;
        gTxDesc[idx].status  = 0;

        // 内存屏障 (x86 无重排需要但保持可读性)
        __asm__ __volatile__("" ::: "memory");

        // 推进尾指针 TDT, 更新软件索引
        regw(REG_TDT, (uint32_t)((idx + 1) % QUEUE_SIZE));
        gTxIdx = (idx + 1) % QUEUE_SIZE;

        // 等待发送完成 (轮询 DD)
        for (int t = 0; t < 200000; t++) {
            if (*(volatile uint8_t*)&gTxDesc[idx].status & DESC_DD) break;
        }

        // 保险: 超时则重写 TCTL 强制触发硬件 start_xmit (兼容性兜底)
        if (!(gTxDesc[idx].status & DESC_DD)) {
            regw(REG_TCTL, reg(REG_TCTL));
            for (int t = 0; t < 200000; t++) {
                if (*(volatile uint8_t*)&gTxDesc[idx].status & DESC_DD) break;
            }
        }

        if (gDbgTx) {
            gDbgTx = false;
            static const char* hx = "0123456789ABCDEF";
            char hb[12];
            VGA::write("E1K TX# tdt=");
            uint32_t v = reg(REG_TDT);
            for (int i = 7; i >= 0; i--) hb[7 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[8] = 0; VGA::write(hb);
            VGA::writeLine(gTxDesc[idx].status & DESC_DD ? " done" : " TIMEOUT");
            Serial::write("E1K TX# tdt="); Serial::write(hb);
            Serial::writeln(gTxDesc[idx].status & DESC_DD ? " done" : " TIMEOUT");
        }
    }

    // ---- 接收 ----
    int pollFrame(uint8_t* out, int maxlen) {
        if (!gPresent) return 0;

        // 一次性诊断: 打印 RX 相关寄存器
        static bool diagOnce = false;
        if (!diagOnce) {
            diagOnce = true;
            static const char* hx = "0123456789ABCDEF";
            char hb[12];
            Serial::write("E1K RX rctl=");
            uint32_t v = reg(REG_RCTL);
            for (int i = 7; i >= 0; i--) hb[7 - i] = hx[(v >> (i * 4)) & 0xF];
            hb[8] = 0; Serial::write(hb);
            Serial::write(" rdh=");
            v = reg(REG_RDH); for (int i = 7; i >= 0; i--) hb[7-i] = hx[(v>>(i*4))&0xF];
            Serial::write(hb);
            Serial::write(" rdt=");
            v = reg(REG_RDT); for (int i = 7; i >= 0; i--) hb[7-i] = hx[(v>>(i*4))&0xF];
            Serial::write(hb);
            Serial::write(" stat=");
            v = reg(REG_STATUS); for (int i = 7; i >= 0; i--) hb[7-i] = hx[(v>>(i*4))&0xF];
            Serial::write(hb);
            Serial::write(" d0=");
            v = gRxDesc[0].status; for (int i = 7; i >= 0; i--) hb[7-i] = hx[(v>>(i*4))&0xF];
            Serial::write(hb);
            Serial::write(" icr=");
            v = reg(REG_ICR); for (int i = 7; i >= 0; i--) hb[7-i] = hx[(v>>(i*4))&0xF];
            Serial::writeln(hb);
        }

        int idx = gRxIdx;
        // 描述符 DD 未置位 -> 无新包
        if (!(gRxDesc[idx].status & DESC_DD)) return 0;

        if (gDbgRx > 0) {
            gDbgRx--;
            static const char* hx = "0123456789ABCDEF";
            char hb[12];
            VGA::write("E1K RX# idx=");
            int v = idx;
            hb[0] = hx[(v >> 4) & 0xF]; hb[1] = hx[v & 0xF]; hb[2] = 0;
            VGA::write(hb);
            VGA::write(" len=");
            v = gRxDesc[idx].len;
            hb[0] = hx[(v >> 12) & 0xF]; hb[1] = hx[(v >> 8) & 0xF];
            hb[2] = hx[(v >> 4) & 0xF]; hb[3] = hx[v & 0xF]; hb[4] = 0;
            VGA::write(hb);
            VGA::write(" data=");
            for (int i = 0; i < 8; i++) {
                hb[0] = hx[gRxBuf[idx][i] >> 4];
                hb[1] = hx[gRxBuf[idx][i] & 0xF];
                hb[2] = 0;
                VGA::write(hb);
            }
            VGA::writeLine("");
        }

        // 只处理完整包 (EOP)
        if (gRxDesc[idx].status & DESC_EOP) {
            int len = gRxDesc[idx].len;
            if (len > maxlen) len = maxlen;
            for (int i = 0; i < len; i++) out[i] = gRxBuf[idx][i];

            // 清状态, 归还描述符
            gRxDesc[idx].status = 0;
            gRxDesc[idx].len = 0;
            regw(REG_RDT, (uint32_t)((idx + QUEUE_SIZE - 1) % QUEUE_SIZE));
            gRxIdx = (idx + 1) % QUEUE_SIZE;
            return len;
        }

        // 非 EOP (少见), 跳过
        gRxDesc[idx].status = 0;
        regw(REG_RDT, (uint32_t)((idx + QUEUE_SIZE - 1) % QUEUE_SIZE));
        gRxIdx = (idx + 1) % QUEUE_SIZE;
        return 0;
    }
}
