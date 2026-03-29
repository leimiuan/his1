#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ambulance.h"
#include "utils.h"

extern AmbulanceNode* ambulance_head;

// 把这里的 void manage_ambulances() 改成 void ambulance_subsystem()
void ambulance_subsystem() {
    int choice = -1;
    // ... 下面的代码保持不变 ...
    while (choice != 0) {
        clear_input_buffer();
        print_header("120 救护车智慧调度中心");
        printf("  [ 1 ] 查看全院救护车实时监控\n");
        printf("  [ 2 ] 紧急派单出车 (120调度)\n");
        printf("  [ 3 ] 车辆回场及消毒登记\n");
        printf(COLOR_RED "  [ 0 ] 返回行政管理中心\n" COLOR_RESET);
        print_divider();
        printf(COLOR_YELLOW "请输入操作指令 (0-3): " COLOR_RESET);
        choice = safe_get_int();

        switch (choice) {
            case 1: {
                print_header("救护车队实时状态");
                printf("%-12s %-12s %-10s %-25s\n", "车牌/编号", "随车司机", "当前状态", "GPS 实时位置");
                print_divider();
                AmbulanceNode* curr = ambulance_head;
                int count = 0;
                while (curr) {
                    const char* st_text;
                    if (curr->status == AMB_IDLE) st_text = COLOR_GREEN "空闲待命" COLOR_RESET;
                    else if (curr->status == AMB_DISPATCHED) st_text = COLOR_RED "执行任务" COLOR_RESET;
                    else st_text = COLOR_YELLOW "维保消毒" COLOR_RESET;
                    
                    printf("%-12s %-12s %-10s %-25s\n", curr->vehicle_id, curr->driver_name, st_text, curr->current_location);
                    curr = curr->next;
                    count++;
                }
                if (count == 0) printf(COLOR_YELLOW "当前系统内尚未录入任何救护车数据。\n" COLOR_RESET);
                wait_for_enter();
                break;
            }
            case 2: {
                printf("请输入需调度的空闲车辆编号: ");
                char vid[MAX_ID_LEN]; safe_get_string(vid, MAX_ID_LEN);
                AmbulanceNode* target = ambulance_head;
                while (target && strcmp(target->vehicle_id, vid) != 0) target = target->next;
                
                if (!target) {
                    printf(COLOR_RED "错误：未找到该车辆。\n" COLOR_RESET);
                } else if (target->status != AMB_IDLE) {
                    printf(COLOR_RED "驳回：该车辆当前不在空闲状态，无法派单。\n" COLOR_RESET);
                } else {
                    printf("请输入急救现场地址/GPS定位: ");
                    safe_get_string(target->current_location, 128);
                    target->status = AMB_DISPATCHED;
                    printf(COLOR_GREEN "派单成功！车辆 %s 已紧急出车前往 [%s]。\n" COLOR_RESET, target->vehicle_id, target->current_location);
                }
                wait_for_enter();
                break;
            }
            case 3: {
                printf("请输入回场车辆编号: ");
                char vid[MAX_ID_LEN]; safe_get_string(vid, MAX_ID_LEN);
                AmbulanceNode* target = ambulance_head;
                while (target && strcmp(target->vehicle_id, vid) != 0) target = target->next;
                
                if (target && target->status == AMB_DISPATCHED) {
                    target->status = AMB_IDLE; // 重置为空闲
                    strcpy(target->current_location, "医院急救中心停车场");
                    printf(COLOR_GREEN "车辆 %s 已确认回场，并已完成标准消毒作业，转为待命状态。\n" COLOR_RESET, target->vehicle_id);
                } else {
                    printf(COLOR_RED "操作失败：未找到该车，或车辆未处于执行任务状态。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
        }
    }
}