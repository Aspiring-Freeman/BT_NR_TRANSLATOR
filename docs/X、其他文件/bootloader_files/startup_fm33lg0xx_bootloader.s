/**
 ******************************************************************************
 * @file      startup_fm33lg0xx_bootloader.s
 * @brief     FM33LG0xx Bootloader 启动文件 (支持中断转发)
 * @author    SmallCaliberUWMTester
 * @version   v1.0.0
 * @date      2025-12-23
 ******************************************************************************
 *
 * 说明:
 * ====
 * 此文件专门用于 Bootloader 项目，实现以下功能：
 * 1. 初始化栈指针和数据段
 * 2. 调用 SystemInit 和 main
 * 3. 所有中断自动转发到 APP 的向量表 (0x4000)
 *
 * 原理:
 * ====
 * Cortex-M0+ 没有 VTOR 寄存器，无法修改向量表基地址。
 * 因此 Bootloader 的中断处理程序需要手动跳转到 APP 的对应处理程序。
 *
 * 使用方法:
 * ========
 * 将此文件复制到 Bootloader 项目，替换原有的 startup_fm33lg0xx.s
 *
 ******************************************************************************
 */

  .syntax unified
  .cpu cortex-m0plus
  .fpu softvfp
  .thumb

/* ============================================================================
 * 常量定义
 * ============================================================================ */

/* APP 起始地址 - 必须与 APP 的链接脚本一致 */
.equ APP_START_ADDRESS, 0x00004000

/* ============================================================================
 * 链接器符号
 * ============================================================================ */

.global  Default_Handler
.global  g_pfnVectors

/* 链接器定义的初始化值 */
.word  _sidata    /* .data 段初始化数据在 Flash 中的起始地址 */
.word  _sdata     /* .data 段在 RAM 中的起始地址 */
.word  _edata     /* .data 段在 RAM 中的结束地址 */
.word  _sbss      /* .bss 段起始地址 */
.word  _ebss      /* .bss 段结束地址 */

/* ============================================================================
 * Reset Handler - Bootloader 自己的复位处理程序
 * ============================================================================ */

  .section  .text.Reset_Handler
  .weak  Reset_Handler
  .type  Reset_Handler, %function
Reset_Handler:
    /* 设置栈指针 */
    ldr   r0, =_estack
    mov   sp, r0

    /* 复制 .data 段从 Flash 到 RAM */
    movs  r1, #0
    b     LoopCopyDataInit

CopyDataInit:
    ldr   r3, =_sidata
    ldr   r3, [r3, r1]
    str   r3, [r0, r1]
    adds  r1, r1, #4

LoopCopyDataInit:
    ldr   r0, =_sdata
    ldr   r3, =_edata
    adds  r2, r0, r1
    cmp   r2, r3
    bcc   CopyDataInit

    /* 清零 .bss 段 */
    ldr   r2, =_sbss
    b     LoopFillZerobss

FillZerobss:
    movs  r3, #0
    str   r3, [r2]
    adds  r2, r2, #4

LoopFillZerobss:
    ldr   r3, =_ebss
    cmp   r2, r3
    bcc   FillZerobss

    /* 调用系统初始化 */
    bl    SystemInit

    /* 调用 main 函数 */
    bl    main

    /* main 返回后进入死循环 */
LoopForever:
    b     LoopForever

  .size  Reset_Handler, .-Reset_Handler

/* ============================================================================
 * Default Handler - 默认中断处理程序 (Bootloader 自用)
 * ============================================================================ */

  .section  .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
    b     Infinite_Loop
  .size  Default_Handler, .-Default_Handler

/* ============================================================================
 * 中断转发处理程序
 * ============================================================================
 *
 * 这些处理程序读取 APP 向量表对应位置的地址并跳转执行。
 * 向量表布局:
 *   偏移 0x00: 初始 SP (不转发)
 *   偏移 0x04: Reset_Handler (不转发)
 *   偏移 0x08: NMI_Handler
 *   偏移 0x0C: HardFault_Handler
 *   ...
 *   偏移 0x3C: SysTick_Handler
 *   偏移 0x40: WWDT_IRQHandler (第一个外部中断)
 *   ...
 */

/* --------------------------------------------------------------------------
 * 系统异常转发 (偏移 0x08 - 0x3C)
 * -------------------------------------------------------------------------- */

  .section .text.NMI_Forward,"ax",%progbits
  .type NMI_Forward, %function
