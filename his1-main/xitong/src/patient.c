#include "patient.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ==========================================
 * 1. 身份识别与登录门户
 * ========================================== */

// 登录校验：同时检索门诊与住院数据库
PatientNode* patient_login(const char* input_id) {
    PatientNode* p = find_outpatient_by_id(input_id);
    if (p != NULL) return p;
    return find_inpatient_by_id(input_id); // 确保住院患者也能正常登录工作台
}

// 患者主控台：整合急救、挂号、财务、发票
void patient_dashboard(PatientNode* me) {
    int choice = -1;
    while (choice != 0) {
        clear_input_buffer();
        printf("\n" COLOR_BLUE "=========================================\n");
        printf("  患者服务中心 | 登录者: %s %s\n", me->name, (me->type == 'O' ? "(门诊)" : "(住院)"));
        printf("  账户余额: " COLOR_GREEN "%.2f" COLOR_RESET " 元 | 就诊卡号: %s\n", me->account_balance, me->id);
        printf("=========================================" COLOR_RESET "\n");
        printf("  [ 1 ] 我的个人资料与住院/ICU状态\n");
        printf("  [ 2 ] 预约挂号 / 急诊绿色通道\n"); 
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
        
        switch (choice) {
            case 1: view_personal_info(me); break;
            case 2: patient_self_register(me); break;
            case 3: check_queue_status(me); break;
            case 4: patient_pay_bill(me); break;
            case 5: patient_top_up(me); break;
            case 6: call_ambulance_service(me); break;
            case 7: view_duty_schedule(NULL); break;
            case 8: query_medical_records(me->id); break;
            case 9: {
                view_patient_financial_report(me->id);
                printf("\n是否需要生成特定单据的电子发票？(1-是, 0-否): ");
                if (safe_get_int() == 1) {
                    printf("请输入已缴费的记录编号 (RECxxx): ");
                    char rid[MAX_ID_LEN]; safe_get_string(rid, sizeof(rid));
                    RecordNode* r = record_head;
                    while(r) {
                        if (strcmp(r->record_id, rid) == 0 && strcmp(r->patient_id, me->id) == 0 && r->status == 1) {
                            generate_electronic_invoice(me, r); break;
                        }
                        r = r->next;
                    }
                }
                break;
            }
            case 0: break;
            default: printf(COLOR_RED "无效选项，请重新输入。\n" COLOR_RESET); wait_for_enter();
        }
    }
}

/* ==========================================
 * 2. 挂号业务：支持急诊与自动引导
 * ========================================== */

void patient_self_register(PatientNode* me) {
    print_header("自主挂号 / 急诊通道");
    printf(" 1. 普通门诊 (诊费: 5.00 元)\n");
    printf(" 2. 急诊中心 (诊费: 50.00 元 - 优先接诊)\n");
    printf(" 0. 返回\n 请选择类型: ");
    int reg_type = safe_get_int();
    if (reg_type != 1 && reg_type != 2) return;

    double fee = (reg_type == 2) ? 50.0 : 5.0; // 急诊费率高于普通门诊

    // 财务安全校验：确保余额足以支付挂号费
    if (me->account_balance < fee) {
        printf(COLOR_RED "余额不足！当前余额 %.2f 元，不足以支付 %.2f 元诊费。\n" COLOR_RESET, me->account_balance, fee);
        printf("是否前往充值？(1-确认, 0-放弃): ");
        if (safe_get_int() == 1) patient_top_up(me);
        return;
    }

    printf("请输入医生工号 (参考值班表): ");
    char doc_id[MAX_ID_LEN]; safe_get_string(doc_id, sizeof(doc_id));
    
    if (find_doctor_by_id(doc_id) == NULL) {
        printf(COLOR_RED "该医生今日不出诊或不存在，请核对。\n" COLOR_RESET);
        wait_for_enter(); return;
    }

    // 生成记录，急诊记录类型为 RECORD_EMERGENCY (5)
    RecordType type = (reg_type == 2) ? RECORD_EMERGENCY : RECORD_REGISTRATION;
    add_new_record(me->id, doc_id, type, (reg_type == 2 ? "急诊绿色通道" : "普通门诊挂号"), fee, rand()%500+1);
    
    printf(COLOR_GREEN "挂号成功！单据已生成，请在 [待缴费] 模块结算后，医生即可呼叫您。\n" COLOR_RESET);
    wait_for_enter();
}

