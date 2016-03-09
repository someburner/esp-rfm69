/*
 * Wrappers intended to simplify working with multiple filesystems.
 */
#ifndef __FLASH_FS_H__
#define __FLASH_FS_H__

#include "user_config.h"
#include "queue.h"
#include "json/cJSON.h"
#include "spiffs/spiffs.h"

#define FS0				0 //semi-static FS
#define FS1				1 //dynamic FS

#define FS_OPEN_OK	1
#define FS_NAME_MAX_LENGTH SPIFFS_OBJ_NAME_LEN

#define MARK_RDONLY		(1<<7)
#define MARK_GZIP    	(1<<6)
#define MARK_DELETE		(1<<5)
#define MARK_DYNFS		(1<<4)
#define MARK_STATICFS	(1<<3)
#define MARK_BIG			(1<<2)

#define FS_JNAME  		"name"
#define FS_JTYPE  		"type"
#define FS_JPATH  		"path"
#define FS_JITEMS  		"items"
#define FS_JSIZE  		"size"

#define FS_JFILE  		"file"
#define FS_JFILES  		"files"
#define FS_JFOLDER  		"folder"

int fsJSON_add_item(cJSON * arr, char * name, char * path, int size);
cJSON * fsJSON_add_folder(cJSON * parent, char * foldername, char * parentpath);
cJSON * fsJSON_get_list();

// int fs_exists(char * name);

struct fs_file_st {
	STAILQ_ENTRY(fs_file_st) next;
   uint8_t  flags;
	uint16_t pix;
	uint32_t size;
	char *name;
};

STAILQ_HEAD(fs_file_head, fs_file_st) fs_file_list;

typedef enum {
	dynfs_flags,
	dynfs_size,
	dynfs_pix
} dynfs_tags_t;

int fs_exists(char * name, struct fs_file_st ** fs_st_ptr);
// struct fs_file_st * fs_exists(char * name);
// int fs_new_file(uint8_t fs, char * filename, bool overwrite, void * data, uint16_t len);
// int fs_new_file(char * filename, bool overwrite);
int fs_new_file(struct fs_file_st ** fs_st_ptr, char * filename, bool overwrite);
int fs_rename_file(uint8_t fs, char * old, char * new);
int fs_append_to_file(struct fs_file_st * fs_st,  void * data, uint16_t len);
int fs_update_pix(struct fs_file_st * fs_st);


#define FS_RDONLY SPIFFS_RDONLY
#define FS_WRONLY SPIFFS_WRONLY
#define FS_RDWR 	SPIFFS_RDWR
#define FS_APPEND SPIFFS_APPEND
#define FS_TRUNC 	SPIFFS_TRUNC
#define FS_CREAT 	SPIFFS_CREAT
#define FS_DIRECT SPIFFS_DIRECT
#define FS_EXCL 	SPIFFS_EXCL

#define FS_SEEK_SET SPIFFS_SEEK_SET
#define FS_SEEK_CUR SPIFFS_SEEK_CUR
#define FS_SEEK_END SPIFFS_SEEK_END

#define myfs_check			myspiffs_check
#define myfs_info				myspiffs_info
#define myfs_rename 			myspiffs_rename
#define myfs_openpage		myspiffs_openpage
#define myfs_openname		myspiffs_open
#define myfs_read				myspiffs_read
#define myfs_write			myspiffs_write
#define myfs_close			myspiffs_close
#define myfs_fstat			myspiffs_fstat
#define myfs_remove 			myspiffs_remove
#define myfs_fremove 		myspiffs_fremove
#define myfs_errno	 		myspiffs_errno
#define myfs_seek				myspiffs_lseek
#define myfs_creat			myspiffs_creat

/******* Wrapper methods ********/

void fs_init_info(void);
void fs_update_list(void);
int fs_remove_by_name(char * filename);
int fs_read_file(struct fs_file_st * fs_st, void * buff, uint16_t len, uint32_t pos);
uint32_t fs_get_avail(int fs);


/******* Static File System ********/

