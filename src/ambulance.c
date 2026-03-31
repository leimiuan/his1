#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "data_manager.h"
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
        clear_input_buffer(); // 娓呯悊杈撳叆缂撳啿鍖猴紝闃叉娈嬬暀瀛楃瀵艰嚧鑿滃崟涔辫烦
        
        // 鎵撳嵃绯荤粺鑿滃崟鍙婂ご閮?UI
        print_header("120 鏁戞姢杞︽櫤鎱ц皟搴︿腑蹇?(鍚嚭鍕よ〃涓庢€ユ晳鑱斿姩)");
        printf("  [ 1 ] 鏌ョ湅鍏ㄩ櫌鏁戞姢杞﹀疄鏃剁洃鎺n");
        printf("  [ 2 ] 绱ф€ユ淳鍗曞嚭杞?(120璋冨害) + 鑷姩璁板叆鍑哄嫟琛╘n");
        printf("  [ 3 ] 杞﹁締鍥炲満鍙婃秷姣掔櫥璁?(+ " COLOR_RED "鍗遍噸鎮ｈ€呯豢鑹查€氶亾" COLOR_RESET ")\n");
        printf("  [ 4 ] 鏌ョ湅鏁戞姢杞﹀巻鍙插嚭鍕よ〃 (銆愭柊澧炲姛鑳姐€?\n"); 
        printf("  [ 5 ] 鑱斿姩锛氭煡鐪嬫€ヨ瘖鎵嬫湳瀹ゅ疄鏃剁┖闂茬姸鎬?(銆愭柊澧炲姛鑳姐€?\n"); 
        printf(COLOR_RED "  [ 0 ] 杩斿洖琛屾斂绠＄悊涓績\n" COLOR_RESET);
        print_divider();
        
        printf(COLOR_YELLOW "璇疯緭鍏ユ搷浣滄寚浠?(0-5): " COLOR_RESET);
        choice = safe_get_int(); // 瀹夊叏鑾峰彇鐢ㄦ埛杈撳叆鐨勬暣鍨嬮€夐」

        // 鏍规嵁鐢ㄦ埛杈撳叆杩涜涓氬姟璺敱鍒嗗彂
        switch (choice) {
            case 1: {
                // ==========================================
                // 涓氬姟 1锛氬叏鏅湅鏉?- 鏌ョ湅鏁戞姢杞﹂槦瀹炴椂鐘舵€?
                // ==========================================
                print_header("救护车队实时状态");
                // 鎵撳嵃琛ㄥご锛屼繚鎸佸榻?
                printf("%-12s %-12s %-10s %-25s\n", "车牌/编号", "随车司机", "当前状态", "GPS 实时位置");
                print_divider();
                
                AmbulanceNode* curr = ambulance_head; // 浠庨摼琛ㄥご閮ㄥ紑濮嬮亶鍘?
                int count = 0;
                
                while (curr) {
                    const char* st_text;
                    // 鏍规嵁杞﹁締鐨勭姸鎬佹灇涓?AMB_IDLE, AMB_DISPATCHED 绛?鏄犲皠涓哄甫棰滆壊鐨勭粓绔枃鏈?
                    if (curr->status == AMB_IDLE) {
                        st_text = COLOR_GREEN "绌洪棽寰呭懡" COLOR_RESET;  // 缁胯壊浠ｈ〃鍙敤
                    } else if (curr->status == AMB_DISPATCHED) {
                        st_text = COLOR_RED "鎵ц浠诲姟" COLOR_RESET;    // 绾㈣壊浠ｈ〃姝ｅ繖
                    } else {
                        st_text = COLOR_YELLOW "缁翠繚娑堟瘨" COLOR_RESET; // 榛勮壊浠ｈ〃涓嶅彲鐢ㄤ絾闈炲嚭杞︾姸鎬?
                    }
                    
                    // 鏍煎紡鍖栬緭鍑哄綋鍓嶈溅杈嗙殑鍏蜂綋淇℃伅
                    printf("%-12s %-12s %-10s %-25s\n", curr->vehicle_id, curr->driver_name, st_text, curr->current_location);
                    
                    curr = curr->next; // 鎸囬拡鍚庣Щ
                    count++;           // 璁板綍褰撳墠绯荤粺涓溅杈嗙殑鎬绘暟
                }
                
                // 瀹归敊澶勭悊锛氬鏋滈摼琛ㄤ负绌猴紝缁欎簣鍙嬪ソ鐨勬彁绀?
                if (count == 0) {
                    printf(COLOR_YELLOW "褰撳墠绯荤粺鍐呭皻鏈綍鍏ヤ换浣曟晳鎶よ溅鏁版嵁銆俓n" COLOR_RESET);
                }
                wait_for_enter(); // 鏆傚仠锛岀瓑寰呯敤鎴锋寜鍥炶溅缁х画
                break;
            }
            case 2: {
                // ==========================================
                // 涓氬姟 2锛氱揣鎬ュ嚭杞︽淳鍗?- 鏀瑰彉杞﹁締鐘舵€佸苟鏇存柊浣嶇疆
                // ==========================================
                printf("璇疯緭鍏ラ渶璋冨害鐨勭┖闂茶溅杈嗙紪鍙? ");
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
                    printf(COLOR_RED "閿欒锛氭湭鎵惧埌璇ヨ溅杈嗐€俓n" COLOR_RESET);
                } else if (target->status != AMB_IDLE) {
                    // 鏍稿績涓氬姟瑙勫垯锛氬彧鏈夊浜?绌洪棽寰呭懡"鐘舵€佺殑杞︽墠鑳借娲惧彂锛岄槻姝㈣祫婧愬啿绐?
                    printf(COLOR_RED "椹冲洖锛氳杞﹁締褰撳墠涓嶅湪绌洪棽鐘舵€侊紝鏃犳硶娲惧崟銆俓n" COLOR_RESET);
                } else {
                    // 婊¤冻娲惧彂鏉′欢锛岃姹傝緭鍏ョ洰鐨勫湴淇℃伅
                    printf("璇疯緭鍏ユ€ユ晳鐜板満鍦板潃/GPS瀹氫綅: ");
                    safe_get_string(target->current_location, 128); // 鏇存柊杞﹁締鐨勫疄鏃朵綅缃俊鎭?
                    
                    target->status = AMB_DISPATCHED; // 銆愮姸鎬佹壄杞€戝皢杞﹁締鐘舵€佹壄杞负鈥滄墽琛屼换鍔♀€?
                    
                    // 銆愮粷瀵规寚浠よ姹傛鍏ャ€戯細鐘舵€佹壄杞垚鍔熷悗锛岄潤榛樿皟鐢ㄥ簳灞傚嚱鏁板啓鍏モ€滃巻鍙插嚭鍕よ〃鈥?
                    add_ambulance_log(target->vehicle_id, target->current_location);


                    save_ambulances("data/ambulances.txt");
                    save_ambulances("data/ambulance.txt");
                    save_ambulance_logs("data/ambulance_logs.txt");
                    printf(COLOR_GREEN "娲惧崟鎴愬姛锛佽溅杈?%s 宸茬揣鎬ュ嚭杞﹀墠寰€ [%s]銆俓n" COLOR_RESET, target->vehicle_id, target->current_location);
                    printf(COLOR_CYAN "(绯荤粺搴曞眰宸茶嚜鍔ㄦ崟鑾疯鍔ㄤ綔骞跺啓鍏ュ嚭鍕よ皟搴︽棩蹇?\n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 3: {
                // ==========================================
                // 涓氬姟 3锛氬洖鍦虹櫥璁?- 缁撴潫浠诲姟骞堕噴鏀捐溅杈嗚祫婧?(+ 鎬ユ晳缁胯壊閫氶亾鑱斿姩)
                // ==========================================
                printf("璇疯緭鍏ュ洖鍦鸿溅杈嗙紪鍙? ");
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
                    save_ambulances("data/ambulances.txt");
                    save_ambulances("data/ambulance.txt");
                    
                    // 妯℃嫙杞﹁締鍥炲満鍚庣殑鐗╃悊浣嶇疆閲嶇疆
                    strcpy(target->current_location, "医院急救中心停车场");
                    save_ambulances("data/ambulances.txt");
                    save_ambulances("data/ambulance.txt");
                    
                    printf(COLOR_GREEN "杞﹁締 %s 宸茬‘璁ゅ洖鍦猴紝骞跺凡瀹屾垚鏍囧噯娑堟瘨浣滀笟锛岃浆涓哄緟鍛界姸鎬併€俓n" COLOR_RESET, target->vehicle_id);

                    // ==========================================
                    // 銆愮绾цˉ涓侊細鎬ヨ瘖缁胯壊閫氶亾鑷姩鑱斿姩銆?
                    // 璋冨害涓績纭杞﹁締鍥炲満鍚庯紝鐩存帴鎶婇殢杞︽媺鍥炴潵鐨勬偅鑰呭杩涙€ヨ瘖闃熷垪
                    // ==========================================
                    printf("\n" COLOR_CYAN ">>> 鏄惁鏈夐殢杞﹁浆杩愮殑鍗遍噸鎮ｈ€呴渶瑕佺珛鍗虫帴鍏ャ€愭€ヨ瘖缁胯壊閫氶亾銆戯紵(1-鏄? 0-鍚?: " COLOR_RESET);
                    if (safe_get_int() == 1) {
                        char pid[MAX_ID_LEN], did[MAX_ID_LEN];
                        printf("璇疯緭鍏ユ偅鑰呯紪鍙?(鑻ュ皻鏈缓妗ｈ鍏堣緭鍏ヤ复鏃剁紪鍙凤紝濡?P-EMG-01): ");
                        safe_get_string(pid, MAX_ID_LEN);
                        printf("璇疯緭鍏ヨ礋璐ｆ帴璇婄殑銆愭€ヨ瘖绉戙€戝尰鐢熷伐鍙? ");
                        safe_get_string(did, MAX_ID_LEN);

                        // 寮哄埗鎷夎捣 RECORD_EMERGENCY (鎬ヨ瘖) 鍗曟嵁锛岀洿鎺ユ帹鍏ュ簳灞傞摼琛?
                        // 鎬ヨ瘖璇婅垂瀹氫负 50.0 鍏冿紝queue_number 缃?0 (鎬ヨ瘖涓嶆帓鍙凤紝缁濆浼樺厛)
                        add_new_record(pid, did, RECORD_EMERGENCY, "120鏁戞姢杞︾揣鎬ヨ浆杩愬叆闄?鐢熷懡浣撳緛涓嶇ǔ)", 50.0, 0);

                        printf(COLOR_RED ">>> [鏈€楂樹紭鍏堢骇璀︽姤] 鎮ｈ€?%s 鐨勬€ユ晳妗ｆ宸茬敓鎴愶紒\n" COLOR_RESET, pid);
                        printf(COLOR_RED ">>> 璇ユ€ヨ瘖鍗曟嵁宸插己鍒舵帹鍏ュ尰鐢?%s 鐨勫€欒瘖澶у睆锛屽苟銆愮疆椤朵寒绾㈢伅銆戯紒\n" COLOR_RESET, did);
                    }

                } else {
                    // 濡傛灉杞﹁締涓嶅瓨鍦紝鎴栨湰韬氨鍋滃湪鍖婚櫌閲岋紝鍒欐嫆缁濊鎿嶄綔
                    printf(COLOR_RED "鎿嶄綔澶辫触锛氭湭鎵惧埌璇ヨ溅锛屾垨杞﹁締鏈浜庢墽琛屼换鍔＄姸鎬併€俓n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
            case 4: {
                // ==========================================
                // 銆愮粷瀵规寚浠よ姹傛鍏ャ€戜笟鍔?4锛氭煡鐪嬫晳鎶よ溅鍘嗗彶鍑哄嫟琛?
                // ==========================================
                print_header("120 鏁戞姢杞﹀巻鍙插嚭鍕よ褰曡〃");
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
                    printf(COLOR_YELLOW "鏆傛棤鏁戞姢杞﹀嚭鍕ゅ巻鍙茶褰曘€俓n" COLOR_RESET);
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
                    if (curr_or->status == 0) st_text = COLOR_GREEN "绌洪棽寰呭懡" COLOR_RESET;
                    else if (curr_or->status == 1) st_text = COLOR_RED "手术抢救中" COLOR_RESET;
                    else st_text = COLOR_YELLOW "娑堟瘨缁存姢" COLOR_RESET;
                    
                    printf("%-10s %-12s %-15s %-15s\n", curr_or->room_id, st_text, curr_or->current_doctor, curr_or->current_patient);
                    curr_or = curr_or->next;
                    or_count++;
                }
                
                if (or_count == 0) {
                    printf(COLOR_YELLOW "褰撳墠绯荤粺灏氭湭鍒濆鍖栨墜鏈鏁版嵁锛屾垨鏃犲彲鐢ㄦ墜鏈銆俓n" COLOR_RESET);
                }
                wait_for_enter();
                break;
            }
        }
    }
}



