#include "doctor.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

/* ==========================================
 * 【兼容性宏】确保编译通过，对应新增的业务枚举
 * ========================================== */
#ifndef RECORD_SURGERY
#define RECORD_SURGERY 6
#define RECORD_CHECKUP 7
#endif

/* ==========================================
 * 【绝对指令植入】救护车出勤日志与手术室节点定义
 * ========================================== */
#ifndef HIS_EXT_NODES_DEFINED
#define HIS_EXT_NODES_DEFINED
/**
 * @brief 救护车出勤日志节点
 * 记录救护车的历史派发流水，供医生参考急诊运送情况。
 */
typedef struct AmbulanceLogNode {
    char log_id[32];            // 日志单号
    char vehicle_id[32];        // 派发的车辆编号
    char dispatch_time[32];     // 派送时间
    char destination[128];      // 目标地点
    struct AmbulanceLogNode* next;
} AmbulanceLogNode;

/**
 * @brief 手术室节点
 * 方便医生实时监控手术室资源状态，以便为急重患者快速开具转入。
 */
typedef struct OperatingRoomNode {
    char room_id[32];           // 手术室编号
    int status;                 // 0: 空闲, 1: 手术中, 2: 消毒维护
    char current_patient[32];   // 当前患者
    char current_doctor[32];    // 主刀医生
    struct OperatingRoomNode* next;
} OperatingRoomNode;
#endif

// 引入全局外部指针，指向底层数据中枢 (data_manager.c) 中维护的链表
extern OperatingRoomNode* or_head;           
extern AmbulanceLogNode* ambulance_log_head; 
extern RecordNode* record_head;

// 声明底层跨模块函数
extern int transfer_outpatient_to_inpatient(const char* patient_id);
extern BedNode* find_available_bed(const char* dept_id, WardType type);

/* ==========================================
 * 1. 核心校验逻辑
 * ========================================== */

/**
 * @brief 校验处方时效性：解析协议字段并对比当前时间
 * @param desc 包含处方信息的描述字符串
 * @return 1 表示有效或非处方记录，0 表示处方已过期
 */
int is_prescription_valid(const char* desc) {
    // 防御性校验：如果不是以 "PRES" 开头，说明这可能只是一条普通诊断记录，直接放行
    if (strncmp(desc, "PRES", 4) != 0) return 1; 

    int type, days;
    char date_str[32];
    
    // 解析医院内部定义的简易处方协议字符串
    // 格式规范: PRES|T:类型|D:天数|ID:药品|Q:数量|DATE:日期(YYYY-MM-DD)
    if (sscanf(desc, "PRES|T:%d|D:%d|ID:%*[^|]|Q:%*d|DATE:%s", &type, &days, date_str) != 3) {
        return 1; // 如果解析失败，为了不阻塞业务，默认放行（或可根据严格模式改为拦截）
    }

    // 将解析出的日期字符串转换为时间结构体 tm
    struct tm tm_pres = {0};
    int year, month, day;
    sscanf(date_str, "%d-%d-%d", &year, &month, &day);
    tm_pres.tm_year = year - 1900; // tm_year 是自 1900 年起的年数
    tm_pres.tm_mon = month - 1;    // tm_mon 范围是 0-11
    tm_pres.tm_mday = day;

    // 将开方时间转换为 Unix 时间戳 (秒)
    time_t time_pres = mktime(&tm_pres);
    // 获取当前系统时间
    time_t time_now = time(NULL);

    // 计算当前时间与开方时间的差值（秒）
    double diff_sec = difftime(time_now, time_pres);
    int diff_days = (int)(diff_sec / (24 * 3600)); // 换算成相差的天数

    // 如果流逝的天数超过了处方允许的有效期天数，则拦截
    if (diff_days > days) {
        printf(COLOR_RED "\n[拦截] 处方已失效（开立于 %s，有效期 %d 天）。\n" COLOR_RESET, date_str, days);
        return 0;
    }
    return 1;
}

/* ==========================================
 * 2. 【神级补丁：执业场景动态侦测】
 * ========================================== */

/**
 * @brief 根据本地排班表，动态侦测该医生今天的执业场景
 * 满足题签防混流要求，自动切换门诊与急诊工作台模式。
 * @return 1: 急诊模式, 2: 普通门诊模式, 0: 今日未排班
 */
