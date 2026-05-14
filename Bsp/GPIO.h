#ifndef __GPIO_H__
#define __GPIO_H__

#include "main.h"
#include <stdbool.h>

void GPIO_Init(void);
void GPIO_IRQHandler(void);
// 运行绿灯开关
void LED_GREEN_ON(void);
void LED_GREEN_OFF(void);
// 运行红灯开关
void LED_RED_ON(void);
void LED_RED_OFF(void);

#endif
