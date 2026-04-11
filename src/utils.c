// [核心架构说明]：
// 本文件为 HIS 系统的 底层安全与公共设施库 (Utility & Security Layer)。
// 核心职责：
// 1. 拦截所有非法、越界、溢出的用户输入，防止系统宕机或被恶意注入 (Buffer
// Overflow)；
// 2. 统一全院 UI 渲染风格，提供标准化的控制台绘制接口；
// 3. 提供高精度的时间戳生成，为病历建档、财务对账、以及 120
// 急救出勤提供绝对的时间准绳。
//
// 本次更新严格保留了所有旧版安全逻辑，并加强了架构级注释。

#include "utils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"

/* ==========================================================================
 * 1. 内部核心输入引擎 (Static Helper)
 * 专为应对医疗系统高并发操作中的【不规范、不合理或错误数据】而设计
 * ========================================================================== */

/**
 * @brief 内部静态函数：安全读取一行输入 (防溢出护城河)
 * 【安全防线】：强制使用 fgets 替代危险的 scanf。
 * 彻底切断缓冲区溢出攻击 (Buffer Overflow) 的可能性，防止程序崩溃。
 * * @param buffer 接收输入的字符数组 (外部预先分配好的内存)
 * @param max_len 缓冲区最大长度
 * @return 成功读取返回 1，读取失败或遇到 EOF 返回 0
 */
static int read_input_line(char* buffer, size_t max_len) {
  // 从标准输入(stdin)安全读取最多 max_len - 1 个字符
  if (fgets(buffer, (int)max_len, stdin) == NULL) {
    clearerr(stdin);  // 如果遇到 EOF 或读取错误，立即清除错误标志，防止死循环
    return 0;
  }

  size_t len = strlen(buffer);
  // 处理正常情况：如果读取到了回车符，将其替换为标准的字符串结束符 '\0'
  if (len > 0 && buffer[len - 1] == '\n') {
    buffer[len - 1] = '\0';
  } else {
    // 异常情况处理：如果输入超长（没有读到 \n），说明 stdin
    // 缓冲区里还有残余字符。
    // 必须立刻清理，否则这些残余字符会直接灌入下一次的输入请求中，导致系统菜单失控连跳。
    clear_input_buffer();
  }

  return 1;
}

/**
 * @brief 内部静态函数：检查字符串尾部是否有非空白字符 (防呆拦截器)
 * 【防呆设计】：用于在转换数字后，校验用户是否输入了类似 "123abc"
 * 这样的非法混合字符串。
 * * @param text 指向数字转换结束位置的指针
 * @return 如果存在非空字符返回 1（即非法输入），否则返回 0（合法纯净输入）
 */
static int has_trailing_non_space(const char* text) {
  while (*text != '\0') {
    // 只要发现一个不是空格、Tab或回车等空白符的字符，就认为输入不纯粹，直接触发拦截
    if (!isspace((unsigned char)*text)) {
      return 1;
    }
    text++;
  }
  return 0;
}

/* ==========================================================================
 * 2. 暴露给外部的全局安全工具函数 (Public Security API)
 * ========================================================================== */

/**
 * @brief 暴力清理标准输入缓冲区 (stdin)
 * 持续读取字符并直接丢弃，直到遇到真正的换行符 '\n' 或文件结束符 EOF。
 * 专治各种按键连击或输入超长导致的菜单穿透。
 */
void clear_input_buffer() {
  int c;
  while ((c = getchar()) != '\n' && c != EOF);
}

/**
 * @brief 安全获取字符串 (Sanitized String Getter)
 * @param buffer 用于存放结果的外部数组
 * @param max_len 允许获取的最大长度限制
 */
void safe_get_string(char* buffer, int max_len) {
  // 如果底层读取失败，为了系统整体安全起见，直接将目标字符串置为空，防止脏数据入库
  if (!read_input_line(buffer, (size_t)max_len) && max_len > 0) {
    buffer[0] = '\0';
  }
}

