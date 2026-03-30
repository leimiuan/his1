#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ambulance.h"
#include "utils.h"

/* ==========================================
 * 【新增引用】为了满足功能要求，在此拉取底层出勤日志与手术室结构
 * ========================================== */
#ifndef HIS_EXT_NODES_DEFINED
#define HIS_EXT_NODES_DEFINED
// 1. 救护车出勤日志节点
typedef struct AmbulanceLogNode {
    char log_id[32];            // 日志单号 (如 ALB1711693450)
    char vehicle_id[32];        // 派出的车辆编号
    char dispatch_time[32];     // 派发时间
    char destination[128];      // 任务目的地
    struct AmbulanceLogNode* next;
} AmbulanceLogNode;

// 2. 手术室节点 (急救联动使用：方便调度员预判抢救室是否有空位)
typedef struct OperatingRoomNode {
    char room_id[32];           // 手术室编号 (如 OR-01)
    int status;                 // 0: 空闲, 1: 手术中, 2: 消毒维护
    char current_patient[32];   // 当前患者
    char current_doctor[32];    // 主刀医生
    struct OperatingRoomNode* next;
} OperatingRoomNode;
#endif

/* ==========================================
 * 全局数据链表头指针 (使用 extern 引用)
 * 实际定义在其他文件（如 data_manager.c）中。
 * ========================================== */
extern AmbulanceNode* ambulance_head;        // 原有的实时车辆状态链表头指针
extern AmbulanceLogNode* ambulance_log_head; // 【新增】出勤记录表头指针
extern OperatingRoomNode* or_head;           // 【新增】手术室联动头指针

// 声明外部记录日志的底层工厂函数 (在 data_manager.c 中实现)
extern void add_ambulance_log(const char* vehicle_id, const char* destination);

/**
 * @brief 120 救护车智慧调度子系统的主入口
 * 提供交互式的菜单，用于对医院的救护车队进行实时监控、紧急派发和回场管理。
 * (已成功扩充出勤表和手术室联动)
 */
