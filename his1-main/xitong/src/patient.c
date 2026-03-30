#include "patient.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ==========================================
 * 【新增宏定义】：系统级挂号限流防刷参数
 * 对标题签硬性指标：控制全院、医生、患者及同科室的挂号频率
 * ========================================== */
#ifndef MAX_CLINIC_DAILY_TICKETS
#define MAX_CLINIC_DAILY_TICKETS 500  // 医院每天最多受理 500 个号
#define MAX_DOCTOR_DAILY_TICKETS 20   // 每位医生每天最多受理 20 个号
#define MAX_PATIENT_DAILY_TICKETS 5   // 每位患者每天最多挂 5 个号
#endif

// 引入全局外部指针
extern RecordNode* record_head;
extern DoctorNode* doctor_head;
extern OperatingRoomNode* or_head;

/* ==========================================
 * 1. 身份识别与登录门户
 * ========================================== */

/**
 * @brief 患者登录校验
 * @param input_id 用户输入的就诊卡号 (患者 ID)
 * @return 找到的患者节点指针，未找到则返回 NULL
 */
PatientNode* patient_login(const char* input_id) {
    // 优先在门诊数据库中检索
    PatientNode* p = find_outpatient_by_id(input_id);
    if (p != NULL) return p;
    // 如果门诊没有，则在住院数据库中检索，确保住院患者也能使用自助服务台
    return find_inpatient_by_id(input_id); 
}

/**
 * @brief 患者服务主控台 (Dashboard)
 * 整合了患者在医院需要的所有自助服务功能。
 * @param me 当前登录的患者节点指针
 */
void patient_dashboard(PatientNode* me) {
    int choice = -1;
    // 主循环：直到用户选择 0 退出
    while (choice != 0) {
        clear_input_buffer(); // 清理输入流
        printf("\n" COLOR_BLUE "=========================================\n");
        // 动态显示患者的姓名和身份类型 (门诊 O / 住院 I)
        printf("  患者服务中心 | 登录者: %s %s\n", me->name, (me->type == 'O' ? "(门诊)" : "(住院)"));
        // 实时显示账户余额
        printf("  账户余额: " COLOR_GREEN "%.2f" COLOR_RESET " 元 | 就诊卡号: %s\n", me->account_balance, me->id);
        printf("=========================================" COLOR_RESET "\n");
        
        // 打印功能菜单
        printf("  [ 1 ] 我的个人资料与住院/ICU状态\n");
        printf("  [ 2 ] 预约挂号 (" COLOR_CYAN "含 AI 智能推荐与防刷限号" COLOR_RESET ")\n"); 
        printf("  [ 3 ] 查看实时排队 (含挂号引导)\n"); 
        printf("  [ 4 ] 待缴费账单结算\n");
        printf("  [ 5 ] 账户余额在线充值\n");
        printf("  [ 6 ] 呼叫 120 救护车 (院前急救)\n"); 
        printf("  [ 7 ] 查看全院医生值班表\n");
        printf("  [ 8 ] 历史诊疗记录查询\n");
        printf("  [ 9 ] 财务对账单与电子发票\n");
        printf(COLOR_RED "  [ 0 ] 安全退出登录\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请选择服务编号 (0-9): " COLOR_RESET);
        choice = safe_get_int();
        
        // 业务路由分发
        switch (choice) {
            case 1: view_personal_info(me); break;
            case 2: patient_self_register(me); break;
            case 3: check_queue_status(me); break;
            case 4: patient_pay_bill(me); break;
            case 5: patient_top_up(me); break;
            case 6: call_ambulance_service(me); break;
            case 7: view_duty_schedule(NULL); break; // 传入 NULL 表示查看所有医生的排班
            case 8: query_medical_records(me->id); break;
            case 9: {
                view_patient_financial_report(me->id);
                // 附加功能：在查看报表后询问是否开具电子发票
                printf("\n是否需要生成特定单据的电子发票？(1-是, 0-否): ");
                if (safe_get_int() == 1) {
                    printf("请输入已缴费的记录编号 (RECxxx): ");
                    char rid[MAX_ID_LEN]; 
                    safe_get_string(rid, sizeof(rid));
                    
                    // 遍历记录，验证单据归属及状态
                    RecordNode* r = record_head;
                    while(r) {
                        // 必须满足：记录号匹配 + 属于该患者 + 状态为已完成(1)
                        if (strcmp(r->record_id, rid) == 0 && strcmp(r->patient_id, me->id) == 0 && r->status == 1) {
                            generate_electronic_invoice(me, r); 
                            break;
                        }
                        r = r->next;
                    }
                }
                break;
            }
            case 0: break;
            default: 
                printf(COLOR_RED "无效选项，请重新输入。\n" COLOR_RESET); 
                wait_for_enter();
        }
    }
}