static int get_current_working_scene(const char* doctor_id) {
    char today[32];
    get_current_time_str(today); // 格式如 2026-03-30 10:00:00
    
    FILE* fp = fopen("data/schedules.txt", "r");
    if (!fp) return 2; // 如果尚未生成排班表，默认放行至门诊模式
    
    char line[256];
    int is_scheduled = 0, is_emergency = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        char d_date[32] = {0}, d_slot[32] = {0}, d_dept[32] = {0}, d_doc[32] = {0};
        if (sscanf(line, "%31s %31s %31s %31s", d_date, d_slot, d_dept, d_doc) == 4) {
            // 日期前 10 位匹配 (YYYY-MM-DD) 且医生 ID 匹配
            if (strcmp(d_doc, doctor_id) == 0 && strncmp(d_date, today, 10) == 0) {
                is_scheduled = 1;
                // 如果科室名包含 EM 或 急诊，则触发急救通道模式
                if (strstr(d_dept, "EM") || strstr(d_dept, "急诊")) {
                    is_emergency = 1;
                    break;
                }
            }
        }
    }
    fclose(fp);
    
    if (!is_scheduled) return 0; // 考勤拦截
    return is_emergency ? 1 : 2;
}

/* ==========================================
 * 3. 医护工作站入口与视图渲染
 * ========================================== */

// 提前声明新增的功能模块
void admission_service(DoctorNode* doctor, PatientNode* patient);
void book_surgery_service(DoctorNode* doctor, PatientNode* patient);
void prescribe_checkup_service(DoctorNode* doctor, PatientNode* patient);
void generate_checkup_report(DoctorNode* doctor);

/**
 * @brief 医生子系统主循环面板
 */
