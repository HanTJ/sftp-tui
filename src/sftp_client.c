#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>   // 파일 정보(크기, 타입 등) 관련 헤더
#include <fcntl.h>      // 파일 제어 관련 헤더
#include <dirent.h>     // 디렉토리 탐색 관련 헤더
#include "../include/sftp_client.h"

/* 
 * sftp_init_client: 서버에 접속하고 SFTP 세션을 준비합니다. 
 */
int sftp_init_client(sftp_client *client, const sftp_config *cfg) {
    struct sockaddr_in sin;
    unsigned long hostaddr;

    // 초기 상태 설정
    client->sock = -1;
    client->session = NULL;
    client->sftp_session = NULL;
    client->connected = 0;
    client->task_count = 0;
    client->current_task_idx = 0;
    client->stop_worker = 0;
    
    // 뮤텍스 초기화: 데이터 동기화를 위해 사용
    pthread_mutex_init(&client->sftp_mutex, NULL);
    pthread_mutex_init(&client->queue_mutex, NULL);

    // IP 주소 변환
    hostaddr = inet_addr(cfg->host);
    if (hostaddr == INADDR_NONE) return -5;

    // TCP 소켓 생성
    client->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client->sock == -1) return -6;

    sin.sin_family = AF_INET;
    sin.sin_port = htons(22); // SSH 기본 포트: 22
    sin.sin_addr.s_addr = hostaddr;

    // 서버 연결 시도 (TCP Connect)
    if (connect(client->sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
        close(client->sock);
        client->sock = -1;
        return -1;
    }

    // SSH 세션 초기화
    client->session = libssh2_session_init();
    if (!client->session) {
        close(client->sock);
        client->sock = -1;
        return -7;
    }

    // SSH 핸드셰이크 (서버와의 보안 협상)
    if (libssh2_session_handshake(client->session, client->sock)) {
        sftp_close_client(client);
        return -2;
    }

    // 비밀번호 기반 사용자 인증
    if (libssh2_userauth_password(client->session, cfg->username, cfg->password)) {
        sftp_close_client(client);
        return -3;
    }

    // SFTP 서비스 시작
    client->sftp_session = libssh2_sftp_init(client->session);
    if (!client->sftp_session) {
        sftp_close_client(client);
        return -4;
    }

    client->connected = 1;
    // 전송을 담당할 일꾼 쓰레드 시작
    pthread_create(&client->worker_thread, NULL, sftp_worker_thread, client);
    return 0;
}

/* 
 * sftp_close_client: 접속을 끊고 할당된 메모리 및 리소스를 정리합니다. 
 */
void sftp_close_client(sftp_client *client) {
    if (client->connected) {
        client->stop_worker = 1; // 일꾼 쓰레드에게 중단 신호를 보냄
        pthread_join(client->worker_thread, NULL); // 일꾼이 완전히 끝날 때까지 대기
    }
    
    // SFTP 및 SSH 세션 해제
    if (client->sftp_session) {
        libssh2_sftp_shutdown(client->sftp_session);
        client->sftp_session = NULL;
    }
    if (client->session) {
        libssh2_session_disconnect(client->session, "Normal Shutdown");
        libssh2_session_free(client->session);
        client->session = NULL;
    }
    if (client->sock != -1) {
        close(client->sock);
        client->sock = -1;
    }
    client->connected = 0;
    // 뮤텍스 리소스 해제
    pthread_mutex_destroy(&client->sftp_mutex);
    pthread_mutex_destroy(&client->queue_mutex);
}

/* 
 * sftp_add_task: 작업 큐에 새 작업을 추가합니다. 
 */
void sftp_add_task(sftp_client *client, task_type type, const char *src, const char *dest) {
    pthread_mutex_lock(&client->queue_mutex); // 큐 접근 권한 획득 (잠금)
    if (client->task_count < MAX_TASKS) {
        sftp_task *t = &client->tasks[client->task_count];
        t->type = type;
        strncpy(t->src, src, sizeof(t->src));
        strncpy(t->dest, dest, sizeof(t->dest));
        t->progress = 0;
        t->status = 0; // 대기 중 (Pending)
        client->task_count++;
    }
    pthread_mutex_unlock(&client->queue_mutex); // 권한 반납 (잠금 해제)
}

/* 
 * do_upload (내부용): 실제 서버로 파일을 업로드하는 로직입니다. 
 */