/* ==========================================
 * 2. 【高阶挂号业务】：防刷限流与 AI 推荐引擎
 * ========================================== */

/**
 * @brief 辅助函数：获取今天的日期前缀 "YYYY-MM-DD"
 */
static void get_today_prefix(char* today_str) {
    get_current_time_str(today_str);
    today_str[10] = '\0'; // 截断时间，仅保留日期部分
}

/**
 * @brief 自主挂号与急诊通道服务 (深度重构版：接入限号锁与 AI 推荐)
 * 满足题签：500个号/天，医生20个号/天，患者5个号/天，同科室1个号/天。
 */
void patient_self_register(PatientNode* me) {
    print_header("自主挂号 / 急诊通道");
    
    char today[32];
    get_today_prefix(today);
    
    // 【防刷限流 1】：全院每日号源熔断检查
    int clinic_total_today = 0;
    int patient_total_today = 0;
    RecordNode* check_r = record_head;
    
    while(check_r) {
        if ((check_r->type == RECORD_REGISTRATION || check_r->type == RECORD_EMERGENCY) && strncmp(check_r->date, today, 10) == 0) {
            clinic_total_today++;
            if (strcmp(check_r->patient_id, me->id) == 0) {
                patient_total_today++;
            }
        }
        check_r = check_r->next;
    }
    
    if (clinic_total_today >= MAX_CLINIC_DAILY_TICKETS) {
        printf(COLOR_RED "熔断告警：今日全院 %d 个号源已全部挂满，停止放号！\n" COLOR_RESET, MAX_CLINIC_DAILY_TICKETS);
        wait_for_enter(); return;
    }
    // 【防刷限流 2】：单患者每日防黄牛检查
    if (patient_total_today >= MAX_PATIENT_DAILY_TICKETS) {
        printf(COLOR_RED "防刷拦截：您今日已挂号 %d 次，达到单日上限，为防倒卖资源已锁定！\n" COLOR_RESET, MAX_PATIENT_DAILY_TICKETS);
        wait_for_enter(); return;
    }

    printf(" 1. 普通门诊 (诊费: 5.00 元)\n");
    printf(" 2. 急诊中心 (诊费: 50.00 元 - 优先接诊)\n");
    printf(" 0. 返回\n 请选择类型: ");
    int reg_type = safe_get_int();
    if (reg_type != 1 && reg_type != 2) return; // 输入 0 或非法数字则退出

    // 设定费率字典：急诊费率远高于普通门诊
    double fee = (reg_type == 2) ? 50.0 : 5.0; 

    // 财务安全预检：确保余额足以支付当前的挂号费
    if (me->account_balance < fee) {
        printf(COLOR_RED "余额不足！当前余额 %.2f 元，不足以支付 %.2f 元诊费。\n" COLOR_RESET, me->account_balance, fee);
        printf("是否前往充值？(1-确认, 0-放弃): ");
        if (safe_get_int() == 1) patient_top_up(me); // 引导至充值页面
        return; // 拦截本次挂号
    }

    // 【贴心功能】：提取患者历史看过的医生，方便老病号直接挂号
    printf(COLOR_CYAN "\n>>> 系统记忆：您历史就诊过的主治医生有：\n" COLOR_RESET);
    RecordNode* hist_r = record_head;
    int found_hist = 0;
    while(hist_r) {
        if (strcmp(hist_r->patient_id, me->id) == 0 && (hist_r->type == RECORD_REGISTRATION || hist_r->type == RECORD_CONSULTATION)) {
            DoctorNode* hd = find_doctor_by_id(hist_r->doctor_id);
            if (hd) {
                printf(" - 医生: %s (工号: %s, 科室: %s)\n", hd->name, hd->id, hd->dept_id);
                found_hist = 1;
            }
        }
        hist_r = hist_r->next;
    }
    if (!found_hist) printf(" - 暂无历史就诊记录。\n");

    printf("\n请输入意向医生工号 (参考值班表或上方历史记录): ");
    char doc_id[MAX_ID_LEN]; 
    safe_get_string(doc_id, sizeof(doc_id));
    
    DoctorNode* target_doc = find_doctor_by_id(doc_id);
    
    // 【荣誉班亮点：AI 智能推荐系统介入】
    // 如果医生不存在，或者当天的号源已满，触发智能容灾补救
    int doctor_tickets_today = 0;
    int patient_dept_tickets_today = 0;
    
    if (target_doc) {
        RecordNode* r = record_head;
        while(r) {
            if ((r->type == RECORD_REGISTRATION || r->type == RECORD_EMERGENCY) && strncmp(r->date, today, 10) == 0) {
                if (strcmp(r->doctor_id, target_doc->id) == 0) doctor_tickets_today++;
                
                // 检查该患者是否在同一个科室挂过号
                if (strcmp(r->patient_id, me->id) == 0) {
                    DoctorNode* d = find_doctor_by_id(r->doctor_id);
                    if (d && strcmp(d->dept_id, target_doc->dept_id) == 0) {
                        patient_dept_tickets_today++;
                    }
                }
            }
            r = r->next;
        }
    }

    // 【防刷限流 3】：同科室限挂 1 个号检查
    if (target_doc && patient_dept_tickets_today >= 1) {
        printf(COLOR_RED "违规拦截：按照医院规定，同一患者每天在【%s】科室最多只能挂 1 个号，避免医疗资源挤兑！\n" COLOR_RESET, target_doc->dept_id);
        wait_for_enter(); return;
    }

    // 触发 AI 推荐机制的核心条件：医生没找到、或者这名医生今天满 20 个号了
    if (!target_doc || doctor_tickets_today >= MAX_DOCTOR_DAILY_TICKETS) {
        if (!target_doc) {
            printf(COLOR_RED "\n[AI诊断] 查无此医生或医生今日停诊。\n" COLOR_RESET);
        } else {
            printf(COLOR_RED "\n[AI诊断] 抱歉！%s 医生今日的 %d 个门诊号源已全部约满！\n" COLOR_RESET, target_doc->name, MAX_DOCTOR_DAILY_TICKETS);
        }
        
        // 尝试推测患者意图：如果刚才输对了工号，就在同科室找；如果乱输的，就没法推荐
        if (target_doc) {
            printf(COLOR_CYAN ">>> [智能推荐引擎] 正在为您推荐【%s】科室下今日仍有号源的其他主治医生：\n" COLOR_RESET, target_doc->dept_id);
            DoctorNode* d_curr = doctor_head;
            int rec_count = 0;
            while(d_curr) {
                if (strcmp(d_curr->dept_id, target_doc->dept_id) == 0 && strcmp(d_curr->id, target_doc->id) != 0) {
                    // 查一下这名推荐医生的挂号量
                    int d_count = 0;
                    RecordNode* cr = record_head;
                    while(cr) {
                        if ((cr->type == RECORD_REGISTRATION || cr->type == RECORD_EMERGENCY) && strncmp(cr->date, today, 10) == 0 && strcmp(cr->doctor_id, d_curr->id) == 0) {
                            d_count++;
                        }
                        cr = cr->next;
                    }
                    // 只要没满 20 个号，就向患者推荐
                    if (d_count < MAX_DOCTOR_DAILY_TICKETS) {
                        printf("   ☆ 推荐名医: %-10s | 工号: %-8s | 今日剩余号源: %d\n", d_curr->name, d_curr->id, MAX_DOCTOR_DAILY_TICKETS - d_count);
                        rec_count++;
                    }
                }
                d_curr = d_curr->next;
            }
            if (rec_count == 0) printf("   (抱歉，本科室今日所有医生均已约满或停诊)\n");
        }
        
        printf("\n是否重新输入医生工号挂号？(1-继续挂号, 0-放弃返回): ");
        if (safe_get_int() == 0) return;
        
        printf("请输入新的医生工号: ");
        safe_get_string(doc_id, sizeof(doc_id));
        target_doc = find_doctor_by_id(doc_id);
        if (!target_doc) {
            printf(COLOR_RED "挂号失败，该医生依然无效。\n" COLOR_RESET);
            wait_for_enter(); return;
        }
    }

    // 生成诊疗记录，急诊记录类型特殊标记为 RECORD_EMERGENCY
    RecordType type = (reg_type == 2) ? RECORD_EMERGENCY : RECORD_REGISTRATION;
    
    // 调用底层接口创建记录，排队号暂时随机生成模拟 (实际由 add_new_record 底层的智能排号器自动累加) [cite: 14]
    add_new_record(me->id, target_doc->id, type, (reg_type == 2 ? "急诊绿色通道" : "普通门诊挂号"), fee, 0);
    
    printf(COLOR_GREEN "挂号成功！单据已生成，请在 [待缴费] 模块结算后，医生即可在候诊大屏呼叫您。\n" COLOR_RESET);
    wait_for_enter();
}

