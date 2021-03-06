-include CrossCompile.mak
#CROSS_COMPILE := arm-none-linux-gnueabi-
CROSS_COMPILE := 
CC := $(CROSS_COMPILE)gcc  
CPP := $(CROSS_COMPILE)g++
AR := $(CROSS_COMPILE)ar
TARGET := cloudfs
PACK_DIR := ./pack/
CMPL_INC := .
CFLAGS := -g -Wall -fmessage-length=0 -DFUSE_USE_VERSION=26 `pkg-config fuse --cflags`  -I ./include -I $(CMPL_INC)
CFLAGS += -I /usr/include/libxml2 -I ./OssCLib/include 
#LDFLAGS := -L ./OssCLib/lib -L ./lib  -lossc -lfuse -lrt -lxml2 -lcurl -lssl -lcrypto -lunwind -ltcmalloc -lpthread -ldl
LDFLAGS := -L ./OssCLib/lib -L ./lib  -lossc -lfuse -lrt -lxml2 -lcurl -lssl -lcrypto -lpthread -ldl
TYPE := exe
OBJ_DIR := obj
EXCLUDE_DIRS := $(OBJ_DIR) .svn obj conf
EXCLUDE_FILES := ./simple_test.cpp ./xml_test.cpp
CURDIR=`pwd`

SUB_DIRS := $(shell ls -F | grep [/$$] | sed 's/\///g')
SUB_DIRS := $(filter-out $(EXCLUDE_DIRS), $(SUB_DIRS))
CURRENT_DIR_SRCS = $(shell find . -maxdepth 1 -name '*.cpp')
CURRENT_DIR_SRCS += $(shell find . -maxdepth 1 -name '*.c')
CURRENT_DIR_SRCS := $(filter-out $(EXCLUDE_FILES), $(CURRENT_DIR_SRCS))

OBJS := $(basename $(notdir $(CURRENT_DIR_SRCS)))  
DEPS := $(OBJS:%=$(OBJ_DIR)/%.d)
OBJS := $(OBJS:%=$(OBJ_DIR)/%.o)
TYPE := $(strip $(TYPE))

all : BEFOREBUILD $(TARGET) AFTERBUILD

$(TARGET) : $(OBJS) $(DEPS)
ifeq ($(TYPE),exe)
	@echo "build exe...."
	$(CPP) $(OBJS) $(LDFLAGS) -o $(TARGET)
#	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)
else ifeq ($(TYPE),static)
	@echo "build static library..."
	$(AR) rcs $(TARGET) $(OBJS)
else ifeq ($(TYPE),shared)
	@echo "build shared objects..."
	$(CPP) -fpic -shared $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)
#	$(CC) -fpic -shared $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)
else
	@echo "target type:$(TYPE) error!!!"
endif

$(OBJ_DIR)/%.o: %.cpp
	$(CPP) $(CFLAGS) -c -o $@ $<
$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR)/%.d: %.cpp
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\([^.]*\)\.o[ :]*,$(OBJ_DIR)/$*.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$
$(OBJ_DIR)/%.d: %.c
	@set -e; rm -f $@; \
	$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
	sed 's,\([^.]*\)\.o[ :]*,$(OBJ_DIR)/$*.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

# DO SOMETHING HERE BEFORE BUILD  
BEFOREBUILD:  
#	@echo "do preparations before build..."  
	@if [ ! -e $(OBJ_DIR) ];then mkdir -p $(OBJ_DIR);echo "mkdir $(OBJ_DIR)";fi  
	@for DIR in $(SUB_DIRS); do \
		cd $(CURDIR)/$$DIR; make install;cd -;\
	done 
# DO SOMETHING HERE AFTER BUILD  
AFTERBUILD: 
#	cp $(TARGET) /root/workspace/linphone_arm/install/fastGateway/bin
#	@echo "build finish, do something here" 
clean:
	@echo $(SUB_DIRS)
	@for DIR in $(SUB_DIRS); do \
		cd $(CURDIR)/$$DIR; make clean;cd -;\
        done 
	rm -f $(TARGET) $(OBJS)
#	rm -f $(TARGET) $(OBJS) $(DEPS)
	rm -rf $(OBJ_DIR) 

pack : all
	mkdir -p $(PACK_DIR)
	cp -f $(TARGET) $(PACK_DIR)
	chmod 777 CloudFS_Install.sh
	cp -f CloudFS_Install.sh $(PACK_DIR)
	cp -rf ./conf/ $(PACK_DIR)
	tar zcf cloudfs_`uname -r`.tar.gz $(PACK_DIR)/*
