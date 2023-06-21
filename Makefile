#-Wall 是打开警告开关
# Wno-unused 变量定义未使用
# Wno-format 含NUL字节的格式字符串
#-O代表默认优化，可选：-O0不优化，-O1低级优化，-O2中级优化，-O3高级优化，-Os代码空间优化。
#-g是生成调试信息，生成的可执行文件具有和源代码关联的可调试的信息
# filter-out 过滤函数
#-dl ，是显式加载动态库的动态函数库
#so编译
#	CFLAGS需要加上 -fPIC
#	LINKFLAGS需要加上 -shared -fPIC
#可执行文件编译
#	CFLAGS不需要 -fPIC
#	LINKFLAGS为空
####################################
#   万能makefile		####
#   Created on: 2019年9月11日	####
#   Author: 汪伟		####
####################################

# 设置目标类型(app, ar, so), 及目标名字

TARGET_TYPE 		:= app
TARGET_NAME			:= $(shell basename $(PWD))
SYSTEM_TYPE			:= x86

CROSS_COMPILE 			:=
GCC 				:= $(CROSS_COMPILE)gcc
CXX 				:= $(CROSS_COMPILE)g++
AR				:= $(CROSS_COMPILE)ar

# 源文件可能的后缀
SRCEXTS 			:= c C cc cpp CPP c++ cxx cp
HDREXTS 			:= h H hh hpp HPP h++ hxx hp

# 其他变量
DEFS     			:= -DHAVE_CONFIG_H -D__x86_64__ -DOS_LINUX

OPTIMIZE 			:= #-O3#-O2

ifeq ($(TARGET_TYPE),app)
	WARNINGS 		:= -Wall -Wno-unused -Wno-format
else 
	WARNINGS 		:= -Wall -Wno-unused -Wno-format# -mcpu=cortex-a9 -mfloat-abi=softfp -mfpu=vfpv3-d16
endif

# 指定源文件目录, 以及目标文件生成目录

PROJECT_DIR      		:= $(shell pwd)

INC_PATH			:= $(PROJECT_DIR)				\
						$(PROJECT_DIR)/src			\
						$(PROJECT_DIR)/sqlite3	\
						$(PROJECT_DIR)/http	\
						$(PROJECT_DIR)/handy

SRC_PATH			:= $(PROJECT_DIR)			\
					$(PROJECT_DIR)/src			\
					$(PROJECT_DIR)/sqlite3	\
				   $(PROJECT_DIR)/http		\
				   	$(PROJECT_DIR)/handy


OBJ_PATH 			:= $(PROJECT_DIR)/obj

LIBA_PATH      			:= $(PROJECT_DIR)/lib

LIBS_PATH      			:= $(PROJECT_DIR)/Bin

LIBA    			:=
				
LIBS    			:=


# 初始化编译选项
INCLUDE :=

CFLAGS  := -g -rdynamic $(OPTIMIZE) $(WARNINGS) $(DEFS)

CPPFLAGS:= -g -rdynamic -std=c++11 $(OPTIMIZE) $(WARNINGS) $(DEFS)

LDFLAGS := -L$(LIBA_PATH) -L$(LIBS_PATH) -lm -ldl -lpthread  $(LIBA) $(LIBS) -Wl,-rpath=./

#编译时间
_XUE_COMPILE_TIME = $(shell date +"%Y-%m-%d")

$(warning "XUE_COMPILE_TIME=$(_XUE_COMPILE_TIME)")

#COM_DEFS += XUE_COMPILE_TIME=$(_XUE_COMPILE_TIME)

CFLAGS += -DXUE_COMPILE_TIME=$(_XUE_COMPILE_TIME)
CPPFLAGS += -DXUE_COMPILE_TIME=$(_XUE_COMPILE_TIME)

#####################################
GIT_SHA := $(shell git rev-list HEAD | awk 'NR==1')
GIT_SVR_PATH := $(shell git remote -v | awk 'NR==1' | sed 's/[()]//g' | sed 's/\t/ /g' | cut -d " " -f2)
GIT_BRANCH := $(shell git branch | sed -n '/\* /s///p')
GIT_TAG_LABEL := $(shell git tag --sort=-taggerdate | head -n 1)

ifeq ($(GIT_SHA),)
CPPFLAGS += -DGIT_COMMIT=\"unknown\"
else
CPPFLAGS += -DGIT_COMMIT=\"$(GIT_SHA)\"
endif

