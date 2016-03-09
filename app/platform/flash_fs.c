/*
 * App-level API for performing SPIFFS operations
 */
#include "user_interface.h"

#include "mem.h"
#include "json/cJSON.h"

#include "user_config.h"
#include "flash_fs.h"
#include "spiffs.h"

#define DO_RETURN(val) \
   return val;

#define CLOSE_AND_RETURN(fs, fd, val) \
   if (val < 0) { \
      myfs_close(fs, fd); \
      return val; \
   }

#define DO_ERRNO(val) \
   NODE_DBG("errno: %d", myfs_errno(FS1)); \
   DO_RETURN(val);

#define NULL_CHECK(ptr, on_err, err_arg) \
	if (!(ptr)) { \
		on_err(err_arg); \
	}

#define NEG_CHECK(input, on_err, err_arg) \
	if (input < 0) { \
		on_err(err_arg); \
	}

#define RDONLY_CHECK(fs_ptr, on_err, err_arg) \
   if (((fs_ptr)->flags) & MARK_RDONLY) { \
      NODE_ERR("This file is marked read only!\n"); \
      on_err(err_arg); \
   }

#define GET_FS(fs_ptr, fsN) \
   fsN = -1; \
	if ((fs_ptr)) { \
		if ((fs_ptr)->flags & MARK_DYNFS) { fsN = FS1; } \
      else if ((fs_ptr)->flags & MARK_STATICFS) { fsN = FS0; } \
	}

void fs_init_info(void)
{
   fs_DIR d, d2;
   struct fs_dirent e, e2;
   struct fs_dirent *pe = &e;
   struct fs_dirent *pe2 = &e2;

   fs_opendir("/", &d);
   while ((pe = fs_readdir(&d, pe)))
   {
      struct fs_file_st *f = NULL;
      f = (struct fs_file_st *)os_malloc(sizeof(struct fs_file_st));
      int size = strlen(pe->name);
      f->pix = pe->pix;
      f->size = pe->size;
      f->name = (char*)os_zalloc(sizeof(char)*size+1);
      strncpy(f->name, pe->name, size+1);
      f->flags = 0;

      /* set initial files to read only */
      f->flags |= MARK_RDONLY;
      f->flags |= MARK_GZIP;
      f->flags |= MARK_STATICFS;

      /* don't gzip png, unset */
      if ((strstr(f->name,".png")) != NULL) {f->flags &= ~MARK_GZIP; }
      if ((strstr(f->name,".conf")) != NULL) {f->flags &= ~MARK_GZIP; }

      STAILQ_INSERT_HEAD(&fs_file_list, f, next);
   }
   fs_closedir(&d);

   /* Initialize dynamic fs files too */
   dynfs_opendir("/", &d2);
   while ((pe2 = dynfs_readdir(&d2, pe2)))
   {
      struct fs_file_st *f2 = NULL;
      f2 = (struct fs_file_st *)os_malloc(sizeof(struct fs_file_st));
      int size = strlen(pe2->name);
      f2->pix = pe2->pix;
      f2->size = pe2->size;
      f2->name = (char*)os_zalloc(sizeof(char)*size+1);
      strncpy(f2->name, pe2->name, size+1);
      f2->flags = 0;

      if (pe2->size > 65000)
      {
         f2->flags |= MARK_BIG;
      }
      f2->flags |= MARK_DYNFS;
      NODE_DBG("dynfs file: %s, flags = %d\n", f2->name, (int)f2->flags);

      STAILQ_INSERT_HEAD(&fs_file_list, f2, next);
   }
   fs_closedir(&d2);

   struct fs_file_st *f3;
   STAILQ_FOREACH(f3, &fs_file_list, next)
   {
      NODE_DBG("[%04x] %s: %d bytes | flags: %d\n", f3->pix, f3->name, f3->size, (int)f3->flags);
   }
   f3 = STAILQ_FIRST(&fs_file_list);

}

/* Create a list of static files. This helps us avoid reads and writes
 * to SPIFFs. More importantly we can add whatever sorts of data we want
 * to the file structure which is useful for our HTTP CGI routines */