NMI_Forward:
    ldr   r0, =APP_START_ADDRESS + 0x08  /* NMI 偏移 */
    ldr   r0, [r0]
    bx    r0
  .size NMI_Forward, .-NMI_Forward

  .section .text.HardFault_Forward,"ax",%progbits
  .type HardFault_Forward, %function
HardFault_Forward:
    ldr   r0, =APP_START_ADDRESS + 0x0C  /* HardFault 偏移 */
    ldr   r0, [r0]
    bx    r0
  .size HardFault_Forward, .-HardFault_Forward

  .section .text.SVC_Forward,"ax",%progbits
  .type SVC_Forward, %function
SVC_Forward:
    ldr   r0, =APP_START_ADDRESS + 0x2C  /* SVC 偏移 */
    ldr   r0, [r0]
    bx    r0
  .size SVC_Forward, .-SVC_Forward

  .section .text.PendSV_Forward,"ax",%progbits
  .type PendSV_Forward, %function
PendSV_Forward:
    ldr   r0, =APP_START_ADDRESS + 0x38  /* PendSV 偏移 */
    ldr   r0, [r0]
    bx    r0
  .size PendSV_Forward, .-PendSV_Forward

  .section .text.SysTick_Forward,"ax",%progbits
  .type SysTick_Forward, %function
SysTick_Forward:
    ldr   r0, =APP_START_ADDRESS + 0x3C  /* SysTick 偏移 */
    ldr   r0, [r0]
    bx    r0
  .size SysTick_Forward, .-SysTick_Forward

/* --------------------------------------------------------------------------
 * 外部中断转发 (偏移 0x40 - 0xBC, IRQ0-IRQ31)
 * -------------------------------------------------------------------------- */

  .section .text.WWDT_IRQForward,"ax",%progbits
  .type WWDT_IRQForward, %function
WWDT_IRQForward:                         /* IRQ0, 偏移 0x40 */
    ldr   r0, =APP_START_ADDRESS + 0x40
    ldr   r0, [r0]
    bx    r0
  .size WWDT_IRQForward, .-WWDT_IRQForward

  .section .text.SVD_IRQForward,"ax",%progbits
  .type SVD_IRQForward, %function
SVD_IRQForward:                          /* IRQ1, 偏移 0x44 */
    ldr   r0, =APP_START_ADDRESS + 0x44
    ldr   r0, [r0]
    bx    r0
  .size SVD_IRQForward, .-SVD_IRQForward

  .section .text.RTC_IRQForward,"ax",%progbits
  .type RTC_IRQForward, %function
RTC_IRQForward:                          /* IRQ2, 偏移 0x48 */
    ldr   r0, =APP_START_ADDRESS + 0x48
    ldr   r0, [r0]
    bx    r0
  .size RTC_IRQForward, .-RTC_IRQForward

  .section .text.FLASH_IRQForward,"ax",%progbits
  .type FLASH_IRQForward, %function
FLASH_IRQForward:                        /* IRQ3, 偏移 0x4C */
    ldr   r0, =APP_START_ADDRESS + 0x4C
    ldr   r0, [r0]
    bx    r0
  .size FLASH_IRQForward, .-FLASH_IRQForward

  .section .text.FDET_IRQForward,"ax",%progbits
  .type FDET_IRQForward, %function
FDET_IRQForward:                         /* IRQ4, 偏移 0x50 */
    ldr   r0, =APP_START_ADDRESS + 0x50
    ldr   r0, [r0]
    bx    r0
  .size FDET_IRQForward, .-FDET_IRQForward

  .section .text.ADC_IRQForward,"ax",%progbits
  .type ADC_IRQForward, %function
ADC_IRQForward:                          /* IRQ5, 偏移 0x54 */
    ldr   r0, =APP_START_ADDRESS + 0x54
    ldr   r0, [r0]
    bx    r0
  .size ADC_IRQForward, .-ADC_IRQForward

  .section .text.DAC_IRQForward,"ax",%progbits
  .type DAC_IRQForward, %function
DAC_IRQForward:                          /* IRQ6, 偏移 0x58 */
    ldr   r0, =APP_START_ADDRESS + 0x58
    ldr   r0, [r0]
    bx    r0
  .size DAC_IRQForward, .-DAC_IRQForward

  .section .text.SPI0_IRQForward,"ax",%progbits
  .type SPI0_IRQForward, %function
SPI0_IRQForward:                         /* IRQ7, 偏移 0x5C */
    ldr   r0, =APP_START_ADDRESS + 0x5C
    ldr   r0, [r0]
    bx    r0
  .size SPI0_IRQForward, .-SPI0_IRQForward

  .section .text.SPI1_IRQForward,"ax",%progbits
  .type SPI1_IRQForward, %function
