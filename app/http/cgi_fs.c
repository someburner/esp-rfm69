/*
 * CGI for interacting with SPIFFS
 */
#include "user_interface.h"
#include "c_stdio.h"
#include "osapi.h"
#include "mem.h"
#include "platform.h"
#include "json/cJSON.h"
#include "flash_fs.h"
#include "rfm/radiohandler.h"

#include "cgi.h"
#include "http.h"
#include "http_parser.h"
#include "http_server.h"
#include "http_process.h"
#include "http_helper.h"
#include "user_config.h"

#include "cgi.h"
#include "cgi_fs.h"

#define CRLF						"\r\n"
#define CRLF2						"\r\n\r\n"
#define FILENAME_START			"filename=\""
#define FILENAME_END				"\"\r\n"
#define BOUND_STRING				"oundary"
#define WEBKIT_STRING         "\r\n------"

#define CGI_API_COUNT	 6

static char * cgi_apis[] = { "list", "dl", "ul", "del", "ren", "fw" };

static http_callback fs_callbacks[CGI_API_COUNT] = {
	http_fs_api_list,					// list files in JSON format
	http_fs_api_download,			// download file
	http_fs_api_upload,				// upload file
	http_fs_api_delete,				// delete file
	http_fs_api_rename,				// rename file
	http_fs_api_fw						// fw update init
};

#define GOTO_LABEL(label) \
	goto label;

#define BAD_REQUEST(c) \
	http_response_BAD_REQUEST(c); \
	return HTTPD_CGI_DONE;

#define NULL_CHECK(ptr, on_err, err_arg) \
	if (!(ptr)) { \
		on_err(err_arg); \
	}

#define RES_CHECK(res, on_err, err_arg) \
   if ((res) < 0) { \
      fs_close(temp_fd); \
		on_err(err_arg); \
   }

#define FS_ERR_CHECK(result) \
   if ((result) < 0) { \
      NODE_ERR("cgi_fs errno: %d\n",fs_errno()); \
   }

