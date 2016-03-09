//#include <stdlib.h>
#include <dirent.h>
#include <spiffs.h>

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <arpa/inet.h>

#include "../../../../app/platform/config.h"

//Gzip
// If compiler complains about missing header, try running "sudo apt-get install zlib1g-dev"
// to install missing package.
#include <zlib.h>
#define FLAG_LASTFILE 			(1<<0)
#define FLAG_GZIP 				(1<<1)
#define COMPRESS_NONE 			0
#define COMPRESS_HEATSHRINK 	1

//Spiffy
// #define LOG_BLOCK_SIZE     	(64*1024)
// #define ERASE_BLOCK_SIZE   	(32*1024)
#define LOG_BLOCK_SIZE     	(8*1024)
#define ERASE_BLOCK_SIZE   	(4*1024)
#define SPI_FLASH_SEC_SIZE 	(4*1024)
#define LOG_PAGE_SIZE			256

#define DEFAULT_FOLDER   "../html_compressed"
#define DEFAULT_ROM_NAME "spiffy_rom.bin"
#define ROM_ERASE 0xFF

// #define DEFAULT_ROM_SIZE 0x40000
#define DEFAULT_ROM_SIZE 0x10000

static spiffs fs;
static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*8];
static u8_t spiffs_cache_buf[(LOG_PAGE_SIZE+32)*2];

#define S_DBG /*printf*/

#define MK_STR_STR(V) #V
#define MK_STR(V) MK_STR_STR(V)

static FILE *rom = 0;

#ifdef SPIFFY_DEBUG
#define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG
#endif

#define PRINT_OUT(...) fprintf(stderr, __VA_ARGS__)

#define MAGIC_NUM    0x1234

RFM_CONF_T rfmConf =
{
   1,       //uint8_t bridgeId;
   3,        //uint8_t deviceId;
   100,        //uint8_t netId;
   91,          //uint8_t freq;
   "Rfm69_EncryptKey\0",   // char[16] key
   {0,0,0},
	MAGIC_NUM,                   //magic
};

WIFI_CONF_T wifiConf =
{
   MK_STR(STA_SSID),
   MK_STR(STA_PASS),
	MAGIC_NUM           //magic
};

//Routines to convert host format to the endianness used in the xtensa
short htoxs(short in)
{
	char r[2];
	r[0]=in;
	r[1]=in>>8;
	return *((short *)r);
}

int htoxl(int in)
{
	unsigned char r[4];
	r[0]=in;
	r[1]=in>>8;
	r[2]=in>>16;
	r[3]=in>>24;
	return *((int *)r);
}

