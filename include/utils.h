#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==========================================
 * 1. UI 美化与控制台颜色宏定义 (ANSI 转义序列)
 * ========================================== */
#ifndef COLOR_RESET
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"      // 用于错误提示、库存告警
#define COLOR_GREEN   "\033[32m"      // 用于操作成功、数据加载完成
#define COLOR_YELLOW  "\033[33m"      // 用于重要输入提示
#define COLOR_BLUE    "\033[34m"      // 用于菜单大标题
#define COLOR_CYAN    "\033[36m"      // 用于普通信息、进度条
#endif

/* ==========================================
 * 2. 防御性输入接口 (Robust Input Handling)
 * ========================================== */

// 清空输入缓冲区残留的换行符
void clear_input_buffer();

// 安全读取字符串：自动处理换行符，防止缓冲区溢出
void safe_get_string(char* buffer, int max_len);

// 安全读取整数：若输入非数字字符会循环提示，绝不崩溃
int safe_get_int();

// 安全读取浮点数：用于金额、费用的安全输入
double safe_get_double();

// 暂停程序，等待用户按回车键继续
void wait_for_enter();

/* ==========================================
 * 3. 界面渲染与辅助工具
 * ========================================== */

// 打印统一风格的菜单头部 (带装饰线)
void print_header(const char* title);

// 打印统一风格的菜单底部隔离线
void print_divider();

// 模拟系统加载进度条动画 (提升视觉高级感)
void show_progress_bar(const char* task_name);

// 获取当前系统时间字符串 
// 【注意】：格式必须为 "YYYY-MM-DD"，严禁带空格，否则会破坏 txt 文件的 sscanf 读取逻辑！
void get_current_time_str(char* time_buffer);

#endif // UTILS_H