int http_static_api(http_connection *c)
{
	HTTP_CGI_DBG("cgi_file_system:\n");

	int * par = (int*)c->cgi.argument;
	if (par==NULL) { *par = HTTP_CGI_ARG_FS0; }
	// NODE_DBG("static par = %d\n", par);

	cgi_fs_state *state = (cgi_fs_state *)c->cgi.data;

	int temp_fd = 0;
   int len, res;
	uint8_t buff[HTTP_BUFFER_SIZE];

	//wait for body end state HTTPD_STATE_HEADERS_END
	if(c->state < HTTPD_STATE_BODY_END)
		return HTTPD_CGI_NEXT_RULE;

   //First call to this cgi. Open the file so we can read it.
	if (state==NULL)
	{
		//there's no PATH on the url ( WTF? ), so not found
		if( ! c->url_parsed.field_set & (1<<UF_PATH) )
		{
			NODE_ERR("No PATH on url\n");
			return HTTPD_CGI_NOTFOUND;
		}

		char * path = http_url_get_path(c);
		CGI_DBG("path = %s\n", path);
		if (*path=='/') { path++; }

		struct fs_file_st *fs_st = NULL;

		res = fs_exists(path, &fs_st);
		NULL_CHECK(fs_st, GOTO_LABEL, badrequest);

      // struct fs_file_st *fs_st = fs_exists(path);
		// NULL_CHECK(fs_st, GOTO_LABEL, badrequest);
		CGI_DBG("File requested: %s\n", fs_st->name);

		/* prepare file by opening and setting cursor to 0 */
      temp_fd = fs_openpage(fs_st->pix, FS_RDONLY);
		RES_CHECK(temp_fd, BAD_REQUEST, c);
      res = fs_seek(temp_fd, 0, FS_SEEK_SET);
		RES_CHECK(temp_fd, BAD_REQUEST, c);

		/* create structs to hold http_cgi_fs data */
		CGI_SPIFFS_ENTRY *entry = (CGI_SPIFFS_ENTRY *)os_malloc(sizeof(CGI_SPIFFS_ENTRY));
		CGI_FILE_STATE_T *f = (CGI_FILE_STATE_T*)os_malloc(sizeof(CGI_FILE_STATE_T));

      entry->name = fs_st->name;
      entry->pix =  fs_st->pix; //assign pix
      entry->size = fs_st->size;
      entry->flags = fs_st->flags; //file-specific flags

      f->readPos = 0;
      f->fs_flags = 0;	//file-system flags (i.e. which FS)
      f->file = entry;

		//save state for next time
		state = (cgi_fs_state*)os_zalloc(sizeof(cgi_fs_state));
		state->f = f;
		c->cgi.data=state;

      //close file
      fs_close(temp_fd);

		//set headers
		http_SET_HEADER(c,HTTP_CONTENT_TYPE,http_get_mime(c->url));
		http_SET_HEADER(c,HTTP_CACHE_CONTROL,HTTP_DEFAULT_CACHE);
		http_SET_CONTENT_LENGTH(c,f->file->size);
		if ((f->file->flags) & MARK_GZIP)
         { http_SET_HEADER(c,HTTP_CONTENT_ENCODING,"gzip"); }

      //send response
		http_response_OK(c);
		return HTTPD_CGI_MORE;
	}
	else //file found, transmit data
	{
		if ((state->f==NULL) || (state->f->fs_flags & CGI_FS_EOF) || (state->f->file->pix < 1)) { goto eof; }
		len = HTTP_BUFFER_SIZE;

      if (temp_fd > 0)
      {
         NODE_DBG("\tcurr temp_fd is %d ???\n", temp_fd);
         fs_close(temp_fd);
      }

      temp_fd = fs_openpage(state->f->file->pix, FS_RDONLY);
      RES_CHECK(temp_fd, GOTO_LABEL, badrequest);

		int remLen = state->f->file->size - state->f->readPos;
		if (remLen < len ) { len = remLen; }

      if (state->f->readPos > 0)
      {
         res = fs_seek(temp_fd, 0, FS_SEEK_SET);
         RES_CHECK(res, GOTO_LABEL, badrequest);
      }

		res = fs_tell(temp_fd);
		RES_CHECK(res, GOTO_LABEL, badrequest);

		if (res != state->f->readPos)
		{
			CGI_FS_DBG("\tfs_tell got %d\n", res);
			res = fs_seek(temp_fd, state->f->readPos, FS_SEEK_SET); // Don't move cursor now (waiting seek)
			RES_CHECK(res, GOTO_LABEL, badrequest);
		}

      CGI_FS_DBG("cgi_fs_read: %d bytes\n", len);
		res = fs_read(temp_fd, buff, len);
		RES_CHECK(res, GOTO_LABEL, badrequest);

		if (http_nwrite(c,(const char*)buff,len) == 1)
		{
			state->f->readPos += len;
         CGI_DBG("sent %d bytes [%d/%d]\n", res, state->f->readPos, state->f->file->size);

			if (state->f->readPos >= state->f->file->size) {state->f->fs_flags |= CGI_FS_EOF; goto eof;}
		}

      fs_close(temp_fd);
      temp_fd = 0;
		return HTTPD_CGI_MORE;
	}

eof:
   CGI_DBG("EOF: closing.");
   fs_close(temp_fd);
	if (state->f->file != NULL) { CGI_FS_DBG("state->f->file, "); os_free(state->f->file); }
	if (state->f != NULL) { CGI_FS_DBG("state->f, "); os_free(state->f); }
	if (state != NULL) { CGI_FS_DBG("state, "); os_free(state); }
	CGI_DBG("\ndone!\n");
	return HTTPD_CGI_DONE;

badrequest:
   CGI_DBG("Bad Request.");
	if (state != NULL) { goto eof; }
	return HTTPD_CGI_DONE;

end:
	return HTTPD_CGI_DONE;
}

int http_fs_api_rename(http_connection *c)
{
	NODE_DBG("http_fs_api_rename\n");
	return HTTPD_CGI_DONE;
}

int http_fs_api_delete(http_connection *c)
{
	CGI_DBG("http_fs_api_delete\n");
	int res;

	/* Obtain CGI data if available. */
	cgi_fs_state *state = (cgi_fs_state *)c->cgi.data;

	/* First call. Generate response headers */
	if (state->api_state == 1)
	{
		char *query=http_url_get_query_param(c,"file");		//get file name

		if (query)
		{
			res = fs_remove_by_name(query);
			NODE_DBG("http_fs_api: delete result = %d\n", res);
			if (res > 0)
			{
				http_response_OK(c);
				if (state != NULL) { os_free(state); }
				return HTTPD_CGI_DONE;
			}
		}
	}

badrequest:
	if (state != NULL) {
		os_free(state);
	}
	http_response_BAD_REQUEST(c);
	return HTTPD_CGI_DONE;
}

