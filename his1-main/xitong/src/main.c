#include "common.h"
#include "data_manager.h"
#include "patient.h"
#include "doctor.h"
#include "admin.h"
#include "ambulance.h" // 新增：救护车调度头文件
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

/* ==========================================
 * 主界面 UI 渲染 (已适配智慧医疗全闭环功能)
 * ========================================== */
void show_main_menu() {
    clear_input_buffer();
    // 版本号升级至 v3.0，标志着具备院前急救与重症转办能力
    print_header("智慧医院全链路管理系统 (HIS) v3.0");
    
    printf(COLOR_CYAN "  请选择业务终端以进入：\n" COLOR_RESET);
    
    // 菜单提示文字更新，体现急诊、ICU 和救护车等新特性
    printf("  [ 1 ] 患者服务大厅 (含急诊挂号、ICU 状态查询、救护车呼叫)\n");
    printf("  [ 2 ] 医护人员工作站 (含呼叫叫号、处方核销、ICU 转院申请)\n");
    printf("  [ 3 ] 决策支持中心 (管理医生/药品、财务报表、床位优化)\n");
    printf("  [ 4 ] 120 急救调度中心 (救护车实时派发、状态监控、回场消毒)\n"); // 新增模块
    printf(COLOR_RED "  [ 0 ] 安全关闭系统 (保存财务、库存与急救数据)\n" COLOR_RESET);
    
    print_divider();
    printf(COLOR_YELLOW "请输入服务编号 (0-4)：" COLOR_RESET);
}

/* ==========================================
 * 程序主入口
 * ========================================== */
int main() {
    // 强制设置控制台编码，确保中文与 ANSI 颜色正常显示
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    // 1. 系统预热与模块加载
    printf("\n" COLOR_BLUE "HIS 智慧医疗平台正在启动...\n" COLOR_RESET);
    show_progress_bar("初始化核心医疗模块与救护车状态");
    
    // 2. 核心数据持久化加载
    // 此时 init_system_data 应已包含 load_ambulances()
    init_system_data(); 
    printf(COLOR_GREEN "\n全局数据库加载成功！按回车键进入管理控制台..." COLOR_RESET);
    wait_for_enter();

    // 3. 全局事件主循环
    int choice = -1;
    while (1) {
        show_main_menu();
        
        // 采用 utils.c 提供的防御性输入接口，防止输入字母崩溃
        choice = safe_get_int();
        
        switch (choice) {
            case 1:
                patient_subsystem(); // 已集成急诊挂号与 120 呼叫
                break;
                
            case 2:
                doctor_subsystem();  // 已集成 ICU 转院决策
                break;
                
            case 3:
                admin_subsystem();   // 行政管理与床位监控
                break;
                
            case 4:
                ambulance_subsystem(); // 新增：独立的救护车调度管理模块
                break;
                
            case 0:
                // 4. 数据保存流程：新增对救护车任务与 ICU 床位状态的保存
                printf("\n" COLOR_YELLOW "正在同步业务数据 (处方时效、救护车轨迹、账户余额) 至本地文件...\n" COLOR_RESET);
                show_progress_bar("正在写入持久化存储");
                
                // save_system_data 应已包含对救护车信息的写入
                save_system_data(); 
                free_all_data();    // 释放全链表节点，保障内存安全
                
                printf(COLOR_GREEN "\n所有业务数据已安全保存。HIS 系统已安全退出。\n" COLOR_RESET);
                exit(0);
                
            default:
                printf(COLOR_RED "\n错误：请输入 0-4 之间的有效功能编号。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
    
    return 0; 
}