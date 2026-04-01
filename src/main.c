#include "common.h"
#include "data_manager.h"
#include "patient.h"
#include "doctor.h"
#include "admin.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

/* ==========================================
 * 主界面 UI 渲染
 * ========================================== */
void show_main_menu() {
    clear_input_buffer(); 
    // 版本号升级，体现系统功能迭代
    print_header("全功能医疗管理系统 (HIS) v2.0"); 
    
    printf(COLOR_CYAN "  请选择您的登录身份：\n" COLOR_RESET);
    // 菜单提示文字更新，精准反映我们新增的核心功能
    printf("  [ 1 ] 患者终端 (充值缴费、医保报销、就诊记录)\n");
    printf("  [ 2 ] 医护终端 (叫号、分类处方、药房发药核销)\n");
    printf("  [ 3 ] 管理终端 (医生管理、药品入库、财务报表)\n");
    printf(COLOR_RED "  [ 0 ] 安全退出系统\n" COLOR_RESET);
    
    print_divider();
    printf(COLOR_YELLOW "请输入选项 (0-3)：" COLOR_RESET);
}

/* ==========================================
 * 程序主入口
 * ========================================== */
int main() {
    // 设置控制台为 UTF-8 编码，防止 Windows 下中文乱码
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    // 1. 系统启动与界面预热
    printf("\n" COLOR_BLUE "系统启动中，正在准备加载数据库...\n" COLOR_RESET);
    show_progress_bar("初始化核心模块"); 
    
    // 2. 加载所有的初始数据到内存链表中
    init_system_data(); 
    printf(COLOR_GREEN "\n数据加载成功！按回车键进入控制台..." COLOR_RESET);
    wait_for_enter();

    // 3. 主事件循环 (Main Event Loop)
    int choice = -1;
    while (1) {
        show_main_menu();
        
        // 使用防御性输入接口
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
                
            case 0:
                // 4. 安全退出流程：数据持久化
                // 提示文案更新，提醒用户关键的财务和库存数据正在保存
                printf("\n" COLOR_YELLOW "正在将业务数据 (账户余额、处方状态、药品库存) 保存到本地...\n" COLOR_RESET);
                show_progress_bar("数据同步中");
                
                save_system_data(); 
                free_all_data();    // 释放内存，防止泄漏
                
                printf(COLOR_GREEN "\n数据已安全保存。感谢您的使用！再见。\n" COLOR_RESET);
                exit(0);
                
            default:
                printf(COLOR_RED "\n无效的选项。请输入 0-3 之间的数字。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
    
    return 0; 
}