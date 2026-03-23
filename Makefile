# 编译器与编译选项
CC = g++
CFLAGS = -std=c++17 -Wall -Iinclude
LDFLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

# 目录定义
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# 源文件与目标文件
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = $(BIN_DIR)/brick_breaker

# 默认目标：编译+运行
all: $(TARGET) run

# 创建目录
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# 编译 .cpp 到 .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接生成可执行文件
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# 运行游戏
run: $(TARGET)
	./$(TARGET)

# 清理编译产物
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all run clean