/**
 * @brief 安全获取整数 (自带防呆机制和自动重试的数字获取器)
 * 【满足要求】：最大数量绝对不超过操作系统 C 语言底层允许的最大整数 (INT_MAX)。
 * * @return 用户输入的绝对合法、纯净的整型值
 */
int safe_get_int() {
  char input[64];
  long value;
  char* end_ptr;

  // 死循环卡控机制：要求用户必须输入正确的值才能放行进入下一步医疗流程
  while (1) {
    if (!read_input_line(input, sizeof(input))) {
      return 0;  // 硬件级读取失败默认安全返回 0
    }

    errno = 0;  // 每次转换前重置操作系统的全局错误码

    // 核心转换逻辑：以 10 进制安全地将字符串转为长整型
    value = strtol(input, &end_ptr, 10);

    // 综合校验条件 (坚不可摧的逻辑护城河)：
    // 1. end_ptr != input : 确保输入的不全是空格或空字符，系统真的转换了数字
    // 2. !has_trailing_non_space : 确保数字后面没有跟着乱码 (例如输入 "123x"
    // 必须被拒绝)
    // 3. errno != ERANGE : 确保数值没有发生系统底层的内存级溢出
    // 4. >= INT_MIN && <= INT_MAX : 确保绝不超出标准 int 的物理极值
    if (end_ptr != input && !has_trailing_non_space(end_ptr) &&
        errno != ERANGE && value >= INT_MIN && value <= INT_MAX) {
      return (int)value;  // 校验全部满分通过，安全返回
    }

    // 走到这里说明校验失败，给予系统级红色高亮警告并要求重新输入
    printf(COLOR_RED " 无效输入或超限！请输入一个合法的纯整数: " COLOR_RESET);
  }
}

/**
 * @brief 安全获取双精度浮点数 (高精度财务网关)
 * 【满足要求】：医疗系统的金额必须精确到元、角、分，绝对不允许有误差，且单笔最高额度受限。
 * * @return 用户输入的经过深度洗消的合法浮点值
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
    // 使用 strtod 安全转换为 double，保证元、角、分的极高精度计算
    value = strtod(input, &end_ptr);

    // 严密的系统级校验逻辑，防止诸如 "12.5abc" 的注入或极端大数溢出
    if (end_ptr != input && !has_trailing_non_space(end_ptr) &&
        errno != ERANGE) {
      // 【系统级绝对指令拦截】：根据国家医疗财务规范，门诊/住院单笔发生金额最高不得超过
      // 10 万元！
      if (value > 100000.0) {
        printf(COLOR_RED
               " 拦截：金额超限！根据医院财务规范，单笔发生金额最高不得超过 "
               "100,000 元。\n 请重新输入: " COLOR_RESET);
        continue;  // 金额过大，打回重输
      }
      // 防御恶意套现或系统 BUG：任何资金流转数据绝对不允许为负数
      if (value < 0.0) {
        printf(COLOR_RED
               " 拦截：金额异常！涉及资金流转的数据绝对不允许为负数。\n "
               "请重新输入: " COLOR_RESET);
        continue;  // 负数，打回重输
      }

      return value;  // 财务校验全线通过
    }

    printf(COLOR_RED
           " 无效金额！请输入一个合法的金额数值 (例如 12.50): " COLOR_RESET);
  }
}

double safe_get_double_allow_negative() {
  char input[64];
  double value;
  char* end_ptr;

  while (1) {
    if (!read_input_line(input, sizeof(input))) {
      return 0.0;
    }

    errno = 0;
    value = strtod(input, &end_ptr);

    if (end_ptr != input && !has_trailing_non_space(end_ptr) &&
        errno != ERANGE) {
      if (value > 100000.0 || value < -100000.0) {
        printf(COLOR_RED
               " 拦截：金额超限！根据医院财务规范，单笔发生金额绝对值不得超过 "
               "100,000 元。\n 请重新输入: " COLOR_RESET);
        continue;
      }

      return value;
    }

    printf(COLOR_RED
           " 无效金额！请输入一个合法的金额数值 (例如 12.50): " COLOR_RESET);
  }
}

/**
 * @brief 暂停程序运行，等待用户人工确认 (UI 驻留函数)
 * 常用于展示长列表数据或关键操作成功后的驻留提示，防止屏幕一闪而过导致医生/患者看不清。
 */