void check_queue_status(PatientNode* patient) {
    print_header("实时排队状态查询");
    RecordNode *curr = record_head, *my_reg = NULL;
    
    // 检索名下所有未处理的挂号或急诊单
    while (curr) {
        if (strcmp(curr->patient_id, patient->id) == 0 && curr->status == 0 &&
            (curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY)) {
            my_reg = curr; break;
        }
        curr = curr->next;
    }

    // 核心遗漏补全：引导式入口
    if (!my_reg) {
        printf(COLOR_YELLOW "您当前没有任何正在排队的记录。是否现在前往挂号？\n" COLOR_RESET);
        printf(" [ 1 ] 立即挂号 | [ 0 ] 返回菜单\n 请选择: ");
        if (safe_get_int() == 1) patient_self_register(patient);
        return;
    }

    // 统计前方人数
    int ahead = 0; curr = record_head;
    while (curr) {
        if (strcmp(curr->doctor_id, my_reg->doctor_id) == 0 && curr->status == 0 && curr->queue_number < my_reg->queue_number) {
            ahead++;
        }
        curr = curr->next;
    }

    printf(COLOR_CYAN "就诊类型: %s\n" COLOR_RESET, (my_reg->type == RECORD_EMERGENCY ? "急诊绿色通道" : "普通门诊"));
    printf("您的号码: %03d | 当前状态: 待诊\n", my_reg->queue_number);
    printf("前方等待人数: " COLOR_RED "%d" COLOR_RESET " 人\n", ahead);
    // 急诊预估时间更短
    printf("预计等待时间: 约 %d 分钟\n", ahead * (my_reg->type == RECORD_EMERGENCY ? 5 : 12));
    wait_for_enter();
}

/* ==========================================
 * 3. 救护车调度：联动全局车队状态
 * ========================================== */

void call_ambulance_service(PatientNode* me) {
    print_header("120 院前急救中心");
    printf("系统正在检索附近空闲车辆...\n");

    // 真正修改全局救护车链表状态
    AmbulanceNode* amb = ambulance_head; 
    while (amb && amb->status != AMB_IDLE) amb = amb->next;

    if (amb) {
        printf(COLOR_GREEN "匹配成功！\n" COLOR_RESET);
        printf(" 车辆编号: %s\n 随车司机: %s\n", amb->vehicle_id, amb->driver_name);
        amb->status = AMB_DISPATCHED; // 锁定车辆
        strcpy(amb->current_location, "前往急救对象坐标途中");
        printf(COLOR_CYAN ">>> 救护车已出发，请保持电话畅通！\n" COLOR_RESET);
    } else {
        printf(COLOR_RED "目前全院救护车均在执行任务，已为您自动进入急救排队序列。\n" COLOR_RESET);
    }
    wait_for_enter();
}

/* ==========================================
 * 4. 补全：财务、住院、值班表等核心逻辑
 * ========================================== */

void patient_pay_bill(PatientNode* me) {
    print_header("待缴费账单");
    RecordNode* r = record_head; int count = 0;
    while (r) {
        if (strcmp(r->patient_id, me->id) == 0 && r->status == 0) {
            printf("[%s] %-15s | 总额: %.2f | 医保扣除: %.2f | 需付: " COLOR_RED "%.2f" COLOR_RESET "\n", 
                   r->record_id, r->description, r->total_fee, r->insurance_covered, r->personal_paid);
            count++;
        }
        r = r->next;
    }
    if (count == 0) { printf("没有待缴费账单。\n"); wait_for_enter(); return; }

    printf("\n请输入支付单号: ");
    char rid[MAX_ID_LEN]; safe_get_string(rid, sizeof(rid));
    r = record_head;
    while (r) {
        if (strcmp(r->record_id, rid) == 0 && strcmp(r->patient_id, me->id) == 0) {
            if (me->account_balance >= r->personal_paid) {
                me->account_balance -= r->personal_paid;
                r->status = 1; // 缴费完成
                printf(COLOR_GREEN "支付成功！账户余额: %.2f\n" COLOR_RESET, me->account_balance);
            } else printf(COLOR_RED "支付失败：余额不足。\n" COLOR_RESET);
            wait_for_enter(); return;
        }
        r = r->next;
    }
}