void doctor_subsystem() {
    clear_input_buffer();
    print_header("医护人员智慧工作站 v3.0");
    
    char id[MAX_ID_LEN];
    printf(COLOR_YELLOW "登录鉴权 - 请输入工号: " COLOR_RESET);
    safe_get_string(id, sizeof(id));
    
    // 验证医生身份
    DoctorNode* me = find_doctor_by_id(id);
    if (me == NULL) {
        printf(COLOR_RED "\n[错误] 认证失败，工号不存在！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    
    // 【执业场景侦测】
    int scene = get_current_working_scene(me->id);
    if (scene == 0) {
        printf(COLOR_YELLOW "\n[考勤提醒] 您今日无排班记录，仅开放只读权限，强制设为门诊旁听模式。\n" COLOR_RESET);
        scene = 2; 
        wait_for_enter();
    }

    int choice = -1;
    PatientNode* current_patient = NULL; // 记录当前正在接诊的患者

    // 主交互循环
    while (choice != 0) {
        clear_input_buffer();
        // 场景化渲染控制台抬头
        if (scene == 1) {
            printf("\n" COLOR_RED "========== 智慧HIS | 120急诊/抢救控制台 ==========" COLOR_RESET "\n");
        } else {
            printf("\n" COLOR_BLUE "========== 智慧HIS | 普通门诊工作台 ==========" COLOR_RESET "\n");
        }
        
        printf("  医生: %-10s | 科室: %-10s | 模式: %s\n", me->name, me->dept_id, (scene == 1 ? COLOR_RED"急诊"COLOR_RESET : COLOR_BLUE"门诊"COLOR_RESET));
        
        // 动态显示当前接诊状态
        if (current_patient != NULL) {
            printf(COLOR_RED "  >>> 就诊中: %s (%s)\n" COLOR_RESET, current_patient->name, current_patient->id);
        } else {
            printf(COLOR_GREEN "  >>> 状态: 候诊中 (Ready)\n" COLOR_RESET);
        }
        printf("==================================================\n");
        
        // 功能菜单 (涵盖 PDF 题签中的全部扩展要求)
        printf("  --- 临床接诊 ---\n");
        printf("  [ 1 ] 查看候诊队列 (物理隔离防混流)\n");
        printf("  [ 2 ] 呼叫下一位患者\n");
        printf("  [ 3 ] 结束当前诊疗任务\n");
        printf("  --- 处方与检查 ---\n");
        printf("  [ 4 ] 开具分类电子处方\n");
        printf("  [ 5 ] 药房发药/处方核销\n");
        printf("  [ 6 ] 开具体检/检查单\n");
        printf("  [ 7 ] 录入结果并生成《体检报告》\n");
        printf("  --- 手术与住院 (重构版) ---\n");
        printf("  [ 8 ] 手术室一键预约与计费 (" COLOR_CYAN "防撞锁开启" COLOR_RESET ")\n"); 
        printf("  [ 9 ] 患者入院收治 (" COLOR_CYAN "底层链表物理搬家" COLOR_RESET ")\n");
        printf("  [ 10] 住院患者 ICU 流转 (" COLOR_CYAN "床位熔断告警开启" COLOR_RESET ")\n"); 
        printf("  --- 监控与报表 ---\n");
        printf("  [ 11] 查看本科室住院患者名单\n");
        printf("  [ 12] 全院手术室实时追踪\n"); 
        printf("  [ 13] 查看 120 救护车历史出勤表\n"); 
        printf("  [ 14] 个人接诊工作量统计\n");
        printf(COLOR_RED "  [ 0 ] 安全退出系统\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请选择业务编号 (0-14): " COLOR_RESET);
        choice = safe_get_int();
        
        switch (choice) {
            case 1: view_my_queue(me); break;
            case 2: 
                if (!current_patient) {
                    current_patient = call_next_patient(me); 
                } else {
                    printf(COLOR_RED "提示：请先结束当前患者 [%s] 的诊疗任务，再呼叫下一位！\n" COLOR_RESET, current_patient->name);
                    wait_for_enter();
                }
                break;
            case 3: 
                if (current_patient) {
                    printf(COLOR_GREEN "已完成患者 %s 的诊疗记录提交，进入待命状态。\n" COLOR_RESET, current_patient->name);
                    current_patient = NULL;
                } else {
                    printf(COLOR_YELLOW "当前没有正在接诊的患者。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            case 4: 
                if (current_patient) prescribe_medicine_advanced(me, current_patient); 
                else { printf(COLOR_RED "提示：未接诊患者！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 5: dispense_medicine_service(); break;
            case 6: 
                if (current_patient) prescribe_checkup_service(me, current_patient);
                else { printf(COLOR_RED "提示：未接诊患者！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 7: generate_checkup_report(me); break;
            case 8: 
                if (current_patient) book_surgery_service(me, current_patient);
                else { printf(COLOR_RED "提示：需先接诊患者才能安排手术！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 9:
                if (current_patient) admission_service(me, current_patient);
                else { printf(COLOR_RED "提示：需先接诊门诊患者才能办理住院！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 10: transfer_patient_to_icu(me); break;
            case 11: view_department_inpatients(me); break;
            case 12: {
                print_header("手术室实时追踪看板");
                printf("%-10s %-12s %-15s %-15s\n", "手术室", "当前状态", "主刀医生", "患者编号");
                print_divider();
                OperatingRoomNode* curr = or_head;
                int c = 0;
                while (curr) {
                    const char* st;
                    if (curr->status == 0) st = COLOR_GREEN "空闲" COLOR_RESET;
                    else if (curr->status == 1) st = COLOR_RED "手术中" COLOR_RESET;
                    else st = COLOR_YELLOW "消毒维护" COLOR_RESET;
                    printf("%-10s %-12s %-15s %-15s\n", curr->room_id, st, curr->current_doctor, curr->current_patient);
                    curr = curr->next; c++;
                }
                if(c == 0) printf(COLOR_YELLOW "目前尚未开启手术室系统。\n" COLOR_RESET);
                wait_for_enter(); break;
            }
            case 13: {
                print_header("120 救护车历史出勤记录表");
                printf("%-15s %-12s %-22s %-30s\n", "调度单号", "车牌编号", "出勤时间", "目的地");
                print_divider();
                AmbulanceLogNode* curr_log = ambulance_log_head;
                int log_count = 0;
                while (curr_log) {
                    printf("%-15s %-12s %-22s %-30s\n",
                           curr_log->log_id, curr_log->vehicle_id,
                           curr_log->dispatch_time, curr_log->destination);
                    curr_log = curr_log->next;
                    log_count++;
                }
                if (log_count == 0) printf(COLOR_YELLOW "暂无救护车出勤历史记录。\n" COLOR_RESET);
                wait_for_enter(); break;
            }
            case 14: view_doctor_statistics(me); break;
            case 0: break;
        }
    }
}

/* ==========================================
 * 4. 诊疗业务：物理隔离的队列与呼叫
 * ========================================== */

/**
 * @brief 查看医生专属的候诊队列，依据工作场景严格物理隔离
 */
void view_my_queue(DoctorNode* doctor) {
    int scene = get_current_working_scene(doctor->id);
    print_header(scene == 1 ? "急诊/抢救 专属优先队列" : "门诊常规就诊队列");
    RecordNode* curr = record_head;
    int count = 0;
    while (curr) {
        // 核心过滤：匹配当前医生，且单据状态为未处理
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 0) {
            
            // 场景隔离：急诊医生只看 RECORD_EMERGENCY，门诊医生只看 RECORD_REGISTRATION
            if (scene == 1 && curr->type == RECORD_EMERGENCY) {
                printf("[%02d]  %-15s " COLOR_RED "[急救通道-置顶]" COLOR_RESET " 患者: %-10s\n", 
                       ++count, curr->record_id, curr->patient_id);
            } 
            else if (scene == 2 && curr->type == RECORD_REGISTRATION) {
                printf("排号:[%03d]  %-15s [普通门诊] 患者: %-10s\n", 
                       curr->queue_number, curr->record_id, curr->patient_id);
                count++;
            }
        }
        curr = curr->next;
    }
    if (count == 0) printf(COLOR_YELLOW "当前暂无符合您排班场景的候诊患者。\n" COLOR_RESET);
    wait_for_enter();
}

/**
 * @brief 呼叫下一位待诊患者，严格执行隔离策略
 */
PatientNode* call_next_patient(DoctorNode* doctor) {
    int scene = get_current_working_scene(doctor->id);
    RecordNode *curr = record_head, *target = NULL;
    int min_q = 999999; 
    
    curr = record_head;
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 0) {
            // 急诊医生抓取最近的急诊记录（一般 queue_number 均为 0 或较小）
            if (scene == 1 && curr->type == RECORD_EMERGENCY) {
                if (curr->queue_number < min_q) { min_q = curr->queue_number; target = curr; }
            }
            // 门诊医生按顺序号抓取
            else if (scene == 2 && curr->type == RECORD_REGISTRATION) {
                if (curr->queue_number < min_q) { min_q = curr->queue_number; target = curr; }
            }
        }
        curr = curr->next;
    }

    if (target) {
        target->status = 1; // 将挂号单状态标记为“就诊完毕/接诊中”
        PatientNode* p = find_outpatient_by_id(target->patient_id);
        if (!p) p = find_inpatient_by_id(target->patient_id); // 兼容住院挂号
        
        if (p) {
            printf(COLOR_RED "\n>>> [系统播报] 请 %s 患者 %s 前往 %s 诊室就诊！<<<\n" COLOR_RESET, 
                      (scene == 1 ? "急诊" : "门诊"), p->name, doctor->name);
        }
        return p;
    }
    
    printf(COLOR_CYAN "候诊大厅目前暂无等待您的患者。\n" COLOR_RESET);
    return NULL;
}

/* ==========================================
 * 5. 新增：手术、住院、体检闭环 (满足题签要求)
 * ========================================== */

/**
 * @brief 办理入院收治 (含严苛的财务押金校验与链表搬家)
 */
void admission_service(DoctorNode* doctor, PatientNode* patient) {
    print_header("住院收治通道 (底层链表重构版)");
    
    if (patient->type == 'I') {
        printf(COLOR_YELLOW "提示：该患者目前已在住院名单中，无需重复办理。\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 严苛财务校验：询问拟住院天数，计算押金底线
    printf("请输入拟住院天数 (用于计算押金标准): ");
    int days = safe_get_int();
    if (days <= 0) days = 1;

    double min_deposit = 200.0 * days;
    printf(COLOR_CYAN "系统测算：最低需缴纳押金 %.2f 元 (且必须为 100 的整数倍)。\n" COLOR_RESET, min_deposit);
    
    printf("请向患者收取住院预缴押金 (元): ");
    double deposit = safe_get_double();

    // 题签硬性规则校验
    if (deposit < min_deposit || ((int)deposit % 100) != 0) {
        printf(COLOR_RED "办理驳回：押金金额不符合财务规范 (不足 %d 或非 100 的倍数)！\n" COLOR_RESET, (int)min_deposit);
        wait_for_enter(); return;
    }

    // 床位分配
    printf("选择病房类型 (1-ICU, 2-普通双人, 3-单人陪护): ");
    int type_val = safe_get_int();
    WardType target = (type_val == 1) ? WARD_SPECIAL : WARD_GENERAL; // 简化映射
    BedNode* b = find_available_bed(doctor->dept_id, target);
    
    if (!b) {
        printf(COLOR_RED "收治阻断：本科室当前无可用床位，请联系行政扩容！\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 录入预计出院日期 (满足题签新增字段要求)
    char expected_date[32];
    printf("请输入预计出院日期 (YYYY-MM-DD): ");
    safe_get_string(expected_date, 32);

    // 【底层核心：链表物理搬家】
    if (transfer_outpatient_to_inpatient(patient->id)) {
        // 搬家成功后，必须重新获取位于 inpatient_head 里的该节点指针
        PatientNode* in_p = find_inpatient_by_id(patient->id);
        if (in_p) {
            in_p->hospital_deposit = deposit;
            strcpy(in_p->bed_id, b->bed_id);
            get_current_time_str(in_p->admission_date);
            // 注: 如果 common.h 扩充了 expected_discharge_date，此处应为其赋值
            
            // 物理锁定床位
            b->is_occupied = 1;
            strcpy(b->patient_id, in_p->id);
            
            // 产生财务流水 (住院手续费，但不含押金以免虚增营业额)
            add_new_record(in_p->id, doctor->id, RECORD_HOSPITALIZATION, "办理入院手续工本费", 50.0, 0);
            
            printf(COLOR_GREEN ">>> 办理成功！患者 %s 已物理流转至住院部，床位: %s\n" COLOR_RESET, in_p->name, b->bed_id);
        }
    } else {
        printf(COLOR_RED "严重异常：内存链表流转失败！\n" COLOR_RESET);
    }
    wait_for_enter();
}

/**
 * @brief 手术室预约与自动计费闭环
 */
void book_surgery_service(DoctorNode* doctor, PatientNode* patient) {
    print_header("手术排班与自动计费中心");
    
    printf("请输入拟施行的手术名称 (如：阑尾切除术): ");
    char s_name[128]; safe_get_string(s_name, 128);
    
    printf("请输入该手术的预计费用 (元): ");
    double fee = safe_get_double();

    // 寻找空闲手术室
    OperatingRoomNode* t = or_head;
    OperatingRoomNode* target_or = NULL;
    while(t) {
        if(t->status == 0) { target_or = t; break; }
        t = t->next;
    }

    if (!target_or) {
        printf(COLOR_RED "告警：全院手术室均在占用或消毒状态，无法排期！\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 【防并发撞车锁】检查医生或患者是否已经在手术中
    OperatingRoomNode* check = or_head;
    while(check) {
        if(check->status == 1) {
            if (strcmp(check->current_doctor, doctor->id) == 0) {
                printf(COLOR_RED "拦截：您当前正在 %s 执行手术，请先结束再开新台！\n" COLOR_RESET, check->room_id);
                wait_for_enter(); return;
            }
            if (strcmp(check->current_patient, patient->id) == 0) {
                printf(COLOR_RED "拦截：患者 %s 正在 %s 接受手术，产生并发冲突！\n" COLOR_RESET, patient->name, check->room_id);
                wait_for_enter(); return;
            }
        }
        check = check->next;
    }

    // 锁定手术室
    target_or->status = 1;
    strcpy(target_or->current_doctor, doctor->id);
    strcpy(target_or->current_patient, patient->id);

    // 生成手术计费账单
    char desc[MAX_DESC_LEN];
    sprintf(desc, "SURGERY|%s|房间:%s", s_name, target_or->room_id);
    add_new_record(patient->id, doctor->id, RECORD_SURGERY, desc, fee, 0);

    printf(COLOR_GREEN ">>> 预约成功！手术室 %s 已锁定。\n" COLOR_RESET, target_or->room_id);
    printf(COLOR_CYAN ">>> 手术账单 (%.2f 元) 已生成，患者缴费后即可推入机房。\n" COLOR_RESET, fee);
    wait_for_enter();
}

/**
 * @brief 开具体检单并生成缴费记录
 */
void prescribe_checkup_service(DoctorNode* doctor, PatientNode* patient) {
    print_header("开具健康体检/筛查单");
    printf("请输入体检套餐名称 (如：入职全身体检): ");
    char c_name[128]; safe_get_string(c_name, 128);
    
    printf("请输入体检项目总费用: ");
    double fee = safe_get_double();

    char desc[MAX_DESC_LEN];
    // 埋入特殊标记用于后续生成报告检索
    sprintf(desc, "CHK_PENDING|%s", c_name);
    
    add_new_record(patient->id, doctor->id, RECORD_CHECKUP, desc, fee, 0);
    printf(COLOR_GREEN "体检单已开具，等待患者前往收费窗口结账后检查。\n" COLOR_RESET);
    wait_for_enter();
}

/**
 * @brief 录入体检结果并打印精美报告
 */
void generate_checkup_report(DoctorNode* doctor) {
    print_header("录入结果并生成《体检报告单》");
    printf("请输入体检记录编号 (RECxxx): ");
    char rid[32]; safe_get_string(rid, 32);

    RecordNode* r = record_head;
    RecordNode* target = NULL;
    while(r) {
        if (strcmp(r->record_id, rid) == 0 && r->type == RECORD_CHECKUP) {
            target = r; break;
        }
        r = r->next;
    }

    if (!target) {
        printf(COLOR_RED "未找到匹配的体检单据。\n" COLOR_RESET);
        wait_for_enter(); return;
    }
    if (target->status == 0) {
        printf(COLOR_RED "拦截：该体检单尚未缴费，检验科拒绝执行！\n" COLOR_RESET);
        wait_for_enter(); return;
    }
    if (strstr(target->description, "CHK_DONE")) {
        printf(COLOR_YELLOW "该单据报告已出具，无需重复录入。\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 录入结果
    printf("请输入各项指标及体检结论 (限制 200 字): ");
    char result[256]; safe_get_string(result, 256);

    // 修改描述打上完毕标记，并留存结果
    char new_desc[MAX_DESC_LEN];
    sprintf(new_desc, "CHK_DONE|结论:%s", result);
    strcpy(target->description, new_desc);

    PatientNode* p = find_outpatient_by_id(target->patient_id);
    if (!p) p = find_inpatient_by_id(target->patient_id);

    // 终端绘制报告单
    clear_input_buffer();
    printf("\n");
    printf("======================================================\n");
    printf("             健康体检报告单 (HIS 自动生成)            \n");
    printf("======================================================\n");
    printf(" 患者姓名: %-12s | 性别: %-6s | 年龄: %d\n", p?p->name:"未知", p?p->gender:"-", p?p->age:0);
    printf(" 报告单号: %-12s | 责任医生: %s\n", target->record_id, doctor->name);
    printf("------------------------------------------------------\n");
    printf(" [ 检 查 结 论 及 医 嘱 ]\n\n");
    printf("   %s\n\n", result);
    printf("------------------------------------------------------\n");
    printf(" 报告时间: %s\n", target->date);
    printf("======================================================\n");
    printf(COLOR_GREEN "报告已永久归档至电子病历系统。\n" COLOR_RESET);
    wait_for_enter();
}

/* ==========================================
 * 6. 辅助与统计模块
 * ========================================== */

/**
 * @brief 将普通住院患者转移至 ICU 病房 (带熔断警告)
 */
void transfer_patient_to_icu(DoctorNode* doctor) {
    print_header("患者转入 ICU / 特殊监护病房");
    printf("请输入拟流转的患者编号: ");
    char p_id[MAX_ID_LEN];
    safe_get_string(p_id, sizeof(p_id));

    PatientNode* p = find_inpatient_by_id(p_id);
    if (!p) {
        printf(COLOR_RED "提示：未在住院名单中找到该患者。\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    BedNode* icu_bed = find_available_bed(doctor->dept_id, WARD_SPECIAL); 
    
    // 【床位熔断告警机制】
    if (!icu_bed) {
        printf("\n" COLOR_RED "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" COLOR_RESET "\n");
        printf(COLOR_RED "!!! [关键医疗资源枯竭告警] 本科室 ICU 病床已全部爆满 !!!" COLOR_RESET "\n");
        printf(COLOR_RED "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" COLOR_RESET "\n");
        printf(COLOR_YELLOW "\n>>> 请立即联系行政主管增加临时床位，或调用降级改造协议！<<<\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    BedNode* old_bed = find_bed_by_id(p->bed_id);
    if (old_bed) { 
        old_bed->is_occupied = 0; 
        strcpy(old_bed->patient_id, "N/A"); 
    }

    strcpy(p->bed_id, icu_bed->bed_id);
    icu_bed->is_occupied = 1;
    strcpy(icu_bed->patient_id, p->id);

    add_new_record(p->id, doctor->id, RECORD_HOSPITALIZATION, "[ICU转入] 重症加强护理单", 1500.0, 0);

    printf(COLOR_GREEN "成功：患者 %s 已成功转入 ICU 床位 %s。\n" COLOR_RESET, p->name, icu_bed->bed_id);
    wait_for_enter();
}

/**
 * @brief 开具电子处方
 */
void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient) {
    print_header("开具分类电子处方");
    printf(" [ 1 ] 普通 (7天) | [ 2 ] 急诊 (3天) | [ 3 ] 慢病 (30天)\n 选择: ");
    int type = safe_get_int();
    int days = (type == 2) ? 3 : (type == 3 ? 30 : 7);

    printf("药品 ID: ");
    char mid[MAX_ID_LEN]; safe_get_string(mid, sizeof(mid));
    MedicineNode* m = find_medicine_by_id(mid);

    if (!m || m->stock <= 0) { 
        printf(COLOR_RED "拦截：库存不足或药品不存在！\n" COLOR_RESET); 
        wait_for_enter(); return; 
    }
    
    printf("数量: ");
    int q = safe_get_int();
    if (q > m->stock) { 
        printf(COLOR_RED "拦截：申领数量大于当前库存！\n" COLOR_RESET); 
        wait_for_enter(); return; 
    }

    char time_str[32]; 
    get_current_time_str(time_str);
    
    char desc[MAX_DESC_LEN];
    sprintf(desc, "PRES|T:%d|D:%d|ID:%s|Q:%d|DATE:%s", type, days, m->id, q, time_str);
    
    add_new_record(patient->id, doctor->id, RECORD_CONSULTATION, desc, m->price * q, 0);
    printf(COLOR_GREEN "处方已下达至药房，有效期 %d 天。\n" COLOR_RESET, days);
    wait_for_enter();
}

/**
 * @brief 药房发药与核销
 */
void dispense_medicine_service() {
    print_header("药房发药/处方核销");
    printf("处方/问诊单号: ");
    char rid[MAX_ID_LEN]; safe_get_string(rid, sizeof(rid));
    
    RecordNode* r = record_head;
    while(r) {
        if (strcmp(r->record_id, rid) == 0) {
            if (r->status == 0) { 
                printf(COLOR_RED "拦截：单据未缴费！\n" COLOR_RESET); 
                wait_for_enter(); return; 
            }
            if (!is_prescription_valid(r->description)) {
                wait_for_enter(); return; 
            }

            char mid[MAX_ID_LEN]; int q;
            if (sscanf(r->description, "PRES|T:%*d|D:%*d|ID:%[^|]|Q:%d", mid, &q) == 2) {
                MedicineNode* m = find_medicine_by_id(mid);
                if (m && m->stock >= q) {
                    m->stock -= q;
                    printf(COLOR_GREEN "核销完成，已发放 %s x %d，库存已物理扣减。\n" COLOR_RESET, m->common_name, q);
                    // 防止重复核销
                    strcat(r->description, "|已核销");
                } else {
                    printf(COLOR_RED "异常：当前库存不足，无法发药！\n" COLOR_RESET);
                }
            } else {
                printf(COLOR_YELLOW "该单据不包含符合协议的处方药品。\n" COLOR_RESET);
            }
            wait_for_enter(); return;
        }
        r = r->next;
    }
    printf(COLOR_RED "未找到对应单据。\n" COLOR_RESET);
    wait_for_enter();
}

void view_doctor_statistics(DoctorNode* doctor) {
    print_header("工作统计");
    int c = 0; double t = 0;
    RecordNode* curr = record_head;
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 1) { 
            c++; t += curr->total_fee; 
        }
        curr = curr->next;
    }
    printf(" 今日接诊: %d 次 | 营收贡献: %.2f 元\n", c, t);
    wait_for_enter();
}

void view_department_inpatients(DoctorNode* doctor) {
    print_header("本科室在院患者名单");
    PatientNode* p = inpatient_head;
    int found = 0;
    while (p) {
        BedNode* b = find_bed_by_id(p->bed_id);
        if (b && strcmp(b->dept_id, doctor->dept_id) == 0) {
            printf("床号: %-10s | 患者: %s\n", b->bed_id, p->name);
            found = 1;
        }
        p = p->next;
    }
    if (!found) printf("本科室目前无住院患者。\n");
    wait_for_enter();
}