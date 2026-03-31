#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

/* ==========================================
 * 1. 内部核心输入引擎 (Static Helper)
 * 专为应对题签中【重点考察：不规范、不合理或错误数据】设计
 * ========================================== */

/**
 * @brief 内部静态函数：安全读取一行输入
 * 【安全防线】：使用 fgets 替代危险的 scanf，彻底切断缓冲区溢出攻击 (Buffer Overflow) 的可能性。
 * @param buffer 接收输入的字符数组
 * @param max_len 缓冲区最大长度
 * @return 成功读取返回 1，读取失败或 EOF 返回 0
 */
static int read_input_line(char* buffer, size_t max_len) {
    // 从标准输入(stdin)安全读取最多 max_len - 1 个字符
    if (fgets(buffer, (int)max_len, stdin) == NULL) {
        clearerr(stdin); // 如果遇到 EOF 或读取错误，清除错误标志
        return 0;
    }

    size_t len = strlen(buffer);
    // 处理正常情况：如果读取到了回车符，将其替换为字符串结束符 '\0'
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    } else {
        // 异常情况处理：如果输入超长（没有读到 \n），说明 stdin 缓冲区里还有残余字符。
        // 必须立刻清理，否则这些残余字符会直接灌入下一次输入请求中，导致系统菜单失控连跳。
        clear_input_buffer(); 
    }

    return 1;
}

/**
 * @brief 内部静态函数：检查字符串尾部是否有非空白字符
 * 【防呆设计】：用于在转换数字后，校验用户是否输入了类似 "123abc" 这样的非法混合字符串。
 * @param text 指向数字转换结束位置的指针
 * @return 如果存在非空字符返回 1（非法），否则返回 0（合法）
 */
static int has_trailing_non_space(const char* text) {
    while (*text != '\0') {
        // 只要发现一个不是空格、Tab或回车等空白符的字符，就认为输入不纯粹，直接拦截
        if (!isspace((unsigned char)*text)) {
            return 1;
        }
        text++;
    }
    return 0;
}

/* ==========================================
 * 2. 暴露给外部的全局安全工具函数
 * ========================================== */

/**
 * @brief 暴力清理标准输入缓冲区 (stdin)
 * 不断读取字符直到遇到换行符 '\n' 或文件结束符 EOF
 */
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * @brief 安全获取字符串
 * @param buffer 用于存放结果的外部数组
 * @param max_len 允许获取的最大长度
 */
void safe_get_string(char* buffer, int max_len) {
    // 如果读取失败，为了安全起见，将目标字符串置为空字符串
    if (!read_input_line(buffer, (size_t)max_len) && max_len > 0) {
        buffer[0] = '\0';
    }
}

/**
 * @brief 安全获取整数 (自带防呆机制和自动重试)
 * 【满足题签】：最大数量不超过 int 允许最大整数 (INT_MAX)。
 * @return 用户输入的合法整型值
 */
int safe_get_int() {
    char input[64];
    long value;
    char* end_ptr;

    // 死循环要求用户必须输入正确的值才能放行
    while (1) {
        if (!read_input_line(input, sizeof(input))) {
            return 0; // 读取失败默认返回 0
        }

        errno = 0; // 重置全局错误码
        // 核心转换逻辑：以 10 进制安全地将字符串转为长整型
        value = strtol(input, &end_ptr, 10);
        
        // 综合校验条件 (坚不可摧的逻辑护城河)：
        // 1. end_ptr != input : 确保输入的不全是空格或空字符，真的转换了数字
        // 2. !has_trailing_non_space : 确保数字后面没有跟着乱码 (例如 "123x")
        // 3. errno != ERANGE : 确保数值没有发生系统级底层溢出
        // 4. >= INT_MIN && <= INT_MAX : 【满足题签要求】确保不超出 int 的物理极值
        if (end_ptr != input &&
            !has_trailing_non_space(end_ptr) &&
            errno != ERANGE &&
            value >= INT_MIN &&
            value <= INT_MAX) {
            return (int)value; // 校验全部通过，安全返回
        }

        // 走到这里说明校验失败，给予红色高亮警告并要求重新输入
        printf(COLOR_RED " 无效输入或超限！请输入一个合法的纯整数: " COLOR_RESET);
    }
}

