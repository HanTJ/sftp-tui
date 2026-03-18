#ifndef TUI_H
#define TUI_H

#include <ncurses.h> // 터미널 UI 라이브러리 (창 관리, 키 입력 등)
#include "sftp_client.h"

/* 
 * current_panel 열거형: 현재 선택된 패널을 나타냅니다. 
 */
typedef enum {
    LOCAL_PANEL,  // 로컬 파일 목록 창
    REMOTE_PANEL  // 원격 서버 파일 목록 창
} panel_type;

/* 
 * tui_state 구조체: TUI 화면 상태와 윈도우 정보를 담습니다. 
 */
typedef struct {
    WINDOW *local_win;      // 로컬 창 객체
    WINDOW *remote_win;     // 원격 창 객체
    WINDOW *status_win;     // 하단 상태바 창 객체
    int current_panel;      // 현재 어떤 창을 조작 중인지 (LOCAL/REMOTE)
    int local_sel;          // 로컬 목록 중 선택된 행 번호
    int remote_sel;         // 원격 목록 중 선택된 행 번호
    char local_path[1024];  // 현재 탐색 중인 로컬 경로
    char remote_path[1024]; // 현재 탐색 중인 원격 경로
    float progress;         // (사용 안 함 - sftp_task 내부로 이동됨)
    sftp_client *client;    // 작업 상태를 표시하기 위해 클라이언트에 접근
} tui_state;

/* TUI 제어 관련 함수 선언 */
void init_tui(tui_state *state);  // TUI 초기화 (화면 설정, 창 생성)
void close_tui();                 // TUI 종료 (ncurses 모드 해제)
void update_display(tui_state *state, const char *msg, file_info *local_files, int local_count, file_info *remote_files, int remote_count); // 화면 갱신
int handle_input(tui_state *state, sftp_client *client, sftp_config *cfg, 
                 file_info **local_files, int *local_count, 
                 file_info **remote_files, int *remote_count); // 키보드 입력 처리

#endif
