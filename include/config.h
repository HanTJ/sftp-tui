#ifndef CONFIG_H
#define CONFIG_H

/* 
 * MAX_STR_LEN: 문자열의 최대 길이를 정의합니다. 
 * 배열의 크기를 고정하여 메모리 관리를 단순화합니다. 
 */
#define MAX_STR_LEN 256

/* 
 * sftp_config 구조체: 서버 접속 정보를 저장합니다. 
 * 각 필드는 문자열 배열(char array)로 구성됩니다. 
 */
typedef struct {
    char host[MAX_STR_LEN];         // 서버 주소 (IP 또는 도메인)
    char username[MAX_STR_LEN];     // 접속 아이디
    char password[MAX_STR_LEN];     // 접속 비밀번호
    char download_dir[MAX_STR_LEN]; // 파일이 저장될 로컬 경로
} sftp_config;

/* 설정 파일을 읽고 쓰는 함수들의 원형(Prototype)입니다. */
int load_config(sftp_config *cfg);       // 파일에서 설정을 불러옴
int save_config(const sftp_config *cfg); // 파일에 설정을 저장함

#endif
