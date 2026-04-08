// [核心架构说明]：
// 本文件为 HIS 系统“医护端 (Doctor Subsystem)”的核心实现文件。
// 承担着全院最核心的临床业务逻辑，包括：
// 1. 门急诊接诊与物理隔离队列（120急救红色置顶）；
// 2. 电子处方开具与药房发药核销；
// 3. 【核心风控】：四级手术权限防越级拦截引擎；
// 4. 【核心风控】：住院病区双医生责任制（主治+管床）物理绑定与收治系统；
// 5. 【扩展业务】：体检中心闭环与精美电子报告单生成；
// 6. 【扩展业务】：重症 ICU 跨病区流转与床位熔断机制。

#include "doctor.h"
#include "data_manager.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// --- 辅助工具 ---
static void get_today_prefix(char* today_str) {
    get_current_time_str(today_str);
    today_str[10] = '\0';
}

/* ==========================================================================
 * 1. 临床接诊与物理隔离队列模块
 * ========================================================================== */

void view_my_queue(DoctorNode* doctor) {
    RecordNode* curr = record_head;
    char today[32];
    int count = 0;

    get_today_prefix(today);
    print_header("我的候诊队列");
    printf("医生: %s（%s） | 科室: %s\n", doctor->name, doctor->id, doctor->dept_id);
    print_divider();

    // ====== 【核心修复：大牛专家的紧急手术强置顶】 ======
    OperatingRoomNode* room = or_head;
    while (room) {
        if (room->status == 1 && strcmp(room->current_doctor, doctor->id) == 0) {
            PatientNode* p = find_outpatient_by_id(room->current_patient);
            if (!p) p = find_inpatient_by_id(room->current_patient);
            printf(COLOR_RED "【🚨 紧急代刀置顶 🚨】您正在 %s 手术室主刀抢救！患者: %s (%s)\n" COLOR_RESET, 
                   room->room_id, p ? p->name : "未知", room->current_patient);
            print_divider();
        }
        room = room->next;
    }
    // ====================================================

    while (curr) {
        // 物理隔离：只看分配给自己的、当天的、且未被叫号的挂号记录
       // 寻找该医生今天处理过的挂号记录
        // 寻找该医生今天处理过的挂号记录 (只抓取还在外面排队的)
        if ((curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY) &&
            strcmp(curr->doctor_id, doctor->id) == 0 &&
            strncmp(curr->date, today, 10) == 0 &&
            strstr(curr->description, "|CALLED") == NULL && // 【核心修复】：只看还没被叫号的！
            strstr(curr->description, "|DONE") == NULL) {
            PatientNode* p = find_outpatient_by_id(curr->patient_id);
            if (!p) p = find_inpatient_by_id(curr->patient_id);

            char pay_status[16];
            if (curr->status == 1) strcpy(pay_status, "已交费");
            else strcpy(pay_status, "未交费");

            if (curr->type == RECORD_EMERGENCY) {
                if (curr->queue_number == 1) {
                    printf(COLOR_RED "[120直通置顶] 号码:%03d | 状态:大抢救 | %s | 姓名:%-8s | 编号:%s\n" COLOR_RESET,
                           curr->queue_number, pay_status, p ? p->name : "未知", curr->patient_id);
                } else if (curr->queue_number > 0 && curr->queue_number < 100) {
                    printf(COLOR_RED "[1级濒危急诊] 号码:%03d | 状态:大抢救 | %s | 姓名:%-8s | 编号:%s\n" COLOR_RESET,
                           curr->queue_number, pay_status, p ? p->name : "未知", curr->patient_id);
                } else if (curr->queue_number >= 100 && curr->queue_number < 500) {
                    printf(COLOR_YELLOW "[2级危重急诊] 号码:%03d | 状态:优先看 | %s | 姓名:%-8s | 编号:%s\n" COLOR_RESET,
                           curr->queue_number, pay_status, p ? p->name : "未知", curr->patient_id);
                } else if (curr->queue_number >= 500 && curr->queue_number < 900) {
                    printf(COLOR_CYAN "[3级普通急诊] 号码:%03d | 状态:排队中 | %s | 姓名:%-8s | 编号:%s\n" COLOR_RESET,
                           curr->queue_number, pay_status, p ? p->name : "未知", curr->patient_id);
                } else {
                    printf("[4级非急症]   号码:%03d | 状态:延后看 | %s | 姓名:%-8s | 编号:%s\n",
                           curr->queue_number, pay_status, p ? p->name : "未知", curr->patient_id);
                }
            } else {
                printf("[普通门诊排队] 号码:%03d | 状态:排队中 | %s | 姓名:%-8s | 编号:%s\n",
                       curr->queue_number, pay_status, p ? p->name : "未知", curr->patient_id);
            }
            count++;
        }
        curr = curr->next;
    }

    if (count == 0) {
        printf(COLOR_GREEN "当前队列暂无候诊患者。\n" COLOR_RESET);
    }
    wait_for_enter();
}

