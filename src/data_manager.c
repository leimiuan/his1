#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "common.h"
#include "data_manager.h"
#include "utils.h"

DepartmentNode* dept_head = NULL;
DoctorNode* doctor_head = NULL;
MedicineNode* medicine_head = NULL;
PatientNode* outpatient_head = NULL;
PatientNode* inpatient_head = NULL;
BedNode* bed_head = NULL;
RecordNode* record_head = NULL;

static void safe_copy_text(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

static void normalize_record_amounts(RecordNode* record) {
    if (!record) return;

    if (record->total_fee < 0) {
        record->total_fee = 0;
    }
    if (record->insurance_covered < 0) {
        record->insurance_covered = 0;
    }
    if (record->insurance_covered > record->total_fee) {
        record->insurance_covered = record->total_fee;
    }
    if (record->personal_paid < 0 ||
        record->personal_paid > record->total_fee ||
        (record->insurance_covered + record->personal_paid) > (record->total_fee + 0.0001)) {
        record->personal_paid = record->total_fee - record->insurance_covered;
    }
    if (record->personal_paid < 0) {
        record->personal_paid = 0;
    }
}

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

#define DUTY_SCHEDULE_FILE "data/schedules.txt"
#define MAX_SCHEDULE_ITEMS 1024

typedef struct {
    char date[32];
    char slot[32];
    char dept_id[MAX_ID_LEN];
    char doctor_id[MAX_ID_LEN];
} DutyScheduleItem;

static int is_blank_text(const char* text) {
    const unsigned char* p = (const unsigned char*)text;
    if (!p) return 1;
    while (*p) {
        if (!isspace(*p)) return 0;
        p++;
    }
    return 1;
}

static int validate_schedule_date(const char* date) {
    int y = 0, m = 0, d = 0;
    if (!date || sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) return 0;
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    return 1;
}

static int parse_schedule_line(const char* line,
                               char* date,
                               char* slot,
                               char* dept_id,
                               char* doctor_id) {
    char local[256];
    char* token = NULL;
    char* fields[8] = {0};
    int count = 0;

    if (!line) return 0;
    strncpy(local, line, sizeof(local) - 1);
    local[sizeof(local) - 1] = '\0';
    local[strcspn(local, "\r\n")] = '\0';

    if (local[0] == '\0' || is_blank_text(local)) return 0;
    if (strstr(local, "日期") && strstr(local, "医生")) return 0;

    token = strtok(local, "\t");
    while (token && count < 8) {
        fields[count++] = token;
        token = strtok(NULL, "\t");
    }

    if (count >= 4) {
        safe_copy_text(date, 32, fields[0]);
        safe_copy_text(slot, 32, fields[1]);
        safe_copy_text(dept_id, MAX_ID_LEN, fields[2]);
        safe_copy_text(doctor_id, MAX_ID_LEN, fields[3]);
        return 1;
    }

    if (sscanf(local, "%31s %31s %31s %31s", date, slot, dept_id, doctor_id) == 4) {
        return 1;
    }

    return 0;
}

static int save_schedule_items(const DutyScheduleItem* items, int count) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "w");
    int i = 0;
    if (!file) return 0;

    fprintf(file, "日期\t班次\t科室ID\t医生ID\n");
    for (i = 0; i < count; ++i) {
        fprintf(file, "%s\t%s\t%s\t%s\n",
                items[i].date,
                items[i].slot,
                items[i].dept_id,
                items[i].doctor_id);
    }

    fclose(file);
    return 1;
}

static int load_schedule_items(DutyScheduleItem* items, int max_count) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "r");
    char line[256];
    int count = 0;

    if (!file) return 0;

    while (fgets(line, sizeof(line), file)) {
        DutyScheduleItem item;
        memset(&item, 0, sizeof(item));

        if (!parse_schedule_line(line, item.date, item.slot, item.dept_id, item.doctor_id)) {
            continue;
        }
        if (count >= max_count) {
            break;
        }

        items[count++] = item;
    }

    fclose(file);
    return count;
}

static void ensure_duty_schedule_file(void) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "r");
    if (file) {
        fclose(file);
        return;
    }

    file = fopen(DUTY_SCHEDULE_FILE, "w");
    if (!file) return;

    fprintf(file, "日期\t班次\t科室ID\t医生ID\n");
    fclose(file);
}

void view_duty_schedule(const char* doctor_id_filter) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    int count = 0;
    int i = 0;
    int shown = 0;

    ensure_duty_schedule_file();
    count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);

    print_header("医生值班表");
    printf("%-12s %-8s %-8s %-14s %-8s %-14s\n",
           "日期", "班次", "科室", "科室名称", "医生", "医生姓名");
    print_divider();

    for (i = 0; i < count; ++i) {
        DepartmentNode* dept = NULL;
        DoctorNode* doctor = NULL;

        if (doctor_id_filter && doctor_id_filter[0] != '\0' &&
            strcmp(items[i].doctor_id, doctor_id_filter) != 0) {
            continue;
        }

        dept = find_department_by_id(items[i].dept_id);
        doctor = find_doctor_by_id(items[i].doctor_id);

        printf("%-12s %-8s %-8s %-14s %-8s %-14s\n",
               items[i].date,
               items[i].slot,
               items[i].dept_id,
               dept ? dept->name : "(未知科室)",
               items[i].doctor_id,
               doctor ? doctor->name : "(未知医生)");
        shown++;
    }

    if (shown == 0) {
        printf(COLOR_YELLOW "暂无匹配的排班记录。\n" COLOR_RESET);
    }

    wait_for_enter();
}

