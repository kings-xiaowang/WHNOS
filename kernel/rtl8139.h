/* rtl8139.h — Realtek RTL8139 网卡驱动 (IO 端口访问, 轮询模式) */
#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

namespace RTL8139 {

    bool init();                        // 探测并初始化网卡
    bool present();

    // 发送一帧 (data 为完整以太网帧, len 为其长度)
    void sendFrame(const uint8_t* data, int len);

    // 尝试接收一帧; 成功拷贝到 out 并返回长度, 无包返回 0
    int  pollFrame(uint8_t* out, int maxlen);

    const uint8_t* mac();               // 指向 6 字节 MAC
    uint32_t ioBase();
}

#endif
