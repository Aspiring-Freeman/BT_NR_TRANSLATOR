#!/usr/bin/env python3
"""
协议校验和计算工具
用于计算水表测试程序的协议校验和
"""

def calculate_checksum(data_bytes):
    """计算简单的字节相加校验和"""
    return sum(data_bytes) & 0xFF

def parse_hex_string(hex_str):
    """解析十六进制字符串为字节数组"""
    hex_str = hex_str.replace(' ', '').replace('0x', '')
    return [int(hex_str[i:i+2], 16) for i in range(0, len(hex_str), 2)]

def format_hex_bytes(data_bytes):
    """格式化字节数组为十六进制字符串"""
    return ' '.join([f'{b:02X}' for b in data_bytes])

# 分析您的原始命令
print("=== 原始命令分析 ===")
original_cmd = "68 AA 19 00 00 01 80 12 60 21 00 01 01 00 0F 00 00 00 0F 00 14 00 E6 59 16"
original_bytes = parse_hex_string(original_cmd)
print(f"原始命令: {original_cmd}")
print(f"命令长度: {len(original_bytes)} 字节")

# 分析问题
print(f"协议头: {original_bytes[0]:02X} ({'正确' if original_bytes[0] == 0x68 else '错误'})")
print(f"命令码: {original_bytes[1]:02X} ({'开始测试' if original_bytes[1] == 0xAA else '未知'})")
print(f"帧长度: {original_bytes[2]:02X} ({original_bytes[2]} 字节)")
print(f"工位号: {original_bytes[3]:02X}")
print(f"协议尾: {original_bytes[-1]:02X} ({'正确' if original_bytes[-1] == 0x16 else '错误'})")

# 计算正确的校验和（不包括校验和字段本身和协议尾）
data_for_checksum = original_bytes[:-3]  # 去掉校验和(E6)、59、协议尾(16)
correct_checksum = calculate_checksum(data_for_checksum)
print(f"您的校验和: {original_bytes[-3]:02X}")
print(f"正确校验和: {correct_checksum:02X}")

print("\n=== 修正后的命令 ===")
# 构建正确的命令（去掉多余的59字节）
corrected_cmd = original_bytes[:-3] + [correct_checksum, 0x16]
corrected_length = len(corrected_cmd)
corrected_cmd[2] = corrected_length  # 更新长度字段

# 重新计算校验和
final_checksum = calculate_checksum(corrected_cmd[:-2])
corrected_cmd[-2] = final_checksum

print(f"修正命令: {format_hex_bytes(corrected_cmd)}")
print(f"命令长度: {len(corrected_cmd)} 字节")

print("\n=== 简化测试命令 ===")
# 创建最简单的开始测试命令
simple_cmd = [0x68, 0xAA, 0x06, 0x00]  # 协议头、命令、长度、工位号
simple_checksum = calculate_checksum(simple_cmd)
simple_cmd.extend([simple_checksum, 0x16])
print(f"简化命令: {format_hex_bytes(simple_cmd)}")

print("\n=== 查询命令 ===")
# 创建查询结果命令
query_cmd = [0x68, 0xAC, 0x06, 0x00]  # 协议头、查询命令、长度、工位号
query_checksum = calculate_checksum(query_cmd)
query_cmd.extend([query_checksum, 0x16])
print(f"查询命令: {format_hex_bytes(query_cmd)}")

print("\n=== 建议测试步骤 ===")
print("1. 首先发送简化命令，检查基本通信:")
print(f"   {format_hex_bytes(simple_cmd)}")
print("2. 如果没有响应，尝试不同的工位号 (01, 02, 03等)")
print("3. 如果有响应，再发送完整的测试命令:")
print(f"   {format_hex_bytes(corrected_cmd)}")
print("4. 最后可以发送查询命令:")
print(f"   {format_hex_bytes(query_cmd)}")
