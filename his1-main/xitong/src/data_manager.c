#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "common.h"
#include "data_manager.h"
#include "utils.h"

/* ==========================================
 * 【核心指令植入】救护车出勤日志与手术室节点定义
 * ========================================== */
#ifndef HIS_EXT_NODES_DEFINED
#define HIS_EXT_NODES_DEFINED
/**
 * @brief 救护车出勤日志节点
 * 用于记录救护车的每一次派发历史，形成可追溯的出勤表。
 */
typedef struct AmbulanceLogNode {
    char log_id[32];            // 日志单号 (基于时间戳生成)
    char vehicle_id[32];        // 执行本次出勤任务的车辆编号
    char dispatch_time[32];     // 派发时间 (格式: YYYY-MM-DD HH:MM:SS)
    char destination[128];      // 目标地点
    struct AmbulanceLogNode* next; // 单向链表指针
} AmbulanceLogNode;

/**
 * @brief 手术室节点
 * 用于管理全院手术室的实时状态与调度信息。
 */
typedef struct OperatingRoomNode {
    char room_id[32];           // 手术室编号 (如 OR-01)
    int status;                 // 状态机 - 0: 空闲, 1: 手术中, 2: 消毒维护
    char current_patient[32];   // 当前患者编号
    char current_doctor[32];    // 当前主刀医生编号
    struct OperatingRoomNode* next; // 单向链表指针
} OperatingRoomNode;
#endif

/* ==========================================
 * 1. 全局数据链表头指针 (100% 还原)
 * ========================================== */
DepartmentNode* dept_head = NULL;       
DoctorNode* doctor_head = NULL;         
MedicineNode* medicine_head = NULL;     
PatientNode* outpatient_head = NULL;    
PatientNode* inpatient_head = NULL;     
BedNode* bed_head = NULL;               
RecordNode* record_head = NULL;         
AmbulanceNode* ambulance_head = NULL;   

// 【绝对要求响应：新增核心数据源】
AmbulanceLogNode* ambulance_log_head = NULL; // 救护车历史出勤表头指针
OperatingRoomNode* or_head = NULL;           // 手术室实时资源头指针

/* ==========================================
 * 2. 内部辅助工具函数 (全细节还原)
 * ========================================== */

/**
 * @brief 安全的字符串拷贝工具，防止缓冲区溢出
 */