CPPFLAGS += -DGIT_PATH=\"$(GIT_SVR_PATH)\"
CPPFLAGS += -DGIT_BRANCH=\"$(GIT_BRANCH)\"
CPPFLAGS += -DGIT_TAG=\"$(GIT_TAG_LABEL)\"
#####################################

# 额外增加的源文件或者排除不编译的源文件
SPECIAL_SRC :=
EXCLUDE_SRC := 

ifeq ($(TARGET_TYPE), so)
	TARGET_NAME := $(LIBS_PATH)/$(shell basename $(PWD).so)	

else ifeq ($(TARGET_TYPE), ar)
	TARGET_NAME := $(LIBA_PATH)/$(shell basename $(PWD).a)
else 
	TARGET_NAME := $(PROJECT_DIR)/Bin/$(SYSTEM_TYPE)/$(shell basename $(PWD))
endif

# 追加搜索目录
SRC_PATH	+= $(dir $(SPECIAL_SRC))
INC_PATH 	+= $(SRC_PATH)

# 源文件, 头文件, 链接文件
SOURCES = $(filter-out $(EXCLUDE_SRC)/, $(foreach d, $(SRC_PATH), $(wildcard $(addprefix $(d)/*., $(SRCEXTS)))))
HEADERS = $(foreach d, $(INC_PATH)/, $(wildcard $(addprefix $(d)/*., $(HDREXTS))))
OBJECTS = $(addprefix  $(OBJ_PATH)/, $(addsuffix .o, $(basename $(notdir $(SOURCES)))))

# 配置编译选项
INCLUDE += $(foreach d, $(sort $(INC_PATH)), -I$d)

ifeq ($(TARGET_TYPE), ar)
	CFLAGS  += -fpic
	CPPFLAGS += -fpic
else ifeq ($(TARGET_TYPE), so)
	CFLAGS  += -fpic -shared
	CPPFLAGS  += -fpic -shared
	LDFLAGS += -shared
endif

# 定义伪目标
PHONY = all .mkdir clean

all: .mkdir $(TARGET_NAME)

# 函数: 添加%.x依赖文件的路径
define add_vpath
$1

endef

# 函数: 生成中间文件
define gen_o_cmd
#$2/%.o: %.$1
$2/%.o:%.c
	$(GCC) $(INCLUDE) -Wp,-MT,$$@ -Wp,-MMD,$$@.d $(CFLAGS) -c -o $$@ $$<
$2/%.o:%.cpp
	$(CXX) $(INCLUDE) -Wp,-MT,$$@ -Wp,-MMD,$$@.d $(CPPFLAGS) -c -o $$@ $$<

endef

# 执行函数, make会将函数里的内容当Makefile解析, 注意$$符号
$(eval $(foreach i, $(SRCEXTS), $(foreach d, $(SRC_PATH), $(call add_vpath,vpath %.$i $d))))
$(eval $(foreach i, $(SRCEXTS), $(call gen_o_cmd,$i,$(OBJ_PATH))))

# 静态, 动态, 可执行
ifeq ($(TARGET_TYPE), ar)
$(TARGET_NAME): $(OBJECTS)
	rm -f $@
	$(AR) rcvs $@ $(OBJECTS)
else 
$(TARGET_NAME): LD=$(if $(filter-out %.c %.C, $(SOURCES)),$(CXX))
$(TARGET_NAME): $(OBJECTS)
	$(LD) $(OBJECTS) -o $@ $(LDFLAGS) 
endif

.mkdir:
	@echo "=============================================="
	@if [ ! -d $(OBJ_PATH) ]; then mkdir -p $(OBJ_PATH); fi
	@echo "====== create object directory successfully, $(OBJ_PATH) ===="
ifeq ($(TARGET_TYPE), ar)
	@if [ ! -d $(LIBA_PATH) ]; then mkdir -p $(LIBA_PATH); fi
	@echo "====== create object directory successfully, $(LIBA_PATH) ===="
else
	@if [ ! -d $(LIBS_PATH) ]; then mkdir -p $(LIBS_PATH); fi
	@echo "====== create object directory successfully, $(LIBS_PATH) ===="
endif
	@echo "=============================================="

clean:
	rm -rf $(OBJ_PATH) $(TARGET_NAME)

run:$(TARGET_NAME)
	@$(TARGET_NAME)
	

.PHONY: $(PHONY) 
