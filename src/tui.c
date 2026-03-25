#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../include/tui.h"

/* 
 * init_tui: ncurses 라이브러리를 초기화하고 창(Window)을 생성합니다. 
 */
void init_tui(tui_state *state) {
    initscr();      // ncurses 모드 시작
    start_color();  // 색상 사용 설정
    cbreak();       // 입력 버퍼링 비활성화 (엔터 없이 즉시 입력)
    noecho();       // 입력한 키를 화면에 표시하지 않음
    keypad(stdscr, TRUE); // 방향키 등 특수키 사용 설정
    curs_set(0);    // 커서를 화면에서 숨김
    timeout(100);   // getch()가 0.1초 동안 입력이 없으면 ERR 반환 (비차단 입력)

    // 색상 쌍 정의 (글자색, 배경색)
    init_pair(1, COLOR_WHITE, COLOR_BLUE);  // 패널 선택됨
    init_pair(2, COLOR_WHITE, COLOR_BLACK); // 패널 일반
    init_pair(3, COLOR_BLACK, COLOR_CYAN);  // 목록에서 선택된 항목

    int h, w;
    getmaxyx(stdscr, h, w); // 현재 터미널의 높이와 너비를 가져옴

    // 각 패널을 위한 독립적인 윈도우 생성 (높이, 너비, 시작Y, 시작X)
    state->local_win = newwin(h - 3, w / 2, 0, 0);
    state->remote_win = newwin(h - 3, w / 2, 0, w / 2);
    state->status_win = newwin(3, w, h - 3, 0);
    
    state->current_panel = LOCAL_PANEL;
    state->local_sel = 0;
    state->remote_sel = 0;

    // 창 테두리 그리기
    box(state->local_win, 0, 0);
    box(state->remote_win, 0, 0);
    box(state->status_win, 0, 0);

    // 변경 사항을 실제 화면에 반영
    wrefresh(state->local_win);
    wrefresh(state->remote_win);
    wrefresh(state->status_win);
}

void close_tui() {
    endwin(); // ncurses 모드 종료 및 터미널 복구
}

/* 
 * draw_files: 특정 창에 파일 목록을 그립니다. 
 */
static void draw_files(WINDOW *win, file_info *files, int count, int selected, int has_focus) {
    werase(win); // 창 내용을 지움
    if (has_focus) wbkgd(win, COLOR_PAIR(1)); // 창이 포커스를 가졌을 때 배경색 변경
    else wbkgd(win, COLOR_PAIR(2));
    
    box(win, 0, 0); // 테두리 다시 그리기
    int h, w;
    getmaxyx(win, h, w);

    if (files != NULL) {
        for (int i = 0; i < count && i < h - 2; i++) {
            // 현재 선택된 행이면 강조 색상 적용
            if (i == selected && has_focus) wattron(win, COLOR_PAIR(3));
            
            // 파일명 출력 ('D'는 디렉토리 표시)
            mvwprintw(win, i + 1, 1, "%c %-.*s", files[i].is_dir ? 'D' : ' ', w - 5, files[i].name);
            
            if (i == selected && has_focus) wattroff(win, COLOR_PAIR(3));
        }
    }
    wrefresh(win);
}

/* 
 * update_display: 전체 화면을 다시 그립니다. 
 */
void update_display(tui_state *state, const char *msg, file_info *local_files, int local_count, file_info *remote_files, int remote_count) {
    // 로컬 패널 그리기
    draw_files(state->local_win, local_files, local_count, state->local_sel, state->current_panel == LOCAL_PANEL);
    // 원격 패널 그리기
    draw_files(state->remote_win, remote_files, remote_count, state->remote_sel, state->current_panel == REMOTE_PANEL);

    // 하단 상태바 그리기
    werase(state->status_win);
    box(state->status_win, 0, 0);
    
    // 현재 실행 중인 전송 작업이 있는지 확인
    sftp_task *active = NULL;
    pthread_mutex_lock(&state->client->queue_mutex);
    if (state->client->current_task_idx < state->client->task_count) {
        active = &state->client->tasks[state->client->current_task_idx];
    }
    int pending = state->client->task_count - state->client->current_task_idx;
    pthread_mutex_unlock(&state->client->queue_mutex);

    // 작업 중이면 프로그레스바 표시
    if (active && active->status == 1) {
        int bar_width = 30;
        int filled = (int)(active->progress / 100.0 * bar_width);
        mvwprintw(state->status_win, 1, 2, "Task [%d/%d]: [", state->client->current_task_idx + 1, state->client->task_count);
        for (int i = 0; i < bar_width; i++) {
            waddch(state->status_win, (i < filled) ? '#' : '-');
        }
        wprintw(state->status_win, "] %.1f%%", active->progress);
    } else {
        // 작업이 없으면 일반 상태 메시지 표시
        mvwprintw(state->status_win, 1, 2, "Status: %s (Pending: %d)", msg, pending > 0 ? pending : 0);
    }
    
    // 현재 경로 정보 표시
    mvwprintw(state->status_win, 1, 60, "L: %s", state->local_path);
    mvwprintw(state->status_win, 2, 60, "R: %s", state->remote_path);
    
    wrefresh(state->status_win);
}