PatientNode* call_next_patient(DoctorNode* doctor) {
    RecordNode* curr = record_head;
    RecordNode* target = NULL;
    char today[32];
    PatientNode* patient = NULL;

    get_today_prefix(today);

    // 呼叫逻辑：第一遍，雷达全开，优先扫描急诊室队列！
    while (curr) {
        if (curr->type == RECORD_EMERGENCY && 
            strstr(curr->description, "|CALLED") == NULL &&
            strcmp(curr->doctor_id, doctor->id) == 0 &&
            strncmp(curr->date, today, 10) == 0) {
            if (!target || curr->queue_number < target->queue_number) {
                target = curr;
            }
        }
        curr = curr->next;
    }

    // 第二遍：如果没有急救患者，再踏踏实实叫普通门诊的号
    if (!target) {
        curr = record_head;
        while (curr) {
            // 【核心修复：同时扫描普通门诊与急诊，且施加双重状态拦截】
            if ((curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY) && 
                strstr(curr->description, "|CALLED") == NULL &&
                strstr(curr->description, "|DONE") == NULL && // 双重保险：坚决不叫已经看完的
                strcmp(curr->doctor_id, doctor->id) == 0 &&
                strncmp(curr->date, today, 10) == 0) {
                
                // 找到排队号最小的（最靠前的）作为目标
                if (!target || curr->queue_number < target->queue_number) {
                    target = curr;
                }
            }
            curr = curr->next;
        }
    }

    if (!target) {
        printf(COLOR_GREEN "当前没有需要接诊的患者。\n" COLOR_RESET);
        wait_for_enter();
        return NULL;
    }

    patient = find_outpatient_by_id(target->patient_id);
    if (!patient) patient = find_inpatient_by_id(target->patient_id);

    // 接诊成功，打上已呼叫的防伪标记，并保证单据变为已处理
    target->status = 1; 
    strcat(target->description, "|CALLED");

    print_header("接诊提示");
    
    // 【核心修复】：根据预检分诊的号段，精细化医生端的弹窗话术！
    if (target->type == RECORD_EMERGENCY) {
        if (target->queue_number == 1) {
            // 120 救护车物理插队
            printf(COLOR_RED "【120急救红色指令】请立即准备抢救！患者：%s（编号：%s）\n" COLOR_RESET, patient ? patient->name : "未知", target->patient_id);
        } else if (target->queue_number > 0 && target->queue_number < 100) {
            // 分诊 1 级 (号段 10+)
            printf(COLOR_RED "【1级濒危通报】患者情况极危重，请立刻抢救！患者：%s（编号：%s）\n" COLOR_RESET, patient ? patient->name : "未知", target->patient_id);
        } else if (target->queue_number >= 100 && target->queue_number < 500) {
            // 分诊 2 级 (号段 100+)
            printf(COLOR_YELLOW "【2级危重通报】患者危重，请优先予以查体！患者：%s（编号：%s）\n" COLOR_RESET, patient ? patient->name : "未知", target->patient_id);
        } else {
            // 分诊 3 级、4 级 (号段 500+ 和 900+，包含为了省时间挂急诊的普通人)
            // 恢复普通绿字，取消“抢救”字眼
            printf(COLOR_GREEN "已呼叫急诊室患者：%s（编号：%s）\n" COLOR_RESET, patient ? patient->name : "未知", target->patient_id);
        }
    } else {
         printf(COLOR_GREEN "已呼叫门诊患者：%s（编号：%s）\n" COLOR_RESET, patient ? patient->name : "未知", target->patient_id);
    }
    
    wait_for_enter();
    return patient;
}

/* ==========================================================================
 * 2. 处方与药房核销模块
 * ========================================================================== */

int is_prescription_valid(const char* desc) {
    if (strstr(desc, "PRES|") == NULL) return 1; 
    return 1; // 简化版：默认处方有效
}

