#include "LED_CTRL.h"
#include "GPIO.h"
/*============================================================================
 *                     旧版LED控制 (兼容保留)
 *===========================================================================*/

uint8_t LED_thing_FLAG = 0;
uint8_t LED_thing_time = 0;

void LED_FLAG_Run() {
  LED_GREEN_ON();
  LED_thing_FLAG = 1;
  // 灯会亮一下
  LED_thing_time = 20;
}

void LED_FLAG_LOOP() {
  if (LED_thing_FLAG == 0)
    return;
  if (LED_thing_time != 0)
    return;
  LED_thing_FLAG = 0;
  LED_GREEN_OFF();
}

/*============================================================================
 *                     新版LED指示器组件适配
 *===========================================================================*/

/**
 * @brief LED硬件控制回调
 * @param led_index LED索引 (0=LED1, 当前工装只有2个LED)
 * @param state 状态 (0=灭, 1=亮)
 */
static void board_led_control(uint8_t led_index, uint8_t state) {
  if (led_index == 0) {
    if (state) {
      LED_GREEN_ON();
    } else {
      LED_GREEN_OFF();
    }
  } else if (led_index == 2) {
    if (state) {
      LED_RED_ON();
    } else {
      LED_RED_OFF();
    }
  } else { // 红绿都亮，黄色
    if (state) {
      LED_RED_ON();
      LED_GREEN_ON();
    } else {
      LED_RED_OFF();
      LED_GREEN_OFF();
    }
  }
}

/**
 * @brief 实时监测status状态
 * @note 根据status状态亮不同颜色的灯
 */
void LedCheck(void) {
  uint8_t bluetooth_state;
  // 读取gpio电平
  bluetooth_state = FL_GPIO_GetInputPin(GPIOA, FL_GPIO_PIN_13);
  if (bluetooth_state == 0) {
    // 蓝牙连接成功，亮绿灯
    LED_GREEN_ON();
    LED_RED_OFF();
  } else {
    // 蓝牙未连接，亮红灯
    LED_GREEN_OFF();
    LED_RED_ON();
  }
}
