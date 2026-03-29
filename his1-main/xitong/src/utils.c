#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

/**
 * 内部静态函数：安全读取一行输入
 * 处理换行符并自动清理多余缓存
 */
static int read_input_line(char* buffer, size_t max_len) {
    if (fgets(buffer, (int)max_len, stdin) == NULL) {
        clearerr(stdin);
        return 0;
    }

    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    } else {
        clear_input_buffer(); // 如果输入超长，清理缓冲区
    }

    return 1;
}

/**
 * 内部静态函数：检查字符串尾部是否有非空白字符
 * 用于校验数值输入的合法性
 */
static int has_trailing_non_space(const char* text) {
    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 1;
        }
        text++;
    }
    return 0;
}

/**
 * 清理标准输入缓冲区
 * 防止残余的换行符干扰后续输入
 */
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * 安全获取字符串
 * 包含对读取失败的处理
 */
void safe_get_string(char* buffer, int max_len) {
    if (!read_input_line(buffer, (size_t)max_len) && max_len > 0) {
        buffer[0] = '\0';
    }
}

/**
 * 安全获取整数
 * 包含类型校验、范围校验及非法字符拦截
 */
int safe_get_int() {
    char input[64];
    long value;
    char* end_ptr;

    while (1) {
        if (!read_input_line(input, sizeof(input))) {
            return 0;
        }

        errno = 0;
        value = strtol(input, &end_ptr, 10);
        // 校验：输入不为空、无后续非法字符、未溢出、处于 INT 范围内
        if (end_ptr != input &&
            !has_trailing_non_space(end_ptr) &&
            errno != ERANGE &&
            value >= INT_MIN &&
            value <= INT_MAX) {
            return (int)value;
        }

        printf(COLOR_RED " 无效输入！请输入一个有效的整数: " COLOR_RESET);
    }
}

/**
 * 安全获取双精度浮点数
 * 专用于费用计算，防止财务数据录入错误
 */
double safe_get_double() {
    char input[64];
    double value;
    char* end_ptr;

    while (1) {
        if (!read_input_line(input, sizeof(input))) {
            return 0.0;
        }

        errno = 0;
        value = strtod(input, &end_ptr);
        if (end_ptr != input && !has_trailing_non_space(end_ptr) && errno != ERANGE) {
            return value;
        }

        printf(COLOR_RED " 无效金额！请输入一个有效的数字 (例如 12.5): " COLOR_RESET);
    }
}

/**
 * 暂停程序，等待用户确认
 * 常用于展示操作结果后（如挂号成功）
 */
void wait_for_enter() {
    char dummy[8];
    printf(COLOR_CYAN "\n请按回车键继续..." COLOR_RESET);
    read_input_line(dummy, sizeof(dummy));
}

/**
 * 打印带颜色的模块标题
 * 提升 UI 可读性
 */
void print_header(const char* title) {
    printf("\n" COLOR_BLUE "===================================================\n");
    printf("  %s\n", title);
    printf("===================================================" COLOR_RESET "\n");
}

/**
 * 打印分割线
 */
void print_divider() {
    printf("---------------------------------------------------\n");
}

/**
 * 展示系统初始化进度条
 * 模拟核心数据加载过程
 */
void show_progress_bar(const char* task_name) {
    const int total = 20;
    printf("%s: [", task_name);
    for (int i = 0; i <= total; i++) {
        printf(COLOR_GREEN "#" COLOR_RESET);
        fflush(stdout);
        // 简单的延时循环
        for (volatile long long j = 0; j < 10000000; j++);
    }
    printf("] 100%% 完成。\n");
}

/**
 * 获取当前系统日期字符串
 * 格式：YYYY-MM-DD，用于生成诊疗记录时间戳
 */
void get_current_time_str(char* time_buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_buffer, 32, "%Y-%m-%d", t);
}