#include "common.h"  // 确保颜色宏能够正常加载
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==========================================
 * 1. 终极防御性输入接口 (针对测试工程师逻辑)
 * ========================================== */

// 清空输入缓冲区
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// 安全读取字符串：防止缓冲区溢出 
void safe_get_string(char* buffer, int max_len) {
    if (fgets(buffer, max_len, stdin) != NULL) {
        // 去掉 fgets 自动读取的换行符
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        } else {
            // 如果输入超长，清空剩余缓冲区，防止污染下次输入
            clear_input_buffer();
        }
    }
}

// 安全读取整数：对抗非法输入
int safe_get_int() {
    char input[64];
    int value;
    while (1) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // 使用 sscanf 尝试解析，若失败则说明是非数字字符
            if (sscanf(input, "%d", &value) == 1) {
                return value;
            }
        }
        printf(COLOR_RED " 无效输入！请输入一个有效的整数: " COLOR_RESET);
    }
}

// 安全读取浮点数：用于金额结算
double safe_get_double() {
    char input[64];
    double value;
    while (1) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            if (sscanf(input, "%lf", &value) == 1) {
                return value;
            }
        }
        printf(COLOR_RED " 无效金额！请输入一个有效的数字 (例如 12.5): " COLOR_RESET);
    }
}

// 人性化停顿 
void wait_for_enter() {
    printf(COLOR_CYAN "\n请按回车键继续..." COLOR_RESET);
    // 这种清理+等待的组合可以确保各种环境下都能正确停顿
    getchar(); 
}

/* ==========================================
 * 2. 界面渲染与美化 
 * ========================================== */

void print_header(const char* title) {
    // 结合 ANSI 颜色代码输出美观的标题 
    printf("\n" COLOR_BLUE "===================================================\n");
    printf("  %s\n", title);
    printf("===================================================" COLOR_RESET "\n");
}

void print_divider() {
    printf("---------------------------------------------------\n");
}

// 模拟进度条：增加系统启动和保存时的仪式感
void show_progress_bar(const char* task_name) {
    const int total = 20;
    printf("%s: [", task_name);
    for (int i = 0; i <= total; i++) {
        // 使用标准的 ASCII 字符代表填充，避免部分终端不支持复杂 Unicode
        printf(COLOR_GREEN "#" COLOR_RESET);
        fflush(stdout); 
        
        // 简单的延时模拟动画效果
        for(volatile long long j = 0; j < 10000000; j++); 
    }
    printf("] 100%% 完成！\n");
}

/* ==========================================
 * 3. 时间工具
 * ========================================== */

void get_current_time_str(char* time_buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // 【关键修复】：改为纯日期格式 (YYYY-MM-DD)，去掉原先的空格
    // 否则带空格的时间字符串存入 description 后，会导致 data_manager.c 的 sscanf 解析截断报错！
    strftime(time_buffer, 32, "%Y-%m-%d", t);
}