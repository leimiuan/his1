#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "utils.h"
#include "data_manager.h"

extern RecordNode* record_head;
extern PatientNode* inpatient_head;
extern RecordNode* record_head;
extern PatientNode* inpatient_head;
extern NurseNode* nurse_head; // 【新增】

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

static void ward_round_service() {
    print_header("病区床位与患者巡查 (Ward Round)");
    printf("%-8s %-12s %-15s %-12s %-12s\n", "床位号", "患者姓名", "患者编号", "主治医生", "管床医生");
    print_divider();
    
    PatientNode* curr = inpatient_head;
    int count = 0;
    while (curr) {
        printf("%-8s %-12s %-15s %-12s %-12s\n", 
               curr->bed_id[0] ? curr->bed_id : "暂无",
               curr->name, 
               curr->id, 
               curr->attending_doctor_id[0] ? curr->attending_doctor_id : "未指派",
               curr->resident_doctor_id[0] ? curr->resident_doctor_id : "未指派");
        curr = curr->next;
        count++;
    }
    print_divider();
    if (count == 0) {
        printf(COLOR_YELLOW "当前病区无住院患者。\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "当前病区在院总人数: %d 人\n" COLOR_RESET, count);
    }
    wait_for_enter();
}

// ==========================================
// 业务 2：执行医生医嘱 (发药/打针/护理)
// ==========================================
static void execute_medical_orders() {
    char p_id[MAX_ID_LEN];
    print_header("执行医嘱与发药 (Execute Orders)");
    printf("请输入目标住院患者编号: ");
    safe_get_string(p_id, MAX_ID_LEN);

    PatientNode* patient = find_inpatient_by_id(p_id);
    if (!patient) {
        printf(COLOR_RED "查无此住院患者，或该患者已出院！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    RecordNode* curr = record_head;
    int pending_count = 0;

    printf("\n【待执行医嘱列表】\n");
    print_divider();
    while (curr) {
        // 查找属于该患者、且包含 "MED|" (药品) 并且还没被执行的单据
        if (strcmp(curr->patient_id, p_id) == 0 && 
            strstr(curr->description, "MED|") != NULL && 
            strstr(curr->description, "|EXECUTED") == NULL) {
            
            printf("[%s] %s | %s\n", curr->record_id, curr->date, curr->description);
            pending_count++;
        }
        curr = curr->next;
    }

    if (pending_count == 0) {
        printf(COLOR_GREEN "该患者当前无待执行的医嘱！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf(COLOR_CYAN "\n是否一键执行所有待办医嘱？(1:执行发药/打针, 0:取消): " COLOR_RESET);
    if (safe_get_int() == 1) {
        curr = record_head;
        while (curr) {
            if (strcmp(curr->patient_id, p_id) == 0 && 
                strstr(curr->description, "MED|") != NULL && 
                strstr(curr->description, "|EXECUTED") == NULL) {
                
                // 盖上护士已执行的防伪戳
                strcat(curr->description, "|EXECUTED");
            }
            curr = curr->next;
        }
        printf(COLOR_GREEN "执行成功！已完成发药与护理确认。\n" COLOR_RESET);
    }
    wait_for_enter();
}

// ==========================================
// 业务 3：录入生命体征 (体温/血压/心率)
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
    add_new_record(p_id, patient->resident_doctor_id[0] ? patient->resident_doctor_id : "SYS", 
                   RECORD_HOSPITALIZATION, desc, 0.0, 0);

    printf(COLOR_GREEN "生命体征录入成功！医生端已同步可见。\n" COLOR_RESET);
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
        printf(COLOR_RED "【拦截】鉴权失败：查无此工号，或已被行政吊销权限！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    
    printf(COLOR_GREEN "鉴权通过！欢迎您，%s %s（所属科室: %s）\n" COLOR_RESET, 
           me->name, me->role_level >= 3 ? "护士长" : "护士", me->dept_id);
    wait_for_enter();

    while (choice != 0) {
        print_header("护士控制台 (Nurse Station)");
        printf("当前值班: %s (%s) | 权限等级: Level %d\n", me->name, me->id, me->role_level);
        print_divider();
        printf("1. 病区床位与住院患者巡查\n");
        printf("2. 执行医生医嘱 (发药/输液)\n");
        printf("3. 录入患者生命体征\n");
        printf(COLOR_RED "0. 交接班并退出\n" COLOR_RESET);
        print_divider();
        
        printf("请选择指令: ");
        choice = safe_get_int();

        switch (choice) {
            case 1: ward_round_service(); break;
            case 2: execute_medical_orders(); break;
            case 3: record_vitals(); break;
            case 0: break;
            default:
                printf(COLOR_YELLOW "指令无法识别。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}