#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "common.h"
#include "data_manager.h"
#include "utils.h"

/* ==========================================
 * 闂傚倷绶氬褍螞濞嗘挸绀夐柡宥庡幗閸嬪鎮楅崷顓炐ラ柣銈傚亾闂備礁鎲￠崝蹇涘疾濞戙垺鍎婇柣鐔稿櫞瑜版帗鍋愭い鏂跨毞閸嬫捇寮介鍕喘瀵濡烽妷褜妲梻渚€娼ч¨鈧紒鍙夋そ瀹曟垿骞樼拠鑼吅闂佺粯鍔曢悺銊╂偟閹绢喗鈷戠痪顓炴噽閸亪鏌涙繝鍐槈闁崇懓鍟村畷銊╁级閹寸媭妲遍梻浣告啞缁诲倻鈧凹鍓熼幃鐢稿醇閺囩喓鍘搁梺鍛婂姧缁犳垿宕濆杈╃＜缂備焦蓱閳锋劗绱掔€ｎ亶妲归柟顖涙婵偓闁绘瑢鍋撻柡澶婄秺濮婃椽宕崟顓炩拡闂佹悶鍔嬮崡鎶姐€佸Ο鑽ょ瘈婵﹩鍘煎▓鐔兼⒑缁嬫寧婀扮紒顔兼湰缁傛帡顢涢悙鏉戔偓鐢告偡濞嗗繐顏柣鎿冨灣缁?
 * ========================================== */
#ifndef HIS_EXT_NODES_DEFINED
#define HIS_EXT_NODES_DEFINED
/**
 * @brief 闂傚倷娴囧銊╂倿閿曞倸鍨傞柤绋跨仛閸欏繘鏌曡箛濠傚⒉濠殿喗绮撻弻銊╁即濡も偓娴滈箖姊洪幖鐐测偓銈夊矗閸愩劎鏆﹂柣鏃傚帶鍥撮梺绯曞墲閿曨偆妲愬鑸殿棅妞ゆ劑鍨烘径鍕亜閵娿儺妲瑰ǎ鍥э躬椤㈡﹢濮€閳ユ枼鍋?
 * 闂傚倷鐒﹀鍨焽閸ф绀夐悗锝庡墲婵櫕銇勯幒鎴濃偓瑙勫垔婵傚憡鐓涘璺哄绾墎绱掓径濠冨仴闁哄备鍓濋幏鍛存偡閹殿喕妗撳┑鐘愁問閸犳捇宕归崫鍕电劷妞ゅ繐鐗婇弲婊堟煟閹伴潧澧伴柡鍡欏Т閳规垿顢氶崱娆忓煂缂備浇顕ч鍥╁垝閳哄懎绾ч柟瀛樻⒐閻庡姊洪弬銉︽珔闁哥噥鍨堕幃婊堟晝閸屾稓鍘卞┑鐐叉閸旀洟鎮橀妷褉鍋撻崷顓х劸妞ゎ厼鍢查悾鐑芥晲閸ャ劌鐝伴悷婊冪箳缁牊寰勭仦鎯ф瀾闂佺厧澹婇崜娆撴倶椤忓牊鐓曢柕濠忕畱閸濊櫣鈧娲╃紞渚€銆佸☉妯锋斀闁搞儜灞肩处濠电姷鏁告慨宥囦沪閻ｅ奔鍖栨繝鐢靛仜閻楀﹪宕归崹顔炬殾婵炲棙鎸告导鐘绘煕閺囥劌浜炵憸浼寸畺閺岋綁鎮欑€电硶妫ㄩ梺鍛婃煥閿曨亪宕洪埀?
 */
typedef struct AmbulanceLogNode {
    char log_id[32];            // 闂傚倷绀侀幖顐﹀疮閵娾晛纾块弶鍫氭櫆瀹曟煡鏌￠崘銊у缁绢厸鍋撻梻浣告惈濞层劑宕戝☉娆戠?(闂傚倷鑳剁涵鍫曞疾閻愬樊娴栭柕濞у棗小濡炪倖甯掔€氼剛鐚惧澶嬬厱妞ゆ劧绲剧粈鍐煥濞戞艾鏋涢柡宀€鍠栧鍫曞箣閻欏懐绀婇梻浣芥〃閻掞箓骞戦崶顒傚祦?
    char vehicle_id[32];        // 闂傚倷绀佸﹢閬嶆偡閹惰棄骞㈤柍鍝勫€归弶鎼佹⒒娴ｅ憡鎯堥柣妤佺矒瀹曟粌顫濇０婵囨櫇婵犻潧鍊婚…鍫㈢矆閸曨垱鐓曟繝闈涙椤忊晠鏌ｅ┑濠冪【闂囧鏌ｅΟ纭咁劅闁搞倖鐟ラ埞鎴︻敊閹勭亪闂佽鍨伴崯鏉戠暦閻旂⒈鏁婇柣锝呯灱濞夋繈姊洪崫鍕垫Ц闁绘妫楄灋婵炴垶纰嶉～鏇熴亜韫囨挾澧曢柣?
    char dispatch_time[32];     // 濠电姷鏁搁崑娑橆嚕閸洖纾块悗鍦О娴滃綊鏌涘畝鈧崑娑氱尵瀹ュ鐓曟い鎰剁稻缁€鍐煥?(闂傚倷绀侀幖顐ょ矓閸洖鍌ㄧ憸蹇撐? YYYY-MM-DD HH:MM:SS)
    char destination[128];      // 闂傚倷鑳堕崕鐢稿疾閳哄懎绐楁俊銈呮噺閸嬪鏌ㄥ┑鍡╂Ц闁圭懓鐖奸弻鐔封枔閸喗鐏堟繛?
    struct AmbulanceLogNode* next; // 闂傚倷绀侀幉锟犮€冮崱妞曟椽寮介鐐电暫濠电姴锕ら悧濠囨偂閿濆鐓欓悷娆忓婵倿鏌嶈閸撴瑩宕幘顔惧祦闁圭増婢橀悙濠冦亜閹哄秶顦﹂柣?
} AmbulanceLogNode;

/**
 * @brief 闂傚倷绀佺紞濠傤焽瑜旈、鏍幢濡ゅ鈧法鈧娲栧ú鐘诲磻閹捐崵宓侀柛顭戝枓閸嬫挸鈽夊▎搴ｇ畾濡炪倖鎸堕崹娲磹?
 * 闂傚倷鐒﹀鍨焽閸ф绀夐悗锝庡墲婵櫕銇勯幒鎴濃偓缁樼▔瀹ュ鐓ユ繛鎴灻顏堟煕婵犲嫬浠遍柡灞剧缁犳稓鈧綆浜滈～鍥р攽閻愬弶鍣归柣顓炲€块悰顔碱吋閸ワ絽浜鹃柨婵嗛娴滄繈姊虹憴锝嗘珚闁诡喗顨婇悰顕€宕归鐟颁壕婵°倐鍋撻柍缁樻崌楠炲鎮╅顫闂侀潻瀵岄崢鍏肩閿旀垝绻嗛柣鎰絻閳ь剙娼￠獮鍐煛閸涱喖娈濈紒鍓у閿氬ù婊勫劤闇夐柨婵嗩樈濡垿鏌ｉ悢鏉戠伈闁诡喖婀遍埀顒婄秵娴滄粓寮冲▎鎾村€电痪顓炴噺閻濐亞绱掑畝鍕喚闁糕晛瀚板畷妯款槾缂佲偓閳ь剟姊?
 */
typedef struct OperatingRoomNode {
    char room_id[32];           // 闂傚倷绀佺紞濠傤焽瑜旈、鏍幢濡ゅ鈧法鈧娲栧ú鐘诲磻閹捐崵宓侀柛顭戝枓閸嬫挸螖閸愨晩娼熷銈呯箰閻楀棝鎯?(婵?OR-01)
    int status;                 // 闂傚倷鑳剁划顖炩€﹂崼銉ユ槬闁哄稁鍘奸悞鍨亜閹达絾纭堕柛鏂跨У缁绘繆顦柛?- 0: 缂傚倸鍊风粈渚€寮甸鈧—鍐寠婢? 1: 闂傚倷绀佺紞濠傤焽瑜旈、鏍幢濡ゅ鈧潡鏌ц箛锝呬簽闁? 2: 濠电姷鏁搁崑鐐哄垂閻㈠憡鍋嬪┑鐘叉处閸庡﹪鏌曟繛褍鎳愰敍婊堟煟鎼淬垻鈯曢柨姘辨偖?
    char current_patient[32];   // 闂佽崵鍠愮划搴㈡櫠濡ゅ懎绠伴柛娑橈攻濞呯娀鏌ｅΟ娆惧殭缂佺姵鐗犻弻銊モ攽閸℃瑥顣甸梺绋款儐閹瑰洤鐣烽妸鈺婃晬婵犲﹤瀚ㄦ禒銏ゆ⒒?
    char current_doctor[32];    // 闂佽崵鍠愮划搴㈡櫠濡ゅ懎绠伴柛娑橈攻濞呯娀鏌ｅΟ铏癸紞闁崇粯妫冮幃褰掑炊閵娿儳绁烽梺璇插瘨閸撶喖寮诲☉銏犲唨妞ゆ巻鍋撻柛姘秺閺屸剝鎷呯憴鍕殫缂備浇椴搁幑鍥ь嚕閸婄噥妲绘繝?
    struct OperatingRoomNode* next; // 闂傚倷绀侀幉锟犮€冮崱妞曟椽寮介鐐电暫濠电姴锕ら悧濠囨偂閿濆鐓欓悷娆忓婵倿鏌嶈閸撴瑩宕幘顔惧祦闁圭増婢橀悙濠冦亜閹哄秶顦﹂柣?
} OperatingRoomNode;
#endif

/* ==========================================
 * 1. 闂傚倷鑳堕…鍫㈡崲閸儱绀夌€光偓閸曨剙鍓冲銈嗗笒鐎氼剟寮告笟鈧弻锝呂旈埀顒勬偋閸℃瑧鐭堥柨鏇炲€归埛鎴︽煟閹伴潧澧紒鈧崘顔界厪闁糕剝顨呴弳鐐碘偓鍨緲鐎氼噣鍩€椤掍胶鈯曢柨姘舵煟閿曗偓缂嶅﹤顫?(100% 闂備礁鎼ˇ顐﹀疾濠婂牊鏅梺?
 * 婵犵數鍋犻幓顏嗗緤閸撗冨灊闁割偁鍨虹€氬鏌ｉ弮鍌氬付闁哄鑳剁槐鎾诲醇閵忕姭鏋呴梺?HIS 缂傚倸鍊风欢锟犲垂闂堟稓鏆﹂柣銏ゆ涧閸ㄦ繃绻涘顔荤凹闁稿骸绉归弻娑㈠即閵娿儰绨婚梺杞扮閻栧ジ骞冨Δ鈧埥澶娾枍閾忣偄鐏寸€殿喗濞婇幃銏ゆ偂鎼达綆鍞规俊鐐€栧Λ渚€宕戦幇顒夋僵闁靛ň鏅滈悡娑氣偓骞垮劚濞村倿寮ㄩ弶搴撴斀闁宠棄妫楀顕€鏌?
 * ========================================== */
DepartmentNode* dept_head = NULL;       
DoctorNode* doctor_head = NULL;         
MedicineNode* medicine_head = NULL;     
PatientNode* outpatient_head = NULL;    // 闂傚倸鍊搁崐鎼佸磹閸洖绀夐煫鍥ㄧ⊕閸庡﹪鏌″搴″箹缂佺姵鐗犻弻銊モ攽閸℃瑥顣甸梺绋款儐閹瑰洤鐣烽妸鈺婃晣婵犲ň鍋撴繛瀵稿厴濮婃椽宕ㄦ繝鍛棟缂備礁顦板娆撴偩?
PatientNode* inpatient_head = NULL;     // 婵犵數鍋犻幓顏嗗緤閻ｅ本宕叉俊顖濄€€閺嬪秶鈧厜鍋撻柛鏇ㄥ亞閻ｆ椽姊洪幐搴ｇ畵闁绘鍟村畷鎴﹀箻鐠囨彃绐涘銈嗘尰缁牆霉閻戣姤鈷戦柛娑橈龚婢规绱掓径瀣稇闁?
BedNode* bed_head = NULL;               
RecordNode* record_head = NULL;         
AmbulanceNode* ambulance_head = NULL;   

// 闂傚倷绶氬褍螞濞嗘挸绀夋俊銈呭暙閸ㄦ繂顪冪€ｎ亞宀搁柛瀣崌閹棄鈻撻幐搴㈡殲婵＄偑鍊愰弲顏嗙不閹寸偞娅忛梻浣侯焾閺堫剛鍠婂澶婄畾婵炲棙鎸哥粻褰掓倵濞戞鎴λ夐弽顐ょ＜闁告瑦锚瀹撳棛鈧鍣崜鐔奉嚕鐠鸿　鏋庨柣鎰靛墯缁额噣姊绘担鍛婃儓缂佸鐖奸垾锕傚醇閵忋垻澶勬俊銈忕到閸燁偊寮告笟鈧弻锝呂旈埀顒勬偋閸℃瑧鐭堥柨鏃傛櫕缁♀偓閻熸粌閰ｅ畷婵嬪箣閿曗偓閻?
AmbulanceLogNode* ambulance_log_head = NULL; // 闂傚倷娴囧銊╂倿閿曞倸鍨傞柤绋跨仛閸欏繘鏌曡箛濠傚⒉濠殿喗绮撻弻銊╁即濡も偓娴滈箖鎮楅崷顓х劸妞ゎ厼鍢查悾鐑芥晲婢跺﹦顔岄梺鍦劋閸ㄥ爼鎯侀弮鍫熲拺闁告稑锕ョ壕鎼佹煕婵犲啰娲村┑鈩冩尦瀹曟﹢鍩℃担铏圭嵁闂佽瀛╃粙鎺楁晪闂佷紮绠戠紞濠傤潖?
OperatingRoomNode* or_head = NULL;           // 闂傚倷绀佺紞濠傤焽瑜旈、鏍幢濡ゅ鈧法鈧娲栧ú鐘诲磻閹捐崵宓侀柛顭戝枓閸嬫捇寮撮姀鈥充痪闂佸憡娲﹂崹鎵尵瀹ュ鐓欐い鏍ㄧ☉椤ュ鏌曢崼顐簼缂佺粯绻嗛ˇ鎶芥煕閺冣偓濞茬喐淇婇悽绋跨睄闁割偅绻勯鍥⒑閹肩偛鍔撮柛鎾存皑閳?

/* ==========================================
 * 2. 闂傚倷绀侀幉锟犲礉閺囥垹绠犳慨妞诲亾鐎规洘娲滈埀顒佺⊕钃辩紓宥呮喘閺屾稑鈽夐崡鐐茬閻炴氨濞€閹鎲撮崟顒€浠╅梺绋块叄娴滆泛鐣烽敐澶婇敜婵°倐鍋撶紒鈧崟顖涚厾闁诡厽甯掗崝婊勭箾?(闂傚倷鑳堕…鍫㈡崲閸儱绀夐幖杈剧到閸ㄦ繈鐓崶銊р姇闁搞倕绉归弻娑氫沪閸撗呯暭缂備胶濮烽崗姗€寮?
 * ========================================== */

/**
 * @brief 闂備浇顕уù鐑藉箠閹惧嚢鍥敍閻愯尙鐓戦棅顐㈡处缁嬫帡宕曞澶嬬厱闁哄洢鍔屾禍鐐烘煙閻ｅ苯鏋旂紒杈ㄥ笒铻栭悗锝庡墰缁愭鏌ｉ悢鍛婄婵☆偅绻堥悰顔跨疀濞戞瑥浠惧銈嗙墬缁姴危閻戣姤鍊甸悷娆忓閸嬬娀鏌涙惔銊ゆ喚鐎规洩缍佸畷绋课旀担瑙勭彨闁诲骸鍘滈崑鎾绘煕閺囥劌鍔柍鍝勫€归崣蹇旂節闂堟稑顏╅柣蹇ｅ枤缁辨帡骞夌€ｎ剛鏆ら悗瑙勬礃閿曘垽鐛幘璇茬闁硅揪闄勯鍐╃節閻㈤潧顫掑ù锝呮憸娴犲ジ姊洪幖鐐测偓銈夊储妤ｅ啫鐒垫い鎴ｆ硶缁佸嘲顭块悷鎵ⅵ鐎规洑绲婚ˇ鎾煕閺嶃劎澧甸柟绛圭節婵″爼宕惰閸樼兘鏌ｆ惔銈庢綈婵炴彃绉瑰鎻掝煥閸涙澘小?
 */
static void safe_copy_text(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return;
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

/**
 * @brief 闂備浇宕垫慨鐢稿礉閹达箑鍨傞悷娆忓閸欏繘鏌ｉ弮鍌氬妺闁哥姴妫濋弻娑㈠即閵娿儲鐏堥梺浼欑畵娴滃爼寮婚垾鎰佸悑闊洦娲滈惁鍫ユ⒑缂佹鈯曢柣鐔村劦椤㈡岸鏁愭径濠傚祮闂佺粯鏌ㄩ幉锟犲磿瑜斿娲箰鎼达絺濮囬梺娲诲弾閸犳牠顢氶敐鍛傛棃宕ㄩ鐐叉暏闂備礁鍚嬬粊鎾棘娓氣偓閸┾偓妞ゆ帊瀛ｉ幋鐘电煔?"婵犵數鍋為崹鍫曞箹閳哄倻顩叉繛鍡楄瑜庡鍕箛椤撶偞鐤佹俊鐐€曠换鎰板疮閹殿喗鏆?+ 闂傚倷绀侀幉锟犳偋閻愪警鏁婂┑鐘插瀹曞弶鎱ㄥΟ鎸庣【缂佺媭鍣ｉ弻鏇熷緞閸℃ɑ鐝曢梺?= 闂備浇宕垫慨鐢稿礉瑜忕划濠氬箣濠靛牊娈鹃梺缁樻⒒閸樠呯不閹存績鏀介柣妯哄暱閸ゎ剟鏌?
 * 濠电姷鏁告慨鎾煟閵夆晛围闁告侗鍘介悘鍡椻攽閳藉棗浜炲褎顨婇垾锕傚炊椤掑鏅㈡俊鐐差儏鐎涒晠宕崫鍔藉綊鏁愰崨顔跨濠电姭鍋撻柟缁㈠枟閻撱儲绻涢幋鐐垫噧闁告繃妞介弻娑㈡晲閸パ傚闂佹寧绋掗崝鏇㈠煘閹达箑閱囬柕蹇嬪灩鐎氬海绱撴担楦挎闁告瑥閰ｅ畷鎶筋敍濠婂嫷娼熷┑鐘绘涧濞层劑鍩㈤弮鍌楀亾楠炲灝鍔氶柟鍐叉捣閺侇噣骞掑Δ浣镐化婵炶揪绲介幉锟犲疮閻愬绡€闁逞屽墯閹峰懘鎮滃Ο铏瑰酱婵＄偑鍊栭悧妤冨枈瀹ュ鐓濋柡鍥ュ灪閻?
 */
static void normalize_record_amounts(RecordNode* record) {
    double expected_personal;
    double delta;
    if (!record) return;

    if (record->total_fee < 0) record->total_fee = 0;
    if (record->insurance_covered < 0) record->insurance_covered = 0;

    if (record->insurance_covered > record->total_fee) {
        record->insurance_covered = record->total_fee;
    }

    expected_personal = record->total_fee - record->insurance_covered;
    if (expected_personal < 0) expected_personal = 0;
    delta = (record->insurance_covered + record->personal_paid) - record->total_fee;

    if (record->personal_paid < 0 ||
        record->personal_paid > record->total_fee ||
        delta > 0.0001 || delta < -0.0001) {
        record->personal_paid = expected_personal;
    }
    if (record->personal_paid < 0) record->personal_paid = 0;
}

/**
 * @brief 闂傚倷绀侀幉锛勬暜閸ヮ剙纾归柡宥庡幖閽冪喖鏌涢妷锝呭闁崇粯妫冮弻宥堫檨闁告挻姘ㄧ划娆愬緞閹板灚鏅滈梺鍛婃处閸樹粙骞夐懡銈囩＝濞达綀顕栧▓锝囩磽閸屾稒鐨戦柟韫嵆瀹曞崬鈽夊Ο鍝勬尋婵＄偑鍊栭悧妤冨枈瀹ュ纾垮┑鐘叉处閻撴洟鐓崶褜鍎忓┑鈩冩そ閹綊骞囬鍏肩亖缂備緡鍠楀畝鎼佸箹閸︻厽鍠嗛柛鏇炲悁濡叉劙姊虹拠鎻掝劉缂佸甯￠妴鍐醇閵夘喗鏅?
 */
static int is_blank_text(const char* text) {
    const unsigned char* p = (const unsigned char*)text;
    if (!p) return 1;
    while (*p) {
        if (!isspace(*p)) return 0;
        p++;
    }
    return 1;
}

static WardType infer_bed_type_from_snapshot(const char* bed_id, double daily_fee) {
    if (bed_id && bed_id[0]) {
        switch (toupper((unsigned char)bed_id[0])) {
            case 'U': return WARD_SPECIAL;
            case 'V': return WARD_ESCORTED;
            case 'S': return WARD_SINGLE;
            case 'D': return WARD_DOUBLE;
            case 'T': return WARD_TRIPLE;
            case 'B': return WARD_GENERAL;
            default: break;
        }
    }

    if (daily_fee >= 1000.0) return WARD_SPECIAL;
    if (daily_fee >= 400.0) return WARD_ESCORTED;
    return WARD_GENERAL;
}

static int bed_type_matches_request(WardType actual, WardType requested) {
    if (actual == requested) return 1;

    if (requested == WARD_GENERAL) {
        return actual == WARD_DOUBLE || actual == WARD_TRIPLE;
    }

    if (requested == WARD_ESCORTED) {
        return actual == WARD_SINGLE || actual == WARD_SANATORIUM;
    }

    return 0;
}

/* ==========================================
 * 3. 闂備浇宕垫慨鏉懨洪妶澶嬫櫇闁挎洖鍊搁崙鐘绘煟閺冨倸鍔嬪┑顖氥偢閺屸剝寰勬惔銏€婄紓浣割槸濞硷繝骞冨畡鎵虫瀻婵炲棙鍨硅摫闂備浇顕у锕€螞閸曨垼鏁嬮柨婵嗩槸閸楁娊鏌ｉ弬鎸庡暈闁?(100% 闂備礁鎼ˇ顐﹀疾濠婂牊鏅梺姹囧焺閸ㄥジ宕戦崨顓у殫闁告洦鍋掗崥瀣煕閳╁喚鐒介柛鎺嶅嵆濮婄粯绗熼崶褌绨煎┑鈥冲级鐢偛鈻?
 * 闂傚倷鐒﹀鍨焽閸ф绀夐悗锝庡墲婵櫕銇勯幒鎴濐仼缂佲偓閸岀偞鐓熼柟鎯у暱閻︺劑鏌涢悢閿嬪妞ゃ劊鍎甸幃娆撳级閹存繂笑闁荤偞纰嶉悷鈺呭蓟閿涘嫧鍋撻敐搴′簻濠殿喖娲弻鐔碱敊閸忕厧绐涢梺缁樿壘婢т粙骞忕€ｎ亞鏆﹂柛銉㈡櫆閸犳帡姊洪懡銈呮瀾闁荤喆鍎抽埀顒佸嚬閸ㄦ娊鍩€椤掆偓閻忔岸鎮ч悩璇茬畾闁告劦鍠栫粈瀣亜閹扳晛鐏╂繛鍏煎灴濮?TXT 闂傚倷娴囧銊╂嚄閼稿灚娅犳俊銈傚亾闁伙絽鐏氱粭鐔煎焵椤掆偓椤曪綁顢楅崟顐ゅ€為梺鎸庢⒒閻℃棃宕?
 * ========================================== */

static int parse_record_line_tab(RecordNode* record, char* line) {
    char* fields[16];
    int count = 0;
    char* token = strtok(line, "\t");

    while (token && count < 16) {
        fields[count++] = token;
        token = strtok(NULL, "\t");
    }

    if (count >= 11) {
        int type_val = atoi(fields[3]);
        if (strcmp(fields[0], "记录编号") == 0 || strcmp(fields[1], "患者编号") == 0 || strcmp(fields[2], "医生编号") == 0) return 0;
        safe_copy_text(record->record_id, sizeof(record->record_id), fields[0]);
        safe_copy_text(record->patient_id, sizeof(record->patient_id), fields[1]);
        safe_copy_text(record->doctor_id, sizeof(record->doctor_id), fields[2]);
        record->type = (RecordType)type_val;
        safe_copy_text(record->date, sizeof(record->date), fields[4]);
        record->queue_number = atoi(fields[5]);
        record->status = atoi(fields[6]);
        safe_copy_text(record->description, sizeof(record->description), fields[7]);
        record->total_fee = strtod(fields[8], NULL);
        record->insurance_covered = strtod(fields[9], NULL);
        record->personal_paid = strtod(fields[10], NULL);
        normalize_record_amounts(record);
        return 1;
    }
    
    if (count >= 9) {
        int type_val = atoi(fields[3]);
        if (strcmp(fields[0], "记录编号") == 0 || strcmp(fields[1], "患者编号") == 0 || strcmp(fields[2], "医生编号") == 0) return 0;
        safe_copy_text(record->record_id, sizeof(record->record_id), fields[0]);
        safe_copy_text(record->patient_id, sizeof(record->patient_id), fields[1]);
        safe_copy_text(record->doctor_id, sizeof(record->doctor_id), fields[2]);
        record->type = (RecordType)type_val;
        record->total_fee = strtod(fields[4], NULL);
        record->insurance_covered = strtod(fields[5], NULL);
        record->personal_paid = strtod(fields[6], NULL);
        record->status = atoi(fields[7]);
        safe_copy_text(record->description, sizeof(record->description), fields[8]);
        
        record->queue_number = 0;
        record->date[0] = '\0';
        normalize_record_amounts(record);
        return 1;
    }
    return 0;
}

static int parse_record_line_legacy(RecordNode* record, const char* line) {
    int type_int = 0;
    int status_int = 1;
    
    int parsed = sscanf(line, "%31s %31s %31s %d %31s %d %d %511s %lf %lf %lf",
        record->record_id, record->patient_id, record->doctor_id, &type_int,
        record->date, &record->queue_number, &status_int, record->description,
        &record->total_fee, &record->insurance_covered, &record->personal_paid);
    
    if (parsed >= 8) {
        if (strcmp(record->record_id, "记录编号") == 0 || strcmp(record->patient_id, "患者编号") == 0 || strcmp(record->doctor_id, "医生编号") == 0) return 0;
        record->type = (RecordType)type_int;
        record->status = status_int;
        normalize_record_amounts(record);
        return 1;
    }

    parsed = sscanf(line, "%31s %31s %31s %d %lf %lf %lf %d %511[^\n]",
        record->record_id, record->patient_id, record->doctor_id, &type_int,
        &record->total_fee, &record->insurance_covered, &record->personal_paid,
        &status_int, record->description);
    
    if (parsed >= 8) {
        if (strcmp(record->record_id, "记录编号") == 0 || strcmp(record->patient_id, "患者编号") == 0 || strcmp(record->doctor_id, "医生编号") == 0) return 0;
        record->type = (RecordType)type_int;
        record->status = status_int;
        record->queue_number = 0;
        record->date[0] = '\0';
        normalize_record_amounts(record);
        return 1;
    }
    return 0;
}

/* ==========================================
 * 4. 缂傚倸鍊峰鎺旂矚閸洖鍨傞柛锔诲幗椤洟鏌嶉崫鍕櫣闁藉啰鍠栭弻鏇熷緞濞戞氨鏆犲銈冨劜缁诲牓寮诲☉銏犵闁挎繂鎲涢幘缁樼厸闁糕剝鐟ラ弸娑欐叏婵犲倹鍋ョ€规洖宕埢搴ょ疀閹惧磭鐫?(100% 闂傚倸鍊风欢锟犲磻閸涙潙绀夐柟鐑橆殕閻撳倹绻濇繝鍌涘櫢婵炲矈浜弻锟犲炊閳轰椒鎴风紒?
 * ========================================== */

PatientNode* find_outpatient_by_id(const char* id) {
    PatientNode* curr = outpatient_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

PatientNode* find_inpatient_by_id(const char* id) {
    PatientNode* curr = inpatient_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

/**
 * @brief 闂傚倷绶氬褍螞濞嗘挸绀夋俊銈呮噷閳ь剙鍟撮弫鎰板炊閳哄偊绱￠柣鐔哥矋閸ㄧ厧螞閵忥紕绠鹃柟瀵稿仦鐏忣偆绱掗煬鎻掆偓妤€危閹扮増鍋嬮柛顐ゅ枎閸斿懘姊洪崫鍕窛闁稿鍠庨敃銏ゅ箻椤旇В鎷洪梺缁樺姦閸撴瑧绮堥崘顔界厪闁糕剝顨呴弳锝夋煙椤斻劌娲ら悡娑㈡煕閹般劍娅呴柍褜鍓涢崑銈夊蓟閻斿搫鏋堥柛妤冨仜椤晛顪冮妶鍡樼叆缂佸顫夋穱?
 * 濠电姷鏁告慨鎾煟閵夆晛围闁告侗鍘介悘鍡涙⒒娴ｇ鎮戦柡瀣吹缁辩偤鍩€椤掑嫭鐓熸繛鎴炵懄缁€瀣煙椤旂偓鏆い銏★耿閸┿儵宕卞Ο鑲╁幀濠电姵顔栭崰姘跺箠閹捐纾诲┑鐘插椤洖霉閻撳海鎽犻柍閿嬪浮閺屾洘绻涢崹顔碱瀷濠碘€冲级瀹€绋款嚕閸洘鍊婚柛鈩冾殔楠炴劗绱撻崒姘毙㈡繛宸幖椤繑绂掔€ｎ亶娼婇梺鏂ユ櫅閸犳艾危椤栫偞鈷戦柟绋挎捣閳藉鏌ゅú璇茬仸闁诡垰鍟存慨鈧柕鍫濇噽閻ｆ椽姊洪幐搴ｇ畵闁绘鍟村畷鎴﹀箻鐠囨彃绐涢梺鍝勵槼椤曞啿鈹戠€ｎ偆鍘介梺鍦婢瑰宕戦幘鏂ユ婵浜崢?outpatient 闂傚倷鑳剁划顖炲Φ濞戙垹鐤炬繝闈涱儏缁犳牕鈹戦悩鍙夋悙缂佺姷鍎ら幈銊ヮ渻鐠囪弓澹曟繝鐢靛仜閹冲酣鏌婇敐澶婃瀬?
 * 濠德板€楁慨鐑藉磻閻愯鑰块悷娆忓缁犻箖鏌涢妷顔煎闁稿鍔戦弻鏇熺箾閸喖濮屾繛瀵稿Т閻楁捇寮婚悢铏圭＜婵☆垵娅ｉ悷鑼磼缂併垹骞栭柣鏍с偢瀵崵浠︾粵瀣倯闂佺硶鍓濋…鍥储閹扮増鈷戦柛锔诲弾閻掗箖鏌涙繝鍛棄闂囧鏌涢妷顔煎闁?inpatient 闂傚倸鍊烽懗鍫曞磻閹惧磭鏆︽慨妞诲亾濠碘剝鎸冲畷姗€顢欓懞銉︾彨闂佽绻掗崑鐐哄垂濠靛棭娼╅柨鏇炲€哥粻褰掓倵濞戞鎴犵矆閸愨晝绠剧紒妤€鎼俊璺ㄧ磼鏉堛劌绗掗柍瑙勫灴瀹曞爼濡搁敂璇叉櫃闂傚倷鑳堕、濠囶敋閺嶎厼绐楅柡宥庡幖閻ゎ噣鏌熺€电浠﹀┑顖氱摠缁绘盯宕卞Δ鍐唹闁诲孩纰嶉悡锟犲蓟閻旇櫣鐭欓柟绋块閺嬬娀姊洪悷鏉挎缂佺粯绻堥悰顕€宕橀鍛櫌婵炶揪绲介幗婊兾ｉ幖浣光拺?
 */
int transfer_outpatient_to_inpatient(const char* patient_id) {
    PatientNode *curr = outpatient_head, *prev = NULL;
    
    while (curr) {
        if (strcmp(curr->id, patient_id) == 0) {
            // 1. 婵犵數鍋涢顓熸叏閹绢喗鐓€闁挎繂鎲樺☉銏╂晢闁稿本绋戦悵鐗堢節閻㈤潧孝闁稿﹨鍩栫粋宥夋晲婢跺鍘卞┑顔斤供閸撴稑鐡柣搴ゎ潐濞叉粓宕伴弽顓炵畺鐟滄棃宕洪敓鐘茬闁宠桨璁查崑鎾绘倷閻戞鍘甸梺鍏间航閸庡鎳撻幐搴ｇ闁告侗鍙冮崣鍕煙瀹曞洤浠辩€规洘甯掗…銊╁礃瑜嶇€?
            if (prev == NULL) {
                outpatient_head = curr->next;
            } else {
                prev->next = curr->next;
            }
            
            // 2. 闂傚倷娴囬～澶愵敊閺嶎厼鐭楅柍褜鍓氱换娑㈠级閹寸儐妫﹂梺缁樻惄閸撶喖銆侀弴銏犵伋闁惧浚鍋呭▓鏃堟⒒娴ｅ憡鎯堢紒澶嬫綑鐓ら柍鍝勬噺閸庡﹪鏌熺€电浠ч柍缁樻瀵爼鎮欓弶鎴缂備礁顦悥鐓庮潖?(Inpatient)
            curr->type = 'I'; 
            
            // 3. 闂傚倸鍊烽悞锕併亹閸愵煁娲Ω閳轰焦鐎梺闈涚墕濡娆㈤悙鐑樺€垫繛鎴烆仾椤忓棗绶ょ紓浣诡焽缁犳儳顭跨捄渚剱缂佲偓閳ь剟姊哄Ч鍥уΩ闁搞劏妫勯悾宄拔旈崨顓犵暰闂佽顔栭崰姘额敊閺囥垺鈷戠紒瀣硶缁犵偤鏌涙惔銏狀棆缂佽京鍋ゅ畷鍫曨敆娴ｅ弶瀵栭梻渚€娼х换鎺撴叏閻楀牏顩查柨婵嗩槹閻撴洘鎱ㄥ鍡楀鐎涙繈鎮?
            curr->next = inpatient_head;
            inpatient_head = curr;
            
            return 1; // 濠电姷鏁搁崑鐔烘崲濠靛枹褰掑炊椤€崇秺瀹曞崬鈽夊Ο纰卞悈闂備礁缍婇崑濠囧礈濞嗘搫缍?
        }
        prev = curr;
        curr = curr->next;
    }
    return 0; // 闂備浇宕垫慨鏉懨洪姀銈呯？闁哄被鍎辩壕濠氭煕濞戞鎽犻柛銈傚亾闂備礁鎲℃笟妤呭储婵傜妫樼憸鏃堝蓟閿熺姴閱囨い鎰╁灩椤晛鈹戦瑙掓粓宕濆鍥╃煓濠㈣泛顭崥瀣煕濞戝崬鏋﹂柛姘煎亰濮婂宕掑顑藉亾閸洖绀夐煫鍥ㄧ⊕閸庡﹪鏌″搴″箹閻熸瑱濡囬埀顒€绠嶉崕閬嶅箠鎼达絿鐭嗛悗锝庝簴濡?
}

DoctorNode* find_doctor_by_id(const char* id) {
    DoctorNode* curr = doctor_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

MedicineNode* find_medicine_by_id(const char* id) {
    MedicineNode* curr = medicine_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

BedNode* find_bed_by_id(const char* id) {
    BedNode* curr = bed_head;
    while (curr) {
        if (strcmp(curr->bed_id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

BedNode* find_available_bed(const char* dept_id, WardType type) {
    BedNode* curr = bed_head;
    while (curr) {
        if (curr->type == 0) {
            curr->type = infer_bed_type_from_snapshot(curr->bed_id, curr->daily_fee);
        }
        if (strcmp(curr->dept_id, dept_id) == 0 &&
            bed_type_matches_request(curr->type, type) &&
            curr->is_occupied == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

DepartmentNode* find_department_by_id(const char* id) {
    DepartmentNode* curr = dept_head;
    while (curr) {
        if (strcmp(curr->id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

AmbulanceNode* find_ambulance_by_id(const char* id) {
    AmbulanceNode* curr = ambulance_head;
    while (curr) {
        if (strcmp(curr->vehicle_id, id) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

void search_medicine_by_keyword(const char* keyword) {
    MedicineNode* curr = medicine_head;
    int count = 0;
    print_header("药品查询");
    printf("%-12s %-25s %-20s %-8s %-8s\n", "药品编号", "通用名", "商品名", "单价", "库存");
    print_divider();
    while (curr) {
        if (keyword == NULL || keyword[0] == '\0' || 
            strstr(curr->common_name, keyword) || 
            strstr(curr->trade_name, keyword)) {
            printf("%-12s %-25s %-20s %-8.2f %-8d\n", 
                   curr->id, curr->common_name, curr->trade_name, curr->price, curr->stock);
            count++;
        }
        curr = curr->next;
    }
    if (count == 0) {
        printf(COLOR_YELLOW "没有找到符合条件的药品。\n" COLOR_RESET);
    }
    wait_for_enter();
}

/* ==========================================
 * 5. 闂傚倷绀侀幉锟犳偋閻愯尙鏆﹂柣銏㈩焾閺嬩線鏌熼幑鎰靛殭缁炬儳缍婇弻銈吤圭€ｎ偅鐝旈梺姹囧€曞Λ婵嬪箖鐠鸿　妲堥柡宓喚鐎锋俊鐐€栭幐濠氬箖閸岀偛绠栭柛顐ｆ礀楠炪垺绻涢崱妯诲碍闁告ɑ鐟х槐?(100% 闂備礁鎼ˇ顐﹀疾濠婂牊鏅梺?
 * ========================================== */

#define DUTY_SCHEDULE_FILE "data/schedules.txt"
#define MAX_SCHEDULE_ITEMS 1024

typedef struct {
    char date[32];
    char slot[32];
    char dept_id[MAX_ID_LEN];
    char doctor_id[MAX_ID_LEN];
} DutyScheduleItem;

static int validate_schedule_date(const char* date) {
    int y, m, d;
    if (!date || sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) return 0;
    if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
    return 1;
}

static int parse_schedule_line(const char* line, char* date, char* slot, char* dept_id, char* doctor_id) {
    char local[256];
    char* fields[8] = {0};
    int count = 0;
    if (!line) return 0;
    strncpy(local, line, 255);
    local[strcspn(local, "\r\n")] = '\0';
    if (local[0] == '\0' || is_blank_text(local)) return 0;
    if ((strstr(local, "date") && strstr(local, "doctor")) || (strstr(local, "日期") && strstr(local, "医生"))) return 0;

    char* token = strtok(local, "\t");
    while (token && count < 8) {
        fields[count++] = token;
        token = strtok(NULL, "\t");
    }

    if (count >= 4) {
        strcpy(date, fields[0]);
        strcpy(slot, fields[1]);
        strcpy(dept_id, fields[2]);
        strcpy(doctor_id, fields[3]);
        return 1;
    }
    if (sscanf(local, "%31s %31s %31s %31s", date, slot, dept_id, doctor_id) == 4) return 1;
    return 0;
}

static int save_schedule_items(const DutyScheduleItem* items, int count) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "w");
    if (!file) return 0;
    fprintf(file, "日期\t班次\t科室编号\t医生编号\n");
    for (int i = 0; i < count; ++i) {
        fprintf(file, "%s\t%s\t%s\t%s\n", items[i].date, items[i].slot, items[i].dept_id, items[i].doctor_id);
    }
    fclose(file);
    return 1;
}

static int load_schedule_items(DutyScheduleItem* items, int max_count) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "r");
    char line[256];
    int count = 0;
    if (!file) return 0;
    while (fgets(line, sizeof(line), file) && count < max_count) {
        if (parse_schedule_line(line, items[count].date, items[count].slot, items[count].dept_id, items[count].doctor_id)) {
            count++;
        }
    }
    fclose(file);
    return count;
}

void ensure_duty_schedule_file(void) {
    FILE* file = fopen(DUTY_SCHEDULE_FILE, "r");
    if (file) { fclose(file); return; }
    file = fopen(DUTY_SCHEDULE_FILE, "w");
    if (!file) return;
    fprintf(file, "日期\t班次\t科室编号\t医生编号\n");
    fclose(file);
}

int is_doctor_on_duty_today(const char* doctor_id) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    int count;
    char today[32] = {0};

    if (!doctor_id || doctor_id[0] == '\0') return 0;

    get_current_time_str(today);
    today[10] = '\0';
    count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);

    for (int i = 0; i < count; ++i) {
        if (strcmp(items[i].doctor_id, doctor_id) == 0 && strncmp(items[i].date, today, 10) == 0) {
            return 1;
        }
    }
    return 0;
}
void view_duty_schedule(const char* doctor_id_filter) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    int count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);
    int shown = 0;

    print_header("值班排班");
    printf("%-12s %-8s %-10s %-14s %-10s %-14s\n", "日期", "班次", "科室编号", "科室名称", "医生编号", "医生姓名");
    print_divider();

    for (int i = 0; i < count; ++i) {
        if (doctor_id_filter && doctor_id_filter[0] != '\0' && strcmp(items[i].doctor_id, doctor_id_filter) != 0) continue;
        
        DoctorNode* d = find_doctor_by_id(items[i].doctor_id);
        DepartmentNode* dept = find_department_by_id(items[i].dept_id);

        printf("%-12s %-8s %-10s %-14s %-10s %-14s\n", 
               items[i].date, items[i].slot, items[i].dept_id, dept?dept->name:"-", 
               items[i].doctor_id, d?d->name:"-");
        shown++;
    }
    if (shown == 0) printf(COLOR_YELLOW "当前没有符合条件的排班记录。\n" COLOR_RESET);
    wait_for_enter();
}

int upsert_duty_schedule(const char* date, const char* slot, const char* dept_id, const char* doctor_id) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    int count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);
    int found = 0;

    if (!validate_schedule_date(date)) return 0;
    
    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].date, date) == 0 && strcmp(items[i].slot, slot) == 0 && strcmp(items[i].dept_id, dept_id) == 0) {
            safe_copy_text(items[i].doctor_id, MAX_ID_LEN, doctor_id);
            found = 1; break;
        }
    }
    if (!found && count < MAX_SCHEDULE_ITEMS) {
        safe_copy_text(items[count].date, 32, date);
        safe_copy_text(items[count].slot, 32, slot);
        safe_copy_text(items[count].dept_id, MAX_ID_LEN, dept_id);
        safe_copy_text(items[count].doctor_id, MAX_ID_LEN, doctor_id);
        count++;
    }
    return save_schedule_items(items, count);
}

int remove_duty_schedule(const char* date, const char* slot, const char* dept_id) {
    DutyScheduleItem items[MAX_SCHEDULE_ITEMS];
    DutyScheduleItem kept[MAX_SCHEDULE_ITEMS];
    int count = load_schedule_items(items, MAX_SCHEDULE_ITEMS);
    int kept_count = 0, removed = 0;

    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].date, date) == 0 && strcmp(items[i].slot, slot) == 0 && strcmp(items[i].dept_id, dept_id) == 0) {
            removed = 1; continue;
        }
        kept[kept_count++] = items[i];
    }
    if (removed) save_schedule_items(kept, kept_count);
    return removed;
}

/* ==========================================
 * 6. 闂傚倷绀侀幖顐ょ矓閸洍鈧箓宕奸姀銏㈠闂佺粯鍔﹂崜娑㈠煝閺冣偓閹便劌顫滈崱妤€顫╁銈庡亖閸婃繂顫忓ú顏嶆晝闁挎繂妫锛勭磽娓氣偓椤ゅ倿宕戦崱娑樼劦妞ゆ巻鍋撴繝鈧柆宥呯；闁绘棁鍋愬畵?(100% 闂備礁鎼ˇ顐﹀疾濠婂牊鏅梺姹囧焺閸ㄥジ宕戞繝鍥ㄥ仒妞ゆ梻鍋撳畷澶娿€掑鐓庣仭婵犮垺妫冮幃?
 * ========================================== */

/**
 * @brief 闂佽姘﹂～澶愬箖閸洖纾块柛妤冨剱閸熷懘鏌涢锝嗙缂佲偓閸曨垱鐓犻柟顓熷笒閸旀粍绻涢崼婵堝煟闁哄被鍔岄埥澶娾枎閹烘埈妫熼梻浣规偠閸斿苯鐣烽鍕垫晪婵炲棙鎸告导鐘绘煕閺囥劌鏋涢柣婵愬灡缁绘盯骞嬮悙鏉戠濡炪倖鍨甸幊鎾诲Φ閹版澘纾奸柣鎰蔼琚濋梻浣告啞濞诧箓宕ｉ埀顒併亜椤愵剛鐣甸柟顔肩秺瀹曟儼顧傞柛鏃傚枛閺?闂傚倷绀佸﹢閬嶁€﹂崼銉ョ９婵繂琚禍褰掓煕瑜庨〃鍛兜閳ь剟姊洪崫鍕窛闁哥姴娴风划?
 * 闂傚倷绶氬褍螞濞嗘挸绀夋繛鍡樻尪閳ь剙鍊块幃鎯х暆閳ь剚銇欓幎鑺ョ厱闁圭偓銇涢崑鎾绘煃瑜滈崜姘舵偋閻樿崵宓侀柍褜鍓涢幉鍛婃償閿濆倸浜鹃柛顭戝亜閸濊櫣鈧鍠曠划娆忕暦閸洖惟闁靛鍎涘鑸电厽闁斥晛鍟粭褔鏌涚€ｃ劌濮傞柛鈹惧亾濡炪倖甯婄粈渚€鎮￠埀顒傜磽娴ｇ缍侀柛妤佸▕瀵崵浠︾粵瀣倯闂佺硶鍓濋悷銏ゅ磻閹炬椿鏁嶉柣鎰皺椤斿洭姊虹粙鎸庢拱闁煎湱鍋撶换娑㈠炊椤掍胶鍘卞┑掳鍊撻悞锔剧矆閳ь剟鎮峰鍛暭閻庣瑳鍛煓濠㈣泛鏈崑姗€鏌嶉柨顖氫壕闂佺顑嗛幐鎼佲€﹂妸鈺佺妞ゆ帒锕︾粈鍡涙⒒?婵犵绱曢崑鎴﹀磹閺囥垹绠规い鎰╁€涙慨铏亜閹惧崬鐏╅柣?闂傚倷绀侀幉锟犳偋閻愯尙鏆﹂柣銏㈩焾閺嬩線鏌熺紒妯哄妇闂傚倷鑳堕崑銊╁磿閼碱剚宕查柟閭﹀枟椤洘绻濋棃娑欏窛闁崇懓绉电换婵嬫濞戝啿濮涘┑鈩冨絻閻楀繒妲愰幘瀛樺闁革富鍘鹃悾闈涱渻閵堝棙澶勯柛锝忕到閻ｇ兘顢楅崟鍨櫖濠电偛妫欓崺鍫熷?
 */
void add_new_record(const char* patient_id, const char* doctor_id, RecordType type, const char* description, double fee, int queue_num) {
    RecordNode* new_node = (RecordNode*)calloc(1, sizeof(RecordNode));
    if (!new_node) return;

    // 闂傚倷绀侀崥瀣磿閹惰棄搴婇柤鑹扮堪娴滃綊鏌涢妷顔煎鐎规挷绶氶幃褰掑传閸曨剚鍎撻梺鍝勬噺閹倿寮诲☉婊呯杸閻庯綆浜滄慨搴ｇ磽閸屾氨孝妞ゆ垵妫涚划娆愬緞鐏炵浜鹃柨婵嗛婢ь垶鏌ｉ敐澶夋喚闁哄本鐩獮鎺楀箻閸ㄦ稒顫嶇紓鍌氬€搁崐褰掑箲閸パ屽殨妞ゆ洍鍋撶€规洖銈稿鎾偆閸屾粌鎯?
    get_current_time_str(new_node->date);

    // 闂傚倷绶氬褍螞濞嗘挸绀夐柡宥庡幖閽冪喎鈹戦悩鎻掓殲婵炲懐濞€閺岋綁骞囬鍥跺妳闂佸搫妫崹宕囨閹烘垟妲堟繛鍡楃箳椤︺劌顪冮妶鍌涙珨缂佺姵鍨堕弲銉╂⒑缁嬭法鐒垮┑鈥虫川缁絽螖閳ь剟鈥︾捄銊﹀磯闁惧繐澧ｉ姀銈嗙厵妞ゆ柨銈搁崣鍕殽閻愭潙濮嶇€规洘甯掗～婵嬵敆閳ь剟鍩㈣箛娑欌拺闁告稑顭▓鏇熺箾閸欏澧抽柕鍥ㄥ姍瀹曠螖娴ｉ晲缂撻梻浣虹帛濮婂鈥﹂崼銉﹀€堕柛銉墯閻撴盯鏌涢锝囩畺闁诡垰鐗撻弻娑㈠煛閸屻倖鐤佹繝?
    // 闂備浇宕甸崰鎰版偡閵壯€鍋撳鐓庡⒋闁诡喚鍋撻妶锝夊礃閵娿儱鏁ら梻浣虹帛閹哥霉妞嬪孩鍏滈柍褜鍓熷娲箰鎼达絺濮囬梺鐑╁墲閻YMMDD-闂備胶鎳撻崥瀣焽濞嗘垶宕查柟閭﹀灛娴?闂傚倷绀侀幉锟犳偋閻愯尙鏆﹂柣銏㈩焾閺嬩線鏌熼悙顒€澧柛鐔锋嚇閺屾洘寰勯崼婵嗗婵?(婵犵數濮烽。浠嬪焵椤掆偓閸熸寧绂嶉幍顔剧＜?0260330-001-D001)
    char pure_date[32] = {0};
    int y = 0, m = 0, d = 0;
    if (sscanf(new_node->date, "%d-%d-%d", &y, &m, &d) == 3) {
        sprintf(pure_date, "%04d%02d%02d", y, m, d);
    } else {
        strcpy(pure_date, "UNKN"); // 闂備浇顕уù鐑藉箠閹惧墎绀婂┑鐘叉搐閺嬩礁鈹戦悩鎻掓殭缂佸墎鍋ら弻娑㈠即閵娿儱绠洪梺?
    }

    // Generate stable daily sequence and avoid record_id collisions
    int seq_id = (queue_num > 0) ? queue_num : 0;
    if (seq_id <= 0) {
        RecordNode* curr = record_head;
        int max_seq = 0;
        while (curr) {
            char date_part[32] = {0};
            char doc_part[MAX_ID_LEN] = {0};
            int seq_part = 0;
            if (sscanf(curr->record_id, "%31[^-]-%d-%31[^\n]", date_part, &seq_part, doc_part) == 3) {
                if (strcmp(date_part, pure_date) == 0 && strcmp(doc_part, doctor_id) == 0 && seq_part > max_seq) {
                    max_seq = seq_part;
                }
            }
            curr = curr->next;
        }
        seq_id = max_seq + 1;
    }

    while (1) {
        int exists = 0;
        RecordNode* curr = record_head;
        snprintf(new_node->record_id, sizeof(new_node->record_id), "%s-%03d-%.15s", pure_date, seq_id % 1000, doctor_id);
        while (curr) {
            if (strcmp(curr->record_id, new_node->record_id) == 0) {
                exists = 1;
                break;
            }
            curr = curr->next;
        }
        if (!exists) break;
        seq_id++;
    }
    
    strcpy(new_node->patient_id, patient_id);
    strcpy(new_node->doctor_id, doctor_id);
    new_node->type = type;
    new_node->total_fee = fee < 0 ? 0 : fee;
    new_node->status = 0; // 婵犳鍠楃敮妤冪矙閹烘せ鈧箓宕奸妷顔芥櫍缂傚倷鐒﹂…鍥煝閺冨牊鐓熸慨姗嗗墻閸ょ喓绱掗幇顓ф疁闁哄矉绻濆畷濂割敃閿涘嫬鍤梻浣芥〃缁讹繝宕板Δ鍐焿?闂佽楠搁悘姘熆濮椻偓楠炲﹥鎯旈妸瀣喘閸╋繝宕ㄩ闂村摋?
    strcpy(new_node->description, description);

    // 闂傚倷绀侀幖顐﹀箠濡偐纾芥慨妯挎硾缁€鍌涙叏濡炶浜惧Δ鐘靛仦鐢繝鐛幒妤€绠ｆ繝闈涙閺嬪繘姊绘笟鈧鑽ょ礊閸℃稒鍋傚璺烘湰椤洟鏌ｅΔ鈧悧蹇涖€呴悜鑺ョ厱婵炴垶顭囬幗鐘绘倵濮橆厾娲撮柟顔筋殜濡啫鈽夊姣欍劌鈹戦埥鍡椾航闁告挻鐩獮蹇涙偐瀹曞洨鐦堥梺绋挎湰缁嬫垿顢旈鐐粹拺閻犲洠鈧啿骞嶉梺绋垮閹告娊骞嗘笟鈧浠嬵敇閻愯尙褰夊┑鐘灱濞夋盯顢栭崱妞绘瀺?
    if (type == RECORD_REGISTRATION || type == RECORD_EMERGENCY) {
        RecordNode* curr = record_head;
        int max_queue = 0;
        
        while (curr) {
            if ((curr->type == RECORD_REGISTRATION || curr->type == RECORD_EMERGENCY) &&
                curr->status != -1 &&
                strcmp(curr->doctor_id, doctor_id) == 0 &&
                strcmp(curr->date, new_node->date) == 0 &&
                curr->queue_number > max_queue) {
                max_queue = curr->queue_number;
            }
            curr = curr->next;
        }
        new_node->queue_number = max_queue + 1;
    }

    // 闂傚倷绀侀幉锟犳偋閻愪警鏁婂┑鐘插瀹曞弶鎱ㄥΟ鎸庣【缂佺媭鍣ｉ弻鏇熷緞閸℃ɑ鐝曢梺杞扮劍閿曘垽寮婚妶鍡樼秶闁靛濡囬崣鍡樼節濞堝灝鏋撻柡鍛Т椤曪絾绂掔€ｎ亙绱堕梺闈涳紡閸曨剛妲?
    PatientNode* p = find_outpatient_by_id(patient_id);
    if (!p) p = find_inpatient_by_id(patient_id);
    
    double coverage_rate = 0.0;
    if (p) {
        if (p->insurance == INSURANCE_WORKER) coverage_rate = 0.7;      // 闂傚倷鑳堕崢褍鐣烽鍕殣妞ゆ牗绋戦閬嶆煙閻楀牊绶茬紒鈧畝鍕仯濞达絽鎽滈敍宥囩磼閻樿京鐭欓柡宀€鍠栭悡顒勫箵閹烘垵袝闂備浇銆€閸?70%
        else if (p->insurance == INSURANCE_RESIDENT) coverage_rate = 0.5; // 闂備浇顕х换鎺楀磻濞戙垹绠犻煫鍥ㄧ⊕閸庡秵銇勯弽顐粶缂佲偓瀹€鍕仯濞达絽鎽滈敍宥囩磼閻樿京鐭欓柡宀€鍠栭悡顒勫箵閹烘垵袝闂備浇銆€閸?50%
    }
    
    new_node->insurance_covered = new_node->total_fee * coverage_rate;
    normalize_record_amounts(new_node);

    // 婵犵數濮伴崹鐓庘枖濞戙垺鍋ら柕濞垮妸娴滃綊鏌涢幇鈺佸Ψ闁哄鐗犻弻锟犲炊閳轰焦鐎绘繝銏ｆ硾缁夊綊寮诲☉婊呯杸閻庯綆浜滄慨搴ㄦ⒑閸涘﹦鎳冪紒缁樺笚缁傛帡鏁冮崒姘卞姦濡炪倖甯掔€氼剛娑甸埀顒勬⒑閸濆嫭宸濋柛鐘叉捣缁﹪鏁冮崒娑掓嫼闂佺粯鍔﹂崜娆戠矆閸愵喗鐓?
    new_node->next = record_head;
    record_head = new_node;
}

/**
 * @brief 闂傚倷鐒﹂惇褰掑垂婵犳艾绐楅柟鐗堟緲閸ㄥ倹鎱ㄥΟ鍝勵暢鐎规挷绶氶弻鐔碱敍閻愯弓鍠婇梺鍝ュ仜婢у酣骞夐幖浣瑰亱闁割偆鍠撻敍娑氱磽娴ｈ鈷愰柟绋垮暱椤曪綁宕奸弴鐐靛姶闂佸憡鍔忛弲婊堟偟閹绢喗鈷戠痪顓炴噽閸亪鏌涙繝鍐槈闁崇懓鍟村畷銊╁级閹寸媭妲遍梻浣告啞缁诲倻鈧凹鍓熼幃鐢稿醇閺囩喓鍘搁梺鍛婂姧缁犳垿宕濆杈╃＜?
 */
void add_ambulance_log(const char* vehicle_id, const char* destination) {
    AmbulanceLogNode* new_node = (AmbulanceLogNode*)calloc(1, sizeof(AmbulanceLogNode));
    if (!new_node) return;

    time_t now = time(NULL);
    sprintf(new_node->log_id, "ALB%lld", (long long)now);
    
    struct tm *t = localtime(&now);
    strftime(new_node->dispatch_time, 32, "%Y-%m-%d %H:%M:%S", t);

    strncpy(new_node->vehicle_id, vehicle_id, 31);
    strncpy(new_node->destination, destination, 127);

    new_node->next = ambulance_log_head;
    ambulance_log_head = new_node;
}

/* ==========================================
 * 7. 闂傚倷娴囧銊╂嚄閼稿灚娅犳俊銈傚亾闁伙絽鐏氱粭鐔煎焵椤掆偓閻ｅ嘲鈹戠€ｎ偄浜归梺缁樺灦椤洭濡舵导瀛樷拻闁稿本纰嶉ˉ婊勪繆椤愶絿娲寸€?(100% 闂備浇顕х换鎺楀磻閻愯娲冀椤愶綆娼熼梺鐟扮摠缁娊骞掗弴鐘垫澑闁硅壈鎻紞鍡樺閸ャ劎绠?Loader)
 * 闂傚倷鐒﹀鍨焽閸ф绀夐悗锝庡墲婵櫕銇勯幒鎴濃偓濠氭儗濞嗘挻鍊甸柨婵嗙凹缁ㄧ晫鐥弶璺ㄐч柡灞剧☉椤繈鎮℃惔锝勫摋濠电姵顔栭崰鎺楀疾閻樺樊鍤曟い鏃傚亾婵ジ鏌涢敂璇插箻妞わ富鍣ｉ弻锝堢疀閹惧墎顔囧┑鈽嗗亝閻熴儵鏁冮姀鈥愁嚤闁哄鍨跺畵宥夋⒑缂佹﹩娈旈柣妤€锕幃妤咁敊濞村骸缍婇幃鈺呭传閸曨厼甯垮┑鐐差嚟婵參宕板Δ鈧銉╁礋椤愵偄鎮戦梺鍛婃处閸犳岸藝娴煎瓨鈷戦柛娑橈工婵洭鏌熼崘鏌ュ弰闁诡喗妞芥俊鎼佸煛閸屾粌寮伴梻浣侯攰椤宕濋幋锕€鐒?
 * ========================================== */

void load_departments(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file); // 闂備浇宕垫慨鎾箹椤愶附鍋柛銉㈡櫆瀹曟煡鏌涢幇鍏哥凹闁稿锕㈤弻鏇熺箾閸喖濮屾繛?
    while (fgets(line, 512, file)) {
        DepartmentNode* n = calloc(1, sizeof(DepartmentNode));
        if (sscanf(line, "%31s %63s", n->id, n->name) == 2) {
            n->next = dept_head;
            dept_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_doctors(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        DoctorNode* n = calloc(1, sizeof(DoctorNode));
        // 婵犵數濮伴崹鐓庘枖濞戞◤娲晝閸屾碍鐎梺缁樺姉閸庛倝寮查浣瑰弿婵妫楁晶缁樼箾鐏炲倸鈧繈寮婚敍鍕ㄥ亾閿濆骸浜為柣顓烆儐缁绘繈鍩€椤掑嫷鏁囬柕蹇曞Х妤?common.h 闂?DoctorNode 婵犵數鍋為崹鍫曞箹閳哄懎鍌ㄩ柤鎭掑劜濞呯娀鏌涜椤ㄥ懐绮堥崒鐐寸厱婵炴垵宕晶浼存煕?level 闂備浇顕х€涒晝绮欓幒妞尖偓鍐幢濞戣鲸鏅╅悗鍏夊亾闁告洦鍓氬▍?
        // 闂傚倷绀侀幉锟犳偡椤栫偛鍨傚ù鐓庣摠閸庡秹鏌曡箛鏇烆€屾繛闂村嵆閺屾洟宕煎┑鍥ь€涘┑鈩冪叀娴滃爼寮婚敐澶娢╅柕澶堝労娴煎倸顪冮妶鍡樼；闁告鍟块悾鐑芥晲婢跺﹤鍞ㄩ梺闈浤涢崘鐐棛闂佽瀛╅鏍窗閺嶎偅宕查柟閭﹀灛娴滅懓顭块懜闈涘闁告纾槐鎾诲醇閵忕姌顒傜磼閳?"%31s %127s %31s %31s" 闂傚倷绀侀幖顐λ囬銏犵？闁告瑥顦遍悵鑸电箾閸℃ɑ灏紒鐘冲▕閺屾洘寰勫☉姘煗闂?
        if (sscanf(line, "%31s %127s %31s", n->id, n->name, n->dept_id) >= 2) {
            n->next = doctor_head;
            doctor_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_medicines(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        MedicineNode* n = calloc(1, sizeof(MedicineNode));
        if (sscanf(line, "%31s %255s %lf %d", n->id, n->common_name, &n->price, &n->stock) >= 3) {
            n->next = medicine_head;
            medicine_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_outpatients(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[1024];
    if (!file) return;
    fgets(line, 1024, file);
    while (fgets(line, 1024, file)) {
        PatientNode* n = calloc(1, sizeof(PatientNode));
        int ins;
        if (sscanf(line, "%31s %127s %d %15s %c %d %lf", n->id, n->name, &n->age, n->gender, &n->type, &ins, &n->account_balance) >= 6) {
            n->insurance = (InsuranceType)ins;
            n->next = outpatient_head;
            outpatient_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_inpatients(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[1024];
    if (!file) return;
    fgets(line, 1024, file);
    while (fgets(line, 1024, file)) {
        PatientNode* n = calloc(1, sizeof(PatientNode));
        int ins;
        // 婵犵數濮伴崹鐓庘枖濞戞◤娲晝閸屾碍鐎梺缁樺姉閸庛倝寮查浣瑰弿婵妫楁晶缁樼箾鐏炲倸鈧繈寮婚敍鍕ㄥ亾閿濆骸浜為柣顓烆儐缁绘繈鍩€椤掑嫷鏁囬柕蹇曞Х妤犲洭姊洪棃娑辨Ф闁稿孩鐓￠悡?common.h 闂?PatientNode 婵犵數鍋為崹鍫曞箹閳哄懎鍌ㄥ┑鍌氭閻戣棄绀冩い鏃囧亹椤︻噣姊虹紒姗嗙劸濞存粠鍓熷畷?expected_discharge_date 闂備浇顕х€涒晝绮欓幒妞尖偓鍐幢濞戣鲸鏅╅悗鍏夊亾闁告洦鍓氬▍?
        // 闂傚倷绀侀幉锟犳偡椤栫偛鍨傚ù鐓庣摠閸庡秹鏌曡箛瀣偓鏇犵矆閸岀偞鐓犵紒瀣硶娴犳盯鏌℃径濠冭础缂佽鲸鎹囧畷姗€骞嗚閻忓牆鈹戦悩顐壕?scanf 闂傚倷鐒﹂惇褰掑礉瀹€鈧埀顒佸嚬閸撶喖骞冮姀銈庢晢闁告洦鍓涢崝鎼佹⒑闂堟侗鐒鹃柛濠冾殜椤㈡梹娼忛…鎴烆啍闂佺粯鍔栧娆撳礉閼哥偣浜滈柡鍥ㄨ壘瀹撳棛鈧娲╃紞浣割嚕娴犲唯闁靛繒濮虫竟?
        if (sscanf(line, "%31s %127s %d %15s %c %d %lf %lf %31s %31s", n->id, n->name, &n->age, n->gender, &n->type, &ins, &n->account_balance, &n->hospital_deposit, n->admission_date, n->bed_id) >= 6) {
            n->insurance = (InsuranceType)ins;
            n->next = inpatient_head;
            inpatient_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_beds(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        BedNode* n = calloc(1, sizeof(BedNode));
        char bed_id[MAX_ID_LEN] = {0};
        char dept_id[MAX_ID_LEN] = {0};
        char patient_id[MAX_ID_LEN] = "N/A";
        int occupied = 0;
        double daily_fee = 0.0;
        int parsed = sscanf(line, "%31s %31s %d %lf %31s", bed_id, dept_id, &occupied, &daily_fee, patient_id);

        if (parsed < 4) {
            char line_copy[512];
            char* fields[6] = {0};
            int count = 0;
            char* token = NULL;

            safe_copy_text(line_copy, sizeof(line_copy), line);
            line_copy[strcspn(line_copy, "\r\n")] = '\0';

            token = strtok(line_copy, "\t");
            while (token && count < 6) {
                fields[count++] = token;
                token = strtok(NULL, "\t");
            }

            if (count >= 4) {
                safe_copy_text(bed_id, sizeof(bed_id), fields[0]);
                safe_copy_text(dept_id, sizeof(dept_id), fields[1]);
                occupied = atoi(fields[2]);
                daily_fee = strtod(fields[3], NULL);
                if (count >= 5 && fields[4] && fields[4][0] != '\0') {
                    safe_copy_text(patient_id, sizeof(patient_id), fields[4]);
                }
                parsed = 4;
            }
        }

        if (parsed >= 4 && !is_blank_text(bed_id) && !is_blank_text(dept_id)) {
            safe_copy_text(n->bed_id, sizeof(n->bed_id), bed_id);
            safe_copy_text(n->dept_id, sizeof(n->dept_id), dept_id);
            n->is_occupied = occupied;
            n->daily_fee = daily_fee;
            safe_copy_text(n->patient_id, sizeof(n->patient_id), patient_id);
            n->type = infer_bed_type_from_snapshot(n->bed_id, n->daily_fee);
            n->next = bed_head;
            bed_head = n;
        } else {
            free(n);
        }
    }
    fclose(file);
}

void load_records(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[1024];
    if (!file) return;
    while (fgets(line, 1024, file)) {
        RecordNode* n = calloc(1, sizeof(RecordNode));
        if (parse_record_line_tab(n, line) || parse_record_line_legacy(n, line)) {
            n->next = record_head;
            record_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_ambulances(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    while (fgets(line, 512, file)) {
        AmbulanceNode* n = calloc(1, sizeof(AmbulanceNode));
        int s;
        if (sscanf(line, "%31s %127s %d %127[^\n]", n->vehicle_id, n->driver_name, &s, n->current_location) >= 3) {
            n->status = (AmbStatus)s;
            n->next = ambulance_head;
            ambulance_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_ambulance_logs(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        AmbulanceLogNode* n = calloc(1, sizeof(AmbulanceLogNode));
        if (sscanf(line, "%31s\t%31s\t%31s\t%127[^\n]", n->log_id, n->vehicle_id, n->dispatch_time, n->destination) >= 3) {
            n->next = ambulance_log_head;
            ambulance_log_head = n;
        } else free(n);
    }
    fclose(file);
}

void load_operating_rooms(const char* filepath) {
    FILE* file = fopen(filepath, "r");
    char line[512];
    if (!file) return;
    fgets(line, 512, file);
    while (fgets(line, 512, file)) {
        OperatingRoomNode* n = calloc(1, sizeof(OperatingRoomNode));
        if (sscanf(line, "%31s\t%d\t%31s\t%31s", n->room_id, &n->status, n->current_patient, n->current_doctor) >= 2) {
            n->next = or_head;
            or_head = n;
        } else free(n);
    }
    fclose(file);
}

/* ==========================================
 * 8. 闂傚倷绀佺紞濠囧疾閹绘帩娓婚柟鐑樻⒒娑撳秵绻涢崱妯诲碍缂佲偓瀹€鍕厸鐎广儱鍟俊鑺ョ箾閸繄鍩ｉ柡宀€鍠撻幏鐘诲灳閾忣偆浜炵紓鍌欒兌缁垰顫濋妸鈺佺劦妞ゆ帊鑳堕埊鏇㈡煥閺囨ê鐏╅柍缁樻閺佸倿宕滆椤?(100% 闂備浇顕х换鎺楀磻閻愯娲冀椤愶綆娼熼梺鐟扮摠缁娊骞掗弴鐘垫澑闁硅壈鎻紞鍡樺閸ャ劎绠?Saver)
 * 闂傚倷鑳堕…鍫㈡崲濡も偓閳诲秹寮撮悙鈺傛暊闂侀€炲苯澧撮柡宀嬬節瀹曟﹢顢橀悢鎭掆偓鎰版⒑濮瑰洤濡搁柛銊ㄦ閻ｅ嘲顫滈埀顒€鐣烽妸鈺婃晣闁绘娅曢崰鏍⒒閸屾瑨鍏岄柛瀣尭閻ｅ嘲顫滈埀顒佷繆閹绢喖绀冩い鏃囧亹椤︻偊鎮楅悷鏉款仾婵犮垺锚椤曪綁骞庨懞銉у幐闂佸壊鍋掗崑鍕櫠椤旂晫绡€闁逞屽墴楠炴﹢顢欓崗纰辨闂備焦鎮堕崕濠氭⒔閸曨垼鏁冨ù鐘差儐閻撱儲绻涢幋鐑嗙劷闁绘帒鎲℃穱濠囨嚑妫版繂娈銈庡亖閸ㄨ姤鎱ㄩ埀顒勬煃閳轰礁鏆熼柛鏃傚厴濮婂搫效閸パ呬患闂佽绻戝畝绋款嚕閹惰姤鍋勯柣鎾虫捣椤斿顪冮妶鍡樷拻缂侇噮鍨辩粙澶愬閻欌偓閻?
 * ========================================== */

void save_departments(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "科室编号\t科室名称\n");
    for (DepartmentNode* c = dept_head; c; c = c->next) fprintf(fp, "%s\t%s\n", c->id, c->name);
    fclose(fp);
}

void save_doctors(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "医生编号\t医生姓名\t科室编号\n");
    for (DoctorNode* c = doctor_head; c; c = c->next) fprintf(fp, "%s\t%s\t%s\n", c->id, c->name, c->dept_id);
    fclose(fp);
}

void save_medicines(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "药品编号\t药品名称\t单价\t库存\n");
    for (MedicineNode* c = medicine_head; c; c = c->next) fprintf(fp, "%s\t%s\t%.2f\t%d\n", c->id, c->common_name, c->price, c->stock);
    fclose(fp);
}

void save_outpatients(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "患者编号\t姓名\t年龄\t性别\t类型\t医保\t余额\n");
    for (PatientNode* c = outpatient_head; c; c = c->next) fprintf(fp, "%s\t%s\t%d\t%s\t%c\t%d\t%.2f\n", c->id, c->name, c->age, c->gender, c->type, (int)c->insurance, c->account_balance);
    fclose(fp);
}

void save_inpatients(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "患者编号\t姓名\t年龄\t性别\t类型\t医保\t余额\t押金\t入院日期\t床位编号\n");
    for (PatientNode* c = inpatient_head; c; c = c->next) fprintf(fp, "%s\t%s\t%d\t%s\t%c\t%d\t%.2f\t%.2f\t%s\t%s\n", c->id, c->name, c->age, c->gender, c->type, (int)c->insurance, c->account_balance, c->hospital_deposit, c->admission_date, c->bed_id);
    fclose(fp);
}

void save_beds(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "床位编号\t科室编号\t占用状态\t日费\t患者编号\n");
    for (BedNode* c = bed_head; c; c = c->next) fprintf(fp, "%s\t%s\t%d\t%.2f\t%s\n", c->bed_id, c->dept_id, c->is_occupied, c->daily_fee, c->patient_id);
    fclose(fp);
}

void save_records(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "记录编号\t患者编号\t医生编号\t类型\t日期\t队列号\t状态\t描述\t总费用\t医保支付\t个人支付\n");
    for (RecordNode* c = record_head; c; c = c->next) fprintf(fp, "%s\t%s\t%s\t%d\t%s\t%d\t%d\t%s\t%.2f\t%.2f\t%.2f\n", c->record_id, c->patient_id, c->doctor_id, (int)c->type, c->date, c->queue_number, c->status, c->description, c->total_fee, c->insurance_covered, c->personal_paid);
    fclose(fp);
}

void save_ambulances(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "车辆编号\t司机\t状态\t位置\n");
    for (AmbulanceNode* c = ambulance_head; c; c = c->next) fprintf(fp, "%s\t%s\t%d\t%s\n", c->vehicle_id, c->driver_name, (int)c->status, c->current_location);
    fclose(fp);
}

void save_ambulance_logs(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "日志编号\t车辆编号\t出车时间\t目的地\n");
    // 闂傚倷绶氬褍螞濞嗘挸绀夐柟鐑橆殔缁犵偤鏌曟繝蹇曠暠缂佸墎鍋熼埀顒€绠嶉崕閬嶅疮閻樿纾婚柟鎹愵嚙缁狅絾銇勯弽銊х煂闁绘稏鍎甸弻锝夋倷鐎电硶妫ㄩ梺绋块叄娴滆泛鐣?for 闂佽娴烽弫濠氬磻婵犲啰顩查柣鎰瀹撲線鏌涢埄鍐姇闁稿骸绉归弻娑㈠即閵娿倖鎹ｉ梺鍛婏供閸撶喎顫忔繝姘＜婵椴搁幉鐓庘攽閻愮鎷￠柛銊ㄤ含缁骞掑Δ鈧€氬鏌ｉ姀銏℃毄闁规彃娼″娲川婵犲倻鐟ㄥ┑鈽嗗亝椤ㄥ濡?
    for (AmbulanceLogNode* c = ambulance_log_head; c != NULL; c = c->next) {
        fprintf(fp, "%s\t%s\t%s\t%s\n", c->log_id, c->vehicle_id, c->dispatch_time, c->destination);
    }
    fclose(fp);
}

void save_operating_rooms(const char* f) {
    FILE* fp = fopen(f, "w");
    if (!fp) return;
    fprintf(fp, "手术室编号\t状态\t患者编号\t医生编号\n");
    for (OperatingRoomNode* c = or_head; c; c = c->next) fprintf(fp, "%s\t%d\t%s\t%s\n", c->room_id, c->status, c->current_patient, c->current_doctor);
    fclose(fp);
}

/* ==========================================
 * 9. 缂傚倸鍊风欢锟犲垂闂堟稓鏆﹂柣銏ゆ涧閸ㄦ繃绻涘顔荤凹闁稿鍔戦弻锝夊籍閸喐鏆犻梺鎼炲€栫敮锟犲蓟濞戞ǚ妲堥柡宓喚鐎存繝鐢靛Л閸嬫捇鏌ㄩ悢鍝勑㈢紒鐘茬－閹茬顓兼径濠冩К?(闂傚倷绀侀幉锛勬暜濡ゅ啯宕查柛宀€鍎戠紞鏍煙閻楀牊绶茬紒鈧畝鍕厸鐎广儱绻愰々顒勬煟閻旀潙鐏叉慨濠冩そ楠炴捇骞掗悙瀛樻毎闂?
 * ========================================== */

void init_system_data() {
    load_departments("data/departments.txt");
    load_doctors("data/doctors.txt");
    load_medicines("data/medicines.txt");
    load_outpatients("data/outpatients.txt");
    load_inpatients("data/inpatients.txt");
    load_beds("data/beds.txt");
    load_records("data/records.txt");
    load_ambulances("data/ambulances.txt");
    load_ambulance_logs("data/ambulance_logs.txt");
    load_operating_rooms("data/operating_rooms.txt");
    
    ensure_duty_schedule_file();
    
    // 闂傚倷绀佺紞濠傤焽瑜旈、鏍幢濡ゅ鈧法鈧娲栧ú鐘诲磻閹捐崵宓侀柛顭戝枓閸嬫挸鈽夐姀鈥崇彃闁诲函缍嗛崰妤呭煕閹剧粯鐓欑紓浣姑粭姘舵煕閻旈潻鑰块柡灞诲妼閳藉鈻庨幒鎴婵＄偑鍊愰弫顏堝炊瑜嶉崵鎴濃攽閻樿宸ユ俊顐弮瀹曠敻濮€閵堝棛鍘卞┑鐐叉閸旀鏅堕浣虹瘈闁逞屽墯鐎靛ジ寮堕幋鐐茬ザ闂備胶绮〃鍛存偋婵犲洦鍊垫い鏃€绁硅ぐ鎺撳亹闁绘垶顭囬妴鎰版偡濠婂嫭绶查柛濠傛健楠炲啴鎼归顒傜煑婵炶揪绲块弲顐λ囬幍顔剧＝濞达絽鎽滈悘閬嶆倶韫囧濡挎俊鍙夊姇閳规垿宕遍埡浣诡吙闂備礁鎲＄换鍌溾偓姘煎墴閹繝骞嬮敂鐣屽幗闂侀潧绻堥崹褰掑几閹达附鐓?3 闂傚倸鍊搁崐鎼佸磹瑜版帒绠扮€规洖娲ら崹婵囥亜閺嶃劎鐭嬪┑顖氥偢閺屾洟宕煎┑鍥舵濡炪們鍊撶欢姘跺蓟閿熺姴閱囨い鎰╁灮娴犫晛顪?
    if (or_head == NULL) {
        for (int i = 3; i >= 1; i--) {
            OperatingRoomNode* n = calloc(1, sizeof(OperatingRoomNode));
            sprintf(n->room_id, "OR-%02d", i);
            n->status = 0;
            strcpy(n->current_patient, "N/A");
            strcpy(n->current_doctor, "N/A");
            n->next = or_head;
            or_head = n;
        }
    }
}

void save_system_data() {
    save_departments("data/departments.txt");
    save_doctors("data/doctors.txt");
    save_medicines("data/medicines.txt");
    save_outpatients("data/outpatients.txt");
    save_inpatients("data/inpatients.txt");
    save_beds("data/beds.txt");
    save_records("data/records.txt");
    save_ambulances("data/ambulances.txt");
    save_ambulance_logs("data/ambulance_logs.txt");
    save_operating_rooms("data/operating_rooms.txt");
}

void free_all_data() {
    #define FREE_NODES(head, type) while(head){type* temp=head; head=head->next; free(temp);}
    FREE_NODES(record_head, RecordNode);
    FREE_NODES(outpatient_head, PatientNode);
    FREE_NODES(inpatient_head, PatientNode);
    FREE_NODES(doctor_head, DoctorNode);
    FREE_NODES(dept_head, DepartmentNode);
    FREE_NODES(medicine_head, MedicineNode);
    FREE_NODES(bed_head, BedNode);
    FREE_NODES(ambulance_head, AmbulanceNode);
    FREE_NODES(ambulance_log_head, AmbulanceLogNode);
    FREE_NODES(or_head, OperatingRoomNode);
}





