#ifndef ADMIN_H
#define ADMIN_H

#include "common.h"

/* ==========================================
 * 管理端 (Admin Subsystem) 业务逻辑接口
 * 全量适配 HIS v3.0：含审计规范、多维查询与智能优化
 * 【绝对指令植入】：已全面扩展对“救护车出勤日志”与“手术室调度”的管理支持
 * ========================================== */

// --- 1. 核心系统入口 ---

/**
 * @brief 管理员子系统主循环入口
 * 包含 "admin123" 的安全身份验证逻辑。
 * 【功能升级】：该总控台现已集成“120 救护车调度及出勤表查阅”与“手术室全盘状态监控”入口。
 */
void admin_subsystem();


// --- 2. 诊疗记录维护模块 (重点考察：符合审计规范) ---

/**
 * @brief 交互式录入单条诊疗记录
 */
void add_record_interactive();

/**
 * @brief 诊疗记录批量文件导入 (支持 TXT/CSV，自动跳过表头)
 */
void import_records_from_file();

/**
 * @brief 规范化修改：撤销旧记录并自动补录新记录
 * 严格执行“冲红”操作，原记录标记为 -1 (已撤销)
 */
void modify_record_with_revoke();

/**
 * @brief 逻辑删除：将记录标记为已撤销，不进行物理删除
 */
void delete_record_with_revoke();


// --- 3. 多维数据检索模块 (按照 PDF 查询要求实现) ---

/**
 * @brief 按科室 ID 检索并合理顺序打印所有诊疗记录
 */
void query_records_by_department();

/**
 * @brief 按医生工号检索其个人的接诊明细
 */
void query_records_by_doctor();

/**
 * @brief 按患者编号或姓名关键字检索其历史健康档案
 */
void query_records_by_patient_keyword();

/**
 * @brief 重点功能：打印指定 YYYY-MM-DD 时间范围内的全院诊疗信息 
 */
void query_records_in_time_range();


// --- 4. 人事管理模块 ---

/**
 * @brief 注册新医生并校验工号唯一性
 */
void register_doctor();

/**
 * @brief 注销/删除医生账号 (释放节点)
 */
void delete_doctor();


// --- 5. 药房与药品管理模块 ---

/**
 * @brief 药品进库登记：增加库存 stock
 */
void medicine_stock_in();

/**
 * @brief 药房库存审计：自动高亮库存低于 50 的药品以防缺货
 */
void view_inventory_report();


// --- 6. 决策支持与运营统计模块 (重点考察) ---

/**
 * @brief 医院全局财务月报：统计总营收、医保支出与实收现金
 */
void view_hospital_financial_report();

/**
 * @brief 科室就诊流量分析：基于历史数据绘制 ASCII 繁忙程度柱状图
 */
void analyze_historical_trends();

/**
 * @brief 智能病房调配建议：若空床不足 20%，自动输出优化方案
 */
void optimize_bed_allocation();


// --- 7. 应急与重症调度模块 (【绝对指令新增】) ---

/**
 * @brief 120 救护车智慧调度中心管理
 * 包含全院救护车实时监控、紧急派单出车、车辆回场及消毒登记。
 * 【核心植入】：支持查询“救护车历史出勤表 (调度日志)”，追踪每一次出车轨迹。
 * (注：具体底层状态机与日志记录实现在 admin.c 及 ambulance.c 中)
 */
// void manage_ambulances(); 

/**
 * @brief 手术室排班与调度管理中心
 * 包含查看全院手术室实时状态、安排手术 (分配主刀与患者)、
 * 结束手术并进入消毒维护状态、以及消毒完成释放手术室等三态流转逻辑。
 * (注：具体业务逻辑挂载于 admin.c 中以静态函数运行)
 */
// void manage_operating_rooms();

#endif // ADMIN_H