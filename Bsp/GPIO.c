#include "GPIO.h"
#include "board_pins.h"
#include "fm33le0xx_fl.h"

// 运行绿灯开关
void LED_GREEN_ON() { FL_GPIO_ResetOutputPin(GPIOD, FL_GPIO_PIN_1); }
void LED_GREEN_OFF() { FL_GPIO_SetOutputPin(GPIOD, FL_GPIO_PIN_1); }
// 运行红灯开关
void LED_RED_ON() { FL_GPIO_ResetOutputPin(GPIOD, FL_GPIO_PIN_6); }
void LED_RED_OFF() { FL_GPIO_SetOutputPin(GPIOD, FL_GPIO_PIN_6); }

// LED的驱动在这里
void GPIO_Init() {
  FL_GPIO_InitTypeDef GPIO_InitStruct;
  // 绿灯
  GPIO_InitStruct.pin = FL_GPIO_PIN_1;
  GPIO_InitStruct.mode = FL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.outputType = FL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.pull = FL_ENABLE;
  GPIO_InitStruct.remapPin = FL_DISABLE;
  (void)FL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  // 初始化设置为高电平，避免功耗浪费
  FL_GPIO_SetOutputPin(GPIOD, FL_GPIO_PIN_0);
  // 红灯
  GPIO_InitStruct.pin = FL_GPIO_PIN_6;
  GPIO_InitStruct.mode = FL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.outputType = FL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.pull = FL_ENABLE;
  GPIO_InitStruct.remapPin = FL_DISABLE;
  (void)FL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  // 初始化配置为高电平
  FL_GPIO_SetOutputPin(GPIOD, FL_GPIO_PIN_6);

  // 配置蓝牙连接状态检测，PA13
  GPIO_InitStruct.pin = FL_GPIO_PIN_13;
  GPIO_InitStruct.mode = FL_GPIO_MODE_INPUT;
  GPIO_InitStruct.outputType = FL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.pull = FL_ENABLE;
  GPIO_InitStruct.remapPin = FL_DISABLE;
  (void)FL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // 初始化蓝牙status io,可以用来检测蓝牙模组是否存在或者状态
}