int http_fs_api_upload(http_connection *c)
{
	HTTP_CGI_DBG("http_fs_api: ul_cb\n");
	cJSON * root = NULL;

	/* Obtain CGI data if available. */
	cgi_fs_state *state = (cgi_fs_state *)c->cgi.data;

	// NULL_CHECK(state, GOTO_LABEL, badrequest);

	/* First call. Generate response headers */
	if (state->api_state == 1)
	{
		HTTP_CGI_DBG("ul_cb: first call\n");

		/* Get filename requested from query param */
		char *query=http_url_get_query_param(c,"file");		//get file name
		// NULL_CHECK(query, GOTO_LABEL, badrequest);

		char *size=http_url_get_query_param(c,"size");		//get file size
		if (size)
		{
			root = cJSON_CreateObject();
			http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);

			HTTP_CGI_DBG("size = %s\n", size);
			uint32_t avail = fs_get_avail(FS1);
			HTTP_CGI_DBG("avail = %u\n", avail);

			if ((uint32_t)atoi(size) < avail)
			{
				cJSON_AddNumberToObject(root, "fs_ok", 1);
				NODE_DBG("fs upload init\n");
			} else {
				cJSON_AddNumberToObject(root, "fs_ok", 0);
			}
			http_write_json(c,root);
			//delete json struct
			cJSON_Delete(root);
			os_free(state);
			// http_response_OK(c);
			return HTTPD_CGI_DONE;
		}
	}

	http_response_BAD_REQUEST(c);
	return HTTPD_CGI_DONE;

badrequest:
	if (state != NULL) {
		os_free(state);
	}
	http_response_BAD_REQUEST(c);
	return HTTPD_CGI_DONE;
}

/* *******************************************************************
 * Parsing methods:
 * 1. In first POST we grab the webkitboundary key and the filename
 * 2. For each subsequent packet we look for '--' and match it against
 *    our known key
 * See RFC 1867: http://www.rfc-editor.org/rfc/rfc1867.txt
 * ******************************************************************* */
int http_fs_api_uploadfile(http_connection *c)
{
	HTTP_CGI_DBG("http_fs_api_uploadfile\n");
	static struct fs_file_st * fs_st = NULL;
	char *b = NULL;
	int len, i;

	/* Obtain CGI data if available. */
	cgi_fs_state *state = (cgi_fs_state *)c->cgi.data;

	//first call, create status
	if (state==NULL)
	{
		HTTP_CGI_DBG("upload: state NULL\n");
		state = (cgi_fs_state*)os_zalloc(sizeof(cgi_fs_state));
		state->api_state=1;
		c->cgi.data=state;
		fs_st = NULL;

		/* Get filename requested from query param */
		char *query=http_url_get_query_param(c,"file");		//get file name
		HTTP_CGI_DBG("\tNULL: begin upload of %s\n", query);

		int ret = fs_new_file(&fs_st, query, true);
		if (ret)
		{
			HTTP_CGI_DBG("\tfs_st->name = %s\n", fs_st->name);
			HTTP_CGI_DBG("\tfs_st->size = %d\n", fs_st->size);
		} else {
			goto badrequest;
		}

		http_use_multipart_body(c);
		http_response_OK(c);
		return HTTPD_CGI_MORE;
	}
	else if (state->api_state == 1)
	{
		int res = -1;
		HTTP_CGI_DBG("upload: state 1\n");
		len = c->body.len;
		HTTP_CGI_DBG("\t1: len = %d fs_st->size = %d\n", len, fs_st->size);
		b = NULL;
		if (state->buff == NULL)
		{
			if (len > 0)
			{
				if ((b = getBoundaryStr(c->body.data)) != NULL)
					{ HTTP_CGI_DBG("Boundary = %s\n", b); state->buff = b; }
				else
					{ HTTP_CGI_DBG("coulnd't find boundary!\n"); return HTTPD_CGI_MORE;} // goto cleanup;
			}
		}

		if (state->buff != NULL)
		{

		}

		// else {
		// 	goto cleanup;
		// }

		// if (len > 0)
		// {
		// 	NODE_DBG("\t1: fs_append_to_file\n");
		// 	res = fs_append_to_file(fs_st, (char *)&c->body, len);
		// 	NODE_DBG("\t1: res = %d, fs_st->size = %d\n", res, fs_st->size);
		//
		// 	return HTTPD_CGI_MORE;
		// }
		// else { http_response_OK(c); return HTTPD_CGI_DONE; }
		return HTTPD_CGI_MORE;
	} else {
		return HTTPD_CGI_MORE;
	}

	return HTTPD_CGI_DONE;

cleanup:
	if (state != NULL) {
		HTTP_CGI_DBG("Freeing state\n");
		 if (state->buff != NULL) os_free(state->buff);
		os_free(state);
	}
	http_response_OK(c);
	return HTTPD_CGI_DONE;

badrequest:
	if (state != NULL) {
		os_free(state);
	}
	http_response_BAD_REQUEST(c);
	return HTTPD_CGI_DONE;
}

