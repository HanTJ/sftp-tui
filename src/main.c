#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/config.h"
#include "../include/sftp_client.h"
#include "../include/tui.h"

int main() {
    sftp_config cfg;     // 설정 정보를 담을 변수
    sftp_client client;   // SFTP 클라이언트 상태 객체
    tui_state tui;       // TUI 화면 상태 객체
    char status_msg[512]; // 하단에 표시할 상태 메시지 버퍼

    // 구조체 메모리를 0으로 초기화 (안전한 프로그래밍 습관)
    memset(&client, 0, sizeof(sftp_client));
    client.sock = -1;

    /* 1. 설정 로드 */
    if (load_config(&cfg) != 0) {
        // 설정 파일이 없으면 기본값으로 채우고 파일을 생성함
        strcpy(cfg.host, "127.0.0.1");
        strcpy(cfg.username, "user");
        strcpy(cfg.password, "password");
        strcpy(cfg.download_dir, "./downloads");
        save_config(&cfg);
    }

    /* 2. SSH 라이브러리 초기화 */
    if (libssh2_init(0)) {
        fprintf(stderr, "libssh2 initialization failed\n");
        return 1;
    }

    /* 3. TUI 화면 초기화 */
    init_tui(&tui);
    tui.client = &client; // TUI가 클라이언트 작업 상태를 볼 수 있게 연결
    strncpy(tui.local_path, ".", sizeof(tui.local_path));
    strncpy(tui.remote_path, ".", sizeof(tui.remote_path));
    tui.progress = 0;
    
    /* 4. 서버 접속 시도 */
    int conn_res = sftp_init_client(&client, &cfg);
    if (conn_res == 0) {
        snprintf(status_msg, sizeof(status_msg), "Connected to %s", cfg.host);
    } else {
        snprintf(status_msg, sizeof(status_msg), "Connection Failed (%d). Press Q to quit.", conn_res);
    }

    /* 5. 파일 목록 초기 로딩 */
    file_info *local_files = NULL, *remote_files = NULL;
    int local_count = 0, remote_count = 0;

    get_local_files(tui.local_path, &local_files, &local_count);
    if (client.connected) {
        sftp_get_files(&client, tui.remote_path, &remote_files, &remote_count);
    }

    // 첫 화면 출력
    update_display(&tui, status_msg, local_files, local_count, remote_files, remote_count);

    /* 6. 메인 이벤트 루프 */
    while (1) {
        // 사용자 입력을 기다리고 처리함 (비차단 모드이므로 0.1초마다 리턴됨)
        if (!handle_input(&tui, &client, &cfg, &local_files, &local_count, &remote_files, &remote_count)) {
            break; // Q를 누르면 루프 탈출
        }
        // 화면을 지속적으로 갱신 (프로그레스바 업데이트 등)
        update_display(&tui, status_msg, local_files, local_count, remote_files, remote_count);
    }

    /* 7. 리소스 정리 및 종료 */
    if (local_files) free(local_files);   // 동적 할당된 메모리 해제
    if (remote_files) free(remote_files);

    close_tui();           // TUI 종료
    sftp_close_client(&client); // SFTP 연결 해제
    libssh2_exit();        // SSH 라이브러리 종료

    return 0;
}
