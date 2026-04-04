#include "doctor.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef RECORD_SURGERY
#define RECORD_SURGERY 6
#define RECORD_CHECKUP 7
#endif

static RecordNode* active_visit_record = NULL;

static int record_has_tag(const RecordNode* record, const char* tag) {
    return record && tag && strstr(record->description, tag) != NULL;
}

static void append_record_tag(RecordNode* record, const char* tag) {
    size_t current_len;
    size_t tag_len;

    if (!record || !tag || record_has_tag(record, tag)) {
        return;
    }

    current_len = strlen(record->description);
    tag_len = strlen(tag);
    if (current_len + tag_len + 1 >= sizeof(record->description)) {
        return;
    }
    strcat(record->description, tag);
}

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

static int is_registration_record(const RecordNode* record) {
    return record && (record->type == RECORD_REGISTRATION || record->type == RECORD_EMERGENCY);
}

static int is_waiting_record_for_scene(const RecordNode* record, const char* doctor_id, int scene) {
    if (!record || !doctor_id) {
        return 0;
    }
    if (strcmp(record->doctor_id, doctor_id) != 0 || record->status != 1) {
        return 0;
    }
    if (record_has_tag(record, "|CALLED") || record_has_tag(record, "|DONE")) {
        return 0;
    }
    if (scene == 1) {
        return record->type == RECORD_EMERGENCY;
    }
    return record->type == RECORD_REGISTRATION;
}

static void ensure_operating_rooms_ready(void) {
    if (or_head != NULL) {
        return;
    }

    for (int i = 3; i >= 1; --i) {
        OperatingRoomNode* node = (OperatingRoomNode*)calloc(1, sizeof(OperatingRoomNode));
        if (!node) {
            return;
        }
        snprintf(node->room_id, sizeof(node->room_id), "OR-%02d", i);
        node->status = 0;
        strcpy(node->current_patient, "N/A");
        strcpy(node->current_doctor, "N/A");
        node->next = or_head;
        or_head = node;
    }
}

