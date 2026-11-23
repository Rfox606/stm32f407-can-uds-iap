# STM32F407底层移植（待实现）

目标芯片：STM32F407VGT6，1 MB Flash，Cortex-M4，bxCAN。

M1至少新增：
1. 可独立编译和链接的Bootloader工程及APP工程，含CMSIS启动、STM32驱动。
2. Flash适配函数，严格识别扇区与保护Bootloader所在扇区0～3。
3. APP跳转函数：清理中断与SysTick、必要外设，设置VTOR和MSP，跳转至复位入口。
4. bxCAN初始化、过滤、接收缓冲及错误路径。
5. 交叉编译证据、链接映射与可执行文件体积检查。

app_memory.ld.inc和boot_memory.ld.inc仅是内存区域片段，**不是完整链接脚本**。STM32F407 Flash编程粒度和F103不同，不可直接照抄半字编程代码。
