#include "doctor.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ==========================================
 * 1. 核心入口与处方时效校验逻辑
 * ========================================== */

// 校验处方是否在有效期内
// 返回值：1-有效，0-已过期
int is_prescription_valid(const char* desc) {
    if (strncmp(desc, "PRES", 4) != 0) return 1; // 非处方记录，直接放行

    int type, days;
    char date_str[32];
    // 解析格式: PRES|T:1|D:7|ID:M001|Q:2|DATE:2026-03-25
    if (sscanf(desc, "PRES|T:%d|D:%d|ID:%*[^|]|Q:%*d|DATE:%s", &type, &days, date_str) != 3) {
        return 1; // 解析失败则跳过校验
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
        printf(COLOR_RED "\n[告警] 处方已过期！\n" COLOR_RESET);
        printf("开具日期: %s, 有效期: %d 天, 已过去: %d 天\n", date_str, days, diff_days);
        return 0;
    }
    return 1;
}

void doctor_subsystem() {
    clear_input_buffer();
    print_header("医护人员工作站");
    
    char id[MAX_ID_LEN];
    printf(COLOR_YELLOW "请输入医生编号以登录: " COLOR_RESET);
    safe_get_string(id, sizeof(id));
    
    DoctorNode* me = find_doctor_by_id(id);
    if (me == NULL) {
        printf(COLOR_RED "\n[登录失败] 未找到该医生编号！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    
    int choice = -1;
    PatientNode* current_patient = NULL; 

    while (choice != 0) {
        clear_input_buffer();
        printf("\n=========================================\n");
        printf(COLOR_GREEN "  欢迎，%s 医生 (科室: %s)\n" COLOR_RESET, me->name, me->dept_id);
        if (current_patient != NULL) {
            printf(COLOR_YELLOW "  [当前就诊患者: %s (编号:%s)]\n" COLOR_RESET, current_patient->name, current_patient->id);
        } else {
            printf(COLOR_CYAN "  [状态: 空闲，等待接诊下一位患者]\n" COLOR_RESET);
        }
        printf("=========================================\n");
        printf("  [ 1 ] 查看候诊队列\n");
        printf("  [ 2 ] 呼叫下一位患者\n");
        printf("  [ 3 ] 开具分类电子处方\n");
        printf("  [ 4 ] 药房发药服务 (核销单据)\n");
        printf("  [ 5 ] 结束当前就诊\n");
        printf("  [ 6 ] 查看本科室住院名单\n");
        printf("  [ 7 ] 查看值班表\n");
        printf("  [ 9 ] 我的工作量统计\n");
        printf(COLOR_RED "  [ 0 ] 退出登录\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请选择 (0-9): " COLOR_RESET);
        choice = safe_get_int();
        
        switch (choice) {
            case 1: view_my_queue(me); break;
            case 2: 
                if (!current_patient) current_patient = call_next_patient(me); 
                else { printf(COLOR_RED "请先结束当前患者就诊！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 3: 
                if (current_patient) prescribe_medicine_advanced(me, current_patient); 
                else { printf(COLOR_RED "当前未接诊任何患者！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 4: dispense_medicine_service(); break;
            case 5: 
                if (current_patient) {
                    printf(COLOR_GREEN "患者 %s 的就诊已结束。\n" COLOR_RESET, current_patient->name);
                    current_patient = NULL;
                }
                wait_for_enter();
                break;
            case 6: view_department_inpatients(me); break;
            case 7: view_duty_schedule(NULL); break;
            case 9: view_doctor_statistics(me); break;
            case 0: break;
            default: printf(COLOR_RED "无效的选择。\n" COLOR_RESET); wait_for_enter();
        }
    }
}

/* ==========================================
 * 2. 处方管理模块 (分类与时效)
 * ========================================== */

void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient) {
    print_header("开具分类电子处方");
    
    printf("请选择处方类型:\n");
    printf(" [ 1 ] 普通处方 (7天有效)\n");
    printf(" [ 2 ] 急性处方 (3天有效)\n");
    printf(" [ 3 ] 慢性病处方 (30天有效)\n");
    printf(COLOR_YELLOW "请选择 (1-3): " COLOR_RESET);
    int type_choice = safe_get_int();
    int valid_days = (type_choice == 2) ? 3 : (type_choice == 3 ? 30 : 7);

    char med_id[MAX_ID_LEN];
    printf("请输入药品编号: ");
    safe_get_string(med_id, sizeof(med_id));
    
    MedicineNode* med = find_medicine_by_id(med_id);
    if (!med || med->stock <= 0) {
        printf(COLOR_RED "错误：药品不存在或库存不足！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    
    printf("药品: %s | 库存: %d | 单价: %.2f\n", med->common_name, med->stock, med->price);
    printf("请输入开药数量: ");
    int qty = safe_get_int();
    
    if (qty <= 0 || qty > med->stock) {
        printf(COLOR_RED "数量无效或库存不足！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    
    // 生成带时间的描述字符串
    char time_now[32];
    get_current_time_str(time_now); 
    char desc[MAX_DESC_LEN];
    // 协议格式: PRES|类型|天数|药品ID|数量|日期
    sprintf(desc, "PRES|T:%d|D:%d|ID:%s|Q:%d|DATE:%s", type_choice, valid_days, med->id, qty, time_now);
    
    double total_cost = med->price * qty;
    add_new_record(patient->id, doctor->id, RECORD_CONSULTATION, desc, total_cost, 0);
    
    printf(COLOR_GREEN "\n>>> 处方开具成功！有效期 %d 天。需缴费后方可发药。\n" COLOR_RESET, valid_days);
    wait_for_enter();
}

/* ==========================================
 * 3. 药房发药联动模块
 * ========================================== */

void dispense_medicine_service() {
    print_header("药房发药/处方核销");
    printf("请输入患者持有的处方记录编号 (RECxxx): ");
    char rec_id[MAX_ID_LEN];
    safe_get_string(rec_id, sizeof(rec_id));

    RecordNode* r = record_head;
    while (r) {
        if (strcmp(r->record_id, rec_id) == 0) {
            // 1. 检查支付状态
            if (r->status == 0) {
                printf(COLOR_RED "禁止发药：该处方尚未缴费！\n" COLOR_RESET);
                wait_for_enter();
                return;
            }

            // 2. 检查时效性
            if (!is_prescription_valid(r->description)) {
                printf(COLOR_RED "该处方已失效，必须请医生重新开具！\n" COLOR_RESET);
                wait_for_enter();
                return;
            }

            // 3. 解析并扣减库存
            char med_id[MAX_ID_LEN];
            int qty;
            if (sscanf(r->description, "PRES|T:%*d|D:%*d|ID:%[^|]|Q:%d", med_id, &qty) == 2) {
                MedicineNode* med = find_medicine_by_id(med_id);
                if (med && med->stock >= qty) {
                    med->stock -= qty;
                    printf(COLOR_GREEN "\n>>> 核销成功！已发放药品: %s x %d\n" COLOR_RESET, med->common_name, qty);
                } else {
                    printf(COLOR_RED "发药失败：药品库存不足！\n" COLOR_RESET);
                }
            } else {
                printf(COLOR_YELLOW "提示：该记录非标准处方，无需发药核销。\n" COLOR_RESET);
            }
            wait_for_enter();
            return;
        }
        r = r->next;
    }
    printf(COLOR_RED "未找到该单据记录。\n" COLOR_RESET);
    wait_for_enter();
}

/* ==========================================
 * 4. 辅助功能
 * ========================================== */

void view_my_queue(DoctorNode* doctor) {
    print_header("候诊队列");
    RecordNode* curr = record_head;
    int count = 0;
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->type == RECORD_REGISTRATION && curr->status == 0) {
            PatientNode* p = find_outpatient_by_id(curr->patient_id);
            if (p) {
                printf("排号: [%03d] 患者: %-10s | 单号: %s\n", curr->queue_number, p->name, curr->record_id);
                count++;
            }
        }
        curr = curr->next;
    }
    if (count == 0) printf(COLOR_GREEN "当前无等候患者。\n" COLOR_RESET);
    wait_for_enter();
}

PatientNode* call_next_patient(DoctorNode* doctor) {
    RecordNode *curr = record_head, *target = NULL;
    int min_q = 999999;
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->type == RECORD_REGISTRATION && curr->status == 0) {
            if (curr->queue_number < min_q) { min_q = curr->queue_number; target = curr; }
        }
        curr = curr->next;
    }
    if (target) {
        target->status = 1; // 标记挂号单为已接诊
        PatientNode* p = find_outpatient_by_id(target->patient_id);
        if (p) printf(COLOR_GREEN "\n>>> 请 [%03d] 号患者 %s 到诊室就诊！\n" COLOR_RESET, target->queue_number, p->name);
        wait_for_enter();
        return p;
    }
    printf(COLOR_YELLOW "无等待患者。\n" COLOR_RESET);
    wait_for_enter();
    return NULL;
}

void view_doctor_statistics(DoctorNode* doctor) {
    print_header("工作统计");
    int count = 0; double total = 0;
    RecordNode* curr = record_head;
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 1) {
            count++; total += curr->total_fee;
        }
        curr = curr->next;
    }
    printf(" 今日已接诊/开单: %d 次\n", count);
    printf(" 累计产生营收: %.2f 元\n", total);
    wait_for_enter();
}

void view_department_inpatients(DoctorNode* doctor) {
    print_header("科室住院名单");
    PatientNode* curr = inpatient_head;
    int count = 0;
    while (curr) {
        if (strlen(curr->bed_id) > 0) {
            BedNode* bed = find_bed_by_id(curr->bed_id);
            if (bed && strcmp(bed->dept_id, doctor->dept_id) == 0) {
                printf(" 床位: [%s] | 患者: %-10s\n", bed->bed_id, curr->name);
                count++;
            }
        }
        curr = curr->next;
    }
    if (count == 0) printf("本科室当前无住院患者。\n");
    wait_for_enter();
}