#include "common.h"
#include "data_manager.h"
#include "admin.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h> // [新增逻辑] 用于字符串转换小写

/* ==========================================================================
 * [新增逻辑] 核心业务规则与辅助引擎
 * ========================================================================== */

// 1. 救护车关键字自动扫描引擎
static int detect_ambulance_signal(const char* desc) {
    if (!desc) return 0;
    char buffer[MAX_DESC_LEN];
    strncpy(buffer, desc, MAX_DESC_LEN - 1);
    buffer[MAX_DESC_LEN - 1] = '\0';
    for(int i = 0; buffer[i]; i++) buffer[i] = (char)tolower(buffer[i]);

    if (strstr(buffer, "救护车") || strstr(buffer, "ambulance") || 
        strstr(buffer, "120") || strstr(buffer, "急救")) {
        return 1;
    }
    return 0;
}

// 2. 医生资质翻译器
static const char* get_title_name(int level) {
    switch (level) {
        case 4: return "正主任医师(Lv4)";
        case 3: return "副主任医师(Lv3)";
        case 2: return "主治医师(Lv2)";
        case 1: return "住院医师(Lv1)";
        default: return "未定岗(Lv0)";
    }
}

/* ==========================================================================
 * 原始底层函数 (100% 完整保留) 
 * ========================================================================== */

static PatientNode* find_patient_any(const char* patient_id) {
    PatientNode* patient = find_outpatient_by_id(patient_id);
    if (patient) return patient;
    return find_inpatient_by_id(patient_id);
}

static const char* record_type_text(RecordType type) {
    switch (type) {
        case RECORD_REGISTRATION: return "挂号";
        case RECORD_CONSULTATION: return "问诊/处方";
        case RECORD_EXAMINATION: return "检查";
        case RECORD_HOSPITALIZATION: return "住院";
        default: return "未知";
    }
}

static const char* record_status_text(int status) {
    if (status == 1) return "已完成";
    if (status == 0) return "待处理";
    if (status == -1) return "已撤销";
    return "未知";
}

static int parse_date_key(const char* date_text) {
    int year = 0;
    int month = 0;
    int day = 0;

    if (!date_text || sscanf(date_text, "%d-%d-%d", &year, &month, &day) != 3) {
        return -1;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31 || year < 2000 || year > 2100) {
        return -1;
    }

    return year * 10000 + month * 100 + day;
}

static void print_record_item(const RecordNode* record) {
    if (!record) return;

    printf("记录号:%-16s 患者:%-10s 医生:%-8s 类型:%-8s 状态:%s\n",
           record->record_id,
           record->patient_id,
           record->doctor_id,
           record_type_text(record->type),
           record_status_text(record->status));
    printf("日期:%-12s 队号:%-4d 金额:%.2f 描述:%s\n",
           record->date,
           record->queue_number,
           record->total_fee,
           record->description);
    print_divider();
}

