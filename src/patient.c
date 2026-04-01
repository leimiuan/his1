#include "patient.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ==========================================
 * 1. 核心入口与身份鉴权
 * ========================================== */

// 模拟患者登录
PatientNode* patient_login(const char* input_id) {
    // 先在门诊链表里找
    PatientNode* p = find_outpatient_by_id(input_id);
    if (p != NULL) return p;
    
    // 如果门诊没有，再去住院链表里找
    p = find_inpatient_by_id(input_id);
    return p;
}

// 登录后的患者个人工作台
void patient_dashboard(PatientNode* me) {
    int choice = -1;
    while (choice != 0) {
        clear_input_buffer();
        printf("\n=========================================\n");
        printf(COLOR_GREEN "  欢迎登录 HIS 系统，%s %s\n" COLOR_RESET, 
               me->name, (me->type == 'O' ? "(门诊)" : "(住院)"));
        printf("  当前账户余额: " COLOR_GREEN "%.2f" COLOR_RESET " 元\n", me->account_balance);
        printf("=========================================\n");
        printf("  [ 1 ] 查看个人信息\n");
        printf("  [ 2 ] 查询历史就诊记录\n");
        printf("  [ 3 ] 待缴费单据 / 自主缴费\n");  // 新增
        printf("  [ 4 ] 账户在线充值\n");            // 新增
        printf("  [ 5 ] 查看排队状态\n");
        printf("  [ 6 ] 查看个人财务对账单\n");
        printf("  [ 7 ] 生成电子发票\n");
        printf("  [ 8 ] 查看值班表\n");
        printf(COLOR_RED "  [ 0 ] 退出登录\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请选择服务项目 (0-8): " COLOR_RESET);
        choice = safe_get_int();
        
        switch (choice) {
            case 1: view_personal_info(me); break;
            case 2: query_medical_records(me->id); break;
            case 3: patient_pay_bill(me); break;     // 调起缴费模块
            case 4: patient_top_up(me); break;       // 调起充值模块
            case 5: check_queue_status(me); break;
            case 6: view_patient_financial_report(me->id); break;
            case 7: {
                printf("请输入要开发票的已缴费记录编号 (例如: RECxxx): ");
                char rec_id[MAX_ID_LEN];
                safe_get_string(rec_id, sizeof(rec_id));
                
                RecordNode* curr = record_head;
                RecordNode* target = NULL;
                while (curr != NULL) {
                    if (strcmp(curr->record_id, rec_id) == 0 && strcmp(curr->patient_id, me->id) == 0) {
                        target = curr;
                        break;
                    }
                    curr = curr->next;
                }
                
                if (target) {
                    if (target->status == 0) {
                        printf(COLOR_RED "该单据尚未缴费，无法生成发票！\n" COLOR_RESET);
                    } else {
                        generate_electronic_invoice(me, target);
                    }
                } else {
                    printf(COLOR_RED "未找到记录 %s 或无权访问。\n" COLOR_RESET, rec_id);
                }
                wait_for_enter();
                break;
            }
            case 8: view_duty_schedule(NULL); break;
            case 0: break;
            default: printf(COLOR_RED "无效的选择。\n" COLOR_RESET); wait_for_enter();
        }
    }
}

