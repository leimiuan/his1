#ifndef PATIENT_H
#define PATIENT_H

#include "common.h"

/* ==========================================
 * 患者端 (Patient Subsystem) 业务逻辑接口
 * 目标：实现患者视角的自助查询、排队监控与财务结算
 * 【绝对指令植入】：本模块已全面接入“手术室状态监控”与“救护车出勤日志记录”的底层联动
 * ========================================== */

// --- 1. 核心入口与鉴权 ---

// 患者端主菜单入口 (由 main.c 调用)
void patient_subsystem();

// 模拟患者登录：在 outpatient_head 和 inpatient_head 中交叉验证 ID
PatientNode* patient_login(const char* input_id);

// 新患者建档 (注册)
void register_new_patient();

// 患者个人工作台 (二级主菜单)
void patient_dashboard(PatientNode* me);


// --- 2. 资金与缴费模块 (核心业务) ---

// 账户在线充值
void patient_top_up(PatientNode* me);

// 待缴费单据查询与自主结算核销
void patient_pay_bill(PatientNode* me);


// --- 3. 基础信息与记录查询 ---

// 打印个人档案：包含医保类型、账户余额(门诊)或押金与床位(住院)
// 【功能升级】：已联动底层手术室系统，若患者正在手术室抢救则直接红色高亮警示
void view_personal_info(PatientNode* patient);

// 调取病历档案：按时间倒序打印该患者 ID 关联的所有 RecordNode
void query_medical_records(const char* patient_id);


// --- 4. 进阶功能与调度服务 ---

// 实时排队监控：定位该患者 status=0 的挂号记录，统计同医生前方人数
void check_queue_status(PatientNode* patient);

// 电子发票系统：在控制台渲染 ASCII 艺术票据，并同步写入本地
void generate_electronic_invoice(PatientNode* patient, RecordNode* record);

// 【新增功能】患者自助挂号 (解决隐式声明报错)
void patient_self_register(PatientNode* me);

// 【新增功能】患者一键呼叫 120 救护车 (解决隐式声明报错)
// 【功能升级】：成功派车后，底层将静默触发，自动将出车轨迹写入“救护车历史出勤表”
void call_ambulance_service(PatientNode* me);


// --- 5. 患者专属报表 ---

// 个人年度/单次财务汇总 (仅统计已缴费 status == 1 的记录)
void view_patient_financial_report(const char* patient_id);

#endif // PATIENT_H