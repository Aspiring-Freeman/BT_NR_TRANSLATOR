/**
 * @file fal_flash_fm33le015_port.c
 * @brief FAL Flash 移植层 - FM33LE015平台
 * @version 1.0.0
 * @date 2026-05-11
 *
 * 实现 FlashDB 的 FAL (Flash Abstraction Layer) 接口
 * 使用 FM33LE0xx FL Driver 进行底层Flash操作
 */

#include "fm33le0xx_fl.h"
#include <fal.h>
#include <string.h>

/*============================================================================
 * FM33LE015 Flash 参数
 *===========================================================================*/

#define FM33LE015_FLASH_START_ADDR 0x00000000UL /**< Flash起始地址 */
#define FM33LE015_FLASH_SIZE (256 * 1024)       /**< Flash总大小 256KB */
#define FM33LE015_FLASH_SECTOR_SIZE (2 * 1024)  /**< 扇区大小 2KB */
#define FM33LE015_FLASH_PAGE_SIZE (512)         /**< 页大小 512B */

/*============================================================================
 * Flash 操作实现
 *===========================================================================*/

/**
 * @brief 初始化 Flash
 * @return 0: 成功
 */
static int fm33le015_flash_init(void) {
  /* FM33LE015 Flash 无需特殊初始化 */
  return 0;
}

/**
 * @brief 从 Flash 读取数据
 * @param offset 相对于设备起始地址的偏移
 * @param buf 读取缓冲区
 * @param size 读取字节数
 * @return 实际读取的字节数，-1表示错误
 */
static int fm33le015_flash_read(long offset, uint8_t *buf, size_t size) {
  uint32_t addr = FM33LE015_FLASH_START_ADDR + offset;

  /* 直接内存读取 */
  memcpy(buf, (const void *)addr, size);

  return size;
}

/**
 * @brief 向 Flash 写入数据
 * @param offset 相对于设备起始地址的偏移
 * @param buf 写入数据
 * @param size 写入字节数
 * @return 实际写入的字节数，-1表示错误
 *
 * @note FM33LE015 要求4字节对齐写入
 */
static int fm33le015_flash_write(long offset, const uint8_t *buf, size_t size) {
  uint32_t addr = FM33LE015_FLASH_START_ADDR + offset;
  size_t i;
  uint32_t write_data;
  const uint8_t *src = buf;
  FL_ErrorStatus status;

  /* 检查4字节对齐 */
  if ((addr % 4) != 0) {
    return -1;
  }

  /* 按4字节写入 */
  for (i = 0; i < size; i += 4) {
    /* 组装32位数据 (小端序) */
    if (i + 4 <= size) {
      write_data = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
    } else {
      /* 处理不足4字节的尾部数据 */
      write_data = 0xFFFFFFFF;
      for (size_t j = 0; j < (size - i); j++) {
        write_data &= ~(0xFF << (j * 8));
        write_data |= (src[j] << (j * 8));
      }
    }

    status = FL_FLASH_Program_Word(FLASH, addr, write_data);
    if (status != FL_PASS) {
      return -1;
    }

    addr += 4;
    src += 4;
  }

  return size;
}

/**
 * @brief 擦除 Flash 扇区
 * @param offset 相对于设备起始地址的偏移
 * @param size 擦除大小 (会向上取整到扇区边界)
 * @return 实际擦除的字节数，-1表示错误
 *
 * @note FM33LE015 扇区大小为2KB
 */
static int fm33le015_flash_erase(long offset, size_t size) {
  uint32_t addr = FM33LE015_FLASH_START_ADDR + offset;
  uint32_t end_addr = addr + size;
  FL_ErrorStatus status;

  /* 按扇区擦除 */
  while (addr < end_addr) {
    status = FL_FLASH_SectorErase(FLASH, addr);
    if (status != FL_PASS) {
      return -1;
    }
    addr += FM33LE015_FLASH_SECTOR_SIZE;
  }

  return size;
}

/*============================================================================
 * Flash 设备定义
 *===========================================================================*/

/**
 * @brief FM33LE015 片上Flash设备
 */
const struct fal_flash_dev fm33le015_onchip_flash = {
    .name = "fm33le015_onchip",
    .addr = FM33LE015_FLASH_START_ADDR,
    .len = FM33LE015_FLASH_SIZE,
    .blk_size = FM33LE015_FLASH_SECTOR_SIZE, /* 擦除块大小 = 扇区大小 */
    .ops =
        {
            .init = fm33le015_flash_init,
            .read = fm33le015_flash_read,
            .write = fm33le015_flash_write,
            .erase = fm33le015_flash_erase,
        },
    .write_gran = 32, /* 写入粒度: 32位 */
};