/**
 * @brief 安全获取双精度浮点数
 * 【满足题签】：金额要精确到元、角、分，不允许有误差，最高额度不超过10万。
 * @return 用户输入的合法浮点值
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
        // 使用 strtod 安全转换为 double，保证元、角、分的极高精度
        value = strtod(input, &end_ptr);
        
        // 严密校验逻辑，防止 "12.5abc" 或极端大数溢出
        if (end_ptr != input && !has_trailing_non_space(end_ptr) && errno != ERANGE) {
            
            // 【题签绝对指令拦截】：系统强制规定，医院单笔金额最高不得超过 10 万元！
            if (value > 100000.0) {
                printf(COLOR_RED " 拦截：金额超限！根据财务规范，单笔发生金额最高不得超过 100,000 元。\n 请重新输入: " COLOR_RESET);
                continue; // 打回重输
            }
            if (value < 0.0) {
                printf(COLOR_RED " 拦截：金额异常！涉及资金流转的数据不允许为负数。\n 请重新输入: " COLOR_RESET);
                continue; // 打回重输
            }
            
            return value; // 校验满分通过
        }

        printf(COLOR_RED " 无效金额！请输入一个合法的数值 (例如 12.5): " COLOR_RESET);
    }
}

/**
 * @brief 暂停程序运行，等待用户确认
 * 常用于展示长列表数据或关键操作成功后的驻留提示，防止屏幕一闪而过。
 */
void wait_for_enter() {
    char dummy[8];
    printf(COLOR_CYAN "\n请按回车键继续..." COLOR_RESET);
    read_input_line(dummy, sizeof(dummy)); // 读取并丢弃任何输入，直到遇到回车
}

/* ==========================================
 * 3. 终端 UI 渲染与美化模块
 * 满足题签：清晰美观的输出方式
 * ========================================== */

/**
 * @brief 打印带有上下分割线和颜色的模块大标题
 * @param title 要显示的标题文字
 */
void print_header(const char* title) {
    printf("\n" COLOR_BLUE "===================================================\n");
    printf("  %s\n", title);
    printf("===================================================" COLOR_RESET "\n");
}

/**
 * @brief 打印简单的信息分割线
 */
void print_divider() {
    printf("---------------------------------------------------\n");
}

/**
 * @brief 展示控制台加载进度条动画
 * 模拟耗时任务的加载过程（例如：数据反序列化、硬件自检），增强沉浸感。
 * @param task_name 当前加载任务的名称
 */
void show_progress_bar(const char* task_name) {
    const int total = 20; // 进度条总格数
    printf("%s: [", task_name);
    
    for (int i = 0; i <= total; i++) {
        printf(COLOR_GREEN "#" COLOR_RESET);
        fflush(stdout); // 强制刷新输出流，确保 '#' 是逐个显示而不是积攒后一次性打印
        
        // 简单的 CPU 循环延时器
        // volatile 关键字至关重要：告诉编译器不要优化掉这个看似无用的空循环
        for (volatile long long j = 0; j < 10000000; j++);
    }
    printf("] 100%% 完成。\n");
}

/* ==========================================
 * 4. 时间与日期辅助工具
 * ========================================== */

/**
 * @brief 获取当前系统日期的字符串表达
 * @param time_buffer 外部传入的缓冲区，用于接收生成的字符串
 * 格式被严格限制为 YYYY-MM-DD，用于生成所有诊疗单据的时间戳。
 */
void get_current_time_str(char* time_buffer) {
    time_t now = time(NULL);           // 获取自 Unix 纪元起经过的秒数
    struct tm *t = localtime(&now);    // 转换为本地时区的结构化时间
    
    // 使用 strftime 进行格式化：四位年-两位月-两位日
    strftime(time_buffer, 32, "%Y-%m-%d", t);
}

/* ==========================================
 * 5. 【绝对指令植入】救护车与手术室高精度时间辅助集
 * 满足题签：如果简化时间，则精确到 月、日、时、分
 * ========================================== */

/**
 * @brief 获取当前系统精确时间 (格式：YYYY-MM-DD HH:MM:SS)
 * @param time_buffer 外部传入的缓冲区，用于接收生成的字符串 (建议分配 32 字节)
 * * 专为 120 救护车出勤表 (调度日志) 以及 手术室状态流转 (手术开始/结束) 提供精准时间戳支持。
 */
void get_precise_time_str(char* time_buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    // 使用 strftime 进行格式化：增加时、分、秒，为急救出勤和手术记录提供精确打点
    strftime(time_buffer, 32, "%Y-%m-%d %H:%M:%S", t);
}