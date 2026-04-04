#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "ambulance.h"
#include "utils.h"

/* ==========================================
 * 銆愭柊澧炲紩鐢ㄣ€戜负浜嗘弧瓒冲姛鑳借姹傦紝鍦ㄦ鎷夊彇搴曞眰鍑哄嫟鏃ュ織涓庢墜鏈缁撴瀯
 * ========================================== */
#ifndef HIS_EXT_NODES_DEFINED
#define HIS_EXT_NODES_DEFINED
// 1. 鏁戞姢杞﹀嚭鍕ゆ棩蹇楄妭鐐?
typedef struct AmbulanceLogNode {
    char log_id[32];            // 鏃ュ織鍗曞彿 (濡?ALB1711693450)
    char vehicle_id[32];        // 娲惧嚭鐨勮溅杈嗙紪鍙?
    char dispatch_time[32];     // 娲惧彂鏃堕棿
    char destination[128];      // 浠诲姟鐩殑鍦?
    struct AmbulanceLogNode* next;
} AmbulanceLogNode;

// 2. 鎵嬫湳瀹よ妭鐐?(鎬ユ晳鑱斿姩浣跨敤锛氭柟渚胯皟搴﹀憳棰勫垽鎶㈡晳瀹ゆ槸鍚︽湁绌轰綅)
typedef struct OperatingRoomNode {
    char room_id[32];           // 鎵嬫湳瀹ょ紪鍙?(濡?OR-01)
    int status;                 // 0: 绌洪棽, 1: 鎵嬫湳涓? 2: 娑堟瘨缁存姢
    char current_patient[32];   // 褰撳墠鎮ｈ€?
    char current_doctor[32];    // 涓诲垁鍖荤敓
    struct OperatingRoomNode* next;
} OperatingRoomNode;
#endif

/* ==========================================
 * 鍏ㄥ眬鏁版嵁閾捐〃澶存寚閽?(浣跨敤 extern 寮曠敤)
 * 瀹為檯瀹氫箟鍦ㄥ叾浠栨枃浠讹紙濡?data_manager.c锛変腑銆?
 * ========================================== */
extern AmbulanceNode* ambulance_head;        // 鍘熸湁鐨勫疄鏃惰溅杈嗙姸鎬侀摼琛ㄥご鎸囬拡
extern AmbulanceLogNode* ambulance_log_head; // 銆愭柊澧炪€戝嚭鍕よ褰曡〃澶存寚閽?
extern OperatingRoomNode* or_head;           // 銆愭柊澧炪€戞墜鏈鑱斿姩澶存寚閽?

// 澹版槑澶栭儴璁板綍鏃ュ織鐨勫簳灞傚伐鍘傚嚱鏁?(鍦?data_manager.c 涓疄鐜?
extern void add_ambulance_log(const char* vehicle_id, const char* destination);
// 銆愯法妯″潡鑱斿姩澹版槑銆戯細寮曞叆鎸傚彿鐢熸垚寮曟搸锛岀敤浜庢晳鎶よ溅鍥炲満鏃惰嚜鍔ㄦ媺璧锋€ヨ瘖閫氶亾
extern void add_new_record(const char* patient_id, const char* doctor_id, RecordType type, const char* description, double fee, int queue_num);

/**
 * @brief 120 鏁戞姢杞︽櫤鎱ц皟搴﹀瓙绯荤粺鐨勪富鍏ュ彛
 * 鎻愪緵浜や簰寮忕殑鑿滃崟锛岀敤浜庡鍖婚櫌鐨勬晳鎶よ溅闃熻繘琛屽疄鏃剁洃鎺с€佺揣鎬ユ淳鍙戝拰鍥炲満绠＄悊銆?
 * (宸叉垚鍔熸墿鍏呭嚭鍕よ〃鍜屾墜鏈鑱斿姩)
 */
