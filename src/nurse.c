#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "data_manager.h"
#include "utils.h"

extern RecordNode* record_head;
extern PatientNode* inpatient_head;
extern RecordNode* record_head;
extern PatientNode* inpatient_head;
extern NurseNode* nurse_head;  // 【新增】

// ==========================================
// 业务 1：病区床位与住院患者巡查
// ==========================================

static NurseNode* find_nurse_by_id(const char* id) {
  NurseNode* curr = nurse_head;
  while (curr) {
    if (strcmp(curr->id, id) == 0) return curr;
    curr = curr->next;
  }
  return NULL;
}

static int is_missing_assignment(const char* value) {
  if (!value || value[0] == '\0') return 1;
  if (strcmp(value, "N/A") == 0) return 1;
  if (strcmp(value, "暂无") == 0) return 1;
  if (strcmp(value, "未指派") == 0) return 1;
  return 0;
}

static const char* display_or_na(const char* value) {
  return is_missing_assignment(value) ? "N/A" : value;
}

static void ward_round_service() {
  PatientNode* curr = inpatient_head;
  int valid_count = 0;
  int abnormal_count = 0;
  int total_count = 0;

  print_header("病区床位与患者巡查 (Ward Round)");
  printf(COLOR_CYAN
         "【在床巡查主列表】(仅显示床位/医生信息完整患者)\n" COLOR_RESET);
  printf("%-8s %-12s %-15s %-12s %-12s\n", "床位号", "患者姓名", "患者编号",
         "主治医生", "管床医生");
  print_divider();

  while (curr) {
    int is_complete = !is_missing_assignment(curr->bed_id) &&
                      !is_missing_assignment(curr->attending_doctor_id) &&
                      !is_missing_assignment(curr->resident_doctor_id);

    if (is_complete) {
      printf("%-8s %-12s %-15s %-12s %-12s\n", curr->bed_id, curr->name,
             curr->id, curr->attending_doctor_id, curr->resident_doctor_id);
      valid_count++;
    } else {
      abnormal_count++;
    }

    curr = curr->next;
    total_count++;
  }

  if (valid_count == 0) {
    printf(COLOR_YELLOW "无可巡查的在床患者（主列表为空）。\n" COLOR_RESET);
  }

  print_divider();
  printf(
      COLOR_GREEN
      "在院总人数: %d 人 | 在床可巡查: %d 人 | 待处理异常: %d 人\n" COLOR_RESET,
      total_count, valid_count, abnormal_count);

  if (abnormal_count > 0) {
    curr = inpatient_head;
    printf("\n" COLOR_YELLOW
           "【待处理异常患者列表】(床位/医生字段缺失或N/A)\n" COLOR_RESET);
    printf("%-8s %-12s %-15s %-12s %-12s\n", "床位号", "患者姓名", "患者编号",
           "主治医生", "管床医生");
    print_divider();

    while (curr) {
      int is_complete = !is_missing_assignment(curr->bed_id) &&
                        !is_missing_assignment(curr->attending_doctor_id) &&
                        !is_missing_assignment(curr->resident_doctor_id);

      if (!is_complete) {
        printf("%-8s %-12s %-15s %-12s %-12s\n", display_or_na(curr->bed_id),
               curr->name, curr->id, display_or_na(curr->attending_doctor_id),
               display_or_na(curr->resident_doctor_id));
      }
      curr = curr->next;
    }
  }

  print_divider();
  if (total_count == 0) {
    printf(COLOR_YELLOW "当前病区无住院患者。\n" COLOR_RESET);
  }
  wait_for_enter();
}

// ==========================================
// 业务 2：录入生命体征 (体温/血压/心率)
// ==========================================
static void record_vitals() {
  char p_id[MAX_ID_LEN];
  double temp;
  char bp[32];

  print_header("录入生命体征 (Vitals Recording)");
  printf("请输入住院患者编号: ");
  safe_get_string(p_id, MAX_ID_LEN);

  PatientNode* patient = find_inpatient_by_id(p_id);
  if (!patient) {
    printf(COLOR_RED "查无此住院患者！\n" COLOR_RESET);
    wait_for_enter();
    return;
  }

  printf("请输入体温 (℃, 例如 37.2): ");
  temp = safe_get_double();
  printf("请输入血压 (收缩压/舒张压, 例如 120/80): ");
  safe_get_string(bp, sizeof(bp));

  // 拼装体征记录
  char desc[MAX_DESC_LEN];
  snprintf(desc, sizeof(desc), "VITALS|体温:%.1f℃|血压:%s", temp, bp);

  // 将体征作为一条不收费的记录写入数据库，供医生查房时调阅
  add_new_record(
      p_id,
      patient->resident_doctor_id[0] ? patient->resident_doctor_id : "SYS",
      RECORD_HOSPITALIZATION, desc, 0.0, 0);

  printf(COLOR_GREEN "生命体征录入成功！医生端已同步可见。\n" COLOR_RESET);
  wait_for_enter();
}

