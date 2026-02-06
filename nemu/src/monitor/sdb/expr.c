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
#include <common.h> //assert
#include <memory/vaddr.h>

int div_zero_count = 0;//全局变量 记录除0次数


// 定义标记类型枚举：用于标识不同类型的标记
enum {
  TK_NOTYPE = 256, // 无类型标记（用于空格等忽略项）
  TK_EQ,           // 等于运算符 "=="
  TK_NEQ,          
  TK_DEC,
  TK_NEG,          //-号 取反
  TK_UPLUS,        //+号 一元运算符

  TK_DREF,         //* 解引用

  TK_REG,          //寄存器类型

  TK_HEX,
            //十六进制数
  TK_L_OR,         // 逻辑或 ||
  TK_L_AND,        //逻辑与 &&
  TK_LE,           //小于等于 <=
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
  {"<=", TK_LE},
  {"&&", TK_L_AND},     //逻辑与 0与任何为0 可以短路运算
  {"\\|\\|", TK_L_OR},     // 逻辑或，转义 |
  {"0[xX][0-9a-fA-F]+", TK_HEX}, //优先级比十进制高
  {"[0-9]+", TK_DEC},   // 添加规则：匹配一个或多个数字字符，类型为 TK_DEC（用于十进制数字）
  {"\\$[a-z0-9]+|\\$\\$[0-9]+", TK_REG},



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
  char str[1024];         // 标记的字符串内容（最多 31 字符 + 结束符）
} Token;
// 全局标记数组：存储解析出的标记
static Token tokens[1024] __attribute__((used)) = {};
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

        // 防止空匹配导致无限循环
        if (substr_len == 0) 
        {
          continue;  // 跳过空匹配，继续下一个规则
        }

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //     i, rules[i].regex, position, substr_len, substr_len, substr_start); //打印token 注释掉

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

          case TK_L_AND:
            tokens[nr_token++].type = TK_L_AND;
            break;

          case TK_L_OR:
            tokens[nr_token++].type = TK_L_OR;
            break;

          case TK_LE:
            tokens[nr_token++].type = TK_LE;
            break;  

          case TK_HEX:
          {
            tokens[nr_token].type = TK_HEX; 
            Assert(substr_len < 32, "length of HEX is too long (> 31)");
            strncpy(tokens[nr_token].str, substr_start , substr_len);
            tokens[nr_token++].str[substr_len] = '\0';
            break;
          }
          
          case TK_REG:
          { 
            tokens[nr_token].type = TK_REG; 
            Assert(substr_len < 32, "length of reg is too long (> 31)");
            strncpy(tokens[nr_token].str, substr_start + 1, substr_len - 1);//跳过首字符$ 总字符也要减一 
            tokens[nr_token++].str[substr_len - 1] = '\0';
            break;      
          }

          case TK_DEC:
            tokens[nr_token].type = TK_DEC; 
            Assert(substr_len < 32, "length of int is too long (> 31)");
            strncpy(tokens[nr_token].str, substr_start , substr_len);
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
//检查表达式是否被括号包围 如果是可以直接去掉进行递归求值
//如果是(1+2) + (3+4)这种类型就不能去掉外层的括号
static bool check_bracket(int p ,int q)
{
  int diff = 0;
  for(int i = p; i < q; i++) //不扫描最后一个
  {
    diff = (tokens[i].type == '(') ? diff + 1 : (tokens[i].type == ')') ? diff - 1 : diff; 
    if(diff == 0) //中途为0 就不能直接拆开
      return false;   
  }

  if(tokens[q].type == ')' && diff == 1)
    return true; //1 代表括号匹配
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
  case TK_NEG: case TK_DREF: case TK_UPLUS:
    return 2;
  
  case '*': case '/':
    return 3;
  
  case '+': case '-':
    return 4;
  
  case TK_LE:
    return 6;

  case TK_EQ: case TK_NEQ: 
    return 7;
  
  case TK_L_AND: case TK_L_OR:
    return 8;

  default:
    return 0;
  }

}

static int find_main_op(int p, int q)  //不能直接用 要对表达式进行处理后用
{
  int op = -1;
  int last_piority = 0;
  for(int i = p; i <= q; i++)
  {
    int op_priority = get_op_priority(tokens[i].type);
    if((!surrounded_by_bracket(i, p, q)) || op_priority == 0) //一定排除括号包围的 以及除了运算符的部分
      continue;
    if(last_piority <= op_priority) //如果相同优先级 保证是右边的作为主运算符
    {
      last_piority = op_priority;
      op = i;
    }
  }
  
  if(last_piority == 0)
  {
    printf(ANSI_FMT("Cant find main op", ANSI_BG_RED) "\n");
    return -1;//返回非法下标
  }
  // printf("op position is %d\n", op);
  // printf("priority is %d\n",last_piority);   打印主运算符
  return op;
}


bool div_zero_flag = false; //除0标志

