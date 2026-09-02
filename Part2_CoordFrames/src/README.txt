Part2_CoordFrames/src 구성

  CoordCore/     좌표변환 정적 라이브러리 (C). coord_frames.c/h + 참고 소스 matrixCalcLib.c/h
  radar_coord/   콘솔. 과제 3.1 입력값 넣고 단계별 값, 왕복오차, 최종 출력 두 가지를 찍음
  CoordUI/       MFC 다이얼로그. 입력값이나 roll/pitch/yaw 슬라이더를 바꾸면 바로 다시 계산해서 단계별 결과를 표로 봄

  실행 프로젝트 둘은 CoordCore 를 ProjectReference 로 물고 있음.
  빌드 결과는 build\x64\Debug|Release 에 모임 (Part1 과 같은 배치).
