#include "common.h"
#include "data_manager.h"
#include "admin.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==========================================
 * 1. 核心入口与权限验证
 * ========================================== */

void admin_subsystem() {
    clear_input_buffer();
    print_header("系统管理与决策支持中心");
    
    printf("请输入管理员授权密码：");
    char pwd[32];
    safe_get_string(pwd, sizeof(pwd));
    
    // 建议：生产环境应从配置文件读取密码
    if (strcmp(pwd, "admin123") != 0) {
        printf(COLOR_RED "密码错误，认证失败！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    int choice = -1;
    while (choice != 0) {
        clear_input_buffer();
        printf("\n=========================================\n");
        printf(COLOR_GREEN "  管理员已登录 (状态：已授权)\n" COLOR_RESET);
        printf("=========================================\n");
        printf("  --- 人事与资源 ---\n");
        printf("  [ 1 ] 注册新医生账号\n");
        printf("  [ 2 ] 注销/删除医生账号\n");
        printf("  [ 3 ] 床位利用率查询\n");
        printf("  --- 药房物流 ---\n");
        printf("  [ 4 ] 药品进库登记\n");
        printf("  [ 5 ] 库存审计报告\n");
        printf("  --- 财务与分析 ---\n");
        printf("  [ 6 ] 全局财务营收报告\n");
        printf("  [ 7 ] 科室就诊流量分析\n");
        printf(COLOR_RED "  [ 0 ] 返回主菜单\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请选择管理项目 (0-7)：" COLOR_RESET);
        choice = safe_get_int();
        
        switch (choice) {
            case 1: register_doctor(); break;
            case 2: delete_doctor(); break;
            case 3: optimize_bed_allocation(); break; // 修正了占位符调用
            case 4: medicine_stock_in(); break;
            case 5: view_inventory_report(); break; // 已整合重复函数
            case 6: view_hospital_financial_report(); break;
            case 7: analyze_historical_trends(); break;
            case 0: break;
            default: printf(COLOR_RED "无效的选择！\n" COLOR_RESET);
        }
    }
}

/* ==========================================
 * 2. 医生账号管理
 * ========================================== */

void register_doctor() {
    print_header("注册新医生账号");
    
    char id[MAX_ID_LEN];
    printf("请输入医生工号 (ID): ");
    safe_get_string(id, MAX_ID_LEN);
    
    if (find_doctor_by_id(id)) {
        printf(COLOR_RED "错误：该工号已存在！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    DoctorNode* new_node = (DoctorNode*)calloc(1, sizeof(DoctorNode));
    strcpy(new_node->id, id);

    printf("请输入医生姓名: ");
    safe_get_string(new_node->name, 32);
    
    printf("请输入所属科室ID: ");
    safe_get_string(new_node->dept_id, MAX_ID_LEN);

    // 校验科室是否存在
    if (!find_department_by_id(new_node->dept_id)) {
        printf(COLOR_YELLOW "警告：科室ID不存在，请核实后再次修改。\n" COLOR_RESET);
    }

    new_node->next = doctor_head;
    doctor_head = new_node;

    printf(COLOR_GREEN "医生 %s 注册成功！系统已同步至 doctors.txt 待保存队列。\n" COLOR_RESET, new_node->name);
    wait_for_enter();
}

void delete_doctor() {
    print_header("注销医生账号");
    printf("请输入要注销的医生工号: ");
    char id[MAX_ID_LEN];
    safe_get_string(id, sizeof(id));

    DoctorNode *curr = doctor_head, *prev = NULL;
    while (curr != NULL) {
        if (strcmp(curr->id, id) == 0) {
            if (prev == NULL) doctor_head = curr->next;
            else prev->next = curr->next;
            
            printf(COLOR_GREEN "医生 %s (ID:%s) 已成功移除。\n" COLOR_RESET, curr->name, id);
            free(curr);
            wait_for_enter();
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    printf(COLOR_RED "未找到该工号。\n" COLOR_RESET);
    wait_for_enter();
}

/* ==========================================
 * 3. 药房管理 (合并重复函数)
 * ========================================== */

void medicine_stock_in() {
    print_header("药品进库登记");
    printf("请输入药品ID：");
    char id[MAX_ID_LEN];
    safe_get_string(id, sizeof(id));
    
    MedicineNode* med = find_medicine_by_id(id);
    if (med) {
        printf("药品信息：[%s] | 当前库存：%d\n", med->common_name, med->stock);
        printf("请输入进库数量：");
        int qty = safe_get_int();
        if (qty > 0) {
            med->stock += qty;
            printf(COLOR_GREEN "进库成功！新库存为：%d\n" COLOR_RESET, med->stock);
        } else {
            printf(COLOR_RED "错误：数量必须大于0。\n" COLOR_RESET);
        }
    } else {
        printf(COLOR_RED "未找到该药品 ID。\n" COLOR_RESET);
    }
    wait_for_enter();
}

void view_inventory_report() {
    print_header("全局药品库存审计");
    printf("%-10s %-20s %-10s %-15s\n", "药品ID", "药品名称", "当前库存", "状态");
    print_divider();
    
    MedicineNode* curr = medicine_head;
    while (curr) {
        printf("%-10s %-20s %-10d ", curr->id, curr->common_name, curr->stock);
        if (curr->stock < 50) 
            printf(COLOR_RED "[库存告急]\n" COLOR_RESET);
        else 
            printf(COLOR_GREEN "[正常]\n" COLOR_RESET);
        curr = curr->next;
    }
    wait_for_enter();
}

/* ==========================================
 * 4. 财务与分析
 * ========================================== */

void view_hospital_financial_report() {
    print_header("医院财务汇总 (基于已缴费记录)");
    double total_revenue = 0, insurance_total = 0, personal_total = 0;
    
    RecordNode* curr = record_head;
    while (curr) {
        // 仅统计已支付状态的财务
        if (curr->status == 1) {
            total_revenue += curr->total_fee;
            insurance_total += curr->insurance_covered;
            personal_total += curr->personal_paid;
        }
        curr = curr->next;
    }
    
    printf(" 总实收营收：   %10.2f 元\n", total_revenue);
    printf(" 医保统筹部分： %10.2f 元\n", insurance_total);
    printf(" 个人支付部分： %10.2f 元\n", personal_total);
    print_divider();
    wait_for_enter();
}

void analyze_historical_trends() {
    print_header("科室流量分析");
    DepartmentNode* d_curr = dept_head;
    while (d_curr) {
        int count = 0;
        RecordNode* r_curr = record_head;
        while (r_curr) {
            DoctorNode* doc = find_doctor_by_id(r_curr->doctor_id);
            if (doc && strcmp(doc->dept_id, d_curr->id) == 0) count++;
            r_curr = r_curr->next;
        }
        printf("%-15s: %d 人次\n", d_curr->name, count);
        d_curr = d_curr->next;
    }
    wait_for_enter();
}

void optimize_bed_allocation() {
    print_header("床位资源监控");
    DepartmentNode* d_curr = dept_head;
    while (d_curr) {
        int total = 0, occupied = 0;
        BedNode* b_curr = bed_head;
        while (b_curr) {
            if (strcmp(b_curr->dept_id, d_curr->id) == 0) {
                total++;
                if (b_curr->is_occupied) occupied++;
            }
            b_curr = b_curr->next;
        }
        double rate = (total > 0) ? (double)occupied / total * 100 : 0;
        printf("科室: %-10s | 占用率: %.1f%% (%d/%d)\n", d_curr->name, rate, occupied, total);
        d_curr = d_curr->next;
    }
    wait_for_enter();
}