#define fs_DIR 				spiffs_DIR
#define fs_dirent 			spiffs_dirent
#define fs_open(a, b) 		myspiffs_open(FS0, a, b)
#define fs_close(a) 			myspiffs_close(FS0, a)

#define fs_openpage(a, b)  myspiffs_openpage(FS0, a, b)
#define fs_opendir(a, b) 	myspiffs_opendir(FS0, a, b)
#define fs_closedir(a) 		myspiffs_closedir(FS0, a)
#define fs_readdir(a, b) 	myspiffs_readdir(FS0, a, b)

#define fs_write(a, b, c) 	myspiffs_write(FS0, a, b, c)
#define fs_read(a, b, c) 	myspiffs_read(FS0, a, b, c)
// #define fs_remove(a) 		myspiffs_remove(FS0, a)
// #define fs_fremove(a) 		myspiffs_fremove(FS0, a)

#define fs_seek(a, b, c) 	myspiffs_lseek(FS0, a, b, c)

#define fs_stat(a, b) 		myspiffs_stat(FS0, a, b)
#define fs_fstat(a, b) 		myspiffs_fstat(FS0, a, b)
#define fs_eof(a) 			myspiffs_eof(FS0, a)
#define fs_getc(a) 			myspiffs_getc(FS0, a)
#define fs_ungetc(a, b) 	myspiffs_ungetc(FS0, a, b)
#define fs_flush(a) 			myspiffs_flush(FS0, a)
#define fs_errno() 			myspiffs_errno(FS0)
#define fs_clearerr() 		myspiffs_clearerr(FS0)
#define fs_tell(a) 			myspiffs_tell(FS0, a)
#define fs_listall() 		myspiffs_listall(FS0)

#define fs_format() 			myspiffs_format(FS0)
#define fs_check() 			myspiffs_check(FS0)
#define fs_size(a) 			myspiffs_size(FS0, a)
#define fs_match(a, b) 		myspiffs_fmatch(FS0, a, b)

#define fs_mount() myspiffs_mount(FS0)
#define fs_mounted() myspiffs_mounted(FS0)
#define fs_unmount() myspiffs_unmount(FS0)


/******* Dynamic File System ********/

#define dynfs_openpage(a, b)  myspiffs_openpage(FS1, a, b)
#define dynfs_open(a, b) 		myspiffs_open(FS1, a, b)
#define dynfs_close(a) 			myspiffs_close(FS1, a)

#define dynfs_opendir(a, b) 	myspiffs_opendir(FS1, a, b)
#define dynfs_closedir(a) 		myspiffs_closedir(FS1, a)
#define dynfs_readdir(a, b) 	myspiffs_readdir(FS1, a, b)

#define dynfs_write(a, b, c) 	myspiffs_write(FS1, a, b, c)
#define dynfs_read(a, b, c) 	myspiffs_read(FS1, a, b, c)
#define dynfs_seek(a, b, c) 	myspiffs_lseek(FS1, a, b, c)
#define dynfs_rename(a, b) 	myspiffs_rename(FS1, a, b)

#define dynfs_stat(a, b) 		myspiffs_stat(FS1, a, b)
#define dynfs_fstat(a, b) 		myspiffs_fstat(FS1, a, b)
#define dynfs_eof(a) 			myspiffs_eof(FS1, a)
#define dynfs_getc(a) 			myspiffs_getc(FS1, a)
#define dynfs_ungetc(a, b) 	myspiffs_ungetc(FS1, a, b)
#define dynfs_flush(a) 			myspiffs_flush(FS1, a)
#define dynfs_errno() 			myspiffs_errno(FS1)
#define dynfs_clearerr() 		myspiffs_clearerr(FS1)
#define dynfs_tell(a) 			myspiffs_tell(FS1, a)
#define dynfs_listall() 		myspiffs_listall(FS1)

#define dynfs_format()			myspiffs_format(FS1)
#define dynfs_check()			myspiffs_check(FS1)

#define dynfs_mount() myspiffs_mount(FS1)
#define dynfs_mounted() myspiffs_mounted(FS1)
#define dynfs_unmount() myspiffs_unmount(FS1)

#endif // #ifndef __FLASH_FS_H__
