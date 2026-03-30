#ifndef DOCTOR_H
#define DOCTOR_H

#include "common.h"

/* ==========================================
 * 医护端 (Doctor Subsystem) 业务逻辑接口契约
 * 适配版本：智慧医院 HIS v4.0 终极重构版 (含荣誉班进阶特性)
 * 【系统定位】：涵盖急门诊隔离叫号、处方核销、手术室防撞、体检报告生成及底层链表流转。
 * ========================================== */

// --- 1. 系统核心入口与协议校验 ---

/**
 * @brief 医护子系统主循环入口
 * 包含医生登录鉴权、动态执业场景侦测（自动识别今天排班是门诊还是急诊）。
 */
void doctor_subsystem();

/**
 * @brief 验证处方记录的时效性
 * 协议解析逻辑：PRES|类型|天数|药品ID|数量|日期
 * @return 1-有效/非处方, 0-处方已过期拦截
 */
int is_prescription_valid(const char* desc);


// --- 2. 临床接诊与物理隔离队列模块 ---

/**
 * @brief 查看当前医生的候诊队列
 * 【题签亮点】：根据医生当天的排班场景执行“物理隔离”。门诊医生仅看门诊号，急诊医生仅看急诊号（带红色置顶高亮）。
 */
void view_my_queue(DoctorNode* doctor);

/**
 * @brief 呼叫下一位患者就诊
 * 【核心逻辑】：强制优先呼叫 RECORD_EMERGENCY (急诊) 类型患者，实现急救绿色通道绝对优先权。
 * @return 成功接诊的患者节点指针，若无患者则返回 NULL
 */
PatientNode* call_next_patient(DoctorNode* doctor);


// --- 3. 处方开具与药房核销模块 ---

/**
 * @brief 开具分类电子处方
 * 支持：普通处方(7天)、急诊处方(3天)、慢病处方(30天)。自带底层物理库存预检逻辑。
 */
void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient);

/**
 * @brief 药房发药与处方物理核销柜台
 * 严格执行多重防呆校验：单据是否存在、是否已缴费、处方是否在有效期内、库存是否足够。
 */
void dispense_medicine_service();


// --- 4. 体检中心全商业闭环模块 (新增) ---

/**
 * @brief 开具体检/检查单 (RECORD_CHECKUP)
 * 写入特殊的待检协议标记，等待患者前往大厅缴费。
 */
void prescribe_checkup_service(DoctorNode* doctor, PatientNode* patient);

/**
 * @brief 录入体检结果并生成《健康体检报告单》
 * 【人性化输出】：医生录入检查指标后，系统自动提取患者信息，并在终端排版绘制一份精美的化验报告单，永久归档。
 */
void generate_checkup_report(DoctorNode* doctor);


// --- 5. 外科手术与住院流转模块 (核心底层重构) ---

/**
 * @brief 手术室一键预约与自动计费
 * 【神级防撞锁】：排期前强制进行全局并发扫描，如果主刀医生或患者正处于其他手术室的“施刀中”状态，立刻进行医疗事故预警拦截！
 */
void book_surgery_service(DoctorNode* doctor, PatientNode* patient);

/**
 * @brief 办理入院收治申请
 * 【财务底线校验】：住院押金必须为 100 的整数倍且 >= 200 * 拟住院天数。
 * 【荣誉班底层挑战】：在内存中执行“链表物理搬家”，将患者节点从门诊大名单物理解绑摘除，使用头插法重新挂载至住院大名单！
 */
void admission_service(DoctorNode* doctor, PatientNode* patient);

/**
 * @brief 住院患者 ICU 重症流转
 * 【床位熔断机制】：当 ICU 床位耗尽时，拒绝死板报错，而是拉响红色行政告警，提示管理端扩容临时床位或执行病房降级改造！
 */
void transfer_patient_to_icu(DoctorNode* doctor);


// --- 6. 个人业务统计与监控模块 ---

/**
 * @brief 医生工作量实时追踪
 * 展示今日接诊人次及个人累计为医院创造的营收贡献。
 */
void view_doctor_statistics(DoctorNode* doctor);

/**
 * @brief 查看本科室在院患者名单
 * 动态展示床位号、患者姓名，方便医生进行每日查房。
 */
void view_department_inpatients(DoctorNode* doctor);

#endif // DOCTOR_H