SPI1_IRQForward:                         /* IRQ8, 偏移 0x60 */
    ldr   r0, =APP_START_ADDRESS + 0x60
    ldr   r0, [r0]
    bx    r0
  .size SPI1_IRQForward, .-SPI1_IRQForward

  .section .text.SPI2_IRQForward,"ax",%progbits
  .type SPI2_IRQForward, %function
SPI2_IRQForward:                         /* IRQ9, 偏移 0x64 */
    ldr   r0, =APP_START_ADDRESS + 0x64
    ldr   r0, [r0]
    bx    r0
  .size SPI2_IRQForward, .-SPI2_IRQForward

  .section .text.UART0_IRQForward,"ax",%progbits
  .type UART0_IRQForward, %function
UART0_IRQForward:                        /* IRQ10, 偏移 0x68 */
    ldr   r0, =APP_START_ADDRESS + 0x68
    ldr   r0, [r0]
    bx    r0
  .size UART0_IRQForward, .-UART0_IRQForward

  .section .text.UART1_IRQForward,"ax",%progbits
  .type UART1_IRQForward, %function
UART1_IRQForward:                        /* IRQ11, 偏移 0x6C */
    ldr   r0, =APP_START_ADDRESS + 0x6C
    ldr   r0, [r0]
    bx    r0
  .size UART1_IRQForward, .-UART1_IRQForward

  .section .text.UART3_IRQForward,"ax",%progbits
  .type UART3_IRQForward, %function
UART3_IRQForward:                        /* IRQ12, 偏移 0x70 */
    ldr   r0, =APP_START_ADDRESS + 0x70
    ldr   r0, [r0]
    bx    r0
  .size UART3_IRQForward, .-UART3_IRQForward

  .section .text.UART4_IRQForward,"ax",%progbits
  .type UART4_IRQForward, %function
UART4_IRQForward:                        /* IRQ13, 偏移 0x74 */
    ldr   r0, =APP_START_ADDRESS + 0x74
    ldr   r0, [r0]
    bx    r0
  .size UART4_IRQForward, .-UART4_IRQForward

  .section .text.UART5_IRQForward,"ax",%progbits
  .type UART5_IRQForward, %function
UART5_IRQForward:                        /* IRQ14, 偏移 0x78 */
    ldr   r0, =APP_START_ADDRESS + 0x78
    ldr   r0, [r0]
    bx    r0
  .size UART5_IRQForward, .-UART5_IRQForward

  .section .text.U7816_IRQForward,"ax",%progbits
  .type U7816_IRQForward, %function
U7816_IRQForward:                        /* IRQ15, 偏移 0x7C */
    ldr   r0, =APP_START_ADDRESS + 0x7C
    ldr   r0, [r0]
    bx    r0
  .size U7816_IRQForward, .-U7816_IRQForward

  .section .text.LPUARTx_IRQForward,"ax",%progbits
  .type LPUARTx_IRQForward, %function
LPUARTx_IRQForward:                      /* IRQ16, 偏移 0x80 */
    ldr   r0, =APP_START_ADDRESS + 0x80
    ldr   r0, [r0]
    bx    r0
  .size LPUARTx_IRQForward, .-LPUARTx_IRQForward

  .section .text.I2C_IRQForward,"ax",%progbits
  .type I2C_IRQForward, %function
I2C_IRQForward:                          /* IRQ17, 偏移 0x84 */
    ldr   r0, =APP_START_ADDRESS + 0x84
    ldr   r0, [r0]
    bx    r0
  .size I2C_IRQForward, .-I2C_IRQForward

  .section .text.CCL_IRQForward,"ax",%progbits
  .type CCL_IRQForward, %function
CCL_IRQForward:                          /* IRQ18, 偏移 0x88 */
    ldr   r0, =APP_START_ADDRESS + 0x88
    ldr   r0, [r0]
    bx    r0
  .size CCL_IRQForward, .-CCL_IRQForward

  .section .text.AES_IRQForward,"ax",%progbits
  .type AES_IRQForward, %function
AES_IRQForward:                          /* IRQ19, 偏移 0x8C */
    ldr   r0, =APP_START_ADDRESS + 0x8C
    ldr   r0, [r0]
    bx    r0
  .size AES_IRQForward, .-AES_IRQForward

  .section .text.LPTIM_IRQForward,"ax",%progbits
  .type LPTIM_IRQForward, %function
