#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==========================================
 * 1. UI 美化与控制台颜色宏定义 (ANSI 转义序列)
 * 目标：提供多级视觉反馈，如红色的“告警”、绿色的“成功”
 * ========================================== */
#ifndef COLOR_RESET
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"    // 用于错误提示、余额不足、库存告警、急诊标记
#define COLOR_GREEN   "\033[1;32m"    // 用于支付成功、挂号完成、救护车出发
#define COLOR_YELLOW  "\033[1;33m"    // 用于重要录入提示、待处理状态
#define COLOR_BLUE    "\033[1;34m"    // 用于系统主标题、电子发票边框
#define COLOR_CYAN    "\033[1;36m"    // 用于排队信息、进度条、普通提示信息
#endif

/* ==========================================
 * 2. 防御性输入接口 (Robust Input Handling)
 * 目标：彻底杜绝 scanf 导致的非法输入死循环
 * ========================================== */

/**
 * 清空标准输入缓冲区 (stdin)
 * 移除残余的换行符和无效字符，防止干扰后续读取
 */
void clear_input_buffer();

/**
 * 安全读取字符串
 * 自动剥离末尾换行符，并进行溢出保护
 * @param buffer 目标缓冲区
 * @param max_len 缓冲区最大长度
 */
void safe_get_string(char* buffer, int max_len);

/**
 * 安全读取整数
 * 具备非数字字符拦截功能，若输入错误会持续提示直至正确
 * @return 转换后的合法整数
 */
int safe_get_int();

/**
 * 安全读取双精度浮点数
 * 专用于费用和充值金额录入，确保财务数据精准
 * @return 转换后的合法双精度数值
 */
double safe_get_double();

/**
 * 暂停程序执行
 * 在屏幕显示“请按回车键继续...”，常用于展示操作结果后
 */
void wait_for_enter();

/* ==========================================
 * 3. 界面渲染与系统辅助工具
 * ========================================== */

/**
 * 打印带装饰线的模块头部
 * @param title 模块标题 (如 "120 救护车调度中心")
 */
void print_header(const char* title);

/**
 * 打印标准化的列表隔离线
 */
void print_divider();

/**
 * 模拟系统加载进度条动画
 * 用于数据持久化保存或系统初始化，增加用户交互沉浸感
 * @param task_name 当前执行的任务描述
 */
void show_progress_bar(const char* task_name);

/**
 * 获取符合系统持久化规范的日期字符串 
 * 格式要求：必须为 "YYYY-MM-DD"
 * 【注意】：严禁包含空格或特殊字符，否则会破坏 data_manager 的解析逻辑
 * @param time_buffer 接收日期的缓冲区 (建议长度 >= 32)
 */
void get_current_time_str(char* time_buffer);

/* ==========================================
 * 4. 【绝对指令植入】救护车与手术室高精度时间辅助集
 * ========================================== */

/**
 * @brief 获取当前系统精确时间 (格式：YYYY-MM-DD HH:MM:SS)
 * @param time_buffer 外部传入的缓冲区，用于接收生成的字符串 (建议分配 32 字节)
 * * 专为 120 救护车出勤表 (调度日志) 以及 手术室状态流转 (手术开始/结束) 提供秒级精准时间戳支持。
 */
void get_precise_time_str(char* time_buffer);

#endif // UTILS_H