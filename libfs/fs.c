#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF
#define SUPER_BLK_IDX 0

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

struct root {
    char file_name[FS_FILENAME_LEN];
    uint32_t file_size;
    uint16_t first_data_idx;
    char padding[10];
}__attribute__((packed));

struct fd_table {
    int seat;   // Reveal the position is occupied by a fd or not
    int root_idx;   // Corresponding root index in root data structure
    int cur_data_blk;   // The i-th data block for offset
    size_t offset;  // Current offset of the file
};

struct superblock super_blk;
uint16_t* fat_entries = NULL;
struct root rt_dirt[FS_FILE_MAX_COUNT];
int is_mount = 0;
struct fd_table opened_fd[FS_OPEN_MAX_COUNT];

void ini_fdt(struct fd_table *fdt) {
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        fdt[i].seat = 0;
    }
}

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
    if (block_disk_open(diskname) == -1) {
        return -1;
    }

    // Read Superblock
    if(block_read(SUPER_BLK_IDX, (void*)&super_blk) == -1) {
        return -1;
    }

    if (memcmp(&super_blk.signature, "ECS150FS", 8) != 0) {
        return -1;
    }

    if (super_blk.total_blk_num != block_disk_count()) {
        return -1;
    }

    // Read FAT
    fat_entries = (uint16_t *) calloc(super_blk.data_block_num * 2, sizeof(uint16_t));
    for (size_t i = SUPER_BLK_IDX + 1; i < (size_t)super_blk.rdir_idx; i++) {
        size_t offset = (i - SUPER_BLK_IDX + 1) * BLOCK_SIZE / sizeof(uint16_t);
        if (block_read(i, (void*)(fat_entries + offset)) == -1) {
            return -1;
        }
    }

    // Read Root directory
    if (block_read(super_blk.rdir_idx, (void*)rt_dirt) == -1) {
        return -1;
    }

    is_mount = 1;
    ini_fdt(opened_fd);

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

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (opened_fd[i].seat != 0) {
            return -1;
        }
    }

    if (block_disk_close() == -1) {
        return -1;
    }
    free(fat_entries);
    return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
    if (!is_mount) {
        return -1;
    }

    int fat_free = 0;
    int rdir_free = 0;
    printf("FS Info:\n");
    printf("total_blk_count=%u\n", super_blk.total_blk_num);
    printf("fat_blk_count=%u\n", super_blk.fat_blk_num);
    printf("rdir_blk=%u\n", super_blk.rdir_idx);
    printf("data_blk=%u\n", super_blk.data_idx);
    printf("data_blk_count=%u\n", super_blk.data_block_num);

    for (int i = 1; i < super_blk.data_block_num; i++) {
        if (fat_entries[i] == 0) {
            fat_free++;
        }
    }
    printf("fat_free_ratio=%u/%u\n", fat_free, super_blk.data_block_num);

    for (int j = 0; j < FS_FILE_MAX_COUNT; j++) {
        struct root cur_entry = rt_dirt[j];
        if (cur_entry.file_name[0] == '\0') {
            rdir_free++;
        }
    }
    printf("rdir_free_ratio=%d/%d\n", rdir_free, FS_FILE_MAX_COUNT);

    return 0;

}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
    if (!is_mount || strlen(filename) > FS_FILENAME_LEN) {
        return -1;
    }

    // Find an empty slot in the root directory.
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (rt_dirt[i].file_name[0] == '\0') {
            memcpy(rt_dirt[i].file_name, (void*)filename,  FS_FILENAME_LEN);
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

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strcmp(rt_dirt[i].file_name, filename) == 0) {
            // Found the file. Now remove it.
            uint16_t curr = rt_dirt[i].first_data_idx;
            while (curr != FAT_EOC) {
                uint16_t next = fat_entries[curr];
                fat_entries[curr] = 0; // Free the block
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
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (rt_dirt[i].file_name[0] != '\0') {
            printf("file: %s, size: %u, data_blk: %u\n", rt_dirt[i].file_name, rt_dirt[i].file_size, rt_dirt[i].first_data_idx);
        }
    }

    return 0;
}

