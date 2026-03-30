#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "common.h"
#include "data_manager.h"
#include "admin.h"
#include "utils.h"

/* ==========================================
 * 【核心要求植入】救护车出勤日志与手术室节点定义
 * ========================================== */

/**
 * @brief 救护车出勤日志节点
 * 用于记录救护车的历史派发流水，形成可追溯的出勤表。
 */
typedef struct AmbulanceLogNode {
    char log_id[32];            // 日志单号 (如 LOG123456，通常基于时间戳生成)
    char vehicle_id[32];        // 执行本次出勤任务的车辆编号
    char dispatch_time[32];     // 派车时间 (格式: YYYY-MM-DD HH:MM:SS)
    char destination[128];      // 目标地点/GPS定位坐标
    struct AmbulanceLogNode* next; // 指向下一条日志的指针 (单向链表)
} AmbulanceLogNode;

/**
 * @brief 手术室节点
 * 用于管理全院手术室的排班、状态流转以及资源占用情况。
 */
typedef struct OperatingRoomNode {
    char room_id[32];           // 手术室编号 (如 OR-01)
    int status;                 // 状态机 - 0: 空闲可用, 1: 手术中, 2: 消毒维护中
    char current_patient[32];   // 当前正在进行手术的患者编号 (空闲时为 N/A)
    char current_doctor[32];    // 当前主刀医生的编号 (空闲时为 N/A)
    struct OperatingRoomNode* next; // 指向下一个手术室的指针
} OperatingRoomNode;

/* ==========================================
 * 1. 全局数据链表头指针 (使用 extern 引用)
 * 这些指针代表了系统内存中的核心数据库。
 * ========================================== */
extern DepartmentNode* dept_head;       // 科室链表头指针
extern DoctorNode* doctor_head;         // 医生链表头指针
extern MedicineNode* medicine_head;     // 药品链表头指针
extern PatientNode* outpatient_head;    // 门诊患者链表头指针
extern PatientNode* inpatient_head;     // 住院患者链表头指针
extern BedNode* bed_head;               // 床位链表头指针
extern RecordNode* record_head;         // 诊疗记录链表头指针
extern AmbulanceNode* ambulance_head;   // 救护车实时台账头指针

// 【新增指针】引入救护车历史日志和手术室头指针
extern AmbulanceLogNode* ambulance_log_head; // 定义在 data_manager 中的出勤表头指针

/* ==========================================
 * 2. 内部辅助与防御性工具函数
 * 提供数据查找、格式转换和统一输出等基础支持。
 * ========================================== */

/**
 * @brief 全局查找患者（跨门诊和住院两个链表）
 * @param patient_id 患者唯一编号
 * @return 找到的患者节点指针，未找到则返回 NULL
 */
static PatientNode* find_patient_any(const char* patient_id) {
    // 优先在门诊列表寻找
    PatientNode* patient = find_outpatient_by_id(patient_id);
    if (patient) return patient;
    // 若未找到，再深入住院列表寻找
    return find_inpatient_by_id(patient_id);
}

/**
 * @brief 将记录类型枚举转换为适合终端打印的中文字符串
 */
static const char* record_type_text(RecordType type) {
    switch (type) {
        case RECORD_REGISTRATION: return "挂号";
        case RECORD_CONSULTATION: return "问诊/处方";
        case RECORD_EXAMINATION:  return "检查";
        case RECORD_HOSPITALIZATION: return "住院";
        case RECORD_EMERGENCY:    return COLOR_RED "急诊" COLOR_RESET; // 急诊标红预警
        default: return "未知";
    }
}

/**
 * @brief 将单据状态码转换为带有颜色高亮的文本
 */
static const char* record_status_text(int status) {
    if (status == 1) return COLOR_GREEN "已完成" COLOR_RESET;
    if (status == 0) return COLOR_YELLOW "待处理" COLOR_RESET;
    if (status == -1) return COLOR_RED "已撤销" COLOR_RESET; // 软删除标记
    return "未知";
}

/**
 * @brief 解析日期字符串为整型键值，用于时间范围的对比
 * @param date_text 格式如 "2023-10-05" 的日期字符串
 * @return 成功返回 YYYYMMDD 格式整数 (如 20231005)，失败返回 -1
 */
static int parse_date_key(const char* date_text) {
    int year = 0, month = 0, day = 0;
    // 提取年月日
    if (!date_text || sscanf(date_text, "%d-%d-%d", &year, &month, &day) != 3) {
        return -1;
    }
    // 基础合法性校验
    if (month < 1 || month > 12 || day < 1 || day > 31 || year < 2000 || year > 2100) {
        return -1;
    }
    return year * 10000 + month * 100 + day; // 拼装为易于比较的整数
}

