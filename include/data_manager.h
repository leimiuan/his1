#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "common.h"

/* ==========================================
 * 板块一：系统数据初始化与文件持久化 (File I/O)
 * 【对标题签 10】：【存储】能够将当前系统中的所有信息保存到文件中。
 * 目标：实现 10 大核心单向链表与本地 .txt 文件的双向无损同步
 * ========================================== */

// 统一总控：程序启动时加载所有数据，退出时执行静默保存
void init_system_data();
void save_system_data();

// --- 救护车与排班文件底层保障 ---
void ensure_duty_schedule_file();

// --- 各模块加载函数 (从 txt 读取数据并使用头插法构建内存链表) ---
void load_departments(const char* filepath);
void load_doctors(const char* filepath);
void load_medicines(const char* filepath);
void load_outpatients(const char* filepath);
void load_inpatients(const char* filepath);
void load_beds(const char* filepath);
void load_records(const char* filepath);
void load_ambulances(const char* filepath); 
void load_ambulance_logs(const char* filepath);  // 救护车出勤日志持久化加载
void load_operating_rooms(const char* filepath); // 【新增】：手术室状态持久化加载

// --- 各模块保存函数 (遍历链表并按制表符 Tab 格式进行格式化落盘) ---
void save_departments(const char* filepath);
void save_doctors(const char* filepath);
void save_medicines(const char* filepath);
void save_outpatients(const char* filepath);
void save_inpatients(const char* filepath);
void save_beds(const char* filepath);
void save_records(const char* filepath);
void save_ambulances(const char* filepath); 
void save_ambulance_logs(const char* filepath);  // 救护车出勤日志落盘保存
void save_operating_rooms(const char* filepath); // 【新增】：手术室状态落盘保存

/* ==========================================
 * 板块二：核心业务查询与操作接口 (CRUD)
 * 目标：屏蔽底层单向链表的遍历细节，向上层业务暴露高可靠的数据访问能力
 * ========================================== */

// --- 基础实体 ID 精准查询 (返回对应内存节点指针) ---
PatientNode* find_outpatient_by_id(const char* id);
PatientNode* find_inpatient_by_id(const char* id);
DoctorNode* find_doctor_by_id(const char* id);
DepartmentNode* find_department_by_id(const char* id);
MedicineNode* find_medicine_by_id(const char* id);
BedNode* find_bed_by_id(const char* id);
AmbulanceNode* find_ambulance_by_id(const char* id); 

// --- 复杂业务逻辑检索与操作 ---

// 模糊搜索：支持通过药品通用名或商品名进行关键词检索
void search_medicine_by_keyword(const char* keyword);

// 资源自动分配：查找指定科室及病房类型(如 WARD_SPECIAL 即 ICU)的空闲床位
BedNode* find_available_bed(const char* dept_id, WardType type);

/**
 * @brief 【荣誉班高阶底层挑战：链表级物理搬家】
 * 门诊患者转住院时，将其从 outpatient_head 链表物理摘除，
 * 并使用头插法重新挂载至 inpatient_head 链表中，严守“全程链表”规范。
 * @return 1 成功，0 失败
 */
int transfer_outpatient_to_inpatient(const char* patient_id);

/**
 * @brief 核心流水记录生成引擎
 * 【对标题签 4】：需自行设计挂号的编码规则，可隐含挂号日期、挂号当天的顺序号、对应的医生等信息，保证唯一。
 * 此函数内部已重构为生成 "YYYYMMDD-排号-医生工号" 格式的复合唯一单据号。
 */
void add_new_record(const char* patient_id, const char* doctor_id, RecordType type, 
                    const char* description, double fee, int queue_num);

/* ==========================================
 * 板块三：医生出诊与值班管理
 * ========================================== */

// 查看指定医生或全院的值班安排表
void view_duty_schedule(const char* doctor_id_filter);

// 更新/新增排班记录 (日期 YYYY-MM-DD, 班次 上午/下午/全天)
int upsert_duty_schedule(const char* date, const char* slot, const char* dept_id, const char* doctor_id);

// 移除指定的排班记录
int remove_duty_schedule(const char* date, const char* slot, const char* dept_id);

/* ==========================================
 * 板块四：内存与生命周期管理
 * 目标：在程序安全退出时彻底清理堆内存，防止操作系统内存泄露 (Memory Leak)
 * ========================================== */

// 依次遍历 10 大全局单向链表，执行 free() 释放每一个节点
void free_all_data();

/* ==========================================
 * 板块五：急救与重症资源联动 (【绝对指令新增】)
 * 目标：暴露救护车出勤日志与手术室状态流转的核心底层接口
 * ========================================== */

/**
 * @brief 生成并记录一条救护车出勤日志
 * 用于在调度中心或患者端派发救护车时静默调用，形成可追溯的历史出勤表。
 * @param vehicle_id 救护车编号
 * @param destination 目的地
 */
void add_ambulance_log(const char* vehicle_id, const char* destination);

#endif // DATA_MANAGER_H