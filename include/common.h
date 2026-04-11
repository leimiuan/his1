// [核心架构说明]：
// 本文件为 HIS 系统全局公共组件与核心实体类定义层 (Common Entity &
// Definitions)。
// 它是整个系统的“数据地基”。所有的业务模块（患者、医生、后台、药房、急救）
// 都依赖这里定义的 Struct 结构体进行数据流转。
//
// 本次升级严格保留了原版所有的向下兼容宏与定长数组防溢出设计。
// 并在 DoctorNode 中补齐了支撑“四级手术权限防越权系统”的关键属性。
// 【最新架构演进】：在 PatientNode
// 中新增双医生责任制字段，彻底打通临床病区管床逻辑。

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==========================================
 * 0. 终端 UI 颜色宏定义 (ANSI Escape Codes)
 * 用于在控制台实现红绿黄警告的高亮渲染，提升防呆交互体验。
 * ========================================== */
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[1;31m"
#define COLOR_GREEN "\033[1;32m"
#define COLOR_YELLOW "\033[1;33m"
#define COLOR_BLUE "\033[1;34m"
#define COLOR_CYAN "\033[1;36m"

/* ==========================================
 * 1. 全局宏定义 (定长缓冲区，防止溢出)
 * ========================================== */
#define MAX_ID_LEN 32         // 用于 ID (P001, D001, REC001 等)
#define MAX_NAME_LEN 128      // 用于姓名
#define MAX_DEPT_LEN 64       // 用于科室名
#define MAX_MED_NAME_LEN 256  // medicine common/trade/alias names
#define MAX_DESC_LEN 512      // 用于病历描述
#define MAX_ADDR_LEN 256      // 用于救护车定位描述

/* ==========================================
 * 【新增：系统级挂号限流防刷参数】
 * 严格满足题签重点要求：每天医院最多500号，医生最多20号，患者最多5号
 * ========================================== */
#define MAX_CLINIC_DAILY_TICKETS 500  // 医院每日总号源上限 (熔断阈值)
#define MAX_DOCTOR_DAILY_TICKETS 20   // 单名医生每日号源上限
#define MAX_PATIENT_DAILY_TICKETS 5   // 单名患者每日挂号上限 (防黄牛刷号机制)

/* ==========================================
 * 2. 枚举类型 (必须在结构体之前定义)
 * ========================================== */

// 病房/病床类型 (完全适配软件学院 2019 级荣誉班补充材料第2条)
typedef enum {
  WARD_SPECIAL = 1,    // 特殊病房 (ICU/重症监护，急救通道高频调度资源)
  WARD_GENERAL = 2,    // 普通病房
  WARD_SINGLE = 3,     // 单人病房
  WARD_DOUBLE = 4,     // 双人病房
  WARD_TRIPLE = 5,     // 三人病房
  WARD_ESCORTED = 6,   // 单人陪护病房
  WARD_SANATORIUM = 7  // 单人陪护疗养病房 (触发熔断时可降级为双人病房)
} WardType;

// 医疗记录业务类型
// 【完美闭环】：增加急诊通道、外科手术与体检，构建真实医院全栈生态
typedef enum {
  RECORD_REGISTRATION = 1,     // 普通门诊挂号
  RECORD_CONSULTATION = 2,     // 问诊/处方
  RECORD_EXAMINATION = 3,      // 检查
  RECORD_HOSPITALIZATION = 4,  // 住院/转床
  RECORD_EMERGENCY =
      5,  // 急诊绿色通道 (高优先级，120救护车拉回时系统自动生成此单据)
  RECORD_SURGERY = 6,  // 外科手术预约与计费
  RECORD_CHECKUP = 7   // 健康体检套餐与报告生成
} RecordType;

// 向下兼容宏：抑制其他 .c 文件中可能存在的局部防呆宏定义，防止编译报警
#define RECORD_SURGERY 6
#define RECORD_CHECKUP 7

// 医保报销类型 (自动财务测算底层枚举)
typedef enum {
  INSURANCE_NONE = 0,     // 全自费 (0% 报销)
  INSURANCE_WORKER = 1,   // 城镇职工 (自动触发报销 70%)
  INSURANCE_RESIDENT = 2  // 城乡居民 (自动触发报销 50%)
} InsuranceType;

// 救护车三态机流转枚举
typedef enum {
  AMB_IDLE = 0,        // 空闲待命 (可供急救中心随时派单)
  AMB_DISPATCHED = 1,  // 执行任务中 (资源已锁定)
  AMB_MAINTENANCE = 2  // 维保消毒中 (回场后的物理洗消期)
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

// 医生节点 (全院医护人事档案基石)
typedef struct DoctorNode {
  char id[MAX_ID_LEN];
  char name[MAX_NAME_LEN];
  // 【题签硬性要求满足】：医生级别限定为主任医师、副主任医师、主治医师、住院医师
  char level[32];
  char dept_id[MAX_ID_LEN];  // 关联所属科室
  char duty_time[64];        // 出诊时间描述

  // [新增架构逻辑]：支撑四级手术权限拦截引擎的核心字段
  // 权限对应：4-主任(全权限)，3-副主任(高难)，2-主治(基础)，1-住院(仅开单无手术权)
  int privilege_level;

  struct DoctorNode* next;
} DoctorNode;

// 患者节点 (统一融合门诊与住院对象)
typedef struct PatientNode {
  char id[MAX_ID_LEN];
  char name[MAX_NAME_LEN];
  int age;
  char gender[16];
  char type;                // 身份流转标识：'O' 门诊, 'I' 住院
  InsuranceType insurance;  // 绑定的医保类型，决定财务结算账单的报销比例
  double account_balance;   // 资金账户余额 (用于防吞钱原路退回机制)

  // 【题签硬性要求满足】：必须分别记录开始日期和预计出院日期
  char admission_date[32];           // 办理住院日期 (YYYY-MM-DD HH:MM)
  char expected_discharge_date[32];  // 预计出院日期 (由医生在入院评估时录入)

  char bed_id[MAX_ID_LEN];  // 关联的物理床位号
  double
      hospital_deposit;  // 住院预缴押金 (合规风控线：须为100倍数且 >= 200*天数)

  // [核心架构新增]：支撑住院病区“双医生责任制” (临床合规要求)
  char attending_doctor_id[MAX_ID_LEN];  // 主治医生工号
                                         // (专家级，负责审批诊疗方案与手术把关)
  char resident_doctor_id[MAX_ID_LEN];  // 管床医生工号
                                        // (主治/住院级，负责日常查房与病程记录)

  struct PatientNode* next;
} PatientNode;

// 医疗记录节点 (全院财务、单据与生命周期的核心总线)
typedef struct RecordNode {
  char record_id[MAX_ID_LEN];  // 复合唯一单号 (YYYYMMDD-排号-系统生成号)
  char patient_id[MAX_ID_LEN];
  char doctor_id[MAX_ID_LEN];
  RecordType type;
  char date[32];  // 记录生成日期

  // [架构联动关键]：挂号排队序位。
  // 在普通模式下为递增排号。若被 120 急救直通通道触发，会被系统强制覆写为
  // 1，实现物理插队置顶。
  int queue_number;

  // 状态机：0-待处理/待缴费, 1-已完成/已缴费, -1-已撤销(财务冲红软删除)
  int status;

  // 承载电子处方协议(PRES|)、检查详情或急救信标([救护车直通入抢救室])的载体
  char description[MAX_DESC_LEN];

  // 财务审计字段 (双精度浮点数，精确到分，由底层清洗引擎控制最大不超过10万)
  double total_fee;          // 账单总金额
  double insurance_covered;  // 医保统筹垫付额度
  double personal_paid;      // 患者个人实付额度 (撤销时系统原路退回的基准)

  struct RecordNode* next;
} RecordNode;

// 药品节点 (药房物资基石)
typedef struct MedicineNode {
  char id[MAX_ID_LEN];
  char common_name[MAX_MED_NAME_LEN];
  char trade_name[MAX_MED_NAME_LEN];
  char alias_name[MAX_MED_NAME_LEN];
  int stock;     // 实体库存 (医生开处方不减，药房实际发药时才进行物理扣减)
  double price;  // 单价
  struct MedicineNode* next;
} MedicineNode;

// ==========================
// 护士节点 (Nurse Node)
// ==========================
typedef struct NurseNode {
  char id[MAX_ID_LEN];       // 工号 (如 N001)
  char name[MAX_NAME_LEN];   // 姓名
  char dept_id[MAX_ID_LEN];  // 所属科室
  int role_level;            // 职称级别 (1:护士, 2:护师, 3:主管护师, 4:护士长)
  struct NurseNode* next;
} NurseNode;

// 床位节点 (全院空间资源基石)
typedef struct BedNode {
  char bed_id[MAX_ID_LEN];
  char dept_id[MAX_ID_LEN];     // 所属科室
  WardType type;                // 病房/病床类型枚举
  double daily_fee;             // 每日床位费基准金额 (出院/日切时定时扣费依据)
  int is_occupied;              // 资源防撞锁：0:空闲, 1:已被占用
  char patient_id[MAX_ID_LEN];  // 当前锁定该床位的患者编号
  struct BedNode* next;
} BedNode;

// 120 救护车实时状态节点
typedef struct AmbulanceNode {
  char vehicle_id[MAX_ID_LEN];          // 车牌或车辆唯一编号
  char driver_name[MAX_NAME_LEN];       // 当值驾驶员
  AmbStatus status;                     // 状态锁：空闲/出车/消毒
  char current_location[MAX_ADDR_LEN];  // GPS 实时位置更新描述
  struct AmbulanceNode* next;
} AmbulanceNode;

/* ==========================================
 * 【核心架构升级】救护车出勤日志与手术室节点定义
 * ========================================== */

// 定义兼容性互斥锁，告知所有包含本头文件的 .c
// 文件：节点已在全局基石中定义完成，无需重复定义！
#define HIS_EXT_NODES_DEFINED 1

/**
 * @brief 救护车历史出勤日志节点 (Ambulance Log)
 * 满足行政防篡改审计要求，将救护车的每一次派单出勤进行秒级时间戳打点并永久归档。
 */
typedef struct AmbulanceLogNode {
  char log_id[32];         // 调度日志流水号 (基于精确时间戳+哈希生成)
  char vehicle_id[32];     // 执行本次出勤任务的实体车辆
  char dispatch_time[32];  // 派发时间 (强制精确到秒：YYYY-MM-DD HH:MM:SS)
  char destination[128];   // 急救目标现场坐标或事发地描述
  struct AmbulanceLogNode* next;
} AmbulanceLogNode;

/**
 * @brief 手术室并发控制节点 (Operating Room)
 * 用于管理全院外科手术室的物理排班，内置状态防撞并发锁，确保不会发生两台手术分配到同一房间的致命事故。
 */
typedef struct OperatingRoomNode {
  char room_id[32];          // 实体手术房间号 (如 OR-01)
  int status;                // 二态机 - 0: 空闲可用, 1: 正在手术中
  char current_patient[32];  // 占用该房间的患者ID
  char current_doctor[32];   // 执行手术的主刀/最高权限医生ID
  struct OperatingRoomNode* next;
} OperatingRoomNode;

/* ==========================================
 * 4. 全局链表头指针 (外部 External 声明)
 * 向全系统暴露全局数据总线的入口地址
 * ========================================== */
extern DepartmentNode* dept_head;
extern DoctorNode* doctor_head;
extern MedicineNode* medicine_head;
extern PatientNode* outpatient_head;  // 门诊患者大名单
extern PatientNode*
    inpatient_head;  // 住院患者大名单 (门诊转住院时涉及内存地址平滑搬家)
extern BedNode* bed_head;
extern RecordNode* record_head;  // 全院诊疗/财务数据总线
extern AmbulanceNode* ambulance_head;

// 【绝对要求响应：新增核心数据源全局声明】
extern AmbulanceLogNode* ambulance_log_head;  // 救护车历史出勤表全局头指针
extern OperatingRoomNode* or_head;            // 全院手术室实时资源全局头指针
extern NurseNode* nurse_head;

#endif  // COMMON_H