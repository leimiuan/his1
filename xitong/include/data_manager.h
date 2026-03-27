#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "common.h"

/* ==========================================
 * 板块一：系统数据初始化与文件持久化 (File I/O)
 * 目标：实现内存链表与本地 .txt 文件的双向同步
 * ========================================== */

// 统一总控：程序启动时加载，退出时保存
void init_system_data();
void save_system_data();

// --- 各模块加载函数 (从 txt 读取并头插法构建链表) ---
void load_departments(const char* filepath);
void load_doctors(const char* filepath);
void load_medicines(const char* filepath);
void load_outpatients(const char* filepath);
void load_inpatients(const char* filepath);
void load_beds(const char* filepath);
void load_records(const char* filepath);

// --- 各模块保存函数 (遍历链表并按 Tab/空格 格式写入 txt) ---
void save_departments(const char* filepath);
void save_doctors(const char* filepath);
void save_medicines(const char* filepath);
void save_outpatients(const char* filepath);
void save_inpatients(const char* filepath);
void save_beds(const char* filepath);
void save_records(const char* filepath);

/* ==========================================
 * 板块二：核心业务查询与操作接口 (CRUD)
 * 目标：屏蔽链表遍历逻辑，直接返回节点指针
 * ========================================== */

// --- 基础 ID 查询接口 ---
PatientNode* find_outpatient_by_id(const char* id);
PatientNode* find_inpatient_by_id(const char* id);
DoctorNode* find_doctor_by_id(const char* id);
DepartmentNode* find_department_by_id(const char* id);
MedicineNode* find_medicine_by_id(const char* id);
BedNode* find_bed_by_id(const char* id);

// --- 复杂业务逻辑检索 ---

// 模糊搜索：匹配药品的通用名、商品名或别名，并打印列表
void search_medicine_by_keyword(const char* keyword);

// 自动分配：查找指定科室、指定病房类型且空闲(is_occupied=0)的床位
BedNode* find_available_bed(const char* dept_id, WardType type);

// 记录生成：创建新 RecordNode 并自动插入 record_head
void add_new_record(const char* patient_id, const char* doctor_id, RecordType type, 
                    const char* description, double fee, int queue_num);

// --- 值班排班表管理 ---
void view_duty_schedule(const char* doctor_id_filter);
int upsert_duty_schedule(const char* date, const char* slot, const char* dept_id, const char* doctor_id);
int remove_duty_schedule(const char* date, const char* slot, const char* dept_id);
/* ==========================================
 * 板块三：内存安全管理
 * 目标：遍历所有链表执行 free()，防止内存泄露
 * ========================================== */
void free_all_data();

#endif // DATA_MANAGER_H