static RecordNode* find_record_by_id(const char* record_id) {
    RecordNode* current = record_head;
    while (current) {
        if (strcmp(current->record_id, record_id) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static int validate_record_input(const char* patient_id,
                                 const char* doctor_id,
                                 int type,
                                 double fee,
                                 int queue_number,
                                 const char* description) {
    if (!patient_id || patient_id[0] == '\0' || !find_patient_any(patient_id)) {
        printf(COLOR_RED "错误：患者编号不存在。\n" COLOR_RESET);
        return 0;
    }
    if (!doctor_id || doctor_id[0] == '\0' || !find_doctor_by_id(doctor_id)) {
        printf(COLOR_RED "错误：医生工号不存在。\n" COLOR_RESET);
        return 0;
    }
    if (type < (int)RECORD_REGISTRATION || type > (int)RECORD_HOSPITALIZATION) {
        printf(COLOR_RED "错误：记录类型必须在 1-4 之间。\n" COLOR_RESET);
        return 0;
    }
    if (fee < 0) {
        printf(COLOR_RED "错误：费用不能小于 0。\n" COLOR_RESET);
        return 0;
    }
    if (queue_number < 0) {
        printf(COLOR_RED "错误：排队号不能小于 0。\n" COLOR_RESET);
        return 0;
    }
    if (!description || description[0] == '\0') {
        printf(COLOR_RED "错误：描述不能为空。\n" COLOR_RESET);
        return 0;
    }
    return 1;
}

static int input_record_payload(char* patient_id,
                                char* doctor_id,
                                int* type,
                                double* fee,
                                int* queue_number,
                                char* description) {
    printf("请输入患者编号: ");
    safe_get_string(patient_id, MAX_ID_LEN);

    printf("请输入医生工号: ");
    safe_get_string(doctor_id, MAX_ID_LEN);

    printf("请输入记录类型(1挂号 2问诊 3检查 4住院): ");
    *type = safe_get_int();

    printf("请输入费用(元): ");
    *fee = safe_get_double();

    printf("请输入排队号(无则填0): ");
    *queue_number = safe_get_int();

    printf("请输入描述: ");
    safe_get_string(description, MAX_DESC_LEN);

    return validate_record_input(patient_id, doctor_id, *type, *fee, *queue_number, description);
}

/* ==========================================================================
 * 混合业务逻辑层 (在原代码基础上嵌入高级验证) 
 * ========================================================================== */

void add_record_interactive() {
    char patient_id[MAX_ID_LEN] = {0};
    char doctor_id[MAX_ID_LEN] = {0};
    char description[MAX_DESC_LEN] = {0};
    int type = 0;
    int queue_number = 0;
    double fee = 0.0;

    print_header("新增单条诊疗记录 (融合急救高级分诊)");
    
    // 1. 保留原有的基础数据输入
    if (!input_record_payload(patient_id, doctor_id, &type, &fee, &queue_number, description)) {
        wait_for_enter();
        return;
    }

    // [新增逻辑] 2. 高级安全与排班校验
    char date[32];
    int req_level;
    char intern_id[MAX_ID_LEN];

    printf(COLOR_YELLOW "\n--- 医疗安全校验（高权限系统）---\n" COLOR_RESET);
    printf("请输入当前诊疗日期(YYYY-MM-DD): ");
    safe_get_string(date, 32);
    printf("该操作所需最低资质等级(1-住院医, 4-主任医): ");
    req_level = safe_get_int();

    DoctorNode* doc = find_doctor_by_id(doctor_id);
    if (doc && doc->privilege_level < req_level) {
        printf(COLOR_RED "【拦截】资质不符！需要:%s，当前医生:%s。\n" COLOR_RESET, 
               get_title_name(req_level), get_title_name(doc->privilege_level));
        wait_for_enter(); return;
    }

    const char* slot = (type == 4) ? "急诊" : "门诊";
    if (!verify_doctor_shift(doctor_id, date, slot)) {
        printf(COLOR_RED "【拦截】排班冲突！医生 %s 在 %s 未排 %s 班次。\n" COLOR_RESET, 
               doc->name, date, slot);
        wait_for_enter(); return;
    }

    // [新增逻辑] 3. 救护车智能预警系统
    if (detect_ambulance_signal(description)) {
        queue_number = 1;
        printf(COLOR_CYAN "【智能干预】检测到紧急/救护车来源，已将该患者列为最高优先级(Pos: 1)\n" COLOR_RESET);
    }

    // [新增逻辑] 4. 住院双医生绑定责任制
    if (type == 4) {
        printf("【重症流程】请指派负责该患者的管床医生(Resident)工号: ");
        safe_get_string(intern_id, MAX_ID_LEN);
        if (!find_doctor_by_id(intern_id)) {
            printf(COLOR_RED "错误：管床医生不存在，分配终止。\n" COLOR_RESET);
            wait_for_enter(); return;
        }
        printf(COLOR_GREEN "已绑定：主治团队 与 管床团队。\n" COLOR_RESET);
    }

    // 执行底层原版录入逻辑
    add_new_record(patient_id, doctor_id, (RecordType)type, description, fee, queue_number);
    printf(COLOR_GREEN "新增成功。\n" COLOR_RESET);
    wait_for_enter();
}

static int parse_import_line(char* line,
                             char* patient_id,
                             char* doctor_id,
                             int* type,
                             double* fee,
                             int* queue_number,
                             char* description) {
    char* token = NULL;
    char* fields[6] = {0};
    int count = 0;

    token = strtok(line, "\t");
    while (token && count < 6) {
        fields[count++] = token;
        token = strtok(NULL, "\t");
    }

    if (count >= 6) {
        strncpy(patient_id, fields[0], MAX_ID_LEN - 1);
        strncpy(doctor_id, fields[1], MAX_ID_LEN - 1);
        *type = atoi(fields[2]);
        *fee = strtod(fields[3], NULL);
        *queue_number = atoi(fields[4]);
        strncpy(description, fields[5], MAX_DESC_LEN - 1);
        return 1;
    }

    if (sscanf(line, "%31s %31s %d %lf %d %511[^\n]", patient_id, doctor_id, type, fee, queue_number, description) == 6) {
        return 1;
    }

    return 0;
}

void import_records_from_file() {
    char path[260] = {0};
    FILE* file = NULL;
    char line[1024];
    int line_number = 0;
    int success_count = 0;
    int failed_count = 0;

    print_header("从文件批量导入诊疗记录");
    printf("请输入导入文件路径: ");
    safe_get_string(path, sizeof(path));

    file = fopen(path, "r");
    if (!file) {
        printf(COLOR_RED "无法打开文件: %s\n" COLOR_RESET, path);
        wait_for_enter();
        return;
    }

    while (fgets(line, sizeof(line), file)) {
        char patient_id[MAX_ID_LEN] = {0};
        char doctor_id[MAX_ID_LEN] = {0};
        char description[MAX_DESC_LEN] = {0};
        int type = 0;
        int queue_number = 0;
        double fee = 0.0;
        char raw[1024];

        line_number++;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        if (line_number == 1 &&
            (strstr(line, "patient") || strstr(line, "患者") || strstr(line, "病人"))) {
            continue;
        }

        strncpy(raw, line, sizeof(raw) - 1);
        raw[sizeof(raw) - 1] = '\0';

        if (!parse_import_line(line, patient_id, doctor_id, &type, &fee, &queue_number, description)) {
            printf(COLOR_YELLOW "第 %d 行格式错误，已跳过: %s\n" COLOR_RESET, line_number, raw);
            failed_count++;
            continue;
        }

        if (!validate_record_input(patient_id, doctor_id, type, fee, queue_number, description)) {
            printf(COLOR_YELLOW "第 %d 行数据不合法，已跳过。\n" COLOR_RESET, line_number);
            failed_count++;
            continue;
        }

        // [新增逻辑] 导入时的救护车强制插队
        if (detect_ambulance_signal(description)) {
            queue_number = 1;
            printf(COLOR_CYAN "导入预警: 第 %d 行检测到紧急信标，已自动调整排队号为 1。\n" COLOR_RESET, line_number);
        }

        add_new_record(patient_id, doctor_id, (RecordType)type, description, fee, queue_number);
        success_count++;
    }

    fclose(file);

    printf(COLOR_GREEN "导入完成：成功 %d 条，失败 %d 条。\n" COLOR_RESET, success_count, failed_count);
    wait_for_enter();
}

static void revoke_record_with_reason(RecordNode* record, const char* reason) {
    if (!record) return;

    record->status = -1;
    if (reason && reason[0] != '\0') {
        char new_desc[MAX_DESC_LEN] = {0};
        snprintf(new_desc, sizeof(new_desc), "[REVOKED:%s] ", reason);
        strncat(new_desc, record->description, sizeof(new_desc) - strlen(new_desc) - 1);
        strncpy(record->description, new_desc, MAX_DESC_LEN - 1);
        record->description[MAX_DESC_LEN - 1] = '\0';
    }
}

void modify_record_with_revoke() {
    char target_id[MAX_ID_LEN] = {0};
    RecordNode* target = NULL;

    char patient_id[MAX_ID_LEN] = {0};
    char doctor_id[MAX_ID_LEN] = {0};
    char description[MAX_DESC_LEN] = {0};
    int type = 0;
    int queue_number = 0;
    double fee = 0.0;

    print_header("修改诊疗记录(撤销后补录)");
    printf("请输入要修改的记录号: ");
    safe_get_string(target_id, sizeof(target_id));

    target = find_record_by_id(target_id);
    if (!target) {
        printf(COLOR_RED "未找到该记录。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (target->status == -1) {
        printf(COLOR_RED "该记录已经是撤销状态。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf(COLOR_CYAN "原记录如下：\n" COLOR_RESET);
    print_record_item(target);

    if (!input_record_payload(patient_id, doctor_id, &type, &fee, &queue_number, description)) {
        wait_for_enter();
        return;
    }

    revoke_record_with_reason(target, "modify");
    add_new_record(patient_id, doctor_id, (RecordType)type, description, fee, queue_number);

    printf(COLOR_GREEN "修改完成：原记录已撤销，新记录已补录。\n" COLOR_RESET);
    wait_for_enter();
}

void delete_record_with_revoke() {
    char target_id[MAX_ID_LEN] = {0};
    RecordNode* target = NULL;

    print_header("删除诊疗记录(撤销)");
    printf("请输入要删除的记录号: ");
    safe_get_string(target_id, sizeof(target_id));

    target = find_record_by_id(target_id);
    if (!target) {
        printf(COLOR_RED "未找到该记录。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (target->status == -1) {
        printf(COLOR_YELLOW "该记录已是撤销状态，无需重复删除。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    revoke_record_with_reason(target, "delete");
    printf(COLOR_GREEN "记录已撤销。\n" COLOR_RESET);
    wait_for_enter();
}

void query_records_by_department() {
    char dept_id[MAX_ID_LEN] = {0};
    RecordNode* current = record_head;
    int count = 0;

    print_header("按科室查询诊疗信息");
    printf("请输入科室ID: ");
    safe_get_string(dept_id, sizeof(dept_id));

    while (current) {
        DoctorNode* doctor = find_doctor_by_id(current->doctor_id);
        if (doctor && strcmp(doctor->dept_id, dept_id) == 0) {
            print_record_item(current);
            count++;
        }
        current = current->next;
    }

    if (count == 0) {
        printf(COLOR_YELLOW "该科室暂无记录。\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "共找到 %d 条记录。\n" COLOR_RESET, count);
    }
    wait_for_enter();
}

void query_records_by_doctor() {
    char doctor_id[MAX_ID_LEN] = {0};
    RecordNode* current = record_head;
    int count = 0;

    print_header("按医生工号查询诊疗信息");
    printf("请输入医生工号: ");
    safe_get_string(doctor_id, sizeof(doctor_id));

    while (current) {
        if (strcmp(current->doctor_id, doctor_id) == 0) {
            print_record_item(current);
            count++;
        }
        current = current->next;
    }

    if (count == 0) {
        printf(COLOR_YELLOW "该医生暂无记录。\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "共找到 %d 条记录。\n" COLOR_RESET, count);
    }
    wait_for_enter();
}

void query_records_by_patient_keyword() {
    char keyword[MAX_NAME_LEN] = {0};
    RecordNode* current = record_head;
    int count = 0;

    print_header("按患者信息查询历史诊疗");
    printf("请输入患者编号或姓名关键字: ");
    safe_get_string(keyword, sizeof(keyword));

    if (keyword[0] == '\0') {
        printf(COLOR_RED "关键字不能为空。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    while (current) {
        PatientNode* patient = find_patient_any(current->patient_id);
        int matched = 0;

        if (strstr(current->patient_id, keyword)) {
            matched = 1;
        } else if (patient && strstr(patient->name, keyword)) {
            matched = 1;
        }

        if (matched) {
            print_record_item(current);
            count++;
        }

        current = current->next;
    }

    if (count == 0) {
        printf(COLOR_YELLOW "未找到匹配记录。\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "共找到 %d 条记录。\n" COLOR_RESET, count);
    }
    wait_for_enter();
}

void query_records_in_time_range() {
    char start_date[32] = {0};
    char end_date[32] = {0};
    int start_key = 0;
    int end_key = 0;
    RecordNode* current = record_head;
    int count = 0;

    print_header("按时间范围查询诊疗信息");
    printf("请输入开始日期(YYYY-MM-DD): ");
    safe_get_string(start_date, sizeof(start_date));
    printf("请输入结束日期(YYYY-MM-DD): ");
    safe_get_string(end_date, sizeof(end_date));

    start_key = parse_date_key(start_date);
    end_key = parse_date_key(end_date);
    if (start_key < 0 || end_key < 0) {
        printf(COLOR_RED "日期格式不正确。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (start_key > end_key) {
        int tmp = start_key;
        start_key = end_key;
        end_key = tmp;
    }

    while (current) {
        int record_key = parse_date_key(current->date);
        if (record_key >= start_key && record_key <= end_key) {
            print_record_item(current);
            count++;
        }
        current = current->next;
    }

    if (count == 0) {
        printf(COLOR_YELLOW "该时间范围内无记录。\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "共找到 %d 条记录。\n" COLOR_RESET, count);
    }
    wait_for_enter();
}

void manage_duty_schedule() {
    int choice = -1;

    while (choice != 0) {
        print_header("值班排班管理");
        printf("  [1] 查看全部排班\n");
        printf("  [2] 按医生查看排班\n");
        printf("  [3] 新增/更新排班（门诊/急诊）\n");
        printf("  [4] 删除排班（门诊/急诊）\n");
        printf(COLOR_RED "  [0] 返回管理菜单\n" COLOR_RESET);
        print_divider();

        printf(COLOR_YELLOW "请选择操作(0-4): " COLOR_RESET);
        choice = safe_get_int();

        switch (choice) {
            case 1:
                view_duty_schedule(NULL);
                break;
            case 2: {
                char doctor_id[MAX_ID_LEN] = {0};
                printf("请输入医生工号: ");
                safe_get_string(doctor_id, sizeof(doctor_id));
                if (!find_doctor_by_id(doctor_id)) {
                    printf(COLOR_RED "医生工号不存在。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }
                view_duty_schedule(doctor_id);
                break;
            }
            case 3: {
                char date[32] = {0};
                char slot[32] = {0};
                char dept_id[MAX_ID_LEN] = {0};
                char doctor_id[MAX_ID_LEN] = {0};
                int scene_choice = 0;
                DoctorNode* doctor = NULL;
                DepartmentNode* dept = NULL;

                printf("请输入日期(YYYY-MM-DD): ");
                safe_get_string(date, sizeof(date));
                printf("请选择诊别(1-门诊，2-急诊): ");
                scene_choice = safe_get_int();
                if (scene_choice == 1) {
                    strcpy(slot, "门诊");
                } else if (scene_choice == 2) {
                    strcpy(slot, "急诊");
                } else {
                    printf(COLOR_RED "诊别输入无效，仅支持门诊/急诊。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }

                printf("请输入科室编号: ");
                safe_get_string(dept_id, sizeof(dept_id));
                printf("请输入医生工号: ");
                safe_get_string(doctor_id, sizeof(doctor_id));

                if (date[0] == '\0' || dept_id[0] == '\0' || doctor_id[0] == '\0') {
                    printf(COLOR_RED "输入不能为空。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }

                dept = find_department_by_id(dept_id);
                if (!dept) {
                    printf(COLOR_RED "科室编号不存在。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }

                doctor = find_doctor_by_id(doctor_id);
                if (!doctor) {
                    printf(COLOR_RED "医生工号不存在。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }

                if (strcmp(doctor->dept_id, dept_id) != 0) {
                    printf(COLOR_RED "医生不属于该科室，不能排班。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }

                if (!upsert_duty_schedule(date, slot, dept_id, doctor_id)) {
                    printf(COLOR_RED "排班保存失败，请检查日期格式。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }

                printf(COLOR_GREEN "排班保存成功。\n" COLOR_RESET);
                wait_for_enter();
                break;
            }
            case 4: {
                char date[32] = {0};
                char slot[32] = {0};
                char dept_id[MAX_ID_LEN] = {0};
                char doctor_id[MAX_ID_LEN] = {0};
                int scene_choice = 0;

                printf("请输入要删除的日期(YYYY-MM-DD): ");
                safe_get_string(date, sizeof(date));
                printf("请选择要删除的诊别(1-门诊，2-急诊): ");
                scene_choice = safe_get_int();
                if (scene_choice == 1) {
                    strcpy(slot, "门诊");
                } else if (scene_choice == 2) {
                    strcpy(slot, "急诊");
                } else {
                    printf(COLOR_RED "诊别输入无效，仅支持门诊/急诊。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }

                printf("请输入要删除的科室编号: ");
                safe_get_string(dept_id, sizeof(dept_id));
                printf("请输入要删除的医生编号: ");
                safe_get_string(doctor_id, sizeof(doctor_id));

                if (date[0] == '\0' || dept_id[0] == '\0' || doctor_id[0] == '\0') {
                    printf(COLOR_RED "输入不能为空。\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }

                if (remove_duty_schedule(date, slot, dept_id, doctor_id)) {
                    printf(COLOR_GREEN "排班删除成功。\n" COLOR_RESET);
                } else {
                    printf(COLOR_YELLOW "未找到匹配的排班记录。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 0:
                break;
            default:
                printf(COLOR_RED "无效的选择。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}

void register_doctor() {
    DoctorNode* new_node;
    DepartmentNode* dept;
    char id[MAX_ID_LEN];
    char name[MAX_NAME_LEN];
    char dept_id[MAX_ID_LEN];

    print_header("注册新医生账号");

    printf("请输入医生工号(ID): ");
    safe_get_string(id, MAX_ID_LEN);
    if (id[0] == '\0') {
        printf(COLOR_RED "错误：医生工号不能为空。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    if (find_doctor_by_id(id)) {
        printf(COLOR_RED "错误：该工号已存在。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("请输入医生姓名: ");
    safe_get_string(name, MAX_NAME_LEN);
    if (name[0] == '\0') {
        printf(COLOR_RED "错误：医生姓名不能为空。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("请输入所属科室ID: ");
    safe_get_string(dept_id, MAX_ID_LEN);
    if (dept_id[0] == '\0') {
        printf(COLOR_RED "错误：科室ID不能为空，医生必须绑定科室。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    dept = find_department_by_id(dept_id);
    if (!dept) {
        printf(COLOR_RED "错误：科室ID不存在，医生必须绑定有效科室。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // [新增逻辑] 医生注册必须定岗定级
    int title_level = 0;
    printf(COLOR_YELLOW "请分配医生职称以确定手术权限(4-主任, 3-副主任, 2-主治, 1-住院): " COLOR_RESET);
    title_level = safe_get_int();
    if (title_level < 1 || title_level > 4) {
        printf(COLOR_YELLOW "输入有误，默认定级为住院医师(Lv 1)。\n" COLOR_RESET);
        title_level = 1;
    }

    new_node = (DoctorNode*)calloc(1, sizeof(DoctorNode));
    if (!new_node) {
        printf(COLOR_RED "内存不足。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    strcpy(new_node->id, id);
    strcpy(new_node->name, name);
    strcpy(new_node->dept_id, dept_id);
    new_node->privilege_level = title_level; // [新增逻辑] 写入权限属性

    new_node->next = doctor_head;
    doctor_head = new_node;

    printf(COLOR_GREEN "医生 %s 注册成功，已授予权限：%s。\n" COLOR_RESET, 
           new_node->name, get_title_name(title_level));
    wait_for_enter();
}

void delete_doctor() {
    char id[MAX_ID_LEN];
    DoctorNode *current = doctor_head, *previous = NULL;

    print_header("注销医生账号");
    printf("请输入要注销的医生工号: ");
    safe_get_string(id, sizeof(id));

    while (current) {
        if (strcmp(current->id, id) == 0) {
            if (previous == NULL) {
                doctor_head = current->next;
            } else {
                previous->next = current->next;
            }

            printf(COLOR_GREEN "医生 %s(ID:%s) 已移除。\n" COLOR_RESET, current->name, id);
            free(current);
            wait_for_enter();
            return;
        }
        previous = current;
        current = current->next;
    }

    printf(COLOR_RED "未找到该工号。\n" COLOR_RESET);
    wait_for_enter();
}

void add_new_medicine() {
    char id[MAX_ID_LEN] = {0};
    char common_name[MAX_MED_NAME_LEN] = {0};
    char trade_name[MAX_MED_NAME_LEN] = {0};
    char alias_name[MAX_MED_NAME_LEN] = {0};
    double price;
    int stock;
    MedicineNode* node;

    print_header("新增药品档案");

    printf("请输入药品ID: ");
    safe_get_string(id, sizeof(id));
    if (id[0] == '\0') {
        printf(COLOR_RED "错误：药品ID不能为空。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    if (find_medicine_by_id(id)) {
        printf(COLOR_RED "错误：药品ID已存在。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("请输入通用名: ");
    safe_get_string(common_name, sizeof(common_name));
    if (common_name[0] == '\0') {
        printf(COLOR_RED "错误：通用名不能为空。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("请输入商品名(可留空): ");
    safe_get_string(trade_name, sizeof(trade_name));
    if (trade_name[0] == '\0') {
        strncpy(trade_name, common_name, sizeof(trade_name) - 1);
    }

    printf("请输入别名/俗名(可留空): ");
    safe_get_string(alias_name, sizeof(alias_name));
    if (alias_name[0] == '\0') {
        strncpy(alias_name, trade_name, sizeof(alias_name) - 1);
    }

    printf("请输入单价: ");
    price = safe_get_double();
    if (price < 0) {
        printf(COLOR_RED "错误：单价不能小于0。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("请输入初始库存: ");
    stock = safe_get_int();
    if (stock < 0) {
        printf(COLOR_RED "错误：库存不能小于0。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    node = (MedicineNode*)calloc(1, sizeof(MedicineNode));
    if (!node) {
        printf(COLOR_RED "内存不足，新增失败。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    strncpy(node->id, id, sizeof(node->id) - 1);
    strncpy(node->common_name, common_name, sizeof(node->common_name) - 1);
    strncpy(node->trade_name, trade_name, sizeof(node->trade_name) - 1);
    strncpy(node->alias_name, alias_name, sizeof(node->alias_name) - 1);
    node->price = price;
    node->stock = stock;

    node->next = medicine_head;
    medicine_head = node;

    printf(COLOR_GREEN "新增成功：%s | %s | %s | 库存 %d\n" COLOR_RESET,
           node->id, node->common_name, node->trade_name, node->stock);
    wait_for_enter();
}

void medicine_stock_in() {
    char id[MAX_ID_LEN];
    MedicineNode* medicine;

    print_header("药品进库登记");
    printf("请输入药品ID: ");
    safe_get_string(id, sizeof(id));

    medicine = find_medicine_by_id(id);
    if (!medicine) {
        printf(COLOR_RED "未找到该药品ID。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("药品: %s | 当前库存: %d\n", medicine->common_name, medicine->stock);
    printf("请输入进库数量: ");

    {
        int quantity = safe_get_int();
        if (quantity <= 0) {
            printf(COLOR_RED "错误：数量必须大于0。\n" COLOR_RESET);
            wait_for_enter();
            return;
        }

        medicine->stock += quantity;
        printf(COLOR_GREEN "进库成功，新库存: %d\n" COLOR_RESET, medicine->stock);
    }

    wait_for_enter();
}

void view_inventory_report() {
    MedicineNode* current = medicine_head;
    const char* title = "\xE5\x85\xA8\xE5\xB1\x80\xE8\x8D\xAF\xE5\x93\x81\xE5\xBA\x93\xE5\xAD\x98\xE5\xAE\xA1\xE8\xAE\xA1";
    const char* col_id = "\xE8\x8D\xAF\xE5\x93\x81\x49\x44";
    const char* col_common = "\xE9\x80\x9A\xE7\x94\xA8\xE5\x90\x8D";
    const char* col_trade = "\xE5\x95\x86\xE5\x93\x81\xE5\x90\x8D";
    const char* col_alias = "\xE5\x88\xAB\xE5\x90\x8D";
    const char* col_stock = "\xE5\xBA\x93\xE5\xAD\x98";
    const char* col_status = "\xE7\x8A\xB6\xE6\x80\x81";
    const char* warn = "\x5B\xE5\xBA\x93\xE5\xAD\x98\xE5\x91\x8A\xE8\xAD\xA6\x5D";
    const char* ok = "\x5B\xE6\xAD\xA3\xE5\xB8\xB8\x5D";

    print_header(title);
    printf("%-10s %-16s %-16s %-14s %-8s %-10s\n", col_id, col_common, col_trade, col_alias, col_stock, col_status);
    print_divider();

    while (current) {
        const char* trade = current->trade_name[0] ? current->trade_name : current->common_name;
        const char* alias = current->alias_name[0] ? current->alias_name : trade;

        printf("%-10s %-16s %-16s %-14s %-8d ", current->id, current->common_name, trade, alias, current->stock);
        if (current->stock < 50) {
            printf(COLOR_RED "%s\n" COLOR_RESET, warn);
        } else {
            printf(COLOR_GREEN "%s\n" COLOR_RESET, ok);
        }
        current = current->next;
    }

    wait_for_enter();
}

void view_hospital_financial_report() {
    double total_revenue = 0.0;
    double insurance_total = 0.0;
    double personal_total = 0.0;
    RecordNode* current = record_head;

    print_header("医院财务汇总(已完成记录)");

    while (current) {
        if (current->status == 1) {
            total_revenue += current->total_fee;
            insurance_total += current->insurance_covered;
            personal_total += current->personal_paid;
        }
        current = current->next;
    }

    printf("总营收:      %10.2f 元\n", total_revenue);
    printf("医保统筹:    %10.2f 元\n", insurance_total);
    printf("个人支付:    %10.2f 元\n", personal_total);

    // [新增逻辑] 深度扫描 inpatients.txt 的 140 位住院患者财务风险
    print_divider();
    printf(COLOR_YELLOW ">>> 住院系统欠费风险深度审计 <<<\n" COLOR_RESET);
    PatientNode* curr_in = inpatient_head;
    int debt_count = 0;
    double risk_total = 0.0;
    
   while(curr_in) {
        if (curr_in->account_balance <= 0.01) {
            debt_count++;
            risk_total += 5000.0; // 风险预估模型
        }
        curr_in = curr_in->next;
    }
    printf("当前在院欠费高危人数: %d 人\n", debt_count);
    printf("当前系统预估坏账敞口: %.2f 元\n", risk_total);

    print_divider();
    wait_for_enter();
}

void analyze_historical_trends() {
    DepartmentNode* department = dept_head;

    print_header("科室就诊流量分析");
    while (department) {
        int count = 0;
        RecordNode* record = record_head;

        while (record) {
            DoctorNode* doctor = find_doctor_by_id(record->doctor_id);
            if (doctor && strcmp(doctor->dept_id, department->id) == 0) {
                count++;
            }
            record = record->next;
        }

        printf("%-15s: %d 人次\n", department->name, count);
        department = department->next;
    }

    wait_for_enter();
}

void optimize_bed_allocation() {
    DepartmentNode* department = dept_head;

    print_header("床位资源监控");
    while (department) {
        int total = 0;
        int occupied = 0;
        BedNode* bed = bed_head;

        while (bed) {
            if (strcmp(bed->dept_id, department->id) == 0) {
                total++;
                if (bed->is_occupied) {
                    occupied++;
                }
            }
            bed = bed->next;
        }

        {
            double rate = (total > 0) ? ((double)occupied / (double)total * 100.0) : 0.0;
            printf("科室: %-10s | 占用率: %5.1f%% (%d/%d)\n", department->name, rate, occupied, total);
        }

        department = department->next;
    }

    wait_for_enter();
}

void admin_subsystem() {
    char pwd[32];
    int choice = -1;

    print_header("系统管理与决策支持中心");

    printf("请输入管理员授权密码: ");
    safe_get_string(pwd, sizeof(pwd));

    if (strcmp(pwd, "admin123") != 0) {
        printf(COLOR_RED "密码错误，认证失败。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    while (choice != 0) {
        printf("\n=========================================\n");
        printf(COLOR_GREEN "  管理员已登录 (高级急救调度已挂载)\n" COLOR_RESET);
        printf("=========================================\n");
        printf("  --- 人事与资源 ---\n");
        printf("  [ 1 ] 注册新医生账号\n");
        printf("  [ 2 ] 注销/删除医生账号\n");
        printf("  [ 3 ] 床位利用率查询\n");
        printf("  --- 药房物流 ---\n");
        printf("  [ 4 ] 新增药品档案\n");
        printf("  [ 5 ] 药品进库登记\n");
        printf("  [ 6 ] 库存审计报告\n");
        printf("  --- 财务与分析 ---\n");
        printf("  [ 7 ] 全局财务营收报告\n");
        printf("  [ 8 ] 科室就诊流量分析\n");
        printf("  --- 诊疗记录 CRUD ---\n");
        printf("  [ 9 ] 新增单条诊疗记录 (支持救护车直通)\n");
        printf("  [10 ] 文件批量导入诊疗记录\n");
        printf("  [11 ] 修改诊疗记录(撤销后补录)\n");
        printf("  [12 ] 删除诊疗记录(撤销)\n");
        printf("  --- 题签查询能力 ---\n");
        printf("  [13 ] 按科室查询诊疗信息\n");
        printf("  [14 ] 按医生工号查询诊疗信息\n");
        printf("  [15 ] 按患者信息查询历史诊疗\n");
        printf("  [16 ] 按时间范围查询诊疗信息\n");
        printf("  [17 ] 值班排班管理\n");
        printf(COLOR_RED "  [ 0 ] 返回主菜单\n" COLOR_RESET);
        print_divider();

        printf(COLOR_YELLOW "请选择管理项目 (0-17): " COLOR_RESET);
        choice = safe_get_int();

        switch (choice) {
            case 1: register_doctor(); break;
            case 2: delete_doctor(); break;
            case 3: optimize_bed_allocation(); break;
            case 4: add_new_medicine(); break;
            case 5: medicine_stock_in(); break;
            case 6: view_inventory_report(); break;
            case 7: view_hospital_financial_report(); break;
            case 8: analyze_historical_trends(); break;
            case 9: add_record_interactive(); break;
            case 10: import_records_from_file(); break;
            case 11: modify_record_with_revoke(); break;
            case 12: delete_record_with_revoke(); break;
            case 13: query_records_by_department(); break;
            case 14: query_records_by_doctor(); break;
            case 15: query_records_by_patient_keyword(); break;
            case 16: query_records_in_time_range(); break;
            case 17: manage_duty_schedule(); break;
            case 0: break;
            default:
                printf(COLOR_RED "无效的选择。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}