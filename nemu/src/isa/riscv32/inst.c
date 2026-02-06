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
  TYPE_N, 
  TYPE_J, TYPE_R, TYPE_B,  // none  //NEW J R  B TYPE
};
// 从指令中取寄存器/立即数

#define src1R() do { *src1 = R(rs1); } while (0) 
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)  //符号位扩展 i是当前取到的32位指令
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)
#define immJ() do { *imm = SEXT( (BITS(i, 31, 31)<<20 | BITS(i, 19, 12)<<12 | BITS(i, 20, 20)<<11 | BITS(i, 30, 21)<<1 ) , 21); }while(0)     //new
//这里的最后一个 21 告诉 SEXT 宏：“这个数的符号位在第 21 位（即 bit 20），请根据这一位做扩展。”
#define immB() do { *imm = SEXT( (BITS(i, 31, 31)<<12 | BITS(i, 7, 7)<<11 | BITS(i, 30, 25)<<5 | BITS(i, 11, 8)<<1 ) , 13); } while(0)//new 最低位为0


static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst;
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd     = BITS(i, 11, 7);
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_J:                   immJ(); break; //new
    case TYPE_R: src1R(); src2R();         break; //new
    case TYPE_B: src1R(); src2R(); immB(); break; //new 
    case TYPE_N: break;
    default: panic("unsupported type = %d", type);
  }
}

static word_t my_div(word_t src1, word_t src2)
{
  if(src2 == 0)
    return (word_t)-1;
  else
  {
    if( (int32_t)src1 == INT32_MIN && (int32_t)src2 == -1)
      return (word_t) INT32_MIN;
    else
    {
      return (word_t)((int32_t)src1 / (int32_t)(src2));
    }
  }
}

static word_t my_rem(word_t src1, word_t src2) 
{
  if (src2 == 0) 
    return src1;
  int32_t a = (int32_t)src1;
  int32_t b = (int32_t)src2;
  if (a == INT32_MIN && b == -1) 
    return 0;
  return (word_t)(a % b); // C99: % 符号随被除数，且与 RISC-V 向零舍入匹配
}

static word_t my_mul(word_t a, word_t b) 
{
  int64_t prod = (int64_t)(int32_t)a * (int64_t)(int32_t)b;
  // 临时调试输出，便于定位错误
  //printf("[my_mul] a=0x%08x b=0x%08x prod=0x%08x\n", (uint32_t)a, (uint32_t)b, (uint32_t)prod);
  return (word_t)prod; // 取低 32 位（用 64 位中间值避免宽度问题）
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
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw     , I, R(rd) = Mr(src1 + imm, 4));
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui    , U, R(rd) = imm  ); //TYPE_U

  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, src2));
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw     , S, Mw(src1 + imm, 4, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh     , S, Mw(src1 + imm, 2, src2));
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub    , R, R(rd) = src1 - src2 );
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu  , I, R(rd) = (src1 < imm)); 
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai   , I, R(rd) = (int32_t)src1 >> BITS(imm, 4, 0) );  //无符号数字比较不用强制转换格式
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra    , R, R(rd) = (int32_t)src1 >> BITS(src2, 4, 0) ); 
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli   , I, R(rd) = src1 >> BITS(imm, 4, 0)  );          //逻辑右移都是直接移动
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl    , R, R(rd) = src1 >> BITS(src2, 4, 0)  ); 
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll    , R, R(rd) = src1 << BITS(src2, 4, 0)  ); 
  INSTPAT("0000000 ????? ????? 001 ????? 00100 11", slli   , I, R(rd) = src1 << BITS(imm, 4, 0)  ); 
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu   , R, R(rd) = (src1 < src2)  ); 

  INSTPAT("0000001 ????? ????? 100 ????? 01100 11", div    , R, R(rd) = my_div(src1, src2) );
  INSTPAT("0000001 ????? ????? 110 ????? 01100 11", rem    , R, R(rd) = my_rem(src1, src2) );
  INSTPAT("0000001 ????? ????? 000 ????? 01100 11", mul    , R, R(rd) = my_mul(src1, src2) );

  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi   , I, R(rd) = src1 + imm ); //addi
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add    , R, R(rd) = src1 + src2 );
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi   , I, R(rd) = src1 & imm ); 
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and    , R, R(rd) = src1 & src2 ); 


  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq    , B, s->dnpc = (src1 == src2) ? s->pc + imm : s->dnpc );
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne    , B, s->dnpc = (src1 != src2) ? s->pc + imm : s->dnpc );
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu   , B, s->dnpc = (src1 >= src2) ? s->pc + imm : s->dnpc );
  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge    , B, s->dnpc = ((int32_t)src1 >= (int32_t)src2) ? s->pc + imm : s->dnpc );
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt    , B, s->dnpc = ((int32_t)src1 < (int32_t)src2) ? s->pc + imm : s->dnpc );

  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal    , J, R(rd) = s->pc + 4; s->dnpc = s->pc + imm); //跳转用dnpc
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr   , I, R(rd) = s->pc + 4; s->dnpc = (src1 + imm) & ~1); //最低位变0

  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori   , I, R(rd) = src1 ^ imm );
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor    , R, R(rd) = src1 ^ src2 );
  
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or     , R, R(rd) = src1 | src2 );


  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));    //匹配所有未匹配的指令 优先级放到最后
  //TODO 添加更多指令




  INSTPAT_END();// 结束匹配(命中后会跳到这里) 然后执行$0变成0 后return

  R(0) = 0; 

  return 0;
}


int isa_exec_once(Decode *s) 
{
  s->isa.inst = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}