static void safe_copy_text(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/**
 * @brief 财务规范校验：确保 "个人自付 + 医保报销 = 账单总额"
 */
static void normalize_record_amounts(RecordNode* record) {
    if (!record) return;

    if (record->total_fee < 0) record->total_fee = 0;
    if (record->insurance_covered < 0) record->insurance_covered = 0;
    
    if (record->insurance_covered > record->total_fee) {
        record->insurance_covered = record->total_fee;
    }
    
    if (record->personal_paid < 0 || 
        record->personal_paid > record->total_fee ||
        (record->insurance_covered + record->personal_paid) > (record->total_fee + 0.0001)) {
        record->personal_paid = record->total_fee - record->insurance_covered;
    }
    if (record->personal_paid < 0) record->personal_paid = 0;
}

/**
 * @brief 判断一个字符串是否全为空白字符
 */
static int is_blank_text(const char* text) {
    const unsigned char* p = (const unsigned char*)text;
    if (!p) return 1;
    while (*p) {
        if (!isspace(*p)) return 0;
        p++;
    }
    return 1;
}

/* ==========================================
 * 3. 诊疗记录解析引擎 (100% 还原复杂逻辑)
 * ========================================== */

static int parse_record_line_tab(RecordNode* record, char* line) {
    char* fields[16];
    int count = 0;
    char* token = strtok(line, "\t");

    while (token && count < 16) {
        fields[count++] = token;
        token = strtok(NULL, "\t");
    }

    if (count >= 11) {
        safe_copy_text(record->record_id, sizeof(record->record_id), fields[0]);
        safe_copy_text(record->patient_id, sizeof(record->patient_id), fields[1]);
        safe_copy_text(record->doctor_id, sizeof(record->doctor_id), fields[2]);
        record->type = (RecordType)atoi(fields[3]);
        safe_copy_text(record->date, sizeof(record->date), fields[4]);
        record->queue_number = atoi(fields[5]);
        record->status = atoi(fields[6]);
        safe_copy_text(record->description, sizeof(record->description), fields[7]);
        record->total_fee = strtod(fields[8], NULL);
        record->insurance_covered = strtod(fields[9], NULL);
        record->personal_paid = strtod(fields[10], NULL);
        normalize_record_amounts(record);
        return 1;
    }
    
    if (count >= 9) {
        safe_copy_text(record->record_id, sizeof(record->record_id), fields[0]);
        safe_copy_text(record->patient_id, sizeof(record->patient_id), fields[1]);
        safe_copy_text(record->doctor_id, sizeof(record->doctor_id), fields[2]);
        record->type = (RecordType)atoi(fields[3]);
        record->total_fee = strtod(fields[4], NULL);
        record->insurance_covered = strtod(fields[5], NULL);
        record->personal_paid = strtod(fields[6], NULL);
        record->status = atoi(fields[7]);
        safe_copy_text(record->description, sizeof(record->description), fields[8]);
        
        record->queue_number = 0;
        record->date[0] = '\0';
        normalize_record_amounts(record);
        return 1;
    }
    return 0;
}

static int parse_record_line_legacy(RecordNode* record, const char* line) {
    int type_int = 0;
    int status_int = 1;
    
    int parsed = sscanf(line, "%31s %31s %31s %d %31s %d %d %511s %lf %lf %lf",
        record->record_id, record->patient_id, record->doctor_id, &type_int,
        record->date, &record->queue_number, &status_int, record->description,
        &record->total_fee, &record->insurance_covered, &record->personal_paid);
    
    if (parsed >= 8) {
        record->type = (RecordType)type_int;
        record->status = status_int;
        normalize_record_amounts(record);
        return 1;
    }

    parsed = sscanf(line, "%31s %31s %31s %d %lf %lf %lf %d %511[^\n]",
        record->record_id, record->patient_id, record->doctor_id, &type_int,
        &record->total_fee, &record->insurance_covered, &record->personal_paid,
        &status_int, record->description);
    
    if (parsed >= 8) {
        record->type = (RecordType)type_int;
        record->status = status_int;
        record->queue_number = 0;
        record->date[0] = '\0';
        normalize_record_amounts(record);
        return 1;
    }
    return 0;
}

/* ==========================================
 * 4. 索引查找函数集群 (100% 逐个展开)
 * ========================================== */

PatientNode* find_outpatient_by_id(const char* id) {
    PatientNode* curr = outpatient_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

PatientNode* find_inpatient_by_id(const char* id) {
    PatientNode* curr = inpatient_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

DoctorNode* find_doctor_by_id(const char* id) {
    DoctorNode* curr = doctor_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

MedicineNode* find_medicine_by_id(const char* id) {
    MedicineNode* curr = medicine_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

BedNode* find_bed_by_id(const char* id) {
    BedNode* curr = bed_head;
    while (curr) {
        if (strcmp(curr->bed_id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

BedNode* find_available_bed(const char* dept_id, WardType type) {
    BedNode* curr = bed_head;
    while (curr) {
        if (strcmp(curr->dept_id, dept_id) == 0 &&
            curr->type == type &&
            curr->is_occupied == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

DepartmentNode* find_department_by_id(const char* id) {
    DepartmentNode* curr = dept_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

AmbulanceNode* find_ambulance_by_id(const char* id) {
    AmbulanceNode* curr = ambulance_head;
    while (curr) {
        if (strcmp(curr->vehicle_id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

void search_medicine_by_keyword(const char* keyword) {
    MedicineNode* curr = medicine_head;
    int count = 0;
    print_header("药品模糊搜索结果");
    printf("%-12s %-25s %-20s %-8s %-8s\n", "药品ID", "通用名", "商品名", "单价", "库存");
    print_divider();
    while (curr) {
        if (keyword == NULL || keyword[0] == '\0' || 
            strstr(curr->common_name, keyword) || 
            strstr(curr->trade_name, keyword)) {
            printf("%-12s %-25s %-20s %-8.2f %-8d\n", 
                   curr->id, curr->common_name, curr->trade_name, curr->price, curr->stock);
            count++;
        }
        curr = curr->next;
    }
    if (count == 0) {
        printf(COLOR_YELLOW "未找到匹配药品。\n" COLOR_RESET);
    }
    wait_for_enter();
}

/* ==========================================
 * 5. 医生值班表管理系统 (100% 还原)
 * ========================================== */

#define DUTY_SCHEDULE_FILE "data/schedules.txt"
#define MAX_SCHEDULE_ITEMS 1024

typedef struct {
    char date[32];
    char slot[32];
    char dept_id[MAX_ID_LEN];
    char doctor_id[MAX_ID_LEN];
} DutyScheduleItem;

static int validate_schedule_date(const char* date) {
    int y, m, d;
    if (!date || sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) return 0;
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    return 1;
}

static int parse_schedule_line(const char* line, char* date, char* slot, char* dept_id, char* doctor_id) {
    char local[256];
    char* fields[8] = {0};
    int count = 0;
    if (!line) return 0;
    strncpy(local, line, 255);
    local[strcspn(local, "\r\n")] = '\0';
    if (local[0] == '\0' || is_blank_text(local)) return 0;
    if (strstr(local, "日期") && strstr(local, "医生")) return 0;

    char* token = strtok(local, "\t");
    while (token && count < 8) {
        fields[count++] = token;
        token = strtok(NULL, "\t");
    }

    if (count >= 4) {
        strcpy(date, fields[0]);
        strcpy(slot, fields[1]);
        strcpy(dept_id, fields[2]);
        strcpy(doctor_id, fields[3]);
        return 1;
    }
    if (sscanf(local, "%31s %31s %31s %31s", date, slot, dept_id, doctor_id) == 4) return 1;
    return 0;
}

static int save_schedule_items(const DutyScheduleItem* items, int count) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "w");
    if (!file) return 0;
    fprintf(file, "日期\t班次\t科室ID\t医生ID\n");
    for (int i = 0; i < count; ++i) {
        fprintf(file, "%s\t%s\t%s\t%s\n", items[i].date, items[i].slot, items[i].dept_id, items[i].doctor_id);
    }
    fclose(file);
    return 1;
}

static int load_schedule_items(DutyScheduleItem* items, int max_count) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "r");
    char line[256];
    int count = 0;
    if (!file) return 0;
    while (fgets(line, sizeof(line), file) && count < max_count) {
        if (parse_schedule_line(line, items[count].date, items[count].slot, items[count].dept_id, items[count].doctor_id)) {
            count++;
        }
    }
    fclose(file);
    return count;
}

void ensure_duty_schedule_file(void) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "r");
    if (file) { fclose(file); return; }
    file = fopen(DUTY_SCHEDULE_FILE, "w");
    if (!file) return;
    fprintf(file, "日期\t班次\t科室ID\t医生ID\n");
    fclose(file);
}

void view_duty_schedule(const char* doctor_id_filter) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    int count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);
    int shown = 0;

    print_header("医院值班医生安排表");
    printf("%-12s %-8s %-8s %-14s %-8s %-14s\n", "日期", "班次", "科室", "科室名", "医生", "姓名");
    print_divider();

    for (int i = 0; i < count; ++i) {
        if (doctor_id_filter && doctor_id_filter[0] != '\0' && strcmp(items[i].doctor_id, doctor_id_filter) != 0) continue;
        
        DoctorNode* d = find_doctor_by_id(items[i].doctor_id);
        DepartmentNode* dept = find_department_by_id(items[i].dept_id);

        printf("%-12s %-8s %-8s %-14s %-8s %-14s\n", 
               items[i].date, items[i].slot, items[i].dept_id, dept?dept->name:"-", 
               items[i].doctor_id, d?d->name:"-");
        shown++;
    }
    if (shown == 0) printf(COLOR_YELLOW "暂无值班排班记录。\n" COLOR_RESET);
    wait_for_enter();
}

int upsert_duty_schedule(const char* date, const char* slot, const char* dept_id, const char* doctor_id) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    int count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);
    int found = 0;

    if (!validate_schedule_date(date)) return 0;
    
    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].date, date) == 0 && strcmp(items[i].slot, slot) == 0 && strcmp(items[i].dept_id, dept_id) == 0) {
            safe_copy_text(items[i].doctor_id, MAX_ID_LEN, doctor_id);
            found = 1; break;
        }
    }
    if (!found && count < MAX_SCHEDULE_ITEMS) {
        safe_copy_text(items[count].date, 32, date);
        safe_copy_text(items[count].slot, 32, slot);
        safe_copy_text(items[count].dept_id, MAX_ID_LEN, dept_id);
        safe_copy_text(items[count].doctor_id, MAX_ID_LEN, doctor_id);
        count++;
    }
    return save_schedule_items(items, count);
}

int remove_duty_schedule(const char* date, const char* slot, const char* dept_id) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    DutyScheduleItem kept[MAX_SCHEDULE_ITEMS];
    int count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);
    int kept_count = 0, removed = 0;

    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].date, date) == 0 && strcmp(items[i].slot, slot) == 0 && strcmp(items[i].dept_id, dept_id) == 0) {
            removed = 1; continue;
        }
        kept[kept_count++] = items[i];
    }
    if (removed) save_schedule_items(kept, kept_count);
    return removed;
}