LPTIM_IRQForward:                        /* IRQ20, 偏移 0x90 */
    ldr   r0, =APP_START_ADDRESS + 0x90
    ldr   r0, [r0]
    bx    r0
  .size LPTIM_IRQForward, .-LPTIM_IRQForward

  .section .text.DMA_IRQForward,"ax",%progbits
  .type DMA_IRQForward, %function
DMA_IRQForward:                          /* IRQ21, 偏移 0x94 */
    ldr   r0, =APP_START_ADDRESS + 0x94
    ldr   r0, [r0]
    bx    r0
  .size DMA_IRQForward, .-DMA_IRQForward

  .section .text.WKUPx_IRQForward,"ax",%progbits
  .type WKUPx_IRQForward, %function
WKUPx_IRQForward:                        /* IRQ22, 偏移 0x98 */
    ldr   r0, =APP_START_ADDRESS + 0x98
    ldr   r0, [r0]
    bx    r0
  .size WKUPx_IRQForward, .-WKUPx_IRQForward

  .section .text.LUT_IRQForward,"ax",%progbits
  .type LUT_IRQForward, %function
LUT_IRQForward:                          /* IRQ23, 偏移 0x9C */
    ldr   r0, =APP_START_ADDRESS + 0x9C
    ldr   r0, [r0]
    bx    r0
  .size LUT_IRQForward, .-LUT_IRQForward

  .section .text.BSTIM_IRQForward,"ax",%progbits
  .type BSTIM_IRQForward, %function
BSTIM_IRQForward:                        /* IRQ24, 偏移 0xA0 */
    ldr   r0, =APP_START_ADDRESS + 0xA0
    ldr   r0, [r0]
    bx    r0
  .size BSTIM_IRQForward, .-BSTIM_IRQForward

  .section .text.COMPx_IRQForward,"ax",%progbits
  .type COMPx_IRQForward, %function
COMPx_IRQForward:                        /* IRQ25, 偏移 0xA4 */
    ldr   r0, =APP_START_ADDRESS + 0xA4
    ldr   r0, [r0]
    bx    r0
  .size COMPx_IRQForward, .-COMPx_IRQForward

  .section .text.GPTIM0_1_IRQForward,"ax",%progbits
  .type GPTIM0_1_IRQForward, %function
GPTIM0_1_IRQForward:                     /* IRQ26, 偏移 0xA8 */
    ldr   r0, =APP_START_ADDRESS + 0xA8
    ldr   r0, [r0]
    bx    r0
  .size GPTIM0_1_IRQForward, .-GPTIM0_1_IRQForward

  .section .text.GPTIM2_IRQForward,"ax",%progbits
  .type GPTIM2_IRQForward, %function
GPTIM2_IRQForward:                       /* IRQ27, 偏移 0xAC */
    ldr   r0, =APP_START_ADDRESS + 0xAC
    ldr   r0, [r0]
    bx    r0
  .size GPTIM2_IRQForward, .-GPTIM2_IRQForward

  .section .text.ATIM_IRQForward,"ax",%progbits
  .type ATIM_IRQForward, %function
ATIM_IRQForward:                         /* IRQ28, 偏移 0xB0 */
    ldr   r0, =APP_START_ADDRESS + 0xB0
    ldr   r0, [r0]
    bx    r0
  .size ATIM_IRQForward, .-ATIM_IRQForward

  .section .text.VREF_IRQForward,"ax",%progbits
  .type VREF_IRQForward, %function
VREF_IRQForward:                         /* IRQ29, 偏移 0xB4 */
    ldr   r0, =APP_START_ADDRESS + 0xB4
    ldr   r0, [r0]
    bx    r0
  .size VREF_IRQForward, .-VREF_IRQForward

  .section .text.GPIO_IRQForward,"ax",%progbits
  .type GPIO_IRQForward, %function
GPIO_IRQForward:                         /* IRQ30, 偏移 0xB8 */
    ldr   r0, =APP_START_ADDRESS + 0xB8
    ldr   r0, [r0]
    bx    r0
  .size GPIO_IRQForward, .-GPIO_IRQForward

  .section .text.CAN_IRQForward,"ax",%progbits
  .type CAN_IRQForward, %function
CAN_IRQForward:                          /* IRQ31, 偏移 0xBC */
    ldr   r0, =APP_START_ADDRESS + 0xBC
    ldr   r0, [r0]
    bx    r0
  .size CAN_IRQForward, .-CAN_IRQForward

