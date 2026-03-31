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
 * 【新增：系统级挂号限流防刷参数】 
 * 严格满足题签重点要求：每天医院最多500号，医生最多20号，患者最多5号
 * ========================================== */
#define MAX_CLINIC_DAILY_TICKETS 500  // 医院每日总号源上限
#define MAX_DOCTOR_DAILY_TICKETS 20   // 单名医生每日号源上限
#define MAX_PATIENT_DAILY_TICKETS 5   // 单名患者每日挂号上限

/* ==========================================
 * 2. 枚举类型 (必须在结构体之前定义)
 * ========================================== */

// 病房/病床类型 (完全适配软件学院 2019 级荣誉班补充材料第2条) 
typedef enum {
    WARD_SPECIAL = 1,           // 特殊病房 (ICU/重症监护)
    WARD_GENERAL = 2,           // 普通病房
    WARD_SINGLE = 3,            // 单人病房
    WARD_DOUBLE = 4,            // 双人病房
    WARD_TRIPLE = 5,            // 三人病房
    WARD_ESCORTED = 6,          // 单人陪护病房
    WARD_SANATORIUM = 7         // 单人陪护疗养病房 (触发熔断时可降级为双人病房)
} WardType;

// 医疗记录业务类型 
// 【完美闭环】：增加急诊通道、外科手术与体检，构建真实医院全栈生态
typedef enum {
    RECORD_REGISTRATION = 1,     // 普通门诊挂号
    RECORD_CONSULTATION = 2,     // 问诊/处方
    RECORD_EXAMINATION = 3,      // 检查
    RECORD_HOSPITALIZATION = 4,  // 住院/转床
    RECORD_EMERGENCY = 5,        // 急诊绿色通道 (高优先级，救护车拉回自动生成)
    RECORD_SURGERY = 6,          // 外科手术预约与计费
    RECORD_CHECKUP = 7           // 健康体检套餐与报告生成
} RecordType;

// 向下兼容宏：抑制其他 .c 文件中可能存在的局部防呆宏定义，防止编译报警
#define RECORD_SURGERY 6
#define RECORD_CHECKUP 7

// 医保报销类型
typedef enum {
    INSURANCE_NONE = 0,    // 全自费
    INSURANCE_WORKER = 1,  // 城镇职工 (报销 70%)
    INSURANCE_RESIDENT = 2 // 城乡居民 (报销 50%)
} InsuranceType;

// 救护车三态机流转枚举
typedef enum {
    AMB_IDLE = 0,          // 空闲待命
    AMB_DISPATCHED = 1,    // 执行任务中
    AMB_MAINTENANCE = 2    // 维保消毒中
} AmbStatus;