int upsert_duty_schedule(const char* date, const char* slot, const char* dept_id, const char* doctor_id) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    int count = 0;
    int i = 0;
    int found = 0;
    DoctorNode* doctor = NULL;

    if (!date || !slot || !dept_id || !doctor_id) return 0;
    if (is_blank_text(date) || is_blank_text(slot) || is_blank_text(dept_id) || is_blank_text(doctor_id)) return 0;
    if (!validate_schedule_date(date)) return 0;

    doctor = find_doctor_by_id(doctor_id);
    if (!doctor) return 0;
    if (!find_department_by_id(dept_id)) return 0;
    if (strcmp(doctor->dept_id, dept_id) != 0) return 0;

    ensure_duty_schedule_file();
    count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);

    for (i = 0; i < count; ++i) {
        if (strcmp(items[i].date, date) == 0 &&
            strcmp(items[i].slot, slot) == 0 &&
            strcmp(items[i].dept_id, dept_id) == 0) {
            safe_copy_text(items[i].doctor_id, sizeof(items[i].doctor_id), doctor_id);
            found = 1;
            break;
        }
    }

    if (!found) {
        if (count >= MAX_SCHEDULE_ITEMS) return 0;
        safe_copy_text(items[count].date, sizeof(items[count].date), date);
        safe_copy_text(items[count].slot, sizeof(items[count].slot), slot);
        safe_copy_text(items[count].dept_id, sizeof(items[count].dept_id), dept_id);
        safe_copy_text(items[count].doctor_id, sizeof(items[count].doctor_id), doctor_id);
        count++;
    }

    return save_schedule_items(items, count);
}

int remove_duty_schedule(const char* date, const char* slot, const char* dept_id) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    DutyScheduleItem kept[MAX_SCHEDULE_ITEMS];
    int count = 0;
    int kept_count = 0;
    int i = 0;
    int removed = 0;

    if (!date || !slot || !dept_id) return 0;

    ensure_duty_schedule_file();
    count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);

    for (i = 0; i < count; ++i) {
        if (strcmp(items[i].date, date) == 0 &&
            strcmp(items[i].slot, slot) == 0 &&
            strcmp(items[i].dept_id, dept_id) == 0) {
            removed = 1;
            continue;
        }
        kept[kept_count++] = items[i];
    }

    if (!removed) return 0;
    return save_schedule_items(kept, kept_count);
}
void add_new_record(const char* patient_id, const char* doctor_id, RecordType type, const char* description, double fee, int queue_num) {
    RecordNode* new_node = (RecordNode*)calloc(1, sizeof(RecordNode));
    if (!new_node) return;

    (void)queue_num;

    sprintf(new_node->record_id, "REC%lld%d", (long long)time(NULL), rand() % 1000);
    strcpy(new_node->patient_id, patient_id);
    strcpy(new_node->doctor_id, doctor_id);
    new_node->type = type;
    new_node->total_fee = fee < 0 ? 0 : fee;
    new_node->queue_number = 0;
    new_node->status = 0;
    strcpy(new_node->description, description);
    get_current_time_str(new_node->date);

    if (type == RECORD_REGISTRATION) {
        RecordNode* curr = record_head;
        int max_queue = 0;

        while (curr) {
            if (curr->type == RECORD_REGISTRATION &&
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

    {
        double coverage_rate = 0.0;
        if (p) {
            if (p->insurance == INSURANCE_WORKER) coverage_rate = 0.7;
            else if (p->insurance == INSURANCE_RESIDENT) coverage_rate = 0.5;
        }

        new_node->insurance_covered = new_node->total_fee * coverage_rate;
        new_node->personal_paid = new_node->total_fee - new_node->insurance_covered;
    }

    normalize_record_amounts(new_node);
    new_node->next = record_head;
    record_head = new_node;
}

void load_records(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[1024];

    if (!file) return;

    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        RecordNode* new_node;
        char tab_line[1024];

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;

        new_node = (RecordNode*)calloc(1, sizeof(RecordNode));
        if (!new_node) break;

        strcpy(tab_line, line);
        if (parse_record_line_tab(new_node, tab_line) || parse_record_line_legacy(new_node, line)) {
            new_node->next = record_head;
            record_head = new_node;
        } else {
            free(new_node);
        }
    }

    fclose(file);
}

void load_doctors(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return;
    char line[512];
    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        DoctorNode* new_node = (DoctorNode*)calloc(1, sizeof(DoctorNode));
        if (sscanf(line, "%31s %127s %31s", new_node->id, new_node->name, new_node->dept_id) >= 2) {
            new_node->next = doctor_head;
            doctor_head = new_node;
        } else { free(new_node); }
    }
    fclose(file);
}

void load_medicines(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return;
    char line[512];
    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        MedicineNode* new_node = (MedicineNode*)calloc(1, sizeof(MedicineNode));
        if (sscanf(line, "%31s %255s %lf %d", new_node->id, new_node->common_name, &new_node->price, &new_node->stock) >= 3) {
            new_node->next = medicine_head;
            medicine_head = new_node;
        } else { free(new_node); }
    }
    fclose(file);
}

