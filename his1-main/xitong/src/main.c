#include "common.h"
#include "data_manager.h"
#include "patient.h"
#include "doctor.h"
#include "admin.h"
#include "ambulance.h" // 救护车调度头文件，引入 120 急救子系统
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

/* ==========================================
 * 主界面 UI 渲染 (已适配智慧医疗全闭环功能)
 * 负责向用户展示系统的主控面板。
 * 【绝对指令植入】：在主菜单全面明示手术室监控与救护车出勤日志的入口。
 * ========================================== */
void show_main_menu() {
    clear_input_buffer(); // 清理输入缓冲区，防止残留字符导致菜单死循环跳动
    
    // 版本号升级至 v3.0，标志着具备院前急救、重症转办与手术室精细管理能力
    print_header("智慧医院全链路管理系统 (HIS) v3.0");
    
    printf(COLOR_CYAN "  请选择业务终端以进入：\n" COLOR_RESET);
    
    // 菜单提示文字更新，全面反映急诊、ICU、手术室和救护车出勤表等新特性
    printf("  [ 1 ] 患者服务大厅 (含急诊挂号、ICU/手术室状态查询、救护车呼叫)\n");
    printf("  [ 2 ] 医护人员工作站 (含呼叫叫号、处方核销、手术室实时联动查看)\n");
    printf("  [ 3 ] 决策支持中心 (管理医生、排班、查账、手术室全盘调度)\n");
    printf("  [ 4 ] 120 急救调度中心 (救护车实时派发、出勤日志查询、回场消毒)\n"); // 独立调配急救资源
    printf(COLOR_RED "  [ 0 ] 安全关闭系统 (保存财务、库存与急救/手术室数据)\n" COLOR_RESET);
    
    print_divider();
    printf(COLOR_YELLOW "请输入服务编号 (0-4)：" COLOR_RESET);
}

/* ==========================================
 * 程序主入口 (Main Entry Point)
 * HIS 系统的总控枢纽，负责初始化、主循环路由以及安全退出。
 * ========================================== */
int main() {
    // 强制设置控制台编码为 UTF-8 (65001)
    // 这是为了确保中文输出以及 ANSI 颜色控制符能在 Windows 终端下正常显示，防止乱码
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    // 1. 系统预热与 UI 反馈
    printf("\n" COLOR_BLUE "HIS 智慧医疗平台正在启动...\n" COLOR_RESET);
    
    // 【绝对指令植入】：更新加载提示，明示加载手术室与出勤表缓存
    show_progress_bar("初始化核心医疗模块、救护车出勤系统与手术室缓存"); 
    
    // 2. 核心数据持久化加载
    // 调用底层数据管理器，从本地 txt 文件反序列化所有的链表数据进内存
    // 此时 init_system_data 应已包含 load_ambulances() 以及初始化手术室的逻辑
    init_system_data(); 
    printf(COLOR_GREEN "\n全局数据库加载成功！按回车键进入管理控制台..." COLOR_RESET);
    wait_for_enter();

    // 3. 全局事件主循环
    int choice = -1;
    while (1) { // 无限循环，直到用户主动选择 0 退出
        show_main_menu();
        
        // 采用 utils.c 提供的防御性输入接口
        // 防止用户输入英文字母或其他非法字符导致 scanf 崩溃
        choice = safe_get_int();
        
        // 路由分发器 (Router)：根据用户输入的编号，进入对应的子系统
        switch (choice) {
            case 1:
                patient_subsystem(); // 进入患者端：已集成急诊挂号、120 呼叫及手术状态联动
                break;
                
            case 2:
                doctor_subsystem();  // 进入医生端：已集成 ICU 转院决策、开处方及查看手术室
                break;
                
            case 3:
                admin_subsystem();   // 进入管理端：行政管理、财务报表、床位监控及手术室排班
                break;
                
            case 4:
                ambulance_subsystem(); // 进入急救端：独立的救护车调度及出勤日志管理模块
                break;
                
            case 0:
                // 4. 数据保存与安全退出流程 (Graceful Shutdown)
                // 【绝对指令植入】：新增对救护车任务轨迹与手术室状态的保存/释放保障提示
                printf("\n" COLOR_YELLOW "正在同步业务数据 (处方时效、救护车出勤轨迹、手术室状态、账户余额) 至本地文件...\n" COLOR_RESET);
                show_progress_bar("正在写入持久化存储");
                
                // 将内存中被修改过的所有链表数据序列化写回本地磁盘
                // save_system_data 会处理救护车等持久化逻辑
                save_system_data(); 
                
                // 释放所有链表节点，归还操作系统内存，保障程序长期运行或退出时无内存泄露
                // 底层的 free_all_data 已被修改，包含了释放新增的手术室和出勤日志节点
                free_all_data();    
                
                printf(COLOR_GREEN "\n所有业务数据已安全保存。HIS 系统已安全退出。\n" COLOR_RESET);
                exit(0); // 正常结束进程
                
            default:
                // 容错处理：处理 0-4 之外的越界或非法输入
                printf(COLOR_RED "\n错误：请输入 0-4 之间的有效功能编号。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
    
    return 0; 
}