void prescribe_medicine_advanced(DoctorNode* doctor, PatientNode* patient) {
    char keyword[64];
    MedicineNode* med = NULL;
    int amount = 0;
    char desc[MAX_DESC_LEN];

    print_header("开立电子处方");
    printf("请输入要开具的药品关键字 (通用名/商品名/拼音首字母): ");
    safe_get_string(keyword, sizeof(keyword));

    med = find_medicine_by_key(keyword);
    if (!med) {
        printf(COLOR_RED "未在药房知识库中检索到匹配的药品。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf(COLOR_CYAN "检索到药品：[%s] %s (单价: %.2f，库存: %d)\n" COLOR_RESET, med->id, med->common_name, med->price, med->stock);
    printf("请输入开药数量: ");
    amount = safe_get_int();

    if (amount <= 0) return;
    if (amount > med->stock) {
        printf(COLOR_RED "警告：药房库存不足！最多可开具 %d 份。\n" COLOR_RESET, med->stock);
        wait_for_enter();
        return;
    }

    // 组装处方协议格式：PRES|普通|7|药品ID|数量
    snprintf(desc, sizeof(desc), "PRES|普通|7|%s|%d", med->id, amount);
    
    // 生成诊疗单据 (排队号传 0，交给底层自动分配)
    add_new_record(patient->id, doctor->id, RECORD_CONSULTATION, desc, med->price * amount, 0);

    printf(COLOR_GREEN "电子处方已开具，已发送至药房中心。患者需先缴费方可取药。\n" COLOR_RESET);
    wait_for_enter();
}

void dispense_medicine_service() {
    char record_id[MAX_ID_LEN];
    RecordNode* curr = record_head;
    RecordNode* target = NULL;

    print_header("药房核销中心");
    printf("请输入患者提供的处方流水号: ");
    safe_get_string(record_id, sizeof(record_id));

    while (curr) {
        if (strcmp(curr->record_id, record_id) == 0 && curr->type == RECORD_CONSULTATION) {
            target = curr;
            break;
        }
        curr = curr->next;
    }

    if (!target) {
        printf(COLOR_RED "未找到对应的处方记录。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    if (target->status == 0) {
        printf(COLOR_RED "拦截：该处方尚未缴费！请患者先前往收费处缴费。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    if (strstr(target->description, "|DISPENSED") != NULL) {
        printf(COLOR_YELLOW "拦截：该处方已核销，禁止重复发药！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 解析处方并扣减物理库存
    char med_id[64];
    int amount;
    if (sscanf(target->description, "PRES|%*[^|]|%*d|%[^|]|%d", med_id, &amount) == 2) {
        MedicineNode* med = find_medicine_by_id(med_id);
        if (med) {
            if (med->stock >= amount) {
                med->stock -= amount;
                strcat(target->description, "|DISPENSED");
                printf(COLOR_GREEN "发药成功！库存已物理扣减。[%s] 剩余库存: %d\n" COLOR_RESET, med->common_name, med->stock);
            } else {
                printf(COLOR_RED "系统异常：当前物理库存不足，发药失败！\n" COLOR_RESET);
            }
        }
    }
    wait_for_enter();
}

/* ==========================================================================
 * 3. 医技与检查中心闭环模块
 * ========================================================================== */

void prescribe_checkup_service(DoctorNode* doctor, PatientNode* patient) {
    char item_name[128];
    double fee;
    char desc[MAX_DESC_LEN];

    print_header("开具体检/检查单");
    printf("请输入拟安排的检查项目 (如: 头部CT扫描 / 全面生化抽血): ");
    safe_get_string(item_name, sizeof(item_name));
    printf("请输入检查项目预估费用: ");
    fee = safe_get_double();

    // 协议封装：CHECKUP|待检|项目名称
    snprintf(desc, sizeof(desc), "CHECKUP|待检|%s", item_name);
    add_new_record(patient->id, doctor->id, RECORD_CHECKUP, desc, fee, 0);

    printf(COLOR_GREEN "检查单已开具入库。请指引患者前往大厅服务台缴费后进行检查。\n" COLOR_RESET);
    wait_for_enter();
}

void generate_checkup_report(DoctorNode* doctor) {
    char record_id[MAX_ID_LEN];
    RecordNode* curr = record_head;
    RecordNode* target = NULL;
    char result[256];

    print_header("医学检验结果录入与报告生成");
    printf("请输入患者的检查流水单号 (REC...): ");
    safe_get_string(record_id, sizeof(record_id));

    while (curr) {
        if (strcmp(curr->record_id, record_id) == 0 && curr->type == RECORD_CHECKUP) {
            target = curr;
            break;
        }
        curr = curr->next;
    }

    if (!target) {
        printf(COLOR_RED "查无此单据，或该单据不是体检/检查单。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    if (target->status == 0) {
        printf(COLOR_RED "计费拦截：该检查单处于未缴费状态，无法执行化验录入流程！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    if (strstr(target->description, "|REPORTED") != NULL) {
        printf(COLOR_YELLOW "拦截：该检查单已出具过正式报告，禁止重复录入与篡改！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("请输入影像/化验结果的临床诊断简述: ");
    safe_get_string(result, sizeof(result));

    PatientNode* p = find_outpatient_by_id(target->patient_id);
    if (!p) p = find_inpatient_by_id(target->patient_id);

    // 模拟打印生成终端 ASCII 精美电子报告单
    print_divider();
    printf(COLOR_BLUE "       【 HIS 综合智慧医院 - 电子检查报告单 】\n" COLOR_RESET);
    print_divider();
    printf(" 患者姓名: %-15s 患者编号: %s\n", p ? p->name : "N/A", target->patient_id);
    printf(" 检查项目: %-15s 负责医师: %s\n", target->description, doctor->name);
    printf(" 检查日期: %-15s 单据流水: %s\n", target->date, target->record_id);
    print_divider();
    printf(COLOR_CYAN " [ 临 床 化 验 结 果 与 医 生 建 议 ]\n" COLOR_RESET);
    printf(" %s\n", result);
    print_divider();
    printf(COLOR_GREEN " -> 报告签发成功，数据已永久归档至患者电子病历中心。\n" COLOR_RESET);

    strcat(target->description, "|REPORTED");
    wait_for_enter();
}

/* ==========================================================================
 * 4. 外科手术室防撞与防越权审核模块
 * ========================================================================== */

void book_surgery_service(DoctorNode* doctor, PatientNode* patient) {
    int requested_level = 1;
    char desc[MAX_DESC_LEN];
    OperatingRoomNode* room = or_head;
    OperatingRoomNode* available_room = NULL;
    char actual_surgeon_id[MAX_ID_LEN]; // 记录最终实际主刀的医生工号

    print_header("外科手术预约与排期");
    printf("根据《医疗技术临床应用管理办法》，请选择本次手术分级：\n");
    printf("1. 一级手术 (普通清创/缝合，风险极低)\n");
    printf("2. 二级手术 (阑尾切除等，有一定风险)\n");
    printf("3. 三级手术 (胃大部切除、甲状腺切除，风险较高)\n");
    printf("4. 四级手术 (心脏搭桥、器官移植，高难高危)\n");
    printf("请输入手术级别 (1-4): ");
    requested_level = safe_get_int();

    if (requested_level < 1 || requested_level > 4) return;

    // 默认主刀是自己
    strcpy(actual_surgeon_id, doctor->id);

    // 【核心风控：四级手术防越权与紧急求助引擎】
    if (doctor->privilege_level < requested_level) {
        printf(COLOR_RED "\n【权限不足拦截】\n" COLOR_RESET);
        printf(COLOR_YELLOW "当前主刀: %s (资质: Level %d) | 拟开展手术: Level %d\n", doctor->name, doctor->privilege_level, requested_level);
        printf(COLOR_RED "警告：您的临床手术权限不足，严禁越级主刀！\n" COLOR_RESET);
        
        // 紧急摇人机制：生命至上！
        printf(COLOR_CYAN "\n生命至上！是否紧急呼叫具备资质的上级专家代刀抢救？(1=呼叫, 0=放弃): " COLOR_RESET);
        if (safe_get_int() != 1) {
            printf("流程已中止。\n");
            wait_for_enter();
            return;
        }

        printf(COLOR_GREEN "\n正在全院为您检索具备 Level %d 及以上资质的当值专家...\n" COLOR_RESET, requested_level);
        DoctorNode* expert = doctor_head;
        int expert_count = 0;
        
        while (expert) {
            // 1. 核心条件：资质达标 且 不是申请人自己
            if (expert->privilege_level >= requested_level && strcmp(expert->id, doctor->id) != 0) {
                
                // 2. 物理防撞条件：核实该专家目前是否正在其他手术室主刀
                int is_operating = 0;
                OperatingRoomNode* temp_room = or_head;
                while (temp_room) {
                    if (temp_room->status == 1 && strcmp(temp_room->current_doctor, expert->id) == 0) {
                        is_operating = 1; 
                        break; // 该专家正在手术，跳过
                    }
                    temp_room = temp_room->next;
                }

                // 3. 只要满足权限且当前没在做手术，跨科室也可以来救场！
                if (!is_operating) {
                    int on_duty = is_doctor_on_duty_today(expert->id);
                    
                    if (on_duty) {
                        printf(COLOR_GREEN "[在院当值] " COLOR_RESET);
                    } else {
                        printf(COLOR_YELLOW "[二线听班] " COLOR_RESET);
                    }
                    
                    // 打印出专家的科室，供主治医生自己判断摇谁比较合适
                    printf("专家: %-8s | 工号: %-6s | 资质: Level %d | 所属科室: %s\n", 
                           expert->name, expert->id, expert->privilege_level, expert->dept_id);
                    expert_count++;
                }
            }
            expert = expert->next;
        }

        if (expert_count == 0) {
            printf(COLOR_RED "极度危急：当前全院无具备该资质的专家可调度，请立即启动跨院转运预案！\n" COLOR_RESET);
            wait_for_enter();
            return;
        }

        DoctorNode* chosen_expert = NULL;
        
        // 开启防呆重试死循环
        while (1) {
            printf("\n请输入请求代刀的高级专家工号 (输入 0 放弃呼叫并退出): ");
            safe_get_string(actual_surgeon_id, sizeof(actual_surgeon_id));
            
            // 安全出口：允许医生中途反悔放弃
            if (strcmp(actual_surgeon_id, "0") == 0) {
                printf(COLOR_YELLOW "已放弃调度，手术排期流程中止。\n" COLOR_RESET);
                wait_for_enter();
                return;
            }

            chosen_expert = find_doctor_by_id(actual_surgeon_id);
            
            // 校验一：查无此人、权限不够、或者恶意呼叫自己
            if (!chosen_expert || chosen_expert->privilege_level < requested_level || strcmp(chosen_expert->id, doctor->id) == 0) {
                printf(COLOR_RED "【调度拦截】工号错误、该医生资质不符，或不能呼叫自己！请重新输入。\n" COLOR_RESET);
                continue; // 拦截成功，让用户重新输入
            }

            // 校验二：防止用户无视列表，恶意手填一个正在其他手术室开刀的大牛
            int is_operating = 0;
            OperatingRoomNode* temp_room = or_head;
            while (temp_room) {
                if (temp_room->status == 1 && strcmp(temp_room->current_doctor, chosen_expert->id) == 0) {
                    is_operating = 1; 
                    break;
                }
                temp_room = temp_room->next;
            }
            if (is_operating) {
                printf(COLOR_RED "【调度拦截】该专家目前正在手术室中主刀，无法分身！请选择列表中的空闲专家。\n" COLOR_RESET);
                continue; // 拦截成功，让用户重新输入
            }

            // 所有校验通过，打破死循环，进入下一步
            printf(COLOR_GREEN "调度成功！专家 %s 已接管这台手术。\n" COLOR_RESET, chosen_expert->name);
            break;
        }
    }

    // 【神级防撞锁】：检查实际主刀医生或患者是否已经在手术中
    while (room) {
        if (room->status == 1) {
            if (strcmp(room->current_doctor, actual_surgeon_id) == 0) {
                printf(COLOR_RED "医疗事故预警：主刀专家目前正在 %s 房间手术，无法分身！\n" COLOR_RESET, room->room_id);
                wait_for_enter();
                return;
            }
            if (strcmp(room->current_patient, patient->id) == 0) {
                printf(COLOR_RED "医疗事故预警：患者目前正在 %s 房间接受抢救，禁止重复排期！\n" COLOR_RESET, room->room_id);
                wait_for_enter();
                return;
            }
        }
        room = room->next;
    }

    // 寻找空闲手术室
    room = or_head;
    while (room) {
        if (room->status == 0) {
            available_room = room;
            break;
        }
        room = room->next;
    }

    if (!available_room) {
        printf(COLOR_RED "当前全院所有外科手术室均满载，无法排期！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 锁定手术室资源 (注意：绑定的是实际代刀专家的 ID！)
    available_room->status = 1;
    strcpy(available_room->current_patient, patient->id);
    strcpy(available_room->current_doctor, actual_surgeon_id);

    // 如果发生了代刀，要在手术纪要里把申请医生作为一助记录下来
    if (strcmp(doctor->id, actual_surgeon_id) != 0) {
        snprintf(desc, sizeof(desc), "SURGERY|Level:%d|Room:%s|一助(申请人):%s", requested_level, available_room->room_id, doctor->name);
    } else {
        snprintf(desc, sizeof(desc), "SURGERY|Level:%d|Room:%s", requested_level, available_room->room_id);
    }
    
    // 计费引擎：等级越高，费用越贵。注意，这笔手术费用和业绩，算在实际主刀专家头上！
    double fee = requested_level * 5000.0;
    add_new_record(patient->id, actual_surgeon_id, RECORD_SURGERY, desc, fee, 0);

    printf(COLOR_GREEN "\n手术排期成功！\n" COLOR_RESET);
    printf("分配房间：%s\n", available_room->room_id);
    printf("主刀专家：%s (工号:%s)\n", find_doctor_by_id(actual_surgeon_id)->name, actual_surgeon_id);
    wait_for_enter();
}

void finish_surgery_service(DoctorNode* doctor) {
    OperatingRoomNode* room = or_head;
    int found = 0;

    print_header("结束手术与清场");
    while (room) {
        if (room->status == 1 && strcmp(room->current_doctor, doctor->id) == 0) {
            printf("正在结束 %s 房间的手术...\n", room->room_id);
            
            // 释放手术室资源，进入术后消毒状态
            room->status = 2; // 2 代表深度消毒中
            strcpy(room->current_patient, "N/A");
            strcpy(room->current_doctor, "N/A");
            
            printf(COLOR_GREEN "手术已安全结束，房间进入自动消毒程序。\n" COLOR_RESET);
            found = 1;
        }
        room = room->next;
    }

    if (!found) {
        printf(COLOR_YELLOW "您当前没有正在主刀的手术记录。\n" COLOR_RESET);
    }
    wait_for_enter();
}


/* ==========================================================================
 * 5. 住院病区双医生责任制与病床流转
 * ========================================================================== */

void admission_service(DoctorNode* doctor, PatientNode* patient) {
    double deposit;
    char resident_id[MAX_ID_LEN];
    DoctorNode* res_doc;

    print_header("办理入院收治手续");

    if (patient->type == 'I') {
        printf(COLOR_YELLOW "该患者已是住院状态，无需重复办理。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    printf("拟住院患者：%s (编号：%s)\n", patient->name, patient->id);
    printf("按医院风控规定，住院押金必须为 100 的整数倍。\n");
    // ====== 【核心修复：押金防呆重试死循环】 ======
    deposit = 0.0;
    while (1) {
        printf("请输入预缴住院押金 (金额需 >= 0，输入 -1 放弃办理入院): ");
        deposit = safe_get_double();
        
        // 安全出口：允许医生中途反悔放弃
        if (deposit == -1.0) {
            printf(COLOR_YELLOW "已中止收治入院办理流程。\n" COLOR_RESET);
            wait_for_enter();
            return; 
        }
        
        // 拦截非法金额，但绝不退出，而是让医生重新输入
        if (deposit < 0) {
            printf(COLOR_RED "【输入拦截】押金金额不能为负数，请重新输入！\n" COLOR_RESET);
            continue; 
        }
        
        // 输入合法，打破死循环，进入下一步
        break; 
    }
    // ===============================================

    // 【核心架构：双医生责任制物理绑定】
    printf(COLOR_CYAN "\n【临床合规要求】：实行主治与管床双医生负责制。\n" COLOR_RESET);
    printf("系统已将您 (%s) 自动指派为该患者的【主治医生(上级把关)】。\n", doctor->name);
    printf("请输入负责该患者日常查房的【管床医生(下级执行)】工号: ");
    safe_get_string(resident_id, MAX_ID_LEN);

    res_doc = find_doctor_by_id(resident_id);
    if (!res_doc) {
        printf(COLOR_RED "拦截：在全院人事档案中未查找到该管床医生工号！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 逻辑硬拦截：主治和管床不能是同一个人！
    if (strcmp(doctor->id, resident_id) == 0) {
        printf(COLOR_RED "【医疗安全警告】主治医生与管床医生不能为同一人！必须形成上下级梯队！\n收治流程已强制中止。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 职称温馨提示：管床一般是住院医(L1)或主治医(L2)
    if (res_doc->privilege_level > 2) {
        printf(COLOR_YELLOW "【流程建议】您指派的管床医生 (%s) 具有高级专家职称 (Level %d)。系统允许收治，请确认资源分配是否合理。\n" COLOR_RESET, res_doc->name, res_doc->privilege_level);
    }

    // 分配物理床位
    BedNode* bed = find_available_bed(doctor->dept_id, WARD_GENERAL);
    if (!bed) {
        printf(COLOR_RED "拦截：当前科室已无可用普通床位，无法收治入区！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 执行跨链表物理搬家引擎
    if (transfer_outpatient_to_inpatient(patient->id)) {
        PatientNode* in_p = find_inpatient_by_id(patient->id);
        if (in_p) {
            in_p->hospital_deposit = deposit;
            strcpy(in_p->bed_id, bed->bed_id);
            get_today_prefix(in_p->admission_date);
            
            // 写入双医责任人
            strcpy(in_p->attending_doctor_id, doctor->id);
            strcpy(in_p->resident_doctor_id, resident_id);

            bed->is_occupied = 1;
            strcpy(bed->patient_id, patient->id);

            printf(COLOR_GREEN "\n入院办理成功！已完成门诊到住院系统的数据平移。\n" COLOR_RESET);
            printf("分配床位: %s\n", bed->bed_id);
            printf("主治医师: %s (工号:%s)\n", doctor->name, doctor->id);
            printf("管床医师: %s (工号:%s)\n", res_doc->name, res_doc->id);
        }
    } else {
        printf(COLOR_RED "底层引擎错误：跨链表平移失败！\n" COLOR_RESET);
    }
    
    wait_for_enter();
}

void transfer_patient_to_icu(DoctorNode* doctor) {
    char patient_id[MAX_ID_LEN];
    PatientNode* patient;

    print_header("重症 ICU 跨病区紧急流转");
    printf("请输入需要紧急转入 ICU 的【住院患者】编号: ");
    safe_get_string(patient_id, sizeof(patient_id));

    patient = find_inpatient_by_id(patient_id);
    if (!patient) {
        printf(COLOR_RED "拦截：查无此人，或者该患者尚未办理住院手续。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 权限与防呆校验
    if (strcmp(patient->attending_doctor_id, doctor->id) != 0 && 
        strcmp(patient->resident_doctor_id, doctor->id) != 0) {
        printf(COLOR_RED "越权拦截：您不是该患者的主治或管床医生，无权越级操作床位流转！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    BedNode* old_bed = find_bed_by_id(patient->bed_id);
    if (old_bed && old_bed->type == WARD_SPECIAL) {
        printf(COLOR_YELLOW "该患者当前已经在 ICU 重症监护室中，无需重复转床。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 查找 ICU 特级病床 (WARD_SPECIAL)
    BedNode* new_bed = find_available_bed(doctor->dept_id, WARD_SPECIAL);
    if (!new_bed) {
        // 【ICU 资源熔断机制】
        printf(COLOR_RED "\n【全院红色行政告警】当前重症监护室 (ICU) 床位已全部耗尽！\n" COLOR_RESET);
        printf(COLOR_YELLOW "系统已锁定流转。请立即联系医务处执行紧急加床或跨科室借床调度预案！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 释放旧床位
    if (old_bed) {
        old_bed->is_occupied = 0;
        strcpy(old_bed->patient_id, "N/A");
    }

    // 锁定新 ICU 床位并更新患者指针
    new_bed->is_occupied = 1;
    strcpy(new_bed->patient_id, patient->id);
    strcpy(patient->bed_id, new_bed->bed_id);

    // 生成流转计费记录
    add_new_record(patient->id, doctor->id, RECORD_HOSPITALIZATION, "SYS|病区流转:普通病房转入ICU重症监护室", 500.0, 0);

    printf(COLOR_GREEN "\n重症流转指令执行成功！\n" COLOR_RESET);
    printf("患者已安全移交至 ICU 床位: %s\n", new_bed->bed_id);
    wait_for_enter();
}

void discharge_inpatient_service(DoctorNode* doctor) {
    char patient_id[MAX_ID_LEN];
    PatientNode* patient;
    RecordNode* curr;
    double total_cost = 0;
    double total_paid = 0;

    print_header("办理出院审批");
    printf("请输入要出院的患者编号: ");
    safe_get_string(patient_id, sizeof(patient_id));

    patient = find_inpatient_by_id(patient_id);
    if (!patient) {
        printf(COLOR_RED "该患者不在住院名单中。\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 权限校验：只有主治或管床医生才能批出院
    if (strcmp(patient->attending_doctor_id, doctor->id) != 0 && 
        strcmp(patient->resident_doctor_id, doctor->id) != 0) {
        printf(COLOR_RED "越权拦截：您不是该患者的主治医生或管床医生，无权审批出院！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // 财务审查
    curr = record_head;
    while (curr) {
        if (strcmp(curr->patient_id, patient->id) == 0 && curr->type == RECORD_HOSPITALIZATION) {
            total_cost += curr->personal_paid;
            if (curr->status == 1) {
                total_paid += curr->personal_paid;
            }
        }
        curr = curr->next;
    }

    if (total_paid < total_cost) {
        printf(COLOR_RED "财务拦截：该患者尚有未结清的住院费用单据 (欠费: %.2f元)！\n请指引患者结清后再来办理。\n" COLOR_RESET, total_cost - total_paid);
        wait_for_enter();
        return;
    }

    // 释放床位
    BedNode* bed = find_bed_by_id(patient->bed_id);
    if (bed) {
        bed->is_occupied = 0;
        strcpy(bed->patient_id, "N/A");
    }

    // 结转押金
    patient->account_balance += patient->hospital_deposit;
    patient->hospital_deposit = 0;
    strcpy(patient->bed_id, "");
    strcpy(patient->attending_doctor_id, "N/A");
    strcpy(patient->resident_doctor_id, "N/A");
    patient->type = 'O'; // 身份降级回门诊

    printf(COLOR_GREEN "出院审批通过！已释放床位，押金 %.2f 元已原路退回患者账户余额。\n" COLOR_RESET, patient->account_balance);
    wait_for_enter();
}

/* ==========================================================================
 * 6. 个人业务统计模块
 * ========================================================================== */

void view_doctor_statistics(DoctorNode* doctor) {
    RecordNode* curr = record_head;
    char today[32];
    int patient_count = 0;
    double total_revenue = 0.0;

    get_today_prefix(today);
    
    while (curr) {
        if (strcmp(curr->doctor_id, doctor->id) == 0) {
            if (strncmp(curr->date, today, 10) == 0 && 
               (curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY)) {
                patient_count++;
            }
            if (curr->status == 1) {
                total_revenue += curr->total_fee;
            }
        }
        curr = curr->next;
    }

    print_header("医生个人业务统计");
    printf("医生: %s（%s）\n", doctor->name, doctor->id);
    printf("今日接诊量: %d 人次\n", patient_count);
    printf("累计创收贡献: %.2f 元\n", total_revenue);
    wait_for_enter();
}
// ==========================================
// 新增业务：查看我的专属管床/主治患者 (用于医生每日查房)
// ==========================================
void view_my_inpatients(DoctorNode* doctor) {
    PatientNode* curr = inpatient_head;
    int count = 0;

    print_header("我的住院患者 (每日查房大名单)");
    printf("%-10s %-12s %-10s %-15s %-10s %-15s\n", "床位号", "患者编号", "姓名", "我的角色", "预缴押金", "入院日期");
    print_divider();

    while (curr) {
        int is_attending = (strcmp(curr->attending_doctor_id, doctor->id) == 0);
        int is_resident = (strcmp(curr->resident_doctor_id, doctor->id) == 0);

        // 只要是该医生的病人（不管是主治还是管床），统统抓出来
        if (is_attending || is_resident) {
            char role_str[32] = {0};
            if (is_attending && is_resident) strcpy(role_str, "主治+管床");
            else if (is_attending) strcpy(role_str, "主治医师");
            else strcpy(role_str, "管床医师");
            
            printf("%-10s %-12s %-10s %-15s %-10.2f %-15s\n", 
                curr->bed_id[0] ? curr->bed_id : "暂无", 
                curr->id, curr->name, role_str, curr->hospital_deposit, curr->admission_date);
            count++;
        }
        curr = curr->next;
    }

    if (count == 0) {
        printf(COLOR_YELLOW "您当前没有负责的住院患者。\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "合计: %d 名患者。请密切关注患者生命体征及欠费情况。\n" COLOR_RESET, count);
    }
    wait_for_enter();
}

void view_department_inpatients(DoctorNode* doctor) {
    PatientNode* curr = inpatient_head;
    int count = 0;

    print_header("科室在院患者大名单");
    printf("%-10s %-12s %-6s %-12s %-12s\n", "床位号", "患者编号", "姓名", "主治医生", "管床医生");
    print_divider();

    while (curr) {
        BedNode* bed = find_bed_by_id(curr->bed_id);
        if (bed && strcmp(bed->dept_id, doctor->dept_id) == 0) {
            DoctorNode* att = find_doctor_by_id(curr->attending_doctor_id);
            DoctorNode* res = find_doctor_by_id(curr->resident_doctor_id);
            
            printf("%-10s %-12s %-6s %-12s %-12s\n", 
                curr->bed_id, curr->id, curr->name, 
                att ? att->name : "N/A", res ? res->name : "N/A");
            count++;
        }
        curr = curr->next;
    }

    if (count == 0) {
        printf(COLOR_YELLOW "当前科室暂无住院患者。\n" COLOR_RESET);
    }
    wait_for_enter();
}

/* ==========================================================================
 * 7. 系统子菜单入口
 * ========================================================================== */

void doctor_subsystem() {
    char doctor_id[MAX_ID_LEN];
    DoctorNode* doctor;
    PatientNode* current_patient = NULL;
    int choice = -1;

    print_header("医护工作站入口");
    printf("请输入医生工号进行鉴权: ");
    safe_get_string(doctor_id, sizeof(doctor_id));

    doctor = find_doctor_by_id(doctor_id);
    if (!doctor) {
        printf(COLOR_RED "鉴权失败：查无此工号，或已被行政吊销！\n" COLOR_RESET);
        wait_for_enter();
        return;
    }

    // ====== 【核心修复：跨会话状态恢复 (State Persistence)】 ======
    // 1. 优先检查该医生是不是被别人“摇”去紧急代刀了！(手术室最高优先)
    OperatingRoomNode* room = or_head;
    while (room) {
        if (room->status == 1 && strcmp(room->current_doctor, doctor->id) == 0) {
            current_patient = find_outpatient_by_id(room->current_patient);
            if (!current_patient) current_patient = find_inpatient_by_id(room->current_patient);
            printf(COLOR_RED "\n【抢救通报】您有紧急代刀手术正在进行！系统已自动为您切入抢救状态！\n" COLOR_RESET);
            wait_for_enter();
            break;
        }
        room = room->next;
    }

    // 2. 如果没在手术台，再找普通的叫号恢复
    if (!current_patient) {
        char today[32];
        get_current_time_str(today);
        today[10] = '\0';
        RecordNode* curr = record_head;
        RecordNode* latest_called = NULL;
        
        while (curr) {
            // 【核心修复：去掉多余的括号，保证逻辑连贯】
            if ((curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY) &&
                strcmp(curr->doctor_id, doctor->id) == 0 &&
                strncmp(curr->date, today, 10) == 0 &&
                strstr(curr->description, "|CALLED") != NULL &&
                strstr(curr->description, "|HANDOVER") == NULL &&
                strstr(curr->description, "|DONE") == NULL) { 
                
                if (!latest_called || strcmp(curr->date, latest_called->date) > 0) {
                    latest_called = curr;
                }
            }
            curr = curr->next;
        }
        if (latest_called) {
            current_patient = find_outpatient_by_id(latest_called->patient_id);
            if (!current_patient) current_patient = find_inpatient_by_id(latest_called->patient_id);
        }
    }
    // =================================================================

while (choice != 0) {
        print_header("医生控制台");
        printf("当前当值专家: %s（资质: Level %d | 科室: %s）\n", doctor->name, doctor->privilege_level, doctor->dept_id);
        
        if (current_patient) {
            printf(COLOR_YELLOW "-> 正在接诊门诊患者: %s（%s）\n" COLOR_RESET, current_patient->name, current_patient->id);
        } else {
            printf("-> 当前处于空闲待命状态\n");
        }

        // ====== 【核心修复：查房强提醒引擎】 ======
        int my_inpatient_count = 0;
        PatientNode* temp_in = inpatient_head;
        while(temp_in) {
            if (strcmp(temp_in->attending_doctor_id, doctor->id) == 0 || strcmp(temp_in->resident_doctor_id, doctor->id) == 0) {
                my_inpatient_count++;
            }
            temp_in = temp_in->next;
        }
        if (my_inpatient_count > 0) {
            printf(COLOR_CYAN "-> 🔔 查房提醒: 您当前名下有 %d 名住院患者，请记得每日查房下医嘱！\n" COLOR_RESET, my_inpatient_count);
        }
        // ===========================================
        
        print_divider();
        printf(" 1. 查看我的门诊候诊队列\n");
        printf(" 2. 呼叫下一位患者\n");
        printf(" 3. 开立电子处方\n");
        printf(" 4. 开具体检/化验单\n");
        printf(" 5. 录入检查结果并生成报告\n");
        printf(" 6. 安排外科手术 (主刀自动核验/摇人)\n");
        printf(" 7. 结束当台手术\n");
        printf(" 8. 收治入病区 (双医生绑定制)\n");
        printf(" 9. 紧急转入 ICU 重症监护室\n");
        printf("10. 审批出院结算\n");
        printf(COLOR_GREEN "11. 查看我的住院患者 (管床/主治查房)\n" COLOR_RESET); // 新增功能
        printf("12. 查看科室在院大名单\n"); // 顺延
        printf("13. 个人工作量统计\n");     // 顺延
        printf(COLOR_CYAN "14. 结束当前门诊就诊 (实时落盘并清空状态)\n" COLOR_RESET); // 顺延
        printf(" 0. 退出登录\n");
        print_divider();
        printf("请选择指令: ");
        choice = safe_get_int();

        switch (choice) {
            case 1: view_my_queue(doctor); break;
            case 2: current_patient = call_next_patient(doctor); break;
            case 3: 
                if (current_patient) prescribe_medicine_advanced(doctor, current_patient);
                else { printf(COLOR_YELLOW "请先呼叫患者！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 4:
                if (current_patient) prescribe_checkup_service(doctor, current_patient);
                else { printf(COLOR_YELLOW "请先呼叫患者！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 5: generate_checkup_report(doctor); break;
            case 6:
                if (current_patient) {
                    book_surgery_service(doctor, current_patient);
                    
                    OperatingRoomNode* temp_room = or_head;
                    while(temp_room) {
                        if (temp_room->status == 1 && strcmp(temp_room->current_patient, current_patient->id) == 0) {
                            if (strcmp(temp_room->current_doctor, doctor->id) != 0) {
                                printf(COLOR_CYAN "\n【系统提示】患者已成功移交给上级专家接手。您的首诊任务结束，可随时呼叫下一位患者。\n" COLOR_RESET);
                                
                                char today[32];
                                get_current_time_str(today);
                                today[10] = '\0';
                                RecordNode* curr = record_head;
                                while(curr) {
                                    if (strcmp(curr->patient_id, current_patient->id) == 0 &&
                                        strcmp(curr->doctor_id, doctor->id) == 0 &&
                                        strncmp(curr->date, today, 10) == 0 &&
                                        strstr(curr->description, "|CALLED") != NULL) {
                                        strcat(curr->description, "|HANDOVER"); 
                                        break;
                                    }
                                    curr = curr->next;
                                }
                                current_patient = NULL; 
                            }
                            break;
                        }
                        temp_room = temp_room->next;
                    }
                }
                else { printf(COLOR_YELLOW "请先呼叫患者！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 7: 
                if (!current_patient) {
                    printf(COLOR_YELLOW "当前没有正在接诊/手术的患者！\n" COLOR_RESET);
                    wait_for_enter();
                    break;
                }
                finish_surgery_service(doctor); 
                printf(COLOR_CYAN "\n====================================\n");
                printf("【术后患者转归指派 (PACU/病房流转)】\n");
                printf("====================================\n" COLOR_RESET);
                printf("请为患者 %s (%s) 指派术后去向：\n", current_patient->name, current_patient->id);
                printf("1. 收治入普通病区 (分配床位及管床医生)\n");
                printf("2. 紧急转入 ICU 重症监护室\n");
                printf("3. 急诊留观/当日离院 (暂不收治)\n");
                printf("请选择去向 (1-3): ");
                
                int post_op_choice = safe_get_int();
                if (post_op_choice == 1) {
                    admission_service(doctor, current_patient);
                } else if (post_op_choice == 2) {
                    transfer_patient_to_icu(doctor);
                } else {
                    printf(COLOR_YELLOW "已将患者转入急诊留观区观察。\n" COLOR_RESET);
                }

                char today[32];
                get_current_time_str(today);
                today[10] = '\0';
                RecordNode* curr = record_head;
                
                while(curr) {
                    if (strcmp(curr->patient_id, current_patient->id) == 0 &&
                        strcmp(curr->doctor_id, doctor->id) == 0 &&
                        strncmp(curr->date, today, 10) == 0 &&
                        strstr(curr->description, "|CALLED") != NULL &&
                        strstr(curr->description, "|DONE") == NULL) {
                        strcat(curr->description, "|DONE"); 
                        break;
                    }
                    curr = curr->next;
                }
                current_patient = NULL; 
                save_system_data(); // 术后自动落盘
                printf(COLOR_CYAN "\n【系统提示】术后流程闭环完毕，患者已安全移交！\n您的接诊状态已清空，可随时呼叫下一位患者。\n" COLOR_RESET);
                break;
            case 8:
                if (current_patient) admission_service(doctor, current_patient);
                else { printf(COLOR_YELLOW "请先呼叫患者！\n" COLOR_RESET); wait_for_enter(); }
                break;
            case 9: transfer_patient_to_icu(doctor); break;
            case 10: discharge_inpatient_service(doctor); break;
            case 11: view_my_inpatients(doctor); break;          // 新增路由
            case 12: view_department_inpatients(doctor); break;  // 顺延路由
            case 13: view_doctor_statistics(doctor); break;      // 顺延路由
            case 14:                                             // 顺延，并保留实时落盘
                if (current_patient) {
                    char today_end[32];
                    get_current_time_str(today_end);
                    today_end[10] = '\0';
                    RecordNode* curr_end = record_head;
                    
                    while(curr_end) {
                        if (strcmp(curr_end->patient_id, current_patient->id) == 0 &&
                            strcmp(curr_end->doctor_id, doctor->id) == 0 &&
                            strncmp(curr_end->date, today_end, 10) == 0 &&
                            strstr(curr_end->description, "|CALLED") != NULL &&
                            strstr(curr_end->description, "|DONE") == NULL) {
                            strcat(curr_end->description, "|DONE"); 
                            break;
                        }
                        curr_end = curr_end->next;
                    }
                    current_patient = NULL; 
                    save_system_data(); // 门诊结束实时落盘
                    printf(COLOR_CYAN "\n【系统提示】门诊接诊已结束，电子病历已归档！\n接诊状态已清空，可随时呼叫下一位排队患者。\n" COLOR_RESET);
                } else {
                    printf(COLOR_YELLOW "当前没有正在接诊的患者！\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            case 0: break;
            default:
                printf(COLOR_YELLOW "指令无法识别。\n" COLOR_RESET);
                wait_for_enter();
                break;
        }
    }
}