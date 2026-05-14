# ADC 分压比核对清单

## 背景

`ADC_CHK.c` 重构后,每个通道的分压反算系数 `divider_ratio` 从写死的 `*11`
改为表中独立配置。但**所有通道当前仍沿用 11**,需要照原理图实测核对每一项。

旧版 `voltage = voltage * 11` 这段代码隐含的假设是:**所有 ADC 输入点的上下分压电阻都是 10:1**。这个假设几乎肯定不对 —— 不同电压量级的被测点用同样的分压比是不合理的(3V 信号也按 11 倍反算 → 算出来 33V)。

## 核对方法

对每个通道:

1. 在原理图找到该 ADC 输入引脚(如 `PA14 → IN_ADC_MAIN`)
2. 顺着信号往源点(被测点)走,看分压电阻 `R_top : R_bot`
3. 分压比 = `(R_top + R_bot) / R_bot`
4. 该值就是 `divider_ratio` 的正确值
5. 没有分压电阻 / 直连 ADC 输入: `divider_ratio = 1`
6. 修改 `Src/ADC_CHK.c` 中 `adc_chan_table[]` 对应行的 `divider_ratio`
7. 用万用表实测被测点电压,调用 `adc_read_mv(ADC_CH_xxx)` 验证 ±5% 内吻合

## 待核对清单

| #  | 通道枚举 | 引脚 | ADC | 网络名 / 被测点 | 期望电压 | R_top : R_bot | 应填 ratio | 当前 ratio | 是否需改 |
|----|----------|------|-----|------------------|----------|----------------|------------|-------------|-----------|
| 1  | `ADC_CH_MAIN_POWER`     | PA14 | IN11 | IN_ADC_MAIN / dc-dc 主电            | 6.5V (碱) / 3.6V (锂)   | ___ : ___ | ___ | 11 | ☐ |
| 2  | `ADC_CH_BACKUP`         | PE9  | IN19 | IN_ADC_BKP / dc-dc 备电模组         | 3.3 ~ 3.5V              | ___ : ___ | ___ | 11 | ☐ |
| 3  | `ADC_CH_FCT_5V`         | PA13 | IN4  | IN_ADC_Borad5V / 工装 5V            | 5V                      | ___ : ___ | ___ | 11 | ☐ |
| 4  | `ADC_CH_FCT_11V`        | PD6  | IN10 | 工装 11V / 12V dc-dc 自检            | 11 ~ 12V                | ___ : ___ | ___ | 11 | ☐ |
| 5  | `ADC_CH_VALVE_R`        | PC12 | IN17 | IN_ADC_DRV- / 阀门 R 路             | 0 ~ 5V (峰值或更高)      | ___ : ___ | ___ | 11 | ☐ |
| 6  | `ADC_CH_VALVE_B`        | PC7  | IN6  | IN_ADC_DRV+ / 阀门 B 路             | 0 ~ 5V (峰值或更高)      | ___ : ___ | ___ | 11 | ☐ |
| 7  | `ADC_CH_METER_TEMP_POW` | PD5  | IN3  | IN_ADC_ExV1 / 表板温压模组 POW      | (查温压模组手册)         | ___ : ___ | ___ | 11 | ☐ |
| 8  | `ADC_CH_METER_SECOND`   | PD4  | IN9  | IN_ADC_ExV2 / 表板次级电压          | 5V (碱) / 3.3V (锂)     | ___ : ___ | ___ | 11 | ☐ |
| 9  | `ADC_CH_METER_THIRD`    | PD3  | IN2  | IN_ADC_ExV3 / 次级经二极管          | 4.7V (碱) / 3.3V (锂)   | ___ : ___ | ___ | 11 | ☐ |
| 10 | `ADC_CH_GPRS_3V6`       | PD2  | IN8  | IN_ADC_ExV4 / GPRS 3.6V             | 3.6V                    | ___ : ___ | ___ | 11 | ☐ |
| 11 | `ADC_CH_RTC_BATTERY`    | PD1  | IN1  | IN_ADC_ExV5 / RTC 备份电池          | 3V                      | ___ : ___ | ___ | 11 | ☐ |
| 12 | `ADC_CH_METER_3V3`      | PD0  | IN7  | IN_ADC_ExV6 / 表板 3.3V             | 3.3V                    | ___ : ___ | ___ | 11 | ☐ |