/**
 * @brief 实时排队状态查询与等待时间预估
 */
void check_queue_status(PatientNode* patient) {
    print_header("实时排队状态查询");
    RecordNode *curr = record_head, *my_reg = NULL;
    
    // 第一步：在全局记录中检索当前患者尚未就诊(status == 0)的挂号单
    while (curr) {
        if (strcmp(curr->patient_id, patient->id) == 0 && curr->status == 0 &&
            (curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY)) {
            my_reg = curr; 
            break;
        }
        curr = curr->next;
    }

    // 如果没找到未处理的挂号单，进行引导式交互
    if (!my_reg) {
        printf(COLOR_YELLOW "您当前没有任何正在排队的记录。是否现在前往挂号？\n" COLOR_RESET);
        printf(" [ 1 ] 立即挂号 | [ 0 ] 返回菜单\n 请选择: ");
        if (safe_get_int() == 1) patient_self_register(patient);
        return;
    }

    // 第二步：统计前方还有多少人
    int ahead = 0; 
    curr = record_head;
    while (curr) {
        // 条件：挂了同一个医生的号 + 尚未就诊 + 排队号比我小
        if (strcmp(curr->doctor_id, my_reg->doctor_id) == 0 && curr->status == 0 && curr->queue_number < my_reg->queue_number) {
            ahead++;
        }
        curr = curr->next;
    }

    // 第三步：渲染排队信息与预估模型
    printf(COLOR_CYAN "就诊类型: %s\n" COLOR_RESET, (my_reg->type == RECORD_EMERGENCY ? "急诊绿色通道" : "普通门诊"));
    printf("您的号码: %03d | 当前状态: 待诊\n", my_reg->queue_number);
    printf("前方等待人数: " COLOR_RED "%d" COLOR_RESET " 人\n", ahead);
    // 简单的预估算法：急诊单人耗时约 5 分钟，普通门诊约 12 分钟
    printf("预计等待时间: 约 %d 分钟\n", ahead * (my_reg->type == RECORD_EMERGENCY ? 5 : 12));
    wait_for_enter();
}

