#include "ambulance.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "utils.h"
// [新增引入] 为了使用 data_manager 中关于医生的查询和资质校验函数
#include "data_manager.h"

/* ==========================================
 * 【新增引用】底层出勤日志与手术室结构
 * ========================================== */
#ifndef HIS_EXT_NODES_DEFINED
#define HIS_EXT_NODES_DEFINED
// 1. 救护车出勤日志节点
typedef struct AmbulanceLogNode {
  char log_id[32];         // 日志单号
  char vehicle_id[32];     // 派出的车辆编号
  char dispatch_time[32];  // 派发时间
  char destination[128];   // 任务目的地
  struct AmbulanceLogNode* next;
} AmbulanceLogNode;

// 2. 手术室节点(急救联动使用)
typedef struct OperatingRoomNode {
  char room_id[32];          // 手术室编号
  int status;                // 0: 空闲, 1: 手术中
  char current_patient[32];  // 当前患者
  char current_doctor[32];   // 主刀医生
  struct OperatingRoomNode* next;
} OperatingRoomNode;
#endif

/* ==========================================
 * 全局数据链表头指针 (使用 extern 引用)
 * ========================================== */
extern AmbulanceNode* ambulance_head;
extern AmbulanceLogNode* ambulance_log_head;
extern OperatingRoomNode* or_head;

// 声明外部底层工厂函数
extern void add_ambulance_log(const char* vehicle_id, const char* destination);
extern void add_new_record(const char* patient_id, const char* doctor_id,
                           RecordType type, const char* description, double fee,
                           int queue_num);

/* ==========================================
 * 【新增逻辑】内部业务规则辅助函数
 * ========================================== */

// 职称代码转换为文字描述
static const char* map_level_to_title(int level) {
  switch (level) {
    case 4:
      return "正主任医师 (Lv.4)";
    case 3:
      return "副主任医师 (Lv.3)";
    case 2:
      return "主治医师 (Lv.2)";
    case 1:
      return "住院医师 (Lv.1)";
    default:
      return "未知资历";
  }
}

// 核心安全校验：检查值班医生是否有权限处理该急救事件
static int verify_doctor_authority_amb(const char* doctor_id, int req_level) {
  DoctorNode* doc = find_doctor_by_id(doctor_id);
  if (!doc) {
    printf(COLOR_RED "[拦截] 医生工号无效，无法指派抢救任务。\n" COLOR_RESET);
    return 0;
  }
  if (doc->privilege_level < req_level) {
    printf(
        COLOR_RED
        "[安全警报] 资质不符！急救任务要求: %s，当前指派医生: %s\n" COLOR_RESET,
        map_level_to_title(req_level),
        map_level_to_title(doc->privilege_level));
    return 0;
  }
  return 1;
}

/**
 * @brief 120 救护车智慧调度子系统
 */
