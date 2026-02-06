#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include <stdint.h>  // for word_t (假设 word_t 是 uint32_t 或类似)

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  char *expr;
  word_t old_val;
  /* TODO: Add more members if necessary */
} WP;

// 全局变量声明（需在 watchpoint.c 中移除 static）
extern WP *head;
extern WP *free_;

// 函数声明
void init_wp_pool();
WP *new_wp(const char *expr, word_t old_val);

void free_wp(int no);
void print_watchpoints();

#endif