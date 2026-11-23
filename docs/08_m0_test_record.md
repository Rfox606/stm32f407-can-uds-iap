# M0主机端测试记录

日期：2026-11-23。

## 执行环境与命令
\`\`\`bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/iap_core_test
\`\`\`

在主机C环境执行可移植更新算法测试：CTest 1/1通过，测试程序内部9/9场景通过。测试内容参见docs/04_test_matrix.md及tests/test_boot_core.c。

## 本阶段未验证
没有STM32F407的可链接完整固件工程；没有真实Flash驱动、bxCAN和UDS实际接入；没有实物断电、实际APP跳转或HardFault证据。

## 已知限制
- 固件数据必须连续写入，单块最多256字节；除最后一块外要求4字节对齐。
- 尚无UDS blockSequenceCounter映射、重复块处理与重试。
- 启动时重新计算整个固件的CRC，目标芯片耗时未知。
- 单APP槽升级中断后只能进入恢复刷写模式，不能自动回滚。