void fs_save_info(void)
{
   NODE_DBG("fs_save\n");
   int check_dynfs = 0;
   cJSON *root;
   int res;
   int dyn_fd;
   spiffs_stat s;
   res = dynfs_stat("static.index", &s);
   if (res < 0)
   {
      NODE_DBG("fstat <0\n");
      dyn_fd = dynfs_open("static.index", FS_CREAT | FS_TRUNC | FS_RDWR); // | FS_EXCL
   } else {
      dyn_fd = dynfs_open("static.index",  FS_RDONLY);
      check_dynfs = 1;
      if (s.size > 0)
      {
         NODE_DBG("static.index size = %d\n", s.size);
         res = dynfs_seek(dyn_fd, 0, FS_SEEK_SET);
         char * buff = (char*)os_malloc(sizeof(char)*s.size);
         res = dynfs_read(dyn_fd, buff, s.size);
         root = cJSON_Parse(buff);
      }
   }

   fs_DIR d;
   struct fs_dirent e;
   struct fs_dirent *pe = &e;

   if (check_dynfs == 0)
   {
      NODE_DBG("check fs=  0\n");
      root = cJSON_CreateObject();
   }

   fs_opendir("/", &d);
   while ((pe = fs_readdir(&d, pe)))
   {
      struct fs_file_st *f = NULL;
      f = (struct fs_file_st *)os_malloc(sizeof(struct fs_file_st));
      int size = strlen(pe->name);
      f->pix = pe->pix;
      f->size = pe->size;
      f->name = (char*)os_zalloc(sizeof(char)*size+1);
      strncpy(f->name, pe->name, size+1);
      f->flags = 0;

      /* set initial files to read only */
      f->flags |= MARK_RDONLY;
      f->flags |= MARK_GZIP;

      /* don't gzip png, unset */
      if ((strstr(f->name,".png")) != NULL) {f->flags &= ~MARK_GZIP; }
      if ((strstr(f->name,".conf")) != NULL) {f->flags &= ~MARK_GZIP; }

      cJSON *item = NULL;
      int i;
      if (check_dynfs)
      {
         item = cJSON_GetObjectItem(root, pe->name);
         NODE_DBG("%s: ", pe->name);

         if (item == NULL)
         {
            NODE_DBG("null\n");
         } else {
            for (i = 0 ; i < cJSON_GetArraySize(item) ; i++)
            {
               cJSON * subitem = cJSON_GetArrayItem(item, i);
               if (subitem != NULL)
               {
                  switch (i)
                  {
                     case dynfs_flags: {
                        if ((int)f->flags != subitem->valueint)
                           { NODE_DBG("flags mismatch"); }
                     } break;
                     case dynfs_size: {
                        if ((int)f->size != subitem->valueint)
                           { NODE_DBG("size mismatch"); }
                     } break;
                     case dynfs_pix: {
                        if ((int)f->pix != subitem->valueint)
                           { NODE_DBG("pix mismatch"); }
                     } break;
                  }
                  NODE_DBG("%d, ", subitem->valueint);
               }
            }
            NODE_DBG("\n");
         }
      }
      else
      {
         NODE_DBG("make array\n");
         cJSON *array = cJSON_CreateArray();
         cJSON_AddNumberToObject(array,"flags",f->flags);
         cJSON_AddNumberToObject(array,"size",pe->size);
         cJSON_AddNumberToObject(array,"pix",pe->pix);
         cJSON_AddItemToObject(root, pe->name, array);
      }
      STAILQ_INSERT_HEAD(&fs_file_list, f, next);
   }
   fs_closedir(&d);

   if (check_dynfs)
   {
      goto end;
   }

   int len, ret;

   char * json_string = cJSON_PrintUnformatted(root);
   len = strlen(json_string);
   NODE_DBG("cjson wrote %d\n", len);

   ret = dynfs_write(dyn_fd,json_string, len);
   if (ret < 0) { NODE_ERR("dynfs write err %i\n", dynfs_errno()); }
   if (ret >= 0) { NODE_DBG("dyn Wrote %d bytes\n", ret); }


   dynfs_close(dyn_fd);
   os_free(json_string);
   cJSON_Delete(root);
   return;

end:
   dynfs_close(dyn_fd);
   cJSON_Delete(root);
}

// bool fs_exists(char * name)
// {
//    NULL_CHECK(name, DO_RETURN, false);
//
//    struct fs_file_st *fs_st = STAILQ_FIRST(&fs_file_list);
//    STAILQ_FOREACH(fs_st, &fs_file_list, next)
//    {
//       if ((strstr(fs_st->name,name)) != NULL)
//       {
//          return true;
//       }
//    }
//    fs_st = STAILQ_FIRST(&fs_file_list);
//    return NULL;
// }
int fs_exists(char * name, struct fs_file_st ** fs_st_ptr)
{
   NULL_CHECK(name, DO_RETURN, -1);
   // NULL_CHECK(fs_st_ptr, DO_RETURN, -2);

   struct fs_file_st *fs_st = STAILQ_FIRST(&fs_file_list);
   STAILQ_FOREACH(fs_st, &fs_file_list, next)
   {
      if ((strstr(fs_st->name,name)) != NULL)
      {
         // return fs_st;
         *fs_st_ptr = fs_st;
         return 1;
      }
   }
   fs_st = STAILQ_FIRST(&fs_file_list);
   return -1;
}