void ambulance_subsystem() {
  int choice = -1;

  while (choice != 0) {
    print_header("120 救护车智慧调度中心(含急救绿通联动)");
    printf("  [ 1 ] 查看全院救护车实时监控\n");
    printf("  [ 2 ] 紧急派单出车(120调度) + 自动记入出勤表\n");
    printf("  [ 3 ] 车辆回场及患者急救直通建档 (闭环核心)\n");
    printf("  [ 4 ] 查看救护车历史出勤表\n");
    printf("  [ 5 ] 联动：查看急诊手术室实时空闲状态\n");
    printf(COLOR_RED "  [ 0 ] 返回行政管理中心\n" COLOR_RESET);
    print_divider();

    printf(COLOR_YELLOW "请输入操作指令(0-5): " COLOR_RESET);
    choice = safe_get_int();

    switch (choice) {
      case 1: {
        // ==========================================
        // 业务 1：查看救护车队实时状态
        // ==========================================
        print_header("救护车队实时状态");
        printf("%-12s %-12s %-10s %-25s\n", "车牌/编号", "随车司机", "当前状态",
               "GPS实时位置");
        print_divider();

        AmbulanceNode* curr = ambulance_head;
        int count = 0;

        while (curr) {
          const char* st_text;
          if (curr->status == AMB_IDLE) {
            st_text = COLOR_GREEN "空闲待命" COLOR_RESET;
          } else if (curr->status == AMB_DISPATCHED) {
            st_text = COLOR_RED "执行任务" COLOR_RESET;
          } else {
            st_text = COLOR_YELLOW "维保消毒" COLOR_RESET;
          }

          printf("%-12s %-12s %-10s %-25s\n", curr->vehicle_id,
                 curr->driver_name, st_text, curr->current_location);
          curr = curr->next;
          count++;
        }

        if (count == 0) {
          printf(COLOR_YELLOW
                 "当前系统内尚未录入任何救护车数据。\n" COLOR_RESET);
        }
        wait_for_enter();
        break;
      }
      case 2: {
        // ==========================================
        // 业务 2：紧急派单出车
        // ==========================================
        printf("请输入需调度的空闲车辆编号: ");
        char vid[MAX_ID_LEN];
        safe_get_string(vid, MAX_ID_LEN);

        AmbulanceNode* target = ambulance_head;
        while (target && strcmp(target->vehicle_id, vid) != 0) {
          target = target->next;
        }

        if (!target) {
          printf(COLOR_RED "错误：未找到该车辆。\n" COLOR_RESET);
        } else if (target->status != AMB_IDLE) {
          printf(COLOR_RED
                 "驳回：该车辆当前不在空闲状态，无法派单。\n" COLOR_RESET);
        } else {
          printf("请输入急救现场地址/GPS定位: ");
          safe_get_string(target->current_location, 128);

          target->status = AMB_DISPATCHED;

          // 静默记录历史出勤表
          add_ambulance_log(target->vehicle_id, target->current_location);

          printf(COLOR_GREEN
                 "派单成功！车辆 %s 已紧急出车前往 [%s]。\n" COLOR_RESET,
                 target->vehicle_id, target->current_location);
          printf(COLOR_CYAN
                 "(系统底层已自动捕获该动作并写入出勤调度日志)\n" COLOR_RESET);
        }
        wait_for_enter();
        break;
      }
      case 3: {
        // ==========================================
        // 【核心增强】业务 3：车辆回场 + 急救直通建档
        // 逻辑：车辆洗消后，如果有带回的患者，直接触发抢救组资源分配
        // ==========================================
        printf("请输入回场车辆编号: ");
        char vid[MAX_ID_LEN];
        safe_get_string(vid, MAX_ID_LEN);

        AmbulanceNode* target = ambulance_head;
        while (target && strcmp(target->vehicle_id, vid) != 0) {
          target = target->next;
        }

        if (target && target->status == AMB_DISPATCHED) {
          // 1. 车辆状态流转 (进入维保或空闲)
          target->status = AMB_IDLE;
          strcpy(target->current_location, "医院急救中心驻地");
          printf(
              COLOR_GREEN
              ">>> 车辆 %s "
              "已确认回场，并已执行深度洗消程序，转为待命状态。\n" COLOR_RESET,
              target->vehicle_id);

          // 2. 急诊直通逻辑触发
          printf(
              COLOR_YELLOW
              "\n该救护车是否载有需紧急抢救的患者？(1:是, 0:否): " COLOR_RESET);
          int has_patient = safe_get_int();

          if (has_patient == 1) {
            char p_id[MAX_ID_LEN], d_id[MAX_ID_LEN], intern_id[MAX_ID_LEN],
                date[32], desc[MAX_DESC_LEN];
            int req_level;

            printf(COLOR_CYAN
                   "==== 启动 120 绿色直通建档程序 (自动强制最高优先级) "
                   "====\n" COLOR_RESET);

            printf("患者编号(或暂定急救号): ");
            safe_get_string(p_id, MAX_ID_LEN);
            printf("当前日期(YYYY-MM-DD): ");
            safe_get_string(date, 32);

            // 资质指派
            printf("根据病情初步评估，请输入所需的手术/抢救资质等级(1-4): ");
            req_level = safe_get_int();

            // ====== 【核心修复：智能防撞扫描大名单】 ======
            printf(COLOR_CYAN
                   "\n正在全院检索满足 Level %d "
                   "抢救资质的专家大名单...\n" COLOR_RESET,
                   req_level);
            print_divider();

            DoctorNode* expert = doctor_head;
            int expert_count = 0;

            while (expert) {
              if (expert->privilege_level >= req_level) {
                // 【物理防撞锁】：把正在手术室里的大牛过滤掉
                int is_operating = 0;
                OperatingRoomNode* temp_room = or_head;
                while (temp_room) {
                  if (temp_room->status == 1 &&
                      strcmp(temp_room->current_doctor, expert->id) == 0) {
                    is_operating = 1;
                    break;
                  }
                  temp_room = temp_room->next;
                }

                // 只有不在手术台上的医生，才显示在列表里
                if (!is_operating) {
                  int on_duty = is_doctor_on_duty_today(expert->id);
                  if (on_duty)
                    printf(COLOR_GREEN "[在院当值] " COLOR_RESET);
                  else
                    printf(COLOR_YELLOW "[二线听班] " COLOR_RESET);

                  printf(
                      "专家: %-8s | 工号: %-6s | 资质: Level %d | 科室: %s\n",
                      expert->name, expert->id, expert->privilege_level,
                      expert->dept_id);
                  expert_count++;
                }
              }
              expert = expert->next;
            }
            print_divider();

            if (expert_count == 0) {
              printf(COLOR_RED
                     "【全院红色告警】当前全院无任何满足该资质且空闲的医生！请"
                     "启动跨院转运预案！\n" COLOR_RESET);
              wait_for_enter();
              break;
            }

            // ====== 【核心修复：防呆死循环输入，且统一使用 d_id 变量】 ======
            while (1) {
              printf("\n请参照上方列表，指派接车主治/主刀医生工号: ");
              safe_get_string(d_id,
                              MAX_ID_LEN);  // 直接存入后续建档需要的 d_id 中！

              // 校验 1：权限与存在性校验
              if (!verify_doctor_authority_amb(d_id, req_level)) {
                printf(COLOR_RED
                       "[拦截] "
                       "医生工号无效或资质不符，请重新输入！\n" COLOR_RESET);
                continue;
              }

              // 校验 2：二次防撞校验（防止调度员没看列表瞎填）
              int is_operating = 0;
              OperatingRoomNode* temp_room = or_head;
              while (temp_room) {
                if (temp_room->status == 1 &&
                    strcmp(temp_room->current_doctor, d_id) == 0) {
                  is_operating = 1;
                  break;
                }
                temp_room = temp_room->next;
              }
              if (is_operating) {
                printf(COLOR_RED
                       "[拦截] "
                       "该医生正在抢救其他患者，分身乏术！请重新选择。"
                       "\n" COLOR_RESET);
                continue;
              }
              break;  // 完美通过所有校验，跳出死循环
            }

            // 【联动拦截】双责任制：必须指定管床建档人
            printf("请指派负责全程陪护/病历跟进的管床医生(住院医)工号: ");
            safe_get_string(intern_id, MAX_ID_LEN);
            if (!find_doctor_by_id(intern_id)) {
              printf(COLOR_RED
                     "直通建档失败：未指定有效的管床医生。\n" COLOR_RESET);
              wait_for_enter();
              break;
            }

            printf("简要病史与现场处置记录: ");
            safe_get_string(desc, MAX_DESC_LEN);

            // 自动拼装特定的描述，触发后续的 999 优先级逻辑
            char auto_desc[MAX_DESC_LEN + 64];
            snprintf(auto_desc, sizeof(auto_desc), "[救护车直通入抢救室] %s",
                     desc);

            // 默认记录为类型 4 (住院/手术)，金额暂估 0.0，排队号强制为 1
            add_new_record(p_id, d_id, RECORD_HOSPITALIZATION, auto_desc, 0.0,
                           1);

            // ====== 【核心修复：跨模块资源强抢占，触动医生端雷达】 ======
            OperatingRoomNode* avail_room = or_head;
            while (avail_room) {
              if (avail_room->status == 0) break;  // 找一间空闲的房间
              avail_room = avail_room->next;
            }

            if (avail_room) {
              avail_room->status = 1;  // 强行锁定为使用中
              strcpy(avail_room->current_patient, p_id);
              strcpy(avail_room->current_doctor, d_id);
              printf(COLOR_RED
                     "【物理资源调度】已自动为您抢占并锁定抢救室: "
                     "%s\n" COLOR_RESET,
                     avail_room->room_id);
            } else {
              printf(COLOR_YELLOW
                     "【物理资源告警】当前全院无空闲手术室/"
                     "抢救室！已通知急诊科就地开展抢救！\n" COLOR_RESET);
            }
            // ================================================================

            printf(COLOR_GREEN
                   "急救通道已激活！患者记录已下发，主刀[%s] & 管床[%s] "
                   "已接收到强提醒任务。\n" COLOR_RESET,
                   d_id, intern_id);
          }
        } else {
          printf(COLOR_RED
                 "操作失败：未找到该车，或车辆当前未处于执行任务状态。"
                 "\n" COLOR_RESET);
        }
        wait_for_enter();
        break;
      }
      case 4: {
        // ==========================================
        // 业务 4：查看救护车历史出勤表
        // ==========================================
        print_header("120 救护车历史出勤记录表");
        printf("%-15s %-12s %-22s %-30s\n", "调度单号", "车牌编号", "出勤时间",
               "目的地");
        print_divider();

        AmbulanceLogNode* curr_log = ambulance_log_head;
        int log_count = 0;

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
        // 业务 5：联动查看手术室状态
        // ==========================================
        print_header("联动查询：急诊手术室实时监控");
        printf("%-10s %-12s %-15s %-15s\n", "手术室", "当前状态", "主刀医生",
               "患者编号");
        print_divider();

        OperatingRoomNode* curr_or = or_head;
        int or_count = 0;

        while (curr_or) {
          const char* st_text;
          if (curr_or->status == 0)
            st_text = COLOR_GREEN "空闲待命" COLOR_RESET;
          else
            st_text = COLOR_RED "手术抢救中" COLOR_RESET;

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