/* If get, return filesystem JSON structure */
int http_fs_api_list(http_connection *c)
{
	CGI_FS_DBG("http_fs_api_list\n");
	static cJSON * fs_json = NULL;

	/* Obtain CGI data if available. */
	cgi_fs_state *state = (cgi_fs_state *)c->cgi.data;

	NULL_CHECK(state, GOTO_LABEL, badrequest);
	CGI_FS_DBG("state->api_state = %d\n", state->api_state);

	/* First call. Generate response headers */
	if (state->api_state == 1)
	{
		NODE_DBG("fs get\n");

		/* Get cJSON filesystem structure from FS api wrapper */
		fs_json = fsJSON_get_list();
		NULL_CHECK(fs_json, GOTO_LABEL, badrequest);

		state->api_state = 2;

		http_SET_HEADER(c,HTTP_CONTENT_TYPE,JSON_CONTENT_TYPE);
		http_response_OK(c);

		return HTTPD_CGI_MORE;
	}

	/* Send file data here */
	else if (state->api_state == 2)
	{
		HTTP_CGI_DBG("fs get2\n");
		NULL_CHECK(fs_json, GOTO_LABEL, badrequest);
		http_write_json(c,fs_json);
		cJSON_Delete(fs_json);

		state->api_state = 3;
		return HTTPD_CGI_MORE;
	}
	HTTP_CGI_DBG("fs get3\n");

cleanup:
	if (state != NULL) {
		os_free(state);
	}
	return HTTPD_CGI_DONE;

badrequest:
	if (state != NULL) {
		os_free(state);
	}
	http_response_BAD_REQUEST(c);
	return HTTPD_CGI_DONE;
}

/* Download API:
 * Example usage: http://10.42.0.81/fs?file=wifi.html&action=dl
 */
int http_fs_api_download(http_connection *c)
{
	static struct fs_file_st * fs_st = NULL;
	int res = -1;
	CGI_FS_DBG("http_fs_api: download\n");

	/* Obtain CGI data if available. */
	cgi_fs_state *state = (cgi_fs_state *)c->cgi.data;

	NULL_CHECK(state, GOTO_LABEL, badrequest);
	CGI_FS_DBG("state->api_state = %d\n", state->api_state);

	/* First call. Generate response headers */
	if (state->api_state == 1)
	{
		char nameBuff[64]; //buffer for formatting HTTP filename suggestion
		CGI_FS_DBG("fs_api: First Call\n");

		/* Get filename requested from query param */
		char *query=http_url_get_query_param(c,"file");		//get file name
		NULL_CHECK(query, GOTO_LABEL, badrequest);

		/* Make sure file exists */
		res = fs_exists(query, &fs_st);
		CGI_FS_DBG("res = %d\n", res);

		NULL_CHECK(fs_st, GOTO_LABEL, badrequest);
		NODE_DBG("file exists: size = %u\n", fs_st->size);

		/* Generate attachment message for HTTP. Append .gz for compressed files */
		if ((fs_st->flags) & MARK_GZIP)
		{
			os_sprintf(nameBuff, "%s%s%s", HTTP_ATTACHMENT_FILENAME, fs_st->name, ".gz\"");
		} else {
			os_sprintf(nameBuff, "%s%s%s", HTTP_ATTACHMENT_FILENAME, fs_st->name, "\"");
		}
		http_SET_HEADER(c,HTTP_CONTENT_TYPE,FORCE_DL_TYPE);
		http_SET_HEADER(c,HTTP_CONTENT_DISP, nameBuff);
		http_SET_CONTENT_LENGTH(c, fs_st->size);

		/* create structs to hold http_cgi_fs data */
		CGI_FILE_STATE_T *f = (CGI_FILE_STATE_T*)os_malloc(sizeof(CGI_FILE_STATE_T));
		f->file = NULL;	//Only one at a time, no entry required here

		f->readPos  = 0;	//CGI read position, if multiple reads necessary
		f->fs_flags = 0;	//file-system flags (i.e. which FS)
		state->f = f;		//attach to state structure

		state->api_state = 2;

		//send headers
		http_response_OK(c);
		CGI_FS_DBG("cgi_fs_dl: response sent\n");
		return HTTPD_CGI_MORE;
	}
	/* Send file data here */
	else if (state->api_state == 2)
	{
		char buff[HTTP_BUFFER_SIZE];
		int len = HTTP_BUFFER_SIZE;
		res = -1;
		CGI_FS_DBG("cgi_fs_dl: state 2\n");

		/* Set len to size if less than HTTP_BUFFER_SIZE */
		if (fs_st->size < len)
			{ len = fs_st->size; }
		else if (fs_st->size - state->f->readPos < len)
			{ len = fs_st->size - state->f->readPos; }

		/* Read data into buffer */
		res = fs_read_file(fs_st, buff, len, state->f->readPos);

		// NODE_DBG("fs_read res: %d bytes from %s\n",res, fs_st->name);
		state->f->readPos += res;

		if (res < 0) { goto badrequest;}

		if (http_nwrite(c,(const char*)buff,res) == 1)
		{
			if (state->f->readPos == fs_st->size)
			{
				state->api_state = 3;
				goto cleanup;
			} else {
				return HTTPD_CGI_MORE;
			}
		}
	}

	return HTTPD_CGI_MORE;

cleanup:
	if (state != NULL) {
		// if (state->f->file != NULL) { os_free(state->f->file); }
		if (state->f != NULL) { os_free(state->f); }
		os_free(state);
	}
	http_response_OK(c);
	return HTTPD_CGI_DONE;

badrequest:
	if (state != NULL) {
		// if (state->f->file != NULL) { os_free(state->f->file); }
		if (state->f != NULL) { os_free(state->f); }
		os_free(state);
	}
	http_response_BAD_REQUEST(c);
	return HTTPD_CGI_DONE;
}

