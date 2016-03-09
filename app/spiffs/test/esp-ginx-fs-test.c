const char *t_file_list[] =
{
	"index.html",
	"ui.js",
	"favicon.ico",
	"console.html",
	"console.js",
	"log.html",
	"wifi.html",
	"wifi.js",
	"icons.png",
	"style.css",
	"pure.css"
};

//put this in the heap timer or similar to check file output. prints bytes
   static int fsCheckCount = -1;

   fsCheckCount++;

   if ( fsCheckCount > 10) { return;}
   int i;
   int test_fd = fs_open(t_file_list[fsCheckCount], SPIFFS_RDONLY);

   if (test_fd < 0)
   {
      NODE_DBG("bad file? %s \n",t_file_list[fsCheckCount]);
      test_fd = 0;
   } else {
      int ret = -1;
		int res = 0;

      NODE_DBG(" %s OK \n",t_file_list[fsCheckCount]);

		//seek to eof
		res = fs_seek(test_fd, 0, FS_SEEK_END);
		if (res < 0) { fs_close(test_fd); }
		NODE_DBG("fs_seek res = %d\n", res);

		//get size
		int size = res;

		if (res < 1460)
		{
			size = res;
		}

		//reset cursor
		res = fs_seek(test_fd, 0, FS_SEEK_SET);
		if (res < 0) { fs_close(test_fd); test_fd = 0; NODE_DBG("\t\tres = %d\n", res); }
		fs_close(test_fd);

		int toRead = size;

		test_fd = fs_open(t_file_list[fsCheckCount], SPIFFS_RDONLY);
		if (test_fd < 0)
		{
			NODE_DBG("bad file? %s \n",t_file_list[fsCheckCount]);
			test_fd = 0;
			return;
		}

		while (toRead > 0)
		{
			int len = 1460;
			if (toRead < 1460)
			{
				len = toRead;
			}

			NODE_DBG("Read len = %d\n", len);
			// if (len%4 > 0) { len+= (4-(len%4)); }
			// NODE_DBG("Read fixed = %d\n", len);
			char * tbuff = (char*)os_zalloc(len+1);
			tbuff[len] = 0;

			//read
			ret = fs_read(test_fd, tbuff, len);
			if (ret < 0) { fs_close(test_fd); NODE_DBG("err reading 1st\n"); fs_close(test_fd); break; }
			NODE_DBG("Begin data:\n");
			for (i=0;i<len;i++)
			{
				os_printf_plus("%02x ", tbuff[i]);
				if ((i+1)%30 == 0) { NODE_DBG("\n"); }
			}
			NODE_DBG("\nTotal Read = %d\n", len);
			toRead -= len;

			os_free(tbuff);

			if (toRead < 4)
			{
				NODE_DBG("toRead = %d\n",toRead);
				break;
			}
		}

      NODE_DBG("\nDONE reading %s\n",t_file_list[fsCheckCount] );

      fs_close(test_fd);
   }
