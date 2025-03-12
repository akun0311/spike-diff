
# 项目介绍

此项目是一生一芯讲义框架中的`spike-diff`,单独抽取了出来使用

## 编译成适配riscv32或riscv64
需要修改`difftest.cc`里面的rv32或者rv64宏，这个宏控制着是编译rv32还是rv64

## 编译后的名字
由`Makefile`中的`NAME = riscv-spike-so` 和`BINARY = $(BUILD_DIR)/$(NAME)`控制