void ambulance_subsystem() {
    int choice = -1;
    
    // 主控循环，直到用户选择 0 (退出) 才结束
    while (choice != 0) {
        clear_input_buffer(); // 清理输入缓冲区，防止残留字符导致菜单乱跳
        
        // 打印系统菜单及头部 UI
        print_header("120 救护车智慧调度中心 (含出勤表与急救联动)");
        printf("  [ 1 ] 查看全院救护车实时监控\n");
        printf("  [ 2 ] 紧急派单出车 (120调度) + 自动记入出勤表\n");
        printf("  [ 3 ] 车辆回场及消毒登记\n");
        printf("  [ 4 ] 查看救护车历史出勤表 (【新增功能】)\n"); 
        printf("  [ 5 ] 联动：查看急诊手术室实时空闲状态 (【新增功能】)\n"); 
        printf(COLOR_RED "  [ 0 ] 返回行政管理中心\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请输入操作指令 (0-5): " COLOR_RESET);
        choice = safe_get_int(); // 安全获取用户输入的整型选项

        // 根据用户输入进行业务路由分发
        switch (choice) {
            case 1: {
                // ==========================================
                // 业务 1：全景看板 - 查看救护车队实时状态
                // ==========================================
                print_header("救护车队实时状态");
                // 打印表头，保持对齐
                printf("%-12s %-12s %-10s %-25s\n", "车牌/编号", "随车司机", "当前状态", "GPS 实时位置");
                print_divider();
                
                AmbulanceNode* curr = ambulance_head; // 从链表头部开始遍历
                int count = 0;
                
                while (curr) {
                    const char* st_text;
                    // 根据车辆的状态枚举(AMB_IDLE, AMB_DISPATCHED 等)映射为带颜色的终端文本
                    if (curr->status == AMB_IDLE) {
                        st_text = COLOR_GREEN "空闲待命" COLOR_RESET;  // 绿色代表可用
                    } else if (curr->status == AMB_DISPATCHED) {
                        st_text = COLOR_RED "执行任务" COLOR_RESET;    // 红色代表正忙
                    } else {
                        st_text = COLOR_YELLOW "维保消毒" COLOR_RESET; // 黄色代表不可用但非出车状态
                    }
                    
                    // 格式化输出当前车辆的具体信息
                    printf("%-12s %-12s %-10s %-25s\n", curr->vehicle_id, curr->driver_name, st_text, curr->current_location);
                    
                    curr = curr->next; // 指针后移
                    count++;           // 记录当前系统中车辆的总数
                }
                
                // 容错处理：如果链表为空，给予友好的提示
                if (count == 0) {
                    printf(COLOR_YELLOW "当前系统内尚未录入任何救护车数据。\n" COLOR_RESET);
                }
                wait_for_enter(); // 暂停，等待用户按回车继续
                break;
            }
            case 2: {
                // ==========================================
                // 业务 2：紧急出车派单 - 改变车辆状态并更新位置
                // ==========================================
                printf("请输入需调度的空闲车辆编号: ");
                char vid[MAX_ID_LEN]; 
                safe_get_string(vid, MAX_ID_LEN);
                
                // 在链表中查找匹配的车辆节点
                AmbulanceNode* target = ambulance_head;
                while (target && strcmp(target->vehicle_id, vid) != 0) {
                    target = target->next;
                }
                
                // 边界与状态校验
                if (!target) {
                    // 找不到对应的车辆
                    printf(COLOR_RED "错误：未找到该车辆。\n" COLOR_RESET);
                } else if (target->status != AMB_IDLE) {
                    // 核心业务规则：只有处于"空闲待命"状态的车才能被派发，防止资源冲突
                    printf(COLOR_RED "驳回：该车辆当前不在空闲状态，无法派单。\n" COLOR_RESET);
                } else {
                    // 满足派发条件，请求输入目的地信息
                    printf("请输入急救现场地址/GPS定位: ");
                    safe_get_string(target->current_location, 128); // 更新车辆的实时位置信息
                    
                    target->status = AMB_DISPATCHED; // 【状态扭转】将车辆状态扭转为“执行任务”
                    
                    // 【绝对指令要求植入】：状态扭转成功后，静默调用底层函数写入“历史出勤表”
                    add_ambulance_log(target->vehicle_id, target->current_location);

                    printf(COLOR_GREEN "派单成功！车辆 %s 已紧急出车前往 [%s]。\n" COLOR_RESET, target->vehicle_id, target->current_location);
                    printf(COLOR_CYAN "(系统底层已自动捕获该动作并写入出勤调度日志)\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 3: {
                // ==========================================
                // 业务 3：回场登记 - 结束任务并释放车辆资源
                // ==========================================
                printf("请输入回场车辆编号: ");
                char vid[MAX_ID_LEN]; 
                safe_get_string(vid, MAX_ID_LEN);
                
                // 在链表中查找匹配的车辆节点
                AmbulanceNode* target = ambulance_head;
                while (target && strcmp(target->vehicle_id, vid) != 0) {
                    target = target->next;
                }
                
                // 边界与状态校验：必须存在且当前状态必须是"执行任务"中
                if (target && target->status == AMB_DISPATCHED) {
                    target->status = AMB_IDLE; // 【状态扭转】将车辆状态重置为“空闲待命”
                    
                    // 模拟车辆回场后的物理位置重置
                    strcpy(target->current_location, "医院急救中心停车场");
                    
                    printf(COLOR_GREEN "车辆 %s 已确认回场，并已完成标准消毒作业，转为待命状态。\n" COLOR_RESET, target->vehicle_id);
                } else {
                    // 如果车辆不存在，或本身就停在医院里，则拒绝该操作
                    printf(COLOR_RED "操作失败：未找到该车，或车辆未处于执行任务状态。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 4: {
                // ==========================================
                // 【绝对指令要求植入】业务 4：查看救护车历史出勤表
                // ==========================================
                print_header("120 救护车历史出勤记录表");
                printf("%-15s %-12s %-22s %-30s\n", "调度单号", "车牌编号", "出勤时间", "目的地");
                print_divider();
                
                AmbulanceLogNode* curr_log = ambulance_log_head;
                int log_count = 0;
                
                // 遍历并展示历史出勤流水
                while (curr_log) {
                    printf("%-15s %-12s %-22s %-30s\n", 
                           curr_log->log_id, curr_log->vehicle_id, 
                           curr_log->dispatch_time, curr_log->destination);
                    curr_log = curr_log->next;
                    log_count++;
                }
                
                if (log_count == 0) {
                    printf(COLOR_YELLOW "暂无救护车出勤历史记录。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 5: {
                // ==========================================
                // 【绝对指令要求植入】业务 5：联动查看手术室状态
                // 场景：调度员在派出救护车接重症患者前，快速了解手术室空位情况
                // ==========================================
                                print_header("联动查询：急诊手术室实时监控");
                printf("%-10s %-12s %-15s %-15s\n", "手术室", "当前状态", "主刀医生", "患者编号");
                print_divider();
                
                OperatingRoomNode* curr_or = or_head;
                int or_count = 0;
                
                // 遍历输出系统全局的手术室动态
                while (curr_or) {
                    const char* st_text;
                    if (curr_or->status == 0) st_text = COLOR_GREEN "空闲待命" COLOR_RESET;
                    else if (curr_or->status == 1) st_text = COLOR_RED "手术抢救中" COLOR_RESET;
                    else st_text = COLOR_YELLOW "消毒维护" COLOR_RESET;
                    
                    printf("%-10s %-12s %-15s %-15s\n", curr_or->room_id, st_text, curr_or->current_doctor, curr_or->current_patient);
                    curr_or = curr_or->next;
                    or_count++;
                }
                
                if (or_count == 0) {
                    printf(COLOR_YELLOW "当前系统尚未初始化手术室数据，或无可用手术室。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
        }
    }
}