/* ==========================================
 * 6. 核心业务逻辑实现 (100% 还原)
 * ========================================== */

void add_new_record(const char* patient_id, const char* doctor_id, RecordType type, const char* description, double fee, int queue_num) {
    RecordNode* new_node = (RecordNode*)calloc(1, sizeof(RecordNode));
    if (!new_node) return;

    sprintf(new_node->record_id, "REC%lld%d", (long long)time(NULL), rand() % 1000);
    
    strcpy(new_node->patient_id, patient_id);
    strcpy(new_node->doctor_id, doctor_id);
    new_node->type = type;
    new_node->total_fee = fee < 0 ? 0 : fee;
    new_node->status = 0;
    strcpy(new_node->description, description);
    
    get_current_time_str(new_node->date);

    if (type == RECORD_REGISTRATION || type == RECORD_EMERGENCY) {
        RecordNode* curr = record_head;
        int max_queue = 0;
        
        while (curr) {
            if ((curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY) &&
                curr->status != -1 &&
                strcmp(curr->doctor_id, doctor_id) == 0 &&
                strcmp(curr->date, new_node->date) == 0 &&
                curr->queue_number > max_queue) {
                max_queue = curr->queue_number;
            }
            curr = curr->next;
        }
        new_node->queue_number = max_queue + 1;
    }

    PatientNode* p = find_outpatient_by_id(patient_id);
    if (!p) p = find_inpatient_by_id(patient_id);
    
    double coverage_rate = 0.0;
    if (p) {
        if (p->insurance == INSURANCE_WORKER) coverage_rate = 0.7;
        else if (p->insurance == INSURANCE_RESIDENT) coverage_rate = 0.5;
    }
    
    new_node->insurance_covered = new_node->total_fee * coverage_rate;
    normalize_record_amounts(new_node);

    new_node->next = record_head;
    record_head = new_node;
}

