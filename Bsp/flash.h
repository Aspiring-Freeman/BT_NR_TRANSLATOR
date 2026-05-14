#ifndef FLASH_H
#define FLASH_H

#include "fm33le0xx_fl.h"

//写入当前状态
void FlashR(uint32_t data);
//读取当前状态
uint32_t FlashW(void);
#endif
