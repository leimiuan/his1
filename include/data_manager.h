// [核心架构说明]：
// 本文件为 HIS 系统核心数据访问层 (Data Access Layer) 的头文件。
// 它是整个医院底层数据的“大管家”，向 admin、doctor、patient 和 ambulance
// 四大子系统暴露所有的文件读写、内存链表查询以及核心流转的 API 接口。
// 本次更新已修复所有的乱码注释，并为急救闭环和权限拦截补齐了接口级说明。

#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "common.h"

/* ==========================================================================
 * 板块一：系统数据初始化与文件持久化 (File I/O & Serialization)
 * 【对标题签】：能够将当前系统中的所有信息保存到文件中。
 * 目标：实现 10 大核心单向链表与本地 txt 文件的双向无损同步。
 * ========================================================================== */

// 统一总控：程序启动时加载所有数据，退出时执行静默保存
void init_system_data();
void save_system_data();

// --- 救护车与排班文件底层保障 ---
void ensure_duty_schedule_file();

// --- 各模块反序列化加载函数 (从 txt 读取数据并使用头插法构建内存链表) ---
void load_departments(const char* filepath);
// [架构提醒]：load_doctors 内部已支持解析 privilege_level
// 字段，支撑防越级手术拦截
void load_doctors(const char* filepath);
void load_medicines(const char* filepath);
void load_outpatients(const char* filepath);
void load_inpatients(const char* filepath);
void load_beds(const char* filepath);
void load_records(const char* filepath);
void load_ambulances(const char* filepath);
void load_ambulance_logs(const char* filepath);   // 救护车出勤日志持久化加载
void load_operating_rooms(const char* filepath);  // 急诊手术室状态持久化加载
void load_nurses();                               // 护士工作站人事档案加载

// --- 各模块持久化保存函数 (遍历链表并按制表符 Tab 格式进行格式化落盘) ---
void save_departments(const char* filepath);
// [架构提醒]：save_doctors 内部已支持将 privilege_level 序列化至文件末尾
void save_doctors(const char* filepath);
void save_medicines(const char* filepath);
void save_outpatients(const char* filepath);
void save_inpatients(const char* filepath);
void save_beds(const char* filepath);
void save_records(const char* filepath);
void save_ambulances(const char* filepath);
void save_ambulance_logs(const char* filepath);   // 救护车出勤日志落盘保存
void save_operating_rooms(const char* filepath);  // 急诊手术室状态落盘保存
void save_nurses();                               // 护士工作站人事档案保存

/* ==========================================================================
 * 板块二：核心业务查询与操作接口 (CRUD & Workflow API)
 * 目标：屏蔽底层单向链表的遍历细节，向上层业务暴露高可靠的数据访问能力
 * ========================================================================== */

// --- 基础实体 ID 精准查询 (返回对应内存节点指针，查无则返回 NULL) ---
PatientNode* find_outpatient_by_id(const char* id);
PatientNode* find_inpatient_by_id(const char* id);
DoctorNode* find_doctor_by_id(const char* id);
DepartmentNode* find_department_by_id(const char* id);
MedicineNode* find_medicine_by_id(const char* id);
MedicineNode* find_medicine_by_key(const char* key);
BedNode* find_bed_by_id(const char* id);
AmbulanceNode* find_ambulance_by_id(const char* id);

// --- 复杂业务逻辑检索与操作 ---

// 模糊搜索：支持通过药品通用名或商品名进行关键字检索
void search_medicine_by_keyword(const char* keyword);

// 资源自动分配：查找指定科室及病房类型(如 WARD_SPECIAL 即 ICU)的空闲床位
BedNode* find_available_bed(const char* dept_id, WardType type);

/**
 * @brief 门诊转住院跨链表物理平移引擎
 * 门诊患者转住院时，将其从 outpatient_head 链表物理摘除，
 * 并使用头插法重新挂载至 inpatient_head 链表中，严守“全程链表”规范。
 * @return 1 成功，0 失败
 */
int transfer_outpatient_to_inpatient(const char* patient_id);

/**
 * @brief 核心流水记录生成引擎 (带急救插队支持)
 * 【对标题签】：需自行设计挂号的编码规则，包含日期、排号、医生等信息，保证唯一。
 * [架构新增]：通过暴露 queue_num 参数，允许上层业务(如救护车调度)强行传入 1，
 * 从而绕过常规排号逻辑，实现 Score 999 级别的物理插队。
 */
void add_new_record(const char* patient_id, const char* doctor_id,
                    RecordType type, const char* description, double fee,
                    int queue_num);

/* ==========================================================================
 * 板块三：医生出诊与值班排班管理 (Duty Schedule Management)
 * ========================================================================== */

// 查看指定医生或全院的值班安排表
void view_duty_schedule(const char* doctor_id_filter);

// 校验该医生今日是否有排班 (防挂号错乱)
int is_doctor_on_duty_today(const char* doctor_id);

// 校验该医生今日是否在特定岗位(门诊/急诊)排班 (防业务互串)
int is_doctor_on_duty_today_by_slot(const char* doctor_id, const char* slot);

// [新增声明] 校验任意日期该医生是否在特定岗位排班
int verify_doctor_shift(const char* doctor_id, const char* date,
                        const char* slot);

// 更新/新增排班记录 (日期 YYYY-MM-DD, 班次 门诊/急诊)
int upsert_duty_schedule(const char* date, const char* slot,
                         const char* dept_id, const char* doctor_id);

// 移除指定的排班记录
int remove_duty_schedule(const char* date, const char* slot,
                         const char* dept_id, const char* doctor_id);

/* ==========================================================================
 * 板块四：内存与生命周期管理 (Memory Lifecycle)
 * 目标：在程序安全退出时彻底清理堆内存，防止操作系统内存泄露 (Memory Leak)
 * ========================================================================== */

// 依次遍历 10 大全局单向链表，执行 free() 释放每一个节点
void free_all_data();

/* ==========================================================================
 * 板块五：急救与重症资源联动 (Ambulance & Emergency)
 * 目标：暴露救护车出勤日志与手术室状态流转的核心底层接口
 * ========================================================================== */

/**
 * @brief 生成并记录一条救护车出勤日志
 * 用于在调度中心或患者端派发救护车时静默调用，形成可追溯的历史出勤表。
 * @param vehicle_id 救护车车牌或编号
 * @param destination 目的地坐标或描述
 */
void add_ambulance_log(const char* vehicle_id, const char* destination);

#endif  // DATA_MANAGER_H