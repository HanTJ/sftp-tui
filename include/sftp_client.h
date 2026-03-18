#ifndef SFTP_CLIENT_H
#define SFTP_CLIENT_H

/* libssh2 라이브러리: SSH 및 SFTP 프로토콜을 구현한 C 라이브러리 */
#ifdef DUMMY_MODE
typedef void* LIBSSH2_SESSION;
typedef void* LIBSSH2_SFTP;
#else
#include <libssh2.h>
#include <libssh2_sftp.h>
#endif

#include <sys/socket.h>  // 네트워크 소켓 관련 헤더
#include <netinet/in.h> // 인터넷 주소 체계 관련 헤더
#include <arpa/inet.h>  // IP 주소 변환(문자열 <-> 숫자) 관련 헤더
#include <unistd.h>     // 리눅스 시스템 호출 관련 헤더
#include <stdio.h>      // 표준 입출력 (printf 등)
#include <pthread.h>    // 멀티쓰레드(동시 작업)를 위한 헤더
#include "config.h"     // 앞에서 정의한 설정 구조체 포함

/* 
 * file_info 구조체: 파일 목록 정보를 저장합니다. 
 */
typedef struct {
    char name[MAX_STR_LEN]; // 파일명 또는 디렉토리명
    int is_dir;             // 1이면 디렉토리, 0이면 파일
    long long size;         // 파일 크기 (바이트 단위)
} file_info;

/* 
 * task_type 열거형: 전송 작업의 종류를 정의합니다. 
 */
typedef enum {
    TASK_UPLOAD,   // 로컬에서 서버로 파일 전송
    TASK_DOWNLOAD  // 서버에서 로컬로 파일 전송
} task_type;

/* 
 * sftp_task 구조체: 하나의 전송 작업(Job)을 정의합니다. 
 */
typedef struct {
    task_type type;         // 업로드 또는 다운로드 구분
    char src[1024];         // 원본 파일 경로
    char dest[1024];        // 목적지 파일 경로
    float progress;         // 전송률 (0.0 ~ 100.0)
    int status;             // 0:대기중, 1:작업중, 2:완료, 3:실패
} sftp_task;

#define MAX_TASKS 100       // 전송 가능한 최대 작업 개수

/* 
 * sftp_client 구조체: SFTP 접속 상태와 작업 큐를 관리합니다. 
 */
typedef struct {
    int sock;                       // 네트워크 연결 소켓 번호
    LIBSSH2_SESSION *session;       // SSH 세션 핸들러
    LIBSSH2_SFTP *sftp_session;     // SFTP 세션 핸들러
    int connected;                  // 서버 연결 여부 (1:연결됨, 0:미연결)
    pthread_mutex_t sftp_mutex;     // SFTP 세션 보호용 뮤텍스(한 명만 사용하게 함)
    
    // 작업 큐 (Queue)
    sftp_task tasks[MAX_TASKS];     // 작업들의 목록 (배열 기반 큐)
    int task_count;                 // 현재 큐에 담긴 전체 작업 개수
    int current_task_idx;           // 지금 처리중인 작업의 인덱스 번호
    pthread_t worker_thread;        // 전송을 전담할 백그라운드 쓰레드 (일꾼)
    pthread_mutex_t queue_mutex;    // 작업 큐 보호용 뮤텍스 (동시 접근 방지)
    int stop_worker;                // 1이면 일꾼 쓰레드 종료
} sftp_client;

/* 함수 선언 (Function Prototypes) */
int sftp_init_client(sftp_client *client, const sftp_config *cfg); // 클라이언트 초기화 및 접속
void sftp_close_client(sftp_client *client);                      // 연결 해제 및 리소스 정리
int sftp_get_files(sftp_client *client, const char *path, file_info **files, int *count); // 서버 파일 목록 가져오기
int get_local_files(const char *path, file_info **files, int *count);                    // 로컬 파일 목록 가져오기

/* 작업 큐 관리 함수들 */
void sftp_add_task(sftp_client *client, task_type type, const char *src, const char *dest); // 작업 추가
void *sftp_worker_thread(void *arg); // 일꾼 쓰레드가 실행할 실제 작업 코드

#endif
