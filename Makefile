#克隆仓库，并把仓库命名为repo
REPO_PATH = repo
ifeq ($(wildcard repo/spike_main),)
  $(shell git clone --depth=1 git@github.com:NJU-ProjectN/riscv-isa-sim $(REPO_PATH))
endif

#编译spike的build和Makefile
REPO_BUILD_PATH = $(REPO_PATH)/build
REPO_MAKEFILE = $(REPO_BUILD_PATH)/Makefile

#制作编译Spike的Makefile
# $(REPO_MAKEFILE):
# 	@mkdir -p $(@D)
# 	cd $(@D) && $(abspath $(REPO_PATH))/configure
# 	sed -i -e 's/-g -O2/-O2/' $@

$(REPO_MAKEFILE):
	$(info [----------------------INFO] Creating directory $(@D))
	@mkdir -p $(@D)
	$(info [----------------------INFO] Running configure script in $(@D))
	cd $(@D) && $(abspath $(REPO_PATH))/configure
	$(info [----------------------INFO] Modifying Makefile: replacing -g -O2 with -O2)
	sed -i -e 's/-g -O2/-O2/' $@

SPIKE = $(REPO_BUILD_PATH)/spike
$(SPIKE): $(REPO_MAKEFILE)
	CFLAGS="-fvisibility=hidden" CXXFLAGS="-fvisibility=hidden" $(MAKE) -C $(^D)

BUILD_DIR = ./build
$(shell mkdir -p $(BUILD_DIR))

inc_dependencies = fesvr riscv disasm customext fdt softfloat spike_main spike_dasm build
INC_PATH  = -I$(REPO_PATH) $(addprefix -I$(REPO_PATH)/, $(inc_dependencies))

lib_dependencies = libspike_main.a libriscv.a libdisasm.a libsoftfloat.a libfesvr.a libfdt.a
INC_LIBS  = $(addprefix $(REPO_PATH)/build/, $(lib_dependencies))

NAME = riscv64-spike-so
BINARY = $(BUILD_DIR)/$(NAME)
SRCS = difftest.cc

$(BINARY): $(SPIKE) $(SRCS)
	g++ -std=c++17 -O2 -shared -fPIC -fvisibility=hidden $(INC_PATH) $(SRCS) $(INC_LIBS) -o $@
clean:
	rm -rf $(BUILD_DIR)
all: $(BINARY)
#.DEFAULT_GOAL = all
.PHONY: all clean $(SPIKE)
.DEFAULT_GOAL = $(BINARY)
