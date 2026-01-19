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

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>// 包含正则表达式库，用于模式匹配

// 定义标记类型枚举：用于标识不同类型的标记
enum {
  TK_NOTYPE = 256, // 无类型标记（用于空格等忽略项）
  TK_EQ,           // 等于运算符 "=="
  TK_NEQ,
  TK_DEC,
  TK_NEG,          //-号 取反
  TK_DREF,         //* 解引用
  // TK_L_OR,         // 逻辑或 ||
  // TK_L_AND,        //逻辑与 &&
  /* TODO: Add more token types */// 待添加更多类型，如数字、变量等

};

static struct rule {
  const char *regex;// 正则表达式字符串
  int token_type;   // 匹配时对应的标记类型
} rules[] = { // 规则数组：定义所有识别规则，按优先级顺序

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    //  规则：匹配一个或多个空格，类型为无类型（忽略）
  {"\\+", '+'},         //  规则：匹配加号 "+"，类型为字符 '+'
  {"==", TK_EQ},        //  规则：匹配等于 "=="，类型为 TK_EQ
  {"\\-", '-'},
  {"\\*", '*'},
  {"\\/", '/'},
  {"\\(", '('},
  {"\\)", ')'},
  {"!=", TK_NEQ},
  {"[0-9]+", TK_DEC},   // 添加规则：匹配一个或多个数字字符，类型为 TK_DEC（用于十进制数字）
  


};

// 计算规则数组长度（宏 ARRLEN 在其他地方定义）
#define NR_REGEX ARRLEN(rules)   //计算长度

// 编译后的正则表达式数组：存储编译好的 regex_t 对象
static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() 
{
  int i;
  char error_msg[128]; // 错误消息缓冲区
  int ret;             // 返回值

  // 遍历所有规则，编译正则表达式
  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    // regcomp: 编译正则表达式，REG_EXTENDED 表示扩展正则语法 ret为0 编译成功
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      // panic: 打印错误并终止程序（panic 函数在其他地方定义）
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}


// 定义标记结构体：存储每个标记的类型和字符串
typedef struct token 
{
  int type;             // 标记类型（如 '+' 或 TK_EQ）
  char str[32];         // 标记的字符串内容（最多 31 字符 + 结束符）
} Token;
// 全局标记数组：存储解析出的标记
static Token tokens[32] __attribute__((used)) = {};
// 当前标记数量
static int nr_token __attribute__((used))  = 0;



static bool make_token(char *e) {
  int position = 0;
  int i;

  regmatch_t pmatch;

  nr_token = 0;// 重置标记数量

  while (e[position] != '\0') {
    /* Try all rules one by one. NR_REGEX 规则的数量*/
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) 
      /*re[i]是编译好的规则，
      e是输入的字符，根据编译好的规则匹配输入字符
      pmatch.rm_so偏移为0*/
      {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) 
        {
          case TK_NOTYPE :
            break;

          case '+':
            tokens[nr_token++].type = '+';
            break;

          case '-':
            tokens[nr_token++].type = '-';
            break;

          case '*':
            tokens[nr_token++].type = '*';
            break;
          
          case '/':
            tokens[nr_token++].type = '/';
            break;

          case '(':
              tokens[nr_token++].type = '(';
              break;
          case ')':
              tokens[nr_token++].type = ')';
              break;

          case TK_EQ:
            tokens[nr_token++].type = TK_EQ;
            break;

          case TK_NEQ:
            tokens[nr_token++].type = TK_NEQ;
            break;

          case TK_DEC:
            tokens[nr_token].type = TK_DEC; 
            Assert(substr_len < 32, "length of int is too long (> 31)");
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token++].str[substr_len] = '\0';
            break;


          default: //TODO(） 
            break;
        }

        break;
      }
    }

    if (i == NR_REGEX) 
    {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

// void print_tokens(char *e) 
// {
//   bool success = make_token(e);
//   if (!success) 
//   {
//     printf("failed tokenization\n");
//   }
//   else
//   {
//     printf("Total token: %d\n", nr_token);
//     for (int i = 0; i < nr_token; i++) 
//     {
//       printf("Token %d is: ", i);
//       if (tokens[i].type == TK_DEC) 
//       {
//         printf("%s\n", tokens[i].str);
//       } 
//       else 
//       {
//         switch (tokens[i].type) 
//         {
//           case '+': printf("+\n"); break;
//           case '-': printf("-\n"); break;
//           case '*': printf("*\n"); break;
//           case '/': printf("/\n"); break;
//           case '(': printf("(\n"); break;
//           case ')': printf(")\n"); break;
//           case TK_EQ: printf("==\n"); break;
//           case TK_NEQ: printf("!=\n"); break;
//           default: printf("unknown (%d)\n", tokens[i].type); break;
//         }
//       }
//     }
//   }

//   return ;
// }


//将函数声明为 static 可以限制其作用域，使其只能在定义该函数的文件内部使用，而不能被其他文件调用。
//检查表达式括号对称
static bool check_bracket(int p ,int q)
{
  if(!(tokens[p].type == '(' && tokens[q].type == ')'))
    return false;// 1 代表错误表达式                              
  
  int diff = 0;
  for(int i = p; i <= q; i++)
  {
    diff = (tokens[i].type == '(') ? diff + 1 : (tokens[i].type == ')') ? diff - 1 : diff;    
  }

  if(diff == 0)
    return true; //0 代表括号匹配
  else
    return false;

}

static bool surrounded_by_bracket(int x, int p ,int q) 
//以表达式的x位置为中心 看括号是否匹配 x一般是运算符的位置
{
  int l_bracket = 0;
  int r_bracket = 0;
  for(int i = x; i >= p; i--)
  {
    if(tokens[i].type == '(')
      l_bracket ++;
    if(tokens[i].type == ')')
      l_bracket --;
  }
  for (int i = x; i <= q; i++)
  {
    if(tokens[i].type == '(')
      r_bracket ++;
    if(tokens[i].type == ')')
      r_bracket --;
  }
  
  if(l_bracket == 0 && r_bracket == 0)
    return true;
  else
    return false;

}


static int get_op_priority(int op) //得到算术运算符的优先级
{
  switch (op)
  {
  case TK_NEG: case TK_DREF:
    return 2;
  
  case '*': case '/':
    return 3;
  
  case '+': case '-':
    return 4;
  
  case TK_EQ: case TK_NEQ:
    return 7;
  
  /*TODO*/
  default:
    return 0;
  }

}

static find_main_op(int p, int q)
{
  int op = -1;
  int last_piority = 0;



}











word_t expr(char *e, bool *success) 
{ 
  if (!make_token(e))  //执行make token
  {
    *success = false;
    return 0;
  }
  else
  {
    printf("Total tokens is %d \n",nr_token); //执行print
    //printf("bracket status is %d \n",check_bracket(0,nr_token - 1));
    *success = true;
    return 0;
  }
  /* TODO: Insert codes to evaluate the expression. */

}
