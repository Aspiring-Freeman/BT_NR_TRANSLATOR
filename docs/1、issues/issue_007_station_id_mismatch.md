# Issue #007: 工位号匹配问题

## 问题描述

**发现日期**: 2026-01-07

**现象**:
- 发送 `68 AA 19 03 ...` (工位3的开始测试命令) 给工位0的设备
- 设备响应 `68 AB 06 03 1C 16` (工位号变成了3)
- 预期应该是设备不响应（因为工位不匹配），或者响应自己的工位号0

**影响**:
- 4个工位UART并联时，所有设备都会响应任意工位的命令
- 设备的工位号被命令覆盖，导致响应工位号错误

## 根本原因

`handle_start_test()` 函数中：
1. 没有检查命令中的工位号是否与本机工位匹配
2. 直接用命令中的工位号覆盖了设备的工位号 (`Test_jiejuo_jilu.gongwei = frame->station_id;`)

设备的工位号应该由**跳线帽检测**决定（在 `gongwei_jiance()` 中初始化），不应该被命令覆盖。

## 解决方案

在 `handle_start_test()` 中添加工位匹配检查：

```c
// 工位匹配检查：只响应与本机工位匹配的命令
if (frame->station_id != Test_jiejuo_jilu.gongwei) {
    log_d("工位不匹配: 命令工位=%d, 本机工位=%d", frame->station_id, Test_jiejuo_jilu.gongwei);
    return; // 不处理不匹配的命令
}
```

同时移除覆盖工位号的代码：
```c
// 删除: Test_jiejuo_jilu.gongwei = frame->station_id;
```

## 修改文件

- `Components/Protocol/PC/pc_protocol_mes.c`
  - `handle_start_test()`: 添加工位匹配检查，移除工位号覆盖

## 验证方法

1. 设备通过跳线帽设置为工位0
2. 发送工位0的命令 → 应该收到响应
3. 发送工位1/2/3的命令 → 应该**没有响应**（设备静默忽略）

## 状态

**已修复** - v2.7.2
