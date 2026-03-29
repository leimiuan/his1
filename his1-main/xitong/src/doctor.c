#include "doctor.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ==========================================
 * 1. 核心校验逻辑
 * ========================================== */

// 校验处方时效性：解析协议字段并对比当前时间
int is_prescription_valid(const char* desc) {
    if (strncmp(desc, "PRES", 4) != 0) return 1; 

    int type, days;
    char date_str[32];
    // 解析协议: PRES|T:类型|D:天数|ID:药品|Q:数量|DATE:日期
    if (sscanf(desc, "PRES|T:%d|D:%d|ID:%*[^|]|Q:%*d|DATE:%s", &type, &days, date_str) != 3) {
        return 1; 
    }

    struct tm tm_pres = {0};
    int year, month, day;
    sscanf(date_str, "%d-%d-%d", &year, &month, &day);
    tm_pres.tm_year = year - 1900;
    tm_pres.tm_mon = month - 1;
    tm_pres.tm_mday = day;

    time_t time_pres = mktime(&tm_pres);
    time_t time_now = time(NULL);

    double diff_sec = difftime(time_now, time_pres);
    int diff_days = (int)(diff_sec / (24 * 3600));

    if (diff_days > days) {
        printf(COLOR_RED "\n[拦截] 处方已失效（开立于 %s，有效期 %d 天）。\n" COLOR_RESET, date_str, days);
        return 0;
    }
    return 1;
}

/* ==========================================
 * 2. 医护工作站入口
 * ========================================== */

void doctor_subsystem() {
    clear_input_buffer();
    print_header("医护人员智慧工作站 v3.0");
    
    char id[MAX_ID_LEN];
    printf(COLOR_YELLOW "登录鉴权 - 请输入工号: " COLOR_RESET);
    safe_get_string(id, sizeof(id));
    
    DoctorNode* me = find_doctor_by_id(id);
    if (me == NULL) {
        printf(COLOR_RED "\n[错误] 认证失败，工号不存在！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    
    int choice = -1;
    PatientNode* current_patient = NULL; 

    while (choice != 0) {
        clear_input_buffer();
        printf("\n" COLOR_BLUE "=========================================\n");
        printf("  姓名: %-10s | 科室: %s\n", me->name, me->dept_id);
        if (current_patient != NULL) {
            printf(COLOR_RED "  >>> 就诊中: %s (%s)\n" COLOR_RESET, current_patient->name, current_patient->id);
        } else {
            printf(COLOR_GREEN "  >>> 状态: 候诊中 (Ready)\n" COLOR_RESET);
        }
        printf("=========================================\n" COLOR_RESET);
        printf("  [ 1 ] 查看候诊队列 (含急诊优先级标记)\n");
        printf("  [ 2 ] 呼叫下一位患者 (急诊优先)\n");
        printf("  [ 3 ] 开具分类电子处方\n");
        printf("  [ 4 ] 药房发药/处方核销\n");
        printf("  [ 5 ] 患者 ICU 转院/转科申请\n"); // 新增功能
        printf("  [ 6 ] 结束当前诊疗任务\n");
        printf("  [ 7 ] 查看本科室住院患者名单\n");
        printf("  [ 8 ] 个人接诊工作量统计\n");
        printf(COLOR_RED "  [ 0 ] 退出登录\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请选择业务编号 (0-8): " COLOR_RESET);
        choice = safe_get_int();
        
        switch (choice) {
            case 1: view_my_queue(me); break;
            case 2: 
                if (!current_patient) current_patient = call_next_patient(me); 
                else {
                    printf(COLOR_RED "（这里是你的提示文字）\n" COLOR_RESET);
                    wait_for_enter();
                }
                break;
            case 3: 
                if (current_patient) prescribe_medicine_advanced(me, current_patient); 

                else {
                    printf(COLOR_RED "（这里是你的提示文字）\n" COLOR_RESET);
                    wait_for_enter();
                }
                break;
            case 4: dispense_medicine_service(); break;
            case 5: transfer_patient_to_icu(me); break;
            case 6: 
                if (current_patient) {
                    printf(COLOR_GREEN "已完成患者 %s 的诊疗记录提交。\n" COLOR_RESET, current_patient->name);
                    current_patient = NULL;
                }
                wait_for_enter();
                break;
            case 7: view_department_inpatients(me); break;
            case 8: view_doctor_statistics(me); break;
            case 0: break;
        }
    }
}

/* ==========================================
 * 3. 诊疗业务：急诊优先叫号
 * ========================================== */

PatientNode* call_next_patient(DoctorNode* doctor) {
    RecordNode *curr = record_head, *target = NULL;
    int min_q = 999999;
    
    // 第一阶段：优先搜索急诊挂号 (RECORD_EMERGENCY = 5)
    curr = record_head;
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->type == RECORD_EMERGENCY && curr->status == 0) {
            if (curr->queue_number < min_q) { min_q = curr->queue_number; target = curr; }
        }
        curr = curr->next;
    }
    
    // 第二阶段：若无急诊，搜索普通门诊 (RECORD_REGISTRATION = 1)
    if (!target) {
        min_q = 999999;
        curr = record_head;
        while (curr) {
            if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->type == RECORD_REGISTRATION && curr->status == 0) {
                if (curr->queue_number < min_q) { min_q = curr->queue_number; target = curr; }
            }
            curr = curr->next;
        }
    }

    if (target) {
        target->status = 1; 
        PatientNode* p = find_outpatient_by_id(target->patient_id);
        if (p) printf(COLOR_RED "\n>>> [叫号] 请 %s 患者 %s 到诊室就诊！\n" COLOR_RESET, 
                      (target->type == RECORD_EMERGENCY ? "急诊" : "普通"), p->name);
        return p;
    }
    printf(COLOR_CYAN "候诊大厅目前暂无等待排队的患者。\n" COLOR_RESET);
    return NULL;
}

