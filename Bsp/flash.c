#include "flash.h"
#include <string.h>

#define FLASH_PROG_ADDR         0x0001E000

#define PAGE_BUFFER_SIZE    128
#define SECTOR_BUFFER_SIZE    512
#define DMA_BUFFER_SIZE    64

/* Page大小为512字节 即128个字 */
uint32_t PageDataBuffer[PAGE_BUFFER_SIZE];
/* Sector大小为2048字节 即512个字 */
uint32_t SectorDataBuffer[SECTOR_BUFFER_SIZE];
/* DMA 写 FLASH 大小为64个字 */
uint32_t DMADataBuffer[DMA_BUFFER_SIZE];
uint32_t ReadData;

void FlashR(uint32_t data)
{
    FL_FLASH_PageErase(FLASH, FLASH_PROG_ADDR);
		memset(PageDataBuffer, 0x00, PAGE_BUFFER_SIZE * 4);
		PageDataBuffer[0] = data;
    FL_FLASH_Program_Word(FLASH, FLASH_PROG_ADDR, PageDataBuffer[0]);

}
uint32_t FlashW(void)
{
  ReadData = *((uint32_t *)FLASH_PROG_ADDR);//将flash数据读出
	return ReadData;
}