static int do_upload(sftp_client *client, sftp_task *task) {
    struct stat st;
    if (stat(task->src, &st) != 0) return -1;
    long long total_size = st.st_size; // 업로드할 총 파일 크기
    long long uploaded = 0;

    FILE *local = fopen(task->src, "rb"); // 로컬 파일을 읽기 전용으로 열기
    if (!local) return -1;

    // 서버에 업로드할 파일을 생성하거나 열기
    pthread_mutex_lock(&client->sftp_mutex);
    LIBSSH2_SFTP_HANDLE *remote = libssh2_sftp_open(client->sftp_session, task->dest,
                                                  LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                                                  0755);
    pthread_mutex_unlock(&client->sftp_mutex);

    if (!remote) {
        fclose(local);
        return -2;
    }

    char buffer[16384]; // 데이터를 실어 나를 임시 바구니 (16KB)
    size_t nread;
    task->status = 1; // 작업 중 (Running) 상태로 변경
    while ((nread = fread(buffer, 1, sizeof(buffer), local)) > 0) {
        char *ptr = buffer;
        size_t remaining = nread;
        while (remaining > 0) {
            // 서버에 데이터 쓰기 (네트워크 작업은 뮤텍스로 보호)
            pthread_mutex_lock(&client->sftp_mutex);
            ssize_t rc = libssh2_sftp_write(remote, ptr, remaining);
            pthread_mutex_unlock(&client->sftp_mutex);
            if (rc < 0) {
                remaining = 0; 
                break;
            }
            ptr += rc;
            remaining -= rc;
            uploaded += rc;
            // 진행률 계산
            if (total_size > 0) task->progress = (float)uploaded / total_size * 100.0f;
        }
        if (client->stop_worker) break; // 일꾼 종료 명령 시 중단
    }

    pthread_mutex_lock(&client->sftp_mutex);
    libssh2_sftp_close(remote);
    pthread_mutex_unlock(&client->sftp_mutex);
    fclose(local);
    return 0;
}

/* 
 * do_download (내부용): 서버에서 파일을 가져오는 로직입니다. 
 */
static int do_download(sftp_client *client, sftp_task *task) {
    pthread_mutex_lock(&client->sftp_mutex);
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    // 서버 파일 정보(크기 등) 확인
    if (libssh2_sftp_stat(client->sftp_session, task->src, &attrs) != 0) {
        pthread_mutex_unlock(&client->sftp_mutex);
        return -1;
    }
    long long total_size = (long long)attrs.filesize;
    LIBSSH2_SFTP_HANDLE *remote = libssh2_sftp_open(client->sftp_session, task->src, LIBSSH2_FXF_READ, 0);
    pthread_mutex_unlock(&client->sftp_mutex);

    if (!remote) return -1;

    FILE *local = fopen(task->dest, "wb"); // 로컬 파일을 쓰기 전용으로 열기
    if (!local) {
        pthread_mutex_lock(&client->sftp_mutex);
        libssh2_sftp_close(remote);
        pthread_mutex_unlock(&client->sftp_mutex);
        return -2;
    }

    char buffer[16384];
    ssize_t nread;
    long long downloaded = 0;
    task->status = 1; // 작업 중
    while (!client->stop_worker) {
        pthread_mutex_lock(&client->sftp_mutex);
        nread = libssh2_sftp_read(remote, buffer, sizeof(buffer)); // 서버에서 읽기
        pthread_mutex_unlock(&client->sftp_mutex);
        if (nread <= 0) break; // 더 읽을 게 없으면 종료

        fwrite(buffer, 1, nread, local); // 로컬에 쓰기
        downloaded += nread;
        if (total_size > 0) task->progress = (float)downloaded / total_size * 100.0f;
    }

    pthread_mutex_lock(&client->sftp_mutex);
    libssh2_sftp_close(remote);
    pthread_mutex_unlock(&client->sftp_mutex);
    fclose(local);
    return 0;
}

/* 
 * sftp_worker_thread: 백그라운드에서 끊임없이 큐를 확인하며 전송 작업을 수행합니다. 
 */