void ambulance_subsystem() {
    int choice = -1;
    
    // 涓绘帶寰幆锛岀洿鍒扮敤鎴烽€夋嫨 0 (閫€鍑? 鎵嶇粨鏉?
    while (choice != 0) {
        
        // 鎵撳嵃绯荤粺鑿滃崟鍙婂ご閮?UI
        print_header("120 救护车智慧调度中心(含出勤表与急救联动)");
        printf("  [ 1 ] 查看全院救护车实时监控\n");
        printf("  [ 2 ] 紧急派单出车(120调度) + 自动记入出勤表\n");
        printf("  [ 3 ] 车辆回场及消毒登记(+ " COLOR_RED "危重患者绿色通道" COLOR_RESET ")\n");
        printf("  [ 4 ] 查看救护车历史出勤表（新增功能）\n"); 
        printf("  [ 5 ] 联动：查看急诊手术室实时空闲状态（新增功能）\n"); 
        printf(COLOR_RED "  [ 0 ] 返回行政管理中心\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "请输入操作指令(0-5): " COLOR_RESET);
        choice = safe_get_int(); // 瀹夊叏鑾峰彇鐢ㄦ埛杈撳叆鐨勬暣鍨嬮€夐」

        // 鏍规嵁鐢ㄦ埛杈撳叆杩涜涓氬姟璺敱鍒嗗彂
        switch (choice) {
            case 1: {
                // ==========================================
                // 涓氬姟 1锛氬叏鏅湅鏉?- 鏌ョ湅鏁戞姢杞﹂槦瀹炴椂鐘舵€?
                // ==========================================
                print_header("救护车队实时状态");
                // 鎵撳嵃琛ㄥご锛屼繚鎸佸榻?
                printf("%-12s %-12s %-10s %-25s\n", "车牌/编号", "随车司机", "当前状态", "GPS实时位置");
                print_divider();
                
                AmbulanceNode* curr = ambulance_head; // 浠庨摼琛ㄥご閮ㄥ紑濮嬮亶鍘?
                int count = 0;
                
                while (curr) {
                    const char* st_text;
                    // 鏍规嵁杞﹁締鐨勭姸鎬佹灇涓?AMB_IDLE, AMB_DISPATCHED 绛?鏄犲皠涓哄甫棰滆壊鐨勭粓绔枃鏈?
                    if (curr->status == AMB_IDLE) {
                        st_text = COLOR_GREEN "空闲待命" COLOR_RESET;  // 缁胯壊浠ｈ〃鍙敤
                    } else if (curr->status == AMB_DISPATCHED) {
                        st_text = COLOR_RED "执行任务" COLOR_RESET;    // 绾㈣壊浠ｈ〃姝ｅ繖
                    } else {
                        st_text = COLOR_YELLOW "维保消毒" COLOR_RESET; // 榛勮壊浠ｈ〃涓嶅彲鐢ㄤ絾闈炲嚭杞︾姸鎬?
                    }
                    
                    // 鏍煎紡鍖栬緭鍑哄綋鍓嶈溅杈嗙殑鍏蜂綋淇℃伅
                    printf("%-12s %-12s %-10s %-25s\n", curr->vehicle_id, curr->driver_name, st_text, curr->current_location);
                    
                    curr = curr->next; // 鎸囬拡鍚庣Щ
                    count++;           // 璁板綍褰撳墠绯荤粺涓溅杈嗙殑鎬绘暟
                }
                
                // 瀹归敊澶勭悊锛氬鏋滈摼琛ㄤ负绌猴紝缁欎簣鍙嬪ソ鐨勬彁绀?
                if (count == 0) {
                    printf(COLOR_YELLOW "当前系统内尚未录入任何救护车数据。\n" COLOR_RESET);
                }
                wait_for_enter(); // 鏆傚仠锛岀瓑寰呯敤鎴锋寜鍥炶溅缁х画
                break;
            }
            case 2: {
                // ==========================================
                // 涓氬姟 2锛氱揣鎬ュ嚭杞︽淳鍗?- 鏀瑰彉杞﹁締鐘舵€佸苟鏇存柊浣嶇疆
                // ==========================================
                printf("请输入需调度的空闲车辆编号: ");
                char vid[MAX_ID_LEN]; 
                safe_get_string(vid, MAX_ID_LEN);
                
                // 鍦ㄩ摼琛ㄤ腑鏌ユ壘鍖归厤鐨勮溅杈嗚妭鐐?
                AmbulanceNode* target = ambulance_head;
                while (target && strcmp(target->vehicle_id, vid) != 0) {
                    target = target->next;
                }
                
                // 杈圭晫涓庣姸鎬佹牎楠?
                if (!target) {
                    // 鎵句笉鍒板搴旂殑杞﹁締
                    printf(COLOR_RED "错误：未找到该车辆。\n" COLOR_RESET);
                } else if (target->status != AMB_IDLE) {
                    // 鏍稿績涓氬姟瑙勫垯锛氬彧鏈夊浜?绌洪棽寰呭懡"鐘舵€佺殑杞︽墠鑳借娲惧彂锛岄槻姝㈣祫婧愬啿绐?
                    printf(COLOR_RED "驳回：该车辆当前不在空闲状态，无法派单。\n" COLOR_RESET);
                } else {
                    // 婊¤冻娲惧彂鏉′欢锛岃姹傝緭鍏ョ洰鐨勫湴淇℃伅
                    printf("请输入急救现场地址/GPS定位: ");
                    safe_get_string(target->current_location, 128); // 鏇存柊杞﹁締鐨勫疄鏃朵綅缃俊鎭?
                    
                    target->status = AMB_DISPATCHED; // 銆愮姸鎬佹壄杞€戝皢杞﹁締鐘舵€佹壄杞负鈥滄墽琛屼换鍔♀€?
                    
                    // 銆愮粷瀵规寚浠よ姹傛鍏ャ€戯細鐘舵€佹壄杞垚鍔熷悗锛岄潤榛樿皟鐢ㄥ簳灞傚嚱鏁板啓鍏モ€滃巻鍙插嚭鍕よ〃鈥?
                    add_ambulance_log(target->vehicle_id, target->current_location);

                    printf(COLOR_GREEN "派单成功！车辆 %s 已紧急出车前往 [%s]。\n" COLOR_RESET, target->vehicle_id, target->current_location);
                    printf(COLOR_CYAN "(系统底层已自动捕获该动作并写入出勤调度日志)\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 3: {
                // ==========================================
                // 涓氬姟 3锛氬洖鍦虹櫥璁?- 缁撴潫浠诲姟骞堕噴鏀捐溅杈嗚祫婧?(+ 鎬ユ晳缁胯壊閫氶亾鑱斿姩)
                // ==========================================
                printf("请输入回场车辆编号: ");
                char vid[MAX_ID_LEN]; 
                safe_get_string(vid, MAX_ID_LEN);
                
                // 鍦ㄩ摼琛ㄤ腑鏌ユ壘鍖归厤鐨勮溅杈嗚妭鐐?
                AmbulanceNode* target = ambulance_head;
                while (target && strcmp(target->vehicle_id, vid) != 0) {
                    target = target->next;
                }
                
                // 杈圭晫涓庣姸鎬佹牎楠岋細蹇呴』瀛樺湪涓斿綋鍓嶇姸鎬佸繀椤绘槸"鎵ц浠诲姟"涓?
                if (target && target->status == AMB_DISPATCHED) {
                    target->status = AMB_IDLE; // 銆愮姸鎬佹壄杞€戝皢杞﹁締鐘舵€侀噸缃负鈥滅┖闂插緟鍛解€?
                    
                    // 妯℃嫙杞﹁締鍥炲満鍚庣殑鐗╃悊浣嶇疆閲嶇疆
                    strcpy(target->current_location, "医院急救中心停车场");
                    
                    printf(COLOR_GREEN "车辆 %s 已确认回场，并已完成标准消毒作业，转为待命状态。\n" COLOR_RESET, target->vehicle_id);

                    // ==========================================
                    // 銆愮绾цˉ涓侊細鎬ヨ瘖缁胯壊閫氶亾鑷姩鑱斿姩銆?
                    // 璋冨害涓績纭杞﹁締鍥炲満鍚庯紝鐩存帴鎶婇殢杞︽媺鍥炴潵鐨勬偅鑰呭杩涙€ヨ瘖闃熷垪
                    // ==========================================
                    printf("\n" COLOR_CYAN ">>> 是否有随车转运的危重患者需要立即接入【急诊绿色通道】？(1-是 0-否): " COLOR_RESET);
                    if (safe_get_int() == 1) {
                        char pid[MAX_ID_LEN], did[MAX_ID_LEN];
                        printf("请输入患者编号(若尚未建档请先输入临时编号，如 P-EMG-01): ");
                        safe_get_string(pid, MAX_ID_LEN);
                        printf("请输入负责接诊的【急诊科】医生工号: ");
                        safe_get_string(did, MAX_ID_LEN);

                        // 寮哄埗鎷夎捣 RECORD_EMERGENCY (鎬ヨ瘖) 鍗曟嵁锛岀洿鎺ユ帹鍏ュ簳灞傞摼琛?
                        // 鎬ヨ瘖璇婅垂瀹氫负 50.0 鍏冿紝queue_number 缃?0 (鎬ヨ瘖涓嶆帓鍙凤紝缁濆浼樺厛)
                        add_new_record(pid, did, RECORD_EMERGENCY, "120救护车紧急转运入院(生命体征不稳)", 50.0, 0);

                        printf(COLOR_RED ">>> [最高优先级警报] 患者 %s 的急救档案已生成！\n" COLOR_RESET, pid);
                        printf(COLOR_RED ">>> 该急诊单据已强制推入医生 %s 的候诊大屏，并【置顶亮红灯】！\n" COLOR_RESET, did);
                    }

                } else {
                    // 濡傛灉杞﹁締涓嶅瓨鍦紝鎴栨湰韬氨鍋滃湪鍖婚櫌閲岋紝鍒欐嫆缁濊鎿嶄綔
                    printf(COLOR_RED "操作失败：未找到该车，或车辆未处于执行任务状态。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 4: {
                // ==========================================
                // 銆愮粷瀵规寚浠よ姹傛鍏ャ€戜笟鍔?4锛氭煡鐪嬫晳鎶よ溅鍘嗗彶鍑哄嫟琛?
                // ==========================================
                print_header("120 救护车历史出勤记录表");
                printf("%-15s %-12s %-22s %-30s\n", "调度单号", "车牌编号", "出勤时间", "目的地");
                print_divider();
                
                AmbulanceLogNode* curr_log = ambulance_log_head;
                int log_count = 0;
                
                // 閬嶅巻骞跺睍绀哄巻鍙插嚭鍕ゆ祦姘?
                while (curr_log) {
                    printf("%-15s %-12s %-22s %-30s\n", 
                           curr_log->log_id, curr_log->vehicle_id, 
                           curr_log->dispatch_time, curr_log->destination);
                    curr_log = curr_log->next;
                    log_count++;
                }
                
                if (log_count == 0) {
                    printf(COLOR_YELLOW "暂无救护车出勤历史记录。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 5: {
                // ==========================================
                // 銆愮粷瀵规寚浠よ姹傛鍏ャ€戜笟鍔?5锛氳仈鍔ㄦ煡鐪嬫墜鏈鐘舵€?
                // 鍦烘櫙锛氳皟搴﹀憳鍦ㄦ淳鍑烘晳鎶よ溅鎺ラ噸鐥囨偅鑰呭墠锛屽揩閫熶簡瑙ｆ墜鏈绌轰綅鎯呭喌
                // ==========================================
                print_header("联动查询：急诊手术室实时监控");
                printf("%-10s %-12s %-15s %-15s\n", "手术室", "当前状态", "主刀医生", "患者编号");
                print_divider();
                
                OperatingRoomNode* curr_or = or_head;
                int or_count = 0;
                
                // 閬嶅巻杈撳嚭绯荤粺鍏ㄥ眬鐨勬墜鏈鍔ㄦ€?
                while (curr_or) {
                    const char* st_text;
                    if (curr_or->status == 0) st_text = COLOR_GREEN "空闲待命" COLOR_RESET;
                    else if (curr_or->status == 1) st_text = COLOR_RED "手术抢救中" COLOR_RESET;
                    else st_text = COLOR_YELLOW "消毒维护" COLOR_RESET;
                    
                    printf("%-10s %-12s %-15s %-15s\n", curr_or->room_id, st_text, curr_or->current_doctor, curr_or->current_patient);
                    curr_or = curr_or->next;
                    or_count++;
                }
                
                if (or_count == 0) {
                    printf(COLOR_YELLOW "当前系统尚未初始化手术室数据，或无可用手术室。\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
        }
    }
}
