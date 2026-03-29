#ifndef ADMIN_H
#define ADMIN_H

#include "common.h"

/* ==========================================
 * 管理端 (Admin Subsystem) 业务逻辑接口
 * 全量适配 HIS v3.0：含审计规范、多维查询与智能优化
 * ========================================== */

// --- 1. 核心系统入口 ---

/**
 * 管理员子系统主循环入口
 * 包含 "admin123" 的安全身份验证逻辑
 */
void admin_subsystem();


// --- 2. 诊疗记录维护模块 (重点考察：符合审计规范) ---

/**
 * 交互式录入单条诊疗记录
 */
void add_record_interactive();

/**
 * 诊疗记录批量文件导入 (支持 TXT/CSV，自动跳过表头)
 */
void import_records_from_file();

/**
 * 规范化修改：撤销旧记录并自动补录新记录
 * 严格执行“冲红”操作，原记录标记为 -1 (已撤销)
 */
void modify_record_with_revoke();

/**
 * 逻辑删除：将记录标记为已撤销，不进行物理删除
 */
void delete_record_with_revoke();


// --- 3. 多维数据检索模块 (按照 PDF 查询要求实现) ---

/**
 * 按科室 ID 检索并合理顺序打印所有诊疗记录
 */
void query_records_by_department();

/**
 * 按医生工号检索其个人的接诊明细
 */
void query_records_by_doctor();

/**
 * 按患者编号或姓名关键字检索其历史健康档案
 */
void query_records_by_patient_keyword();

/**
 * 重点功能：打印指定 YYYY-MM-DD 时间范围内的全院诊疗信息 
 */
void query_records_in_time_range();


// --- 4. 人事管理模块 ---

/**
 * 注册新医生并校验工号唯一性
 */
void register_doctor();

/**
 * 注销/删除医生账号 (释放节点)
 */
void delete_doctor();


// --- 5. 药房与药品管理模块 ---

/**
 * 药品进库登记：增加库存 stock
 */
void medicine_stock_in();

/**
 * 药房库存审计：自动高亮库存低于 50 的药品以防缺货
 */
void view_inventory_report();


// --- 6. 决策支持与运营统计模块 (重点考察) ---

/**
 * 医院全局财务月报：统计总营收、医保支出与实收现金
 */
void view_hospital_financial_report();

/**
 * 科室就诊流量分析：基于历史数据绘制 ASCII 繁忙程度柱状图
 */
void analyze_historical_trends();

/**
 * 智能病房调配建议：若空床不足 20%，自动输出优化方案
 */
void optimize_bed_allocation();

#endif // ADMIN_H