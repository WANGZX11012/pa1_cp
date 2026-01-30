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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static uint32_t choose(uint32_t n)
{
  return rand() % n + 0;//生成0~n-1的随机数 
}

static void gen_num()//随机整数的生成
{
  char num_str[10];
  if(!(sprintf(num_str, "%d",(rand() % 100 + 1))))
    {
      printf("error gen num");
      return;
    }
  else
  strcat(buf, num_str);
}

static void gen_hex_num()//生成随机4位十六进制数，格式 0x1111
{
  uint32_t num = rand() % 65536; // 0 到 65535
  char hex_str[10];
  sprintf(hex_str, "0x%04X", num);
  strcat(buf, hex_str);
}



static void gen_char(char c)
{
  char my_c[2] = {c, '\0'}; //一定要\0结尾 不能定义后赋值
  strcat(buf, my_c);
}

static void gen_rand_op()
{
  char *ops[] = {"+", "-", "*", "/", "<=", "==", "!=", "||", "&&"};
  strcat(buf, ops[choose(9)]);
}

static void gen_unary_ops()
{
  char my_unary[2] = {'+', '-'};
  gen_char(my_unary[choose(2)]);
}


static void gen_rand_expr(); 
static void gen_term() 
{
  if (strlen(buf) > 50) 
  {
    gen_num();
    return;
  }

  switch (choose(3)) 
  {
  case 0:
    gen_num();
    break;
  case 1:
    gen_hex_num();
    break;

  default:
    gen_char('('); gen_rand_expr(); gen_char(')');
    break;
  }
}

static void gen_rand_expr() 
{
  if (strlen(buf) > 50) 
  {
    gen_term();
    return;
  }

  switch (choose(3)) 
  {
  case 0:
    gen_term();
    break;
  case 1:
    gen_unary_ops(); gen_char(' '); gen_term();  // 一元 + 空格 + term
    break;
  default:
    gen_term(); gen_char(' '); gen_rand_op(); gen_char(' '); gen_term();  // term + 空格 + op + 空格 + term
    break;
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
 
  int i;
  for (i = 0; i < loop; i ++) {
    
    buf[0] = '\0';//循环生成表达式前清空缓冲区
    gen_rand_expr();
    if(strlen(buf) >= 50000) //超出界限
    {
      printf("buffer overflow\n");
      exit(1);
    }

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
