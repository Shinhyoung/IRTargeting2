# OptiTrack Flex 13 IR Tracking System

OptiTrack Flex 13 카메라를 사용한 실시간 적외선 영상 추적 및 호모그래피 변환 시스템

## 📋 목차

1. [프로젝트 개요](#프로젝트-개요)
2. [주요 기능](#주요-기능)
3. [시스템 요구사항](#시스템-요구사항)
4. [설치 가이드](#설치-가이드)
5. [빌드 방법](#빌드-방법)
6. [사용 방법](#사용-방법)
7. [문제 해결](#문제-해결)

---

## 프로젝트 개요

OptiTrack Flex 13 카메라의 IR 영상을 실시간으로 처리하고, 관심 영역을 선택하여 호모그래피 변환을 적용하는 프로그램입니다.

### 개발 환경
- **OS**: Windows 10/11
- **언어**: C++17
- **빌드 시스템**: CMake
- **컴파일러**: Visual Studio 2022 (MSVC)

---

## 주요 기능

### 영상 처리
- ✅ Grayscale IR 영상 실시간 캡처 (노출 200)
- ✅ Morphological Dilation (3회, 3x3 커널)
- ✅ Binary Threshold (임계값 200)
- ✅ 객체 중심점 자동 검출 및 좌표 표시

### 호모그래피 변환
- ✅ 마우스 클릭으로 관심 영역 선택 (4개 점)
- ✅ 1024x768 사각형 영상으로 변환
- ✅ 좌표 자동 변환 및 실시간 표시

### 카메라 설정
- ✅ IR LED 비활성화
- ✅ 노출 조절 가능
- ✅ 실시간 프레임 처리

---

## 시스템 요구사항

### 하드웨어
- **카메라**: OptiTrack Flex 13 (USB 연결)
- **메모리**: 최소 4GB RAM
- **저장 공간**: 최소 5GB 여유 공간

### 소프트웨어
| 구분 | 필수 소프트웨어 | 버전 | 설치 위치 |
|------|----------------|------|-----------|
| SDK | OptiTrack Camera SDK | 3.4.0 | `C:\Program Files (x86)\OptiTrack\CameraSDK` |
| 라이브러리 | OpenCV | 4.5.4 | `C:\opencv\opencv\build` |
| 빌드 도구 | CMake | 3.15 이상 | `C:\Program Files\CMake` |
| 컴파일러 | Visual Studio Build Tools | 2022 | 자동 설치 경로 |

---

## 설치 가이드

### 1. OptiTrack Camera SDK 설치

#### 다운로드
1. OptiTrack 공식 웹사이트 방문
2. Camera SDK 3.4.0 다운로드
   - 링크: https://optitrack.com/support/downloads/developer-tools.html

#### 설치
1. 다운로드한 설치 파일 실행
2. 설치 경로: **`C:\Program Files (x86)\OptiTrack\CameraSDK`** (기본값 유지 권장)
3. 설치 완료 후 다음 파일들이 있는지 확인:
   ```
   C:\Program Files (x86)\OptiTrack\CameraSDK\
   ├── include\
   │   └── cameralibrary.h
   ├── lib\
   │   ├── CameraLibrary2019x64S.lib
   │   └── CameraLibrary2019x64S.dll
   └── Samples\
   ```

---

### 2. OpenCV 설치

#### 다운로드
1. OpenCV 공식 GitHub Releases 페이지 방문
2. OpenCV 4.5.4 Windows 버전 다운로드
   - 링크: https://github.com/opencv/opencv/releases/tag/4.5.4
   - 파일: `opencv-4.5.4-vc14_vc15.exe`

#### 설치
1. 다운로드한 파일 실행 (자동 압축 해제)
2. 압축 해제 경로: **`C:\opencv`** (권장)
3. 최종 경로 확인:
   ```
   C:\opencv\opencv\build\
   ├── include\
   │   └── opencv2\
   ├── x64\
   │   └── vc15\
   │       ├── bin\
   │       │   └── opencv_world454.dll
   │       └── lib\
   │           └── opencv_world454.lib
   ```

#### 환경 변수 설정 (선택사항)
1. `시스템 환경 변수 편집` 열기
2. `Path` 변수에 추가: `C:\opencv\opencv\build\x64\vc15\bin`

---

### 3. CMake 설치

#### 다운로드
1. CMake 공식 웹사이트 방문
   - 링크: https://cmake.org/download/
2. Windows x64 Installer 다운로드 (최신 버전 또는 3.15 이상)

#### 설치
1. 설치 파일 실행
2. 설치 옵션:
   - ✅ **"Add CMake to the system PATH for all users"** 체크
3. 설치 경로: `C:\Program Files\CMake` (기본값)
4. 설치 완료 후 CMD에서 확인:
   ```cmd
   cmake --version
   ```

---

### 4. Visual Studio Build Tools 2022 설치

#### 다운로드
1. Visual Studio Build Tools 다운로드 페이지 방문
   - 링크: https://visualstudio.microsoft.com/downloads/
   - **"Build Tools for Visual Studio 2022"** 선택

#### 설치
1. 다운로드한 설치 파일 실행
2. **필수 워크로드** 선택:
   - ✅ **"C++를 사용한 데스크톱 개발"**
3. 개별 구성 요소 확인:
   - ✅ MSVC v143 - VS 2022 C++ x64/x86 빌드 도구
   - ✅ Windows 10 SDK (최신 버전)
   - ✅ CMake tools for Windows
4. 설치 (약 6GB 필요)

---

## 빌드 방법

### 1. 프로젝트 클론 또는 복사

```bash
# Git을 사용하는 경우
git clone <repository-url>
cd IRTargeting

# 또는 프로젝트 폴더를 원하는 위치에 복사
```

### 2. 경로 확인 및 수정

`CMakeLists.txt` 파일에서 설치 경로가 맞는지 확인:

```cmake
# OptiTrack Camera SDK paths
set(CAMERA_SDK_PATH "C:/Program Files (x86)/OptiTrack/CameraSDK")

# OpenCV paths
set(OPENCV_PATH "C:/opencv/opencv/build")
```

**중요**: 경로가 다르면 수정 필요!

### 3. 빌드 디렉토리 생성

```bash
mkdir build
cd build
```

### 4. CMake 설정

```bash
cmake .. -G "Visual Studio 17 2022" -A x64
```

**출력 예시**:
```
-- Selecting Windows SDK version 10.0.26100.0
-- The C compiler identification is MSVC 19.44.35222.0
-- The CXX compiler identification is MSVC 19.44.35222.0
-- Camera SDK: C:/Program Files (x86)/OptiTrack/CameraSDK
-- OpenCV Path: C:/opencv/opencv/build
-- Configuring done
-- Generating done
```

### 5. 빌드 (Release 모드)

```bash
cmake --build . --config Release
```

**출력 예시**:
```
Building Custom Rule
main.cpp
IRViewer.vcxproj -> C:\...\IRTargeting\build\Release\IRViewer.exe
Copying Camera SDK DLL
Copying OpenCV DLLs to output directory
```

### 6. 실행 파일 확인

```bash
cd Release
dir IRViewer.exe
```

빌드가 성공하면 다음 파일들이 생성됩니다:
```
build\Release\
├── IRViewer.exe
├── CameraLibrary2019x64S.dll
└── opencv_world454.dll
```

---

## 사용 방법

### 1. 프로그램 실행

#### 방법 1: 명령 프롬프트
```bash
cd build\Release
IRViewer.exe
```

#### 방법 2: VSCode (F5)
- `main.cpp` 파일 열기
- **F5** 키 누르기
- 또는 `Run and Debug` → `Run IRViewer (Release)` 선택

### 2. 화면 구성

프로그램 실행 시 **메인 창** (좌우 분할) 표시:
- **왼쪽**: Grayscale 원본 영상
- **오른쪽**: Dilate + 이진화 + 객체 추적 영상

### 3. 호모그래피 변환 사용법

#### Step 1: 관심 영역 선택
왼쪽 영상에서 마우스로 **4개 점 클릭** (순서 중요!):
1. **왼쪽 위** (Left Top)
2. **오른쪽 위** (Right Top)
3. **오른쪽 아래** (Right Bottom)
4. **왼쪽 아래** (Left Bottom)

#### Step 2: Warped View 확인
4개 점 선택 완료 시:
- **"Warped View (1024x768)"** 창 자동 생성
- 선택한 영역이 1024x768 사각형으로 변환
- 검출된 객체의 좌표도 함께 변환되어 표시

### 4. 단축키

| 키 | 기능 |
|----|------|
| **마우스 좌클릭** | 왼쪽 영상에서 점 선택 |
| **'r'** | 선택한 점 초기화 (다시 선택 가능) |
| **'q'** 또는 **ESC** | 프로그램 종료 |

### 5. 화면 표시 정보

#### 원본 영상 (왼쪽)
- 🔵 파란색 원: 선택한 점 (번호 표시)
- 💛 노란색 선: 선택한 점들을 연결

#### 이진화 영상 (오른쪽)
- 🔴 빨간색 원: 검출된 객체 중심
- 🟢 초록색 텍스트: 객체 좌표 (x, y)

#### Warped View 영상
- 🔴 빨간색 원: 변환된 객체 중심
- 🟢 초록색 텍스트: 변환된 좌표 (1024x768 좌표계)

---

## 문제 해결

### 1. 카메라가 인식되지 않음

**증상**: "No OptiTrack cameras found" 에러

**해결 방법**:
1. USB 케이블 연결 확인
2. 장치 관리자에서 카메라 확인:
   - `장치 관리자` → `이미징 장치` 또는 `범용 직렬 버스 컨트롤러`
   - "OptiTrack Flex 13" 또는 USB 장치 확인
3. 다른 프로그램 종료:
   - **Motive** 등 OptiTrack 소프트웨어 종료
   - 동시에 한 프로그램만 카메라 사용 가능

### 2. DLL 파일이 없다는 에러

**증상**: "opencv_world454.dll을 찾을 수 없습니다"

**해결 방법**:
1. OpenCV 설치 경로 확인
2. 환경 변수에 추가:
   ```
   C:\opencv\opencv\build\x64\vc15\bin
   ```
3. 또는 DLL 파일을 실행 파일과 같은 폴더에 복사

### 3. CMake 설정 오류

**증상**: "Could not find Camera SDK" 또는 "Could not find OpenCV"

**해결 방법**:
1. `CMakeLists.txt` 파일의 경로 확인 및 수정:
   ```cmake
   set(CAMERA_SDK_PATH "C:/Program Files (x86)/OptiTrack/CameraSDK")
   set(OPENCV_PATH "C:/opencv/opencv/build")
   ```
2. 경로 구분자는 `/` 사용 (Windows에서도)
3. 공백이 있는 경로는 따옴표로 감싸기

### 4. 빌드 오류 (MSVC 관련)

**증상**: "MSVC를 찾을 수 없습니다" 또는 컴파일러 오류

**해결 방법**:
1. Visual Studio Build Tools 2022 재설치
2. "C++를 사용한 데스크톱 개발" 워크로드 확인
3. CMake 재실행:
   ```bash
   cd build
   cmake .. -G "Visual Studio 17 2022" -A x64
   ```

### 5. Windows 스마트 앱 컨트롤 차단

**증상**: "스마트 앱 컨트롤이 이 앱의 일부를 차단했습니다"

**해결 방법**:
1. 경고 창에서 **"추가 정보"** 클릭
2. **"실행"** 버튼 클릭
3. 또는 Windows 보안 설정에서 제외 폴더 추가

### 6. 카메라 초기화 타임아웃

**증상**: "Camera initialization timeout"

**해결 방법**:
1. USB 케이블을 다른 포트에 연결
2. USB 3.0 포트 사용 권장
3. USB 허브 사용 시 직접 연결로 변경
4. 카메라 전원 재연결

---

## 프로젝트 구조

```
IRTargeting/
├── main.cpp              # 메인 소스 코드
├── CMakeLists.txt        # CMake 빌드 설정
├── README.md             # 이 문서
├── CLAUDE.md             # 프로젝트 요구사항
├── .gitignore            # Git 제외 파일 목록
├── .vscode/              # VSCode 설정
│   ├── launch.json       # 디버그 설정
│   ├── tasks.json        # 빌드 작업
│   ├── c_cpp_properties.json  # IntelliSense 설정
│   └── settings.json     # 프로젝트 설정
└── build/                # 빌드 디렉토리 (생성됨)
    └── Release/
        └── IRViewer.exe  # 실행 파일
```

---

## Git 버전 관리

현재 프로젝트는 Git으로 버전 관리됩니다.

### 커밋 히스토리 확인
```bash
git log --oneline
```

### 이전 버전으로 롤백
```bash
# 커밋 목록 확인
git log --oneline

# 특정 커밋으로 되돌리기
git checkout <commit-id> main.cpp

# 또는 완전히 되돌리기
git reset --hard <commit-id>
```

---

## 주요 설정 값

### 카메라 설정
- **Video Mode**: Grayscale
- **Exposure**: 200
- **IR Intensity**: 0 (비활성화)

### 영상 처리 설정
- **Dilation**: 3회 (3x3 사각형 커널)
- **Threshold**: 200
- **Warped Resolution**: 1024x768

### 성능 설정
- **Frame Wait**: 1ms
- **CPU Delay**: 10ms

---

## 기술 스택

### 라이브러리
- **OptiTrack Camera SDK 3.4.0**: 카메라 제어 및 프레임 획득
- **OpenCV 4.5.4**: 영상 처리 및 시각화
- **C++ Standard Library**: 기본 자료구조 및 유틸리티

### 주요 알고리즘
- **Morphological Dilation**: 노이즈 제거 및 객체 강조
- **Binary Thresholding**: 밝은 객체 분리
- **Contour Detection**: 객체 윤곽선 검출
- **Image Moments**: 중심 좌표 계산
- **Homography Transformation**: 원근 변환

---

## 라이선스

이 프로젝트는 교육 및 연구 목적으로 개발되었습니다.

---

## 참고 자료

### OptiTrack
- [OptiTrack 공식 웹사이트](https://optitrack.com/)
- [Camera SDK 다운로드](https://optitrack.com/support/downloads/developer-tools.html)
- [Camera SDK 문서](https://docs.optitrack.com/)

### OpenCV
- [OpenCV 공식 웹사이트](https://opencv.org/)
- [OpenCV 문서](https://docs.opencv.org/4.5.4/)
- [OpenCV GitHub](https://github.com/opencv/opencv)

### CMake
- [CMake 공식 웹사이트](https://cmake.org/)
- [CMake 문서](https://cmake.org/documentation/)

### Visual Studio
- [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/)
- [MSVC 문서](https://docs.microsoft.com/en-us/cpp/)

---

## 연락처 및 지원

문제가 발생하거나 질문이 있는 경우:
1. `IRViewer_log.txt` 파일 확인
2. GitHub Issues에 문제 보고
3. 로그 파일 첨부

---

**마지막 업데이트**: 2026-02-13
**버전**: 1.0.0
**개발 환경**: Windows 11, Visual Studio 2022, OptiTrack Flex 13