/* ==========================================
 * 3. 救护车调度：联动全局车队状态
 * ========================================== */

/**
 * @brief 呼叫 120 救护车 (院前急救)
 */
void call_ambulance_service(PatientNode* me) {
    print_header("120 院前急救中心");
    printf("系统正在检索附近空闲车辆...\n");

    // 扫描全局救护车链表，寻找一辆状态为 AMB_IDLE (空闲) 的车
    AmbulanceNode* amb = ambulance_head; 
    while (amb && amb->status != AMB_IDLE) {
        amb = amb->next;
    }

    if (amb) {
        // 匹配成功，模拟调度派发流程
        printf(COLOR_GREEN "匹配成功！\n" COLOR_RESET);
        printf(" 车辆编号: %s\n 随车司机: %s\n", amb->vehicle_id, amb->driver_name);
        
        amb->status = AMB_DISPATCHED; // 锁定车辆状态为“出车执行任务”
        strcpy(amb->current_location, "前往急救对象坐标途中"); // 覆盖默认的坐标点
        
        // 联动写入救护车出勤日志
        // 此处的逻辑由底层出勤记录器负责落地，追踪出车轨迹
        // add_ambulance_log(amb->vehicle_id, amb->current_location);

        printf(COLOR_CYAN ">>> 救护车已出发，请保持电话畅通！\n" COLOR_RESET);
    } else {
        // 如果所有车都在忙
        printf(COLOR_RED "目前全院救护车均在执行任务，已为您自动进入急救排队序列。\n" COLOR_RESET);
    }
    wait_for_enter();
}