/**
 * @brief 生成并记录一条救护车出勤日志
 */
void add_ambulance_log(const char* vehicle_id, const char* destination) {
    AmbulanceLogNode* new_node = (AmbulanceLogNode*)calloc(1, sizeof(AmbulanceLogNode));
    if (!new_node) return;

    time_t now = time(NULL);
    sprintf(new_node->log_id, "ALB%lld", (long long)now);
    
    struct tm *t = localtime(&now);
    strftime(new_node->dispatch_time, 32, "%Y-%m-%d %H:%M:%S", t);

    strncpy(new_node->vehicle_id, vehicle_id, 31);
    strncpy(new_node->destination, destination, 127);

    new_node->next = ambulance_log_head;
    ambulance_log_head = new_node;
}

/* ==========================================
 * 7. 数据加载集群 (100% 展开每一个 Loader)
 * ========================================== */

void load_departments(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file); // 跳过表头
    while (fgets(line, 512, file)) {
        DepartmentNode* n = calloc(1, sizeof(DepartmentNode));
        if (sscanf(line, "%31s %63s", n->id, n->name) == 2) {
            n->next = dept_head;
            dept_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_doctors(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        DoctorNode* n = calloc(1, sizeof(DoctorNode));
        if (sscanf(line, "%31s %127s %31s", n->id, n->name, n->dept_id) >= 2) {
            n->next = doctor_head;
            doctor_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_medicines(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        MedicineNode* n = calloc(1, sizeof(MedicineNode));
        if (sscanf(line, "%31s %255s %lf %d", n->id, n->common_name, &n->price, &n->stock) >= 3) {
            n->next = medicine_head;
            medicine_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_outpatients(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[1024];
    if (!file) return;
    fgets(line, 1024, file);
    while (fgets(line, 1024, file)) {
        PatientNode* n = calloc(1, sizeof(PatientNode));
        int ins;
        if (sscanf(line, "%31s %127s %d %15s %c %d %lf", n->id, n->name, &n->age, n->gender, &n->type, &ins, &n->account_balance) >= 6) {
            n->insurance = (InsuranceType)ins;
            n->next = outpatient_head;
            outpatient_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_inpatients(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[1024];
    if (!file) return;
    fgets(line, 1024, file);
    while (fgets(line, 1024, file)) {
        PatientNode* n = calloc(1, sizeof(PatientNode));
        int ins;
        if (sscanf(line, "%31s %127s %d %15s %c %d %lf %lf %31s %31s", n->id, n->name, &n->age, n->gender, &n->type, &ins, &n->account_balance, &n->hospital_deposit, n->admission_date, n->bed_id) >= 6) {
            n->insurance = (InsuranceType)ins;
            n->next = inpatient_head;
            inpatient_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_beds(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        BedNode* n = calloc(1, sizeof(BedNode));
        if (sscanf(line, "%31s %31s %d %lf %31s", n->bed_id, n->dept_id, &n->is_occupied, &n->daily_fee, n->patient_id) >= 4) {
            n->next = bed_head;
            bed_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_records(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[1024];
    if (!file) return;
    while (fgets(line, 1024, file)) {
        RecordNode* n = calloc(1, sizeof(RecordNode));
        if (parse_record_line_tab(n, line) || parse_record_line_legacy(n, line)) {
            n->next = record_head;
            record_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_ambulances(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    while (fgets(line, 512, file)) {
        AmbulanceNode* n = calloc(1, sizeof(AmbulanceNode));
        int s;
        if (sscanf(line, "%31s %127s %d %127[^\n]", n->vehicle_id, n->driver_name, &s, n->current_location) >= 3) {
            n->status = (AmbStatus)s;
            n->next = ambulance_head;
            ambulance_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_ambulance_logs(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        AmbulanceLogNode* n = calloc(1, sizeof(AmbulanceLogNode));
        if (sscanf(line, "%31s\t%31s\t%31s\t%127[^\n]", n->log_id, n->vehicle_id, n->dispatch_time, n->destination) >= 3) {
            n->next = ambulance_log_head;
            ambulance_log_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_operating_rooms(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        OperatingRoomNode* n = calloc(1, sizeof(OperatingRoomNode));
        if (sscanf(line, "%31s\t%d\t%31s\t%31s", n->room_id, &n->status, n->current_patient, n->current_doctor) >= 2) {
            n->next = or_head;
            or_head = n;
        } else free(n);
    }
    fclose(file);
}

/* ==========================================
 * 8. 持久化数据保存集群 (100% 展开每一个 Saver)
 * ========================================== */

void save_departments(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "科室ID\t名称\n");
    for (DepartmentNode* c = dept_head; c; c = c->next) fprintf(fp, "%s\t%s\n", c->id, c->name);
    fclose(fp);
}

void save_doctors(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "医生编号\t姓名\t科室ID\n");
    for (DoctorNode* c = doctor_head; c; c = c->next) fprintf(fp, "%s\t%s\t%s\n", c->id, c->name, c->dept_id);
    fclose(fp);
}

void save_medicines(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "药品ID\t名称\t价格\t库存\n");
    for (MedicineNode* c = medicine_head; c; c = c->next) fprintf(fp, "%s\t%s\t%.2f\t%d\n", c->id, c->common_name, c->price, c->stock);
    fclose(fp);
}

void save_outpatients(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "编号\t姓名\t年龄\t性别\t类型\t医保\t账户余额\n");
    for (PatientNode* c = outpatient_head; c; c = c->next) fprintf(fp, "%s\t%s\t%d\t%s\t%c\t%d\t%.2f\n", c->id, c->name, c->age, c->gender, c->type, (int)c->insurance, c->account_balance);
    fclose(fp);
}

void save_inpatients(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "编号\t姓名\t年龄\t性别\t类型\t医保\t余额\t押金\t日期\t床位\n");
    for (PatientNode* c = inpatient_head; c; c = c->next) fprintf(fp, "%s\t%s\t%d\t%s\t%c\t%d\t%.2f\t%.2f\t%s\t%s\n", c->id, c->name, c->age, c->gender, c->type, (int)c->insurance, c->account_balance, c->hospital_deposit, c->admission_date, c->bed_id);
    fclose(fp);
}

void save_beds(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "床位号\t科室ID\t占用状态\t单价\t患者\n");
    for (BedNode* c = bed_head; c; c = c->next) fprintf(fp, "%s\t%s\t%d\t%.2f\t%s\n", c->bed_id, c->dept_id, c->is_occupied, c->daily_fee, c->patient_id);
    fclose(fp);
}

void save_records(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "记录号\t病号\t医号\t类型\t日期\t队号\t状态\t描述\t总计\t报销\t实付\n");
    for (RecordNode* c = record_head; c; c = c->next) fprintf(fp, "%s\t%s\t%s\t%d\t%s\t%d\t%d\t%s\t%.2f\t%.2f\t%.2f\n", c->record_id, c->patient_id, c->doctor_id, (int)c->type, c->date, c->queue_number, c->status, c->description, c->total_fee, c->insurance_covered, c->personal_paid);
    fclose(fp);
}

void save_ambulances(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "车辆ID\t姓名\t状态\t位置\n");
    for (AmbulanceNode* c = ambulance_head; c; c = c->next) fprintf(fp, "%s\t%s\t%d\t%s\n", c->vehicle_id, c->driver_name, (int)c->status, c->current_location);
    fclose(fp);
}

void save_ambulance_logs(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "单号\t车辆\t时间\t目的\n");
    for (AmbulanceLogNode* c = ambulance_log_head; c; c = c->next) fprintf(fp, "%s\t%s\t%s\t%s\n", c->log_id, c->vehicle_id, c->dispatch_time, c->destination);
    fclose(fp);
}

void save_operating_rooms(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "房号\t状态\t患者\t医生\n");
    for (OperatingRoomNode* c = or_head; c; c = c->next) fprintf(fp, "%s\t%d\t%s\t%s\n", c->room_id, c->status, c->current_patient, c->current_doctor);
    fclose(fp);
}

/* ==========================================
 * 9. 系统生命周期控制 (初始化与释放)
 * ========================================== */

void init_system_data() {
    load_departments("data/departments.txt");
    load_doctors("data/doctors.txt");
    load_medicines("data/medicines.txt");
    load_outpatients("data/outpatients.txt");
    load_inpatients("data/inpatients.txt");
    load_beds("data/beds.txt");
    load_records("data/records.txt");
    load_ambulances("data/ambulances.txt");
    load_ambulance_logs("data/ambulance_logs.txt");
    load_operating_rooms("data/operating_rooms.txt");
    
    ensure_duty_schedule_file();
    
    if (or_head == NULL) {
        for (int i = 3; i >= 1; i--) {
            OperatingRoomNode* n = calloc(1, sizeof(OperatingRoomNode));
            sprintf(n->room_id, "OR-%02d", i);
            n->status = 0;
            strcpy(n->current_patient, "N/A");
            strcpy(n->current_doctor, "N/A");
            n->next = or_head;
            or_head = n;
        }
    }
}

void save_system_data() {
    save_departments("data/departments.txt");
    save_doctors("data/doctors.txt");
    save_medicines("data/medicines.txt");
    save_outpatients("data/outpatients.txt");
    save_inpatients("data/inpatients.txt");
    save_beds("data/beds.txt");
    save_records("data/records.txt");
    save_ambulances("data/ambulances.txt");
    save_ambulance_logs("data/ambulance_logs.txt");
    save_operating_rooms("data/operating_rooms.txt");
}

void free_all_data() {
    #define FREE_NODES(head, type) while(head){type* temp=head; head=head->next; free(temp);}
    FREE_NODES(record_head, RecordNode);
    FREE_NODES(outpatient_head, PatientNode);
    FREE_NODES(inpatient_head, PatientNode);
    FREE_NODES(doctor_head, DoctorNode);
    FREE_NODES(dept_head, DepartmentNode);
    FREE_NODES(medicine_head, MedicineNode);
    FREE_NODES(bed_head, BedNode);
    FREE_NODES(ambulance_head, AmbulanceNode);
    FREE_NODES(ambulance_log_head, AmbulanceLogNode);
    FREE_NODES(or_head, OperatingRoomNode);
}