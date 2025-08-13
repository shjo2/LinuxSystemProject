TARGET = toy_system

SYSTEM = ./system
UI = ./ui
WEB_SERVER = ./web_server
HAL = ./hal
ENGINE = ./engine

INCLUDES = -I$(SYSTEM) -I$(UI) -I$(WEB_SERVER) -I$(HAL) -I./

BUILDROOT_DIR=/home/sungho/kdt/buildroot/
TOOLCHAIN_PATH=${BUILDROOT_DIR}/output/host/bin
CC=${TOOLCHAIN_PATH}/aarch64-buildroot-linux-gnu-gcc
CXX=${TOOLCHAIN_PATH}/aarch64-buildroot-linux-gnu-g++

# 여기를 buildroot의 gcc를 사용하도록 수정하세요
#CC = gcc
# CXX = g++
CXXLIBS = -lpthread -lm -lrt -Wl,--no-as-needed -ldl -lseccomp
CXXFLAGS = $(INCLUDEDIRS) -g -O0 -std=c++14 -DTARGET

objects = main.o \
		system_server.o \
		web_server.o \
		input.o \
		gui.o \
		shared_memory.o \
		dump_state.o \
		hardware.o \
		Allocator.o \
		Engine.o \
		Fault.o \
		LeftMotor.o \
		RightMotor.o \
		StateMachine.o \
		stdafx.o \
		xallocator.o

# cxx_objects = camera_HAL.o ControlThread.o
cxx_objects =
shared_libs = libcamera.oem.so libcamera.toy.so

$(TARGET): $(objects) $(cxx_objects) $(shared_libs)
	$(CXX) -o $(TARGET) $(objects) $(cxx_objects) $(CXXLIBS)

main.o:  main.c
	$(CC) -g $(INCLUDES) -c main.c

system_server.o: $(SYSTEM)/system_server.h $(SYSTEM)/system_server.c
	$(CC) -g $(INCLUDES) -c ./system/system_server.c

dump_state.o: $(SYSTEM)/dump_state.h $(SYSTEM)/dump_state.c
	$(CC) -g $(INCLUDES) -c ./system/dump_state.c

shared_memory.o: $(SYSTEM)/shared_memory.h $(SYSTEM)/shared_memory.c
	$(CC) -g $(INCLUDES) -c ./system/shared_memory.c

gui.o: $(UI)/gui.h $(UI)/gui.c
	$(CC) -g $(INCLUDES) -c $(UI)/gui.c

input.o: $(UI)/input.h $(UI)/input.c
	$(CC) -g $(INCLUDES) -c $(UI)/input.c

web_server.o: $(WEB_SERVER)/web_server.h $(WEB_SERVER)/web_server.c
	$(CC) -g $(INCLUDES) -c $(WEB_SERVER)/web_server.c

hardware.o: $(HAL)/hardware.c
	$(CC) -g $(INCLUDES) -c  $(HAL)/hardware.c

Allocator.o: $(ENGINE)/Allocator.cpp
	$(CC) -g $(INCLUDES) -c  $(ENGINE)/Allocator.cpp

Engine.o: $(ENGINE)/Engine.cpp
	$(CC) -g $(INCLUDES) -c  $(ENGINE)/Engine.cpp

Fault.o: $(ENGINE)/Fault.cpp
	$(CC) -g $(INCLUDES) -c  $(ENGINE)/Fault.cpp

Motor.o: $(ENGINE)/Motor.cpp
	$(CC) -g $(INCLUDES) -c  $(ENGINE)/Motor.cpp

StateMachine.o: $(ENGINE)/StateMachine.cpp
	$(CC) -g $(INCLUDES) -c  $(ENGINE)/StateMachine.cpp

stdafx.o: $(ENGINE)/stdafx.cpp
	$(CC) -g $(INCLUDES) -c  $(ENGINE)/stdafx.cpp

xallocator.o: $(ENGINE)/xallocator.cpp
	$(CC) -g $(INCLUDES) -c  $(ENGINE)/xallocator.cpp

.PHONY: libcamera.oem.so
libcamera.oem.so:
	$(CC) -g -shared -fPIC -o libcamera.oem.so $(INCLUDES) $(CXXFLAGS) $(HAL)/oem/camera_HAL_oem.cpp $(HAL)/oem/ControlThread.cpp

.PHONY: libcamera.toy.so
libcamera.toy.so:
	$(CC) -g -shared -fPIC -o libcamera.toy.so $(INCLUDES) $(CXXFLAGS) $(HAL)/toy/camera_HAL_toy.cpp $(HAL)/toy/ControlThread.cpp

# libcamera.toy.so:
# 	$(CC) -g -shared -fPIC -o libfoo2.so foo2.c

# libfoo3.so:
# 	cc -g -Wl,-Bsymbolic -Wl,-allow-shlib-undefined \
# 		-shared -fPIC -o libfoo3.so foo3.

# camera_HAL.o: $(HAL)/camera_HAL.cpp
# 	$(CXX) -g $(INCLUDES) $(CXXFLAGS) -c  $(HAL)/camera_HAL.cpp

# ControlThread.o: $(HAL)/ControlThread.cpp
# 	$(CXX) -g $(INCLUDES) $(CXXFLAGS) -c  $(HAL)/ControlThread.cpp

.PHONY: clean
clean:
	rm -rf *.o
	rm -rf $(TARGET)
