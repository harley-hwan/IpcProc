# IPC 구현 정리

개발환경 : Windows / Visual Studio 2022 (v143) / x64
개발언어 : 코어는 C, 확인용 UI 만 MFC

## 구성

```
IpcProc.sln
├─ IpcCore (C DLL)  - IPC 기능 구현
│   ├─ IpcThread.c   쓰레드 간 송수신
│   ├─ IpcMsgQ.c     프로세스 간 송수신 (메시지 큐)
│   └─ IpcSocket.c   TCP/IP (SocketUtility.h/.c 포팅)
└─ IpcUI (MFC 대화상자) - 버튼으로 코어 함수를 호출하고 로그만 표시
```

로그는 코어의 워커 쓰레드에서 콜백으로 올라오고, UI 는 `PostMessage(WM_IPC_LOG)` 로
자기 쓰레드에 넘겨 리스트박스에 뿌린다. 코어 문자열은 UTF-8 이라 UI 에서 변환한다.

세 가지 모두 같은 시나리오를 쓴다. **1 로 시작하는 int 를 0.1 초 간격으로 100 까지 송신,
수신측은 받은 값을 출력.**

## 1. 쓰레드 간 (IpcThread.c)

전역 링버퍼(8슬롯)를 두고

- 뮤텍스(`CRITICAL_SECTION`) : 링버퍼 인덱스/데이터 상호배제
- 세마포어 2개 : 빈 슬롯 수, 찬 슬롯 수

송신 쓰레드는 `sem_wait(빈슬롯) → lock → write → unlock → sem_post(찬슬롯)`,
수신 쓰레드는 그 반대. 뮤텍스만 쓰면 "빌 때까지 대기"가 바쁜 대기가 되므로
개수를 세는 세마포어를 같이 쓴다.

Linux 대응 : `CRITICAL_SECTION` = `pthread_mutex_t`, `CreateSemaphore` = `sem_init`,
`_beginthreadex` = `pthread_create`.
(CRT 자원 때문에 `CreateThread` 대신 `_beginthreadex` 를 쓴다)

## 2. 프로세스 간 (IpcMsgQ.c)

Windows 에는 System V 메시지 큐가 없어서 직접 만들었다.

| Linux | Windows |
|---|---|
| `msgget(key, IPC_CREAT)` | `CreateFileMapping` (네임드 공유메모리) + 링버퍼 헤더 초기화 |
| `msgsnd()` | 세마포어(빈슬롯) 대기 → 뮤텍스 → 링버퍼 write → 세마포어(찬슬롯) 증가 |
| `msgrcv()` | 세마포어(찬슬롯) 대기 → 뮤텍스 → 링버퍼 read → 세마포어(빈슬롯) 증가 |
| `msgctl(IPC_RMID)` | 모든 핸들 CloseHandle |
| `mtype` | `ST_IpcMsg.iMsgType` |

커널 오브젝트 이름은 `Local\IpcProc.<큐이름>.map / .mtx / .sem.e / .sem.f`.
`Global\` 이 아니라 `Local\` 을 쓰므로 관리자 권한이 필요 없다.

`f_IpcMsgQCreate()` 는 없으면 만들고 있으면 참여하므로 송신/수신 어느 쪽이 먼저 떠도 된다.
링버퍼 헤더 초기화는 뮤텍스 안에서 한 번만 하고, 세마포어 초기 카운트가 최초 생성 시에만
반영되는 Windows 동작을 그대로 이용한다.

## 3. TCP/IP (IpcSocket.c)

첨부받은 Linux `SocketUtility.h/.c` v5.0 을 Winsock2 로 포팅했다.
함수 이름과 인자, 상수, enum 은 원본 그대로 두어 리눅스쪽 코드와 맞물리게 했다.
(원본 .c 는 없어서 헤더 선언만 보고 구현)

포팅하면서 걸린 것들

| 항목 | Linux | Windows |
|---|---|---|
| 소켓 핸들 | `int` | `SOCKET`(UINT_PTR). x64 에서 64bit 라 `iSockId` → `hSockId` |
| 송/수신 타임아웃 | `setsockopt(SO_RCVTIMEO, struct timeval)` | **DWORD 밀리초**. 원본대로 timeval 을 넘기면 엉뚱한 값이 된다 |
| 소켓 닫기 | `close()` | `closesocket()` |
| 초기화 | 없음 | `WSAStartup` / `WSACleanup` |
| 에러 | `errno` | `WSAGetLastError()` |
| `select()` 1번 인자 | `maxfd + 1` | 무시됨 |
| 마이크로초 대기 | `usleep()` | 없음. Sleep 또는 QPC 스핀 |

역할은 **Tx = 클라이언트(connect), Rx = 서버(bind/listen/accept)** 로 잡았다.
UDP 쪽이 Tx 는 목적지 주소, Rx 는 자기 bind 주소를 받으므로 TCP 도 같게 맞춘 것이다.
상대가 반대로 동작하면 Tx/Rx Init 함수를 바꿔 부르면 된다.

`accept` 는 200ms 단위 `select` 루프로, `connect` 는 논블로킹 + `select` 로 처리해서
정지 요청에 바로 반응하게 했다. 블로킹 `connect` 는 실패 확정까지 20초 이상 걸린다.

송/수신은 요청한 크기를 다 처리할 때까지 반복한다. 반환값은
`>0` 처리 바이트 수, `0` 타임아웃, `-1` 에러 또는 상대 종료.

`_Sync` 계열 핸드셰이크는 `SOCKET_SYNC_PASS_*` 상수만 보고 맞춘 것이다
(Rx 가 LISTEN 을 보내고 Tx 가 CONNECT 로 답한다). 다른 프로그램과 붙일 때는
`_Normal` 을 쓰면 되고, Sync 가 필요하면 그 두 함수만 고치면 된다.

데이터는 프레임 헤더 없이 int 4바이트를 그대로 보낸다. x64 Windows 와 x86-64 Linux 는
둘 다 little-endian 이라 값이 그대로 맞고, 필요하면 htonl 체크박스를 켠다.

## 사용법

빌드하면 `build\x64\Debug\` 에 IpcUI.exe 와 IpcCore.dll 이 같이 나온다.

[New Process] 는 같은 exe 를 인자 없이 한 번 더 실행해 창을 하나 더 띄우기만 한다.
프로세스 간 확인은 창 두 개에서 역할 버튼을 각각 누르는 방식이다.

- **Thread** : [Start] 누르면 한 프로세스 안에서 송/수신 로그가 0.1초 간격으로 찍힌다.
- **Message Queue** : [New Process] 로 창을 하나 더 띄우고, 한쪽은 [Recv], 다른쪽은 [Send].
  큐 이름만 같으면 되고 누가 먼저 눌러도 된다.
- **TCP/IP** : [New Process] 로 창을 하나 더 띄우고, 한쪽은 [Recv](서버), 다른쪽은 [Send](클라이언트).
  다른 PC 와 붙을 때는 수신측 IP 를 `0.0.0.0` 으로 두고 방화벽에서 포트를 연다.
