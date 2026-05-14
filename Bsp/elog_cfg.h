/**
 * @file    elog_cfg.h
 * @brief   EasyLogger 配置文件
 */
#ifndef _ELOG_CFG_H_
#define _ELOG_CFG_H_

/* 启用日志输出 */
#define ELOG_OUTPUT_ENABLE
/* 静态输出级别（VERBOSE = 全部输出） */
#define ELOG_OUTPUT_LVL ELOG_LVL_VERBOSE
/* 启用断言检查 */
#define ELOG_ASSERT_ENABLE
/* 每行日志缓冲区大小 */
#define ELOG_LINE_BUF_SIZE 256
/* 行号最大长度 */
#define ELOG_LINE_NUM_MAX_LEN 5
/* tag 最大长度 */
#define ELOG_FILTER_TAG_MAX_LEN 16
/* 关键字最大长度 */
#define ELOG_FILTER_KW_MAX_LEN 16
/* tag 级别过滤最大数量 */
#define ELOG_FILTER_TAG_LVL_MAX_NUM 5
/* 换行符 */
#define ELOG_NEWLINE_SIGN "\r\n"
/* 启用彩色输出 */
#define ELOG_COLOR_ENABLE
/* 输出函数名 */
#define ELOG_FMT_USING_FUNC
/* 输出行号 */
#define ELOG_FMT_USING_LINE
/* 输出文件名 */
#define ELOG_FMT_USING_DIR

#endif /* _ELOG_CFG_H_ */