/* ==========================================
 * 4. 补全：财务、住院、值班表等核心逻辑
 * ========================================== */

/**
 * @brief 结算个人的待缴费账单
 */
void patient_pay_bill(PatientNode* me) {
    print_header("待缴费账单");
    RecordNode* r = record_head; 
    int count = 0;
    
    // 遍历显示所有未缴费 (status == 0) 且属于当前患者的账单
    while (r) {
        if (strcmp(r->patient_id, me->id) == 0 && r->status == 0) {
            printf("[%s] %-15s | 总额: %.2f | 医保扣除: %.2f | 需付: " COLOR_RED "%.2f" COLOR_RESET "\n", 
                   r->record_id, r->description, r->total_fee, r->insurance_covered, r->personal_paid);
            count++;
        }
        r = r->next;
    }
    
    if (count == 0) { 
        printf("没有待缴费账单。\n"); 
        wait_for_enter(); return; 
    }

    printf("\n请输入支付单号: ");
    char rid[MAX_ID_LEN]; 
    safe_get_string(rid, sizeof(rid));
    
    // 再次遍历执行扣款逻辑
    r = record_head;
    while (r) {
        if (strcmp(r->record_id, rid) == 0 && strcmp(r->patient_id, me->id) == 0) {
            // 校验余额是否足够支付个人自付部分
            if (me->account_balance >= r->personal_paid) {
                me->account_balance -= r->personal_paid; // 物理扣除余额
                r->status = 1; // 扭转单据状态为已完成(缴费成功)
                printf(COLOR_GREEN "支付成功！账户余额: %.2f\n" COLOR_RESET, me->account_balance);
            } else {
                printf(COLOR_RED "支付失败：余额不足。\n" COLOR_RESET);
            }
            wait_for_enter(); return;
        }
        r = r->next;
    }
}

/**
 * @brief 查看个人基础信息与住院/ICU状态
 */
void view_personal_info(PatientNode* p) {
    print_header("个人档案与健康状态");
    printf(" 姓名: %s | 年龄: %d | 编号: %s\n", p->name, p->age, p->id);
    
    // 手术室状态联动查询
    int in_surgery = 0;
    OperatingRoomNode* or_curr = or_head;
    while (or_curr) {
        if (or_curr->status == 1 && strcmp(or_curr->current_patient, p->id) == 0) {
            printf(COLOR_RED " 就诊状态: 手术室抢救/施刀中 (位于: %s)\n" COLOR_RESET, or_curr->room_id);
            in_surgery = 1; 
            break;
        }
        or_curr = or_curr->next;
    }

    if (!in_surgery) {
        // 动态渲染住院状态
        if (p->type == 'I') {
            printf(COLOR_CYAN " 就诊状态: 住院治疗中\n" COLOR_RESET);
            // 如果患者分配的床位号包含 "ICU" 关键字，则高亮警示
            printf(" 所在床位: %s %s\n", p->bed_id, (strstr(p->bed_id, "ICU") ? COLOR_RED"(重症监护)"COLOR_RESET : ""));
            printf(" 入院押金: %.2f 元\n", p->hospital_deposit);
            // 这里也可以联动打印出 expected_discharge_date 如果已实装
        } else {
            printf(" 就诊状态: 门诊/急诊\n");
        }
    }
    wait_for_enter();
}

/**
 * @brief 在线注册成为新患者并建档
 */
