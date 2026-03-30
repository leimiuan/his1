#ifndef AMBULANCE_H
#define AMBULANCE_H

#include "common.h"

/* ==========================================
 * 120 救护车智慧调度与急救联动子系统
 * 【绝对指令响应】：本模块已全面扩展支持“救护车历史出勤表”与“手术室联动查询”功能。
 * ========================================== */

/**
 * @brief 120 救护车智慧调度子系统的主入口 (原 manage_ambulances 升级而来)
 * 提供交互式的菜单，用于对医院的救护车队进行实时监控、紧急派发和回场管理。
 * 现已无缝集成“出勤日志追踪”与“手术室空闲状态实时联动查询”核心功能。
 */
void ambulance_subsystem();

/**
 * @brief 生成并记录一条救护车出勤日志 (底层数据接口)
 * @param vehicle_id 派发执行任务的车辆编号
 * @param destination 救护车前往的目的地或坐标
 * * 核心要求实现：用于在急救中心或患者端派发救护车时自动调用，
 * 从而在底层形成完整、可追溯的救护车历史出勤表。
 */
void add_ambulance_log(const char* vehicle_id, const char* destination);

#endif // AMBULANCE_H