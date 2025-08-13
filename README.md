

# Introduction
프로그래머스 데브코스 리눅스 커널 과정에서 진행한 토이 프로젝트입니다.  

프로젝트의 핵심은 커널의 내부 동작 원리에 대한 깊은 이해를 바탕으로, 커스텀 디바이스 드라이버를 통해 하드웨어(GPIO, I2C 센서)를 직접 제어하고 고도화된 멀티프로세스 아키텍처를 통해 시스템 로직, 데이터 흐름, 웹 기반 HMI(Human-Machine Interface)를 유기적으로 연동하는 실시간 모니터링 및 제어 시스템을 구축하는 데 있습니다.

---
# Key Features
- 실시간 센서 모니터링: BMP280 센서 드라이버와 sysfs, poll을 이용해 온도 데이터를 실시간으로 수집하고 웹 GUI에 표시합니다.
- 웹 기반 모터 제어: 웹 UI에서 입력한 값에 따라 가상 모터(LED)의 점멸 속도를 실시간으로 제어합니다.
- 물리 버튼 인터럽트: GPIO 인터럽트를 활용하여 물리 버튼을 누르면 모터 동작을 즉시 중단시키는 긴급 정지 기능을 구현합니다.
- 다중 프로세스/스레드 구조: fork와 pthread를 사용하여 시스템 서버, 입력 처리, 웹 서버 등 기능별 모듈을 독립적인 프로세스/스레드로 분리하여 안정적인 동시성을 확보합니다.
- 커널-유저스페이스 통신: ioctl, sysfs, 공유 메모리, 메시지 큐 등 다양한 IPC 기법을 목적에 맞게 활용하여 커널과 사용자 공간 간의 데이터를 효율적으로 교환합니다.
- 상태 머신 디자인 패턴: 복잡한 LED 제어 로직(짧은 클릭, 긴 클릭, 더블 클릭 등)을 상태 머신 드라이버(state_machine.ko)로 구현하여 코드의 확장성과 가독성을 높입니다.

---
# Directory Structure
.
├── /bin/               # toy_system, toy-be 등 최종 실행 파일
├── /drivers/           # 디바이스 드라이버 소스 코드 (engine, sensor 등)
│   ├── engine/
│   ├── sensor/
│   ├── simple_io/
│   └── state_machine/
├── /kernel/            # 커널 관련 스크립트 및 수정 사항
├── /system/            # 시스템 애플리케이션 소스 코드
│   ├── engine/
│   ├── hal/
│   ├── system/
│   └── ui/
├── /toy-api/           # 웹 프론트엔드 API (Javascript)
├── /toy-be/            # 웹 백엔드 (Go)
├── /toy-fe/            # 웹 프론트엔드 (React + Typescript)
├── buildroot/          # 임베디드 리눅스 빌드 시스템
└── Makefile            # 프로젝트 전체 빌드를 위한 최상위 Makefile

---
# Environment
- 타겟 하드웨어: Raspberry Pi 4
- 호스트 OS: Ubuntu 22.04 or higher
- 임베디드 빌드 시스템: Buildroot (`raspberrypi_64_defconfig` 기반)
- 주요 사용 언어 및 기술:
  - System & Drivers: C, C++
  - Web Backend: Go
  - Web Frontend: React, Typescript

---
# System Architecture & Data Flow
## 제어 명령 흐름 (Control Command Flow)
- 사용자 입력: 웹 GUI 또는 CLI를 통해 모터 속도 값이 입력됩니다.
- 프로세스 통신: 해당 명령은 메시지 큐를 통해 System Server Process로 전달됩니다.
- 드라이버 호출: System Server는 새로운 속도 값을 담아 engine.ko 드라이버에 ioctl 시스템 콜을 호출합니다.
- 하드웨어 제어: engine.ko 드라이버는 hrtimer의 주기를 조절하여 GPIO에 연결된 LED의 점멸 속도를 변경함으로써, 새로운 모터 속도를 시각적으로 표현합니다.

## 센서 데이터 흐름 (Sensor Data Flow)
- 하드웨어 데이터 읽기: sensor.ko 드라이버의 hrtimer가 주기적으로 workqueue 태스크를 실행시켜 I2C 버스를 통해 BMP280 센서로부터 온도 데이터를 읽습니다.
- 커널 내부 데이터 업데이트: 드라이버는 읽어온 값을 sysfs 노드(/sys/toy/notify)를 통해 외부에 공개합니다.
- 유저스페이스에 변경 알림: 드라이버가 sysfs_notify()를 호출하면 poll()을 통해 대기 중이던 Input Process의 Sensor Thread가 깨어납니다(unblock).
- 데이터 전파: Sensor Thread는 sysfs에서 새로운 값을 읽어 공유 메모리 세그먼트에 기록하고, System Server에 메시지를 보내 데이터 업데이트를 알립니다.
- GUI 업데이트: System Server와 웹 백엔드는 공유 메모리의 최신 데이터를 조회하여 웹 GUI에 실시간으로 표시합니다.