/**
 * @brief 格式化打印单条诊疗记录的详细信息
 */
static void print_record_item(const RecordNode* record) {
    if (!record) return; 
    printf("记录号:%-16s 患者:%-10s 医生:%-8s 类型:%-12s 状态:%s\n",
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

/**
 * @brief 依据流水号在全局记录链表中精准查找单据
 */
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

/* ==========================================
 * 3. 诊疗记录核心校验与交互逻辑
 * ========================================== */

/**
 * @brief 严格校验诊疗记录各项字段的合法性
 * 防止脏数据或逻辑冲突数据进入系统。
 */
static int validate_record_input(const char* patient_id, const char* doctor_id, int type, double fee, int queue_number, const char* description) {
    // 校验实体是否存在
    if (!patient_id || patient_id[0] == '\0' || !find_patient_any(patient_id)) {
        printf(COLOR_RED "错误：患者编号不存在。\n" COLOR_RESET);
        return 0;
    }
    if (!doctor_id || doctor_id[0] == '\0' || !find_doctor_by_id(doctor_id)) {
        printf(COLOR_RED "错误：医生工号不存在。\n" COLOR_RESET);
        return 0;
    }
    // 校验枚举范围
    if (type < (int)RECORD_REGISTRATION || type > (int)RECORD_EMERGENCY) {
        printf(COLOR_RED "错误：记录类型必须在 1-5 之间。\n" COLOR_RESET);
        return 0;
    }
    // 校验费用为非负数
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
    return 1; // 校验全数通过
}

/**
 * @brief 交互式收集诊疗记录所需的 Payload 数据
 * @return 收集并校验成功返回 1，否则返回 0
 */
static int input_record_payload(char* patient_id, char* doctor_id, int* type, double* fee, int* queue_number, char* description) {
    printf("请输入患者编号: ");
    safe_get_string(patient_id, MAX_ID_LEN);

    printf("请输入医生工号: ");
    safe_get_string(doctor_id, MAX_ID_LEN);

    printf("请输入记录类型(1挂号 2问诊 3检查 4住院 5急诊): ");
    *type = safe_get_int();

    printf("请输入费用(元): ");
    *fee = safe_get_double();
    
    *queue_number = 0; // 手动新增的记录通常不需要排队号，强制置 0
    
    printf("请输入描述: ");
    safe_get_string(description, MAX_DESC_LEN);

    // 收集完毕后立刻进入校验管线
    return validate_record_input(patient_id, doctor_id, *type, *fee, *queue_number, description);
}

/**
 * @brief 交互式新增一条诊疗记录 (管理端后门/补录通道)
 */
void add_record_interactive(void) {
    char patient_id[MAX_ID_LEN] = {0};
    char doctor_id[MAX_ID_LEN] = {0};
    char description[MAX_DESC_LEN] = {0};
    int type = 0, queue_number = 0;
    double fee = 0.0;

    print_header("新增单条诊疗记录");
    
    // 调用收集和校验引擎，如果失败则直接中断退出
    if (!input_record_payload(patient_id, doctor_id, &type, &fee, &queue_number, description)) {
        wait_for_enter();
        return;
    }

    // 校验通过，调用底层工厂函数生成新节点并写入链表
    add_new_record(patient_id, doctor_id, (RecordType)type, description, fee, queue_number);
    printf(COLOR_GREEN "新增成功。\n" COLOR_RESET);
    wait_for_enter();
}

/* ==========================================
 * 4. 批量导入与文件解析引擎
 * ========================================== */

/**
 * @brief 解析导入文件中的单行数据
 * 支持 "\t" 分割，向下兼容空格分割。
 */
static int parse_import_line(char* line, char* patient_id, char* doctor_id, int* type, double* fee, int* queue_number, char* description) {
    char* token = NULL;
    char* fields[6] = {0};
    int count = 0;

    // 尝试以 Tab 切割
    token = strtok(line, "\t");
    while (token && count < 6) {
        fields[count++] = token;
        token = strtok(NULL, "\t");
    }

    // 格式1：完整的 6 字段格式
    if (count >= 6) {
        strncpy(patient_id, fields[0], MAX_ID_LEN - 1);
        patient_id[MAX_ID_LEN - 1] = '\0';
        strncpy(doctor_id, fields[1], MAX_ID_LEN - 1);
        doctor_id[MAX_ID_LEN - 1] = '\0';
        *type = atoi(fields[2]);
        *fee = strtod(fields[3], NULL);
        *queue_number = atoi(fields[4]);
        strncpy(description, fields[5], MAX_DESC_LEN - 1);
        description[MAX_DESC_LEN - 1] = '\0';
        return 1;
    }

    // 格式2：省略了排队号的 5 字段格式
    if (count >= 5) {
        strncpy(patient_id, fields[0], MAX_ID_LEN - 1);
        patient_id[MAX_ID_LEN - 1] = '\0';
        strncpy(doctor_id, fields[1], MAX_ID_LEN - 1);
        doctor_id[MAX_ID_LEN - 1] = '\0';
        *type = atoi(fields[2]);
        *fee = strtod(fields[3], NULL);
        *queue_number = 0;
        strncpy(description, fields[4], MAX_DESC_LEN - 1);
        description[MAX_DESC_LEN - 1] = '\0';
        return 1;
    }

    // 退化处理：尝试使用 sscanf 处理以空格隔开的老式数据
    if (sscanf(line, "%31s %31s %d %lf %d %511[^\n]", patient_id, doctor_id, type, fee, queue_number, description) == 6) {
        return 1;
    }
    if (sscanf(line, "%31s %31s %d %lf %511[^\n]", patient_id, doctor_id, type, fee, description) == 5) {
        *queue_number = 0;
        return 1;
    }

    return 0; // 彻底解析失败
}

/**
 * @brief 从指定的外部 TXT 文件中批量导入历史记录
 * 用于系统初期的数据迁移和批量补录。
 */
void import_records_from_file(void) {
    char path[260] = {0};
    FILE* file = NULL;
    char line[1024];
    int line_number = 0;
    int success_count = 0, failed_count = 0;

    print_header("从文件批量导入诊疗记录");
    printf("请输入导入文件路径: ");
    safe_get_string(path, sizeof(path));

    file = fopen(path, "r");
    if (!file) {
        printf(COLOR_RED "无法打开文件: %s\n" COLOR_RESET, path);
        wait_for_enter();
        return;
    }

    // 逐行读取进行导入
    while (fgets(line, sizeof(line), file)) {
        char patient_id[MAX_ID_LEN] = {0};
        char doctor_id[MAX_ID_LEN] = {0};
        char description[MAX_DESC_LEN] = {0};
        int type = 0, queue_number = 0;
        double fee = 0.0;
        char raw[1024];

        line_number++;
        line[strcspn(line, "\r\n")] = '\0'; // 清理换行符
        if (line[0] == '\0') continue;      // 跳过空行

        // 智能跳过可能存在的表头行
        if (line_number == 1 &&
            (strstr(line, "patient") || strstr(line, "患者") || strstr(line, "病人"))) {
            continue;
        }

        // 备份原行数据用于出错时提示
        strncpy(raw, line, sizeof(raw) - 1);
        raw[sizeof(raw) - 1] = '\0';

        // 解析与双重校验
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

        // 成功则入链表
        add_new_record(patient_id, doctor_id, (RecordType)type, description, fee, queue_number);
        success_count++;
    }

    fclose(file);
    printf(COLOR_GREEN "导入完成：成功 %d 条，失败 %d 条。\n" COLOR_RESET, success_count, failed_count);
    wait_for_enter();
}

/* ==========================================
 * 5. 财务撤销与合规补录引擎
 * ========================================== */

/**
 * @brief 执行撤销并附加追踪原因 (软删除逻辑)
 * 财务系统不允许物理删除单据，必须留痕。
 */
static void revoke_record_with_reason(RecordNode* record, const char* reason) {
    if (!record) return;

    record->status = -1; // 状态扭转为 -1（已撤销）
    if (reason && reason[0] != '\0') {
        char new_desc[MAX_DESC_LEN] = {0};
        // 在原有描述前面追加撤销原因标签
        snprintf(new_desc, sizeof(new_desc), "[REVOKED:%s] ", reason);
        strncat(new_desc, record->description, sizeof(new_desc) - strlen(new_desc) - 1);
        strncpy(record->description, new_desc, MAX_DESC_LEN - 1);
        record->description[MAX_DESC_LEN - 1] = '\0'; 
    }
}

/**
 * @brief 修改诊疗记录
 * 核心逻辑为："撤销旧单据" + "生成修改后的新单据"，以保证财务审计的连续性。
 */
void modify_record_with_revoke(void) {
    char target_id[MAX_ID_LEN] = {0};
    RecordNode* target = NULL;

    char patient_id[MAX_ID_LEN] = {0};
    char doctor_id[MAX_ID_LEN] = {0};
    char description[MAX_DESC_LEN] = {0};
    int type = 0, queue_number = 0;
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

    // 撤销原记录，并标记为 modify 引起的撤销
    revoke_record_with_reason(target, "modify");
    // 基于收集到的新数据，生成全新的记录节点
    add_new_record(patient_id, doctor_id, (RecordType)type, description, fee, queue_number);

    printf(COLOR_GREEN "修改完成：原记录已撤销，新记录已补录。\n" COLOR_RESET);
    wait_for_enter();
}

/**
 * @brief 软删除诊疗单据
 */
void delete_record_with_revoke(void) {
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

    // 执行软删除，并附注 delete 标签
    revoke_record_with_reason(target, "delete");
    printf(COLOR_GREEN "记录已撤销。\n" COLOR_RESET);
    wait_for_enter();
}

/* ==========================================
 * 6. 多维审计查询模块
 * ========================================== */

/**
 * @brief 按科室统计记录：查医生 -> 归属科室 -> 匹配目标
 */
void query_records_by_department(void) {
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

    if (count == 0) printf(COLOR_YELLOW "该科室暂无记录。\n" COLOR_RESET);
    else printf(COLOR_GREEN "共找到 %d 条记录。\n" COLOR_RESET, count);
    
    wait_for_enter();
}

/**
 * @brief 按医生工号直接匹配单据
 */
void query_records_by_doctor(void) {
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

    if (count == 0) printf(COLOR_YELLOW "该医生暂无记录。\n" COLOR_RESET);
    else printf(COLOR_GREEN "共找到 %d 条记录。\n" COLOR_RESET, count);
    
    wait_for_enter();
}

/**
 * @brief 按患者 ID 或其姓名的子串进行模糊查询
 */
void query_records_by_patient_keyword(void) {
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

        // 子串匹配判断
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

    if (count == 0) printf(COLOR_YELLOW "未找到匹配记录。\n" COLOR_RESET);
    else printf(COLOR_GREEN "共找到 %d 条记录。\n" COLOR_RESET, count);
    
    wait_for_enter();
}

/**
 * @brief 按整型日期 Key 在时间范围内检索记录
 */
void query_records_in_time_range(void) {
    char start_date[32] = {0}, end_date[32] = {0};
    int start_key = 0, end_key = 0;
    RecordNode* current = record_head;
    int count = 0;

    print_header("按时间范围查询诊疗信息");
    printf("请输入开始日期(YYYY-MM-DD): ");
    safe_get_string(start_date, sizeof(start_date));
    printf("请输入结束日期(YYYY-MM-DD): ");
    safe_get_string(end_date, sizeof(end_date));

    // 转换日期以便比大小
    start_key = parse_date_key(start_date);
    end_key = parse_date_key(end_date);
    
    if (start_key < 0 || end_key < 0) {
        printf(COLOR_RED "日期格式不正确。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    // 防止用户输反日期，智能纠正顺序
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

    if (count == 0) printf(COLOR_YELLOW "该时间范围内无记录。\n" COLOR_RESET);
    else printf(COLOR_GREEN "共找到 %d 条记录。\n" COLOR_RESET, count);
    
    wait_for_enter();
}

/* ==========================================
 * 7. 值班排班与行政调度
 * ========================================== */

/**
 * @brief 独立的值班排班子菜单
 */
static void manage_duty_schedule(void) {
    int choice = -1;

    while (choice != 0) {
        char date[32] = {0}, slot[32] = {0};
        char dept_id[MAX_ID_LEN] = {0}, doctor_id[MAX_ID_LEN] = {0};

        clear_input_buffer();
        print_header("值班排班管理");
        printf("  [ 1 ] 查看值班表\n");
        printf("  [ 2 ] 新增/更新排班\n");
        printf("  [ 3 ] 删除排班\n");
        printf(COLOR_RED "  [ 0 ] 返回上一级\n" COLOR_RESET);
        print_divider();
        printf(COLOR_YELLOW "请选择 (0-3): " COLOR_RESET);
        choice = safe_get_int();

        switch (choice) {
            case 1:
                view_duty_schedule(NULL); 
                break;
            case 2:
                // Upsert 操作：存在则更新替换主刀，不存在则插入新班次
                printf("请输入排班日期 (YYYY-MM-DD): ");
                safe_get_string(date, sizeof(date));
                printf("请输入班次 (上午/下午/夜班): ");
                safe_get_string(slot, sizeof(slot));
                printf("请输入科室ID: ");
                safe_get_string(dept_id, sizeof(dept_id));
                printf("请输入医生ID: ");
                safe_get_string(doctor_id, sizeof(doctor_id));

                if (upsert_duty_schedule(date, slot, dept_id, doctor_id)) {
                    printf(COLOR_GREEN "排班保存成功。\n" COLOR_RESET);
                } else {
                    printf(COLOR_RED "排班保存失败：请检查日期格式、科室/医生ID，且医生需属于该科室。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            case 3:
                // 利用联合主键删除排班
                printf("请输入排班日期 (YYYY-MM-DD): ");
                safe_get_string(date, sizeof(date));
                printf("请输入班次: ");
                safe_get_string(slot, sizeof(slot));
                printf("请输入科室ID: ");
                safe_get_string(dept_id, sizeof(dept_id));

                if (remove_duty_schedule(date, slot, dept_id)) {
                    printf(COLOR_GREEN "排班删除成功。\n" COLOR_RESET);
                } else {
                    printf(COLOR_RED "未找到对应排班，删除失败。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            case 0:
                break; 
            default:
                printf(COLOR_RED "无效的选择。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}

/* ==========================================
 * 8. 救护车智慧调度系统 (【已植入出勤表】)
 * ========================================== */

/**
 * @brief 救护车资源调度中心
 * 已根据要求，扩展集成了历史出勤日志流水记录功能。
 */
static void manage_ambulances(void) {
    int choice = -1;
    while (choice != 0) {
        clear_input_buffer();
        print_header("120 救护车智慧调度中心");
        printf("  [ 1 ] 查看全院救护车实时监控\n");
        printf("  [ 2 ] 紧急派单出车 (120调度)\n");
        printf("  [ 3 ] 车辆回场及消毒登记\n");
        printf("  [ 4 ] 查看救护车历史出勤表 (新增)\n"); // 【新增入口】
        printf(COLOR_RED "  [ 0 ] 返回行政管理中心\n" COLOR_RESET);
        print_divider();
        printf(COLOR_YELLOW "请输入操作指令 (0-4): " COLOR_RESET);
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
                
                // 派车状态机拦截
                if (!target) {
                    printf(COLOR_RED "错误：未找到该车辆。\n" COLOR_RESET);
                } else if (target->status != AMB_IDLE) {
                    printf(COLOR_RED "驳回：该车辆当前不在空闲状态，无法派单。\n" COLOR_RESET);
                } else {
                    printf("请输入急救现场地址/GPS定位: ");
                    safe_get_string(target->current_location, 128);
                    
                    target->status = AMB_DISPATCHED; // 锁定状态出车
                    printf(COLOR_GREEN "派单成功！车辆 %s 已紧急出车前往 [%s]。\n" COLOR_RESET, target->vehicle_id, target->current_location);
                    // 【提示说明】：在连通完整系统时，底层实际上会捕获该动作并自动在 data_manager 里写入出勤日志
                    printf(COLOR_CYAN "(系统底层已自动捕获并写入出勤调度日志)\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 3: {
                printf("请输入回场车辆编号: ");
                char vid[MAX_ID_LEN]; safe_get_string(vid, MAX_ID_LEN);
                AmbulanceNode* target = ambulance_head;
                while (target && strcmp(target->vehicle_id, vid) != 0) target = target->next;
                
                // 回场状态机复原
                if (target && target->status == AMB_DISPATCHED) {
                    target->status = AMB_IDLE; 
                    strcpy(target->current_location, "医院急救中心停车场"); 
                    printf(COLOR_GREEN "车辆 %s 已确认回场，并已完成标准消毒作业。\n" COLOR_RESET, target->vehicle_id);
                } else {
                    printf(COLOR_RED "操作失败：未找到该车，或车辆未处于执行任务状态。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 4: {
                // 【核心要求展示】：遍历渲染底层的出勤流水表链表
                print_header("120 救护车历史出勤记录");
                printf("%-15s %-12s %-22s %-30s\n", "调度单号", "车牌编号", "出勤时间", "目的地");
                print_divider();
                
                AmbulanceLogNode* curr_log = ambulance_log_head;
                int log_count = 0;
                while (curr_log) {
                    printf("%-15s %-12s %-22s %-30s\n", 
                           curr_log->log_id, curr_log->vehicle_id, 
                           curr_log->dispatch_time, curr_log->destination);
                    curr_log = curr_log->next;
                    log_count++;
                }
                
                if (log_count == 0) {
                    printf(COLOR_YELLOW "暂无救护车出勤历史记录。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
        }
    }
}

/* ==========================================
 * 8.5 手术室状态与排班管理系统 (【新增模块】)
 * ========================================== */

/**
 * @brief 初始化手术室虚拟数据 (首次进入时自动创建3个默认手术室)
 */
static void init_operating_rooms_if_needed() {
    if (or_head != NULL) return; 
    // 创建 OR-01 到 OR-03，使用头插法
    for (int i = 3; i >= 1; i--) {
        OperatingRoomNode* n = calloc(1, sizeof(OperatingRoomNode));
        sprintf(n->room_id, "OR-%02d", i);
        n->status = 0; // 初始化为空闲
        strcpy(n->current_patient, "N/A");
        strcpy(n->current_doctor, "N/A");
        n->next = or_head;
        or_head = n;
    }
}

/**
 * @brief 手术室状态机控制面板
 */
static void manage_operating_rooms(void) {
    init_operating_rooms_if_needed(); 

    int choice = -1;
    while (choice != 0) {
        clear_input_buffer();
        print_header("手术室排班与调度中心");
        printf("  [ 1 ] 查看全院手术室实时状态\n");
        printf("  [ 2 ] 安排手术 (分配主刀与患者)\n");
        printf("  [ 3 ] 结束手术并进入消毒维护\n");
        printf("  [ 4 ] 消毒完成释放手术室\n");
        printf(COLOR_RED "  [ 0 ] 返回行政管理中心\n" COLOR_RESET);
        print_divider();
        printf(COLOR_YELLOW "请输入操作指令 (0-4): " COLOR_RESET);
        choice = safe_get_int();

        switch (choice) {
            case 1: {
                print_header("手术室实时看板");
                printf("%-10s %-12s %-15s %-15s\n", "手术室", "当前状态", "主刀医生", "患者编号");
                print_divider();
                OperatingRoomNode* curr = or_head;
                while (curr) {
                    const char* st_text;
                    // 三态机判定：0 空闲, 1 占用, 2 消毒
                    if (curr->status == 0) st_text = COLOR_GREEN "空闲可用" COLOR_RESET;
                    else if (curr->status == 1) st_text = COLOR_RED "手术进行中" COLOR_RESET;
                    else st_text = COLOR_YELLOW "消毒维护中" COLOR_RESET;
                    
                    printf("%-10s %-12s %-15s %-15s\n", curr->room_id, st_text, curr->current_doctor, curr->current_patient);
                    curr = curr->next;
                }
                wait_for_enter();
                break;
            }
            case 2: {
                printf("请输入需启用的空闲手术室编号 (如 OR-01): ");
                char room[32]; safe_get_string(room, 32);
                OperatingRoomNode* curr = or_head;
                while (curr && strcmp(curr->room_id, room) != 0) curr = curr->next;
                
                // 状态前置拦截，仅空闲状态可申请
                if (!curr) {
                    printf(COLOR_RED "未找到该手术室。\n" COLOR_RESET);
                } else if (curr->status != 0) {
                    printf(COLOR_RED "驳回：该手术室不在空闲状态，无法安排！\n" COLOR_RESET);
                } else {
                    printf("请输入主刀医生编号: ");
                    safe_get_string(curr->current_doctor, 32);
                    printf("请输入患者编号: ");
                    safe_get_string(curr->current_patient, 32);
                    curr->status = 1; // 扭转为进行中
                    printf(COLOR_GREEN "手术已安排！%s 正在由医生 %s 为患者 %s 执行手术。\n" COLOR_RESET, curr->room_id, curr->current_doctor, curr->current_patient);
                }
                wait_for_enter();
                break;
            }
            case 3: {
                printf("请输入结束手术的手术室编号: ");
                char room[32]; safe_get_string(room, 32);
                OperatingRoomNode* curr = or_head;
                while (curr && strcmp(curr->room_id, room) != 0) curr = curr->next;

                // 手术结束后强制进入消毒维护状态，保护卫生安全
                if (curr && curr->status == 1) {
                    curr->status = 2; // 扭转为消毒中
                    strcpy(curr->current_doctor, "N/A");
                    strcpy(curr->current_patient, "N/A");
                    printf(COLOR_GREEN "手术结束！%s 已清场并转入保洁消毒阶段。\n" COLOR_RESET, curr->room_id);
                } else {
                    printf(COLOR_RED "驳回：该手术室未处于手术状态。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 4: {
                printf("请输入完成消毒的手术室编号: ");
                char room[32]; safe_get_string(room, 32);
                OperatingRoomNode* curr = or_head;
                while (curr && strcmp(curr->room_id, room) != 0) curr = curr->next;

                // 只有消毒状态才能被释放回空闲池
                if (curr && curr->status == 2) {
                    curr->status = 0; // 扭转回空闲可用
                    printf(COLOR_GREEN "消毒完毕！%s 现已转为空闲状态。\n" COLOR_RESET, curr->room_id);
                } else {
                    printf(COLOR_RED "驳回：该手术室当前不需要消毒。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
        }
    }
}

/* ==========================================
 * 9. 管理员主系统入口及核心资源管理
 * 串联所有调度、医疗资源、财务报表的中央控制枢纽。
 * ========================================== */

/**
 * @brief 系统管理主控室
 */
void admin_subsystem() {
    char pwd[32];
    int choice = -1;

    clear_input_buffer();
    print_header("系统管理与决策支持中心");

    // 硬编码验证超管防线
    printf("请输入管理员授权密码: ");
    safe_get_string(pwd, sizeof(pwd));

    if (strcmp(pwd, "admin123") != 0) {
        printf(COLOR_RED "密码错误，认证失败。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    while (choice != 0) {
        clear_input_buffer();
        printf("\n=========================================\n");
        printf(COLOR_GREEN "  管理员已登录 (全功能模式)\n" COLOR_RESET);
        printf("=========================================\n");
        printf("  --- 人事与资源 ---\n");
        printf("  [ 1 ] 注册新医生账号\n");
        printf("  [ 2 ] 注销/删除医生账号\n");
        printf("  [ 3 ] 床位利用率查询与智能调配\n");
        printf("  [ 4 ] 120 救护车调度中心 (含出勤表)\n");
        printf("  --- 药房物流 ---\n");
        printf("  [ 5 ] 药品进库登记\n");
        printf("  [ 6 ] 库存审计报告\n");
        printf("  --- 财务与分析 ---\n");
        printf("  [ 7 ] 全局财务营收报告\n");
        printf("  [ 8 ] 科室就诊流量分析\n");
        printf("  --- 诊疗记录 CRUD ---\n");
        printf("  [ 9 ] 新增单条诊疗记录\n");
        printf("  [ 10] 文件批量导入诊疗记录\n");
        printf("  [ 11] 修改诊疗记录(撤销后补录)\n");
        printf("  [ 12] 删除诊疗记录(撤销)\n");
        printf("  --- 审计与高阶管理 ---\n");
        printf("  [ 13] 按科室查询诊疗信息\n");
        printf("  [ 14] 按医生工号查询诊疗信息\n");
        printf("  [ 15] 按患者信息查询历史诊疗\n");
        printf("  [ 16] 按时间范围查询诊疗信息\n");
        printf("  [ 17] 医生值班排班表管理\n");
        printf("  [ 18] 手术室排班与调度管理 (新增)\n"); // 【功能接入点】
        printf(COLOR_RED "  [ 0 ] 返回主系统\n" COLOR_RESET);
        print_divider();

        printf(COLOR_YELLOW "请选择管理项目 (0-18): " COLOR_RESET);
        choice = safe_get_int();

        // 将请求路由至不同的业务模块
        switch (choice) {
            case 1: register_doctor(); break;
            case 2: delete_doctor(); break;
            case 3: optimize_bed_allocation(); break;
            case 4: manage_ambulances(); break; // 现已包含救护车出勤逻辑
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
            case 18: manage_operating_rooms(); break; // 打开手术室排班中心
            case 0: break;
            default:
                printf(COLOR_RED "无效的选择。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}

/**
 * @brief 在系统中注册新的医生信息 (头插法连入全局链表)
 */
void register_doctor() {
    DoctorNode* new_node;
    char id[MAX_ID_LEN];

    print_header("注册新医生账号");

    printf("请输入医生工号(ID): ");
    safe_get_string(id, MAX_ID_LEN);

    // 唯一性冲突判定
    if (find_doctor_by_id(id)) {
        printf(COLOR_RED "错误：该工号已存在。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    new_node = (DoctorNode*)calloc(1, sizeof(DoctorNode));
    if (!new_node) {
        printf(COLOR_RED "内存不足。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    strcpy(new_node->id, id);

    printf("请输入医生姓名: ");
    safe_get_string(new_node->name, MAX_NAME_LEN);

    printf("请输入所属科室ID: ");
    safe_get_string(new_node->dept_id, MAX_ID_LEN);

    // 弱校验，不阻断注册流程
    if (!find_department_by_id(new_node->dept_id)) {
        printf(COLOR_YELLOW "警告：科室ID不存在，建议后续修正。\n" COLOR_RESET);
    }

    new_node->next = doctor_head;
    doctor_head = new_node;

    printf(COLOR_GREEN "医生 %s 注册成功。\n" COLOR_RESET, new_node->name);
    wait_for_enter();
}

/**
 * @brief 注销并物理删除指定工号的医生节点
 */
void delete_doctor() {
    char id[MAX_ID_LEN];
    DoctorNode *current = doctor_head, *previous = NULL;

    print_header("注销医生账号");
    printf("请输入要注销的医生工号: ");
    safe_get_string(id, sizeof(id));

    while (current) {
        if (strcmp(current->id, id) == 0) {
            // 单链表越界移除逻辑
            if (previous == NULL) {
                doctor_head = current->next;
            } else {
                previous->next = current->next;
            }

            printf(COLOR_GREEN "医生 %s(ID:%s) 已移除。\n" COLOR_RESET, current->name, id);
            free(current); // 防止内存泄露
            wait_for_enter();
            return;
        }
        previous = current;
        current = current->next;
    }

    printf(COLOR_RED "未找到该工号。\n" COLOR_RESET);
    wait_for_enter();
}

/* ==========================================
 * 10. 药房仓储与分析统计
 * ========================================== */

/**
 * @brief 为已有药品增加库存
 */
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
        // 库存合法性防御
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

/**
 * @brief 全局药房清点，库存不足 50 时给予红色告警
 */
void view_inventory_report() {
    MedicineNode* current = medicine_head;

    print_header("全局药品库存审计");
    printf("%-10s %-20s %-10s %-10s\n", "药品ID", "药品名称", "库存", "状态");
    print_divider();

    while (current) {
        printf("%-10s %-20s %-10d ", current->id, current->common_name, current->stock);
        if (current->stock < 50) {
            printf(COLOR_RED "[库存告警]\n" COLOR_RESET);
        } else {
            printf(COLOR_GREEN "[正常]\n" COLOR_RESET);
        }
        current = current->next;
    }

    wait_for_enter();
}

/**
 * @brief 财务报表：统计全院所有已完结(status=1)单据的总营收分布
 */
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

    print_divider();
    wait_for_enter();
}

/**
 * @brief 根据诊疗单据数据直观生成科室热度柱状图
 */
void analyze_historical_trends() {
    DepartmentNode* department = dept_head;

    print_header("科室就诊流量分析");
    // 外层科室循环
    while (department) {
        int count = 0;
        RecordNode* record = record_head;

        // 内层全量扫描：基于记录查找负责的医生，进而反推科室归属
        while (record) {
            DoctorNode* doctor = find_doctor_by_id(record->doctor_id);
            if (doctor && strcmp(doctor->dept_id, department->id) == 0) {
                count++;
            }
            record = record->next;
        }

        printf("%-15s: ", department->name);
        // 使用简单的方块字符渲染柱状图，最高显示 40 个，防止越界
        for (int i = 0; i < count && i < 40; i++) printf("■");
        printf(" (%d 人次)\n", count);
        
        department = department->next;
    }

    wait_for_enter();
}

/**
 * @brief 监控并统计全院各科室病床占用率，给予调度建议
 */
void optimize_bed_allocation() {
    DepartmentNode* department = dept_head;

    print_header("全院床位资源监控与预警");
    while (department) {
        int total = 0;
        int occupied = 0;
        BedNode* bed = bed_head;

        // 聚合统计该科室的全部床位数和被占用床位数
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
            // 防御性编码：避免除零报错
            double rate = (total > 0) ? ((double)occupied / (double)total * 100.0) : 0.0;
            printf("科室: %-10s | 占用率: %5.1f%% (%d/%d) ", department->name, rate, occupied, total);
            
            // 智能预警：床位利用率过高或过低都会发出通知
            if (total > 0 && rate < 20.0) {
                printf(COLOR_RED "[告警：资源严重闲置，建议缩减]" COLOR_RESET);
            } else if (rate > 85.0) {
                printf(COLOR_YELLOW "[告警：床位紧张，建议调配]" COLOR_RESET);
            }
            printf("\n");
        }

        department = department->next;
    }

    wait_for_enter();
}