static OperatingRoomNode* find_operating_room_by_id_local(const char* room_id) {
    OperatingRoomNode* curr = or_head;
    while (curr) {
        if (strcmp(curr->room_id, room_id) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static const char* room_status_text(int status) {
    if (status == 0) return COLOR_GREEN "空闲" COLOR_RESET;
    if (status == 1) return COLOR_RED "使用中" COLOR_RESET;
    return COLOR_YELLOW "消杀中" COLOR_RESET;
}

static int get_current_working_scene(const char* doctor_id) {
    char today[32] = {0};
    char line[256];
    int is_scheduled = 0;
    int is_emergency = 0;
    FILE* fp;

    get_current_time_str(today);
    fp = fopen("data/schedules.txt", "r");
    if (!fp) {
        return 2;
    }

    while (fgets(line, sizeof(line), fp)) {
        char date_buf[32] = {0};
        char slot_buf[32] = {0};
        char dept_buf[32] = {0};
        char doc_buf[32] = {0};

        if (sscanf(line, "%31s %31s %31s %31s", date_buf, slot_buf, dept_buf, doc_buf) != 4) {
            continue;
        }
        if (strcmp(doc_buf, doctor_id) != 0 || strncmp(date_buf, today, 10) != 0) {
            continue;
        }
        is_scheduled = 1;
        if (strstr(dept_buf, "EM") != NULL || strstr(dept_buf, "急诊") != NULL) {
            is_emergency = 1;
            break;
        }
    }
    fclose(fp);

    if (!is_scheduled) {
        return 0;
    }
    return is_emergency ? 1 : 2;
}

static void view_operating_room_board(void) {
    OperatingRoomNode* curr;
    int count = 0;

    ensure_operating_rooms_ready();
    print_header("手术室看板");
    printf("%-10s %-12s %-15s %-15s\n", "手术室", "状态", "医生", "患者");
    print_divider();
    for (curr = or_head; curr; curr = curr->next) {
        printf("%-10s %-12s %-15s %-15s\n",
            curr->room_id,
            room_status_text(curr->status),
            curr->current_doctor,
            curr->current_patient);
        count++;
    }
    if (count == 0) {
        printf(COLOR_YELLOW "当前没有可用手术室。\n" COLOR_RESET);
    }
    wait_for_enter();
}

static void view_ambulance_logs_board(void) {
    AmbulanceLogNode* curr = ambulance_log_head;
    int count = 0;

    print_header("救护车调度记录");
    printf("%-15s %-12s %-22s %-30s\n", "记录", "车辆", "时间", "目的地");
    print_divider();
    while (curr) {
        printf("%-15s %-12s %-22s %-30s\n",
            curr->log_id,
            curr->vehicle_id,
            curr->dispatch_time,
            curr->destination);
        curr = curr->next;
        count++;
    }
    if (count == 0) {
        printf(COLOR_YELLOW "暂无调度记录。\n" COLOR_RESET);
    }
    wait_for_enter();
}

static void mark_active_visit_done(void) {
    if (active_visit_record) {
        append_record_tag(active_visit_record, "|DONE");
        active_visit_record = NULL;
    }
}

int is_prescription_valid(const char* desc) {
    int type = 0;
    int days = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    char date_str[32] = {0};
    struct tm tm_pres = {0};
    time_t time_pres;
    time_t time_now;
    double diff_sec;
    int diff_days;

    if (!desc || strncmp(desc, "PRES", 4) != 0) {
        return 1;
    }

    if (sscanf(desc, "PRES|T:%d|D:%d|ID:%*[^|]|Q:%*d|DATE:%31s", &type, &days, date_str) != 3) {
        return 1;
    }
    if (sscanf(date_str, "%d-%d-%d", &year, &month, &day) != 3) {
        return 1;
    }

    tm_pres.tm_year = year - 1900;
    tm_pres.tm_mon = month - 1;
    tm_pres.tm_mday = day;
    time_pres = mktime(&tm_pres);
    time_now = time(NULL);
    diff_sec = difftime(time_now, time_pres);
    diff_days = (int)(diff_sec / (24 * 3600));

    if (diff_days > days) {
        printf(COLOR_RED "处方已过期：开具日期 %s，有效期 %d 天。\n" COLOR_RESET, date_str, days);
        return 0;
    }
    return 1;
}

void view_my_queue(DoctorNode* doctor) {
    RecordNode* curr = record_head;
    int scene = get_current_working_scene(doctor->id);
    int count = 0;

    print_header(scene == 1 ? "急诊队列" : "医生候诊队列");
    while (curr) {
        if (is_waiting_record_for_scene(curr, doctor->id, scene)) {
            PatientNode* patient = find_outpatient_by_id(curr->patient_id);
            if (!patient) {
                patient = find_inpatient_by_id(curr->patient_id);
            }
            printf("[%03d] %-18s %-10s %-12s\n",
                curr->queue_number,
                curr->record_id,
                curr->patient_id,
                patient ? patient->name : "未知");
            count++;
        }
        curr = curr->next;
    }
    if (count == 0) {
        printf(COLOR_YELLOW "当前没有等待该医生接诊的患者。\n" COLOR_RESET);
    }
    wait_for_enter();
}

PatientNode* call_next_patient(DoctorNode* doctor) {
    RecordNode* curr = record_head;
    RecordNode* target = NULL;
    int scene = get_current_working_scene(doctor->id);
    int min_queue = 2147483647;

    while (curr) {
        if (is_waiting_record_for_scene(curr, doctor->id, scene) && curr->queue_number < min_queue) {
            min_queue = curr->queue_number;
            target = curr;
        }
        curr = curr->next;
    }

    if (!target) {
        printf(COLOR_CYAN "当前没有已缴费且在候诊中的挂号记录。\n" COLOR_RESET);
        return NULL;
    }

    append_record_tag(target, "|CALLED");
    active_visit_record = target;

    PatientNode* patient = find_outpatient_by_id(target->patient_id);
    if (!patient) {
        patient = find_inpatient_by_id(target->patient_id);
    }

    if (patient) {
        printf(COLOR_GREEN "正在叫号：%s（%s）。\n" COLOR_RESET, patient->name, patient->id);
    }
    return patient;
}

void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient) {
    int type_choice;
    int days;
    int quantity;
    char medicine_id[MAX_ID_LEN];
    char today[32] = {0};
    char desc[MAX_DESC_LEN];
    MedicineNode* medicine;

    (void)doctor;
    print_header("处方开立");
    printf("1. 普通处方（7天）\n2. 急诊处方（3天）\n3. 慢病处方（30天）\n请选择: ");
    type_choice = safe_get_int();
    if (type_choice < 1 || type_choice > 3) {
        type_choice = 1;
    }
    days = (type_choice == 2) ? 3 : (type_choice == 3 ? 30 : 7);

    printf("药品编号: ");
    safe_get_string(medicine_id, sizeof(medicine_id));
    medicine = find_medicine_by_id(medicine_id);
    if (!medicine || medicine->stock <= 0) {
        printf(COLOR_RED "药品不存在或库存不足。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("数量: ");
    quantity = safe_get_int();
    if (quantity <= 0 || quantity > medicine->stock) {
        printf(COLOR_RED "数量无效或库存不足。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    get_current_time_str(today);
    snprintf(desc, sizeof(desc), "PRES|T:%d|D:%d|ID:%s|Q:%d|DATE:%s", type_choice, days, medicine->id, quantity, today);
    add_new_record(patient->id, doctor->id, RECORD_CONSULTATION, desc, medicine->price * quantity, 0);
    if (record_head) {
        printf(COLOR_CYAN "处方记录编号: %s\n" COLOR_RESET, record_head->record_id);
    }
    printf(COLOR_GREEN "处方开立成功，有效期 %d 天。\n" COLOR_RESET, days);
    wait_for_enter();
}

void dispense_medicine_service() {
    char record_id[MAX_ID_LEN];
    RecordNode* record;
    char medicine_id[MAX_ID_LEN] = {0};
    int quantity = 0;
    MedicineNode* medicine;

    print_header("药房发药");
    {
        RecordNode* curr = record_head;
        int shown = 0;
        print_divider();
        printf("%-18s %-12s %-8s %-10s\n", "记录编号", "患者", "状态", "应付");
        while (curr) {
            if (strstr(curr->description, "PRES|") != NULL && strstr(curr->description, "|DISPENSED") == NULL) {
                printf("%-18s %-12s %-8s %-10.2f\n",
                    curr->record_id,
                    curr->patient_id,
                    curr->status == 1 ? "已缴费" : "待缴费",
                    curr->personal_paid);
                shown++;
            }
            curr = curr->next;
        }
        if (shown == 0) {
            printf(COLOR_YELLOW "当前没有可发药的处方。\n" COLOR_RESET);
            wait_for_enter();
            return;
        }
        print_divider();
    }
    printf("记录编号: ");
    safe_get_string(record_id, sizeof(record_id));
    record = find_record_by_id_local(record_id);
    if (!record || !record_has_tag(record, "PRES|")) {
        printf(COLOR_RED "未找到对应处方记录编号。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (record->status == 0) {
        printf(COLOR_RED "该处方尚未缴费。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (record_has_tag(record, "|DISPENSED")) {
        printf(COLOR_YELLOW "该处方已经发药。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (!is_prescription_valid(record->description)) {
        wait_for_enter();
        return;
    }
    if (sscanf(record->description, "PRES|T:%*d|D:%*d|ID:%31[^|]|Q:%d", medicine_id, &quantity) != 2) {
        printf(COLOR_RED "处方数据格式异常。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    medicine = find_medicine_by_id(medicine_id);
    if (!medicine || medicine->stock < quantity) {
        printf(COLOR_RED "该处方对应药品库存不足。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    medicine->stock -= quantity;
    append_record_tag(record, "|DISPENSED");
    printf(COLOR_GREEN "发药成功：%s x %d。\n" COLOR_RESET, medicine->common_name, quantity);
    wait_for_enter();
}

void prescribe_checkup_service(DoctorNode* doctor, PatientNode* patient) {
    char item_name[128];
    double fee;
    char desc[MAX_DESC_LEN];

    print_header("检查单开立");
    printf("检查项目: ");
    safe_get_string(item_name, sizeof(item_name));
    printf("费用: ");
    fee = safe_get_double();
    if (fee < 0) {
        fee = 0;
    }

    snprintf(desc, sizeof(desc), "CHK_PENDING|%s", item_name);
    add_new_record(patient->id, doctor->id, RECORD_CHECKUP, desc, fee, 0);
    if (record_head) {
        printf(COLOR_CYAN "\u68c0\u67e5\u5355\u8bb0\u5f55\u7f16\u53f7: %s\n" COLOR_RESET, record_head->record_id);
    }
    printf(COLOR_GREEN "检查单已开立，患者可前往缴费。\n" COLOR_RESET);
    wait_for_enter();
}

void generate_checkup_report(DoctorNode* doctor) {
    char record_id[MAX_ID_LEN];
    char result[256];
    char new_desc[MAX_DESC_LEN];
    RecordNode* record;
    PatientNode* patient;

    print_header("生成检查报告");
    {
        RecordNode* curr = record_head;
        int shown = 0;
        print_divider();
        printf("%-18s %-12s %-8s %-20s\n", "\u8bb0\u5f55\u7f16\u53f7", "\u60a3\u8005", "\u72b6\u6001", "\u68c0\u67e5\u9879\u76ee");
        while (curr) {
            if (curr->type == RECORD_CHECKUP && !record_has_tag(curr, "CHK_DONE|")) {
                const char* item_name = "-";
                if (strncmp(curr->description, "CHK_PENDING|", 12) == 0 && curr->description[12] != '\0') {
                    item_name = curr->description + 12;
                }
                printf("%-18s %-12s %-8s %-20s\n",
                    curr->record_id,
                    curr->patient_id,
                    curr->status == 1 ? "\u5df2\u7f34\u8d39" : "\u5f85\u7f34\u8d39",
                    item_name);
                shown++;
            }
            curr = curr->next;
        }
        if (shown == 0) {
            printf(COLOR_YELLOW "\u5f53\u524d\u6ca1\u6709\u5f85\u5b8c\u6210\u7684\u68c0\u67e5\u5355\u3002\n" COLOR_RESET);
            wait_for_enter();
            return;
        }
        print_divider();
    }
    printf("检查记录编号: ");
    safe_get_string(record_id, sizeof(record_id));
    record = find_record_by_id_local(record_id);
    if (!record || record->type != RECORD_CHECKUP) {
        printf(COLOR_RED "未找到检查记录。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (record->status == 0) {
        printf(COLOR_RED "该检查尚未缴费。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (record_has_tag(record, "CHK_DONE|")) {
        printf(COLOR_YELLOW "该检查报告已经生成。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("结果摘要: ");
    safe_get_string(result, sizeof(result));
    snprintf(new_desc, sizeof(new_desc), "CHK_DONE|%s", result);
    strcpy(record->description, new_desc);

    patient = find_outpatient_by_id(record->patient_id);
    if (!patient) {
        patient = find_inpatient_by_id(record->patient_id);
    }

    printf("\n");
    print_divider();
    printf("检查报告\n");
    print_divider();
    printf("患者  : %s\n", patient ? patient->name : "未知");
    printf("记录  : %s\n", record->record_id);
    printf("医生  : %s\n", doctor->name);
    printf("日期  : %s\n", record->date);
    printf("结果  : %s\n", result);
    print_divider();
    wait_for_enter();
}

void admission_service(DoctorNode* doctor, PatientNode* patient) {
    int days;
    double deposit;
    double min_deposit;
    int type_val;
    WardType target_type;
    BedNode* bed;
    PatientNode* inpatient;

    print_header("住院收治");
    if (patient->type == 'I') {
        printf(COLOR_YELLOW "该患者已在住院中。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("预计住院天数: ");
    days = safe_get_int();
    if (days <= 0) {
        days = 1;
    }
    min_deposit = 200.0 * days;
    printf("最低押金 %.2f（且必须为100的整数倍）\n", min_deposit);
    printf("实收押金: ");
    deposit = safe_get_double();

    if (deposit < min_deposit || ((int)deposit % 100) != 0 || deposit != (double)((int)deposit)) {
        printf(COLOR_RED "押金规则校验未通过。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("病房类型（1-ICU，2-普通，3-陪护）: ");
    type_val = safe_get_int();
    target_type = (type_val == 1) ? WARD_SPECIAL : ((type_val == 3) ? WARD_ESCORTED : WARD_GENERAL);
    bed = find_available_bed(doctor->dept_id, target_type);
    if (!bed) {
        printf(COLOR_RED "收治失败：本科室当前无可用床位。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("预计出院日期（YYYY-MM-DD）: ");
    safe_get_string(patient->expected_discharge_date, sizeof(patient->expected_discharge_date));

    if (!transfer_outpatient_to_inpatient(patient->id)) {
        printf(COLOR_RED "患者从门诊转入住院列表失败。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    inpatient = find_inpatient_by_id(patient->id);
    if (!inpatient) {
        printf(COLOR_RED "转入住院后未能在住院患者列表中找到该患者。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    inpatient->hospital_deposit = deposit;
    strcpy(inpatient->bed_id, bed->bed_id);
    get_current_time_str(inpatient->admission_date);
    bed->is_occupied = 1;
    strcpy(bed->patient_id, inpatient->id);

    add_new_record(inpatient->id, doctor->id, RECORD_HOSPITALIZATION, "住院办理费", 50.0, 0);
    printf(COLOR_GREEN "收治完成：%s -> 床位 %s\n" COLOR_RESET, inpatient->name, bed->bed_id);
    wait_for_enter();
}

void book_surgery_service(DoctorNode* doctor, PatientNode* patient) {
    char surgery_name[128];
    double fee;
    char desc[MAX_DESC_LEN];
    OperatingRoomNode* curr;
    OperatingRoomNode* target_room = NULL;

    ensure_operating_rooms_ready();
    print_header("安排手术");
    printf("手术名称: ");
    safe_get_string(surgery_name, sizeof(surgery_name));
    printf("预估费用: ");
    fee = safe_get_double();
    if (fee < 0) {
        fee = 0;
    }

    for (curr = or_head; curr; curr = curr->next) {
        if (curr->status == 1) {
            if (strcmp(curr->current_doctor, doctor->id) == 0) {
                printf(COLOR_RED "医生 %s 当前已在 %s 手术中。\n" COLOR_RESET, doctor->id, curr->room_id);
                wait_for_enter();
                return;
            }
            if (strcmp(curr->current_patient, patient->id) == 0) {
                printf(COLOR_RED "患者 %s 当前已在手术室 %s 中。\n" COLOR_RESET, patient->id, curr->room_id);
                wait_for_enter();
                return;
            }
        }
        if (!target_room && curr->status == 0) {
            target_room = curr;
        }
    }

    if (!target_room) {
        printf(COLOR_RED "当前没有空闲手术室可供安排。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    target_room->status = 1;
    strcpy(target_room->current_doctor, doctor->id);
    strcpy(target_room->current_patient, patient->id);

    snprintf(desc, sizeof(desc), "SURGERY|%s|ROOM:%s", surgery_name, target_room->room_id);
    add_new_record(patient->id, doctor->id, RECORD_SURGERY, desc, fee, 0);
    printf(COLOR_GREEN "手术已安排至 %s。\n" COLOR_RESET, target_room->room_id);
    wait_for_enter();
}

void finish_surgery_service(DoctorNode* doctor) {
    char room_id[MAX_ID_LEN] = {0};
    OperatingRoomNode* curr = or_head;
    OperatingRoomNode* target = NULL;
    int shown = 0;
    char finished_patient[MAX_ID_LEN] = {0};

    ensure_operating_rooms_ready();
    print_header("\u624b\u672f\u7ed3\u675f\u5e76\u79fb\u51fa\u60a3\u8005");
    print_divider();
    printf("%-10s %-12s %-15s %-15s\n", "???", "??", "??", "??");
    while (curr) {
        if (curr->status == 1) {
            printf("%-10s %-12s %-15s %-15s\n", curr->room_id, "???", curr->current_doctor, curr->current_patient);
            shown++;
        }
        curr = curr->next;
    }

    if (shown == 0) {
        printf(COLOR_YELLOW "\u5f53\u524d\u6ca1\u6709\u8fdb\u884c\u4e2d\u7684\u624b\u672f\u3002\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    print_divider();
    printf("\u8bf7\u8f93\u5165\u624b\u672f\u5ba4\u7f16\u53f7(\u7559\u7a7a\u81ea\u52a8\u9009\u62e9\u672c\u4eba\u9996\u53f0): ");
    safe_get_string(room_id, sizeof(room_id));

    if (room_id[0] == '\0') {
        for (curr = or_head; curr; curr = curr->next) {
            if (curr->status == 1 && strcmp(curr->current_doctor, doctor->id) == 0) {
                target = curr;
                break;
            }
        }
        if (!target) {
            printf(COLOR_RED "\u672a\u627e\u5230\u5f53\u524d\u533b\u751f\u53ef\u7ed3\u675f\u7684\u624b\u672f\u3002\n" COLOR_RESET);
            wait_for_enter();
            return;
        }
    } else {
        target = find_operating_room_by_id_local(room_id);
        if (!target || target->status != 1) {
            printf(COLOR_RED "\u672a\u627e\u5230\u8fdb\u884c\u4e2d\u7684\u8be5\u624b\u672f\u5ba4\u8bb0\u5f55\u3002\n" COLOR_RESET);
            wait_for_enter();
            return;
        }
        if (strcmp(target->current_doctor, doctor->id) != 0) {
            printf(COLOR_RED "\u8be5\u624b\u672f\u4e0d\u5c5e\u4e8e\u5f53\u524d\u533b\u751f\uff0c\u65e0\u6cd5\u7ed3\u675f\u3002\n" COLOR_RESET);
            wait_for_enter();
            return;
        }
    }

    strncpy(finished_patient, target->current_patient, sizeof(finished_patient) - 1);
    finished_patient[sizeof(finished_patient) - 1] = '\0';

    target->status = 0;
    strcpy(target->current_doctor, "N/A");
    strcpy(target->current_patient, "N/A");

    for (RecordNode* rec = record_head; rec; rec = rec->next) {
        if (rec->type == RECORD_SURGERY && strcmp(rec->patient_id, finished_patient) == 0 && !record_has_tag(rec, "|SURG_DONE")) {
            append_record_tag(rec, "|SURG_DONE");
            break;
        }
    }

    printf(COLOR_GREEN "\u624b\u672f\u5df2\u7ed3\u675f\uff1a\u624b\u672f\u5ba4 %s\uff0c\u60a3\u8005 %s \u5df2\u79fb\u51fa\u3002\n" COLOR_RESET, target->room_id, finished_patient);
    wait_for_enter();
}

void transfer_patient_to_icu(DoctorNode* doctor) {
    char patient_id[MAX_ID_LEN];
    PatientNode* patient;
    BedNode* icu_bed;
    BedNode* old_bed;

    print_header("转入 ICU");
    printf("住院患者编号: ");
    safe_get_string(patient_id, sizeof(patient_id));

    patient = find_inpatient_by_id(patient_id);
    if (!patient) {
        printf(COLOR_RED "未找到住院患者。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    icu_bed = find_available_bed(doctor->dept_id, WARD_SPECIAL);
    if (!icu_bed) {
        printf("\n" COLOR_RED "[警告] 本科室 ICU 床位已满。\n" COLOR_RESET);
        printf(COLOR_YELLOW "请尽快申请临时加床或启动降级处置流程。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    old_bed = find_bed_by_id(patient->bed_id);
    if (old_bed) {
        old_bed->is_occupied = 0;
        strcpy(old_bed->patient_id, "N/A");
    }

    strcpy(patient->bed_id, icu_bed->bed_id);
    icu_bed->is_occupied = 1;
    strcpy(icu_bed->patient_id, patient->id);
    add_new_record(patient->id, doctor->id, RECORD_HOSPITALIZATION, "[ICU转入]重症监护医嘱", 1500.0, 0);

    printf(COLOR_GREEN "ICU 转入完成：%s -> %s\n" COLOR_RESET, patient->name, icu_bed->bed_id);
    wait_for_enter();
}

void discharge_inpatient_service(DoctorNode* doctor) {
    char patient_id[MAX_ID_LEN] = {0};
    PatientNode* curr = inpatient_head;
    PatientNode* prev = NULL;
    BedNode* bed = NULL;
    OperatingRoomNode* room = NULL;
    int shown = 0;

    print_header("\u529e\u7406\u4f4f\u9662\u60a3\u8005\u51fa\u9662");
    print_divider();
    printf("%-12s %-12s %-12s\n", "\u60a3\u8005\u7f16\u53f7", "\u59d3\u540d", "\u5e8a\u4f4d");
    for (curr = inpatient_head; curr; curr = curr->next) {
        bed = find_bed_by_id(curr->bed_id);
        if (bed && strcmp(bed->dept_id, doctor->dept_id) == 0) {
            printf("%-12s %-12s %-12s\n", curr->id, curr->name, curr->bed_id);
            shown++;
        }
    }
    if (shown == 0) {
        printf(COLOR_YELLOW "\u5f53\u524d\u79d1\u5ba4\u6ca1\u6709\u53ef\u51fa\u9662\u7684\u4f4f\u9662\u60a3\u8005\u3002\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    print_divider();

    printf("\u8bf7\u8f93\u5165\u8981\u51fa\u9662\u7684\u60a3\u8005\u7f16\u53f7: ");
    safe_get_string(patient_id, sizeof(patient_id));

    curr = inpatient_head;
    prev = NULL;
    while (curr && strcmp(curr->id, patient_id) != 0) {
        prev = curr;
        curr = curr->next;
    }

    if (!curr) {
        printf(COLOR_RED "\u672a\u627e\u5230\u8be5\u4f4f\u9662\u60a3\u8005\u3002\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    bed = find_bed_by_id(curr->bed_id);
    if (bed && strcmp(bed->dept_id, doctor->dept_id) != 0) {
        printf(COLOR_RED "\u8be5\u60a3\u8005\u4e0d\u5728\u5f53\u524d\u533b\u751f\u79d1\u5ba4\uff0c\u65e0\u6cd5\u529e\u7406\u51fa\u9662\u3002\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    for (room = or_head; room; room = room->next) {
        if (room->status == 1 && strcmp(room->current_patient, curr->id) == 0) {
            printf(COLOR_RED "\u8be5\u60a3\u8005\u4ecd\u5728\u624b\u672f\u4e2d\uff0c\u8bf7\u5148\u6267\u884c\u624b\u672f\u7ed3\u675f\u79fb\u51fa\u3002\n" COLOR_RESET);
            wait_for_enter();
            return;
        }
    }

    if (bed) {
        bed->is_occupied = 0;
        strcpy(bed->patient_id, "N/A");
    }

    if (prev) {
        prev->next = curr->next;
    } else {
        inpatient_head = curr->next;
    }

    curr->type = 'O';
    curr->hospital_deposit = 0.0;
    curr->admission_date[0] = '\0';
    curr->expected_discharge_date[0] = '\0';
    strcpy(curr->bed_id, "N/A");

    curr->next = outpatient_head;
    outpatient_head = curr;

    printf(COLOR_GREEN "\u51fa\u9662\u529e\u7406\u5b8c\u6210\uff1a%s(%s) \u5df2\u79fb\u51fa\u4f4f\u9662\u5e76\u91ca\u653e\u5e8a\u4f4d\u3002\n" COLOR_RESET, curr->name, curr->id);
    printf(COLOR_CYAN "\u6309\u4f60\u7684\u8981\u6c42\uff1a\u51fa\u9662\u4e0d\u518d\u5f3a\u5236\u7f34\u6e05\u8d39\u7528\u3002\n" COLOR_RESET);
    wait_for_enter();
}

void view_doctor_statistics(DoctorNode* doctor) {
    RecordNode* curr = record_head;
    char today[32] = {0};
    int today_count = 0;
    double total_revenue = 0.0;

    get_current_time_str(today);
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 1) {
            if (is_registration_record(curr) &&
                strncmp(curr->date, today, 10) == 0 &&
                (record_has_tag(curr, "|CALLED") || record_has_tag(curr, "|DONE"))) {
                today_count++;
            }
            total_revenue += curr->total_fee;
        }
        curr = curr->next;
    }

    print_header("医生统计");
    printf("今日接诊量: %d\n", today_count);
    printf("累计收入  : %.2f\n", total_revenue);
    wait_for_enter();
}

void view_department_inpatients(DoctorNode* doctor) {
    PatientNode* curr = inpatient_head;
    int found = 0;

    print_header("科室住院患者");
    while (curr) {
        BedNode* bed = find_bed_by_id(curr->bed_id);
        if (bed && strcmp(bed->dept_id, doctor->dept_id) == 0) {
            printf("床位 %-10s 患者 %-12s（%s）\n", bed->bed_id, curr->name, curr->id);
            found = 1;
        }
        curr = curr->next;
    }
    if (!found) {
        printf(COLOR_YELLOW "当前科室暂无住院患者。\n" COLOR_RESET);
    }
    wait_for_enter();
}

void doctor_subsystem() {
    DoctorNode* me;
    PatientNode* current_patient = NULL;
    char doctor_id[MAX_ID_LEN];
    int scene;
    int choice = -1;

    print_header("医生工作台");
    printf("医生工号: ");
    safe_get_string(doctor_id, sizeof(doctor_id));
    me = find_doctor_by_id(doctor_id);
    if (!me) {
        printf(COLOR_RED "医生登录失败。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    scene = get_current_working_scene(me->id);
    if (scene == 0) {
        printf(COLOR_YELLOW "今天没有排班记录，系统将以门诊模式打开。\n" COLOR_RESET);
        scene = 2;
        wait_for_enter();
    }

    active_visit_record = NULL;
    while (choice != 0) {
        print_header(scene == 1 ? "医生工作台-急诊" : "医生工作台-门诊");
        printf("医生: %s（%s）\n", me->name, me->id);
        printf("科室: %s\n", me->dept_id);
        printf("当前患者: %s\n", current_patient ? current_patient->name : "空闲");
        print_divider();
        printf(" 1. 查看候诊队列\n");
        printf(" 2. 呼叫下一位患者\n");
        printf(" 3. 结束当前接诊\n");
        printf(" 4. 开立处方\n");
        printf(" 5. 药房发药\n");
        printf(" 6. 开立检查单\n");
        printf(" 7. 生成检查报告\n");
        printf(" 8. 安排手术\n");
        printf(" 9. 办理住院收治\n");
        printf("10. 转入住院患者到 ICU\n");
        printf("11. 查看科室住院患者\n");
        printf("12. 查看手术室看板\n");
        printf("13. 查看救护调度记录\n");
        printf("14. 查看统计信息\n");
        printf("15. \u624b\u672f\u7ed3\u675f\u5e76\u79fb\u51fa\u60a3\u8005\n");
        printf("16. \u529e\u7406\u4f4f\u9662\u60a3\u8005\u51fa\u9662\n");
        printf(" 0. 返回\n");
        print_divider();
        printf("请选择: ");
        choice = safe_get_int();

        switch (choice) {
            case 1:
                view_my_queue(me);
                break;
            case 2:
                if (current_patient) {
                    printf(COLOR_YELLOW "请先完成当前患者的接诊。\n" COLOR_RESET);
                    wait_for_enter();
                } else {
                    current_patient = call_next_patient(me);
                }
                break;
            case 3:
                if (current_patient) {
                    mark_active_visit_done();
                    printf(COLOR_GREEN "%s 的接诊已结束。\n" COLOR_RESET, current_patient->name);
                    current_patient = NULL;
                } else {
                    printf(COLOR_YELLOW "当前没有正在接诊的患者。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            case 4:
                if (current_patient) {
                    prescribe_medicine_advanced(me, current_patient);
                } else {
                    printf(COLOR_RED "请先呼叫患者。\n" COLOR_RESET);
                    wait_for_enter();
                }
                break;
            case 5:
                dispense_medicine_service();
                break;
            case 6:
                if (current_patient) {
                    prescribe_checkup_service(me, current_patient);
                } else {
                    printf(COLOR_RED "请先呼叫患者。\n" COLOR_RESET);
                    wait_for_enter();
                }
                break;
            case 7:
                generate_checkup_report(me);
                break;
            case 8:
                if (current_patient) {
                    book_surgery_service(me, current_patient);
                } else {
                    printf(COLOR_RED "请先呼叫患者。\n" COLOR_RESET);
                    wait_for_enter();
                }
                break;
            case 9:
                if (current_patient) {
                    admission_service(me, current_patient);
                } else {
                    printf(COLOR_RED "请先呼叫一位门诊患者。\n" COLOR_RESET);
                    wait_for_enter();
                }
                break;
            case 10:
                transfer_patient_to_icu(me);
                break;
            case 11:
                view_department_inpatients(me);
                break;
            case 12:
                view_operating_room_board();
                break;
            case 13:
                view_ambulance_logs_board();
                break;
            case 14:
                view_doctor_statistics(me);
                break;
            case 15:
                finish_surgery_service(me);
                break;
            case 16:
                discharge_inpatient_service(me);
                break;
            case 0:
                mark_active_visit_done();
                current_patient = NULL;
                break;
            default:
                printf(COLOR_YELLOW "未知选项。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}



