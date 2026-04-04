#include "common.h"
#include "data_manager.h"
#include "patient.h"
#include "doctor.h"
#include "admin.h"
#include "ambulance.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static void show_main_menu(void) {
    print_header("医院信息系统主菜单");
    printf("1. 患者端\n");
    printf("2. 医生端\n");
    printf("3. 管理端\n");
    printf("4. 急救中心\n");
    printf("0. 保存并退出\n");
    print_divider();
    printf("请选择: ");
}

int main(void) {
    int choice = -1;

    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    printf(COLOR_BLUE "正在启动医院信息系统...\n" COLOR_RESET);
    show_progress_bar("正在加载系统数据");
    init_system_data();

    while (1) {
        show_main_menu();
        choice = safe_get_int();

        switch (choice) {
            case 1:
                patient_subsystem();
                break;
            case 2:
                doctor_subsystem();
                break;
            case 3:
                admin_subsystem();
                break;
            case 4:
                ambulance_subsystem();
                break;
            case 0:
                printf(COLOR_YELLOW "正在保存数据...\n" COLOR_RESET);
                show_progress_bar("正在保存系统数据");
                save_system_data();
                free_all_data();
                printf(COLOR_GREEN "系统已安全退出。\n" COLOR_RESET);
                return 0;
            default:
                printf(COLOR_RED "无效选项。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}

