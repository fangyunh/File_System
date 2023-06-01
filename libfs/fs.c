#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define RDIR_EN_LEN 32
#define FAT_EN_LEN 2
#define BLOCK_SIZE 512
#define FS_OPEN_MAX_COUNT 256

/* TODO: Phase 1 */
// Data structures of blocks
struct superblock {
    char signature[8];
    uint16_t total_blk_num;    // Total amount of blocks of virtual disk
    uint16_t rdir_idx;
    uint16_t data_idx;         // Data block start index
    uint16_t data_block_num;
    uint8_t fat_blk_num;
    char padding[4079];
} __attribute__ ((packed));

struct fat {
    uint16_t* entries;
    uint16_t size;
};

struct  root {
    char file_name[FS_FILENAME_LEN];
    uint32_t file_size;
    uint16_t first_data_idx;
    char padding[10];
    int size;
}__attribute__((packed));

struct superblock super_blk;
struct fat fat_blk[FS_OPEN_MAX_COUNT];
struct root rt_dirt[FS_FILE_MAX_COUNT];
int is_mount = 0;
int opened_fd[FS_OPEN_MAX_COUNT] = {0};
size_t fd_offset[FS_OPEN_MAX_COUNT];

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
    if (block_disk_open(diskname) == -1) {
        return -1;
    }

    // Read Superblock
    if(block_read(0, (void*)super_blk) == -1) {
        return -1;
    }

    if (strcmp(super_blk.signature, "ECS150FS") != 0) {
        return -1;
    }

    if (super_blk.total_blk_num != block_disk_count()) {
        return -1;
    }

    // Read FAT
    for (size_t i = 0; i < (size_t)super_blk.fat_blk_num; i++) {
        if (block_read(i + 1, (void*)&fat_blk.entries[i]) == -1) {
            return -1;
        }
    }
    fat_blk.size = (uint16_t)super_blk.fat_blk_num * BLOCK_SIZE / FAT_EN_LEN;

    // Read Root directory
    if (block_read(super_blk.fat_blk_num + 1, (void*)&rt_dirt) == -1) {
        return -1;
    }
    rt_dirt.size = BLOCK_SIZE / RDIR_EN_LEN;

    is_mount = 1;

    return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
    // Check is there a FS mounted.
    if (!is_mount) {
        return -1;
    }

    if (block_disk_count() != -1) {
        return -1;
    }

    if (block_disk_close() == -1) {
        return -1;
    }

    return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
    if (!is_mount) {
        return -1;
    }

    uint16_t fat_free = 0;
    int rdir_free = 0;

    printf("FS Info:\n");
    printf("total_blk_count=%u\n", super_blk.total_blk_num);
    printf("fat_blk_count=%u\n", super_blk.fat_blk_num);
    printf("rdir_blk=%u\n", super_blk.rdir_idx);
    printf("data_blk=%u\n", super_blk.data_idx);
    printf("data_blk_count=%u\n", super_blk.data_block_num);

    for (uint16_t i = 0; i < fat_blk.size; i++) {
        if (fat_blk.entries[i] == 0) {
            fat_free++;
        }
    }
    printf("fat_free_ratio=%u/%u\n", fat_free, fat_blk.size);

    for (int j = 0; j < rt_dirt.size; j++) {
        if (rt_dirt[j].file_name[0] == '\0') {
            rdir_free++;
        }
    }
    printf("rdir_free_ratio=%d/%d\n", rdir_free, rt_dirt.size);

    return 0;

}

int fs_create(const char *filename)
{
    /* TODO: Phase 2 */
    if (!is_mount || strlen(filename) > RDIR_EN_LEN) {
        return -1;
    }

    // Find an empty slot in the root directory.
    for (int i = 0; i < rt_dirt->size; i++) {
        if (rt_dirt[i].file_name[0] == '\0') {
            memcpy(rt_dirt[i].filename, (void*)filename,  FS_FILENAME_LEN);
            rt_dirt[i].file_size = 0;
            rt_dirt[i].first_data_idx = FAT_EOC;
            // You may need to write changes to the disk here.

            return 0;
        }
    }

    // If we reach here, the root directory is full.
    return -1;
}

int fs_delete(const char *filename)
{
    /* TODO: Phase 2 */
    if (!is_mount) {
        return -1;
    }

    for (int i = 0; i < rt_dirt->size; i++) {
        if (strcmp(rt_dirt[i].file_name, filename) == 0) {
            // Found the file. Now remove it.
            uint16_t curr = rt_dirt[i].first_data_idx;
            while (curr != FAT_EOC) {
                uint16_t next = fat_blk->entries[curr];
                fat_blk->entries[curr] = 0; // Free the block
                curr = next;
            }
            memset(&rt_dirt[i], 0, sizeof(rt_dirt[i])); // Clear the directory entry
            // You may need to write changes to the disk here.

            return 0;
        }
    }

    // If we reach here, the file does not exist.
    return -1;
}

