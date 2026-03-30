#include "doctor.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

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
 * 2. 医护工作站入口
 * ========================================== */

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
    
    int choice = -1;
    PatientNode* current_patient = NULL; // 记录当前正在接诊的患者

    // 主交互循环
    while (choice != 0) {
        clear_input_buffer();
        printf("\n" COLOR_BLUE "=========================================\n");
        printf("  姓名: %-10s | 科室: %s\n", me->name, me->dept_id);
        
        // 动态显示当前接诊状态
        if (current_patient != NULL) {
            printf(COLOR_RED "  >>> 就诊中: %s (%s)\n" COLOR_RESET, current_patient->name, current_patient->id);
        } else {
            printf(COLOR_GREEN "  >>> 状态: 候诊中 (Ready)\n" COLOR_RESET);
        }
        printf("=========================================\n" COLOR_RESET);
        
        // 功能菜单 (严格依指令新增了 9 和 10 两个选项，原功能纹丝不动)
        printf("  [ 1 ] 查看候诊队列 (含急诊优先级标记)\n");
        printf("  [ 2 ] 呼叫下一位患者 (急诊优先)\n");
        printf("  [ 3 ] 开具分类电子处方\n");
        printf("  [ 4 ] 药房发药/处方核销\n");
        printf("  [ 5 ] 患者 ICU 转院/转科申请\n"); // 新增重症流转功能
        printf("  [ 6 ] 结束当前诊疗任务\n");
        printf("  [ 7 ] 查看本科室住院患者名单\n");
        printf("  [ 8 ] 个人接诊工作量统计\n");
        printf("  [ 9 ] 全院手术室实时状态追踪 (急救联动)\n"); // 【绝对指令：手术室功能】
        printf("  [ 10] 查看 120 救护车历史出勤表 (急诊参考)\n"); // 【绝对指令：救护车出勤表】
        printf(COLOR_RED "  [ 0 ] 退出登录\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请选择业务编号 (0-10): " COLOR_RESET);
        choice = safe_get_int();
        
        switch (choice) {
            case 1: view_my_queue(me); break;
            case 2: 
                // 防止医生在未结束上一位患者诊疗时重复叫号
                if (!current_patient) {
                    current_patient = call_next_patient(me); 
                } else {
                    printf(COLOR_RED "提示：请先结束当前患者 [%s] 的诊疗任务，再呼叫下一位！\n" COLOR_RESET, current_patient->name);
                    wait_for_enter();
                }
                break;
            case 3: 
                // 必须在接诊状态下才能开具处方
                if (current_patient) {
                    prescribe_medicine_advanced(me, current_patient); 
                } else {
                    printf(COLOR_RED "提示：当前无接诊患者，无法开具处方！请先叫号。\n" COLOR_RESET);
                    wait_for_enter();
                }
                break;
            case 4: dispense_medicine_service(); break;
            case 5: transfer_patient_to_icu(me); break;
            case 6: 
                // 结束当前就诊，清空当前患者指针
                if (current_patient) {
                    printf(COLOR_GREEN "已完成患者 %s 的诊疗记录提交。\n" COLOR_RESET, current_patient->name);
                    current_patient = NULL;
                } else {
                    printf(COLOR_YELLOW "当前没有正在接诊的患者。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            case 7: view_department_inpatients(me); break;
            case 8: view_doctor_statistics(me); break;
            case 9: {
                // 【绝对指令响应】：手术室联动查看模块
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
            case 10: {
                // 【绝对指令响应】：救护车历史出勤表查看模块
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
            case 0: break;
        }
    }
}

/* ==========================================
 * 3. 诊疗业务：急诊优先叫号
 * ========================================== */

/**
 * @brief 呼叫下一位待诊患者，严格执行“急诊优先”策略
 * @return 被呼叫的患者节点指针，若队列为空则返回 NULL
 */
PatientNode* call_next_patient(DoctorNode* doctor) {
    RecordNode *curr = record_head, *target = NULL;
    int min_q = 999999; // 用于寻找最小排队号
    
    // 第一阶段：全局优先搜索急诊挂号 (RECORD_EMERGENCY = 5)
    // 即使普通号排在前面，急诊也拥有绝对优先权
    curr = record_head;
    while (curr) {
        // 匹配条件：同医生 + 是急诊 + 状态为待处理(0)
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->type == RECORD_EMERGENCY && curr->status == 0) {
            if (curr->queue_number < min_q) { 
                min_q = curr->queue_number; 
                target = curr; 
            }
        }
        curr = curr->next;
    }
    
    // 第二阶段：如果没找到急诊患者，则按正常排队顺序搜索普通门诊 (RECORD_REGISTRATION = 1)
    if (!target) {
        min_q = 999999; // 重置最小排队号
        curr = record_head;
        while (curr) {
            // 匹配条件：同医生 + 是普通挂号 + 状态为待处理(0)
            if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->type == RECORD_REGISTRATION && curr->status == 0) {
                if (curr->queue_number < min_q) { 
                    min_q = curr->queue_number; 
                    target = curr; 
                }
            }
            curr = curr->next;
        }
    }

    // 如果成功找到下一位患者
    if (target) {
        target->status = 1; // 将挂号单状态标记为“已完成(就诊中)”
        PatientNode* p = find_outpatient_by_id(target->patient_id);
        
        if (p) {
            printf(COLOR_RED "\n>>> [叫号] 请 %s 患者 %s 到诊室就诊！\n" COLOR_RESET, 
                      (target->type == RECORD_EMERGENCY ? "急诊" : "普通"), p->name);
        }
        return p;
    }
    
    printf(COLOR_CYAN "候诊大厅目前暂无等待排队的患者。\n" COLOR_RESET);
    return NULL;
}

/* ==========================================
 * 4. 业务扩展：ICU 转院申请
 * ========================================== */

/**
 * @brief 将普通住院患者转移至 ICU 病房
 */
void transfer_patient_to_icu(DoctorNode* doctor) {
    print_header("患者转入 ICU / 特殊监护病房");
    printf("请输入患者编号: ");
    char p_id[MAX_ID_LEN];
    safe_get_string(p_id, sizeof(p_id));

    PatientNode* p = find_inpatient_by_id(p_id);
    if (!p) {
        printf(COLOR_RED "提示：未在住院名单中找到该患者。\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 自动寻找当前医生所属科室中，类型为 1 (SPECIAL_WARD 即 ICU) 的空闲床位
    BedNode* icu_bed = find_available_bed(doctor->dept_id, 1); 
    if (!icu_bed) {
        printf(COLOR_RED "告警：本科室 ICU 资源已满，无法完成转院申请！\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 【床位流转逻辑】：释放患者原来占用的普通床位
    BedNode* old_bed = find_bed_by_id(p->bed_id);
    if (old_bed) { 
        old_bed->is_occupied = 0; 
        strcpy(old_bed->patient_id, ""); 
    }

    // 【床位流转逻辑】：分配新的 ICU 床位
    strcpy(p->bed_id, icu_bed->bed_id);
    icu_bed->is_occupied = 1;
    strcpy(icu_bed->patient_id, p->id);

    // 产生财务流水：自动增加一条 ICU 护理记录用于后期结算
    add_new_record(p->id, doctor->id, RECORD_HOSPITALIZATION, "[ICU转入] 重症加强护理单", 1500.0, 0);

    printf(COLOR_GREEN "成功：患者 %s 已成功转入 ICU 床位 %s。\n" COLOR_RESET, p->name, icu_bed->bed_id);
    wait_for_enter();
}

/* ==========================================
 * 5. 处方与发药
 * ========================================== */

/**
 * @brief 开具电子处方（生成特定格式的协议字符串存入记录）
 */
void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient) {
    print_header("开具分类电子处方");
    // 根据国家处方管理办法，分类设定处方有效天数
    printf(" [ 1 ] 普通 (7天) | [ 2 ] 急诊 (3天) | [ 3 ] 慢病 (30天)\n 选择: ");
    int type = safe_get_int();
    int days = (type == 2) ? 3 : (type == 3 ? 30 : 7);

    printf("药品 ID: ");
    char mid[MAX_ID_LEN]; safe_get_string(mid, sizeof(mid));
    MedicineNode* m = find_medicine_by_id(mid);

    // 库存预检
    if (!m || m->stock <= 0) { 
        printf(COLOR_RED "库存不足！\n" COLOR_RESET); 
        return; 
    }
    
    printf("数量: ");
    int q = safe_get_int();
    if (q > m->stock) { 
        printf(COLOR_RED "库存不足！\n" COLOR_RESET); 
        return; 
    }

    // 获取当前时间戳作为开具日期
    char time_str[32]; 
    get_current_time_str(time_str);
    
    // 组装处方协议字符串 (PRES 协议)
    char desc[MAX_DESC_LEN];
    sprintf(desc, "PRES|T:%d|D:%d|ID:%s|Q:%d|DATE:%s", type, days, m->id, q, time_str);
    
    // 生成问诊单据，将开药总价记入财务系统
    add_new_record(patient->id, doctor->id, RECORD_CONSULTATION, desc, m->price * q, 0);
    printf(COLOR_GREEN "处方已下达，有效期 %d 天。\n" COLOR_RESET, days);
}

/**
 * @brief 药房发药与处方核销服务
 */
void dispense_medicine_service() {
    print_header("药房发药/处方核销");
    printf("记录编号: ");
    char rid[MAX_ID_LEN]; safe_get_string(rid, sizeof(rid));
    
    RecordNode* r = record_head;
    while(r) {
        if (strcmp(r->record_id, rid) == 0) {
            // 前置拦截：如果单据状态为 0 (表示尚未前往收费窗口缴费)
            if (r->status == 0) { 
                printf(COLOR_RED "拦截：单据未缴费！\n" COLOR_RESET); 
                return; 
            }
            
            // 时效性拦截：检查处方是否过期
            if (!is_prescription_valid(r->description)) return; 

            char mid[MAX_ID_LEN]; 
            int q;
            
            // 拆解处方协议，提取药品ID和需要扣减的数量
            if (sscanf(r->description, "PRES|T:%*d|D:%*d|ID:%[^|]|Q:%d", mid, &q) == 2) {
                MedicineNode* m = find_medicine_by_id(mid);
                // 二次核对库存并执行物理扣减
                if (m && m->stock >= q) {
                    m->stock -= q;
                    printf(COLOR_GREEN "核销完成，已发放 %s x %d。\n" COLOR_RESET, m->common_name, q);
                } else {
                    printf(COLOR_RED "异常：当前库存不足，无法核销！\n" COLOR_RESET);
                }
            }
            wait_for_enter(); return;
        }
        r = r->next;
    }
    printf(COLOR_RED "未找到单据。\n" COLOR_RESET);
}

/* ==========================================
 * 6. 统计与辅助
 * ========================================== */

/**
 * @brief 查看医生专属的候诊队列，优先展示急诊患者
 */
void view_my_queue(DoctorNode* doctor) {
    print_header("科室候诊队列 (急诊置顶)");
    RecordNode* curr = record_head;
    int count = 0;
    while (curr) {
        // 过滤条件：匹配当前医生ID、状态待处理、类型为挂号或急诊
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 0 &&
           (curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY)) {
            
            // 终端高亮区分急诊患者
            printf("[%s] %-10s | 排号: %d\n", 
                   (curr->type == RECORD_EMERGENCY ? COLOR_RED"急诊"COLOR_RESET : "普通"), 
                   curr->patient_id, 
                   curr->queue_number);
            count++;
        }
        curr = curr->next;
    }
    if (count == 0) printf("当前无患者等候。\n");
    wait_for_enter();
}

/**
 * @brief 查看该医生当天的接诊人次与创收统计
 */
void view_doctor_statistics(DoctorNode* doctor) {
    print_header("工作统计");
    int c = 0; 
    double t = 0;
    RecordNode* curr = record_head;
    
    while (curr) {
        // 仅统计已完成状态(status == 1) 的单据
        if (strcmp(curr->doctor_id, doctor->id) == 0 && curr->status == 1) { 
            c++; 
            t += curr->total_fee; 
        }
        curr = curr->next;
    }
    printf(" 今日接诊: %d 次 | 营收贡献: %.2f 元\n", c, t);
    wait_for_enter();
}

/**
 * @brief 查看当前医生所属科室的全部住院患者及床位分布
 */
void view_department_inpatients(DoctorNode* doctor) {
    print_header("本科室在院患者");
    PatientNode* p = inpatient_head;
    while (p) {
        BedNode* b = find_bed_by_id(p->bed_id);
        // 通过患者关联的床位，反查该床位是否属于本医生的科室
        if (b && strcmp(b->dept_id, doctor->dept_id) == 0) {
            printf("床号: %-10s | 患者: %s\n", b->bed_id, p->name);
        }
        p = p->next;
    }
    wait_for_enter();
}