// [核心架构说明]：
// 本文件为 HIS 系统“患者端 (Patient Subsystem)”的接口契约头文件。
// 对外暴露了患者建档、挂号、缴费、查病历、呼叫 120 救护车等面向 C 端用户的业务 API。
// 本次更新严格保留了所有的原始声明与注释，并针对新增的【120急救绿色通道联动】
// 与【排队置顶特权高亮】机制，补全了底层的架构设计说明。

#ifndef PATIENT_H
#define PATIENT_H

#include "common.h"

/* ==========================================
 * 患者端 (Patient Subsystem) 业务逻辑接口契约
 * 适配版本：智慧医院 HIS v4.0 终极重构版 (含荣誉班进阶特性)
 * 目标：实现患者视角的自助查询、排队监控与财务结算
 * 【系统定位】：承载全院最严格的挂号防刷限流防线，以及极具人性化的 AI 智能医疗推荐引擎。
 * ========================================== */

// --- 1. 核心入口与鉴权 ---

/**
 * @brief 患者端主菜单入口 (由 main.c 路由调用)
 */
void patient_subsystem();

/**
 * @brief 模拟患者登录
 * 在 outpatient_head 和 inpatient_head 两个底层大名单中进行交叉身份验证。
 */
PatientNode* patient_login(const char* input_id);

/**
 * @brief 新患者在线建档 (注册)
 * 自动分配全局唯一 ID 并执行底层头插法物理挂载。
 */
void register_new_patient();

/**
 * @brief 患者个人工作台 (Dashboard 二级主菜单)
 * 整合患者在院期间所有的自助服务与调度入口。
 */
void patient_dashboard(PatientNode* me);


// --- 2. 资金与缴费模块 (核心财务业务) ---

/**
 * @brief 账户在线充值
 * 带有严密的防呆校验，防止充值负数金额引发财务漏洞。
 */
void patient_top_up(PatientNode* me);

/**
 * @brief 待缴费单据查询与自主结算核销
 * 严格验证余额，执行物理扣款并扭转单据 status 为 1 (已缴费)。
 */
void patient_pay_bill(PatientNode* me);


// --- 3. 基础信息与记录查询 ---

/**
 * @brief 打印个人档案与健康状态
 * 包含医保类型、门诊余额、住院押金与床位号等。
 * 【神级联动】：已底层打通手术室并发系统，若患者正在手术室抢救（status==1），大屏直接红色高亮警示！
 */
void view_personal_info(PatientNode* patient);

/**
 * @brief 调取病历档案
 * 按时间线打印该患者关联的所有历史诊疗 RecordNode (单据号、项目、结果等)。
 */
void query_medical_records(const char* patient_id);


// --- 4. 进阶功能与 AI 调度服务 (重构核心亮点) ---

/**
 * @brief 实时排队监控与等待时间预估
 * 动态定位该患者 status=0 的挂号记录，自动统计同科室/同医生前方等待人数并给出科学的预估耗时。
 * [新增架构逻辑]：完美联动急救置顶系统。若患者底层的 queue_number 被覆写为 1，将直接在屏幕打出【绿色通道】特权高亮提示！
 */
void check_queue_status(PatientNode* patient);

/**
 * @brief 电子发票系统
 * 提取实付金额 (personal_paid)，在终端控制台渲染包含医院抬头的 ASCII 艺术电子票据。
 */
void generate_electronic_invoice(PatientNode* patient, RecordNode* record);

/**
 * @brief 【重点考察】患者自助挂号与急诊通道
 * 【防刷限流引擎】：严格执行全院500号/天、单医生20号/天、单患者5号/天、同科室1号/天的物理红线拦截。
 * 【AI 智能推荐】：若目标医生停诊或号满，系统触发容灾预案，自动检索并向患者推荐同科室下今日仍有号源的其他名医。
 */
void patient_self_register(PatientNode* me);

/**
 * @brief 患者一键呼叫 120 救护车 (院前急救联动)
 * 扫描全院车辆，锁定空闲救护车并更新 GPS 坐标。
 * 【行政审计落实】：成功派车后，底层静默调用 add_ambulance_log 将出车轨迹永久写入“救护车历史出勤表”。
 * [新增架构逻辑]：完成 120 急救闭环！派发救护车成功后，向患者同步推送直通急救室的联动提示语。
 */
void call_ambulance_service(PatientNode* me);


// --- 5. 患者专属报表 ---

/**
 * @brief 个人财务对账汇总
 * 仅统计已缴费 (status == 1) 的记录，计算患者在医院的总自费支出。
 */
void view_patient_financial_report(const char* patient_id);

#endif // PATIENT_H