int fs_ls(void)
{
    /* TODO: Phase 2 */
    if (!is_mount) {
        return -1;
    }

    printf("FS Ls:\n");
    for (int i = 0; i < rt_dirt->size; i++) {
        if (rt_dirt[i].file_name[0] != '\0') {
            printf("file: %s, size: %u, data_blk: %u\n", rt_dirt[i].file_name, rt_dirt[i].file_size, rt_dirt[i].first_data_idx);
        }
    }

    return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */

    if (!is_mount) {
        return -1;
    }

    if (filename == NULL) {
        return -1;
    }

    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        return -1;
    }

    if(fd >= FS_OPEN_MAX_COUNT) {
        close(fd);
        return -1;
    }
    opened_fd[fd] = 1;

    return fd;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
    if (!is_mount) {
        return -1;
    }

    if (fd >= FS_OPEN_MAX_COUNT || fd < 0) {
        return -1
    }

    if (opened_fd[fd] == 0) {
        return -1;
    }

    opened_fd[fd] = 0;
    close(fd);
    return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
    if (!is_mount) {
        return -1;
    }

    if (fd >= FS_OPEN_MAX_COUNT || fd < 0) {
        return -1
    }

    if (opened_fd[fd] == 0) {
        return -1;
    }

    // Ref: https://man7.org/linux/man-pages/man2/lseek.2.html
    off_t cur_offset = lseek(fd, 0, SEEK_CUR);
    if (cur_offset == (off_t)-1) {
        return -1;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size == (off_t)-1) {
        return -1;
    }

    if (lseek(fd, cur_offset, SEEK_SET) == (off_t)-1) {
        return -1;
    }

    return size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
    if (!is_mount) {
        return -1;
    }

    if (fd >= FS_OPEN_MAX_COUNT || fd < 0) {
        return -1
    }

    if (opened_fd[fd] == 0) {
        return -1;
    }

    if (offset > (size_t) fs_stat(fd)) {
        return -1;
    }

    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        return -1;
    }

    return 0;
}

uint16_t get_next_block_index(uint16_t current) {
    return fat_entries[current];
}

uint16_t allocate_new_block() {
    uint16_t fat_entries_num = super_blk.fat_blk_num * BLOCK_SIZE / sizeof(uint16_t);
    for (uint16_t i = super_blk.data_idx; i < fat_entries_num; i++) {
        if (fat_entries[i] == 0) {
            fat_entries[i] = FAT_EOC;
            return i;
        }
    }
    return 0; // No block left
}

int fs_write(int fd, void *buf, size_t count)
{
    if (!is_mount || fd < 0 || fd >= FS_OPEN_MAX_COUNT || opened_fd[fd] == 0 || buf == NULL) {
        return -1;
    }

    size_t remaining = count;
    const char *cur_buf = (const char *) buf;

    // Calculate the current block index and offset within the block
    uint16_t current_block = fd_offset[fd] / BLOCK_SIZE;
    size_t block_offset = fd_offset[fd] % BLOCK_SIZE;

    while (remaining > 0) {
        char block[BLOCK_SIZE];
        if (block_read(current_block, block) == -1) {
            // If we can't read the current block, we need to allocate a new one.
            current_block = allocate_new_block();
            if (current_block == FAT_EOC) {
                // If we can't allocate a new block, we're out of disk space.
                break
		    }
	}
    }
}

int fs_read(int fd, void *buf, size_t count)
{
    if (!is_mount || fd < 0 || fd >= FS_OPEN_MAX_COUNT || opened_fd[fd] == 0 || buf == NULL) {
        return -1;
    }

    size_t remaining = count;
    char *cur_buf = (char *) buf;

    // Calculate the current block index and offset within the block
    uint16_t current_block = fd_offset[fd] / BLOCK_SIZE;
    size_t block_offset = fd_offset[fd] % BLOCK_SIZE;

    while (remaining > 0 && current_block != FAT_EOC) {
        char block[BLOCK_SIZE];
        if (block_read(current_block, block) == -1) {
            break;
        }

        // Calculate how many bytes we can read in the current block
        size_t to_read = remaining;
        if (to_read > BLOCK_SIZE - block_offset) {
            to_read = BLOCK_SIZE - block_offset;
        }

        memcpy(cur_buf, block + block_offset, to_read);

        remaining -= to_read;
        cur_buf += to_read;
        fd_offset[fd] += to_read;

        // If we are at the end of a block, move to the next one.
        if (block_offset + to_read == BLOCK_SIZE) {
            current_block = get_next_block_index(current_block);
            block_offset = 0;
        } else {
            block_offset += to_read;
        }
    }

    return count - remaining;
}
