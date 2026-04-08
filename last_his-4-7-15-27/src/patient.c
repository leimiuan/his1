// [核心架构说明]：
// 本文件为 HIS 系统患者自助服务终端层 (Patient Portal Layer)。
// 涵盖了患者建档、挂号、缴费、查报告、呼叫救护车等面向大众的 C 端业务。
// 本次修改严格遵循“原代码不删减”的铁律，保留了所有的流程控制与界面交互。
// 并在“呼叫 120 救护车”和“排队状态”模块，打通了急救绿色通道（Score 999）的闭环提示。

#include "patient.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// [架构注释] 定义医院每日承载量的熔断阈值
#ifndef MAX_CLINIC_DAILY_TICKETS
#define MAX_CLINIC_DAILY_TICKETS 500     // 全院每日最大挂号量
#define MAX_DOCTOR_DAILY_TICKETS 20      // 单个医生每日最大接诊量
#define MAX_PATIENT_DAILY_TICKETS 5      // 单个患者每日最大挂号次数，防黄牛刷号
#endif

// 引用外部全局链表与底层写库函数
extern RecordNode* record_head;
extern DoctorNode* doctor_head;
extern OperatingRoomNode* or_head;
extern AmbulanceNode* ambulance_head;
extern void add_ambulance_log(const char* vehicle_id, const char* destination);

/**
 * @brief 获取当日日期的前缀 (YYYY-MM-DD)，用于流水号生成
 */
static void get_today_prefix(char* today_str) {
    get_current_time_str(today_str);
    today_str[10] = '\0';
}

/**
 * @brief 自动生成每日患者编号的自增序列
 */
static int get_today_patient_sequence(const char* ymd8) {
    int max_seq = 0;
    PatientNode* curr = NULL;

    // 扫描门诊患者池
    for (curr = outpatient_head; curr != NULL; curr = curr->next) {
        char date_part[9] = {0};
        int seq = 0;
        if (strlen(curr->id) == 12 &&
            sscanf(curr->id, "P%8[0-9]%3d", date_part, &seq) == 2 &&
            strcmp(date_part, ymd8) == 0 &&
            seq > max_seq) {
            max_seq = seq;
        }
    }

    // 扫描住院患者池
    for (curr = inpatient_head; curr != NULL; curr = curr->next) {
        char date_part[9] = {0};
        int seq = 0;
        if (strlen(curr->id) == 12 &&
            sscanf(curr->id, "P%8[0-9]%3d", date_part, &seq) == 2 &&
            strcmp(date_part, ymd8) == 0 &&
            seq > max_seq) {
            max_seq = seq;
        }
    }

    return max_seq + 1;
}

/**
 * @brief 内部辅助：根据 ID 查特定就诊记录
 */
