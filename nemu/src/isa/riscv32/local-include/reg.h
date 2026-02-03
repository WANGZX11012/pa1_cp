/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

// 头文件保护宏：防止重复包含此头文件
#ifndef __RISCV_REG_H__
#define __RISCV_REG_H__

#include <common.h>


// 参数：idx - 寄存器索引
// 返回：有效的索引值
// 功能：如果启用了运行时检查(CONFIG_RT_CHECK)，断言索引在有效范围内（0 到 31 或 15，取决于 CONFIG_RVE）
static inline int check_reg_idx(int idx) {
  IFDEF(CONFIG_RT_CHECK, if (!(idx >= 0 && idx < MUXDEF(CONFIG_RVE, 16, 32))) { fprintf(stderr, "check_reg_idx fail: idx=%d caller=%p\n", idx, __builtin_return_address(0)); })
  IFDEF(CONFIG_RT_CHECK, assert(idx >= 0 && idx < MUXDEF(CONFIG_RVE, 16, 32)));
  return idx;
}

#define gpr(idx) (cpu.gpr[check_reg_idx(idx)])

// 通过宏和函数提供对 cpu.gpr[] 数组的访问，并进行索引有效性检查（防止越界）。
// 运行时检查：如果启用 CONFIG_RT_CHECK，断言索引在 0 到 31（或 15，如果启用 CONFIG_RVE）范围内。
// 寄存器名称映射：提供 reg_name() 函数，根据索引返回可读的寄存器名字符串（如 "zero", "ra"），用于调试显示。
// 抽象层：简化代码，避免直接访问 cpu.gpr[idx]，提高可维护性和安全性。
static inline const char* reg_name(int idx) {
  extern const char* regs[];
  return regs[check_reg_idx(idx)];
}

#endif
