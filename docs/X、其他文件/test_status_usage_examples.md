# 测试状态说明功能使用示例

## 概述

v1.5版本增强了测试状态系统，现在不仅支持7种状态枚举，还支持详细的测试失败说明，能够准确记录失败时的测试步骤和具体原因。

## 新增字段

在`struct Test_quanju_canshu`中新增了以下字段：

```c
struct Test_quanju_canshu {
  uint16_t time_softdelay_ms;       // 软件延时计数器(ms)
  uint32_t time_aroundtest_ms;      // 全局测试超时计数器(ms)
  uint16_t danbu_chaoshishijian_ms; // 单步测试超时计数器(ms)
  enum Test_Status test_status;     // 测试状态，替代原来的test_over
  enum Test_liucheng failed_step;   // 记录失败时的测试步骤
  char status_description[64];      // 测试状态详细说明
};
```

## 新增便捷函数

### 1. 设置状态和说明

```c
void set_test_status_with_description(enum Test_Status status, enum Test_liucheng step, const char* description);
```

**使用示例：**
```c
// 当检测到硬件错误时
if (hardware_check_failed) {
    set_test_status_with_description(TEST_ERROR_HARDWARE, w_gonghao_CHK, "主电压检测失败，电压值超出范围");
    Test_liucheng_L = w_end;
}

// 当协议通信失败时
if (protocol_timeout) {
    set_test_status_with_description(TEST_ERROR_PROTOCOL, get_biaohao, "获取表号超时，设备无响应");
    Test_liucheng_L = w_end;
}
```

### 2. 获取状态说明

```c
const char* get_test_status_description(void);
```

**使用示例：**
```c
// 在调试或上位机查询时
if (Test_quanju_canshu_L.test_status != TEST_SUCCESS) {
    DeBug_print("测试失败原因: %s\r\n", get_test_status_description());
}
```

### 3. 获取测试步骤名称

```c
const char* get_test_step_name(enum Test_liucheng step);
```

**使用示例：**
```c
// 记录当前测试步骤
DeBug_print("当前执行步骤: %s\r\n", get_test_step_name(Test_liucheng_L));

// 记录失败步骤
if (Test_quanju_canshu_L.test_status != TEST_SUCCESS) {
    DeBug_print("失败步骤: %s\r\n", get_test_step_name(Test_quanju_canshu_L.failed_step));
}
```

## 实际应用场景

### 1. 超时处理增强

在`test_err_end_Func()`函数中，现在会自动记录超时发生的具体步骤：

```c
void test_err_end_Func() {
    if (Test_quanju_canshu_L.test_status == TEST_RUNNING) {
        if (Test_quanju_canshu_L.time_aroundtest_ms == 0) {
            // 全局测试超时 - 会自动生成详细说明
            char description[64];
            sprintf(description, "全局测试超时，当前步骤：%s", get_test_step_name(Test_liucheng_L));
            set_test_status_with_description(TEST_TIMEOUT_GLOBAL, Test_liucheng_L, description);
            Test_liucheng_L = w_end;
        }
    }
}
```

### 2. 硬件检测失败记录

```c
// 在功耗检测步骤中
case w_gonghao_CHK:
    if (voltage_out_of_range) {
        char desc[64];
        sprintf(desc, "功耗检测失败，电压值: %.2fV (期望: 3.3V±0.1V)", measured_voltage);
        set_test_status_with_description(TEST_ERROR_HARDWARE, w_gonghao_CHK, desc);
        Test_liucheng_L = w_end;
    }
    break;
```

### 3. 协议通信失败记录

```c
// 在获取表号步骤中
case get_biaohao:
    if (communication_failed) {
        char desc[64];
        sprintf(desc, "获取表号失败，重试%d次后仍无响应", retry_count);
        set_test_status_with_description(TEST_ERROR_PROTOCOL, get_biaohao, desc);
        Test_liucheng_L = w_end;
    }
    break;
```

### 4. 阀门测试失败记录

```c
// 在阀门测试步骤中
case w_famenceshi:
    if (valve_test_failed) {
        const char* valve_type = (Test_jiejuo_jilu.biaoju_leixing_famen == 0) ? "机械表" : "超声波表";
        char desc[64];
        sprintf(desc, "%s阀门动作检测失败，未检测到预期的动作信号", valve_type);
        set_test_status_with_description(TEST_ERROR_HARDWARE, w_famenceshi, desc);
        Test_liucheng_L = w_end;
    }
    break;
```

## 调试信息输出

在`w_end`步骤中，系统会根据状态自动输出详细的调试信息：

```c
case w_end:
    switch (Test_quanju_canshu_L.test_status) {
    case TEST_TIMEOUT_STEP:
        DeBug_print("测试因单步超时而结束\r\n");
        DeBug_print("详细信息: %s\r\n", get_test_status_description());
        break;
    case TEST_ERROR_HARDWARE:
        DeBug_print("测试因硬件错误而结束\r\n");
        DeBug_print("详细信息: %s\r\n", get_test_status_description());
        break;
    // ... 其他状态
    }
    break;
```

## 上位机接口

上位机可以通过查询以下信息获得详细的测试结果：

1. `Test_quanju_canshu_L.test_status` - 测试状态枚举
2. `Test_quanju_canshu_L.failed_step` - 失败步骤
3. `Test_quanju_canshu_L.status_description` - 详细说明文本

这样可以精确知道测试在哪一步失败，失败的具体原因是什么，大大提高了问题定位和调试效率。

## 总结

新的测试状态说明功能解决了原有"test_over=0/1"设计的问题：

1. **状态详细化**：从2种状态扩展到7种状态
2. **失败定位**：准确记录失败发生的测试步骤
3. **原因说明**：提供64字节的详细失败原因描述
4. **调试友好**：自动生成调试信息，提高问题定位效率
5. **向后兼容**：保持原有接口的基本功能

这一改进使得测试系统更加完善，便于维护和故障排除。