// struct fs_file_st * fs_exists(char * name)
// {
//    NULL_CHECK(name, DO_RETURN, NULL);
//
//    struct fs_file_st *fs_st = STAILQ_FIRST(&fs_file_list);
//    STAILQ_FOREACH(fs_st, &fs_file_list, next)
//    {
//       if ((strstr(fs_st->name,name)) != NULL)
//       {
//          return fs_st;
//       }
//    }
//    fs_st = STAILQ_FIRST(&fs_file_list);
//    return NULL;
// }

int fs_read_file(struct fs_file_st * fs_st, void * buff, uint16_t len, uint32_t pos)
{
   int fd = -1;
   int res = -1;
   NULL_CHECK(fs_st, DO_RETURN, -1);

   int fsNo = 0;
   GET_FS(fs_st, fsNo);
   NEG_CHECK(fsNo, DO_RETURN, -1);

   if (!(fs_st->flags & MARK_BIG))
   {
      // fd = myfs_openpage(fsNo, fs_st->pix, FS_RDWR);
      fd = myfs_openname(fsNo, fs_st->name, FS_RDWR);
   } else {
      fd = myfs_openname(fsNo, fs_st->name, FS_RDWR);
   }
   NEG_CHECK(fd, DO_RETURN, -1);

   if (fs_st->size > 0)
   {
      res = myfs_seek(fsNo, fd, pos, FS_SEEK_SET);
      CLOSE_AND_RETURN(fsNo, fd, res);
   } else {
      NODE_ERR("Zero length file?\n");
      CLOSE_AND_RETURN(fsNo, fd, -1);
   }

   res = myfs_read(fsNo, fd, buff, len);
   CLOSE_AND_RETURN(fsNo, fd, res);
   // NEG_CHECK(res, DO_RETURN, -1);

   myfs_close(fsNo, fd);

   return res;
}

int fs_rename_file(uint8_t fs, char * old, char * new)
{
   struct fs_file_st * fs_st = NULL;
   int ret;
   NULL_CHECK(old, DO_RETURN, -1);
   NULL_CHECK(new, DO_RETURN, -1);

   ret = fs_exists(old, &fs_st);
   NULL_CHECK(fs_st, DO_RETURN, -1);

   int fsNo = 0;
   GET_FS(fs_st, fsNo);
   NEG_CHECK(fsNo, DO_RETURN, -1);

   ret = myfs_rename(fsNo, old, new);
   NEG_CHECK(ret, DO_RETURN, -1);

   os_free(fs_st->name);
   ret = strlen(new);

   fs_st->name = (char*)os_zalloc(sizeof(char)*ret+1);
   memcpy(fs_st->name, new, ret+1);

   return 1;
}

