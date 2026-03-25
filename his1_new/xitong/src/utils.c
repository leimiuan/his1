#include "common.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

static int read_input_line(char* buffer, size_t max_len) {
    if (fgets(buffer, (int)max_len, stdin) == NULL) {
        clearerr(stdin);
        return 0;
    }

    {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        } else {
            clear_input_buffer();
        }
    }

    return 1;
}

static int has_trailing_non_space(const char* text) {
    while (*text != '\0') {
        if (!isspace((unsigned char)*text)) {
            return 1;
        }
        text++;
    }
    return 0;
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void safe_get_string(char* buffer, int max_len) {
    if (!read_input_line(buffer, (size_t)max_len) && max_len > 0) {
        buffer[0] = '\0';
    }
}

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

void wait_for_enter() {
    char dummy[8];
    printf(COLOR_CYAN "\n请按回车键继续..." COLOR_RESET);
    read_input_line(dummy, sizeof(dummy));
}

void print_header(const char* title) {
    printf("\n" COLOR_BLUE "===================================================\n");
    printf("  %s\n", title);
    printf("===================================================" COLOR_RESET "\n");
}

void print_divider() {
    printf("---------------------------------------------------\n");
}

void show_progress_bar(const char* task_name) {
    const int total = 20;
    printf("%s: [", task_name);
    for (int i = 0; i <= total; i++) {
        printf(COLOR_GREEN "#" COLOR_RESET);
        fflush(stdout);
        for (volatile long long j = 0; j < 10000000; j++);
    }
    printf("] 100%% 完成。\n");
}

void get_current_time_str(char* time_buffer) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_buffer, 32, "%Y-%m-%d", t);
}
