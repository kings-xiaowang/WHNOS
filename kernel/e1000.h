/* e1000.h — Intel PRO/1000 (e1000/82540EM 系) 网卡驱动
 * 适用: VMware E1000 虚拟网卡 (vendor 0x8086), 也是 QEMU e1000 模型同款。
 * 访问方式: BAR0 MMIO, 轮询收发 (不用中断)。legacy 16 字节描述符环。
 */
#ifndef E1000_H
#define E1000_H

#include <stdint.h>

namespace E1000 {

    bool init();                        // 探测并初始化网卡
    bool present();

    // 发送一帧 (data 为完整以太网帧, len 为其长度)
    void sendFrame(const uint8_t* data, int len);

    // 尝试接收一帧; 成功拷贝到 out 并返回长度, 无包返回 0
    int  pollFrame(uint8_t* out, int maxlen);

    const uint8_t* mac();               // 指向 6 字节 MAC
    const char*    name();              // 网卡名称 (调试显示)
}

#endif