/* ============================================================================
 * 向量表
 * ============================================================================
 *
 * 向量表必须放在 Flash 起始地址 (0x0000)
 * - 前两个条目 (SP, Reset) 是 Bootloader 自己的
 * - 其他所有中断都转发到 APP
 */

  .section  .isr_vector,"a",%progbits
  .type  g_pfnVectors, %object
  .size  g_pfnVectors, .-g_pfnVectors

g_pfnVectors:
    /* 系统异常 (0x00 - 0x3C) */
    .word  _estack                  /* 0x00: 初始栈指针 (Bootloader) */
    .word  Reset_Handler            /* 0x04: Reset (Bootloader 自己的) */
    .word  NMI_Forward              /* 0x08: NMI -> 转发到 APP */
    .word  HardFault_Forward        /* 0x0C: HardFault -> 转发到 APP */
    .word  0                        /* 0x10: Reserved */
    .word  0                        /* 0x14: Reserved */
    .word  0                        /* 0x18: Reserved */
    .word  0                        /* 0x1C: Reserved */
    .word  0                        /* 0x20: Reserved */
    .word  0                        /* 0x24: Reserved */
    .word  0                        /* 0x28: Reserved */
    .word  SVC_Forward              /* 0x2C: SVC -> 转发到 APP */
    .word  0                        /* 0x30: Reserved */
    .word  0                        /* 0x34: Reserved */
    .word  PendSV_Forward           /* 0x38: PendSV -> 转发到 APP */
    .word  SysTick_Forward          /* 0x3C: SysTick -> 转发到 APP */

    /* 外部中断 (0x40 - 0xBC, IRQ0-IRQ31) */
    .word  WWDT_IRQForward          /* 0x40: IRQ0  - WWDT */
    .word  SVD_IRQForward           /* 0x44: IRQ1  - SVD */
    .word  RTC_IRQForward           /* 0x48: IRQ2  - RTC */
    .word  FLASH_IRQForward         /* 0x4C: IRQ3  - FLASH */
    .word  FDET_IRQForward          /* 0x50: IRQ4  - FDET */
    .word  ADC_IRQForward           /* 0x54: IRQ5  - ADC */
    .word  DAC_IRQForward           /* 0x58: IRQ6  - DAC */
    .word  SPI0_IRQForward          /* 0x5C: IRQ7  - SPI0 */
    .word  SPI1_IRQForward          /* 0x60: IRQ8  - SPI1 */
    .word  SPI2_IRQForward          /* 0x64: IRQ9  - SPI2 */
    .word  UART0_IRQForward         /* 0x68: IRQ10 - UART0 */
    .word  UART1_IRQForward         /* 0x6C: IRQ11 - UART1 */
    .word  UART3_IRQForward         /* 0x70: IRQ12 - UART3 */
    .word  UART4_IRQForward         /* 0x74: IRQ13 - UART4 */
    .word  UART5_IRQForward         /* 0x78: IRQ14 - UART5 */
    .word  U7816_IRQForward         /* 0x7C: IRQ15 - U7816 */
    .word  LPUARTx_IRQForward       /* 0x80: IRQ16 - LPUARTx */
    .word  I2C_IRQForward           /* 0x84: IRQ17 - I2C */
    .word  CCL_IRQForward           /* 0x88: IRQ18 - CCL */
    .word  AES_IRQForward           /* 0x8C: IRQ19 - AES */
    .word  LPTIM_IRQForward         /* 0x90: IRQ20 - LPTIM */
    .word  DMA_IRQForward           /* 0x94: IRQ21 - DMA */
    .word  WKUPx_IRQForward         /* 0x98: IRQ22 - WKUPx */
    .word  LUT_IRQForward           /* 0x9C: IRQ23 - LUT */
    .word  BSTIM_IRQForward         /* 0xA0: IRQ24 - BSTIM */
    .word  COMPx_IRQForward         /* 0xA4: IRQ25 - COMPx */
    .word  GPTIM0_1_IRQForward      /* 0xA8: IRQ26 - GPTIM0/1 */
    .word  GPTIM2_IRQForward        /* 0xAC: IRQ27 - GPTIM2 */
    .word  ATIM_IRQForward          /* 0xB0: IRQ28 - ATIM */
    .word  VREF_IRQForward          /* 0xB4: IRQ29 - VREF */
    .word  GPIO_IRQForward          /* 0xB8: IRQ30 - GPIO */
    .word  CAN_IRQForward           /* 0xBC: IRQ31 - CAN */

/* ============================================================================
 * 结束
 * ============================================================================ */
