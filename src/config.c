#include <stdio.h>
#include <string.h>
#include "../include/config.h"

/* 
 * load_config: 설정 파일을 한 줄씩 읽어 구조체에 채웁니다. 
 */
int load_config(sftp_config *cfg) {
    // fopen: 파일을 엽니다. "r"은 읽기 모드(read)입니다.
    FILE *f = fopen("config/settings.conf", "r");
    if (!f) return -1; // 파일이 없으면 에러 반환

    char line[512];
    // fgets: 파일에서 한 줄씩 읽어옵니다.
    while (fgets(line, sizeof(line), f)) {
        // sscanf: 읽어온 줄에서 특정 형식(KEY=VALUE)을 찾아 변수에 담습니다.
        if (sscanf(line, "HOST=%s", cfg->host) == 1) continue;
        if (sscanf(line, "USER=%s", cfg->username) == 1) continue;
        if (sscanf(line, "PASSWORD=%s", cfg->password) == 1) continue;
        if (sscanf(line, "DOWNLOAD_DIR=%s", cfg->download_dir) == 1) continue;
    }

    fclose(f); // 작업 완료 후 파일을 닫습니다. (메모리 누수 방지)
    return 0;
}

/* 
 * save_config: 구조체의 내용을 설정 파일에 씁니다. 
 */
int save_config(const sftp_config *cfg) {
    // "w"는 쓰기 모드(write)입니다. 기존 파일 내용을 덮어씁니다.
    FILE *f = fopen("config/settings.conf", "w");
    if (!f) return -1;

    // fprintf: 서식에 맞게 파일에 내용을 씁니다. (printf의 파일 버전)
    fprintf(f, "HOST=%s\n", cfg->host);
    fprintf(f, "USER=%s\n", cfg->username);
    fprintf(f, "PASSWORD=%s\n", cfg->password);
    fprintf(f, "DOWNLOAD_DIR=%s\n", cfg->download_dir);

    fclose(f);
    return 0;
}
