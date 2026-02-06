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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

#include "watchpoint.h"
#include <memory/vaddr.h>//adding .h


extern bool div_zero_flag ;//除0标志


static int is_batch_mode = false;

void init_regex();      // 初始化正则表达式（用于表达式求值）
void init_wp_pool();    // 初始化监视点池



/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() 
{
  static char *line_read = NULL;

  if (line_read) 
  {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) 
  {
    add_history(line_read);
  }

  return line_read;// 返回输入字符串 类似fget
}

static int cmd_c(char *args) 
{
  cpu_exec(-1); // 执行无限步（直到结束）
  return 0;  // 返回 0 表示继续循环  -1才是退出
}


// 命令 'q' 的处理函数：退出 NEMU
static int cmd_q(char *args) 
{
  nemu_state.state = NEMU_QUIT;
  return -1;// 返回 -1 表示退出主循环
}












//新增命令注册
static int cmd_help(char *args);// 声明 help 命令处理函数
static int cmd_si(char *args);//单步执行
static int cmd_info(char *args);//寄存器或监视点信息
static int cmd_x(char *args);//查看内存地址
static int cmd_p(char *agrs);//表达式求值
static int cmd_t_expr(char *args);//测试表达式

static int cmd_w(char *args);//添加监视点
static int cmd_d(char *args);//删除监视点


// 命令表结构体：存储命令名、描述和处理函数
static struct {
  const char *name;         // 命令名
  const char *description;  // 描述
  int (*handler) (char *);  // 处理函数指针
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  //si[N]
  { "si","Let program step through N insts and then pause execution", cmd_si},
  //usage si 10 就是单步执行10次
  { "info","Type r to print all regs ; w to print all watchpoints",cmd_info},
  //usage ： info r 就是打印所有寄存器 info w打印监视点还没实现

  { "x","x to examine memory", cmd_x},
  //usage: x 10 0x8000000打印 内存地址为0x8000_0000附近的10个字节 内存的值

  { "p", "Print expression ", cmd_p},

  {"t_expr", "Tests 1000 generated expr", cmd_t_expr},
  
  {"w", "Add WatchPoint", cmd_w},
  {"d", "Delete WatchPoint", cmd_d},


  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)// 命令数量宏