/* Initiate fw update for a given node ID and fw file */
int http_fs_api_fw(http_connection *c)
{
	CGI_DBG("http_fs_api_fw\n");

	/* Obtain CGI data if available. */
	cgi_fs_state *state = (cgi_fs_state *)c->cgi.data;
	NULL_CHECK(state, GOTO_LABEL, badrequest);

	/* First call. Generate response headers */
	if (state->api_state == 1)
	{
		/* Get filename requested from query param */
		char *query=http_url_get_query_param(c,"file");		//get file name
		NULL_CHECK(query, GOTO_LABEL, badrequest);
		CGI_DBG("file requested: %s\n", query);

		char *nid=http_url_get_query_param(c,"nid");		//get file size
		if (nid)
		{
			CGI_DBG("node id: %s\n", nid);

			rfm_begin_ota(query, atoi(nid));
			state->api_state = 2;

			http_response_OK(c);
			goto cleanup;
			// return HTTPD_CGI_DONE;
		} else {
			goto badrequest;
		}
	}


cleanup:
	if (state != NULL) {
		os_free(state);
	}
	return HTTPD_CGI_DONE;

badrequest:
	if (state != NULL) {
		os_free(state);
	}
	http_response_BAD_REQUEST(c);
	return HTTPD_CGI_DONE;
}


/* FS API:
 * Example usage (for file list): http://10.42.0.81/fs/get
 */