size_t compressGzip(char *in, int insize, char *out, int outsize, int level)
{
	z_stream stream;
	int zresult;

	stream.zalloc = Z_NULL;
	stream.zfree  = Z_NULL;
	stream.opaque = Z_NULL;
	stream.next_in = in;
	stream.avail_in = insize;
	stream.next_out = out;
	stream.avail_out = outsize;
	// 31 -> 15 window bits + 16 for gzip
	zresult = deflateInit2 (&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
	if (zresult != Z_OK)
	{
		DEBUG("DeflateInit2 failed with code %d\n", zresult);
		exit(1);
	}

	zresult = deflate(&stream, Z_FINISH);
	if (zresult != Z_STREAM_END)
	{
		DEBUG("Deflate failed with code %d\n", zresult);
		exit(1);
	}

	zresult = deflateEnd(&stream);
	if (zresult != Z_OK)
	{
		DEBUG("DeflateEnd failed with code %d\n", zresult);
		exit(1);
	}

	return stream.total_out;
}

char **gzipExtensions = NULL;

int shouldCompressGzip(char *name)
{
	char *ext = name + strlen(name);
	while (*ext != '.')
	{
		ext--;
		// no dot in file name -> no extension -> nothing to match against
		if (ext < name)
			{ return 0; }
	}
	ext++;
	int i = 0;
	while (gzipExtensions[i] != NULL)
	{
		if (strcmp(ext,gzipExtensions[i]) == 0)
			{ return 1; }
		i++;
	}

	return 0;
}

int parseGzipExtensions(char *input)
{
	char *token;
	char *extList = input;
	int count = 2; // one for first element, second for terminator

	// count elements
	while (*extList != 0)
	{
		if (*extList == ',') count++;
		extList++;
	}

	// split string
	extList = input;
	gzipExtensions = malloc(count * sizeof(char*));
	count = 0;
	token = strtok(extList, ",");
	while (token)
	{
		gzipExtensions[count++] = token;
		token = strtok(NULL, ",");
	}
	// terminate list
	gzipExtensions[count] = NULL;

	return 1;
}


/* ############################################################ *
 * ##################         SPIFFS           ################ *
 * ############################################################ */
void hexdump_mem(u8_t *b, u32_t len)
{
	int i;
	for (i = 0; i < len; i++) {
		S_DBG("%02x", *b++);
		if ((i % 16) == 15) S_DBG("\n");
		else if ((i % 16) == 7) S_DBG(" ");
	}
	if ((i % 16) != 0) S_DBG("\n");
}


static s32_t my_spiffs_read(u32_t addr, u32_t size, u8_t *dst)
{
	int res;

	if (fseek(rom, addr, SEEK_SET))
	{
		DEBUG("Unable to seek to %d.\n", addr);
		return SPIFFS_ERR_END_OF_OBJECT;
	}

	res = fread(dst, 1, size, rom);
	if (res != size)
	{
		DEBUG("Unable to read - tried to get %d bytes only got %d.\n", size, res);
		return SPIFFS_ERR_NOT_READABLE;
	}

	S_DBG("Read %d bytes from offset %d.\n", size, addr);
	hexdump_mem(dst, size);
	return SPIFFS_OK;
}

static int asd = 0;
static s32_t my_spiffs_write(u32_t addr, u32_t size, u8_t *src)
{
	if (fseek(rom, addr, SEEK_SET))
	{
		DEBUG("Unable to seek to %d.\n", addr);
		return SPIFFS_ERR_END_OF_OBJECT;
	}

	if (fwrite(src, 1, size, rom) != size)
	{
		DEBUG("Unable to write.\n");
		return SPIFFS_ERR_NOT_WRITABLE;
	}

	fflush(rom);
	S_DBG("Wrote %d bytes to offset %d.\n", size, addr);

	return SPIFFS_OK;
}


static s32_t my_spiffs_erase(u32_t addr, u32_t size)
{
	int i;

	if (fseek(rom, addr, SEEK_SET))
	{
		DEBUG("Unable to seek to %d.\n", addr);
		return SPIFFS_ERR_END_OF_OBJECT;
	}

	for (i = 0; i < size; i++)
	{
		if (fputc(ROM_ERASE, rom) == EOF)
		{
			DEBUG("Unable to write.\n");
			return SPIFFS_ERR_NOT_WRITABLE;
		}
	}

	fflush(rom);
	S_DBG("Erased %d bytes at offset 0x%06x.\n", (int)size, addr);

	return SPIFFS_OK;
}

int my_spiffs_mount(u32_t msize)
{
	spiffs_config cfg;
#if (SPIFFS_SINGLETON == 0)
	cfg.phys_size = msize;
	cfg.phys_addr = 0;

	cfg.phys_erase_block =  ERASE_BLOCK_SIZE;
	cfg.log_block_size =  LOG_BLOCK_SIZE;
	cfg.log_page_size = LOG_PAGE_SIZE;
#endif

	cfg.hal_read_f = my_spiffs_read;
	cfg.hal_write_f = my_spiffs_write;
	cfg.hal_erase_f = my_spiffs_erase;

	int res = SPIFFS_mount(&fs,
			&cfg,
			spiffs_work_buf,
			spiffs_fds,
			sizeof(spiffs_fds),
			spiffs_cache_buf,
			sizeof(spiffs_cache_buf),
			0);
	PRINT_OUT("Mount result: %d.\n", res);

	return res;
}

void my_spiffs_unmount()
{
	SPIFFS_unmount(&fs);
}

int my_spiffs_format()
{
	int res  = SPIFFS_format(&fs);
	DEBUG("Format result: %d.\n", res);
	return res;
}

int write_to_spiffs(char *fname, u8_t *data, int size)
{
	int ret = 0;
	spiffs_file fd = -1;

	fd = SPIFFS_open(&fs, fname, SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);

	if (fd < 0) { PRINT_OUT("Unable to open spiffs file '%s', error %d.\n", fname, fd); }
	else
	{
		if (SPIFFS_write(&fs, fd, (u8_t *)data, size) < SPIFFS_OK)
		{
			DEBUG("Unable to write to spiffs file '%s', errno %d.\n", fname, SPIFFS_errno(&fs));
		}
		else { ret = 1; }
	}

	if (fd >= 0)
	{
		SPIFFS_close(&fs, fd);
		PRINT_OUT("Closed spiffs file '%s'.\n", fname);
	}
	return ret;
}

int get_rom_size (const char *str)
{
	long val;

	// accept decimal or hex, but not octal
	if ((strlen(str) > 2) && (str[0] == '0') && (((str[1] == 'x')) || ((str[1] == 'X'))))
	{
		val = strtol(str, NULL, 16);
	}
	else { val = strtol(str, NULL, 10); }

	return (int)val;
}

int handleFile(int f, char *name, int compression, int level, char **compName, off_t *csizePtr)
{
	char *fdat, *cdat;
	off_t size, sizein, csize, padsize, fstart, cout;
	int cdiff = 0;

	int ret = 0;
	// u8_t *buff = 0;
	FILE *fp = 0;
	char *path = 0;
	// DEBUG("cptr = %d\n", (int*)csizePtr);

	//EspFsHeader h;
	int nameLen;
	int8_t flags = 0;

	fstart=lseek(f, 0, SEEK_SET);

	/* makespfs log.html = 2506 */
	size=lseek(f, 0, SEEK_END);

	int file_len =0;

	file_len = htoxl(size - fstart);
	// DEBUG("file_len = %d, file_len/4 = %d \n", file_len, (file_len%4));

	fdat=mmap(NULL, size, PROT_READ, MAP_PRIVATE, f, 0);
	if (fdat==MAP_FAILED)
	{
		perror("mmap");
		return 0;
	}
	int cx = 0;

	if (shouldCompressGzip(name))
	{
		csize = (size)*3;
		if (csize<100) // gzip has some headers that do not fit when trying to compress small files
			csize = 100; // enlarge buffer if this is the case

		cdat=malloc(csize);
		cout = compressGzip(fdat, size, cdat, csize, level);
		csize = cout;
		file_len = htoxl(cout);
		// DEBUG("cout_len = %d, cdiff = %d \n", file_len, cdiff);
		compression = COMPRESS_NONE;
		flags = FLAG_GZIP;

	}
	else if (compression==COMPRESS_NONE)
	{
		csize=size;
		cdat=fdat;
	}
	else
	{
		PRINT_OUT("Unknown compression - %d\n", compression);
		exit(1);
	}

	if (csize>size)
	{
		//Compressing enbiggened this file. Revert to uncompressed store.
		compression=COMPRESS_NONE;
		csize=size;
		cdat=fdat;
		flags=0;
	}
	// if (csize < 1000)
	// {
	// 	int gz = csize;
	// 	DEBUG("gzip csize = %d\n",csize);
	// 	for(gz=0; gz< csize; gz++)
	// 	{
	// 		DEBUG("%02x ", (u8_t)cdat[gz]);
	// 		if ((gz+1)%30 == 0)
	// 		{
	// 			DEBUG("\n");
	// 		}
	// 	}
	// 	DEBUG("\n---- end gz ----\n");
	// }

	if (write_to_spiffs(name, cdat, htoxl(csize)))
	{
		PRINT_OUT("Added '%s' to spiffs (%d bytes).\n", name, htoxl(csize));
		ret = 1;
	}

	munmap(fdat, size);

	if (compName != NULL)
	{
		if (compression==COMPRESS_NONE)
		{
			if (flags & FLAG_GZIP)
				{ *compName = "gzip"; }
			else
				{ *compName = "none"; }
		}
		else
			{ *compName = "unknown"; }
	}
  *csizePtr = csize;
	return (csize*100)/size;
}


int main(int argc, char **argv)
{
	int f, x;
	char fileName[1024];
	char *realName;
	struct stat statBuf;
	int serr;
	int rate;
	int err=0;
	int compType;  //default compression type - heatshrink
	int compLvl=-1;
	int res, ret = EXIT_SUCCESS;

	compType = Z_BEST_COMPRESSION;

	const char *sizestr;
	const char *folder;
	const char *romfile  = DEFAULT_ROM_NAME;
   #ifdef GZIP_COMPRESSION
   PRINT_OUT("GZIP_COMPRESSION defined == %d\n", GZIP_COMPRESSION);
   #else
   PRINT_OUT("GZIP_COMPRESSION is NOT defined\n");
   #endif

	int romsize;
	romsize = DEFAULT_ROM_SIZE;
	folder = DEFAULT_FOLDER;
	PRINT_OUT("\nSpiffy main.c begin\n");
	PRINT_OUT("Creating rom '%s' of size 0x%x (%d) bytes.\n", romfile, romsize, romsize);
	DEBUG(" > %s opened for writing...\n", DEFAULT_ROM_NAME);

	rom = fopen(romfile, "wb+");
	if (!rom)
	{
		PRINT_OUT("Unable to open file '%s' for writing.\n", romfile);
		exit(0);
	}

	DEBUG("Writing blanks...\n");
	int i;
	for (i = 0; i < romsize; i++)
	{
		fputc(ROM_ERASE, rom);
	}
	fflush(rom);

	DEBUG("Parsing args...\n");

	for (x=1; x<argc; x++)
	{
		if (strcmp(argv[x], "-c")==0 && argc>=x-2)
		{
			compType=atoi(argv[x+1]);
			x++;
		}
		else if (strcmp(argv[x], "-l")==0 && argc>=x-2)
		{
			compLvl=atoi(argv[x+1]);
			if (compLvl<1 || compLvl>9)
				{ err=1; }
			x++;
		}
		else if (strcmp(argv[x], "-g")==0 && argc>=x-2)
		{
			if (!parseGzipExtensions(argv[x+1]))
				{ err=1; }
			x++;
		}
		else { err=1; }
	}
	PRINT_OUT("Using GZIP compression\n");
	if (gzipExtensions == NULL)
	{
		parseGzipExtensions(strdup("html,css,js,png,ico,txt"));
	}
	if (err)
	{
		PRINT_OUT("%s - Program to create espfs images\n", argv[0]);
		PRINT_OUT("Usage: \nfind | %s [-c compressor] [-l compression_level] ", argv[0]);
		PRINT_OUT("[-g gzipped_extensions] ");
		PRINT_OUT("> out.espfs\n");
		PRINT_OUT("Compressors:\n");
		PRINT_OUT("0 - None(default)\n");
		PRINT_OUT("\nCompression level: 1 is worst but low RAM usage, higher is better compression \nbut uses more ram on decompression. -1 = compressors default.\n");
		PRINT_OUT("\nGzipped extensions: list of comma separated, case sensitive file extensions \nthat will be gzipped. Defaults to 'html,css,js'\n");
		exit(0);
	}

	if (!my_spiffs_mount(romsize))
	{
		my_spiffs_unmount();
	}

	if ((res =  my_spiffs_format()))
	{
		PRINT_OUT("Failed to format spiffs, error %d.\n", res);
		ret = EXIT_FAILURE;
	} else if ((res = my_spiffs_mount(romsize))) {
		PRINT_OUT("Failed to mount spiffs, error %d.\n", res);
		ret = EXIT_FAILURE;
	} else {
		while(fgets(fileName, sizeof(fileName), stdin))
		{
			//Kill off '\n' at the end
			fileName[strlen(fileName)-1]=0;
			//Only include files
			char *aptr;
			aptr = strstr(fileName, "spiffy_rom.bin");
			if (aptr != NULL)
				{ continue; }
			serr=stat(fileName, &statBuf);

			if ((serr==0) && S_ISREG(statBuf.st_mode))
			{
				//Strip off './' or '/' madness.
				realName=fileName;
				if (fileName[0]=='.') realName++;
				if (realName[0]=='/') realName++;

				// f=open(fileName, O_RDONLY);
				f=open(fileName, O_RDONLY);
				if (f>0)
				{
					char *compName = "unknown";
	        		off_t csize;
					rate=handleFile(f, realName, compType, compLvl, &compName, &csize);
					DEBUG("%-16s (%3d%%, %s, %4u bytes)\n", realName, rate, compName, (int)csize);
					close(f);
				} else {
					perror(fileName);
				}
			} else {  /*invalid file name*/
 				if (serr!=0)
					{ perror(fileName); }
			}
		} /*end fgets loops*/

		write_to_spiffs("rfm.conf",  (uint8_t*)&rfmConf,  sizeof(rfmConf));
		write_to_spiffs("wifi.conf", (uint8_t*)&wifiConf, sizeof(wifiConf));

	} /*end my_spiffs_mount*/

	/*
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

	spiffs_file fd;
	int nt = -1;
	int toRead;
	uint8_t httpBuff[1460];

	while (nt < 11)
	{
		nt++;
		// fd = SPIFFS_open_by_dirent(&fs, pe, SPIFFS_RDONLY, 0);
		fd = SPIFFS_open(&fs, t_file_list[6], SPIFFS_RDONLY, 0);
		if (fd < 0) { DEBUG("err opening file\n"); SPIFFS_close(&fs, fd); break; }
		DEBUG("fd 1st open = %d", (int)fd);

		toRead = 1460;

		// Seek to EOF
		res = SPIFFS_lseek(&fs, fd, 0, SPIFFS_SEEK_END);
		if (res < 0) { DEBUG("lseek err: %d\n", res); }
		// SPIFFS_close(&fs, fd);

		// re-open file for reading
		// fd = SPIFFS_open(&fs, t_file_list[nt], SPIFFS_RDONLY, 0);
		// if (fd < 0) { DEBUG("err opening file\n"); SPIFFS_close(&fs, fd); break; }
		// DEBUG("fd 2nd open = %d", (int)fd);

		// reset cursor
		ret = SPIFFS_lseek(&fs, fd, 0, SPIFFS_SEEK_SET);
		if (ret < 0) { DEBUG("lseek err: %d\n", ret); }

		if (res < 1460) { toRead = res; }
		DEBUG("Reading %d bytes from %s ...\n", toRead, t_file_list[nt]);

		ret = SPIFFS_read(&fs, fd, httpBuff, toRead);
		if (ret < 0) { SPIFFS_close(&fs, fd); DEBUG("err reading %d bytes\n", toRead); SPIFFS_close(&fs, fd); break; }

		DEBUG("httpBuff=\n");
		int x;
		for (x=0; x<toRead;x++)
		{
			DEBUG("%02x ", httpBuff[x]);
		}
		DEBUG("\nEOF\n");

      DEBUG("Closing file..\n");
		SPIFFS_close(&fs, fd);
		break;
	}

	*/


	// /*
		// char *myp;

		// spiffs_DIR d;
		// spiffs_file fd2;
		// struct spiffs_dirent e;
		// struct spiffs_dirent *pe = &e;
		// SPIFFS_opendir(&fs, "/", &d);
		// while ((pe = SPIFFS_readdir(&d, pe))) {
		// res = SPIFFS_lseek(&fs, fd, 0, SPIFFS_SEEK_END);
		// if (res<0)
		// {
		// 	int testalign = (res%4);
		// 	DEBUG("SPIFFS_lseek end=%d, aligned = %d\n",res, testalign);
		// 	res = SPIFFS_lseek(&fs, fd, 0, SPIFFS_SEEK_SET);
		// }
		//
		// res = SPIFFS_eof(&fs, fd);
		// if (res<0) {
		// 	DEBUG("SPIFFS_eof returned %d\n",res);
		// }

		// res = SPIFFS_tell(&fs, fd);
		// DEBUG("SPIFFS_tel returned %d\n",res);
			// myp = (char*)strstr(pe->name,"html");
			// int z = 0;
			// if (myp != NULL)
			// {
				// fd2 = SPIFFS_open(&fs,pe->name, SPIFFS_RDONLY, 0);
				// if (fd2 >= 0) {
				// 	DEBUG("opened index.html. size = %d\n", pe->size);
				// } else {
				// 	DEBUG("err = %d\n", (int) fd2);
				// 	break;
				// }
				// uint8_t buffer[836];
				// buffer[836] = 0;
				// int res = 0;
				// res = SPIFFS_lseek(&fs, fd2, -1, SPIFFS_SEEK_END);
				// if (res<0) {
				// 	DEBUG("SPIFFS_lseek returned %d\n",res);
				// }
				// // res = SPIFFS_lseek(&fs, fd2, 0, SPIFFS_SEEK_CUR);
				// // res = SPIFFS_read(&fs, fd2, buffer, 836);
				// // DEBUG("read returned %d\n DATA = \n", res);
				// // for (z=0;z<pe->size;z++) {
				// 	// DEBUG("%02x ", buffer[z]);
				// // }
				// DEBUG("\nDATA END\n");
				// // break;
				// }

		// SPIFFS_close(&fs, fd);
	// }
	// */
	// my_spiffs_unmount();

	if (rom) fclose(rom);
	if (ret == EXIT_FAILURE) unlink(romfile);
	exit(EXIT_SUCCESS);
return 0;
}