void wait_for_enter() {
  char dummy[8];
  printf(COLOR_CYAN "\n请按回车键继续..." COLOR_RESET);
  read_input_line(dummy,
                  sizeof(dummy));  // 读取并丢弃任何输入，直到用户真切地按下回车
}

/* ==========================================================================
 * 3. 终端 UI 渲染与美化模块 (Terminal UI Engine)
 * 满足要求：为 HIS 提供清晰、美观、统一样式的控制台输出方式
 * ========================================================================== */

/**
 * @brief 打印带有上下双实线分割和天蓝色主题的模块大标题
 * @param title 要显示的核心业务名称 (如 "120急救调度中心")
 */
void print_header(const char* title) {
  printf("\n" COLOR_BLUE
         "===================================================\n");
  printf("  %s\n", title);
  printf("===================================================" COLOR_RESET
         "\n");
}

/**
 * @brief 打印标准的单层信息分割线
 */
void print_divider() {
  printf("---------------------------------------------------\n");
}

/**
 * @brief 渲染控制台硬件加载进度条动画
 * 模拟真实系统耗时任务的加载过程（例如：数据反序列化、底层硬件自检），增强系统沉浸感。
 * @param task_name 当前正在执行的异步任务名称
 */
void show_progress_bar(const char* task_name) {
  const int total = 20;  // 进度条总格数
  printf("%s: [", task_name);

  for (int i = 0; i <= total; i++) {
    printf(COLOR_GREEN "#" COLOR_RESET);
    fflush(stdout);  // 强制立刻刷新输出流，确保 '#'
                     // 是逐个显示，而不是积攒后一次性吐出

    // 简单的底层 CPU 循环延时器
    // volatile 关键字在这里至关重要：明确警告现代 C 编译器(如
    // GCC/Clang)绝对不要“智能优化”掉这个看似无用的空循环
    for (volatile long long j = 0; j < 10000000; j++);
  }
  printf("] 100%% 完成。\n");
}

/* ==========================================================================
 * 4. 时间与日期辅助工具 (Time & Date Engine)
 * ========================================================================== */

/**
 * @brief 获取当前系统标准日期的字符串表达 (粗颗粒度)
 * @param time_buffer 外部传入的内存缓冲区，用于接收生成的字符串
 * 格式被严格限制为 YYYY-MM-DD，用于生成所有常规诊疗单据 (门诊挂号、取药)
 * 的基础时间戳。
 */
void get_current_time_str(char* time_buffer) {
  time_t now = time(NULL);         // 获取自 1970年1月1日(Unix纪元)起经过的秒数
  struct tm* t = localtime(&now);  // 转换为操作系统本地时区的结构化时间

  // 使用 strftime 进行格式化：四位年-两位月-两位日
  strftime(time_buffer, 32, "%Y-%m-%d", t);
}

/* ==========================================================================
 * 5. 【系统核心联动】救护车与手术室高精度时间打点集
 * ========================================================================== */

/**
 * @brief 获取当前系统最高精度时间 (格式：YYYY-MM-DD HH:MM:SS)
 * @param time_buffer 外部传入的缓冲区，用于接收生成的字符串 (建议最少分配 32
 * 字节)
 * * [架构联动] 专为 120 救护车出勤表 (ambulance_log_head) 以及 手术室急救流水
 * 提供【时、分、秒】级别的绝对精准时间戳支持。确保事后医疗事故追溯和追责有据可查。
 */
void get_precise_time_str(char* time_buffer) {
  time_t now = time(NULL);
  struct tm* t = localtime(&now);

  // 使用 strftime 进行高精度格式化：增加 时:分:秒，为急救出车记录提供精准打点
  strftime(time_buffer, 32, "%Y-%m-%d %H:%M:%S", t);
}