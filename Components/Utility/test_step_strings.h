/**
 * @file test_step_strings.h
 * @brief 测试步骤名称字符串库
 * @note 公共组件，用于将测试步骤枚举转换为可读字符串
 */

#ifndef __TEST_STEP_STRINGS_H__
#define __TEST_STEP_STRINGS_H__

#include <stdint.h>

/**
 * @brief 获取测试步骤的中文名称
 * @param step_id 步骤枚举值 (Test_liucheng)
 * @return 步骤中文名称字符串
 */
const char *TestStep_GetName(uint8_t step_id);

/**
 * @brief 获取测试步骤的简短名称
 * @param step_id 步骤枚举值 (Test_liucheng)
 * @return 步骤简短名称字符串
 */
const char *TestStep_GetShortName(uint8_t step_id);

#endif /* __TEST_STEP_STRINGS_H__ */