// 命令 'help' 的处理函数
static int cmd_help(char *args) 
{
  /* extract the first argument */
  char *arg = strtok(NULL, " ");// NULL是继续分割输入的字符 前面已经通过分割得到命令的token
                                // 如help c这里就会让arg得到c
  int i;

  if (arg == NULL) 
  {
    /* no argument given */
    printf("Command  Description\n");
    for (i = 0; i < NR_CMD; i ++) 
    {
      printf("%-7s %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else 
  {
    for (i = 0; i < NR_CMD; i ++) 
    {
      if (strcmp(arg, cmd_table[i].name) == 0) 
      {
        printf("%-7s %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_si(char *args)
{
  char *arg = strtok(NULL, " ");
  int num = 1;
  if (arg != NULL) {
    char *endptr;
    num = strtol(arg, &endptr, 10); //把arg转成10进制的long型 &endptr用来存储转换停止的地方
    if (*endptr != '\0' || num <= 0) //例如输入 si a *endptr会保存a,if条件为真
    {
      printf(ANSI_FMT("Input is not a positive number", ANSI_BG_RED) "\n"); //红色背景
      return 0;  // 报错但继续循环
    }
  }
  cpu_exec(num);
  return 0;
}


// static int cmd_si(char *args) {
//   char *num_str = strtok(NULL, " ");
//   int num = 0;
//   if (num_str != NULL) {
//     Assert(sscanf(num_str, "%d", &num), "The input step num is not a number");
//   } else {
//     /* no argument given, the default num of step is 1 */
//     num = 1;
//   }
//   cpu_exec(num);
//   return 0;
// }




static int cmd_info(char *args)
{
  char *arg = strtok(NULL, " ");
  if(arg != NULL && strcmp(arg, "r") == 0) //限制只能输入一个r 命令中间能隔很多个空格
  {
    isa_reg_display();
  }
  else if(arg != NULL && strcmp(arg, "w") == 0)
  {
    print_watchpoints();

  }
  else
  {
    printf("Usage: info r | info w\n");
  }

  return 0;
}

static int cmd_x(char *args)//打印内存
{
  char *offset = strtok(NULL, " ");
  if(offset == NULL)
  { 
    printf("cmd_x Usage: x N Expr\n");
    return 0;
  }

  int32_t mem_offset = strtol(offset, NULL , 10);

  char *m_addr = strtok(NULL," ");
  if(m_addr == NULL)
  { 
    printf("cmd_x Usage: x N Expr\n");
    return 0;
  }
  vaddr_t mem_addr = strtol(m_addr, NULL, 16);
  
  printf("\nAddress     Data(32-bit)\n");
  for(int i = 0; i < mem_offset; i++)
  {
    printf("0x%08x  0x%08x\n", mem_addr, vaddr_read(mem_addr, 4)); //4字节数据
    mem_addr += 4; 
  }

  return 0;

}

// void print_tokens(char *e); //打印token的函数

// static int cmd_p(char *args)
// {
//   bool success;
//   word_t result = expr(args, &success);   //word_t is uint32_t
//   if (success) 
//   {
//     printf("Result: %u (0x%x)\n", result, result);
//     print_tokens(args);
//   } 
//   else 
//   {
//     printf("Invalid expression\n");
//   }
//   return 0;
// }



static int cmd_p(char *args) 
{
  bool success;
  if (args == NULL || *args == '\0') 
  {
    printf("Usage: p <expression>\n");
    return 0;
  }
  int expr_result = expr(args, &success);
  if (success) {
    printf("Expr = %u (0x%08x)\n", expr_result, (uint32_t)expr_result);
  } else {
    printf("Invalid expression\n");
  }
  return 0;
}

static int cmd_t_expr(char *args)
{
  FILE *fp = fopen(args, "r");
  if(!fp)
  {
    printf("Fail to open test file %s\n",args);
    return 0;
  }

  char line[1024];

  int passed = 0, total = 0;//通过测试数与测试总数
  int div_zero_count = 0;  // 初始化除0计数

  while ( fgets(line, sizeof(line),fp) ) //把文件读到line里面 每次读一行？
  {
    uint32_t expected;
    char expr_str[1024];

    if( (sscanf(line, "%u %[^\n]", &expected, expr_str)) == 2)//如果成功读取并存储
    //读取一行 前面的整数存expected后面的直到换行符存expr_str
    {
      // 去掉前导空格
      char *start = expr_str;
      while (*start == ' ') start++;
      strcpy(expr_str, start);

      bool success;
      div_zero_flag = false;

      word_t result = expr(expr_str, &success);

      if(div_zero_flag)
      {
        div_zero_count++;
        continue;
      }

      if(!success)
      {
        printf("Invalid expr %s", expr_str);
        continue;
      }
      else if(result == expected)
      {
        passed ++;
      }
      else 
      {
        printf("not equal: expected is %u yours is %u\n",expected, result);
        printf("expr is %s\n",expr_str);
      }

      total ++;
    }
  }

  fclose(fp);  // 关闭文件
  printf("passed :%d total: %d div_zero: %d\n", passed, total, div_zero_count);
  return 0;

}


static int cmd_w(char *args)
{
  bool success ;
  char *wp_expr;//不需要strtok分割

   if (args == NULL || *args == '\0') 
  {
    printf("Usage: w <expression>\n");
    return 0;
  }

  wp_expr = args;

  int result = expr(wp_expr, &success);//得到结果
  if(success == true)
  {
    WP *wp = new_wp(wp_expr, result);
    if (wp) {
      printf("Watchpoint %d set: %s = 0x%08x\n", wp->NO, wp_expr, (uint32_t)result);
    }
  }



  return 0;
}


static int cmd_d(char *args)
{
  char *wp_no_str = strtok(args, " ");
  if (wp_no_str == NULL) {
    printf("Usage: d <NO>\n");
    return 0;
  }
  
  char *endptr;
  int wp_num = strtol(wp_no_str, &endptr, 10);
  if (*endptr != '\0' || wp_num < 0) {
    printf("Invalid watchpoint number: %s\n", wp_no_str);
    return 0;
  }
  
  free_wp(wp_num);  // 传入编号，让 free_wp 内部查找
  return 0;
}




// 设置批处理模式
void sdb_set_batch_mode() 
{
  is_batch_mode = true;
}







// SDB 主循环：处理用户输入-----------------------------------------------//
void sdb_mainloop() 
{
  if (is_batch_mode) 
  { 
    // 批处理模式：直接执行 'c' 命令
    cmd_c(NULL);// 无额外参数 直接执行
    return;
  }
  // 交互模式：循环读取输入
  for (char *str; (str = rl_gets()) != NULL; ) //相当于while 无限读输入直到null或eof错误
  {
    char *str_end = str + strlen(str); // 输入字符串末尾  str代表开头

    /* extract the first token as the command */
    char *cmd = strtok(str, " "); //以空格分割得到第一个token
    if (cmd == NULL) { continue; } // 空输入，跳过

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1; //得到第一个命令后args继续向后移动 得到命令的参数
    if (args >= str_end)  //边界检查 如果越界就给null
    {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();// 清除 SDL 事件队列（如果启用设备
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) 
    {
      if (strcmp(cmd, cmd_table[i].name) == 0) // 查找匹配命令
      {
        if (cmd_table[i].handler(args) < 0) { return; }// 执行命令，如果返回 -1 则退出
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }// 未找到命令
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();// 编译正则表达式

  /* Initialize the watchpoint pool. */
  init_wp_pool();// 初始化监视点池
}