// ==========================================
// 业务 3：发药核销
// ==========================================

void dispense_medicine_service() {
  RecordNode* curr = record_head;
  RecordNode* available[100];  // 假设最多100个
  int count = 0;
  int choice;

  print_header("药房核销中心");

  // 收集可用的处方
  while (curr && count < 100) {
    if (curr->type == RECORD_CONSULTATION && curr->status == 1 &&
        strncmp(curr->description, "PRES|", 5) == 0 &&
        strstr(curr->description, "|DISPENSED") == NULL) {
      available[count++] = curr;
    }
    curr = curr->next;
  }

  if (count == 0) {
    printf(COLOR_YELLOW "当前没有待发药的处方。\n" COLOR_RESET);
    wait_for_enter();
    return;
  }

  printf("请选择要发药的处方:\n");
  print_divider();
  for (int i = 0; i < count; i++) {
    PatientNode* p = find_outpatient_by_id(available[i]->patient_id);
    if (!p) p = find_inpatient_by_id(available[i]->patient_id);
    // 解析药品信息
    char med_id[64];
    int amount;
    char med_name[64] = "未知药品";
    if (sscanf(available[i]->description, "PRES|%*[^|]|%*d|%[^|]|%d", med_id,
               &amount) == 2) {
      MedicineNode* med = find_medicine_by_id(med_id);
      if (med) {
        strcpy(med_name, med->common_name);
      }
    }
    printf(" %d. 患者: %-10s | 药品: %-15s | 数量: %d | 流水号: %s\n", i + 1,
           p ? p->name : "未知", med_name, amount, available[i]->record_id);
  }
  printf(" 0. 返回\n");
  print_divider();
  printf("请选择: ");
  choice = safe_get_int();

  if (choice < 1 || choice > count) {
    return;
  }

  RecordNode* target = available[choice - 1];

  // 解析处方并扣减物理库存
  char med_id[64];
  int amount;
  if (sscanf(target->description, "PRES|%*[^|]|%*d|%[^|]|%d", med_id,
             &amount) == 2) {
    MedicineNode* med = find_medicine_by_id(med_id);
    if (med) {
      if (med->stock >= amount) {
        med->stock -= amount;
        strcat(target->description, "|DISPENSED");
        printf(COLOR_GREEN
               "发药成功！库存已物理扣减。[%s] 剩余库存: %d\n" COLOR_RESET,
               med->common_name, med->stock);
      } else {
        printf(COLOR_RED
               "系统异常：当前物理库存不足，发药失败！\n" COLOR_RESET);
      }
    }
  }
  wait_for_enter();
}

// ==========================================
// 主入口：护士工作站控制台
// ==========================================
void nurse_subsystem() {
  char nurse_id[MAX_ID_LEN];
  int choice = -1;

  print_header("护士工作站入口");
  printf("请输入值班护士工号进行系统鉴权: ");
  safe_get_string(nurse_id, sizeof(nurse_id));

  NurseNode* me = find_nurse_by_id(nurse_id);
  if (!me) {
    printf(COLOR_RED
           "【拦截】鉴权失败：查无此工号，或已被行政吊销权限！\n" COLOR_RESET);
    wait_for_enter();
    return;
  }

  printf(COLOR_GREEN "鉴权通过！欢迎您，%s %s（所属科室: %s）\n" COLOR_RESET,
         me->name, me->role_level >= 3 ? "护士长" : "护士", me->dept_id);
  wait_for_enter();

  while (choice != 0) {
    print_header("护士控制台 (Nurse Station)");
    printf("当前值班: %s (%s) | 权限等级: Level %d\n", me->name, me->id,
           me->role_level);
    print_divider();
    printf("1. 病区床位与住院患者巡查\n");
    printf("2. 录入患者生命体征\n");
    printf("3. 发药核销\n");
    printf(COLOR_RED "0. 交接班并退出\n" COLOR_RESET);
    print_divider();

    printf("请选择指令: ");
    choice = safe_get_int();

    switch (choice) {
      case 1:
        ward_round_service();
        break;
      case 2:
        record_vitals();
        break;
      case 3:
        dispense_medicine_service();
        break;
      case 0:
        break;
      default:
        printf(COLOR_YELLOW "指令无法识别。\n" COLOR_RESET);
        wait_for_enter();
        break;
    }
  }
}