int fs_new_file(struct fs_file_st ** fs_st_ptr, char * filename, bool overwrite)
{
   NODE_DBG("fs_new_file\n");
   NULL_CHECK(filename, DO_RETURN, NULL);
   NODE_DBG("filename %s\n", filename);
   struct fs_file_st * fs_st = NULL;
   int fd = -1;
   spiffs_stat s;
   int ret = -1;
   int queue_res = -1;
   int flags = (FS_RDWR | FS_CREAT);

   int fsNo = FS1;
   // GET_FS(fs_st, fsNo);
   // NEG_CHECK(fsNo, DO_RETURN, -1);

   /* If file exists, make sure we can overwrite */
   // fs_st = fs_exists(filename);
   ret = fs_exists(filename, &fs_st);
   NODE_DBG("fs_exists ret = %d\n", ret);
   // NULL_CHECK(fs_st, DO_RETURN, -1);
   if (ret == 1)
   {
      NODE_DBG("file exists, size = %d\n", fs_st->size);
      if (!(overwrite)) { return NULL; }

      if (fs_st->size > 0) { flags |= FS_TRUNC; fs_st->size = 0; }
      fd = myfs_openname(fsNo, filename, flags);
      // fd = myfs_openpage(fsNo, fs_st->pix, flags);
      /* Check that file is not read only */
      // RDONLY_CHECK(fs_st, DO_RETURN, ret);
   } else {
      NODE_DBG("file DNE\n");
      fd = myfs_openname(fsNo, filename, flags);
   }

   /* Don't cache if we have data to write. also check that we have data */
   // if (len)
   // {
   //    NULL_CHECK(data, DO_RETURN, -1);
   //    flags |= FS_DIRECT;
   // }
   NODE_DBG("myfs_creat fd = %d\n", fd);

   // fd = myfs_openname(fsNo, filename, flags);
   // fd = myfs_creat(fsNo, filename);
   NEG_CHECK(fd, DO_ERRNO, NULL);
   NODE_DBG("file create ok\n");

   // if (len)
   // {
   //    ret = myfs_write(fsNo, fd, data, len);
   // } else {
   //    ret = 0; //indicates file successfully made
   // }

   queue_res = myfs_fstat(fsNo, fd, &s);
   NODE_DBG("queue_res = %d\n", queue_res);
   if (queue_res >= 0) { NODE_DBG("stat size = %d, stat pix = %d\n", s.size, s.pix); }

   if (queue_res >= 0)
   {
      if (fs_st == NULL)
      {
         fs_st = (struct fs_file_st *)os_malloc(sizeof(struct fs_file_st));
         int size = strlen(filename);
         fs_st->name = (char*)os_zalloc(sizeof(char)*size+1);
         strncpy(fs_st->name, filename, size);
         fs_st->name[size] = '\0';
         fs_st->flags = 0;
         fs_st->flags |= MARK_DYNFS;
         STAILQ_INSERT_HEAD(&fs_file_list, fs_st, next);
      }

      fs_st->pix = s.pix;
      fs_st->size = 0;
      // fs_st->flags = (1 << (fsNo+3));
      NODE_DBG("fs_st values updated: pix = %d\n", fs_st->pix);
   } else {
      NODE_DBG("error adding to fs_st queue\n");
   }
   myfs_close(fsNo, fd);

   *fs_st_ptr = fs_st;
   // return fs_st;
   return 1;
}

uint32_t fs_get_avail(int fs)
{
   int res = -1;
   uint32_t used, total;
   res = myfs_info(fs, &total, &used);
   NEG_CHECK(res, DO_RETURN, -1);
   return (total - used);
}

int fs_append_to_file(struct fs_file_st * fs_st,  void * data, uint16_t len)
{
   int fd = -1;
   int res = -1;

   NULL_CHECK(len, DO_RETURN, res);
   NULL_CHECK(data, DO_RETURN, res);
   NULL_CHECK(fs_st, DO_RETURN, res);
   // RDONLY_CHECK(fs_st, DO_RETURN, res);

   if (fs_st->size + len > 65000)
   {
      fs_st->flags |= MARK_BIG;
   }

   int fsNo = FS1;
   // GET_FS(fs_st, fsNo);
   // NEG_CHECK(fsNo, DO_RETURN, -1);
   // NODE_DBG("fs_append: st->pix = %d\n", fs_st->pix);
   // fd = myfs_openpage(fsNo, fs_st->pix, (FS_RDWR | FS_APPEND));
   fd = myfs_openname(fsNo, fs_st->name, (FS_RDWR | FS_APPEND));
   if (fd < 0)
   {
      NODE_DBG("open err: ");
      DO_ERRNO(-1);
   }
   // NEG_CHECK(fd, DO_ERRNO, -1);

   res = myfs_write(fsNo, fd, data, len);


   myfs_close(fsNo, fd);
   if (res > 0) { fs_st->size += res; }
   else { NODE_DBG("write err: "); DO_ERRNO(-1); }

   return res;
}

int fs_update_pix(struct fs_file_st * fs_st)
{
   spiffs_stat s;
   int stat_res = -1;
   int fsNo = FS1;
   int fd = myfs_openname(fsNo, fs_st->name, FS_RDWR);
   NEG_CHECK(fd, DO_ERRNO, -1);

   stat_res = myfs_fstat(fsNo, fd, &s);
   NODE_DBG("append fstat res = %d\n", stat_res);
   if (stat_res >= 0)
   {
      NODE_DBG("stat size = %d, stat pix = %d\n", s.size, s.pix);
      fs_st->size = s.size;
      fs_st->pix = s.pix;
   }
}


