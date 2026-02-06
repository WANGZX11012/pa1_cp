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

#include "sdb.h"
#include <string.h>
#include <stdlib.h>

#include <common.h>

#define NR_WP 32


typedef struct watchpoint 
{
  int NO;
  struct watchpoint *next;
  char *expr;
  word_t old_val;
  /* TODO: Add more members if necessary */

} WP;

static WP wp_pool[NR_WP] = {};//全局
static WP *head = NULL, *free_ = NULL; //全局

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);//一直指向下一个
  }

  head = NULL;
  free_ = wp_pool;
}

WP *new_wp(const char *expr, word_t old_val)
{

  if(free_ == NULL)
  {
    printf("wp is full\n");
    return NULL;
  }
  else 
  {
    WP *p = free_;//取空闲的头节点 新建的节点NO不用赋值 FREE_表中已经有了
    free_ = free_->next; //更新空闲wp表的头节点 指向下一个
    
    if(expr != NULL)
    {
      p->expr = malloc(strlen(expr) + 1);//先分配再赋值
      strcpy(p->expr, expr);
      p->old_val = old_val;

      p->next = head; //插入到head表
      head = p;
      return p;
    }
    return NULL;
  }
}


void free_wp(int no) 
{
  // 空链表
  if (head == NULL) 
  {
    printf("Watchpoint %d not found\n", no);
    return;
  }

  // 删除头节点
  if (head->NO == no) 
  {
    WP *target = head;
    head = head->next;
    if (target->expr) 
    { 
      free(target->expr); 
      target->expr = NULL; 
    }
    target->old_val = 0;

    target->next = free_;//放回到free_表 空闲表中
    free_ = target;
    printf("Deleted watchpoint %d\n", no);
    return;
  }

  // 用一个指针 p，检查 p->next 是否是目标
  WP *p;
  for (p = head; p->next != NULL; p = p->next) 
  {
    if (p->next->NO == no) 
    {
      WP *target = p->next;
      p->next = target->next;         // 从 head 链表移除
      if (target->expr) 
      { 
        free(target->expr); 
        target->expr = NULL; 
      }
      target->old_val = 0;
      target->next = free_;           // 放回空闲链表
      free_ = target;
      printf("Deleted watchpoint %d\n", no);
      return;
    }
  }

  printf("Watchpoint %d not found\n", no);
}

void print_watchpoints(void) //打印wp
{
  if (head == NULL) {
    printf("No watchpoints.\n");
    return;
  }
  printf("No  Expr                                 OldVal\n");
  for (WP *p = head; p != NULL; p = p->next) 
  {
    printf("%-3d %-36s 0x%08x\n",
           p->NO,
           p->expr ? p->expr : "(null)",
           p->old_val);
  }
}

int update_wp(void)
{
  int change_times = 0;
  WP *ptr;

  ptr = head;

  while (ptr != NULL)//检查每一个WP
  {
    bool success;
    word_t new_val = expr(ptr->expr, &success);
    if(!success)
    {
      printf("eval failure\n");
      continue;
    }
    else
    {
      if(new_val != ptr->old_val)
      { 
        printf("WP NO.%d %s changed ",ptr->NO, ptr->expr);
        printf("Old val is: 0x%08x, New is: 0x%08x \n",ptr->old_val, new_val);
        change_times ++;
        ptr->old_val = new_val;
      }
    }


    ptr = ptr->next;
  }
  
  return change_times;

}





/* TODO: Implement the functionality of watchpoint */

