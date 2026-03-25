#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/config.h"
#include "../include/sftp_client.h"

void test_config() {
    printf("Testing config load/save...\n");
    sftp_config cfg;
    strcpy(cfg.host, "test.host");
    strcpy(cfg.username, "testuser");
    strcpy(cfg.password, "testpass");
    strcpy(cfg.download_dir, "./test_downloads");

    // Save
    int res = save_config(&cfg);
    assert(res == 0);

    // Load
    sftp_config loaded;
    res = load_config(&loaded);
    assert(res == 0);
    assert(strcmp(loaded.host, "test.host") == 0);
    assert(strcmp(loaded.username, "testuser") == 0);
    printf("Config test passed!\n");
}

void test_local_files() {
    printf("Testing local file listing...\n");
    file_info *files = NULL;
    int count = 0;
    
    int res = get_local_files(".", &files, &count);
    assert(res == 0);
    assert(count > 0);
    assert(files != NULL);

    int found_makefile = 0;
    int found_parent = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(files[i].name, "Makefile") == 0) found_makefile = 1;
        if (strcmp(files[i].name, "..") == 0) found_parent = 1;
    }
    assert(found_makefile == 1);
    assert(found_parent == 1);
    free(files);
    printf("Local file listing test passed (including '..')!\n");
}

int main() {
    printf("Starting unit tests...\n");
    test_config();
    test_local_files();
    printf("All unit tests passed successfully!\n");
    return 0;
}
