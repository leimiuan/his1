# ==========================================
# 医疗管理系统 (HIS) 编译配置文件
# ==========================================

CC = gcc
CFLAGS = -Iinclude -Wall -g
TARGET = his_system.exe
SRCDIR = src
OBJDIR = obj
TMPDIR = tmp

export TMP = $(TMPDIR)
export TEMP = $(TMPDIR)

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

all: $(OBJDIR) $(TMPDIR) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)
	@echo ======================================
	@echo 编译成功！输入 .\$(TARGET) 即可运行程序
	@echo ======================================

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	@if not exist $(OBJDIR) mkdir $(OBJDIR)

$(TMPDIR):
	@if not exist $(TMPDIR) mkdir $(TMPDIR)

clean:
	@if exist $(OBJDIR) rd /s /q $(OBJDIR)
	@if exist $(TARGET) del $(TARGET)
	@echo 项目已清理干净。