int http_fs_api(http_connection *c)
{
	CGI_DBG("http_fs_api:\n");
	/* Choose filesystem to use */
	// int *par = (int*)c->cgi.argument;
	// if (par!=NULL)
   // NODE_DBG("par = %d\n", *par);

	/* wait for whole body */
	if (c->state < HTTPD_STATE_BODY_END) { return HTTPD_CGI_MORE; }

	/* Obtain CGI data if available. */
	cgi_fs_state *state = (cgi_fs_state *)c->cgi.data;

	/* first call, determine request and send headers.*/
	if(state==NULL)
	{
		int ret = -1;

		/* Allocate memory to track CGI status, attach to connection */
		state = (cgi_fs_state*)os_zalloc(sizeof(cgi_fs_state));
		state->api_state=1;
		c->cgi.data=state;

		/* Determine URL path for get requests */
      char * path = http_url_get_path(c);
      CGI_DBG("path = %s\n", path);
      char * ptr = NULL;

		/* Perform file-specific FS api */
		CGI_FS_DBG("checking for fs API fn\n");
		ptr = NULL;
		unsigned i;

		/* All cgi FS APIs require a valid action */
		char *query=http_url_get_query_param(c,"file");
		char *action = http_url_get_query_param(c,"action"); //get action type
		NULL_CHECK(query, GOTO_LABEL, badrequest);
		NULL_CHECK(action, GOTO_LABEL, badrequest);
		NODE_DBG("query = %s\n", query);
		NODE_DBG("action = %s\n", action);

		/* Find corresponding FS API */
		for (i=0; i<CGI_API_COUNT; i++)
		{
			if ((ptr=strstr(cgi_apis[i], action)))
			{
				NODE_DBG("Found cgi action: %s\n", cgi_apis[i]);

				/* Attach to new CGI callback based on api type */
				c->cgi.function = NULL;
				c->cgi.function = fs_callbacks[i];
				CGI_DBG("New CGI attached\n");

				/* Execute once before returning as different commands will
				 *	warrant different response headers to the client.
				 *	Note: api_state should == 1 at this point
				*/
				int tr = fs_callbacks[i](c);
				CGI_DBG("fs api returned %d\n", tr);
				CGI_DBG("c->cgi.function new = %p\n", c->cgi.function);
				return tr;
			}
		}
		goto cleanup;
	} else {
		goto cleanup;
	}

cleanup:
	if (state != NULL) {
		if (state->f != NULL) {
			// if (state->f->file != NULL) { os_free(state->f->file); }
			os_free(state->f);
		}
		os_free(state);
	}
	return HTTPD_CGI_DONE;

badrequest:
	NODE_DBG("badrequest\n");
	http_response_BAD_REQUEST(c);
	if (state != NULL) {
		if (state->f != NULL) {
			if (state->f->file != NULL) { os_free(state->f->file); }
			os_free(state->f);
		}
		os_free(state);
	}
	return HTTPD_CGI_DONE;

	return HTTPD_CGI_DONE;
}

char * getBoundaryStr(char * buff)
{
	char * outStr = NULL;
	int start = 0;
	unsigned len = 0;

	char * bound = strstr(buff, BOUND_STRING);		// Search for 'oundary'
	char * lineBreak = strstr(buff, CRLF);	// Find first line-feed

	if ((bound != NULL) && (lineBreak != NULL))
	{
		start = (int)(lineBreak-bound);
		NODE_DBG("start = %d\n", start);
		if (start)
		{
			len = start - strlen(BOUND_STRING);
			outStr = (char*)os_malloc(len+3);
			NODE_DBG("len = %d\n", len);
			if (strncpy(outStr, buff+start+1, len))
				{
					outStr[len] = '-';
					outStr[len+1] = '-';
					outStr[len+2] = '\0';
					return outStr;
				}
		}
	}
	return outStr;
}

char * getFilename(char * buff)
{
	char * outStr = NULL;
	int diff = 0;
	unsigned len = 0;

	char * start = strstr(buff, FILENAME_START);	// Search for 'filename="'
	char * end = strstr(buff, FILENAME_END);		// Find 2nd quote and linefeed

	if ((start != NULL) && (end != NULL))
	{
		diff = (int)(end-start);					// total length of 'filename="abc.txt"'
		os_printf("diff = %d\n", diff);
		if (diff)
		{
			len = diff - strlen(FILENAME_START);	// length of filename only
			outStr = (char*)os_malloc(len+1);
			if (strncpy(outStr, end-diff+strlen(FILENAME_START), len))
				{ outStr[len] = '\0'; return outStr; }
		}
	}
	return outStr;
}

char * getDataBegin(char * buff)
{
	char * outStr = NULL;
	int diff = 0;
	unsigned len = 0;

	char * start = strstr(buff, HTTP_CONTENT_TYPE);	// Search for 'content type'
	char * end = NULL;


	if (start != NULL)
	{
		end = strstr(start, CRLF2);		// Find 2nd quote and linefeed
		if (end != NULL)
			{ return (end + 4); } 		// skip CRLF x 2
	}

	return NULL; 						// indicates invalid header
}

// returns location of end if end found, else -1
int checkForLastPkt(char * buff, char * boundStr)
{
	NODE_DBG("boundstr check = %s strlen = %d\n", boundStr, strlen(boundStr));
	char * outStr = NULL;
	int end = -1;

	char * found = strstr(buff, boundStr);	// Search for

	if (found != NULL)
	{
		char * b = strstr(buff, WEBKIT_STRING);
		if (b!=NULL)
		{
			end = (int) (b - buff);
		} else {
			NODE_DBG("b==NULL\n");
		}

	} else {
		NODE_DBG("didn't find end\n");
	}

	return end; 						// indicates invalid header
}