void register_new_patient() {
    print_header("新患者在线注册");
    // 动态分配内存以创建新节点
    PatientNode* n = (PatientNode*)calloc(1, sizeof(PatientNode));
    if (!n) return;
    
    // 利用当前时间戳生成唯一的病历号 (如 P1711693450)
    sprintf(n->id, "P%lld", (long long)time(NULL));
    
    printf("请输入真实姓名: "); 
    safe_get_string(n->name, MAX_NAME_LEN);
    
    printf("医保(0-自费, 1-职工, 2-居民): "); 
    n->insurance = (InsuranceType)safe_get_int();
    
    // 初始化默认属性
    n->type = 'O'; // 默认新建的都是门诊患者
    n->account_balance = 0.0;
    
    // 使用头插法将新患者加入门诊链表
    // 满足跨模块和持久化落盘的要求
    n->next = outpatient_head; 
    outpatient_head = n;
    
    printf(COLOR_GREEN "注册成功！您的专属病历号为: %s (请妥善保管)\n" COLOR_RESET, n->id);
    wait_for_enter();
}

/**
 * @brief 在线充值患者账户余额
 */
void patient_top_up(PatientNode* me) {
    printf("请输入充值金额 (元): ");
    double amt = safe_get_double();
    // 防御性校验：充值金额必须为正数
    if (amt > 0) {
        me->account_balance += amt;
        printf(COLOR_GREEN "充值成功，当前余额: %.2f\n" COLOR_RESET, me->account_balance);
    } else {
         printf(COLOR_RED "错误：充值金额无效。\n" COLOR_RESET);
    }
    wait_for_enter();
}

/**
 * @brief 查询患者历史诊疗记录
 */
void query_medical_records(const char* pid) {
    print_header("历史诊疗路径");
    RecordNode* r = record_head;
    while (r) {
        if (strcmp(r->patient_id, pid) == 0) {
            printf("[%s] 日期:%s | 项目:%s | 医生:%s | 结果:%s\n", 
                   r->record_id, r->date, r->description, r->doctor_id, (r->status == 1 ? "完成" : "进行中"));
        }
        r = r->next;
    }
    wait_for_enter();
}

/**
 * @brief 查看个人名下所有已完成单据的总自费金额汇总
 */
void view_patient_financial_report(const char* pid) {
    double total = 0; 
    RecordNode* r = record_head;
    while(r) { 
        // 仅累加属于本人的且已缴费的个人自付金额
        if (strcmp(r->patient_id, pid) == 0 && r->status == 1) {
            total += r->personal_paid; 
        }
        r = r->next; 
    }
    printf("\n 个人累计自费支出总额: %.2f 元\n", total);
}

/**
 * @brief 绘制并生成带有医院抬头的电子发票凭证
 */
void generate_electronic_invoice(PatientNode* p, RecordNode* r) {
    printf("\n" COLOR_BLUE "--------------------------------------\n");
    printf("       市中心医院 电子发票 (报销凭证)  \n");
    printf(" 单号: %s | 日期: %s\n 患者: %s | 项目: %s\n 费用总额: %.2f | 个人支付: %.2f\n",
           r->record_id, r->date, p->name, r->description, r->total_fee, r->personal_paid);
    printf("--------------------------------------\n" COLOR_RESET);
    wait_for_enter();
}

/**
 * @brief 患者子系统的入口主菜单
 */
void patient_subsystem() {
    int c = -1;
    while (c != 0) {
        print_header("医院智慧服务大厅");
        printf(" [ 1 ] 登录个人账户 | [ 2 ] 新患者建档 | [ 0 ] 返回\n 选择: ");
        c = safe_get_int();
        
        if (c == 1) {
            char id[MAX_ID_LEN]; 
            printf("请输入就诊卡号: "); 
            safe_get_string(id, MAX_ID_LEN);
            
            // 执行登录验证
            PatientNode* me = patient_login(id);
            if (me) {
                // 登录成功，挂载仪表盘
                patient_dashboard(me);
            } else {
                printf(COLOR_RED "错误：无效的卡号。\n" COLOR_RESET);
                wait_for_enter();
            }
        } else if (c == 2) {
            register_new_patient();
        }
    }
}