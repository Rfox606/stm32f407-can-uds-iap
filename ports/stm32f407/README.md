# STM32F407VGT6 底层工程（M1）

当前提供独立Bootloader和APP源码、Cortex-M4启动文件、完整链接脚本、Flash擦写驱动、向量表检查与初始元数据生成。

目标芯片：STM32F407VGT6（1 MB Flash、128 KB常规SRAM和64 KB CCM RAM，当前未使用CCM）。演示开发板假定为STM32F407G-DISC1：PD12和PD13为LED，PA0为用户按键。

构建：运行 bash tools/build_firmware.sh （ARM GCC及其objcopy/objdump）。Clang交叉编译可设置 ARM_CC=clang、ARM_OBJCOPY=llvm-objcopy、ARM_OBJDUMP=llvm-objdump。

上电：如果PA0没有按下、APP元数据和CRC有效，则跳转APP；否则闪烁PD13并留在恢复模式。当前恢复模式尚未提供CAN下载，M2再接入UDS服务。

初次使用需将boot.bin、app_factory_meta.bin、app.bin分别写入0x08000000、0x08010000和0x08020000，详见M1验证文档。

Flash使用x32编程，假设板上目标电压为3.3V（2.7～3.6V条件）。未实物验证，切勿把编译通过等同于上板运行通过。
