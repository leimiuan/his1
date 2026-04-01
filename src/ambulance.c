#include "ambulance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "data_manager.h"
#include "utils.h"

/* ==========================================
 * 新增扩展节点定义：用于出勤日志与手术室联动展示
 * ========================================== */
#ifndef HIS_EXT_NODES_DEFINED
#define HIS_EXT_NODES_DEFINED
// 1. 救护车出勤日志节点
typedef struct AmbulanceLogNode {
  char log_id[32];         // 日志单号 (例如 ALB1711693450)
  char vehicle_id[32];     // 派出的车辆编号
  char dispatch_time[32];  // 派发时间
  char destination[128];   // 任务目的地
  struct AmbulanceLogNode* next;
} AmbulanceLogNode;

// 2. 手术室节点（急救联动使用，便于预判抢救室是否有空位）
typedef struct OperatingRoomNode {
  char room_id[32];          // 手术室编号 (例如 OR-01)
  int status;                // 0: 空闲, 1: 手术中, 2: 消毒维护
  char current_patient[32];  // 当前患者
  char current_doctor[32];   // 主刀医生
  struct OperatingRoomNode* next;
} OperatingRoomNode;
#endif

/* ==========================================
 * 全局数据链表头指针（通过 extern 引用）
 * 实际定义位于其他文件（如 data_manager.c）
 * ========================================== */
extern AmbulanceNode* ambulance_head;         // 原有的救护车实时状态链表头指针
extern AmbulanceLogNode* ambulance_log_head;  // 新增：出勤记录表头指针
extern OperatingRoomNode* or_head;            // 新增：手术室联动链表头指针

// 声明外部日志写入函数（在 data_manager.c 中实现）
extern void add_ambulance_log(const char* vehicle_id, const char* destination);
// 跨模块联动：引入挂号记录创建接口，用于车辆回场后快速接入急诊流程
extern void add_new_record(const char* patient_id, const char* doctor_id,
                           RecordType type, const char* description, double fee,
                           int queue_num);

/**
 * @brief 120 救护车智能调度子系统主入口
 * 提供交互式菜单，用于救护车实时监控、紧急派单和回场管理。
 * 已扩展：出勤记录查询、急诊手术室联动查询。
 */
