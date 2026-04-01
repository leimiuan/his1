#ifndef ADMIN_H
#define ADMIN_H

#include "common.h"

/* ==========================================
 * 管理端 (Admin Subsystem) 业务逻辑接口
 * ========================================== */

// --- 1. 核心入口 ---
// 管理员主菜单，包含密码验证 (默认: admin123)
void admin_subsystem();


// --- 2. 人事与基础资源管理模块 (已更新) ---

// 注册新医生账号并分配科室
void register_doctor();

// 注销/删除医生账号
void delete_doctor();

// 维护科室信息 (支持增删改查) 
void manage_departments();

// 全局床位统筹管理 (维护一般房、高级房、ICU的三级体系) 
void manage_beds();


// --- 3. 药房与药品管理模块 ---

// 药品入库管理 (增加现有药品的 stock 库存) 
void medicine_stock_in();

// 药房库存盘点报表：自动识别并高亮库存低于 50 的药品 
void view_inventory_report();


// --- 4. 全局统计与报表模块 ---

// 财务与运营报表：汇总记录中的总收入、医保支出及个人自付 (仅统计已缴费记录)
void view_hospital_financial_report();


// --- 5. 试验班专属：数据分析与智能预测 ---

// 历史数据趋势分析：基于医疗记录绘制各科室流量的 ASCII 柱状图
void analyze_historical_trends();

// 智能病房分配优化建议：计算各科室床位利用率，输出调配建议
void optimize_bed_allocation();

#endif // ADMIN_H