---
# Technical Components
## 커널 레벨 요소 (Drivers)
- engine.ko (모터 제어 드라이버)
  - 8개의 LED로 가상 모터 2개를 시뮬레이션합니다.
  - 정밀하고 논블로킹(non-blocking) 방식의 제어를 위해 hrtimer를 사용합니다.
  - GPIO 인터럽트 핸들러(request_irq)를 등록하여 비상 정지 버튼을 구현, 하드웨어 주도의 선점형 이벤트 처리 방식을 보여줍니다.
- sensor.ko (BMP280 I2C 드라이버)
  - I2C 프로토콜을 통해 BMP280 센서와 통신합니다.
  - I/O 바운드 작업인 센서 읽기를 workqueue에 위임하여, 타이머 콜백과 같은 중요한 커널 컨텍스트가 블로킹되는 것을 방지하는 커널 프로그래밍의 핵심 원칙을 적용했습니다.
  - sysfs를 통해 센서 데이터를 외부에 공개하고 poll()을 통한 효율적인 변경 알림 방식을 사용하여 현대적인 드라이버 개발의 모범 사례를 따릅니다.
- state_machine.ko (입력 로직 드라이버)
  - 복잡한 버튼 입력 패턴을 해석하기 위해 유한 상태 머신(FSM)을 구현한 독립 드라이버입니다. 이는 최소한의 하드웨어로 정교한 사용자 인터페이스를 구축하는 견고한 방법을 보여줍니다.

## 유저스페이스 레벨 요소 (Application)
- 멀티프로세스 애플리케이션 (toy_system)
  - 감독 역할을 하는 부모 프로세스가 fork()를 통해 Input Process, System Server Process 등 전문화된 자식 프로세스를 생성하여 모듈성과 오류 격리(fault isolation)를 보장합니다.
  - fork() 실행 전에 모든 IPC 메커니즘(메시지 큐, 공유 메모리)을 초기화합니다.
- Input Process
  - Command Thread: 시스템 제어 및 디버깅을 위한 CLI를 제공합니다.
  - Sensor Thread: poll()을 사용하여 커널의 알림을 효율적으로 대기하고 센서 데이터가 공유 메모리로 흐르는 과정을 총괄합니다.
- System Server Process
  - 애플리케이션의 중앙 제어 시스템 역할을 합니다.
  - 여러 프로세스로부터 오는 메시지를 수신 대기하고 Engine Thread를 통해 ioctl 명령을 보내는 등 적절한 스레드에 작업을 분배합니다.

---
# Usage & Demonstration

## 실행 방법
1. 프로젝트를 컴파일하고 scp를 사용하여 필요한 파일들을 라즈베리파이 4로 전송합니다.
2. insmod sensor.ko, insmod engine.ko 명령으로 필요한 드라이버를 커널에 로드합니다.
3. ./toy_system을 실행하여 전체 시스템을 구동합니다.
4. 실시간 센서 값과 모터 속도 변화는 웹 GUI를 통해 확인할 수 있으며, 터미널 로그에는 메시지 큐로 수신된 센서 데이터와 GUI의 메시지가 표시됩니다.

### Execute Example:
- **로그 출력**: 오른쪽 터미널에 메시지 큐로 수신된 센서 값 및 GUI에서 받은 메시지가 출력됩니다.  

- **BMP280 센서**: 실시간으로 읽은 온도 데이터가 웹 GUI에 표시됩니다.  

- **모터 속도 조절**: GUI에서 입력한 모터 속도가 로그에 표시되며, LED로 구현된 모터 속도가 변경됩니다.  

---
# Installation & Development Setup

### **Buildroot**:
```sh
git clone https://github.com/raspberrypi/buildroot
cd buildroot
git reset 39a2ff16f92a61a3e8593c394d211c18d01de8d4 –hard
make raspberrypi4_64_defconfig
make linux-menuconfig -> kernel hacking -> compile-time checks and compiler options -> compile the kernel with debug info (o)
make menuconfig -> Target options -> Filesystem images -> cpio the root filesystem (o)
make
```

### **Makefile**:
프로젝트를 다시 빌드하려면 빌드루트의 커널 컴파일 이후 프로젝트에 존재하는 makefile의 내용을 아래처럼 buildroot에 대한 경로를 바꿔줘야합니다.

```Makefile
# Modify the top-level Makefile
BUILDROOT_DIR = /path/to/buildroot   # buildroot 경로 설정
TOOLCHAIN_DIR = $(BUILDROOT_DIR)/output/host/bin   # 툴체인 경로 설정

### **Host (PC)**:
```sh
make toy-fe
make toy-be
make

```

### **Target (Raspberry Pi 4)**:
```sh
# 빌드된 파일들을 라즈베리파이로 전송
scp bin/toy_system toy-be/toy-be drivers/engine/engine.ko drivers/sensor/sensor.ko root@<your-rpi4-ip>:/root

# I2C 커널 모듈 로드
modprobe i2c-bcm2835
modprobe i2c-dev

# 프로젝트 드라이버 로드
insmod sensor.ko
insmod engine.ko

# 메인 시스템 실행
./toy_system
```

