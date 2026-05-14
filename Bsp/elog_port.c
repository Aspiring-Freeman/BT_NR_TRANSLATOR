/**
 * @file    elog_port.c
 * @brief   EasyLogger 平台适配层 (FM33LE0xx 裸机)

 */
#include "Peripheral/uart/pc_uart.h"
#include "main.h"
#include <elog.h>

/* 裸机单线程锁（简单标志位） */
static volatile uint8_t elog_locked = 0;

ElogErrCode elog_port_init(void) {
  /* PC UART 已在 main.c 中初始化 */
  return ELOG_NO_ERR;
}

void elog_port_deinit(void) { /* nothing */
}

void elog_port_output(const char *log, size_t size) {

  /* Debug_Mode = 0 时不输出任何日志 */
  if (Debug_Mode == 0) {
    return;
  }

  PCUart_Tx_SendBlocking((const uint8_t *)log, (uint16_t)size);
}

void elog_port_output_lock(void) {
  while (elog_locked) {
  }
  elog_locked = 1;
}

void elog_port_output_unlock(void) { elog_locked = 0; }

const char *elog_port_get_time(void) { return ""; }
const char *elog_port_get_p_info(void) { return ""; }
const char *elog_port_get_t_info(void) { return ""; }
