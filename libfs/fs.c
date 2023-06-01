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
    size_t offset;  // Current offset of the file
};

struct superblock super_blk;
uint16_t* fat_entries;
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

    if (strcmp(super_blk.signature, "ECS150FS") != 0) {
        return -1;
    }

    if (super_blk.total_blk_num != block_disk_count()) {
        return -1;
    }

    // Read FAT
    size_t fat_entries_num = super_blk.fat_blk_num * BLOCK_SIZE / sizeof(uint16_t);
    fat_entries = (uint16_t *) calloc(fat_entries_num, sizeof(uint16_t));

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

    uint16_t fat_free = 0;
    int rdir_free = 0;
    uint16_t fat_entries_num = super_blk.fat_blk_num * BLOCK_SIZE / sizeof(uint16_t);

    printf("FS Info:\n");
    printf("total_blk_count=%u\n", super_blk.total_blk_num);
    printf("fat_blk_count=%u\n", super_blk.fat_blk_num);
    printf("rdir_blk=%u\n", super_blk.rdir_idx);
    printf("data_blk=%u\n", super_blk.data_idx);
    printf("data_blk_count=%u\n", super_blk.data_block_num);

    for (uint16_t i = 0; i < fat_entries_num; i++) {
        if (fat_entries[i] == 0) {
            fat_free++;
        }
    }
    printf("fat_free_ratio=%u/%u\n", fat_free, fat_entries_num);

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

int find_file(const char *name) {
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

    file_root_idx = find_file(filename);
    if (file_root_idx == -1) {
        return file_root_idx;
    }

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (opened_fd[i].seat == 0) {
            opened_fd[i].seat = 1;
            opened_fd[i].root_idx = file_root_idx;
            opened_fd[i].offset = 0;
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
   printf("File descriptor: %d\n", fd);
   printf("Buffer address: %p\n", buf);
   printf("Count: %zu\n", count);
   return 0;
}
size_t fd_offset[FS_OPEN_MAX_COUNT] = {0};
int fs_read(int fd, void *buf, size_t count)
{
    if (!is_mount || fd < 0 || fd >= FS_OPEN_MAX_COUNT ||  buf == NULL) {
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
