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

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>

#define R(i) gpr(i)       // 读/写通用寄存器
#define Mr vaddr_read     // 读内存
#define Mw vaddr_write    // 写内存

enum {
  TYPE_I, TYPE_U, TYPE_S,
  TYPE_N, // none
};
// 从指令中取寄存器/立即数

#define src1R() do { *src1 = R(rs1); } while (0) 
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst;
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd     = BITS(i, 11, 7);
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_N: break;
    default: panic("unsupported type = %d", type);
  }
}

static int decode_exec(Decode *s) 
{
  s->dnpc = s->snpc; // 默认下一条PC=顺序执行PC(先假定不跳转)

#define INSTPAT_INST(s) ((s)->isa.inst)// 取当前指令字(32-bit)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  int rd = 0; \
  word_t src1 = 0, src2 = 0, imm = 0; \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

  INSTPAT_START(); // 开始遍历指令模式(内部会生成一个“结束跳转点”) 用的是空参数的宏
  // 下面每条 INSTPAT 都会展开成一个 if 匹配：
  // if (((inst >> shift) & mask) == key) { 执行语义; goto end; }
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm);   
  /*宏展开pattern_decode(...) 把字符串模板编成 key/mask/shift。
  用 INSTPAT_INST(s) 取当前指令字（s->isa.inst）。
  判断 ((inst >> shift) & mask) == key，成立就命中这条规则。
  进入 INSTPAT_MATCH(...)：先 decode_operand(...) 解出 rd/imm（U 型），然后执行 R(rd) = s->pc + imm。
  goto *(__instpat_end) 跳出整个匹配过程（不再检查后面的指令规则）。*/

  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, src2));

  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));
  INSTPAT_END();// 结束匹配(命中后会跳到这里) 然后执行$0变成0 后return

  R(0) = 0; 

  return 0;
}


int isa_exec_once(Decode *s) 
{
  s->isa.inst = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}