void view_personal_info(PatientNode* p) {
    print_header("个人档案与健康状态");
    printf(" 姓名: %s | 年龄: %d | 编号: %s\n", p->name, p->age, p->id);
    if (p->type == 'I') {
        printf(COLOR_CYAN " 就诊状态: 住院治疗中\n" COLOR_RESET);
        // 体现 ICU 状态：如果床位号包含 ICU 字样或根据系统分配
        printf(" 所在床位: %s %s\n", p->bed_id, (strstr(p->bed_id, "ICU") ? COLOR_RED"(重症监护)"COLOR_RESET : ""));
        printf(" 入院押金: %.2f 元\n", p->hospital_deposit);
    } else {
        printf(" 就诊状态: 门诊/急诊\n");
    }
    wait_for_enter();
}

void register_new_patient() {
    print_header("新患者在线注册");
    PatientNode* n = (PatientNode*)calloc(1, sizeof(PatientNode));
    if (!n) return;
    sprintf(n->id, "P%lld", (long long)time(NULL));
    printf("请输入真实姓名: "); safe_get_string(n->name, MAX_NAME_LEN);
    printf("医保(0-自费, 1-职工, 2-居民): "); n->insurance = (InsuranceType)safe_get_int();
    n->type = 'O'; n->account_balance = 0.0;
    n->next = outpatient_head; outpatient_head = n;
    printf(COLOR_GREEN "注册成功！您的专属病历号为: %s (请妥善保管)\n" COLOR_RESET, n->id);
    wait_for_enter();
}

void patient_top_up(PatientNode* me) {
    printf("请输入充值金额 (元): ");
    double amt = safe_get_double();
    if (amt > 0) {
        me->account_balance += amt;
        printf(COLOR_GREEN "充值成功，当前余额: %.2f\n" COLOR_RESET, me->account_balance);
    }
    wait_for_enter();
}

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

void view_patient_financial_report(const char* pid) {
    double total = 0; RecordNode* r = record_head;
    while(r) { 
        if (strcmp(r->patient_id, pid) == 0 && r->status == 1) total += r->personal_paid; 
        r = r->next; 
    }
    printf("\n 个人累计自费支出总额: %.2f 元\n", total);
}

void generate_electronic_invoice(PatientNode* p, RecordNode* r) {
    printf("\n" COLOR_BLUE "--------------------------------------\n");
    printf("       市中心医院 电子发票 (报销凭证)  \n");
    printf(" 单号: %s | 日期: %s\n 患者: %s | 项目: %s\n 费用总额: %.2f | 个人支付: %.2f\n",
           r->record_id, r->date, p->name, r->description, r->total_fee, r->personal_paid);
    printf("--------------------------------------\n" COLOR_RESET);
}

void patient_subsystem() {
    int c = -1;
    while (c != 0) {
        print_header("医院智慧服务大厅");
        printf(" [ 1 ] 登录个人账户 | [ 2 ] 新患者建档 | [ 0 ] 返回\n 选择: ");
        c = safe_get_int();
        if (c == 1) {
            char id[MAX_ID_LEN]; printf("请输入就诊卡号: "); safe_get_string(id, MAX_ID_LEN);
            PatientNode* me = patient_login(id);
            if (me) patient_dashboard(me);
            else printf(COLOR_RED "错误：无效的卡号。\n" COLOR_RESET);
        } else if (c == 2) register_new_patient();
    }
}