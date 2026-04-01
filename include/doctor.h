#ifndef DOCTOR_H
#define DOCTOR_H

#include "common.h"

/* ==========================================
 * 医护端 (Doctor Subsystem) 业务逻辑接口
 * 目标：实现医务人员的诊疗流程与住院管理
 * ========================================== */

// --- 1. 核心入口与鉴权 ---

// 医护端主菜单入口 (由 main.c 调用)
void doctor_subsystem();

// 模拟医生登录：根据 ID 返回医生节点指针，未找到返回 NULL
DoctorNode* doctor_login(const char* input_id);


// --- 2. 门诊工作站模块 (Outpatient Station) ---

// 查看当前医生的候诊队列 (status == 0 的挂号记录)
void view_my_queue(DoctorNode* doctor);

// 呼叫下一位患者看诊 (核心业务：将挂号记录 status 改为 1)
PatientNode* call_next_patient(DoctorNode* doctor);

// 【已升级】为当前看诊的患者开具分类处方 (包含时效：普通/急性/慢性)
void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient);

// 为患者开具检查单 (如 CT、血检，生成 RECORD_EXAMINATION 记录)
void order_examination(DoctorNode* doctor, PatientNode* patient);


// --- 3. 药房联动核销模块 (Pharmacy) ---

// 【新增】药房发药柜台：根据处方单号校验时效与缴费状态，并扣减库存
void dispense_medicine_service();


// --- 4. 住院部查房与床位管理模块 (Inpatient Management) ---

// 查看本科室所有的住院患者 (筛选出关联本科室床位的患者)
void view_department_inpatients(DoctorNode* doctor);

// 住院患者床位分配与动态转床 (实现一般房、VIP、ICU 之间的转换) 
void transfer_ward(DoctorNode* doctor);

// 办理出院核算 (解除床位占用，完成财务结算)
void discharge_patient(DoctorNode* doctor);


// --- 5. 医护专属查询与报表 ---

// 基础信息查询：医生按关键字检索药品库存与单价
void query_medicine_inventory();

// 医护统计报表：展示该医生当天的接诊总量与创收总额
void view_doctor_statistics(DoctorNode* doctor);

#endif // DOCTOR_H