## 高度怀疑的几项 (基于电压量级合理性推测)

以下通道旧版 `*11` 系数**几乎肯定不对**,优先核对:

### 1. `ADC_CH_RTC_BATTERY` (PD1, IN1)

RTC 备份电池通常是 3V 纽扣电池,内阻高、电流极小。一般做法是**直接接 ADC 引脚不分压**(因为分压电阻会形成漏电流,加速电池消耗)。

- 如果直连: `ratio = 1`
- 如果有简单分压用以保护: `ratio = 2` 左右
- `*11` 几乎绝无可能 — ADC 看到 3V 会被算成 33V,完全离谱

### 2. `ADC_CH_METER_3V3` (PD0, IN7)

3.3V 在 ADC 量程内 (VDDA ≈ 3V)。

- 不分压: `ratio = 1`
- 简单 1:1 分压 (避免 3.3V 略超 VDDA): `ratio = 2`
- `*11` 不可能

### 3. `ADC_CH_GPRS_3V6` (PD2, IN8)

3.6V 略高于 VDDA,大概率有简单分压。

- 1/2 分压: `ratio = 2`
- 1/3 分压: `ratio = 3`

### 4. `ADC_CH_METER_TEMP_POW` (PD5, IN3)

温压模组电源通常 5V 量级。

- 1/2 分压: `ratio = 2`
- 1/3 分压: `ratio = 3`

### 5. `ADC_CH_FCT_11V` (PD6, IN10)

11V 输入要降到 ADC 量程 (<3V)。

- 1/4 分压 (3k:1k): `ratio = 4`
- 1/5 分压 (4k:1k): `ratio = 5`
- 绝对不可能是 11(11V * 11 = 121V?)

### 6. `ADC_CH_FCT_5V` / `ADC_CH_METER_SECOND` 等 5V 量级

- 大概率 1/2 分压: `ratio = 2`

### 7. 阀门 R/B (`ADC_CH_VALVE_R/B`)

阀门驱动可能有 5V 稳态、开关脉冲峰值更高 (取决于电路)。这两个的 `*11` 有可能就是对的(脉冲峰值会冲到 30V+ 量级时需要),但建议示波器实测一次确认。

## 验证步骤

1. 按上表填好实际 R_top : R_bot,算出 `应填 ratio`
2. 修改 `Src/ADC_CHK.c` 中 `adc_chan_table[]` 对应行的 `divider_ratio`
3. 用万用表实测被测点真实电压
4. 调用 `adc_read_mv(ADC_CH_xxx)` 看返回值
5. 二者应在 ±5% 内吻合 (16 倍硬件过采样后噪声很低,误差主要来自电阻精度和 ADC INL)
6. 不吻合时排查:
   - VDDA 实际值 (用万用表测 MCU 的 VDD 引脚)
   - `ADC_VREF` 在 FL 驱动中的出厂校准值是否合理
   - 分压电阻精度 (常见 1% 或 5%)
   - PCB 布线是否有干扰耦合到 ADC 输入
   - 是否需要在 ADC 输入加 0.1µF 滤波电容到地

## 可选:把分压电阻值也搬进表里

如果想做得更彻底,可以把 R_top / R_bot 值也搬到 `adc_chan_table[]` 里:

```c
typedef struct {
    GPIO_Type  *port;
    uint32_t    pin;
    uint32_t    fl_channel;
    uint32_t    r_top_kohm;     /* 上分压电阻,单位 kΩ */
    uint32_t    r_bot_kohm;     /* 下分压电阻,单位 kΩ */
    const char *name;
} adc_chan_cfg_t;
```

然后 `adc_read_mv` 内部用 `(r_top + r_bot) / r_bot` 替代 `divider_ratio`。
好处是原理图改电阻时改起来直观,坏处是每次读都要做除法(可考虑预计算缓存)。

当前 `divider_ratio` 简化版已经够用,以后真有需要再改。
