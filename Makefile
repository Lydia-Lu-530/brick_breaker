# 编译器与编译选项
CC = g++
CFLAGS = -std=c++17 -Wall -Iinclude
LDFLAGS = -lraylib -lm -lpthread -ldl -lrt -lX11

# 目录定义
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = test  # 新增：测试文件目录（建议把测试文件移到test/，更规范）

# 源文件与目标文件
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
# 排除测试文件（避免编译游戏时包含测试代码）
SRCS := $(filter-out $(SRC_DIR)/test_collision.cpp, $(SRCS))
OBJS = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
TARGET = $(BIN_DIR)/brick_breaker

# 测试相关配置
TEST_SRC = $(SRC_DIR)/test_collision.cpp  # 如果你没移到test/，就用这个路径
TEST_OBJ = $(OBJ_DIR)/test_collision.o
TEST_TARGET = $(BIN_DIR)/collision_test
# gtest 链接库（必须加 pthread）
GTEST_LIBS = -lgtest -lgtest_main -lpthread

# 默认目标：编译+运行游戏（原有逻辑不变）
all: $(TARGET)

# 创建目录
$(OBJ_DIR) $(BIN_DIR) $(TEST_DIR):
	mkdir -p $@

# 编译 .cpp 到 .o（原有逻辑不变）
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接生成游戏可执行文件
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# 运行游戏
run: $(TARGET)
	./$(TARGET)

# ---------------------- 新增：单元测试规则 ----------------------
# 编译测试目标文件
$(TEST_OBJ): $(TEST_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接生成测试可执行文件
$(TEST_TARGET): $(OBJ_DIR)/Ball.o $(OBJ_DIR)/Brick.o $(TEST_OBJ) | $(BIN_DIR)
	$(CC) $(OBJ_DIR)/Ball.o $(OBJ_DIR)/Brick.o $(TEST_OBJ) -o $@ $(GTEST_LIBS) $(LDFLAGS)

# 运行单元测试
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# 清理编译产物
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all run clean test