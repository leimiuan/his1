#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "common.h"

/* ==========================================
 * 鏉垮潡涓€锛氱郴缁熸暟鎹垵濮嬪寲涓庢枃浠舵寔涔呭寲 (File I/O)
 * 銆愬鏍囬绛?10銆戯細銆愬瓨鍌ㄣ€戣兘澶熷皢褰撳墠绯荤粺涓殑鎵€鏈変俊鎭繚瀛樺埌鏂囦欢涓€?
 * 鐩爣锛氬疄鐜?10 澶ф牳蹇冨崟鍚戦摼琛ㄤ笌鏈湴 .txt 鏂囦欢鐨勫弻鍚戞棤鎹熷悓姝?
 * ========================================== */

// 缁熶竴鎬绘帶锛氱▼搴忓惎鍔ㄦ椂鍔犺浇鎵€鏈夋暟鎹紝閫€鍑烘椂鎵ц闈欓粯淇濆瓨
void init_system_data();
void save_system_data();

// --- 鏁戞姢杞︿笌鎺掔彮鏂囦欢搴曞眰淇濋殰 ---
void ensure_duty_schedule_file();

// --- 鍚勬ā鍧楀姞杞藉嚱鏁?(浠?txt 璇诲彇鏁版嵁骞朵娇鐢ㄥご鎻掓硶鏋勫缓鍐呭瓨閾捐〃) ---
void load_departments(const char* filepath);
void load_doctors(const char* filepath);
void load_medicines(const char* filepath);
void load_outpatients(const char* filepath);
void load_inpatients(const char* filepath);
void load_beds(const char* filepath);
void load_records(const char* filepath);
void load_ambulances(const char* filepath); 
void load_ambulance_logs(const char* filepath);  // 鏁戞姢杞﹀嚭鍕ゆ棩蹇楁寔涔呭寲鍔犺浇
void load_operating_rooms(const char* filepath); // 銆愭柊澧炪€戯細鎵嬫湳瀹ょ姸鎬佹寔涔呭寲鍔犺浇

// --- 鍚勬ā鍧椾繚瀛樺嚱鏁?(閬嶅巻閾捐〃骞舵寜鍒惰〃绗?Tab 鏍煎紡杩涜鏍煎紡鍖栬惤鐩? ---
void save_departments(const char* filepath);
void save_doctors(const char* filepath);
void save_medicines(const char* filepath);
void save_outpatients(const char* filepath);
void save_inpatients(const char* filepath);
void save_beds(const char* filepath);
void save_records(const char* filepath);
void save_ambulances(const char* filepath); 
void save_ambulance_logs(const char* filepath);  // 鏁戞姢杞﹀嚭鍕ゆ棩蹇楄惤鐩樹繚瀛?
void save_operating_rooms(const char* filepath); // 銆愭柊澧炪€戯細鎵嬫湳瀹ょ姸鎬佽惤鐩樹繚瀛?

/* ==========================================
 * 鏉垮潡浜岋細鏍稿績涓氬姟鏌ヨ涓庢搷浣滄帴鍙?(CRUD)
 * 鐩爣锛氬睆钄藉簳灞傚崟鍚戦摼琛ㄧ殑閬嶅巻缁嗚妭锛屽悜涓婂眰涓氬姟鏆撮湶楂樺彲闈犵殑鏁版嵁璁块棶鑳藉姏
 * ========================================== */

// --- 鍩虹瀹炰綋 ID 绮惧噯鏌ヨ (杩斿洖瀵瑰簲鍐呭瓨鑺傜偣鎸囬拡) ---
PatientNode* find_outpatient_by_id(const char* id);
PatientNode* find_inpatient_by_id(const char* id);
DoctorNode* find_doctor_by_id(const char* id);
DepartmentNode* find_department_by_id(const char* id);
MedicineNode* find_medicine_by_id(const char* id);
BedNode* find_bed_by_id(const char* id);
AmbulanceNode* find_ambulance_by_id(const char* id); 

// --- 澶嶆潅涓氬姟閫昏緫妫€绱笌鎿嶄綔 ---

// 妯＄硦鎼滅储锛氭敮鎸侀€氳繃鑽搧閫氱敤鍚嶆垨鍟嗗搧鍚嶈繘琛屽叧閿瘝妫€绱?
void search_medicine_by_keyword(const char* keyword);

