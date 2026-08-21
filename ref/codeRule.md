1. 선언
	규칙: 선언(declare) 접두사는 다음을 사용한다.
	세부규칙:
		- 구조체: ST_
		- 공용체: UN_

	규칙: 구조체의 변수명은 ST_뒤에 첫 단위는 대문자로 시작, 단어간 새로운 단어 시작 시 대문자로 하고 나머지는 소문자로 한다.
	세부규칙:
		- 구조체: ST_PlotInfo
		- 공용체: UN_PlotInfo

	규칙: 기본 데이터형 대신 다음을 사용한다.
	세부규칙:
		typedef unsigned char		UCHAR;
		typedef unsigned char		UINT8;
		typedef unsigned short		USHORT;
		typedef unsigned short		UINT16;
		typedef unsigned short int	USHORTINT;
		typedef unsigned int		UINT32;
		typedef unsigned long		ULONG;
		typedef unsigned long int	ULONGINT;
		typedef unsigned long long	UINT64;

		typedef char				CHAR;
		typedef char				INT8;
		typedef short				SHORT;
		typedef signed short		INT16;
		typedef signed short int	SSHORT_INT;
		typedef signed int			INT32;
		typedef signed long			SLONG;
		typedef signed long	int		SLONGINT;
		typedef signed long long	INT64;

		typedef float				FLOAT32;
		typedef double				FLOAT64;

		typedef void				VOID;

2. 변수 및 함수 이름
	규칙: 정의(define) 접두사는 다음을 사용한다.
	세부규칙:
		- 프로젝트 내 전역변수: g_
		- 정적 전역변수: s_
		- 정적포인터변수: p_
		- 함수명: f_
		- 카운터: nXXX
		- 구조체: st_
	예:
		- 프로젝트 내 전역변수: g_comRdp
		- 정적 전역변수: s_comRdp
		- 전역포인터변수: p_comRdp
		- 카운터: nPlotNum
		- 구조체: st_comRdp

	규칙: 함수명은 단어 단위로 대소문자를 섞어 사용한다.
	세부규칙: f_ 이후 단어 시작은 대문자로 시작하고, 나머지는 소문자로 한다.
	예: void f_PulseCompressiion(Void);

	규칙: 구조체는 st_ 이후에 단어 단위로 대소문자를 섞어 사용한다.
	세부규칙: st_ 이후 단어 시작은 대문자로 시작하고, 나머지는 소문자로 한다.
	예: ST_PulseStruct st_PulseCompression;

	규칙: 지역변수는 소문자로 시작한다.
	예: INT32 pulseWidth

	규칙: Define 정의는 대문자로만 이루어지며 _로 구분한다.
	예: #define EFRM_ANTENNA_SIM		2

3. 함수 인자
	규칙: 함수 인자가 없는 경우 Void 형을 명시적으로 선언한다.
	예: Void PulseCompression (Void);

	규칙: 함수 인자가 변경되지 않는 경우는 항상 상수형으로 정의한다.
	예: void GenPlotinfo(ST_PlotInfo *st_PlotInfo, const ST_TargetInfo *st_Target);

4. 구조체
	규칙: 구조체는 typedef를 사용하여 선언한다.
	예:
		typedef
		struct
		{
			INT msg_id;
		} ST_MSG_ANT;

5. 파일 관리
	규칙: 프로젝트 디렉토리 구성은 다음과 같다.
	세부규칙: 프로젝트명_CSCI - CSC 명 - 하위디렉토리
	예:
		RPG_CSCI -> RDP -> include
				--> include
				--> Common

	규칙: 외부 ICD는 CSCI당 1개만 존재하며, Xxx(사업축약어)ExternalICD.h 파일에서 관리한다.
			Ex) EfrmExternalICD.h 파일은 프로젝트_CSCI->include 에 위치한다.
	세부규칙: 외부 ICD는 CSC, CSCI간 통신에 사용되는 ICD이다.
			EfrmExternalICD.h 파일은 프로젝트_CSI->include에 위치한다.

	규칙: 내부 ICD는 CSC당 1개만 존재하며, Xxx(사업축약어)InternalICD.h 파일에서 관리한다.
			Ex) EfrmExternalICD.h
	세부규칙: 내부 ICD는 CSC 이하 단위에서 통신에서 사용되는 ICD이다.
			XxxInternalICD.h 파일은 프로젝트_CSCI->CSC명->include에 위치한다.

6. 주석 규칙
	SLM와 장비간 코드가 복사되는 과정에서 한글이 깨지게 됩니다. 한글을 사용하려면 Settings 시 UTF-8로 설정하고 해야 한글이 안깨지니 꼭 설정 하십시오.

	규칙: 모든 C 파일은 앞부분에 다음과 같은 내용을 포함한다.
	세부규칙:
		//
		// @file	file.c
		// @brief	설명
		// @author	작성자 이름
		// @date	작성일
		//
	예:
		//
		// @file	RSPLib.c
		// @brief	Radar Signal Processign Library
		//			라이브러리 독립적으로 구성하기 위하여
		//			레이더 신호처리에 쓰이는 기본 연산을 추상화 한 계층임
		// @author	S. G. Riewe
		// @date	20xx.xx.xx.
		//

	규칙: 모든 함수는 앞부분에 다음과 같은 내용을 포함한다.
	세부규칙:
		//
		// @brief	함수설명
		// @param 파라미터명	설명
		// @todo	tode 설명
		// @return	없음
		// @author	제작자
		//
	예:
		// 
		// @author	S. G. Riew
		// @brief	Square Law Detector: 입력 신호에 대하여 mag^2를 계산함
		// @param magut	출력크기벡터(mag^2)
		// @param cpxIn	입력복소벡터
		// @param len	벡터길이
		// @todo		함수 속도 개선
		// @return		없음
		//
		void SquareLawDetector(FLOAT_32 *magOut, COMPLEX *cpxIn, INT32 len)

	규칙: 구조체는 다음과 같은 내용을 포함한다.
	세부규칙:
		//
		// @struct 구조체명
		// @brief	설명
		//
	예:
		//
		// @struct ST_rspData
		// @brief RSP를 위한 구조체
		//

		struct ST_rspData
		{
			int data1;	// @var data
			int data2;	// @var data
		}

	규칙: 매크로는 다음과 같은 내용을 포함한다.
	세부규칙:	
		// @def 매크로명
		// @brief 설명
	예:
		// @def MAXSIZE
		// @brief 최대 크기
		#define MAXSIZE 100


첨부:
	Item				설명
	@author				작성자 이름을 나타낼 때
	@brief				간략한 설명을 씀
	@code				중요 코드를 설명할 때 시작 지점 설정
	@date				작성 날짜를 나타낼 때
	@endcode			중요	코드 설명할 때 종료 지점 설정
	@exception			예외 처리
	@file				파일 이름을 구별
	@fn					함수를 나타낼 때
	@param				함수 파라미터 표시
	@remark				자세한 설명을 할 때
	@return				함수의 리턴 값을 나타낼 때
	@see				참고할 함수나 페이지 지정
	@struct				구조체 정의
	@bug				버그
	@mainpage			주페이지
	@section			섹션
	@tode				todo