/* ==========================================
 * 3. 核心数据结构 (全单向链表架构，严格遵守题签说明1)
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
    // 【题签硬性要求满足】：医生级别限定为主任医师、副主任医师、主治医师、住院医师
    char level[32];           
    char dept_id[MAX_ID_LEN]; // 关联所属科室
    char duty_time[64];       // 出诊时间描述
    struct DoctorNode* next;
} DoctorNode;

// 患者节点 (统一融合门诊与住院对象)
typedef struct PatientNode {
    char id[MAX_ID_LEN];
    char name[MAX_NAME_LEN];
    int age;
    char gender[16];
    char type;                // 身份标识：'O' 门诊, 'I' 住院
    InsuranceType insurance;  
    double account_balance;   // 资金账户余额 (用于防吞钱原路退回)
    
    // 【题签硬性要求满足】：必须分别记录开始日期和预计出院日期
    char admission_date[32];            // 办理住院日期 (YYYY-MM-DD HH:MM)
    char expected_discharge_date[32];   // 预计出院日期 (由医生在入院时录入)
    
    char bed_id[MAX_ID_LEN];  // 关联的床位号
    double hospital_deposit;  // 住院预缴押金 (合规线：须为100倍数且 >= 200*天数)
    struct PatientNode* next;
} PatientNode;

// 医疗记录节点 (全院财务、单据与生命周期的核心总线)
typedef struct RecordNode {
    char record_id[MAX_ID_LEN]; // 复合唯一单号 (YYYYMMDD-排号-医生工号)
    char patient_id[MAX_ID_LEN];
    char doctor_id[MAX_ID_LEN];
    RecordType type;
    char date[32];            // 记录生成日期
    int queue_number;         // 挂号排队序位
    
    // 状态机：0-待处理/待缴费, 1-已完成/已缴费, -1-已撤销(财务冲红软删除) 
    int status;               
    
    char description[MAX_DESC_LEN]; // 电子处方协议、检查详情或防吞钱冲红留痕
    
    // 财务审计字段 (双精度浮点数，精确到分，最大不超过10万)
    double total_fee;         // 账单总金额
    double insurance_covered; // 医保统筹垫付额
    double personal_paid;     // 患者个人实付额 (撤销时退回的基准)
    
    struct RecordNode* next;
} RecordNode;

// 药品节点
typedef struct MedicineNode {
    char id[MAX_ID_LEN];
    char common_name[MAX_MED_NAME_LEN];
    char trade_name[MAX_MED_NAME_LEN];
    int stock;                // 实体库存 (发药时进行物理扣减)
    double price;             // 单价
    struct MedicineNode* next;
} MedicineNode;

// 床位节点
typedef struct BedNode {
    char bed_id[MAX_ID_LEN];
    char dept_id[MAX_ID_LEN]; // 所属科室
    WardType type;            // 病房/病床类型
    double daily_fee;         // 每日床位费 (08:00 定时扣费基准)
    int is_occupied;          // 状态锁：0:空闲, 1:占用
    char patient_id[MAX_ID_LEN]; // 被哪个患者占用
    struct BedNode* next;
} BedNode;

// 120 救护车实时状态节点
typedef struct AmbulanceNode {
    char vehicle_id[MAX_ID_LEN]; // 车牌或车辆编号
    char driver_name[MAX_NAME_LEN];
    AmbStatus status;
    char current_location[MAX_ADDR_LEN]; // GPS 实时位置更新
    struct AmbulanceNode* next;
} AmbulanceNode;

/* ==========================================
 * 【核心架构升级】救护车出勤日志与手术室节点定义
 * ========================================== */

// 定义兼容性互斥锁，告知所有包含本头文件的 .c 文件：节点已在全局基石中定义完成，无需重新定义！
#define HIS_EXT_NODES_DEFINED 1

/**
 * @brief 救护车历史出勤日志节点
 * 满足行政审计要求，将救护车的每一次出勤进行时间戳打点并归档。
 */
typedef struct AmbulanceLogNode {
    char log_id[32];            // 调度流水号 (基于精确时间戳生成)
    char vehicle_id[32];        // 执行本次出勤任务的车辆
    char dispatch_time[32];     // 派发时间 (精确到秒：YYYY-MM-DD HH:MM:SS)
    char destination[128];      // 急救目标现场坐标
    struct AmbulanceLogNode* next; 
} AmbulanceLogNode;

/**
 * @brief 手术室并发控制节点
 * 用于管理全院外科手术室的物理排班，内置防撞并发锁。
 */
typedef struct OperatingRoomNode {
    char room_id[32];           // 实体房间号 (如 OR-01)
    int status;                 // 三态机 - 0: 空闲可用, 1: 手术进行中, 2: 清场消毒中
    char current_patient[32];   // 占用该房间的患者
    char current_doctor[32];    // 执行手术的主刀医生
    struct OperatingRoomNode* next; 
} OperatingRoomNode;


/* ==========================================
 * 4. 全局链表头指针 (外部 External 声明)
 * ========================================== */
extern DepartmentNode* dept_head;
extern DoctorNode* doctor_head;
extern MedicineNode* medicine_head;
extern PatientNode* outpatient_head; // 门诊患者大名单
extern PatientNode* inpatient_head;  // 住院患者大名单 (门诊转住院时涉及内存地址搬家)
extern BedNode* bed_head;
extern RecordNode* record_head;      // 全院诊疗/财务总线
extern AmbulanceNode* ambulance_head; 

// 【绝对要求响应：新增核心数据源全局声明】
extern AmbulanceLogNode* ambulance_log_head; // 救护车历史出勤表全局头指针
extern OperatingRoomNode* or_head;           // 全院手术室实时资源全局头指针

#endif // COMMON_H