static RecordNode* find_record_by_id_local(const char* record_id) {
    RecordNode* curr = record_head;
    while (curr) {
        if (strcmp(curr->record_id, record_id) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/**
 * @brief 判定是否为挂号相关的核心记录 (门诊/急诊)
 */
static int is_registration_record(const RecordNode* record) {
    return record && (record->type == RECORD_REGISTRATION || record->type == RECORD_EMERGENCY);
}

/**
 * @brief 判定患者是否处于正常候诊状态 (已缴费、未呼叫、未完成)
 */
static int is_waiting_registration(const RecordNode* record) {
    return is_registration_record(record) &&
        record->status == 1 &&
        strstr(record->description, "|CALLED") == NULL &&
        strstr(record->description, "|DONE") == NULL;
}

/**
 * @brief 字符串解析器：提取 YYYY-MM-DD
 */
static int parse_ymd10(const char* text, int* year, int* month, int* day) {
    if (!text || sscanf(text, "%d-%d-%d", year, month, day) != 3) {
        return 0;
    }
    if (*year < 2000 || *year > 2100 || *month < 1 || *month > 12 || *day < 1 || *day > 31) {
        return 0;
    }
    return 1;
}

/**
 * @brief 校验是否已产生“住院办理费”账单
 */
static int has_positive_admission_charge(const char* patient_id) {
    RecordNode* curr = record_head;
    while (curr) {
        if (strcmp(curr->patient_id, patient_id) == 0 &&
            curr->type == RECORD_HOSPITALIZATION &&
            strstr(curr->description, "住院办理费") != NULL &&
            curr->total_fee > 0.0) {
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

/**
 * @brief 校验某日是否已结转过床位费
 */
static int has_bed_fee_for_day(const char* patient_id, const char* day_text) {
    RecordNode* curr = record_head;
    char tag[64];
    snprintf(tag, sizeof(tag), "BEDFEE|%s|", day_text);
    while (curr) {
        if (strcmp(curr->patient_id, patient_id) == 0 &&
            curr->type == RECORD_HOSPITALIZATION &&
            strstr(curr->description, tag) != NULL) {
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}

/**
 * @brief 自动补账引擎 (Auto-Billing Engine)
 * 在患者查账或缴费前，系统会自动扫描患者在院天数，
 * 补齐所有未生成的每日床位费账单，确保财务不流失。
 */
static void ensure_inpatient_cost_records(PatientNode* me) {
    int start_y = 0, start_m = 0, start_d = 0;
    int end_y = 0, end_m = 0, end_d = 0;
    struct tm tm_start = {0};
    struct tm tm_end = {0};
    time_t start_time;
    time_t end_time;
    double daily_fee = 200.0;
    char today[32] = {0};
    BedNode* bed;

    if (!me || me->type != 'I') {
        return;
    }

    bed = find_bed_by_id(me->bed_id);
    if (bed && bed->daily_fee > 0.0) {
        daily_fee = bed->daily_fee;
    }

    if (!has_positive_admission_charge(me->id)) {
        add_new_record(me->id, "SYS_BILL", RECORD_HOSPITALIZATION, "住院办理费", 50.0, 0);
    }

    get_today_prefix(today);
    if (!parse_ymd10(today, &end_y, &end_m, &end_d)) {
        return;
    }
    if (!parse_ymd10(me->admission_date, &start_y, &start_m, &start_d)) {
        start_y = end_y;
        start_m = end_m;
        start_d = end_d;
    }

    tm_start.tm_year = start_y - 1900;
    tm_start.tm_mon = start_m - 1;
    tm_start.tm_mday = start_d;
    tm_start.tm_isdst = -1;
    tm_end.tm_year = end_y - 1900;
    tm_end.tm_mon = end_m - 1;
    tm_end.tm_mday = end_d;
    tm_end.tm_isdst = -1;
    start_time = mktime(&tm_start);
    end_time = mktime(&tm_end);
    if (start_time == (time_t)-1 || end_time == (time_t)-1) {
        return;
    }
    if (start_time > end_time) {
        start_time = end_time;
    }

    for (time_t t = start_time; t <= end_time; t += 24 * 3600) {
        char day_text[16] = {0};
        char desc[MAX_DESC_LEN];
        struct tm* local_day = localtime(&t);
        if (!local_day) {
            continue;
        }
        strftime(day_text, sizeof(day_text), "%Y-%m-%d", local_day);
        if (has_bed_fee_for_day(me->id, day_text)) {
            continue;
        }
        snprintf(desc, sizeof(desc), "BEDFEE|%s|BED:%s", day_text, (me->bed_id[0] ? me->bed_id : "N/A"));
        add_new_record(me->id, "SYS_BILL", RECORD_HOSPITALIZATION, desc, daily_fee, 0);
    }
}

/**
 * @brief 患者跨库登录鉴权
 */
PatientNode* patient_login(const char* input_id) {
    PatientNode* patient = find_inpatient_by_id(input_id);
    if (patient) return patient;
    return find_outpatient_by_id(input_id);
}

void patient_top_up(PatientNode* me) {
    double amount;

    print_header("账户充值");
    printf("充值金额: ");
    amount = safe_get_double();
    if (amount <= 0) {
        printf(COLOR_RED "充值金额必须大于0。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    me->account_balance += amount;
    printf(COLOR_GREEN "充值成功，当前余额：%.2f\n" COLOR_RESET, me->account_balance);
    wait_for_enter();
}

/**
 * @brief 生成电子票据存根
 */
void generate_electronic_invoice(PatientNode* patient, RecordNode* record) {
    print_divider();
    printf("电子票据\n");
    print_divider();
    printf("患者: %s（%s）\n", patient->name, patient->id);
    printf("记录: %s\n", record->record_id);
    printf("类型: %d\n", (int)record->type);
    printf("日期: %s\n", record->date);
    printf("总额: %.2f\n", record->total_fee);
    printf("统筹支付: %.2f\n", record->insurance_covered);
    printf("个人支付: %.2f\n", record->personal_paid);
    print_divider();
}

/**
 * @brief 财务风控：患者自助缴费引擎
 */
void patient_pay_bill(PatientNode* me) {
    RecordNode* curr;
    char record_id[MAX_ID_LEN];
    RecordNode* record;
    int count = 0;

    ensure_inpatient_cost_records(me);
    curr = record_head;

    print_header("待缴费用");
    printf("%-18s %-14s %-10s %-10s\n", "记录", "类型", "总额", "应付");
    print_divider();
    while (curr) {
        if (strcmp(curr->patient_id, me->id) == 0 && curr->status == 0) {
            printf("%-18s %-14d %-10.2f %-10.2f\n", curr->record_id, (int)curr->type, curr->total_fee, curr->personal_paid);
            count++;
        }
        curr = curr->next;
    }
    if (count == 0) {
        printf(COLOR_YELLOW "当前没有待缴费用。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("请输入要缴费的记录编号: ");
    safe_get_string(record_id, sizeof(record_id));
    record = find_record_by_id_local(record_id);
    if (!record || strcmp(record->patient_id, me->id) != 0 || record->status != 0) {
        printf(COLOR_RED "未找到对应账单。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (me->account_balance < record->personal_paid) {
        printf(COLOR_RED "余额不足。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    me->account_balance -= record->personal_paid;
    record->status = 1;
    printf(COLOR_GREEN "缴费成功，当前余额：%.2f\n" COLOR_RESET, me->account_balance);
    generate_electronic_invoice(me, record);
    wait_for_enter();
}

void view_personal_info(PatientNode* patient) {
    OperatingRoomNode* room = or_head;

    print_header("个人信息");
    printf("姓名    : %s\n", patient->name);
    printf("编号    : %s\n", patient->id);
    printf("年龄    : %d\n", patient->age);
    printf("性别    : %s\n", patient->gender);
    printf("余额    : %.2f\n", patient->account_balance);

    // 神级联动：检查患者是否正在抢救中
    while (room) {
        if (room->status == 1 && strcmp(room->current_patient, patient->id) == 0) {
            printf(COLOR_RED "特殊状态: 您目前正在 %s 手术室接受手术抢救中！\n" COLOR_RESET, room->room_id);
            wait_for_enter();
            return;
        }
        room = room->next;
    }

    // 状态分流展示
    if (patient->type == 'I') {
        printf("状态    : 住院中\n");
        printf("床位    : %s\n", patient->bed_id);
        printf("押金    : %.2f\n", patient->hospital_deposit);
        
        // 【核心修复：动态读取并展示主治与管床医生】
        DoctorNode* att_doc = find_doctor_by_id(patient->attending_doctor_id);
        DoctorNode* res_doc = find_doctor_by_id(patient->resident_doctor_id);
        
        printf(COLOR_CYAN "主治医生: %s (工号:%s)\n" COLOR_RESET, 
               att_doc ? att_doc->name : "未指派", 
               patient->attending_doctor_id[0] ? patient->attending_doctor_id : "N/A");
               
        printf(COLOR_CYAN "管床医生: %s (工号:%s)\n" COLOR_RESET, 
               res_doc ? res_doc->name : "未指派", 
               patient->resident_doctor_id[0] ? patient->resident_doctor_id : "N/A");
               
    } else {
        printf("状态    : 门诊/急诊待命中\n");
    }
    wait_for_enter();
}

void query_medical_records(const char* patient_id) {
    RecordNode* curr = record_head;
    int count = 0;

    print_header("病历记录");
    while (curr) {
        if (strcmp(curr->patient_id, patient_id) == 0) {
            printf("[%s] %s | 类型=%d | 费用=%.2f | 状态=%d\n", curr->record_id, curr->date, (int)curr->type, curr->total_fee, curr->status);
            printf("  描述: %s\n", curr->description);
            count++;
        }
        curr = curr->next;
    }
    if (count == 0) {
        printf(COLOR_YELLOW "未找到相关记录。\n" COLOR_RESET);
    }
    wait_for_enter();
}

void check_queue_status(PatientNode* patient) {
    RecordNode* curr = record_head;
    RecordNode* waiting = NULL;
    RecordNode* unpaid = NULL;
    int ahead = 0;

    print_header("排队状态");

    // 1. 扫描患者名下的挂号记录
    while (curr) {
        if (strcmp(curr->patient_id, patient->id) == 0 && 
            (curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY) &&
            strstr(curr->description, "|CALLED") == NULL) { // 必须是未被医生呼叫过的
            
            // 【核心修复】：急诊记录无视缴费状态，直接获取排队特权！
            if (curr->type == RECORD_EMERGENCY) {
                waiting = curr;
                break;
            } 
            // 普通门诊记录：必须已缴费 (status == 1) 才能排队
            else if (curr->status == 1) {
                waiting = curr;
                break;
            } 
            // 普通门诊且未缴费，记录下来用于后续拦截提示
            else if (curr->status == 0) {
                if (!unpaid) unpaid = curr;
            }
        }
        curr = curr->next;
    }

    // 2. 状态分发拦截
    if (!waiting) {
        if (unpaid) {
            printf(COLOR_YELLOW "您有一条【普通门诊】挂号单尚未缴费。\n" COLOR_RESET);
            printf("请先前往收费处完成缴费，方可进入候诊队列。\n");
            printf("记录编号: %s\n", unpaid->record_id);
            wait_for_enter();
            return;
        }
        printf(COLOR_YELLOW "当前没有处于候诊中的挂号记录。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 3. 计算前方排队人数
    curr = record_head;
    while (curr) {
        if (strcmp(curr->doctor_id, waiting->doctor_id) == 0 &&
            (curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY) &&
            strstr(curr->description, "|CALLED") == NULL) {
            
            // 只有同为急诊（或门诊已缴费）的才算在有效队列内
            int is_valid_queue = (curr->type == RECORD_EMERGENCY) || (curr->status == 1);
            
            if (is_valid_queue && curr->queue_number < waiting->queue_number) {
                ahead++;
            }
        }
        curr = curr->next;
    }

    // 4. UI 渲染与特权展示
    printf("就诊类型: %s\n", waiting->type == RECORD_EMERGENCY ? COLOR_RED "急诊 (绿色通道)" COLOR_RESET : "普通门诊");
    printf("队列号 : %03d\n", waiting->queue_number);
    
    if (waiting->status == 0) {
        printf(COLOR_CYAN "费用状态: 未交费 (特权：先诊疗，后付费)\n" COLOR_RESET);
    } else {
        printf("费用状态: 已交费\n");
    }

    // 120 绿色通道插队状态
    if (waiting->queue_number == 1 && waiting->type == RECORD_EMERGENCY) {
        printf(COLOR_RED "【最高优先级】您已接入急救网络，医生正准备接诊！\n" COLOR_RESET);
    } else {
        printf("前方人数: %d\n", ahead);
        printf("预计等待: 约%d 分钟\n", ahead * (waiting->type == RECORD_EMERGENCY ? 5 : 12));
    }
    
    wait_for_enter();
}

void patient_self_register(PatientNode* me) {
    char today[32];
    int clinic_total = 0;
    int patient_total = 0;
    int reg_type;
    double fee;
    char doctor_id[MAX_ID_LEN];
    DoctorNode* target_doctor;

    get_today_prefix(today);
    print_header("挂号");

    for (RecordNode* curr = record_head; curr; curr = curr->next) {
        if (is_registration_record(curr) && strncmp(curr->date, today, 10) == 0) {
            clinic_total++;
            if (strcmp(curr->patient_id, me->id) == 0) {
                patient_total++;
            }
        }
    }

    if (clinic_total >= MAX_CLINIC_DAILY_TICKETS) {
        printf(COLOR_RED "今日医院挂号总量已达上限。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (patient_total >= MAX_PATIENT_DAILY_TICKETS) {
        printf(COLOR_RED "患者当日挂号次数已达上限。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("1. 门诊挂号（5.00）\n2. 急诊挂号（50.00）\n0. 返回\n请选择: ");
    reg_type = safe_get_int();
    if (reg_type != 1 && reg_type != 2) {
        return;
    }
    fee = (reg_type == 2) ? 50.0 : 5.0;

    if (me->account_balance < fee) {
        printf(COLOR_RED "余额不足，无法完成挂号。\n" COLOR_RESET);
        printf("是否立即充值？（1=是，0=否）: ");
        if (safe_get_int() == 1) {
            patient_top_up(me);
        }
        return;
    }

    printf(COLOR_CYAN "你最近就诊过的医生：\n" COLOR_RESET);
    {
        int found = 0;
        for (RecordNode* curr = record_head; curr; curr = curr->next) {
            if (strcmp(curr->patient_id, me->id) == 0 && (curr->type == RECORD_REGISTRATION || curr->type == RECORD_CONSULTATION)) {
                DoctorNode* doc = find_doctor_by_id(curr->doctor_id);
                if (doc) {
                    printf("- %s（%s，科室 %s）\n", doc->name, doc->id, doc->dept_id);
                    found = 1;
                }
            }
        }
        if (!found) {
            printf("- 无\n");
        }
    }

    while (1) {
        int doctor_total = 0;
        int same_dept_total = 0;

        printf("医生工号（0返回）: ");
        safe_get_string(doctor_id, sizeof(doctor_id));
        target_doctor = find_doctor_by_id(doctor_id);
        if (strcmp(doctor_id, "0") == 0) {
            return;
        }
        if (!target_doctor) {
            printf(COLOR_RED "未找到该医生。\n" COLOR_RESET);
            wait_for_enter();
            return;
        }
        if (!is_doctor_on_duty_today(target_doctor->id)) {
            printf(COLOR_RED "输入无效：该医生今日未排班，无法挂号。\n" COLOR_RESET);
            printf(COLOR_CYAN "请重新选择医生，先查看值班表。\n" COLOR_RESET);
            view_duty_schedule(NULL);
            continue;
        }
        if (reg_type == 1 && !is_doctor_on_duty_today_by_slot(target_doctor->id, "门诊")) {
            printf(COLOR_RED "输入无效：门诊挂号不能选择急诊排班医生。\n" COLOR_RESET);
            printf(COLOR_CYAN "请改选门诊医生，或返回后改挂急诊。\n" COLOR_RESET);
            view_duty_schedule(NULL);
            continue;
        }
        if (reg_type == 2 && !is_doctor_on_duty_today_by_slot(target_doctor->id, "急诊")) {
            printf(COLOR_RED "输入无效：急诊挂号不能选择门诊排班医生。\n" COLOR_RESET);
            printf(COLOR_CYAN "请改选急诊医生，或返回后改挂门诊。\n" COLOR_RESET);
            view_duty_schedule(NULL);
            continue;
        }


        for (RecordNode* curr = record_head; curr; curr = curr->next) {
            if (is_registration_record(curr) && strncmp(curr->date, today, 10) == 0) {
                if (strcmp(curr->doctor_id, target_doctor->id) == 0) {
                    doctor_total++;
                }
                if (strcmp(curr->patient_id, me->id) == 0) {
                    DoctorNode* doc = find_doctor_by_id(curr->doctor_id);
                    if (doc && strcmp(doc->dept_id, target_doctor->dept_id) == 0) {
                        same_dept_total++;
                    }
                }
            }
        }

        if (same_dept_total >= 1) {
            printf(COLOR_RED "同一患者同一天同一科室只允许挂号一次。\n" COLOR_RESET);
            wait_for_enter();
            return;
        }
        if (doctor_total >= MAX_DOCTOR_DAILY_TICKETS) {
            printf(COLOR_YELLOW "所选医生今日号源已满，推荐同科室可选医生：\n" COLOR_RESET);
            for (DoctorNode* doc = doctor_head; doc; doc = doc->next) {
                if (strcmp(doc->dept_id, target_doctor->dept_id) == 0) {
                    int count = 0;
                    for (RecordNode* curr = record_head; curr; curr = curr->next) {
                        if (is_registration_record(curr) && strncmp(curr->date, today, 10) == 0 && strcmp(curr->doctor_id, doc->id) == 0) {
                            count++;
                        }
                    }
                    if (count < MAX_DOCTOR_DAILY_TICKETS) {
                        printf("- %s（%s），剩余 %d 个号\n", doc->name, doc->id, MAX_DOCTOR_DAILY_TICKETS - count);
                    }
                }
            }
            printf("是否改选其他医生？（1=是，0=否）: ");
            if (safe_get_int() == 1) {
                continue;
            }
            return;
        }
        break;
    }

    // ---------------------------------------------------------
    // 【急诊预检分诊引擎 (Triage System)】
    // ---------------------------------------------------------
    int final_queue_num = 0; 

    if (reg_type == 2) { // 2 代表急诊挂号
        int triage_level = 4;
        print_divider();
        printf(COLOR_RED "【急诊预检分诊台】\n" COLOR_RESET);
        printf("分诊护士正在评估您的病情，请如实选择您的症状：\n");
        printf("1. 濒危 (心跳骤停/大出血/休克)   -> 1级响应 (极重度)\n");
        printf("2. 危重 (急性心梗/严重呼吸困难)  -> 2级响应 (重度)\n");
        printf("3. 急症 (骨折/剧烈腹痛/高烧)     -> 3级响应 (中度)\n");
        printf("4. 非急症 (感冒/开药/单纯想快点) -> 4级响应 (轻度)\n");
        printf("请选择您的病情等级 (1-4): ");
        triage_level = safe_get_int();

        int base_score = 900; // 默认非急症号段
        if (triage_level == 1) base_score = 10;       // 1级濒危：11, 12, 13...排在最前！
        else if (triage_level == 2) base_score = 100; // 2级危重：101, 102, 103...
        else if (triage_level == 3) base_score = 500; // 3级急症：501, 502, 503...
        else base_score = 900;                        // 4级非急症：901, 902...排在最后！

        // 动态计算该号段内的排队顺位，防止同级别患者插队
        RecordNode* curr = record_head;
        int max_offset = 0;
        char today[32];
        get_current_time_str(today);
        today[10] = '\0';
        
        while (curr) {
            // 找到同一位医生、同一天、且属于同一号段的患者
            if (strcmp(curr->doctor_id, target_doctor->id) == 0 && strncmp(curr->date, today, 10) == 0) {
                 if (curr->queue_number > base_score && curr->queue_number <= base_score + 99) {
                     if (curr->queue_number - base_score > max_offset) {
                         max_offset = curr->queue_number - base_score;
                     }
                 }
            }
            curr = curr->next;
        }
        final_queue_num = base_score + max_offset + 1;
        
        printf(COLOR_CYAN "\n预检分诊完毕！系统已为您分配对应优先级的排队号：%03d\n" COLOR_RESET, final_queue_num);
    }

    // 调用底层核心工厂，排队逻辑由 data_manager 托管 (注意这里把 0 换成了 final_queue_num)
    add_new_record(me->id,
        target_doctor->id,
        reg_type == 2 ? RECORD_EMERGENCY : RECORD_REGISTRATION,
        reg_type == 2 ? "急诊挂号" : "门诊挂号",
        fee,
        final_queue_num);

    // 挂号成功瞬间的门急诊话术分离
    if (reg_type == 2) {
        print_divider();
        printf(COLOR_RED "急诊挂号成功！【急救绿色通道已开启】\n" COLOR_RESET);
        printf(COLOR_CYAN "特权提示：先诊疗，后付费！请立刻前往急诊诊室，无需等待缴费！\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "\n门诊挂号成功，请先前往收费处完成缴费后再等待叫号。\n" COLOR_RESET);
    }
    wait_for_enter();
}

/**
 * @brief 患者自助呼叫救护车
 * [新增架构逻辑]：完善 120 急救闭环，患者点击呼叫后联动指挥中心分配车辆
 */
void call_ambulance_service(PatientNode* me) {
    AmbulanceNode* amb = ambulance_head;

    print_header("呼叫120救护车");
    while (amb && amb->status != AMB_IDLE) {
        amb = amb->next;
    }
    if (!amb) {
        printf(COLOR_RED "当前没有可调度的救护车。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    amb->status = AMB_DISPATCHED;
    strcpy(amb->current_location, "出车中");
    add_ambulance_log(amb->vehicle_id, me->id);
    printf(COLOR_GREEN "救护车 %s 已出车。\n" COLOR_RESET, amb->vehicle_id);
    
    // [新增架构逻辑] 联动向患者发送急诊绿色通道直通提示
    printf(COLOR_CYAN "【系统提示】120指挥中心已受理！您到达医院后将自动接入绿色抢救通道。\n" COLOR_RESET);
    
    wait_for_enter();
}

void view_patient_financial_report(const char* patient_id) {
    RecordNode* curr = record_head;
    double total_paid = 0.0;

    print_header("费用统计");
    while (curr) {
        if (strcmp(curr->patient_id, patient_id) == 0 && curr->status == 1) {
            total_paid += curr->personal_paid;
        }
        curr = curr->next;
    }
    printf("累计个人支付: %.2f\n", total_paid);
    wait_for_enter();
}

void register_new_patient() {
    PatientNode* patient = (PatientNode*)calloc(1, sizeof(PatientNode));
    if (!patient) {
        return;
    }

    print_header("挂号新患者");
    {
        char today[32] = {0};
        char ymd8[9] = {0};
        int i = 0;
        int j = 0;
        int seq = 1;

        get_current_time_str(today);
        for (i = 0; today[i] != '\0' && j < 8; ++i) {
            if (today[i] >= '0' && today[i] <= '9') {
                ymd8[j++] = today[i];
            }
        }
        ymd8[8] = '\0';

        seq = get_today_patient_sequence(ymd8);
        snprintf(patient->id, sizeof(patient->id), "P%s%03d", ymd8, seq);
        while (patient_login(patient->id) != NULL) {
            seq++;
            snprintf(patient->id, sizeof(patient->id), "P%s%03d", ymd8, seq);
        }
    }
    printf("姓名: ");
    safe_get_string(patient->name, sizeof(patient->name));
    printf("年龄: ");
    patient->age = safe_get_int();
    printf("性别: ");
    safe_get_string(patient->gender, sizeof(patient->gender));
    printf("医保类型（0-无，1-职工，2-居民）: ");
    patient->insurance = (InsuranceType)safe_get_int();
    patient->type = 'O';
    patient->account_balance = 0.0;
    patient->next = outpatient_head;
    outpatient_head = patient;

    printf(COLOR_GREEN "患者建档成功，新编号：%s\n" COLOR_RESET, patient->id);
    wait_for_enter();
}

void patient_dashboard(PatientNode* me) {
    int choice = -1;

    while (choice != 0) {
        print_header("患者主页");
        printf("患者: %s（%s）\n", me->name, me->id);
        printf("余额    : %.2f\n", me->account_balance);
        print_divider();
        printf("1. 个人信息\n");
        printf("2. 挂号\n");
        printf("3. 排队状态\n");
        printf("4. 缴费\n");
        printf("5. 充值\n");
        printf("6. 呼叫 120 救护车\n");
        printf("7. 查看值班表\n");
        printf("8. 查看病历\n");
        printf("9. 费用统计\n");
        printf("0. 返回\n");
        print_divider();
        printf("请选择: ");
        choice = safe_get_int();

        switch (choice) {
            case 1: view_personal_info(me); break;
            case 2: patient_self_register(me); break;
            case 3: check_queue_status(me); break;
            case 4: patient_pay_bill(me); break;
            case 5: patient_top_up(me); break;
            case 6: call_ambulance_service(me); break;
            case 7: view_duty_schedule(NULL); break;
            case 8: query_medical_records(me->id); break;
            case 9: view_patient_financial_report(me->id); break;
            case 0: break;
            default:
                printf(COLOR_YELLOW "未知选项。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}

void patient_subsystem() {
    char patient_id[MAX_ID_LEN];
    PatientNode* patient;

    print_header("患者端");
    printf("1. 登录\n2. 挂号新患者\n0. 返回\n请选择: ");
    switch (safe_get_int()) {
        case 1:
            printf("患者编号: ");
            safe_get_string(patient_id, sizeof(patient_id));
            patient = patient_login(patient_id);
            if (!patient) {
                printf(COLOR_RED "未找到该患者。\n" COLOR_RESET);
                wait_for_enter();
                return;
            }
            patient_dashboard(patient);
            break;
        case 2:
            register_new_patient();
            break;
        default:
            break;
    }
}