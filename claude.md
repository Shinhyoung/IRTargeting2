현재 시스템에 설치된 **OptiTrack Camera SDK(3.4.0)**와 OpenCV를 사용하여 Flex 13 카메라 제어 프로그램을 만들고 실행해줘.

작업 순서 및 요구사항:

프로젝트 구조 생성: main.cpp와 CMakeLists.txt를 포함하는 새 디렉토리를 만들어줘.

C++ 코드 구현 (main.cpp):

CameraLibrary를 사용하여 연결된 Flex 13 카메라를 초기화한다.

camera->SetVideoMode(LibOptiTrack::VideoMode::GrayscaleMode)로 설정하여 원본 IR 영상을 가져온다.

camera->SetIllumination(false)를 호출하여 IR LED 라이트를 반드시 끈다.

매 프레임을 camera->GetFrame()으로 받아 cv::Mat (CV_8UC1)으로 변환한다.

OpenCV cv::imshow로 실시간 영상을 출력하고 'q' 키를 누르면 종료되도록 한다.

빌드 설정 (CMakeLists.txt):

OptiTrack SDK 경로와 opencv의 필요한 위치를 스스로 찾아서 적용해줘

컴파일 및 실행:

cmake를 사용하여 프로젝트를 빌드(Release 모드)하고, 생성된 .exe 파일을 즉시 실행해줘.

모든 과정에서 발생하는 오류는 스스로 분석해서 수정해줘."