void fs_update_list(void)
{
   struct fs_file_st *fs_st = STAILQ_FIRST(&fs_file_list);
   STAILQ_FOREACH(fs_st, &fs_file_list, next)
   {
      if (fs_st->flags & MARK_DELETE)
      {
         STAILQ_REMOVE(&fs_file_list, fs_st, fs_file_st, next);
         os_free(fs_st->name);
         os_free(fs_st);
         break;
      }
   }
   fs_st = STAILQ_FIRST(&fs_file_list);
}

int fs_remove_by_name(char * filename)
{
   int res = -1;
   int fd = -1;
   NULL_CHECK(filename, DO_RETURN, res);
   struct fs_file_st * fs_st = NULL;

   res = fs_exists(filename, &fs_st);
   NODE_DBG("remove exists res=  %d\n", res);
   NULL_CHECK(fs_st, DO_RETURN, res);
   // struct fs_file_st *fs_st = fs_exists(filename);
   // NULL_CHECK(fs_st, DO_RETURN, res);
   // RDONLY_CHECK(fs_st, DO_RETURN, res);

   int fsNo = FS1;
   // GET_FS(fs_st, fsNo);
   // NEG_CHECK(fsNo, DO_RETURN, res);

   fd = myfs_openname(fsNo, fs_st->name, FS_RDWR);
   NEG_CHECK(fd, DO_RETURN, -1);
   NODE_DBG("remove exists fd =  %d\n", fd);

   res = myfs_fremove(fsNo, fd);
   NODE_DBG("remove res=  %d\n", res);
   NEG_CHECK(fd, DO_RETURN, myfs_errno(fsNo));

   if ((res >= 0) || (res == -10002))
   {
      STAILQ_REMOVE(&fs_file_list, fs_st, fs_file_st, next);
      os_free(fs_st->name);
      os_free(fs_st);
      res = 1;
   }

   return res;
}

int fsJSON_add_item(cJSON * arr, char * name, char * path, int size)
{
	char temp_path[32];
	cJSON * file_obj = cJSON_CreateObject();

	if (arr)
	{
		cJSON_AddStringToObject(file_obj, FS_JNAME, name);
		cJSON_AddStringToObject(file_obj, FS_JTYPE, FS_JFILE);
		if ((path) && (name))
		{
			os_sprintf(temp_path, "%s/%s", path, name);
			cJSON_AddStringToObject(file_obj, FS_JPATH, temp_path);
		}
		cJSON_AddNumberToObject(file_obj, FS_JSIZE, size);
		cJSON_AddItemToArray(arr, file_obj);
		return 1;
	}
	return 0;
}

//returns pointer to new folder items
cJSON * fsJSON_add_folder(cJSON * parent, char * foldername, char * parentpath)
{
	char temp_path[64];
	cJSON * folder_obj = cJSON_CreateObject();
	cJSON * folder_arr = NULL;

	if (parent)
	{
		cJSON_AddStringToObject(folder_obj, FS_JNAME, foldername);
		cJSON_AddStringToObject(folder_obj, FS_JTYPE, FS_JFOLDER);
		if ((parentpath) && (foldername))
		{
         os_sprintf(temp_path, "%s/%s", parentpath, foldername);
			cJSON_AddStringToObject(folder_obj, FS_JPATH, temp_path);
		}
		folder_arr = cJSON_CreateArray();
		cJSON_AddItemToObject(folder_obj, FS_JITEMS, folder_arr );
		cJSON_AddItemToArray(parent, folder_obj);
		return folder_arr;
	}
	return NULL;
}

cJSON * fsJSON_get_list()
{
   /* Init JSON Structure */
   cJSON * fs_root = cJSON_CreateObject();
	cJSON_AddStringToObject(fs_root, FS_JNAME, FS_JFILES); 	   //root name
	cJSON_AddStringToObject(fs_root, FS_JTYPE, FS_JFOLDER); 	//root type
	cJSON_AddStringToObject(fs_root, FS_JPATH, FS_JFILES); 		//root path

   cJSON * fs_array = cJSON_CreateArray();

   struct fs_file_st *f_st = STAILQ_FIRST(&fs_file_list);
   STAILQ_FOREACH(f_st, &fs_file_list, next)
   {
      if ((fsJSON_add_item(fs_array, f_st->name, FS_JFILES, f_st->size)) < 1)
      {
         NODE_DBG("err on:  %s\n", f_st->name);
      }
   }
   f_st = STAILQ_FIRST(&fs_file_list);

   cJSON_AddItemToObject(fs_root, FS_JITEMS, fs_array); 	//name
   // char * json_string = cJSON_PrintUnformatted(fs_root);
   // int len = strlen(json_string);

   if (fs_root)
   {
      return fs_root;
   }

   return NULL;
}
