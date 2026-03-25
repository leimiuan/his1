#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "data_manager.h"
#include "utils.h"

/* ==========================================
 * 1. 全局链表头指针实例化
 * ========================================== */
DepartmentNode* dept_head = NULL;
DoctorNode* doctor_head = NULL;
MedicineNode* medicine_head = NULL;
PatientNode* outpatient_head = NULL; 
PatientNode* inpatient_head = NULL;  
BedNode* bed_head = NULL;
RecordNode* record_head = NULL;

/* ==========================================
 * 2. 核心查询接口 (CRUD)
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

// 修正报错位置：将 int type 改为 WardType type
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

/* ==========================================
 * 3. 业务逻辑接口
 * ========================================== */

void add_new_record(const char* patient_id, const char* doctor_id, RecordType type, const char* description, double fee, int queue_num) {
    RecordNode* new_node = (RecordNode*)calloc(1, sizeof(RecordNode));
    if (!new_node) return;
    sprintf(new_node->record_id, "REC%lld%d", (long long)time(NULL), rand() % 1000); 
    strcpy(new_node->patient_id, patient_id);
    strcpy(new_node->doctor_id, doctor_id);
    new_node->type = type;
    new_node->total_fee = fee;

    PatientNode* p = find_outpatient_by_id(patient_id);
    if (!p) p = find_inpatient_by_id(patient_id);

    double coverage_rate = 0.0;
    if (p) {
        if (p->insurance == INSURANCE_WORKER) coverage_rate = 0.7;      
        else if (p->insurance == INSURANCE_RESIDENT) coverage_rate = 0.5; 
    }
    
    new_node->insurance_covered = fee * coverage_rate;
    new_node->personal_paid = fee - new_node->insurance_covered;
    new_node->queue_number = queue_num;
    new_node->status = 0; 
    strcpy(new_node->description, description);
    
    new_node->next = record_head;
    record_head = new_node;
}

/* ==========================================
 * 4. 数据加载模块 (补全了之前省略的部分)
 * ========================================== */

void load_records(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    if (!file) return;
    char line[1024];
    fgets(line, sizeof(line), file); 
    while (fgets(line, sizeof(line), file)) {
        RecordNode* new_node = (RecordNode*)calloc(1, sizeof(RecordNode));
        int type_int, status_int;
        int parsed = sscanf(line, "%s %s %s %d %lf %lf %lf %d %[^\n]", 
                   new_node->record_id, new_node->patient_id, new_node->doctor_id, 
                   &type_int, &new_node->total_fee, &new_node->insurance_covered, 
                   &new_node->personal_paid, &status_int, new_node->description);
        if (parsed >= 4) {
            new_node->type = (RecordType)type_int;
            new_node->status = (parsed >= 8) ? status_int : 1;
            new_node->next = record_head;
            record_head = new_node;
        } else { free(new_node); }
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
        if (sscanf(line, "%s %s %s", new_node->id, new_node->name, new_node->dept_id) >= 2) {
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
        if (sscanf(line, "%s %s %lf %d", new_node->id, new_node->common_name, &new_node->price, &new_node->stock) >= 3) {
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
        if (sscanf(line, "%s %s", new_node->id, new_node->name) >= 2) {
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
        int type_int;
        if (sscanf(line, "%s %s %d %lf %s", new_node->bed_id, new_node->dept_id, 
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
        if (sscanf(line, "%s %s %d %s %c %d %lf", new_node->id, new_node->name, &new_node->age, 
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
        if (sscanf(line, "%s %s %d %s %c %d %lf %lf %s %s", new_node->id, new_node->name, &new_node->age, 
                   new_node->gender, &new_node->type, &ins, &new_node->account_balance, 
                   &new_node->hospital_deposit, new_node->admission_date, new_node->bed_id) >= 6) {
            new_node->insurance = (InsuranceType)ins;
            new_node->next = inpatient_head;
            inpatient_head = new_node;
        } else { free(new_node); }
    }
    fclose(file);
}

/* ==========================================
 * 5. 数据保存模块
 * ========================================== */

void save_records(const char* filepath) {
    FILE* file = fopen(filepath, "w");
    if (!file) return;
    fprintf(file, "记录编号\t病人编号\t医生编号\t类型\t总费用\t医保支付\t个人自付\t状态\t描述\n");
    RecordNode* curr = record_head;
    while (curr) {
        fprintf(file, "%s\t%s\t%s\t%d\t%.2f\t%.2f\t%.2f\t%d\t%s\n",
                curr->record_id, curr->patient_id, curr->doctor_id,
                (int)curr->type, curr->total_fee, curr->insurance_covered, 
                curr->personal_paid, curr->status, curr->description);
        curr = curr->next;
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

// ... 此处可按需补全其他 save 函数

/* ==========================================
 * 6. 系统生命周期控制
 * ========================================== */

void init_system_data() {
    load_departments("data/departments.txt");
    load_doctors("data/doctors.txt");
    load_medicines("data/medicines.txt");
    load_outpatients("data/outpatients.txt");
    load_inpatients("data/inpatients.txt");
    load_beds("data/beds.txt");
    load_records("data/records.txt");
}

void save_system_data() {
    save_outpatients("data/outpatients.txt");
    save_records("data/records.txt");
    // 补全其他保存逻辑...
}

void free_all_data() {
    RecordNode* r = record_head; while(r) { RecordNode* t = r; r = r->next; free(t); } record_head = NULL;
    PatientNode* po = outpatient_head; while(po) { PatientNode* t = po; po = po->next; free(t); } outpatient_head = NULL;
    DoctorNode* doc = doctor_head; while(doc) { DoctorNode* t = doc; doc = doc->next; free(t); } doctor_head = NULL;
}