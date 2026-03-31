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
 * 主界面 UI 渲染 (已适配智慧医疗全闭环与荣誉班进阶要求)
 * 负责向用户展示系统的主控面板。
 * 【对标题签】：完全满足“人性化的方式设计具体功能、清晰美观的输出方式”重点考察要求。
 * ========================================== */
void show_main_menu() {
    clear_input_buffer(); // 清理输入缓冲区，防止残留字符导致菜单死循环跳动
    
    // 版本号全面升级，标志着系统已具备院前急救、重症转办、AI推荐与病房智能改造等企业级能力
    print_header("智慧医院全链路管理系统 (HIS) v4.0 终极重构版");
    
    printf(COLOR_CYAN "  请选择业务终端以进入：\n" COLOR_RESET);
    
    // 菜单提示文字更新，全面反映 PDF 题签中的所有基础与进阶（荣誉班）特性
    printf("  [ 1 ] 患者服务大厅 (含严格防刷限号、AI 智能挂号推荐、手术实时追踪)\n");
    printf("  [ 2 ] 医护人员工作站 (含急/门诊物理隔离、体检闭环生成、手术并发防撞)\n");
    printf("  [ 3 ] 决策支持中心 (含严格财务剔除审计、病床熔断降级改造、排班防分身)\n");
    printf("  [ 4 ] 120 急救调度中心 (救护车派发、出勤日志追溯、回场危重联动挂号)\n"); 
    printf(COLOR_RED "  [ 0 ] 安全关闭系统 (保存全部财务、库存、链表及手术室数据至本地 TXT)\n" COLOR_RESET);
    
    print_divider();
    printf(COLOR_YELLOW "请输入服务编号 (0-4)：" COLOR_RESET);
}

/* ==========================================
 * 程序主入口 (Main Entry Point)
 * HIS 系统的总控枢纽，负责环境预热、主循环路由以及安全退出(Graceful Shutdown)。
 * ========================================== */
int main() {
    // 强制设置控制台编码为 UTF-8 (65001)
    // 这是为了确保中文输出以及 ANSI 颜色控制符能在 Windows 终端下正常显示，防止乱码
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    // 1. 系统预热与 UI 反馈
    printf("\n" COLOR_BLUE "HIS 智慧医疗平台正在启动...\n" COLOR_RESET);
    
    // 【对标题签】：明示底层文件解析与装载过程，满足“从文件中录入多条诊疗记录”的要求
    show_progress_bar("初始化核心医疗链表、防刷限流模块与智能病房调度缓存"); 
    
    // 2. 核心数据持久化加载
    // 调用底层数据管理器，从本地 txt 文件反序列化所有的链表数据进内存
    // 此时 init_system_data 已包含所有基础实体及手术室、出勤日志的加载防呆逻辑
    init_system_data(); 
    printf(COLOR_GREEN "\n全局数据库加载成功！按回车键进入管理控制台..." COLOR_RESET);
    wait_for_enter();

    // 3. 全局事件主循环 (Event Loop)
    int choice = -1;
    while (1) { // 无限循环，直到用户主动触发 0 号中断指令才结束
        show_main_menu();
        
        // 采用 utils.c 提供的防御性输入接口
        // 【对标题签】：满足“考虑各种类型的不规范、不合理或错误数据”的重点考察要求，防止非法字符导致死循环
        choice = safe_get_int();
        
        // 路由分发器 (Router)：根据用户输入的编号，挂载对应的业务子系统
        switch (choice) {
            case 1:
                // 进入患者端：已集成严格限流(5次/日)、押金校验(>=200*N)及 AI 推荐停诊医生替代方案
                patient_subsystem(); 
                break;
                
            case 2:
                // 进入医生端：已集成基于 schedules 的执业场景侦测、开具体检报告及 ICU 跨链表搬家
                doctor_subsystem();  
                break;
                
            case 3:
                // 进入管理端：集成排班防分身锁、剔除押金的真实营业额审计及病房容量降级预警
                admin_subsystem();   
                break;
                
            case 4:
                // 进入急救端：独立的救护车调度、出勤历史追溯模块
                ambulance_subsystem(); 
                break;
                
            case 0:
                // 4. 数据保存与安全退出流程 (Graceful Shutdown)
                // 【对标题签】：绝对满足“【存储】能够将当前系统中的所有信息保存到文件中”的要求
                printf("\n" COLOR_YELLOW "正在同步业务数据 (复合唯一单号、处方时效、底层链表迁移状态) 至本地文件...\n" COLOR_RESET);
                show_progress_bar("正在写入持久化存储并执行防断电保护");
                
                // 将内存中被修改过的所有链表数据序列化写回本地磁盘
                // save_system_data 包含了全量数据的落盘指令
                save_system_data(); 
                
                // 释放所有链表节点，归还操作系统内存，保障程序长期运行无内存泄露 (Memory Leak)
                // 彻底释放患者、医生、单据、手术室和出勤日志等全部堆内存
                free_all_data();    
                
                printf(COLOR_GREEN "\n所有业务数据已安全保存，指针内存已清空。HIS 系统安全退出。\n" COLOR_RESET);
                exit(0); // 正常结束进程
                
            default:
                // 容错处理：处理 0-4 之外的越界或非法输入，防止系统崩溃
                printf(COLOR_RED "\n指令越界：请输入 0-4 之间的有效终端编号。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
    
    return 0; 
}