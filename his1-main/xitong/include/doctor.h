#ifndef DOCTOR_H
#define DOCTOR_H

#include "common.h"

/* ==========================================
 * 医护端 (Doctor Subsystem) 业务逻辑接口
 * 适配 HIS v3.0：含急诊优先、分级处方与重症病房管理
 * ========================================== */

// --- 1. 系统核心入口与鉴权 ---

/**
 * 医护子系统主循环入口
 * 处理医生登录鉴权及工作台状态显示
 */
void doctor_subsystem();

/**
 * 验证处方记录的时效性
 * 协议解析逻辑：PRES|类型|天数|药品ID|数量|日期
 * @return 1-有效, 0-已过期
 */
int is_prescription_valid(const char* desc);


// --- 2. 门诊诊疗模块 (Outpatient Station) ---

/**
 * 查看当前医生的候诊队列
 * 按照挂号顺序排列，并高亮标识“急诊”患者 [cite: 24]
 */
void view_my_queue(DoctorNode* doctor);

/**
 * 呼叫下一位患者就诊
 * 核心逻辑：强制优先呼叫 RECORD_EMERGENCY (急诊) 类型患者 
 * @return 成功接诊的患者节点指针
 */
PatientNode* call_next_patient(DoctorNode* doctor);

/**
 * 开具分类电子处方
 * 支持：普通处方(7天)、急性处方(3天)、慢性病处方(30天) 
 * 包含库存校验逻辑
 */
void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient);

/**
 * 开具检查单 (RECORD_EXAMINATION)
 * 记录单项检查费用及总费用 
 */
void order_examination(DoctorNode* doctor, PatientNode* patient);


// --- 3. 药房发药与核销模块 (Pharmacy) ---

/**
 * 药房发药核销柜台
 * 严格执行三重校验：是否存在、是否已缴费、是否在有效期内 
 */
void dispense_medicine_service();


// --- 4. 住院部管理模块 (Inpatient Station) ---

/**
 * 查看本科室在院患者名单
 * 展示床位号、患者姓名及护理级别 (ICU/普通) [cite: 27]
 */
void view_department_inpatients(DoctorNode* doctor);

/**
 * 办理住院申请
 * 校验押金：100整数倍，且不低于 200*N 元 
 */
void admission_service(DoctorNode* doctor, PatientNode* patient);

/**
 * 患者病房转换与转科申请
 * 支持 7 类病房切换：ICU、单人、双人、三人、陪护、疗养房 [cite: 63, 64]
 */
void transfer_patient_to_icu(DoctorNode* doctor);

/**
 * 办理出院结算
 * 逻辑：00:00-08:00不计当天费；自动从押金中扣除费用 
 */
void discharge_patient(DoctorNode* doctor);


// --- 5. 个人业务统计与查询 ---

/**
 * 医生工作量实时统计
 * 展示今日接诊人次 (含急诊/门诊) 及累计营收 
 */
void view_doctor_statistics(DoctorNode* doctor);

/**
 * 药品库存检索
 * 允许医生在开方前查询特定药品的余量与单价 [cite: 12]
 */
void query_medicine_inventory();

#endif // DOCTOR_H