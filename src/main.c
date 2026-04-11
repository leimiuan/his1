// [核心架构说明]：
// 本文件为整个 HIS（医院信息系统）的顶级主程序入口 (Main Entry Point)。
// 核心职责：
// 1. 初始化终端环境（解决跨平台与 Windows 的 UTF-8 编码乱码问题）；
// 2. 调度 data_manager 统一下载全盘数据，重建内存链表；
// 3. 作为“总路由”，负责分发进入 患者端、医生端、管理后台 及 120急救调度中心；
// 4. 监听系统退出指令，确保退出前执行全量数据落盘与内存安全释放。

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "admin.h"
#include "ambulance.h"
#include "common.h"
#include "data_manager.h"
#include "doctor.h"
#include "patient.h"
#include "utils.h"

// [新增架构]：引入护士工作站系统入口
extern void nurse_subsystem();

/**
 * @brief 渲染并显示系统顶级的主干导航菜单
 * 涵盖了系统的五大核心业务中枢
 */
static void show_main_menu(void) {
  print_header("医院信息系统主菜单");
  printf("1. 患者端\n");
  printf("2. 医生端\n");
  printf("3. 管理端\n");
  printf(
      "4. 急救中心\n");  // [架构联动] 这里是跳转至 ambulance_subsystem() 的入口
  printf("5. 护士工作站\n");  // [新增入口] 护士站
  printf("0. 保存并退出\n");
  print_divider();
  printf("请选择: ");
}

int main(void) {
  int choice = -1;

  // [环境初始化]：强制将 Windows CMD/PowerShell 的代码页切换为 65001 (UTF-8)
  // 彻底解决之前由于 GBK 导致的中文乱码及生僻字显示问题
  SetConsoleOutputCP(65001);
  SetConsoleCP(65001);

  // [系统启动]：打印启动引导并加载底层数据
  printf(COLOR_BLUE "正在启动医院信息系统...\n" COLOR_RESET);
  show_progress_bar("正在加载系统数据");

  // 调用 data_manager.c 的接口，将 txt 文本库全量反序列化至内存链表
  init_system_data();

  // 顶级事件循环 (Event Loop)，阻塞式监听用户指令，直至输入 0 安全退出
  while (1) {
    show_main_menu();
    choice = safe_get_int();  // 拦截非法非数字输入，防止崩溃

    switch (choice) {
      case 1:
        // 进入患者自助服务子系统（挂号、缴费、查报告）
        patient_subsystem();
        break;
      case 2:
        // 进入医生工作站子系统（接诊、开处方、安排手术、重症收治等）
        doctor_subsystem();
        break;
      case 3:
        // 进入系统管理与决策后台（人事权限定级、药品审计、财务预警等）
        admin_subsystem();
        break;
      case 4:
        // 进入120急救调度与急诊绿通子系统（支持救护车状态监控与自动强制插队）
        ambulance_subsystem();
        break;
      case 5:
        // [新增联动] 进入护士工作站子系统（查房、发药、测体征）
        nurse_subsystem();
        break;
      case 0:
        // [生命周期管理]：安全退出与持久化流程
        printf(COLOR_YELLOW "正在保存数据...\n" COLOR_RESET);
        show_progress_bar("正在保存系统数据");

        // 将当前内存中的链表树状结构全量序列化并覆盖写入 data/ 目录下的 txt
        save_system_data();

        // 逐个释放所有链表节点，防止内存泄漏 (Memory Leak)
        free_all_data();

        printf(COLOR_GREEN "系统已安全退出。\n" COLOR_RESET);
        return 0;  // 终止进程
      default:
        // 容错处理机制
        printf(COLOR_RED "无效选项。\n" COLOR_RESET);
        wait_for_enter();
        break;
    }
  }
}