// 新患者建档 (注册)
void register_new_patient() {
    print_header("新患者建档 (注册)");
    
    PatientNode* new_node = (PatientNode*)calloc(1, sizeof(PatientNode));
    if (!new_node) {
        printf(COLOR_RED "系统内存不足，注册失败！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 自动生成唯一就诊卡号
    sprintf(new_node->id, "P%lld%03d", (long long)time(NULL), rand() % 1000);

    printf("请输入真实姓名: ");
    safe_get_string(new_node->name, sizeof(new_node->name));

    printf("请输入年龄: ");
    new_node->age = safe_get_int();

    printf("请输入性别 (男/女): ");
    safe_get_string(new_node->gender, sizeof(new_node->gender));

    printf("请选择医保类型 (0-自费, 1-职工医保, 2-居民医保): ");
    int ins = safe_get_int();
    new_node->insurance = (InsuranceType)(ins >= 0 && ins <= 2 ? ins : 0);

    // 初始化基础属性
    new_node->type = 'O'; 
    new_node->account_balance = 0.0;
    new_node->hospital_deposit = 0.0;
    strcpy(new_node->bed_id, "");

    // 头插法加入门诊链表
    new_node->next = outpatient_head;
    outpatient_head = new_node;

    printf(COLOR_GREEN "\n>>> 建档成功！ <<<\n" COLOR_RESET);
    printf(COLOR_YELLOW "您的专属就诊卡号是: " COLOR_RED "%s\n" COLOR_RESET, new_node->id);
    printf(COLOR_CYAN "提示：请务必牢记此编号，下次需凭此编号登录和就诊。\n" COLOR_RESET);
    wait_for_enter();
}

// 患者端入口门面
void patient_subsystem() {
    int portal_choice = -1;
    while (portal_choice != 0) {
        clear_input_buffer();
        print_header("患者服务大厅");
        printf("  [ 1 ] 登录已有账号\n");
        printf("  [ 2 ] 新患者建档 (注册)\n");
        printf(COLOR_RED "  [ 0 ] 返回系统主菜单\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请选择 (0-2): " COLOR_RESET);
        portal_choice = safe_get_int();
        
        if (portal_choice == 1) {
            char id[MAX_ID_LEN];
            printf(COLOR_YELLOW "请输入患者编号 (就诊卡号) 以登录: " COLOR_RESET);
            safe_get_string(id, sizeof(id));
            
            PatientNode* me = patient_login(id);
            if (me == NULL) {
                printf(COLOR_RED "\n[登录失败] 未找到该患者编号！请检查是否输入正确或先进行注册。\n" COLOR_RESET);
                wait_for_enter();
            } else {
                patient_dashboard(me); 
            }
        } 
        else if (portal_choice == 2) {
            register_new_patient();
        } 
        else if (portal_choice == 0) {
            break;
        } 
        else {
            printf(COLOR_RED "无效的选择。\n" COLOR_RESET);
            wait_for_enter();
        }
    }
}

/* ==========================================
 * 2. 新增核心功能：充值与缴费
 * ========================================== */

// 账户在线充值
void patient_top_up(PatientNode* me) {
    print_header("账户充值中心");
    printf(" 当前可用余额: %.2f 元\n", me->account_balance);
    printf(" 请输入充值金额: ");
    double amt = safe_get_double();
    
    if (amt > 0) {
        me->account_balance += amt;
        printf(COLOR_GREEN " >>> 充值成功！新余额: %.2f 元\n" COLOR_RESET, me->account_balance);
    } else {
        printf(COLOR_RED " 充值金额必须大于 0！\n" COLOR_RESET);
    }
    wait_for_enter();
}

// 自主缴费与报销核销
void patient_pay_bill(PatientNode* me) {
    print_header("待缴费单据清单");
    RecordNode* curr = record_head;
    int count = 0;
    
    while (curr) {
        if (strcmp(curr->patient_id, me->id) == 0 && curr->status == 0) {
            printf(" [%s] 项目: %-15s | 总额: %.2f | 医保抵扣: %.2f | 需自付: " COLOR_RED "%.2f" COLOR_RESET "\n", 
                   curr->record_id, curr->description, curr->total_fee, 
                   curr->insurance_covered, curr->personal_paid);
            count++;
        }
        curr = curr->next;
    }
    
    if (count == 0) { 
        printf(COLOR_GREEN " 暂无待缴费项目，您的账单已全部结清。\n" COLOR_RESET); 
        wait_for_enter(); 
        return; 
    }

    printf("\n 请输入要缴纳的单号 (输入 0 返回): ");
    char tid[MAX_ID_LEN];
    safe_get_string(tid, sizeof(tid));
    if (strcmp(tid, "0") == 0) return;

    curr = record_head;
    while (curr) {
        if (strcmp(curr->record_id, tid) == 0 && strcmp(curr->patient_id, me->id) == 0) {
            if (me->account_balance >= curr->personal_paid) {
                me->account_balance -= curr->personal_paid;
                curr->status = 1; // 标记为已缴费
                printf(COLOR_GREEN " >>> 支付成功！单据 %s 已结清。剩余余额: %.2f\n" COLOR_RESET, 
                       tid, me->account_balance);
            } else { 
                printf(COLOR_RED " >>> 余额不足，请先充值！(需自付: %.2f, 当前余额: %.2f)\n" COLOR_RESET, 
                       curr->personal_paid, me->account_balance); 
            }
            wait_for_enter(); 
            return;
        }
        curr = curr->next;
    }
    printf(COLOR_RED " 未找到对应单据。\n" COLOR_RESET);
    wait_for_enter();
}

/* ==========================================
 * 3. 基础信息与记录查询 
 * ========================================== */

void view_personal_info(PatientNode* patient) {
    print_header("个人信息");
    printf(" 姓名: %-15s 年龄: %d\n", patient->name, patient->age);
    printf(" 性别: %-13s 编号: %s\n", patient->gender, patient->id);
    
    const char* ins_str = "自费";
    if (patient->insurance == INSURANCE_WORKER) ins_str = "职工医保";
    else if (patient->insurance == INSURANCE_RESIDENT) ins_str = "居民医保";
    printf(" 医保类型: %s\n", ins_str);
    
    if (patient->type == 'O') {
        printf(" 状态: 门诊\n");
    } else {
        printf(" 状态: 住院\n");
        printf(" 入院日期: %s\n", patient->admission_date);
        printf(" 床位号: %s\n", (strlen(patient->bed_id) > 0 ? patient->bed_id : "未分配"));
        printf(" 住院押金: %.2f 元\n", patient->hospital_deposit);
    }
    print_divider();
    wait_for_enter();
}

void query_medical_records(const char* patient_id) {
    print_header("历史就诊记录");
    RecordNode* curr = record_head;
    int count = 0;
    
    while (curr != NULL) {
        if (strcmp(curr->patient_id, patient_id) == 0) {
            printf("\n记录编号: %s \n", curr->record_id);
            const char* type_str = "未知";
            if (curr->type == RECORD_REGISTRATION) type_str = "挂号";
            else if (curr->type == RECORD_CONSULTATION) type_str = "问诊/处方";
            else if (curr->type == RECORD_EXAMINATION) type_str = "检查";
            else if (curr->type == RECORD_HOSPITALIZATION) type_str = "住院";
            
            printf(" 类型: %-15s | 医生编号: %s\n", type_str, curr->doctor_id);
            printf(" 描述: %s\n", curr->description);
            printf(" 费用: %.2f 元 | 状态: %s\n", curr->total_fee, (curr->status == 1 ? COLOR_GREEN"已缴费"COLOR_RESET : COLOR_RED"待缴费"COLOR_RESET));
            count++;
        }
        curr = curr->next;
    }
    if (count == 0) printf(COLOR_YELLOW "未找到就诊记录。\n" COLOR_RESET);
    print_divider();
    wait_for_enter();
}

void check_queue_status(PatientNode* patient) {
    print_header("排队状态");
    RecordNode* my_reg = NULL;
    RecordNode* curr = record_head;
    
    while (curr != NULL) {
        if (strcmp(curr->patient_id, patient->id) == 0 && 
            curr->type == RECORD_REGISTRATION && 
            curr->status == 0) {
            if (my_reg == NULL || curr->queue_number < my_reg->queue_number) {
                my_reg = curr;
            }
        }
        curr = curr->next;
    }
    
    if (my_reg == NULL) {
        printf(COLOR_YELLOW "未找到正在等待的挂号记录。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }
    
    int people_ahead = 0;
    curr = record_head;
    while (curr != NULL) {
        if (strcmp(curr->doctor_id, my_reg->doctor_id) == 0 &&
            curr->type == RECORD_REGISTRATION &&
            curr->status == 0 &&
            curr->queue_number < my_reg->queue_number) {
            people_ahead++;
        }
        curr = curr->next;
    }
    
    printf(COLOR_CYAN "您的排号: %d\n" COLOR_RESET, my_reg->queue_number);
    printf("医生编号: %s\n", my_reg->doctor_id);
    printf("前方等待人数: " COLOR_RED "%d" COLOR_RESET "\n", people_ahead);
    printf("预计等待时间: 约 %d 分钟\n", people_ahead * 10);
    print_divider();
    wait_for_enter();
}

/* ==========================================
 * 4. 财务与发票 (已适配新版数据结构)
 * ========================================== */

void generate_electronic_invoice(PatientNode* patient, RecordNode* record) {
    const char* ins_name = "自费";
    if (patient->insurance == INSURANCE_WORKER) ins_name = "职工医保";
    else if (patient->insurance == INSURANCE_RESIDENT) ins_name = "居民医保";

    printf("\n" COLOR_BLUE "======================================================\n");
    printf("                 市中心医院电子发票           \n");
    printf("------------------------------------------------------\n" COLOR_RESET);
    printf(" 发票代码: %s\n", record->record_id);
    printf(" 患者:     %-15s 医保类型: %s\n", patient->name, ins_name);
    printf(" 项目明细: %s\n", record->description);
    printf(COLOR_BLUE "------------------------------------------------------\n" COLOR_RESET);
    printf(" 总金额:   %.2f 元\n", record->total_fee);
    printf(" 医保统筹: %.2f 元\n", record->insurance_covered);
    printf(" 个人自付: " COLOR_RED "%.2f" COLOR_RESET " 元 (已结清)\n", record->personal_paid);
    printf(COLOR_BLUE "======================================================\n" COLOR_RESET);

    // 写入文本文件作为票据存档
    char filename[256];
    sprintf(filename, "invoice_%s.txt", record->record_id);
    FILE* fp = fopen(filename, "w");
    if (fp) {
        fprintf(fp, "发票代码: %s\n患者: %s\n项目: %s\n总金额: %.2f\n医保统筹: %.2f\n个人自付: %.2f\n状态: 已缴费\n", 
                record->record_id, patient->name, record->description, record->total_fee, record->insurance_covered, record->personal_paid);
        fclose(fp);
        printf(COLOR_GREEN " -> 电子发票已存档至本地: %s\n" COLOR_RESET, filename);
    }
}

void view_patient_financial_report(const char* patient_id) {
    print_header("个人财务对账单 (仅统计已结清项目)");
    double sum_total = 0, sum_ins = 0, sum_personal = 0;
    RecordNode* curr = record_head;
    
    while (curr) {
        // 仅统计 status == 1 (已缴费) 的记录
        if (strcmp(curr->patient_id, patient_id) == 0 && curr->status == 1) {
            sum_total += curr->total_fee;
            sum_ins += curr->insurance_covered;
            sum_personal += curr->personal_paid;
        }
        curr = curr->next;
    }
    
    printf(" 累计产生费用 (总额): %.2f 元\n", sum_total);
    printf(" 医保累计统筹 (报销): %.2f 元\n", sum_ins);
    printf(" 个人实际支付 (自费): %.2f 元\n", sum_personal);
    print_divider();
    wait_for_enter();
}