static uint32_t eval(int p, int q, bool *success)  //两个bug(1+2) + (3+4)会因为外面括号去掉 报错 以及除0的时候报错
{
  if(!*success)
    return 0;

  if(p > q)
  {
    printf(ANSI_FMT("Bad expr", ANSI_BG_RED) "\n");
    *success = false;
    return 0;
  }
  else if(p == q) //单个token 整数或者寄存器
  {
    if(tokens[p].type == TK_DEC)
      return strtoul(tokens[p].str, NULL, 10);//?;
    else if(tokens[p].type == TK_REG)
    {
      
      return isa_reg_str2val(tokens[p].str, success);
    }
    else if(tokens[p].type == TK_HEX)
    {
      return strtoul(tokens[p].str, NULL, 16);//16进制
    }
    else
    {  
      printf(ANSI_FMT("Bad expr", ANSI_BG_RED) "\n");
      *success = false;
      return 0;
    }
  }

  
  else if(check_bracket(p, q) == true)
  {
    return eval(p+1, q-1, success);//去掉外面的括号 递归求值
  }
  else 
  {
    int op = find_main_op(p, q); //以主操作符为中心分割表达式
    if(op == -1) //主运算符查找错误的返回值
    {
      printf(ANSI_FMT("Bad expr", ANSI_BG_RED) "\n");
      *success = false; //遇到错误才false 默认是true
        return 0;
    }

    uint32_t val1, val2;

    if(tokens[op].type == TK_NEG || tokens[op].type == TK_UPLUS)
    {
      val2 = eval(op+1, q, success);  // 一元只用右边
      return (tokens[op].type == TK_NEG) ? -val2 : val2;
    }

    else if (tokens[op].type == TK_DREF) 
    {
      uint32_t addr = eval(op + 1, q, success);
      if (!*success) return 0;
      return vaddr_read(addr, 4);
    }
    //上面这个if 全是一元运算符的
    
    if(tokens[op].type == TK_L_AND)//短路算逻辑与
    {
      val1 = eval(p, op-1, success);
      if(!*success)
        return 0;
      if(val1 == 0)
        return 0;
      val2 = eval(op+1, q, success);
      return (val2 != 0);
    }

    if(tokens[op].type == TK_L_OR)//短路算逻辑或
    {
      val1 = eval(p, op-1, success);
      if(!*success)
        return 0;
      if(val1 != 0)
        return 1;
      val2 = eval(op+1, q, success);
      return (val2 != 0);
    }

   
    
    val1 = eval(p, op-1, success);
    val2 = eval(op+1, q, success);
    


    switch (tokens[op].type)
    {
      case TK_LE:
        return ((int32_t)val1 <= (int32_t) val2);
        break;

      case '+':
        return val1 + val2;
        break;
      case '-':
        return val1 - val2;
        break;
      case '*':
        return val1 * val2;
        break;
      case '/':
        if(val2 == 0)
        {
          printf(ANSI_FMT("Cant devide 0", ANSI_BG_RED) "\n");
          div_zero_flag = true;
          *success = false;
          return 0;
        }
        else
          return (int32_t)val1 / (int32_t)val2;  //一定会舍弃小数
        break;
    
      case TK_EQ:
        return(val1 == val2) ? 1 : 0;
      case TK_NEQ:
        return(val1 == val2) ? 0 : 1;


      default:
        printf(ANSI_FMT("sth wrong", ANSI_BG_RED) "\n");
        *success = false;
        return 0;
        break;
    }
  }
}

static void fix_ops(Token *tokens, int nr_token) //一元运算符的处理 解引用和正负
{
  for (int i = 0; i <= nr_token - 1; i++) 
  {

//宏持续到"\"符号结束    
  #define PREV_ALLOW_UNARY(i) \
  ((i) == 0 || \
   tokens[(i)-1].type == '+' || tokens[(i)-1].type == '-' || \
   tokens[(i)-1].type == '*' || tokens[(i)-1].type == '/' || \
   tokens[(i)-1].type == '(' || \
   tokens[(i)-1].type == TK_NEG || tokens[(i)-1].type == TK_UPLUS || \
   tokens[(i)-1].type == TK_DREF || \
   tokens[(i)-1].type == TK_EQ || tokens[(i)-1].type == TK_NEQ || \
   tokens[(i)-1].type == TK_LE || tokens[(i)-1].type == TK_L_AND || \
   tokens[(i)-1].type == TK_L_OR)

    if (PREV_ALLOW_UNARY(i)) {
      if (tokens[i].type == '+') tokens[i].type = TK_UPLUS;
      else if (tokens[i].type == '-') tokens[i].type = TK_NEG;
      else if (tokens[i].type == '*') tokens[i].type = TK_DREF;
    }

#undef PREV_ALLOW_UNARY
  }
}





word_t expr(char *e, bool *success) //求值主函数
{ 
  if (!make_token(e)) 
  {
    *success = false;
    return 0;
  }

  fix_ops(tokens, nr_token);//修改运算符类型

  bool eval_success = true; //初始化默认是true
  uint32_t result = eval(0, nr_token - 1, &eval_success);

  if (eval_success) 
  {
    // printf(ANSI_FMT("Expr result is %u", ANSI_FG_GREEN) "\n", result);
    // printf(ANSI_FMT("Expr result is 0x%08x", ANSI_FG_GREEN) "\n", result);
    //打印集成在sdb的 cmd_p里面
    //printf("Expr result is %u\n", result); // 计算成功才打印
    *success = true;
    return result; //result是 uint32格式
  }
  else 
  {
    *success = false;
    return 0;
  }
}