void ambulance_subsystem() {
  int choice = -1;

  // 主循环：直到用户选择 0（退出）才结束
  while (choice != 0) {
    clear_input_buffer();  // 清理输入缓冲区，防止残留字符导致菜单跳转异常

    // 打印系统菜单与头部 UI
    print_header("120 救护车智能调度中心（含出勤记录与急救联动）");
    printf("  [ 1 ] 查看全院救护车实时监控\n");
    printf("  [ 2 ] 紧急派单出车(120调度) + 自动写入出勤记录\n");
    printf("  [ 3 ] 车辆回场与消毒登记(+ " COLOR_RED
           "危重患者绿色通道" COLOR_RESET ")\n");
    printf("  [ 4 ] 查看救护车历史出勤记录（新增）\n");
    printf("  [ 5 ] 联动：查看急诊手术室实时空闲状态（新增）\n");
    printf(COLOR_RED "  [ 0 ] 返回行政管理中心\n" COLOR_RESET);
    print_divider();

    printf(COLOR_YELLOW "请输入操作指令(0-5): " COLOR_RESET);
    choice = safe_get_int();  // 安全读取用户输入的整型选项

    // 根据用户输入进行业务分发
    switch (choice) {
      case 1: {
        // ==========================================
        // 业务 1：查看救护车队实时状态
        // ==========================================
        print_header("救护车队实时状态");
        // 打印表头并对齐
        printf("%-12s %-12s %-10s %-25s\n", "车牌/编号", "随车司机", "当前状态",
               "GPS 实时位置");
        print_divider();

        AmbulanceNode* curr = ambulance_head;  // 从链表头开始遍历
        int count = 0;

        while (curr) {
          const char* st_text;
          // 根据状态码映射为带颜色的终端文本
          if (curr->status == AMB_IDLE) {
            st_text = COLOR_GREEN "空闲待命" COLOR_RESET;  // 绿色表示可用
          } else if (curr->status == AMB_DISPATCHED) {
            st_text = COLOR_RED "执行任务" COLOR_RESET;  // 红色表示执行任务中
          } else {
            st_text = COLOR_YELLOW "维护消毒" COLOR_RESET;  // 黄色表示维护/消毒
          }

          // 输出当前车辆信息
          printf("%-12s %-12s %-10s %-25s\n", curr->vehicle_id,
                 curr->driver_name, st_text, curr->current_location);

          curr = curr->next;  // 指针后移
          count++;            // 记录车辆总数
        }

        // 容错：链表为空时给出提示
        if (count == 0) {
          printf(COLOR_YELLOW "当前系统尚未录入任何救护车数据。\n" COLOR_RESET);
        }
        wait_for_enter();  // 暂停，等待用户回车
        break;
      }
      case 2: {
        // ==========================================
        // 业务 2：紧急派单出车，更新车辆状态与位置
        // ==========================================
        printf("请输入需要调度的空闲车辆编号: ");
        char vid[MAX_ID_LEN];
        safe_get_string(vid, MAX_ID_LEN);

        // 在链表中查找匹配车辆
        AmbulanceNode* target = ambulance_head;
        while (target && strcmp(target->vehicle_id, vid) != 0) {
          target = target->next;
        }

        // 边界与状态校验
        if (!target) {
          // 未找到对应车辆
          printf(COLOR_RED "错误：未找到该车辆。\n" COLOR_RESET);
        } else if (target->status != AMB_IDLE) {
          // 仅空闲车辆允许派发，防止资源冲突
          printf(COLOR_RED
                 "驳回：该车辆当前不在空闲状态，无法派单。\n" COLOR_RESET);
        } else {
          // 满足派发条件，采集目的地并更新状态
          printf("请输入急救现场地址/GPS定位: ");
          safe_get_string(target->current_location, 128);  // 更新实时位置

          target->status = AMB_DISPATCHED;  // 状态切换为“执行任务”

          // 状态切换成功后，写入历史出勤日志
          add_ambulance_log(target->vehicle_id, target->current_location);

          save_ambulances("data/ambulances.txt");
          save_ambulances("data/ambulance.txt");
          save_ambulance_logs("data/ambulance_logs.txt");
          printf(COLOR_GREEN
                 "派单成功！车辆 %s 已紧急出车前往 [%s]。\n" COLOR_RESET,
                 target->vehicle_id, target->current_location);
          printf(COLOR_CYAN "(系统已自动记录该次出勤调度日志)\n" COLOR_RESET);
        }
        wait_for_enter();
        break;
      }
      case 3: {
        // ==========================================
        // 业务 3：回场登记，结束任务并释放车辆资源（含急救联动）
        // ==========================================
        printf("请输入回场车辆编号: ");
        char vid[MAX_ID_LEN];
        safe_get_string(vid, MAX_ID_LEN);

        // 在链表中查找匹配车辆
        AmbulanceNode* target = ambulance_head;
        while (target && strcmp(target->vehicle_id, vid) != 0) {
          target = target->next;
        }

        // 校验：车辆存在且当前状态为“执行任务”
        if (target && target->status == AMB_DISPATCHED) {
          target->status = AMB_IDLE;  // 状态重置为“空闲待命”
          save_ambulances("data/ambulances.txt");
          save_ambulances("data/ambulance.txt");

          // 模拟车辆回场后的位置重置
          strcpy(target->current_location, "医院急救中心停车场");
          save_ambulances("data/ambulances.txt");
          save_ambulances("data/ambulance.txt");

          printf(
              COLOR_GREEN
              "车辆 %s "
              "已确认回场，并已完成标准消毒作业，转为待命状态。\n" COLOR_RESET,
              target->vehicle_id);

          // ==========================================
          // 急诊绿色通道自动联动
          // 车辆回场后，可将随车危重患者直接接入急诊队列
          // ==========================================
          printf(
              "\n" COLOR_CYAN
              ">>> 是否有随车转运的危重患者需要立即接入【急诊绿色通道】？(1-是 "
              "0-否): " COLOR_RESET);
          if (safe_get_int() == 1) {
            char pid[MAX_ID_LEN], did[MAX_ID_LEN];
            printf(
                "请输入患者编号(若尚未建档，请先输入临时编号，如 P-EMG-01): ");
            safe_get_string(pid, MAX_ID_LEN);
            printf("请输入负责接诊的【急诊科】医生工号: ");
            safe_get_string(did, MAX_ID_LEN);

            // 强制创建 RECORD_EMERGENCY 急诊记录并推入底层记录链表
            // 急诊费固定 50.0 元，queue_number=0 表示急诊优先
            add_new_record(pid, did, RECORD_EMERGENCY,
                           "120救护车紧急转运入院(生命体征不稳)", 50.0, 0);

            printf(
                COLOR_RED
                ">>> [最高优先级警报] 患者 %s 的急救档案已生成！\n" COLOR_RESET,
                pid);
            printf(COLOR_RED
                   ">>> 该急诊单据已强制推送至医生 %s "
                   "的候诊屏，并【置顶亮红灯】！\n" COLOR_RESET,
                   did);
          }

        } else {
          // 车辆不存在或未处于任务状态，拒绝回场登记
          printf(COLOR_RED
                 "操作失败：未找到该车辆，或车辆未处于执行任务状态。"
                 "\n" COLOR_RESET);
        }
        wait_for_enter();
        break;
      }
      case 4: {
        // ==========================================
        // 业务 4：查看救护车历史出勤记录
        // ==========================================
        print_header("120 救护车历史出勤记录");
        printf("%-15s %-12s %-22s %-30s\n", "调度单号", "车牌编号", "出勤时间",
               "目的地");
        print_divider();

        AmbulanceLogNode* curr_log = ambulance_log_head;
        int log_count = 0;

        // 遍历并输出历史出勤流水
        while (curr_log) {
          printf("%-15s %-12s %-22s %-30s\n", curr_log->log_id,
                 curr_log->vehicle_id, curr_log->dispatch_time,
                 curr_log->destination);
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
        // 业务 5：联动查询急诊手术室实时状态
        // 场景：派车接危重患者前，先快速确认手术室空位情况
        // ==========================================
        print_header("联动查询：急诊手术室实时监控");
        printf("%-10s %-12s %-15s %-15s\n", "手术室", "当前状态", "主刀医生",
               "患者编号");
        print_divider();

        OperatingRoomNode* curr_or = or_head;
        int or_count = 0;

        // 遍历输出全部手术室状态
        while (curr_or) {
          const char* st_text;
          if (curr_or->status == 0)
            st_text = COLOR_GREEN "空闲待命" COLOR_RESET;
          else if (curr_or->status == 1)
            st_text = COLOR_RED "手术抢救中" COLOR_RESET;
          else
            st_text = COLOR_YELLOW "消毒维护" COLOR_RESET;

          printf("%-10s %-12s %-15s %-15s\n", curr_or->room_id, st_text,
                 curr_or->current_doctor, curr_or->current_patient);
          curr_or = curr_or->next;
          or_count++;
        }

        if (or_count == 0) {
          printf(
              COLOR_YELLOW
              "当前系统尚未初始化手术室数据，或无可用手术室。\n" COLOR_RESET);
        }
        wait_for_enter();
        break;
      }
    }
  }
}