// 璧勬簮鑷姩鍒嗛厤锛氭煡鎵炬寚瀹氱瀹ゅ強鐥呮埧绫诲瀷(濡?WARD_SPECIAL 鍗?ICU)鐨勭┖闂插簥浣?
BedNode* find_available_bed(const char* dept_id, WardType type);

/**
 * @brief 銆愯崳瑾夌彮楂橀樁搴曞眰鎸戞垬锛氶摼琛ㄧ骇鐗╃悊鎼銆?
 * 闂ㄨ瘖鎮ｈ€呰浆浣忛櫌鏃讹紝灏嗗叾浠?outpatient_head 閾捐〃鐗╃悊鎽橀櫎锛?
 * 骞朵娇鐢ㄥご鎻掓硶閲嶆柊鎸傝浇鑷?inpatient_head 閾捐〃涓紝涓ュ畧鈥滃叏绋嬮摼琛ㄢ€濊鑼冦€?
 * @return 1 鎴愬姛锛? 澶辫触
 */
int transfer_outpatient_to_inpatient(const char* patient_id);

/**
 * @brief 鏍稿績娴佹按璁板綍鐢熸垚寮曟搸
 * 銆愬鏍囬绛?4銆戯細闇€鑷璁捐鎸傚彿鐨勭紪鐮佽鍒欙紝鍙殣鍚寕鍙锋棩鏈熴€佹寕鍙峰綋澶╃殑椤哄簭鍙枫€佸搴旂殑鍖荤敓绛変俊鎭紝淇濊瘉鍞竴銆?
 * 姝ゅ嚱鏁板唴閮ㄥ凡閲嶆瀯涓虹敓鎴?"YYYYMMDD-鎺掑彿-鍖荤敓宸ュ彿" 鏍煎紡鐨勫鍚堝敮涓€鍗曟嵁鍙枫€?
 */
void add_new_record(const char* patient_id, const char* doctor_id, RecordType type, 
                    const char* description, double fee, int queue_num);

/* ==========================================
 * 鏉垮潡涓夛細鍖荤敓鍑鸿瘖涓庡€肩彮绠＄悊
 * ========================================== */

// 鏌ョ湅鎸囧畾鍖荤敓鎴栧叏闄㈢殑鍊肩彮瀹夋帓琛?
void view_duty_schedule(const char* doctor_id_filter);
int is_doctor_on_duty_today(const char* doctor_id);

// 鏇存柊/鏂板鎺掔彮璁板綍 (鏃ユ湡 YYYY-MM-DD, 鐝 涓婂崍/涓嬪崍/鍏ㄥぉ)
int upsert_duty_schedule(const char* date, const char* slot, const char* dept_id, const char* doctor_id);

// 绉婚櫎鎸囧畾鐨勬帓鐝褰?
int remove_duty_schedule(const char* date, const char* slot, const char* dept_id);

/* ==========================================
 * 鏉垮潡鍥涳細鍐呭瓨涓庣敓鍛藉懆鏈熺鐞?
 * 鐩爣锛氬湪绋嬪簭瀹夊叏閫€鍑烘椂褰诲簳娓呯悊鍫嗗唴瀛橈紝闃叉鎿嶄綔绯荤粺鍐呭瓨娉勯湶 (Memory Leak)
 * ========================================== */

// 渚濇閬嶅巻 10 澶у叏灞€鍗曞悜閾捐〃锛屾墽琛?free() 閲婃斁姣忎竴涓妭鐐?
void free_all_data();

/* ==========================================
 * 鏉垮潡浜旓細鎬ユ晳涓庨噸鐥囪祫婧愯仈鍔?(銆愮粷瀵规寚浠ゆ柊澧炪€?
 * 鐩爣锛氭毚闇叉晳鎶よ溅鍑哄嫟鏃ュ織涓庢墜鏈鐘舵€佹祦杞殑鏍稿績搴曞眰鎺ュ彛
 * ========================================== */

/**
 * @brief 鐢熸垚骞惰褰曚竴鏉℃晳鎶よ溅鍑哄嫟鏃ュ織
 * 鐢ㄤ簬鍦ㄨ皟搴︿腑蹇冩垨鎮ｈ€呯娲惧彂鏁戞姢杞︽椂闈欓粯璋冪敤锛屽舰鎴愬彲杩芥函鐨勫巻鍙插嚭鍕よ〃銆?
 * @param vehicle_id 鏁戞姢杞︾紪鍙?
 * @param destination 鐩殑鍦?
 */
void add_ambulance_log(const char* vehicle_id, const char* destination);

#endif // DATA_MANAGER_H

