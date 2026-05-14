/**
 * @file    main.h
 * @brief   应用层公共头文件
 */
#ifndef __MAIN_H__
#define __MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "fm33le0xx_fl.h"
#include "mf_config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 全局测试模式标志 (定义在 main.c) */
extern uint8_t Debug_Mode;           /**< 调试模式: 0=关闭, 1=开启 */
extern uint8_t PassThrough_Mode;     /**< 透传模式: 0=普通, 1=透传 */
extern uint8_t PassThrough_Preamble; /**< 透传前导: 0=无, 1=有 */
extern uint16_t debug_print_time;

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */
