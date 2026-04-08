#include "data_manager.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "utils.h"

/* ==========================================================================
 * 医疗系统核心数据访问层 (Data Access Layer)
 * --------------------------------------------------------------------------
 * 本模块负责整个 HIS 系统底层数据的生命周期管理，
 * 包括：内存链表的初始化、本地 TXT 文件的反序列化(Load)、
 * 内存数据的持久化(Save)，以及各类实体节点的核心查询与流转。
 * 此次更新全面支持了急诊救护车绿色通道、医生分级权限以及【住院双医生责任制】。
 * ========================================================================== */

#ifndef HIS_EXT_NODES_DEFINED
#define HIS_EXT_NODES_DEFINED
/**
 * @brief 救护车出勤日志节点结构体 (Ambulance Log Node)
 * 记录每一次 120 救护车派发的详细追踪信息。
 * 包含：日志唯一流水号、车辆编号、派发时间戳、目的地等。
 * 此数据结构用于支撑 ambulance.c 中的历史出勤报表功能。
 */
typedef struct AmbulanceLogNode {
  char log_id[32];            // 日志单号 (格式如: ALB1711693450，确保全局唯一)
  char vehicle_id[32];        // 派出的车辆编号 (对应 AmbulanceNode 的 ID)
  char dispatch_time[32];     // 派发时间戳 (格式: YYYY-MM-DD HH:MM:SS)
  char destination[128];      // 任务目的地 / 急救现场地址 / GPS定位
  struct AmbulanceLogNode* next; // 单向链表指针：指向下一条历史出勤记录
} AmbulanceLogNode;

/**
 * @brief 急诊手术室资源节点结构体 (Operating Room Node)
 * 监控全院急诊手术室的实时占用状态，用于急救绿色通道的资源调度。
 * 防止在抢救车回场时，发生“有急症患者但无可用手术室”的医疗事故。
 */
typedef struct OperatingRoomNode {
  char room_id[32];           // 手术室全局唯一编号 (如: OR-01)
  int status;                 // 手术室当前状态码:
                              // 0: 空闲待命状态 (随时可供急救插队使用)
                              // 1: 正在手术抢救中 (资源已被锁定)
                              // 2: 术后深度消毒/常规维保中 (不可用)
  char current_patient[32];   // 当前正在手术室内的患者编号 (若空闲则为N/A)
  char current_doctor[32];    // 当前主刀的最高权限医生工号 (若空闲则为N/A)
  struct OperatingRoomNode* next; // 单向链表指针：指向下一间手术室
} OperatingRoomNode;
#endif

/* ==========================================================================
 * 全局数据链表头指针 (Global Linked-List Heads)
 * --------------------------------------------------------------------------
 * 系统的所有内存数据流均通过以下指针作为入口进行全表扫描或节点挂载。
 * ========================================================================== */
DepartmentNode* dept_head = NULL;            // 全院科室资源目录
DoctorNode* doctor_head = NULL;              // 全院医生/医护人员人事档案
MedicineNode* medicine_head = NULL;          // 药房进销存及药品价格目录
PatientNode* outpatient_head = NULL;         // 门诊/急诊患者建档链表
PatientNode* inpatient_head = NULL;          // 住院系统患者在院及财务状态链表
BedNode* bed_head = NULL;                    // 全院床位资源池及占用状态
RecordNode* record_head = NULL;              // 核心业务：全院历年诊疗、手术、检查记录流水
AmbulanceNode* ambulance_head = NULL;        // 120救护车队车辆基础档案及实时状态
AmbulanceLogNode* ambulance_log_head = NULL; // 120救护车队历次出勤任务流转日志
OperatingRoomNode* or_head = NULL;           // 急诊手术室大屏实时监控看板资源
NurseNode* nurse_head = NULL;              // 护士工作站核心数据链表头指针

/* ==========================================================================
 * 系统底层辅助与清洗引擎 (Utility & Sanitization)
 * ========================================================================== */

/**
 * @brief 安全的内存字符串拷贝函数
 * 强制阻止缓存区溢出攻击 (Buffer Overflow) 或超长字符串导致的系统宕机。
 * @param dest 目标缓冲区地址
 * @param dest_size 目标缓冲区最大容量
 * @param src 源字符串地址
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
 * @brief 格式化和校验医疗账单金额引擎 (财务风控核心)
 * 处理医保统筹报销与个人自付额度，执行严格的账务平衡校验：
 * 1. 拦截负数非法金额；
 * 2. 校验医保报销额度绝不能超过医疗记录总费用；
 * 3. 自动计算个人应补交的尾款：个人自付 = 总费用 - 医保报销；
 * 4. 消除浮点数精度导致的微小财务不平账现象。
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
  delta =
      (record->insurance_covered + record->personal_paid) - record->total_fee;

  // 严格财务精度校验，若不平账则强行修正个人自付金额
  if (record->personal_paid < 0 || record->personal_paid > record->total_fee ||
      delta > 0.0001 || delta < -0.0001) {
    record->personal_paid = expected_personal;
  }
  if (record->personal_paid < 0) record->personal_paid = 0;
}

/**
 * @brief 获取指定日期的每日最高流水序列号
 * 通过全表扫描当天的所有 Record 记录，确保生成的下一个订单流水号全局唯一
 */
static int get_next_record_sequence_for_day(const char* ymd8) {
  int max_seq = 0;
  RecordNode* curr = record_head;

  while (curr) {
    char date_part[9] = {0};
    int seq = 0;
    if (strlen(curr->record_id) == 14 &&
        sscanf(curr->record_id, "REC%8[0-9]%3d", date_part, &seq) == 2 &&
        strcmp(date_part, ymd8) == 0 && seq > max_seq) {
      max_seq = seq;
    }
    curr = curr->next;
  }

  return max_seq + 1;
}

/**
 * @brief 防碰撞检测：验证 Record ID 是否在全网已存在
 */
static int record_id_exists(const char* record_id) {
  RecordNode* curr = record_head;
  while (curr) {
    if (strcmp(curr->record_id, record_id) == 0) {
      return 1;
    }
    curr = curr->next;
  }
  return 0;
}

/**
 * @brief 脏数据检测：判断字符串是否为空白字符（空格、换行、制表符）组成
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

/**
 * @brief 床位类型智能推断分析器
 * 依靠床位编号的前缀字母或每日床位费，自动倒推属于哪种类型的监护病房：
 * 'U' = ICU特级重症监护, 'V' = VIP特需陪护, 'S' = 单人, 'D' = 双人, 'B' = 普通
 */
static WardType infer_bed_type_from_snapshot(const char* bed_id,
                                             double daily_fee) {
  if (bed_id && bed_id[0]) {
    switch (toupper((unsigned char)bed_id[0])) {
      case 'U':
        return WARD_SPECIAL;
      case 'V':
        return WARD_ESCORTED;
      case 'S':
        return WARD_SINGLE;
      case 'D':
        return WARD_DOUBLE;
      case 'T':
        return WARD_TRIPLE;
      case 'B':
        return WARD_GENERAL;
      default:
        break;
    }
  }

  // 若无编号前缀，依靠价格阶梯硬性裁定
  if (daily_fee >= 1000.0) return WARD_SPECIAL;
  if (daily_fee >= 400.0) return WARD_ESCORTED;
  return WARD_GENERAL;
}

/**
 * @brief 床位分级请求匹配逻辑：允许患者被安排到兼容或降级的床位中
 */
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

/* ==========================================================================
 * 文本数据解析器 (Parser)
 * --------------------------------------------------------------------------
 * 负责解析从文件中读取出来的行数据。提供两种解析方案以兼容新老数据格式：
 * 1. Tab 分隔符规范解析 (parse_record_line_tab)
 * 2. 旧版混合空格解析 (parse_record_line_legacy)
 * ========================================================================== */

static int parse_record_line_tab(RecordNode* record, char* line) {
  char* fields[16];
  int count = 0;
  char* token = strtok(line, "\t");

  while (token && count < 16) {
    fields[count++] = token;
    token = strtok(NULL, "\t");
  }

  // 严格数据列映射解析
  if (count >= 11) {
    int type_val = atoi(fields[3]);
    if ((fields[3][0] < '0' || fields[3][0] > '9') && fields[3][0] != '-')
      return 0;
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

  // 降级容错解析
  if (count >= 9) {
    int type_val = atoi(fields[3]);
    if ((fields[3][0] < '0' || fields[3][0] > '9') && fields[3][0] != '-')
      return 0;
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
                      record->record_id, record->patient_id, record->doctor_id,
                      &type_int, record->date, &record->queue_number,
                      &status_int, record->description, &record->total_fee,
                      &record->insurance_covered, &record->personal_paid);

  if (parsed >= 8) {
    if ((record->record_id[0] < 'A' || record->record_id[0] > 'Z') &&
        (record->record_id[0] < 'a' || record->record_id[0] > 'z'))
      return 0;
    record->type = (RecordType)type_int;
    record->status = status_int;
    normalize_record_amounts(record);
    return 1;
  }

  parsed = sscanf(line, "%31s %31s %31s %d %lf %lf %lf %d %511[^\n]",
                  record->record_id, record->patient_id, record->doctor_id,
                  &type_int, &record->total_fee, &record->insurance_covered,
                  &record->personal_paid, &status_int, record->description);

  if (parsed >= 8) {
    if ((record->record_id[0] < 'A' || record->record_id[0] > 'Z') &&
        (record->record_id[0] < 'a' || record->record_id[0] > 'z'))
      return 0;
    record->type = (RecordType)type_int;
    record->status = status_int;
    record->queue_number = 0;
    record->date[0] = '\0';
    normalize_record_amounts(record);
    return 1;
  }
  return 0;
}

/* ==========================================================================
 * 数据流转与查找逻辑模块 (Entities Query Flow)
 * --------------------------------------------------------------------------
 * 承载根据各种主键 (ID, Keyword) 从内存链表中高效定位节点的功能
 * ========================================================================== */

/**
 * @brief 查门诊库患者
 */
PatientNode* find_outpatient_by_id(const char* id) {
  PatientNode* curr = outpatient_head;
  while (curr) {
    if (strcmp(curr->id, id) == 0) return curr;
    curr = curr->next;
  }
  return NULL;
}

/**
 * @brief 查住院库患者
 */
PatientNode* find_inpatient_by_id(const char* id) {
  PatientNode* curr = inpatient_head;
  while (curr) {
    if (strcmp(curr->id, id) == 0) return curr;
    curr = curr->next;
  }
  return NULL;
}

/**
 * @brief 患者状态流转：由门诊系统平滑划转至住院系统
 * 核心逻辑：
 * 1. 从门诊链表 (outpatient_head) 中定位并完整摘除该患者节点；
 * 2. 修改患者业务类型标志位 (Type 从 O 变更为 I)；
 * 3. 将该节点挂载到住院患者链表 (inpatient_head) 头部，完成物理流转。
 */
int transfer_outpatient_to_inpatient(const char* patient_id) {
  PatientNode *curr = outpatient_head, *prev = NULL;

  while (curr) {
    if (strcmp(curr->id, patient_id) == 0) {
      // 1. 摘除门诊链表节点
      if (prev == NULL) {
        outpatient_head = curr->next;
      } else {
        prev->next = curr->next;
      }

      // 2. 扭转系统属性 (门诊转住院)
      curr->type = 'I';

      // 3. 挂载到住院系统链表
      curr->next = inpatient_head;
      inpatient_head = curr;

      return 1;  
    }
    prev = curr;
    curr = curr->next;
  }
  return 0;  
}

/**
 * @brief 根据工号精准查询医生人事档案
 */
DoctorNode* find_doctor_by_id(const char* id) {
  DoctorNode* curr = doctor_head;
  while (curr) {
    if (strcmp(curr->id, id) == 0) return curr;
    curr = curr->next;
  }
  return NULL;
}

/**
 * @brief 内部模糊查询修剪器：去除检索关键字两端的冗余空白符
 */
static size_t trim_copy_lookup_key(const char* input, char* output,
                                   size_t output_size) {
  size_t start = 0;
  size_t len = 0;

  if (!output || output_size == 0) {
    return 0;
  }
  output[0] = '\0';
  if (!input) {
    return 0;
  }

  while (input[start] != '\0' && isspace((unsigned char)input[start])) {
    start++;
  }
  len = strlen(input + start);
  while (len > 0 && isspace((unsigned char)input[start + len - 1])) {
    len--;
  }

  if (len == 0) {
    return 0;
  }
  if (len >= output_size) {
    len = output_size - 1;
  }

  memcpy(output, input + start, len);
  output[len] = '\0';
  return len;
}

/**
 * @brief 跨平台支持的忽略大小写模糊对比
 */
static int text_equals_ignore_case_trimmed(const char* left,
                                           const char* right) {
  char lbuf[MAX_MED_NAME_LEN] = {0};
  char rbuf[MAX_MED_NAME_LEN] = {0};

  if (trim_copy_lookup_key(left, lbuf, sizeof(lbuf)) == 0 ||
      trim_copy_lookup_key(right, rbuf, sizeof(rbuf)) == 0) {
    return 0;
  }

  return _stricmp(lbuf, rbuf) == 0;
}

/**
 * @brief 药品 ID 精准匹配 (包含前后空格容错处理)
 */
MedicineNode* find_medicine_by_id(const char* id) {
  MedicineNode* curr = medicine_head;
  char key[MAX_ID_LEN] = {0};
  size_t start = 0;
  size_t len = 0;

  if (!id) {
    return NULL;
  }

  while (id[start] == ' ' || id[start] == 9 || id[start] == 10 ||
         id[start] == 13) {
    start++;
  }
  len = strlen(id + start);
  while (len > 0) {
    char ch = id[start + len - 1];
    if (ch == ' ' || ch == 9 || ch == 10 || ch == 13) {
      len--;
    } else {
      break;
    }
  }
  if (len == 0) {
    return NULL;
  }
  if (len >= sizeof(key)) {
    len = sizeof(key) - 1;
  }
  memcpy(key, id + start, len);
  key[len] = 0;

  while (curr) {
    char curr_id[MAX_ID_LEN] = {0};
    size_t cstart = 0;
    size_t clen = 0;

    while (curr->id[cstart] == ' ' || curr->id[cstart] == 9 ||
           curr->id[cstart] == 10 || curr->id[cstart] == 13) {
      cstart++;
    }
    clen = strlen(curr->id + cstart);
    while (clen > 0) {
      char ch = curr->id[cstart + clen - 1];
      if (ch == ' ' || ch == 9 || ch == 10 || ch == 13) {
        clen--;
      } else {
        break;
      }
    }
    if (clen >= sizeof(curr_id)) {
      clen = sizeof(curr_id) - 1;
    }
    memcpy(curr_id, curr->id + cstart, clen);
    curr_id[clen] = 0;

    if (_stricmp(curr_id, key) == 0) {
      return curr;
    }
    curr = curr->next;
  }
  return NULL;
}

/**
 * @brief 药品综合名称模糊匹配 (支持匹配别名、商品名、通用名)
 */
MedicineNode* find_medicine_by_key(const char* key) {
  MedicineNode* curr = NULL;
  char normalized[MAX_MED_NAME_LEN] = {0};

  if (trim_copy_lookup_key(key, normalized, sizeof(normalized)) == 0) {
    return NULL;
  }

  curr = find_medicine_by_id(normalized);
  if (curr) {
    return curr;
  }

  curr = medicine_head;
  while (curr) {
    if (text_equals_ignore_case_trimmed(curr->alias_name, normalized) ||
        text_equals_ignore_case_trimmed(curr->trade_name, normalized) ||
        text_equals_ignore_case_trimmed(curr->common_name, normalized)) {
      return curr;
    }
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
        bed_type_matches_request(curr->type, type) && curr->is_occupied == 0) {
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

/* ==========================================================================
 * 药房管理系统的控制台 UI 宏定义及常量
 * --------------------------------------------------------------------------
 * 用于渲染带边框和对齐的药房审计面板
 * ========================================================================== */
#define UTF8_MED_TITLE_SEARCH "\xE8\x8D\xAF\xE5\x93\x81\xE6\x9F\xA5\xE8\xAF\xA2"
#define UTF8_MED_COL_ID "\xE8\x8D\xAF\xE5\x93\x81\xE7\xBC\x96\xE5\x8F\xB7"
#define UTF8_MED_COL_COMMON "\xE9\x80\x9A\xE7\x94\xA8\xE5\x90\x8D"
#define UTF8_MED_COL_TRADE "\xE5\x95\x86\xE5\x93\x81\xE5\x90\x8D"
#define UTF8_MED_COL_ALIAS "\xE5\x88\xAB\xE5\x90\x8D"
#define UTF8_MED_COL_PRICE "\xE5\x8D\x95\xE4\xBB\xB7"
#define UTF8_MED_COL_STOCK "\xE5\xBA\x93\xE5\xAD\x98"
#define UTF8_MED_MSG_NOT_FOUND                                               \
  "\xE6\xB2\xA1\xE6\x9C\x89\xE6\x89\xBE\xE5\x88\xB0\xE7\xAC\xA6\xE5\x90\x88" \
  "\xE6\x9D\xA1\xE4\xBB\xB6\xE7\x9A\x84\xE8\x8D\xAF\xE5\x93\x81\xE3\x80\x82"

void search_medicine_by_keyword(const char* keyword) {
  MedicineNode* curr = medicine_head;
  int count = 0;

  print_header(UTF8_MED_TITLE_SEARCH);
  printf("%-12s %-18s %-18s %-16s %-8s %-8s\n", UTF8_MED_COL_ID,
         UTF8_MED_COL_COMMON, UTF8_MED_COL_TRADE, UTF8_MED_COL_ALIAS,
         UTF8_MED_COL_PRICE, UTF8_MED_COL_STOCK);
  print_divider();

  while (curr) {
    if (keyword == NULL || keyword[0] == '\0' ||
        strstr(curr->common_name, keyword) ||
        strstr(curr->trade_name, keyword) ||
        strstr(curr->alias_name, keyword)) {
      printf("%-12s %-18s %-18s %-16s %-8.2f %-8d\n", curr->id,
             curr->common_name, curr->trade_name, curr->alias_name, curr->price,
             curr->stock);
      count++;
    }
    curr = curr->next;
  }

  if (count == 0) {
    printf(COLOR_YELLOW "%s\n" COLOR_RESET, UTF8_MED_MSG_NOT_FOUND);
  }
  wait_for_enter();
}

/* ==========================================================================
 * 核心增强工厂函数 (Data Entity Factories)
 * --------------------------------------------------------------------------
 * 包含了：急救调度优先级的融合、医保测算的自动结算机制、出勤日志生成。
 * ========================================================================== */

/**
 * @brief 新增一条合规的全局诊疗/手术记录
 * [新增架构逻辑]：强力支持外部急救子系统传入的 queue_num 排队号。
 * 若传入 queue_num 为 1 (表示救护车或急救中心强行插队)，则严格执行置顶；
 * 否则执行常规排序逻辑：检索指定医生名下的最大排队号并顺延排在末尾。
 */
void add_new_record(const char* patient_id, const char* doctor_id,
                    RecordType type, const char* description, double fee,
                    int queue_num) {
  RecordNode* new_node = (RecordNode*)calloc(1, sizeof(RecordNode));
  if (!new_node) return;

  // 算法：自动生成全局唯一的记录流水号 (基于当天日期 + 自增序号)
  {
    char ymd8[9] = {0};
    int i = 0;
    int j = 0;
    int seq_id = 1;

    get_current_time_str(new_node->date);
    for (i = 0; new_node->date[i] != '\0' && j < 8; ++i) {
      if (new_node->date[i] >= '0' && new_node->date[i] <= '9') {
        ymd8[j++] = new_node->date[i];
      }
    }
    ymd8[8] = '\0';

    seq_id = get_next_record_sequence_for_day(ymd8);
    snprintf(new_node->record_id, sizeof(new_node->record_id), "REC%s%03d",
             ymd8, seq_id);
    while (record_id_exists(new_node->record_id)) {
      seq_id++;
      snprintf(new_node->record_id, sizeof(new_node->record_id), "REC%s%03d",
               ymd8, seq_id);
    }
  }

  strcpy(new_node->patient_id, patient_id);
  strcpy(new_node->doctor_id, doctor_id);
  new_node->type = type;
  new_node->total_fee = fee < 0 ? 0 : fee;
  new_node->status = 0;  // 状态：0-待处理/未完结
  strcpy(new_node->description, description);

  // [新增架构逻辑] 完全尊重外界传入的优先级队列号
  // 支持 120 救护车或高权限系统的强行插队 (如 queue_num = 1)
  if (queue_num > 0) {
      new_node->queue_number = queue_num;
  } else {
      // 常规队列计算：查出当天同一医生业务下的最大队列号，自动加一排队
      if (type == RECORD_REGISTRATION || type == RECORD_EMERGENCY || type == RECORD_HOSPITALIZATION) {
        RecordNode* curr = record_head;
        int max_queue = 0;

        // 找到这段循环：
        while (curr) {
          if ((curr->type == RECORD_REGISTRATION ||
               curr->type == RECORD_EMERGENCY || curr->type == RECORD_HOSPITALIZATION) &&
              curr->status != -1 && strcmp(curr->doctor_id, doctor_id) == 0 &&
              // 【核心修复：将原来的 strcmp 替换为 strncmp，只比对前 10 位日期！】
              strncmp(curr->date, new_node->date, 10) == 0 && 
              curr->queue_number > max_queue) {
            max_queue = curr->queue_number;
          }
          curr = curr->next;
        }
        new_node->queue_number = max_queue + 1;
      } else {
          new_node->queue_number = 0;
      }
  }

  // [新增架构逻辑] 医保报销自动核算引擎
  PatientNode* p = find_outpatient_by_id(patient_id);
  if (!p) p = find_inpatient_by_id(patient_id);

  double coverage_rate = 0.0;
  if (p) {
    if (p->insurance == INSURANCE_WORKER)
      coverage_rate = 0.7;  // 职工医保按 70% 报销核算
    else if (p->insurance == INSURANCE_RESIDENT)
      coverage_rate = 0.5;  // 居民医保按 50% 报销核算
  }

  new_node->insurance_covered = new_node->total_fee * coverage_rate;
  normalize_record_amounts(new_node); // 进行对账清洗修正

  // 记录挂载入列
  new_node->next = record_head;
  record_head = new_node;
}

/**
 * @brief 记录救护车出勤日志工厂函数
 * 为 120 救护车生成带时间戳的轨迹追踪日志
 */
void add_ambulance_log(const char* vehicle_id, const char* destination) {
  AmbulanceLogNode* new_node =
      (AmbulanceLogNode*)calloc(1, sizeof(AmbulanceLogNode));
  if (!new_node) return;

  time_t now = time(NULL);
  sprintf(new_node->log_id, "ALB%lld", (long long)now);

  struct tm* t = localtime(&now);
  strftime(new_node->dispatch_time, 32, "%Y-%m-%d %H:%M:%S", t);

  strncpy(new_node->vehicle_id, vehicle_id, 31);
  strncpy(new_node->destination, destination, 127);

  new_node->next = ambulance_log_head;
  ambulance_log_head = new_node;
}

/* ==========================================================================
 * 数据反序列化加载器 (Data Loaders)
 * --------------------------------------------------------------------------
 * 核心逻辑：从 /data 目录下的文本数据库读取，以空格或 Tab 为分隔符重建内存链表。
 * ========================================================================== */

void load_departments(const char* filepath) {
  FILE* file = fopen(filepath, "r");
  char line[512];
  if (!file) return;
  fgets(line, 512, file); 
  while (fgets(line, 512, file)) {
    DepartmentNode* n = calloc(1, sizeof(DepartmentNode));
    if (sscanf(line, "%31s %63s", n->id, n->name) == 2) {
      n->next = dept_head;
      dept_head = n;
    } else
      free(n);
  }
  fclose(file);
}

/**
 * @brief 载入全院医生人事档案
 * [新增架构逻辑] 额外捕获并记录医生的手术权限资质 (privilege_level 1-4级)。
 * 这对于后台进行医疗安全防越权审计具有决定性作用。
 */
void load_doctors(const char* filepath) {
  FILE* file = fopen(filepath, "r");
  char line[512];
  if (!file) return;
  fgets(line, 512, file);
  while (fgets(line, 512, file)) {
    DoctorNode* n = calloc(1, sizeof(DoctorNode));
    
    int priv = 1; // 默认赋予最低的住院医权限，兼容旧版无权限字段的数据文件
    
    // [新增架构逻辑] 支持解析权限等级字段 (privilege_level)
    int parsed = sscanf(line, "%31s %127s %31s %d", n->id, n->name, n->dept_id, &priv);
    if (parsed >= 2) {
      if (parsed >= 4) {
          n->privilege_level = priv;
      } else {
          n->privilege_level = 1;
      }
      n->next = doctor_head;
      doctor_head = n;
    } else
      free(n);
  }
  fclose(file);
}

void load_nurses() {
    FILE* fp = fopen("data/nurses.txt", "r");
    if (!fp) return;
    
    char line[512];
    NurseNode* tail = NULL;
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;
        
        NurseNode* node = (NurseNode*)calloc(1, sizeof(NurseNode));
        if (sscanf(line, "%[^|]|%[^|]|%[^|]|%d", 
                   node->id, node->name, node->dept_id, &node->role_level) == 4) {
            if (!nurse_head) nurse_head = node;
            else tail->next = node;
            tail = node;
        } else {
            free(node);
        }
    }
    fclose(fp);
}

void save_nurses() {
    FILE* fp = fopen("data/nurses.txt", "w");
    if (!fp) return;
    
    NurseNode* curr = nurse_head;
    while (curr) {
        fprintf(fp, "%s|%s|%s|%d\n", curr->id, curr->name, curr->dept_id, curr->role_level);
        curr = curr->next;
    }
    fclose(fp);
}

void load_medicines(const char* filepath) {
  FILE* file = fopen(filepath, "r");
  char line[1024];

  if (!file) return;
  fgets(line, sizeof(line), file);

  while (fgets(line, sizeof(line), file)) {
    MedicineNode* n = calloc(1, sizeof(MedicineNode));
    char local[1024];
    char* fields[8] = {0};
    int count = 0;

    if (!n) continue;

    strncpy(local, line, sizeof(local) - 1);
    local[sizeof(local) - 1] = '\0';
    local[strcspn(local, "\r\n")] = '\0';

    if (local[0] == '\0' || is_blank_text(local)) {
      free(n);
      continue;
    }

    if ((strstr(local, UTF8_MED_COL_ID) && strstr(local, UTF8_MED_COL_PRICE)) ||
        (strstr(local, "medicine") && strstr(local, "price"))) {
      free(n);
      continue;
    }

    {
      char* token = strtok(local, "\t");
      while (token && count < 8) {
        fields[count++] = token;
        token = strtok(NULL, "\t");
      }
    }

    if (count >= 6) {
      safe_copy_text(n->id, sizeof(n->id), fields[0]);
      safe_copy_text(n->common_name, sizeof(n->common_name), fields[1]);
      safe_copy_text(n->trade_name, sizeof(n->trade_name), fields[2]);
      safe_copy_text(n->alias_name, sizeof(n->alias_name), fields[3]);
      n->price = strtod(fields[4], NULL);
      n->stock = atoi(fields[5]);
    } else if (count >= 5) {
      safe_copy_text(n->id, sizeof(n->id), fields[0]);
      safe_copy_text(n->common_name, sizeof(n->common_name), fields[1]);
      safe_copy_text(n->trade_name, sizeof(n->trade_name), fields[2]);
      safe_copy_text(n->alias_name, sizeof(n->alias_name), fields[2]);
      n->price = strtod(fields[3], NULL);
      n->stock = atoi(fields[4]);
    } else if (count >= 4) {
      safe_copy_text(n->id, sizeof(n->id), fields[0]);
      safe_copy_text(n->common_name, sizeof(n->common_name), fields[1]);
      safe_copy_text(n->trade_name, sizeof(n->trade_name), fields[1]);
      safe_copy_text(n->alias_name, sizeof(n->alias_name), fields[1]);
      n->price = strtod(fields[2], NULL);
      n->stock = atoi(fields[3]);
    } else if (sscanf(line, "%31s %255s %255s %255s %lf %d", n->id,
                      n->common_name, n->trade_name, n->alias_name, &n->price,
                      &n->stock) == 6) {
    } else if (sscanf(line, "%31s %255s %255s %lf %d", n->id, n->common_name,
                      n->trade_name, &n->price, &n->stock) == 5) {
      safe_copy_text(n->alias_name, sizeof(n->alias_name), n->trade_name);
    } else if (sscanf(line, "%31s %255s %lf %d", n->id, n->common_name,
                      &n->price, &n->stock) >= 3) {
      safe_copy_text(n->trade_name, sizeof(n->trade_name), n->common_name);
      safe_copy_text(n->alias_name, sizeof(n->alias_name), n->common_name);
    } else {
      free(n);
      continue;
    }

    if (n->trade_name[0] == '\0')
      safe_copy_text(n->trade_name, sizeof(n->trade_name), n->common_name);
    if (n->alias_name[0] == '\0')
      safe_copy_text(n->alias_name, sizeof(n->alias_name), n->trade_name);
    if (n->stock < 0) n->stock = 0;
    if (n->price < 0) n->price = 0;

    n->next = medicine_head;
    medicine_head = n;
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
    if (sscanf(line, "%31s %127s %d %15s %c %d %lf", n->id, n->name, &n->age,
               n->gender, &n->type, &ins, &n->account_balance) >= 6) {
      n->insurance = (InsuranceType)ins;
      n->next = outpatient_head;
      outpatient_head = n;
    } else
      free(n);
  }
  fclose(file);
}

/**
 * @brief 从数据文件加载住院患者大名单
 * [核心架构增强]：全面支持双医生责任制，可解析涵盖主治和管床医生的 12 列数据。
 * 同时保留向下兼容 10 列旧数据的能力，防呆防崩溃。
 */
void load_inpatients(const char* filepath) {
  FILE* file = fopen(filepath, "r");
  char line[1024];
  if (!file) return;
  fgets(line, 1024, file);
  while (fgets(line, 1024, file)) {
    PatientNode* n = calloc(1, sizeof(PatientNode));
    int ins;
    
    // 扩展读取：囊括主治和管床医生字段 (共 12 列)
    int parsed = sscanf(line, "%31s %127s %d %15s %c %d %lf %lf %31s %31s %31s %31s", 
                        n->id, n->name, &n->age, n->gender, &n->type, &ins, 
                        &n->account_balance, &n->hospital_deposit, n->admission_date, n->bed_id,
                        n->attending_doctor_id, n->resident_doctor_id);
               
    if (parsed >= 6) {
      n->insurance = (InsuranceType)ins;
      
      // 容错处理：如果文件是旧版，缺少医生字段，则初始化为 N/A 以防指针越界
      if (parsed < 11) strcpy(n->attending_doctor_id, "N/A");
      if (parsed < 12) strcpy(n->resident_doctor_id, "N/A");
      
      n->next = inpatient_head;
      inpatient_head = n;
    } else {
      free(n);
    }
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
    int parsed = sscanf(line, "%31s %31s %d %lf %31s", bed_id, dept_id,
                        &occupied, &daily_fee, patient_id);

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
    } else
      free(n);
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
    if (sscanf(line, "%31s %127s %d %127[^\n]", n->vehicle_id, n->driver_name,
               &s, n->current_location) >= 3) {
      n->status = (AmbStatus)s;
      n->next = ambulance_head;
      ambulance_head = n;
    } else
      free(n);
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
    if (sscanf(line, "%31s\t%31s\t%31s\t%127[^\n]", n->log_id, n->vehicle_id,
               n->dispatch_time, n->destination) >= 3) {
      n->next = ambulance_log_head;
      ambulance_log_head = n;
    } else
      free(n);
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
    if (sscanf(line, "%31s\t%d\t%31s\t%31s", n->room_id, &n->status,
               n->current_patient, n->current_doctor) >= 2) {
      n->next = or_head;
      or_head = n;
    } else
      free(n);
  }
  fclose(file);
}

/* ==========================================================================
 * 数据持久化序列化器 (Data Savers)
 * --------------------------------------------------------------------------
 * 核心逻辑：遍历内存中重构好的所有系统链表数据，
 * 依次写回 TXT 文件，并在写入医生档案时，强制持久化附加权限字段。
 * ========================================================================== */

void save_departments(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  fprintf(fp, "\u79d1\u5ba4\u7f16\u53f7\t\u79d1\u5ba4\u540d\u79f0\n");
  for (DepartmentNode* c = dept_head; c; c = c->next)
    fprintf(fp, "%s\t%s\n", c->id, c->name);
  fclose(fp);
}

/**
 * @brief 持久化保存全院医生人事档案
 * [新增架构逻辑]：在持久化过程中，不仅写入基本身份信息，额外序列化 privilege_level
 * 等级到文件行末，确保即使服务器关机重启权限认证策略依旧生效。
 */
void save_doctors(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  // 打印表头，补充“职称等级”标识
  fprintf(fp,
          "医生编号\t医生姓名\t科室编号\t职称等级\n");
  for (DoctorNode* c = doctor_head; c; c = c->next)
    fprintf(fp, "%s\t%s\t%s\t%d\n", c->id, c->name, c->dept_id, c->privilege_level);
  fclose(fp);
}

void save_medicines(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;

  fprintf(fp, "%s\t%s\t%s\t%s\t%s\t%s\n", UTF8_MED_COL_ID, UTF8_MED_COL_COMMON,
          UTF8_MED_COL_TRADE, UTF8_MED_COL_ALIAS, UTF8_MED_COL_PRICE,
          UTF8_MED_COL_STOCK);
  for (MedicineNode* c = medicine_head; c; c = c->next) {
    const char* trade = c->trade_name[0] ? c->trade_name : c->common_name;
    const char* alias = c->alias_name[0] ? c->alias_name : trade;
    fprintf(fp, "%s\t%s\t%s\t%s\t%.2f\t%d\n", c->id, c->common_name, trade,
            alias, c->price, c->stock);
  }

  fclose(fp);
}

void save_outpatients(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  fprintf(fp,
          "患者编号\t姓名\t年龄\t性别\t类型\t医保\t余额\n");
  for (PatientNode* c = outpatient_head; c; c = c->next)
    fprintf(fp, "%s\t%s\t%d\t%s\t%c\t%d\t%.2f\n", c->id, c->name, c->age,
            c->gender, c->type, (int)c->insurance, c->account_balance);
  fclose(fp);
}

/**
 * @brief 将住院患者大名单落盘序列化
 * [核心架构增强]：保存时将主治医生和管床医生一同固化到文件，确保财务对账不丢失责任人。
 */
void save_inpatients(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  // 更新表头，支持双医生列
  fprintf(fp,
          "患者编号\t姓名\t年龄\t性别\t类型\t医保\t余额\t押金\t入院日期\t床位编号\t主治医生\t管床医生\n");
  for (PatientNode* c = inpatient_head; c; c = c->next) {
    // 容错处理：确保如果医生工号为空时不发生越界
    const char* att_doc = (c->attending_doctor_id[0] != '\0') ? c->attending_doctor_id : "N/A";
    const char* res_doc = (c->resident_doctor_id[0] != '\0') ? c->resident_doctor_id : "N/A";
    
    fprintf(fp, "%s\t%s\t%d\t%s\t%c\t%d\t%.2f\t%.2f\t%s\t%s\t%s\t%s\n", 
            c->id, c->name, c->age, c->gender, c->type, (int)c->insurance, 
            c->account_balance, c->hospital_deposit, c->admission_date, c->bed_id,
            att_doc, res_doc);
  }
  fclose(fp);
}

void save_beds(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  fprintf(fp,
          "床位编号\t科室编号\t占用状态\t日费\t患者编号\n");
  for (BedNode* c = bed_head; c; c = c->next)
    fprintf(fp, "%s\t%s\t%d\t%.2f\t%s\n", c->bed_id, c->dept_id, c->is_occupied,
            c->daily_fee, c->patient_id);
  fclose(fp);
}

void save_records(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  fprintf(fp,
          "记录编号\t患者编号\t医生编号\t类型\t日期\t队列号\t状态\t描述\t总费用\t医保支付\t个人支付\n");
  for (RecordNode* c = record_head; c; c = c->next)
    fprintf(fp, "%s\t%s\t%s\t%d\t%s\t%d\t%d\t%s\t%.2f\t%.2f\t%.2f\n",
            c->record_id, c->patient_id, c->doctor_id, (int)c->type, c->date,
            c->queue_number, c->status, c->description, c->total_fee,
            c->insurance_covered, c->personal_paid);
  fclose(fp);
}

void save_ambulances(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  fprintf(
      fp,
      "车辆编号\t司机\t状态\t位置\n");
  for (AmbulanceNode* c = ambulance_head; c; c = c->next)
    fprintf(fp, "%s\t%s\t%d\t%s\n", c->vehicle_id, c->driver_name,
            (int)c->status, c->current_location);
  fclose(fp);
}

void save_ambulance_logs(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  fprintf(fp,
          "日志编号\t车辆编号\t出车时间\t目的地\n");
  for (AmbulanceLogNode* c = ambulance_log_head; c != NULL; c = c->next) {
    fprintf(fp, "%s\t%s\t%s\t%s\n", c->log_id, c->vehicle_id, c->dispatch_time,
            c->destination);
  }
  fclose(fp);
}

void save_operating_rooms(const char* f) {
  FILE* fp = fopen(f, "w");
  if (!fp) return;
  fprintf(fp,
          "手术室编号\t状态\t患者编号\t医生编号\n");
  for (OperatingRoomNode* c = or_head; c; c = c->next)
    fprintf(fp, "%s\t%d\t%s\t%s\n", c->room_id, c->status, c->current_patient,
            c->current_doctor);
  fclose(fp);
}

/* ==========================================================================
 * 值班表引擎管理 (Duty Schedule Management)
 * --------------------------------------------------------------------------
 * 负责医生在急诊与门诊系统的出勤排班逻辑
 * ========================================================================== */

#define DUTY_SCHEDULE_FILE "data/schedules.txt"

#define UTF8_SLOT_EMERGENCY "急诊"
#define UTF8_SLOT_OUTPATIENT "门诊"
#define UTF8_WORD_DATE "日期"
#define UTF8_WORD_DOCTOR "医生"
#define UTF8_WORD_CATEGORY "诊别"
#define UTF8_WORD_DEPT_ID "科室编号"
#define UTF8_WORD_DEPT_NAME "科室名称"
#define UTF8_WORD_DOCTOR_ID "医生编号"
#define UTF8_WORD_DOCTOR_NAME "医生姓名"
#define UTF8_TITLE_DUTY_SCHEDULE \
  "值班排班"
#define UTF8_MSG_EMPTY_SCHEDULE                                              \
  "当前没有符合条件的排班记录。"

typedef struct DutyScheduleNode {
  char date[32];
  char slot[32];
  char dept_id[MAX_ID_LEN];
  char doctor_id[MAX_ID_LEN];
  struct DutyScheduleNode* next;
} DutyScheduleNode;

static void normalize_schedule_slot(const char* input, char* output,
                                    size_t output_size) {
  if (!output || output_size == 0) return;
  output[0] = '\0';
  if (!input || input[0] == '\0') return;

  if (strstr(input, UTF8_SLOT_EMERGENCY) != NULL ||
      strstr(input, "EM") != NULL || strstr(input, "em") != NULL) {
    safe_copy_text(output, output_size, UTF8_SLOT_EMERGENCY);
  } else {
    safe_copy_text(output, output_size, UTF8_SLOT_OUTPATIENT);
  }
}

static int validate_schedule_date(const char* date) {
  int y, m, d;
  if (!date || sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) return 0;
  if (y < 2000 || y > 2100 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  return 1;
}

static int parse_schedule_line(const char* line, char* date, char* slot,
                               char* dept_id, char* doctor_id) {
  char local[256];
  char* fields[8] = {0};
  int count = 0;

  if (!line) return 0;
  strncpy(local, line, 255);
  local[255] = '\0';
  local[strcspn(local, "\r\n")] = '\0';

  if (local[0] == '\0' || is_blank_text(local)) return 0;
  if ((strstr(local, "date") && strstr(local, "doctor")) ||
      (strstr(local, UTF8_WORD_DATE) && strstr(local, UTF8_WORD_DOCTOR))) {
    return 0;
  }

  {
    char* token = strtok(local, "\t");
    while (token && count < 8) {
      fields[count++] = token;
      token = strtok(NULL, "\t");
    }
  }

  if (count >= 4) {
    safe_copy_text(date, 32, fields[0]);
    normalize_schedule_slot(fields[1], slot, 32);
    safe_copy_text(dept_id, MAX_ID_LEN, fields[2]);
    safe_copy_text(doctor_id, MAX_ID_LEN, fields[3]);
    return 1;
  }

  if (sscanf(local, "%31s %31s %31s %31s", date, slot, dept_id, doctor_id) ==
      4) {
    char normalized[32] = {0};
    normalize_schedule_slot(slot, normalized, sizeof(normalized));
    safe_copy_text(slot, 32, normalized);
    return 1;
  }

  return 0;
}

static DutyScheduleNode* create_schedule_node(const char* date,
                                              const char* slot,
                                              const char* dept_id,
                                              const char* doctor_id) {
  DutyScheduleNode* node =
      (DutyScheduleNode*)calloc(1, sizeof(DutyScheduleNode));
  if (!node) return NULL;

  safe_copy_text(node->date, sizeof(node->date), date);
  safe_copy_text(node->slot, sizeof(node->slot), slot);
  safe_copy_text(node->dept_id, sizeof(node->dept_id), dept_id);
  safe_copy_text(node->doctor_id, sizeof(node->doctor_id), doctor_id);
  node->next = NULL;

  return node;
}

static void free_schedule_list(DutyScheduleNode* head) {
  while (head) {
    DutyScheduleNode* next = head->next;
    free(head);
    head = next;
  }
}

static DutyScheduleNode* load_schedule_list(void) {
  FILE* file = fopen(DUTY_SCHEDULE_FILE, "r");
  char line[256];
  DutyScheduleNode* head = NULL;
  DutyScheduleNode* tail = NULL;

  if (!file) return NULL;

  while (fgets(line, sizeof(line), file)) {
    char date[32] = {0};
    char slot[32] = {0};
    char dept_id[MAX_ID_LEN] = {0};
    char doctor_id[MAX_ID_LEN] = {0};
    DutyScheduleNode* node;

    if (!parse_schedule_line(line, date, slot, dept_id, doctor_id)) continue;

    node = create_schedule_node(date, slot, dept_id, doctor_id);
    if (!node) continue;

    if (!head) {
      head = tail = node;
    } else {
      tail->next = node;
      tail = node;
    }
  }

  fclose(file);
  return head;
}

static int save_schedule_list(const DutyScheduleNode* head) {
  FILE* file = fopen(DUTY_SCHEDULE_FILE, "w");
  const DutyScheduleNode* curr;
  if (!file) return 0;

  fprintf(file, "%s\t%s\t%s\t%s\n", UTF8_WORD_DATE, UTF8_WORD_CATEGORY,
          UTF8_WORD_DEPT_ID, UTF8_WORD_DOCTOR_ID);
  for (curr = head; curr; curr = curr->next) {
    fprintf(file, "%s\t%s\t%s\t%s\n", curr->date, curr->slot, curr->dept_id,
            curr->doctor_id);
  }

  fclose(file);
  return 1;
}

void ensure_duty_schedule_file(void) {
  FILE* file = fopen(DUTY_SCHEDULE_FILE, "r");
  if (file) {
    fclose(file);
    return;
  }

  file = fopen(DUTY_SCHEDULE_FILE, "w");
  if (!file) return;
  fprintf(file, "%s\t%s\t%s\t%s\n", UTF8_WORD_DATE, UTF8_WORD_CATEGORY,
          UTF8_WORD_DEPT_ID, UTF8_WORD_DOCTOR_ID);
  fclose(file);
}

int is_doctor_on_duty_today(const char* doctor_id) {
  DutyScheduleNode* list;
  DutyScheduleNode* curr;
  char today[32] = {0};
  int on_duty = 0;

  if (!doctor_id || doctor_id[0] == '\0') return 0;

  get_current_time_str(today);
  today[10] = '\0';
  list = load_schedule_list();

  for (curr = list; curr; curr = curr->next) {
    if (strcmp(curr->doctor_id, doctor_id) == 0 &&
        strncmp(curr->date, today, 10) == 0) {
      on_duty = 1;
      break;
    }
  }

  free_schedule_list(list);
  return on_duty;
}

int is_doctor_on_duty_today_by_slot(const char* doctor_id, const char* slot) {
  DutyScheduleNode* list;
  DutyScheduleNode* curr;
  char today[32] = {0};
  char slot_norm[32] = {0};
  int on_duty = 0;

  if (!doctor_id || doctor_id[0] == '\0') return 0;

  normalize_schedule_slot(slot, slot_norm, sizeof(slot_norm));
  if (slot_norm[0] == '\0') return 0;

  get_current_time_str(today);
  today[10] = '\0';
  list = load_schedule_list();

  for (curr = list; curr; curr = curr->next) {
    if (strcmp(curr->doctor_id, doctor_id) == 0 &&
        strncmp(curr->date, today, 10) == 0 &&
        strcmp(curr->slot, slot_norm) == 0) {
      on_duty = 1;
      break;
    }
  }

  free_schedule_list(list);
  return on_duty;
}

int verify_doctor_shift(const char* doctor_id, const char* date, const char* slot) {
  DutyScheduleNode* list;
  DutyScheduleNode* curr;
  char slot_norm[32] = {0};
  int on_duty = 0;

  if (!doctor_id || doctor_id[0] == '\0') return 0;

  normalize_schedule_slot(slot, slot_norm, sizeof(slot_norm));
  if (slot_norm[0] == '\0') return 0;

  list = load_schedule_list();

  for (curr = list; curr; curr = curr->next) {
    if (strcmp(curr->doctor_id, doctor_id) == 0 &&
        strncmp(curr->date, date, 10) == 0 &&
        strcmp(curr->slot, slot_norm) == 0) {
      on_duty = 1;
      break;
    }
  }

  free_schedule_list(list);
  return on_duty;
}

void view_duty_schedule(const char* doctor_id_filter) {
  DutyScheduleNode* list = load_schedule_list();
  DutyScheduleNode* curr;
  int shown = 0;

  print_header(UTF8_TITLE_DUTY_SCHEDULE);
  printf("%-12s %-8s %-10s %-14s %-10s %-14s\n", UTF8_WORD_DATE,
         UTF8_WORD_CATEGORY, UTF8_WORD_DEPT_ID, UTF8_WORD_DEPT_NAME,
         UTF8_WORD_DOCTOR_ID, UTF8_WORD_DOCTOR_NAME);
  print_divider();

  for (curr = list; curr; curr = curr->next) {
    DoctorNode* d;
    DepartmentNode* dept;

    if (doctor_id_filter && doctor_id_filter[0] != '\0' &&
        strcmp(curr->doctor_id, doctor_id_filter) != 0)
      continue;

    d = find_doctor_by_id(curr->doctor_id);
    dept = find_department_by_id(curr->dept_id);

    printf("%-12s %-8s %-10s %-14s %-10s %-14s\n", curr->date, curr->slot,
           curr->dept_id, dept ? dept->name : "-", curr->doctor_id,
           d ? d->name : "-");
    shown++;
  }

  if (shown == 0)
    printf(COLOR_YELLOW "%s\n" COLOR_RESET, UTF8_MSG_EMPTY_SCHEDULE);
  free_schedule_list(list);
  wait_for_enter();
}

int upsert_duty_schedule(const char* date, const char* slot,
                         const char* dept_id, const char* doctor_id) {
  DutyScheduleNode* list;
  DutyScheduleNode* curr;
  DutyScheduleNode* tail = NULL;
  DutyScheduleNode* inserted;
  char slot_norm[32] = {0};
  int saved;

  if (!validate_schedule_date(date)) return 0;
  normalize_schedule_slot(slot, slot_norm, sizeof(slot_norm));
  if (slot_norm[0] == '\0') return 0;

  list = load_schedule_list();

  for (curr = list; curr; curr = curr->next) {
    tail = curr;
    if (strcmp(curr->date, date) == 0 && strcmp(curr->slot, slot_norm) == 0 &&
        strcmp(curr->dept_id, dept_id) == 0) {
      safe_copy_text(curr->doctor_id, MAX_ID_LEN, doctor_id);
      saved = save_schedule_list(list);
      free_schedule_list(list);
      return saved;
    }
  }

  inserted = create_schedule_node(date, slot_norm, dept_id, doctor_id);
  if (!inserted) {
    free_schedule_list(list);
    return 0;
  }

  if (!list) {
    list = inserted;
  } else {
    tail->next = inserted;
  }

  saved = save_schedule_list(list);
  free_schedule_list(list);
  return saved;
}

int remove_duty_schedule(const char* date, const char* slot,
                         const char* dept_id, const char* doctor_id) {
  DutyScheduleNode* list;
  DutyScheduleNode* curr;
  DutyScheduleNode* prev = NULL;
  char slot_norm[32] = {0};
  int removed = 0;
  int saved;

  normalize_schedule_slot(slot, slot_norm, sizeof(slot_norm));
  if (slot_norm[0] == '\0') return 0;

  list = load_schedule_list();
  curr = list;

  while (curr) {
    if (strcmp(curr->date, date) == 0 && strcmp(curr->slot, slot_norm) == 0 &&
        strcmp(curr->dept_id, dept_id) == 0 &&
        strcmp(curr->doctor_id, doctor_id) == 0) {
      DutyScheduleNode* to_delete = curr;
      if (prev)
        prev->next = curr->next;
      else
        list = curr->next;
      curr = curr->next;
      free(to_delete);
      removed = 1;
      break;
    }
    prev = curr;
    curr = curr->next;
  }

  if (!removed) {
    free_schedule_list(list);
    return 0;
  }

  saved = save_schedule_list(list);
  free_schedule_list(list);
  return saved;
}

/* ==========================================================================
 * 系统的生命周期管理引擎 (System Lifecycle Core)
 * --------------------------------------------------------------------------
 * 启动时一键拉起所有数据文件载入内存，终止时一键将所有节点回刷。
 * ========================================================================== */

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

  // 若启动时未能从文件读取到急诊手术室数据，则硬编码生成 OR-01 至 OR-03 为备用抢救室
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
#define FREE_NODES(head, type) \
  while (head) {               \
    type* temp = head;         \
    head = head->next;         \
    free(temp);                \
  }
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