/* 
 * handle_input: 키보드 입력을 받아 그에 맞는 동작을 수행합니다. 
 */
int handle_input(tui_state *state, sftp_client *client, sftp_config *cfg, 
                 file_info **local_files, int *local_count, 
                 file_info **remote_files, int *remote_count) {
    int ch = getch(); // 키 입력 받기 (timeout 설정으로 인해 0.1초 후 ERR 반환 가능)
    if (ch == ERR) return 1; // 입력이 없으면 그냥 리턴 (화면 갱신을 위해)
    
    char src[2048], dest[2048];

    switch (ch) {
        case '\t': // TAB: 패널 간 포커스 이동
            state->current_panel = (state->current_panel == LOCAL_PANEL) ? REMOTE_PANEL : LOCAL_PANEL;
            break;
        case KEY_UP: // 위 방향키
            if (state->current_panel == LOCAL_PANEL && state->local_sel > 0) state->local_sel--;
            else if (state->current_panel == REMOTE_PANEL && state->remote_sel > 0) state->remote_sel--;
            break;
        case KEY_DOWN: // 아래 방향키
            if (state->current_panel == LOCAL_PANEL && state->local_sel < *local_count - 1) state->local_sel++;
            else if (state->current_panel == REMOTE_PANEL && state->remote_sel < *remote_count - 1) state->remote_sel--;
            break;
        case '\n': // ENTER: 디렉토리 이동
            if (state->current_panel == LOCAL_PANEL && *local_count > 0) {
                if ((*local_files)[state->local_sel].is_dir) {
                    if (strcmp((*local_files)[state->local_sel].name, "..") == 0) {
                        snprintf(src, sizeof(src), "%s/..", state->local_path);
                    } else {
                        snprintf(src, sizeof(src), "%s/%s", state->local_path, (*local_files)[state->local_sel].name);
                    }
                    strncpy(state->local_path, src, sizeof(state->local_path));
                    free(*local_files); // 기존 목록 메모리 해제
                    get_local_files(state->local_path, local_files, local_count); // 새 목록 로드
                    state->local_sel = 0;
                }
            } else if (state->current_panel == REMOTE_PANEL && *remote_count > 0 && client->connected) {
                if ((*remote_files)[state->remote_sel].is_dir) {
                    if (strcmp((*remote_files)[state->remote_sel].name, "..") == 0) {
                        snprintf(src, sizeof(src), "%s/..", state->remote_path);
                    } else {
                        snprintf(src, sizeof(src), "%s/%s", state->remote_path, (*remote_files)[state->remote_sel].name);
                    }
                    strncpy(state->remote_path, src, sizeof(state->remote_path));
                    free(*remote_files);
                    sftp_get_files(client, state->remote_path, remote_files, remote_count);
                    state->remote_sel = 0;
                }
            }
            break;
        case 'u':
        case 'U': // U: 업로드 (작업 큐에 추가)
            if (state->current_panel == LOCAL_PANEL && *local_count > 0 && client->connected) {
                snprintf(src, sizeof(src), "%s/%s", state->local_path, (*local_files)[state->local_sel].name);
                snprintf(dest, sizeof(dest), "%s/%s", state->remote_path, (*local_files)[state->local_sel].name);
                sftp_add_task(client, TASK_UPLOAD, src, dest);
            }
            break;
        case 'd':
        case 'D': // D: 다운로드 (작업 큐에 추가)
            if (state->current_panel == REMOTE_PANEL && *remote_count > 0 && client->connected) {
                snprintf(src, sizeof(src), "%s/%s", state->remote_path, (*remote_files)[state->remote_sel].name);
                snprintf(dest, sizeof(dest), "%s/%s", cfg->download_dir, (*remote_files)[state->remote_sel].name);
                sftp_add_task(client, TASK_DOWNLOAD, src, dest);
            }
            break;
        case 'q':
        case 'Q': // Q: 프로그램 종료
            return 0; 
    }
    return 1;
}
