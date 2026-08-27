IpcProc - Part.1 IPC  전체 자료
================================

[ 구성 ]

  IpcProc\              솔루션 전체 소스
    IpcProc.sln
    IpcCore\            IPC 기능 구현 (C, DLL)
      IpcCommon.h         공통 정의 / 로그 / 초기화
      IpcCore.c           로그 파이프라인
      IpcThread.h/.c      쓰레드 간 송수신   (전역 링버퍼 + 뮤텍스 + 세마포어)
      IpcMsgQ.h/.c        프로세스 간 송수신 (공유메모리 링버퍼 = 메시지 큐)
      IpcSocket.h/.c      TCP/IP 송수신      (SocketUtility.h 의 Windows 포팅)
    IpcUI\              동작 확인용 MFC 대화상자
    docs\               구현 내용 정리
    .gitignore / .gitattributes

  발표자료\
    IPC_발표자료.pptx   26장 (편집용)
    IPC_발표자료.pdf    같은 내용 PDF

  참고\
    SocketUtility.h     포팅 기준이 된 원본 리눅스 헤더


[ 빌드 ]

  Visual Studio 2022 에서 IpcProc\IpcProc.sln 열기
  구성 : Debug|x64  (x86 구성은 없음)
  솔루션 다시 빌드
  결과물 : IpcProc\build\x64\Debug\IpcUI.exe , IpcCore.dll


[ 동작 확인 ]

  [New Process] 는 같은 프로그램 창을 하나 더 띄우기만 한다.
  프로세스 간 확인(메시지 큐 / TCP)은 창 두 개에서 각각 버튼을 누르면 된다.

  Thread         [Start] -> 0.1초 간격으로 1~100 송/수신 로그
  Message Queue  [New Process] 로 창을 하나 더 띄운 뒤
                 한쪽 [Recv], 다른쪽 [Send]  (큐 이름만 같으면 된다)
  TCP/IP         [New Process] 로 창을 하나 더 띄운 뒤
                 한쪽 [Recv](서버), 다른쪽 [Send](클라이언트)

  다른 PC 와 붙일 때는 수신측 IP 를 0.0.0.0 으로 두고 방화벽 포트를 연다.
  상대가 htonl 을 쓰면 htonl 체크박스를 켠다.


[ 기존 프로젝트에 덮어쓸 때 ]

  IpcProc\ 폴더의 내용을 솔루션 루트에 덮어쓰면 된다.
  단, 아래 두 파일은 덮어쓰기로는 지워지지 않으니 직접 삭제할 것.
  (IpcUI.idl 로부터 MIDL 이 $(IntDir) 에 재생성하는 산출물이다)

    IpcUI\IpcUI_h.h
    IpcUI\IpcUI_i.c

  소스는 UTF-8(BOM) 로 저장되어 있으니 붙여넣기 말고 파일을 그대로 교체할 것.
