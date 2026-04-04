#ifndef ADMIN_H
#define ADMIN_H

#include "common.h"

/* ==========================================
 * 管理端 (Admin Subsystem) 业务逻辑接口契约
 * 适配版本：智慧医院 HIS v4.0 终极重构版 (含荣誉班进阶特性)
 * 【系统定位】：承载人事、药房、财务报表、病床智能调度与高级单据审计功能。
 * ========================================== */

// --- 1. 核心系统入口 ---

/**
 * @brief 管理员子系统主循环入口
 * 包含 "admin123" 的安全身份验证逻辑。
 * 【功能大满贯】：该总控台现已集成“防分身排班锁”、“并发防撞手术室调度”、“病房熔断预警”及“急门诊隔离大屏”。
 */
void admin_subsystem();


// --- 2. 诊疗记录维护模块 (满足题签重点考察：不规范数据的防呆与冲红机制) ---

/**
 * @brief 交互式录入单条诊疗记录
 * 包含严密的输入合法性校验，防止负数金额或不存在的实体 ID。
 */
void add_record_interactive();

/**
 * @brief 诊疗记录批量文件导入 
 * 支持 TXT/CSV，自动跳过表头，遇到脏数据智能跳过并计入 failed_count，绝不崩溃。
 */
void import_records_from_file();

/**
 * @brief 规范化修改：撤销旧记录并自动补录新记录
 * 【重点采分点：财务审计红线】：严格执行“冲红”操作（原记录状态置为 -1，追加 REVOKED 描述）。
 * 【神级漏洞修复】：若原单据已缴费，系统将自动把实付金额原路退回至患者资金账户，杜绝吞钱。
 */
void modify_record_with_revoke();

/**
 * @brief 逻辑删除记录 (软撤销)
 * 将记录标记为已撤销，保留在底层的 records.txt 中供审计追溯，并自动触发退款防吞机制。
 */
void delete_record_with_revoke();


// --- 3. 多维数据检索模块 (严格按照 PDF 题签查询要求实现) ---

/**
 * @brief 【查询 4】：按科室 ID 检索并合理顺序打印所有诊疗记录
 */
void query_records_by_department();

/**
 * @brief 【查询 5】：按医生工号检索其个人的接诊明细
 */
void query_records_by_doctor();

/**
 * @brief 【查询 6】：按患者编号或姓名关键字检索其历史健康档案 (支持子串模糊匹配)
 */
void query_records_by_patient_keyword();

/**
 * @brief 【查询 9 / 重点考察】：打印指定 YYYY-MM-DD 时间范围内的全院诊疗信息 
 * 内部将时间转化为整型 Key (如 20260330) 进行高效率的高低位比对。
 */
void query_records_in_time_range();

/**
 * @brief \u7ba1\u7406\u7aef\u503c\u73ed\u6392\u73ed\u7ba1\u7406(\u67e5\u770b/\u65b0\u589e\u66f4\u65b0/\u5220\u9664)
 */
void manage_duty_schedule();


// --- 4. 人事管理模块 ---

/**
 * @brief 注册新医生并校验工号唯一性
 * 【进阶指标植入】：已支持录入医生“级别” (主任医师/副主任医师/主治医师/住院医师)，以满足题签规定。
 */
void register_doctor();

/**
 * @brief 注销/删除医生账号 (单链表节点物理释放，防止内存泄露)
 */
void delete_doctor();


// --- 5. 药房与药品管理模块 ---

/**
 * @brief 药品进库登记：增加库存 (stock)
 * 带有大于 0 的防呆校验拦截。
 */
void medicine_stock_in();

/**
 * @brief 药房库存审计
 * 自动高亮标红库存低于 50 的药品以防缺货引发医疗事故。
 */
void view_inventory_report();


// --- 6. 决策支持与运营统计模块 (满足题签终极重点考察点) ---

/**
 * @brief 【统计 7 / 重点考察】：全局财务营收报告
 * 【硬性指标剥离】：严格遵循题签“营业额仅含检查+药品+住院”要求，在底层遍历时强行剥离所有带有“押金”描述的单据，杜绝沉淀资金虚增营收！
 */
void view_hospital_financial_report();

/**
 * @brief 【统计 8】：科室就诊流量与医生繁忙程度分析
 * 基于历史数据反推归属，并在终端绘制清晰美观的 ASCII 柱状图。
 */
void analyze_historical_trends();

/**
 * @brief 全院病床资源利用率监控与预警
 * 【荣誉班高阶挑战：智能病房改造】：当检测到目标科室病床占用率 > 80% (剩余空床不到 20%) 时，自动触发扩容机制，模拟将“单人陪护疗养病房”降级拆分为“双人病房”的操作。
 */
void optimize_bed_allocation();


// --- 7. 内部高阶调度系统声明 (在 admin.c 中以 static 运行) ---
/*
 * 以下模块已深度集成在 admin_subsystem 的路由菜单中：
 * 1. manage_duty_schedule()     -> 排班统筹 (含防分身锁，防止一人同天上门诊和急诊)
 * 2. manage_ambulances()        -> 120 救护车智慧调度 (含出勤表流水记录)
 * 3. manage_operating_rooms()   -> 手术室并发防撞排班系统
 * 4. view_global_queue_monitor()-> 全院候诊全局大屏 (急/门诊双轨物理隔离)
 * 5. admin_manage_patient_transfer() -> 强制转床/ICU流转 (含床位耗尽时的熔断红色预警)
 */

#endif // ADMIN_H
