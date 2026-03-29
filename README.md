# 全功能医疗管理系统 (HIS)

基于 C 语言开发的医院信息管理系统（Hospital Information System），支持管理员、医生、患者三种角色登录，提供门诊挂号、住院管理、医嘱开具、费用结算等核心功能。

## 代码行数统计

| 文件 | 行数 |
|------|------|
| `xitong/src/main.c` | 87 |
| `xitong/src/utils.c` | 125 |
| `xitong/src/doctor.c` | 287 |
| `xitong/src/patient.c` | 386 |
| `xitong/src/data_manager.c` | 659 |
| `xitong/src/admin.c` | 822 |
| `xitong/include/admin.h` | 52 |
| `xitong/include/utils.h` | 56 |
| `xitong/include/patient.h` | 57 |
| `xitong/include/doctor.h` | 60 |
| `xitong/include/data_manager.h` | 67 |
| `xitong/include/common.h` | 144 |
| **合计** | **2802** |

> 统计范围：`xitong/src/` 和 `xitong/include/` 下的所有 `.c` / `.h` 源文件，共 **2802 行**。

## 项目结构

```
xitong/
├── src/            # C 源文件
│   ├── main.c          # 程序入口与主菜单
│   ├── admin.c         # 管理员模块
│   ├── doctor.c        # 医生模块
│   ├── patient.c       # 患者模块
│   ├── data_manager.c  # 数据持久化
│   └── utils.c         # 工具函数
├── include/        # 头文件
│   ├── common.h        # 公共定义
│   ├── admin.h
│   ├── doctor.h
│   ├── patient.h
│   ├── data_manager.h
│   └── utils.h
├── data/           # 数据文件（txt）
├── obj/            # 编译中间文件
└── Makefile
```

## 编译与运行

```bash
cd xitong
make
./his_system.exe   # Windows
```
