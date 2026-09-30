# kopo incheon PLC
> 사거리 신호등 제어

## 실행 파일 위치 
[![](IMG\plc_download_banner.png)](https://github.com/CutTheWire/kopo-incheon--PLC/releases)

## 주의 사항
- 해당 리포지토리는 .xpd와 .xgwx를 저장하는 저장소로, Git Branch 개발 방식에 의거한 git push를 금지한다.
- main Branch만을 사용하며, 단일 작업자 원칙 방식으로 운용되어야 한다. 
## 개발 환경

### Software
- PLC : XG-5000
  - Ver : V4.81_2025_11_13_Kr
  - [다운로드 링크](https://sftpssqblobcdn.blob.core.windows.net/prod/largefile/document/17630088191370/XG5000_V4.81_2025_11_13_Kr.exe)
- HMI : XP-Builder
  - Ver : V3.90.1817
  - [다운로드 링크](https://sftpssqblobcdn.blob.core.windows.net/prod/largefile/document/17709456440910/XP-Builder_V3.90.1817.exe)

### Hardware
- PLC : [XBC-DR32H](https://www.ls-electric.com/ko/product/view/P01118)
- HMI
  - 시리즈 : IPC Series(wXP)
  - 모델 : PC/AT
  - 해상도 : 1600x900

## HMI
### B-1 화면
![B-1](./IMG/portfolio/패널.png)

### B-2 화면
![B-2](./IMG/portfolio/탑뷰.png)

### 점멸등 HMI
![점멸등](./IMG/portfolio/점멸등.gif)
- 1번 화면 : B-2 화면 (탑뷰)
- 2번 화면 : B-1 화면 (패널)

### 신호등 HMI
![신호등](./IMG/portfolio/신호등.gif)
- 1번 화면 : B-2 화면 (탑뷰)
- 2번 화면 : B-1 화면 (패널)

## 1. 디지털 입력 명세서 (Digital Input: 16점)

* **PLC 주소**: P00000 ~ P0000F
* **신호 방식**: DC 24V Sink/Source 타입 (포토커플러 절연)

| No. | PLC P주소 | 태그명 (Symbol) | 연결 장치 / 센서 | 접점 방식 | 비고 |
| :---: | :---: | :---: | :--- | :---: | :--- |
| 1 | P00000 | - | - | - | - |
| 2 | P00001 | PB1 | 신호등 운용 | NO (A접점) | ESP 4ch릴레이 K1 NO (A접점) |
| 3 | P00002 | - | - | - | - |
| 4 | P00003 | PB2 | 정지 버튼 클릭 시 메모리 초기화 | NC (B접점) & NO (A접점) | ESP 4ch릴레이 K2 NO (A접점) |
| 5 | P00004 | - | - | - | - |
| 6 | P00005 | - | - | - | - |
| 7 | P00006 | - | - | - | - |
| 8 | P00007 | - | - | - | - |
| 9 | P00008 | - | - | - | - |
| 10 | P00009 | - | - | - | - |
| 11 | P0000A | - | - | - | - |
| 12 | P0000B | - | - | - | - |
| 13 | P0000C | - | - | - | - |
| 14 | P0000D | - | - | - | - |
| 15 | P0000E | - | - | - | - |
| 16 | P0000F | - | - | - | - |

---

## 2. 디지털 출력 명세서 (Digital Output: 16점)

* **PLC 주소**: P00020 ~ P0002F
* **신호 방식**: 릴레이 출력 (접점 정격: AC 250V)
* **COM 그룹**: COM0 ~ COM3 (AC 220V 부하)

| No. | PLC P주소 | 태그명 (Symbol) | 구동 대상 (Actuator) | 전원/공통 | 비고 |
| :---: | :---: | :---: | :--- | :---: | :---: |
| 1 | P00020 | RL_C1 | TB1 신호등 적색 램프 | AC 220V (COM0) | - |
| 2 | P00021 | GL_C1 | TB1 신호등 녹색 램프 | AC 220V (COM0) | - |
| 3 | P00022 | YL_C1 | TB1 신호등 황색 램프 | AC 220V (COM0) | - |
| 4 | P00023 | RL_C2 | TB2 신호등 적색 램프 | AC 220V (COM0) | - |
| 5 | P00024 | GL_C2 | TB2 신호등 녹색 램프 | AC 220V (COM1) | - |
| 6 | P00025 | YL_C2 | TB2 신호등 황색 램프 | AC 220V (COM1) | - |
| 7 | P00026 | RL_C3 | TB3 신호등 적색 램프 | AC 220V (COM1) | - |
| 8 | P00027 | GL_C3 | TB3 신호등 녹색 램프 | AC 220V (COM1) | - |
| 9 | P00028 | YL_C3 | TB3 신호등 황색 램프 | AC 220V (COM2) | - |
| 10 | P00029 | RL_C4 | TB4 신호등 적색 램프 | AC 220V (COM2) | - |
| 11 | P0002A | GL_C4 | TB4 신호등 녹색 램프 | AC 220V (COM2) | - |
| 12 | P0002B | YL_C4 | TB4 신호등 황색 램프 | AC 220V (COM2) | - |
| 13 | P0002C | - | - | - | - |
| 14 | P0002D | - | - | - | - |
| 15 | P0002E | - | - | - | - |
| 16 | P0002F | - | - | - | - |