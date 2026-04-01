#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==========================================
 * 0. 终端 UI 颜色宏定义 (ANSI Escape Codes)
 * ========================================== */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_CYAN    "\033[1;36m"

/* ==========================================
 * 1. 全局宏定义 (定长缓冲区，防止溢出)
 * ========================================== */
#define MAX_ID_LEN 32        // 用于 ID (P001, D001, REC001 等)
#define MAX_NAME_LEN 128     // 用于姓名
#define MAX_DEPT_LEN 64      // 用于科室名
#define MAX_MED_NAME_LEN 256 // 用于药品通用名/商品名
#define MAX_DESC_LEN 512     // 用于病历描述

/* ==========================================
 * 2. 枚举类型 (必须在结构体之前定义)
 * ========================================== */

// 病房类型
typedef enum {
    WARD_GENERAL = 1,  // 一般房
    WARD_VIP = 2,      // 高级房
    WARD_ICU = 3       // 重症监护室
} WardType;

// 医疗记录业务类型
typedef enum {
    RECORD_REGISTRATION = 1,     // 挂号
    RECORD_CONSULTATION = 2,     // 看诊/处方
    RECORD_EXAMINATION = 3,      // 检查
    RECORD_HOSPITALIZATION = 4   // 住院/转床
} RecordType;

// 医保报销类型
typedef enum {
    INSURANCE_NONE = 0,    // 全自费
    INSURANCE_WORKER = 1,  // 城镇职工 (报销比例高)
    INSURANCE_RESIDENT = 2 // 城乡居民 (报销比例中)
} InsuranceType;

/* ==========================================
 * 3. 核心数据结构 (链表节点)
 * ========================================== */

// 科室节点
typedef struct DepartmentNode {
    char id[MAX_ID_LEN];
    char name[MAX_DEPT_LEN];
    struct DepartmentNode* next;
} DepartmentNode;

// 医生节点
typedef struct DoctorNode {
    char id[MAX_ID_LEN];
    char name[MAX_NAME_LEN];
    char dept_id[MAX_ID_LEN]; // 关联 DepartmentNode 的 ID
    struct DoctorNode* next;
} DoctorNode;

// 患者节点 (统一门诊与住院)
typedef struct PatientNode {
    char id[MAX_ID_LEN];
    char name[MAX_NAME_LEN];
    int age;
    char gender[16];
    char type;                // 'O' 为门诊 (Outpatient), 'I' 为住院 (Inpatient)
    InsuranceType insurance;  // 医保类型
    double account_balance;   // 门诊余额 (用于自主缴费)
    char admission_date[32];  // 入院日期 (仅住院)
    char bed_id[MAX_ID_LEN];  // 床位 ID (仅住院)
    double hospital_deposit;  // 住院预缴押金 (仅住院)
    struct PatientNode* next;
} PatientNode;

// 医疗记录节点 (系统流水账)
typedef struct RecordNode {
    char record_id[MAX_ID_LEN];
    char patient_id[MAX_ID_LEN];
    char doctor_id[MAX_ID_LEN];
    RecordType type;
    char date[32];
    int queue_number;         // 挂号排队号
    
    // 【关键状态字段】：0-候诊/未缴费，1-已完成/已缴费
    int status;               
    
    // 处方结构化描述 (如 PRES|T:1|D:7|ID:M01|Q:2|DATE:2026-03-24)
    char description[MAX_DESC_LEN];
    
    // 财务字段 (核心：用于报表统计和自主扣款)
    double total_fee;         // 总金额
    double insurance_covered; // 医保统筹支付金额
    double personal_paid;     // 个人实际自付金额
    
    struct RecordNode* next;
} RecordNode;

// 药品节点
typedef struct MedicineNode {
    char id[MAX_ID_LEN];
    char common_name[MAX_MED_NAME_LEN];
    char trade_name[MAX_MED_NAME_LEN];
    char alias[MAX_MED_NAME_LEN];
    char related_dept_id[MAX_ID_LEN];
    int stock;                // 药房库存 (发药核销时扣减)
    double price;
    struct MedicineNode* next;
} MedicineNode;

// 床位节点
typedef struct BedNode {
    char bed_id[MAX_ID_LEN];
    WardType type;            // 枚举类型 (1, 2, 3)
    double daily_fee;         // 每日床位费
    char dept_id[MAX_ID_LEN]; // 所属科室
    int is_occupied;          // 0: 空闲, 1: 占用
    char patient_id[MAX_ID_LEN];
    struct BedNode* next;
} BedNode;

/* ==========================================
 * 4. 全局链表头指针 (外部声明)
 * ========================================== */
extern DepartmentNode* dept_head;
extern DoctorNode* doctor_head;
extern MedicineNode* medicine_head;
extern PatientNode* outpatient_head; 
extern PatientNode* inpatient_head;  
extern BedNode* bed_head;
extern RecordNode* record_head;

#endif // COMMON_H