void *sftp_worker_thread(void *arg) {
    sftp_client *client = (sftp_client *)arg;
    while (!client->stop_worker) {
        sftp_task *current = NULL;
        // 큐를 확인하여 할 일이 있는지 검사
        pthread_mutex_lock(&client->queue_mutex);
        if (client->current_task_idx < client->task_count) {
            current = &client->tasks[client->current_task_idx];
        }
        pthread_mutex_unlock(&client->queue_mutex);

        if (current) {
            int res;
            // 할 일 수행
            if (current->type == TASK_UPLOAD) res = do_upload(client, current);
            else res = do_download(client, current);
            
            // 결과 기록
            current->status = (res == 0) ? 2 : 3;
            current->progress = (res == 0) ? 100.0f : current->progress;
            
            pthread_mutex_lock(&client->queue_mutex);
            client->current_task_idx++; // 다음 작업으로 넘어감
            pthread_mutex_unlock(&client->queue_mutex);
        } else {
            usleep(100000); // 할 일이 없으면 0.1초 동안 휴식 (CPU 낭비 방지)
        }
    }
    return NULL;
}

/* 
 * sftp_get_files: 서버 디렉토리의 파일 목록을 가져와 동적 배열에 저장합니다. 
 */
int sftp_get_files(sftp_client *client, const char *path, file_info **files, int *count) {
    *files = NULL;
    *count = 0;

    if (!client->connected || !client->sftp_session) return -10;

    pthread_mutex_lock(&client->sftp_mutex);
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_opendir(client->sftp_session, path);
    pthread_mutex_unlock(&client->sftp_mutex);
    if (!handle) return -1;

    int capacity = 10; // 초기 메모리 할당 공간 (10개)
    *files = malloc(sizeof(file_info) * capacity);
    if (!*files) {
        pthread_mutex_lock(&client->sftp_mutex);
        libssh2_sftp_closedir(handle);
        pthread_mutex_unlock(&client->sftp_mutex);
        return -8;
    }

    char mem[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    while (1) {
        pthread_mutex_lock(&client->sftp_mutex);
        int res = libssh2_sftp_readdir(handle, mem, sizeof(mem), &attrs);
        pthread_mutex_unlock(&client->sftp_mutex);
        if (res <= 0) break;

        if (strcmp(mem, ".") == 0) continue; // '.'은 자기 자신이므로 무시

        // 배열이 가득 찼으면 크기를 2배로 늘림 (Realloc)
        if (*count >= capacity) {
            capacity *= 2;
            file_info *temp = realloc(*files, sizeof(file_info) * capacity);
            if (!temp) {
                free(*files);
                *files = NULL;
                pthread_mutex_lock(&client->sftp_mutex);
                libssh2_sftp_closedir(handle);
                pthread_mutex_unlock(&client->sftp_mutex);
                return -9;
            }
            *files = temp;
        }
        
        // 목록 정보 채우기
        memset(&(*files)[*count], 0, sizeof(file_info));
        strncpy((*files)[*count].name, mem, MAX_STR_LEN - 1);
        (*files)[*count].is_dir = (LIBSSH2_SFTP_S_ISDIR(attrs.permissions));
        (*files)[*count].size = (long long)attrs.filesize;
        (*count)++;
    }

    pthread_mutex_lock(&client->sftp_mutex);
    libssh2_sftp_closedir(handle);
    pthread_mutex_unlock(&client->sftp_mutex);
    return 0;
}

/* 
 * get_local_files: 내 컴퓨터(로컬)의 파일 목록을 읽어옵니다. 
 */
int get_local_files(const char *path, file_info **files, int *count) {
    *files = NULL;
    *count = 0;

    DIR *d = opendir(path);
    if (!d) return -1;

    int capacity = 10;
    *files = malloc(sizeof(file_info) * capacity);
    if (!*files) {
        closedir(d);
        return -2;
    }

    struct dirent *dir;
    struct stat st;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0) continue;

        if (*count >= capacity) {
            capacity *= 2;
            file_info *temp = realloc(*files, sizeof(file_info) * capacity);
            if (!temp) {
                free(*files);
                *files = NULL;
                closedir(d);
                return -3;
            }
            *files = temp;
        }

        memset(&(*files)[*count], 0, sizeof(file_info));
        strncpy((*files)[*count].name, dir->d_name, MAX_STR_LEN - 1);
        
        char full_path[2048];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, dir->d_name);
        // stat: 파일의 상세 정보(디렉토리 여부, 크기 등)를 가져옵니다.
        if (stat(full_path, &st) == 0) {
            (*files)[*count].is_dir = S_ISDIR(st.st_mode);
            (*files)[*count].size = (long long)st.st_size;
        }
        (*count)++;
    }

    closedir(d);
    return 0;
}
