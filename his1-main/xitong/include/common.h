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
#define MAX_ADDR_LEN 256     // 用于救护车定位描述

/* ==========================================
 * 2. 枚举类型 (必须在结构体之前定义)
 * ========================================== */

// 病房/病床类型 (完全适配PDF补充材料第2条) 
typedef enum {
    WARD_SPECIAL = 1,           // 特殊病房 (ICU/监护)
    WARD_GENERAL = 2,           // 普通病房
    WARD_SINGLE = 3,            // 单人病房
    WARD_DOUBLE = 4,            // 双人病房
    WARD_TRIPLE = 5,            // 三人病房
    WARD_ESCORTED = 6,          // 单人陪护病房
    WARD_SANATORIUM = 7         // 单人陪护疗养病房
} WardType;

// 医疗记录业务类型 (增加急诊类型)
typedef enum {
    RECORD_REGISTRATION = 1,     // 普通门诊挂号
    RECORD_CONSULTATION = 2,     // 问诊/处方
    RECORD_EXAMINATION = 3,      // 检查
    RECORD_HOSPITALIZATION = 4,  // 住院/转床
    RECORD_EMERGENCY = 5         // 急诊绿色通道 (高优先级)
} RecordType;

// 医保报销类型 (根据代码逻辑定义)
typedef enum {
    INSURANCE_NONE = 0,    // 全自费
    INSURANCE_WORKER = 1,  // 城镇职工 (报销70%)
    INSURANCE_RESIDENT = 2 // 城乡居民 (报销50%)
} InsuranceType;

// 救护车状态枚举
typedef enum {
    AMB_IDLE = 0,          // 空闲待命
    AMB_DISPATCHED = 1,    // 执行任务中
    AMB_MAINTENANCE = 2    // 维保消毒中
} AmbStatus;

/* ==========================================
 * 3. 核心数据结构 (全链表架构)
 * ========================================== */

// 科室节点
typedef struct DepartmentNode {
    char id[MAX_ID_LEN];
    char name[MAX_DEPT_LEN];
    struct DepartmentNode* next;
} DepartmentNode;

// 医生节点 (含级别与出诊时间)
typedef struct DoctorNode {
    char id[MAX_ID_LEN];
    char name[MAX_NAME_LEN];
    char level[32];           // 职称：主任医师等
    char dept_id[MAX_ID_LEN]; // 关联科室
    char duty_time[64];       // 出诊时间描述
    struct DoctorNode* next;
} DoctorNode;

// 患者节点 (统一门诊与住院)
typedef struct PatientNode {
    char id[MAX_ID_LEN];
    char name[MAX_NAME_LEN];
    int age;
    char gender[16];
    char type;                // 'O' 门诊, 'I' 住院
    InsuranceType insurance;  
    double account_balance;   // 账户余额
    char admission_date[32];  // 入院日期 (YYYY-MM-DD HH:MM)
    char bed_id[MAX_ID_LEN];  // 床位号
    double hospital_deposit;  // 住院预缴押金 (需>=1000)
    struct PatientNode* next;
} PatientNode;

// 医疗记录节点 (核心审计流水)
typedef struct RecordNode {
    char record_id[MAX_ID_LEN];
    char patient_id[MAX_ID_LEN];
    char doctor_id[MAX_ID_LEN];
    RecordType type;
    char date[32];            // 记录日期
    int queue_number;         // 挂号排队号
    
    // 状态：0-待处理/未缴费, 1-已完成/已缴费, -1-已撤销 (冲红) 
    int status;               
    
    char description[MAX_DESC_LEN]; // 处方或检查详情
    
    // 财务字段 (精确到分)
    double total_fee;         // 总金额
    double insurance_covered; // 医保报销额
    double personal_paid;     // 个人实付额
    
    struct RecordNode* next;
} RecordNode;

// 药品节点
typedef struct MedicineNode {
    char id[MAX_ID_LEN];
    char common_name[MAX_MED_NAME_LEN];
    char trade_name[MAX_MED_NAME_LEN];
    int stock;                // 库存
    double price;             // 单价
    struct MedicineNode* next;
} MedicineNode;

// 床位节点
typedef struct BedNode {
    char bed_id[MAX_ID_LEN];
    char dept_id[MAX_ID_LEN]; // 所属科室
    WardType type;            // 病房类型
    double daily_fee;         // 每日床位费
    int is_occupied;          // 0:空闲, 1:占用
    char patient_id[MAX_ID_LEN];
    struct BedNode* next;
} BedNode;

// 新增：120救护车节点
typedef struct AmbulanceNode {
    char vehicle_id[MAX_ID_LEN]; // 车牌/编号
    char driver_name[MAX_NAME_LEN];
    AmbStatus status;
    char current_location[MAX_ADDR_LEN];
    struct AmbulanceNode* next;
} AmbulanceNode;

/* ==========================================
 * 【绝对指令植入】救护车出勤日志与手术室节点定义
 * 提升至全局头文件，使全系统均可无缝调用联动
 * ========================================== */

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
 * 用于管理全院手术室的实时状态与三态流转信息。
 */
typedef struct OperatingRoomNode {
    char room_id[32];           // 手术室编号 (如 OR-01)
    int status;                 // 状态机 - 0: 空闲, 1: 手术中, 2: 消毒维护
    char current_patient[32];   // 当前患者编号
    char current_doctor[32];    // 当前主刀医生编号
    struct OperatingRoomNode* next; // 单向链表指针
} OperatingRoomNode;


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
extern AmbulanceNode* ambulance_head; // 原有救护车实时节点

// 【绝对要求响应：新增核心数据源全局声明】
extern AmbulanceLogNode* ambulance_log_head; // 救护车历史出勤表全局头指针
extern OperatingRoomNode* or_head;           // 手术室实时资源全局头指针

#endif // COMMON_H