void load_departments(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return;
    char line[512];
    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        DepartmentNode* new_node = (DepartmentNode*)calloc(1, sizeof(DepartmentNode));
        if (sscanf(line, "%31s %63s", new_node->id, new_node->name) >= 2) {
            new_node->next = dept_head;
            dept_head = new_node;
        } else { free(new_node); }
    }
    fclose(file);
}

void load_beds(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return;
    char line[512];
    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        BedNode* new_node = (BedNode*)calloc(1, sizeof(BedNode));
        if (sscanf(line, "%31s %31s %d %lf %31s", new_node->bed_id, new_node->dept_id,
                   &new_node->is_occupied, &new_node->daily_fee, new_node->patient_id) >= 4) {
            new_node->next = bed_head;
            bed_head = new_node;
        } else { free(new_node); }
    }
    fclose(file);
}

void load_outpatients(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return;
    char line[1024];
    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        PatientNode* new_node = (PatientNode*)calloc(1, sizeof(PatientNode));
        int ins;
        if (sscanf(line, "%31s %127s %d %15s %c %d %lf", new_node->id, new_node->name, &new_node->age,
                   new_node->gender, &new_node->type, &ins, &new_node->account_balance) >= 6) {
            new_node->insurance = (InsuranceType)ins;
            new_node->next = outpatient_head;
            outpatient_head = new_node;
        } else { free(new_node); }
    }
    fclose(file);
}

void load_inpatients(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return;
    char line[1024];
    fgets(line, sizeof(line), file);
    while (fgets(line, sizeof(line), file)) {
        PatientNode* new_node = (PatientNode*)calloc(1, sizeof(PatientNode));
        int ins;
        if (sscanf(line, "%31s %127s %d %15s %c %d %lf %lf %31s %31s", new_node->id, new_node->name, &new_node->age,
                   new_node->gender, &new_node->type, &ins, &new_node->account_balance,
                   &new_node->hospital_deposit, new_node->admission_date, new_node->bed_id) >= 6) {
            new_node->insurance = (InsuranceType)ins;
            new_node->next = inpatient_head;
            inpatient_head = new_node;
        } else { free(new_node); }
    }
    fclose(file);
}

void save_records(const char* filepath) {
    FILE* file = fopen(filepath, "w");
    if (!file) return;

    fprintf(file, "记录编号\t病人编号\t医生编号\t类型\t日期\t排队号\t状态\t描述\t总费用\t医保支付\t个人自付\n");
    for (RecordNode* curr = record_head; curr != NULL; curr = curr->next) {
        normalize_record_amounts(curr);
        if (curr->date[0] == '\0') {
            get_current_time_str(curr->date);
        }
        fprintf(file, "%s\t%s\t%s\t%d\t%s\t%d\t%d\t%s\t%.2f\t%.2f\t%.2f\n",
                curr->record_id, curr->patient_id, curr->doctor_id, (int)curr->type,
                curr->date, curr->queue_number, curr->status, curr->description,
                curr->total_fee, curr->insurance_covered, curr->personal_paid);
    }

    fclose(file);
}

void save_outpatients(const char* filepath) {
    FILE* file = fopen(filepath, "w");
    if (!file) return;
    fprintf(file, "编号\t姓名\t年龄\t性别\t类型\t医保\t账户余额\n");
    PatientNode* curr = outpatient_head;
    while (curr) {
        fprintf(file, "%s\t%s\t%d\t%s\t%c\t%d\t%.2f\n",
                curr->id, curr->name, curr->age, curr->gender,
                curr->type, (int)curr->insurance, curr->account_balance);
        curr = curr->next;
    }
    fclose(file);
}

void init_system_data() {
    load_departments("data/departments.txt");
    load_doctors("data/doctors.txt");
    load_medicines("data/medicines.txt");
    load_outpatients("data/outpatients.txt");
    load_inpatients("data/inpatients.txt");
    load_beds("data/beds.txt");
    load_records("data/records.txt");
    ensure_duty_schedule_file();
}

void save_system_data() {
    save_outpatients("data/outpatients.txt");
    save_records("data/records.txt");
}

void free_all_data() {
    RecordNode* r = record_head; while (r) { RecordNode* t = r; r = r->next; free(t); } record_head = NULL;
    PatientNode* po = outpatient_head; while (po) { PatientNode* t = po; po = po->next; free(t); } outpatient_head = NULL;
    DoctorNode* doc = doctor_head; while (doc) { DoctorNode* t = doc; doc = doc->next; free(t); } doctor_head = NULL;
}