/* ==========================================
 * 4. 业务扩展：ICU 转院申请
 * ========================================== */

void transfer_patient_to_icu(DoctorNode* doctor) {
    print_header("患者转入 ICU / 特殊监护病房");
    printf("请输入患者编号: ");
    char p_id[MAX_ID_LEN];
    safe_get_string(p_id, sizeof(p_id));

    PatientNode* p = find_inpatient_by_id(p_id);
    if (!p) {
        printf(COLOR_RED "提示：未在住院名单中找到该患者。\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 自动寻找本科室空余的 ICU 床位 (Type 1 = SPECIAL_WARD)
    BedNode* icu_bed = find_available_bed(doctor->dept_id, 1); 
    if (!icu_bed) {
        printf(COLOR_RED "告警：本科室 ICU 资源已满，无法完成转院申请！\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 释放旧床位并占用新 ICU 床位
    BedNode* old_bed = find_bed_by_id(p->bed_id);
    if (old_bed) { old_bed->is_occupied = 0; strcpy(old_bed->patient_id, ""); }

    strcpy(p->bed_id, icu_bed->bed_id);
    icu_bed->is_occupied = 1;
    strcpy(icu_bed->patient_id, p->id);

    // 自动增加一条 ICU 护理记录用于后期费用核算
    add_new_record(p->id, doctor->id, RECORD_HOSPITALIZATION, "[ICU转入] 重症加强护理单", 1500.0, 0);

    printf(COLOR_GREEN "成功：患者 %s 已成功转入 ICU 床位 %s。\n" COLOR_RESET, p->name, icu_bed->bed_id);
    wait_for_enter();
}

/* ==========================================
 * 5. 处方与发药
 * ========================================== */

void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient) {
    print_header("开具分类电子处方");
    printf(" [ 1 ] 普通 (7天) | [ 2 ] 急诊 (3天) | [ 3 ] 慢病 (30天)\n 选择: ");
    int type = safe_get_int();
    int days = (type == 2) ? 3 : (type == 3 ? 30 : 7);

    printf("药品 ID: ");
    char mid[MAX_ID_LEN]; safe_get_string(mid, sizeof(mid));
    MedicineNode* m = find_medicine_by_id(mid);

    if (!m || m->stock <= 0) { printf(COLOR_RED "库存不足！\n" COLOR_RESET); return; }
    
    printf("数量: ");
    int q = safe_get_int();
    if (q > m->stock) { printf(COLOR_RED "库存不足！\n" COLOR_RESET); return; }

    char time_str[32]; get_current_time_str(time_str);
    char desc[MAX_DESC_LEN];
    sprintf(desc, "PRES|T:%d|D:%d|ID:%s|Q:%d|DATE:%s", type, days, m->id, q, time_str);
    
    add_new_record(patient->id, doctor->id, RECORD_CONSULTATION, desc, m->price * q, 0);
    printf(COLOR_GREEN "处方已下达，有效期 %d 天。\n" COLOR_RESET, days);
}

void dispense_medicine_service() {
    print_header("药房发药/处方核销");
    printf("记录编号: ");
    char rid[MAX_ID_LEN]; safe_get_string(rid, sizeof(rid));
    RecordNode* r = record_head;
    while(r) {
        if (strcmp(r->record_id, rid) == 0) {
            if (r->status == 0) { printf(COLOR_RED "拦截：单据未缴费！\n" COLOR_RESET); return; }
            if (!is_prescription_valid(r->description)) return; 

            char mid[MAX_ID_LEN]; int q;
            if (sscanf(r->description, "PRES|T:%*d|D:%*d|ID:%[^|]|Q:%d", mid, &q) == 2) {
                MedicineNode* m = find_medicine_by_id(mid);
                if (m && m->stock >= q) {
                    m->stock -= q;
                    printf(COLOR_GREEN "核销完成，已发放 %s x %d。\n" COLOR_RESET, m->common_name, q);
                }
            }
            wait_for_enter(); return;
        }
        r = r->next;
    }
    printf(COLOR_RED "未找到单据。\n" COLOR_RESET);
}

/* ==========================================
 * 6. 统计与辅助
 * ========================================== */

void view_my_queue(DoctorNode* doctor) {
    print_header("科室候诊队列 (急诊置顶)");
    RecordNode* curr = record_head;
    int count = 0;
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 0 &&
           (curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY)) {
            printf("[%s] %-10s | 排号: %d\n", (curr->type == RECORD_EMERGENCY ? COLOR_RED"急诊"COLOR_RESET : "普通"), 
                   curr->patient_id, curr->queue_number);
            count++;
        }
        curr = curr->next;
    }
    if (count == 0) printf("当前无患者等候。\n");
    wait_for_enter();
}

void view_doctor_statistics(DoctorNode* doctor) {
    print_header("工作统计");
    int c = 0; double t = 0;
    RecordNode* curr = record_head;
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 1) { c++; t += curr->total_fee; }
        curr = curr->next;
    }
    printf(" 今日接诊: %d 次 | 营收贡献: %.2f 元\n", c, t);
    wait_for_enter();
}

void view_department_inpatients(DoctorNode* doctor) {
    print_header("本科室在院患者");
    PatientNode* p = inpatient_head;
    while (p) {
        BedNode* b = find_bed_by_id(p->bed_id);
        if (b && strcmp(b->dept_id, doctor->dept_id) == 0) {
            printf("床号: %-10s | 患者: %s\n", b->bed_id, p->name);
        }
        p = p->next;
    }
    wait_for_enter();
}