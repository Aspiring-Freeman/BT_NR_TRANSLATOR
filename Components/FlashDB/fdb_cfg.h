/**
 * @file fdb_cfg.h
 * @brief FlashDB 配置文件 - FM33LG04x平台
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef _FDB_CFG_H_
#define _FDB_CFG_H_

/* 使用 KVDB (Key-Value数据库) 功能 */
#define FDB_USING_KVDB

#ifdef FDB_USING_KVDB
/* KV版本号改变时自动更新到最新默认值 */
/* #define FDB_KV_AUTO_UPDATE */
#endif

/* 不使用 TSDB (时间序列数据库) 功能 - 节省Flash空间 */
/* #define FDB_USING_TSDB */

/* 使用 FAL (Flash Abstraction Layer) 存储模式 */
#define FDB_USING_FAL_MODE

#ifdef FDB_USING_FAL_MODE
/**
 * Flash 写入粒度，单位: bit
 * FM33LG04x 支持32位(4字节)对齐写入
 * 支持的值: 1(nor flash)/ 8(stm32f2/f4)/ 32(stm32f1, fm33lg)/ 64(stm32f7)/
 * 128(stm32h5)
 */
#define FDB_WRITE_GRAN 32
#endif

/* 不使用文件存储模式 */
/* #define FDB_USING_FILE_LIBC_MODE */
/* #define FDB_USING_FILE_POSIX_MODE */

/* MCU字节序配置 - FM33LG04x 是小端序，使用默认即可 */
/* #define FDB_BIG_ENDIAN */

/* 使用 EasyLogger 作为日志输出 */
#ifdef COMPONENT_EASYLOGGER
#include <elog.h>
#define FDB_PRINT(...) log_i(__VA_ARGS__)
#else
#define FDB_PRINT(...) /* 无日志 */
#endif

/* 启用调试信息 */
#define FDB_DEBUG_ENABLE

#endif /* _FDB_CFG_H_ */
