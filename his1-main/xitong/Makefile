# ==========================================
# 医疗管理系统 (HIS) 编译配置文件
# ==========================================

# 1. 指定编译器
CC = gcc

# 2. 编译选项
# -Iinclude: 告诉编译器去 include 文件夹找头文件
# -Wall: 显示所有警告（帮助你定位潜在 Bug）
# -g: 包含调试信息
CFLAGS = -Iinclude -Wall -g

# 3. 目标文件名
TARGET = his_system.exe

# 4. 目录定义
SRCDIR = src
OBJDIR = obj

# 5. 自动搜索所有的源文件和生成目标对象文件
# 自动寻找 src 目录下所有的 .c 文件
SRCS = $(wildcard $(SRCDIR)/*.c)
# 将 src/*.c 映射为 obj/*.o
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

# 默认执行任务
all: $(OBJDIR) $(TARGET)

# 链接步骤：把所有的 .o 文件打包成 .exe
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)
	@echo ======================================
	@echo 编译成功！输入 .\$(TARGET) 即可运行程序
	@echo ======================================

# 编译步骤：把每个 .c 文件编译成 .o 文件
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 创建用于存放临时编译文件的 obj 文件夹
$(OBJDIR):
	@if not exist $(OBJDIR) mkdir $(OBJDIR)

# 清理任务：删除编译生成的临时文件和 exe
clean:
	@if exist $(OBJDIR) rd /s /q $(OBJDIR)
	@if exist $(TARGET) del $(TARGET)
	@echo 项目已清理干净。