int file_exist(const char *name) {
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (strcmp(name, (char *)rt_dirt[i].file_name) == 0) {
            return i;
        }
    }
    return -1;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
    int file_root_idx = 0;
    if (!is_mount) {
        return -1;
    }

    if (filename == NULL) {
        return -1;
    }

    file_root_idx = file_exist(filename);
    if (file_root_idx == -1) {
        return -1;
    }

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (opened_fd[i].seat == 0) {
            opened_fd[i].seat = 1;
            opened_fd[i].root_idx = file_root_idx;
            opened_fd[i].offset = 0;
            opened_fd[i].cur_data_blk = 0;
            return i;
        }
    }

    return -1;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
    if (!is_mount) {
        return -1;
    }

    if (fd >= FS_OPEN_MAX_COUNT || fd < 0) {
        return -1;
    }

    if (opened_fd[fd].seat == 0) {
        return -1;
    }

    opened_fd[fd].seat = 0;
    return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
    if (!is_mount) {
        return -1;
    }

    if (fd >= FS_OPEN_MAX_COUNT || fd < 0) {
        return -1;
    }

    if (opened_fd[fd].seat == 0) {
        return -1;
    }

    uint32_t size = rt_dirt[opened_fd[fd].root_idx].file_size;
    return size;

}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
    if (!is_mount) {
        return -1;
    }

    if (fd >= FS_OPEN_MAX_COUNT || fd < 0) {
        return -1;
    }

    if (opened_fd[fd].seat == 0) {
        return -1;
    }

    if (offset > (size_t) fs_stat(fd)) {
        return -1;
    }

    opened_fd[fd].offset = offset;
    // Calculate the offset is in which block in the file
    if (opened_fd[fd].offset >= BLOCK_SIZE) {
        opened_fd[fd].cur_data_blk = opened_fd[fd].offset / BLOCK_SIZE;
    }

    return 0;
}
// Return the data blk idx of offset currently in
int get_data_blk_idx(int fd) {
    int blk_num = opened_fd[fd].cur_data_blk;
    int blk_idx = 0;
    int first_blk_idx = rt_dirt[opened_fd[fd].root_idx].first_data_idx;
    if (blk_num == 0) {
        return first_blk_idx;
    } else {
        for (int i = 0; i < blk_num; i++) {
            blk_idx = fat_entries[first_blk_idx];
            first_blk_idx = blk_idx;
        }
    }
    blk_idx += super_blk.fat_blk_num + 2;
    return blk_idx;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    if (!is_mount) {
        return -1;
    }

    if (fd >= FS_OPEN_MAX_COUNT || fd < 0) {
        return -1;
    }

    if (opened_fd[fd].seat == 0) {
        return -1;
    }

    if (buf == NULL) {
        return -1;
    }
    size_t remaining = count;
    size_t buf_pos = 0;
    uint write_size = 0;

    while (remaining > 0) {
        int data_blk_idx = get_data_blk_idx(fd);
        char *bounce = (char *) calloc(BLOCK_SIZE, sizeof(char));
        int cur_offset = opened_fd[fd].offset;
        size_t offset_in_blk = cur_offset % BLOCK_SIZE;
        size_t cost = 0;
        size_t free_idx = 0;

        if (BLOCK_SIZE - offset_in_blk > remaining) {
            cost = remaining;
        } else {
            cost = BLOCK_SIZE - offset_in_blk;
        }

        if (block_read(data_blk_idx, (void *)bounce) == -1) {
            free(bounce);
            return -1;
        };
        memcpy(bounce + cur_offset % BLOCK_SIZE, (char *)buf + buf_pos, cost);
        block_write(data_blk_idx, (void *)bounce);
        buf_pos += cost;
        remaining -= cost;
        write_size += cost;
        if (fs_lseek(fd, cur_offset + cost) == -1) {
            free(bounce);
            return -1;
        }

        if (remaining > 0 && opened_fd[fd].offset >= rt_dirt[opened_fd[fd].root_idx].file_size) {
            // Find free index
            for (int i = 1; i < super_blk.data_block_num; i++) {
                if (fat_entries[i] == 0) {
                    free_idx = i;
                    break;
                }

                if (i == super_blk.data_block_num - 1) {
                    return -1;
                }
            }

            // Allocate new block
            fat_entries[data_blk_idx] = free_idx;
            fat_entries[free_idx] = FAT_EOC;
        }
        rt_dirt[opened_fd[fd].root_idx].file_size += cost;
        free(bounce);
    }

    return write_size;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */

    printf("File descriptor: %d\n", fd);
    printf("Buffer address: %p\n", buf);
    printf("Count: %zu\n", count);
    return 0;
}

