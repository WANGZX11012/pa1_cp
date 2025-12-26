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

static int is_batch_mode = false;

void init_regex();      // 初始化正则表达式（用于表达式求值）
void init_wp_pool();    // 初始化监视点池

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;// 返回输入字符串 类似fget
}

static int cmd_c(char *args) {
  cpu_exec(-1); // 执行无限步（直到结束）
  return 0;  // 返回 0 表示继续循环  -1才是退出
}


// 命令 'q' 的处理函数：退出 NEMU
static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;// 返回 -1 表示退出主循环
}

static int cmd_help(char *args);// 声明 help 命令处理函数
static int cmd_si(char *args);

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
  { "si","Let program step through N insts and then pause execution", cmd_si}





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
    for (i = 0; i < NR_CMD; i ++) 
    {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else 
  {
    for (i = 0; i < NR_CMD; i ++) 
    {
      if (strcmp(arg, cmd_table[i].name) == 0) 
      {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
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
      printf("input is not a valid positive number\n");
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
