// first includes global, second local config.h
// it should either be merged or get
// separate names
#include <config.h>
#include "pv_config.h"
#ifdef FBV_SUPPORT_JPEG

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#if __cplusplus >= 201103
#include <cmath>
#endif
#include <setjmp.h>

#include <global.h>
#include "pictureviewer.h"
#include "picv_client_server.h"

#undef HAVE_STDLIB_H // -Werror complain
extern "C" {
#include <jpeglib.h>
}

#define MIN(a,b) ((a)>(b)?(b):(a))

struct r_jpeg_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf envbuffer;
};


int fh_jpeg_id(const char *name)
{
//	dbout("fh_jpeg_id {\n");
	int fd;
	unsigned char id[10];
	fd=open(name,O_RDONLY); if(fd==-1) return(0);
	read(fd,id,10);
	close(fd);
//	 dbout("fh_jpeg_id }\n");
	if(id[6]=='J' && id[7]=='F' && id[8]=='I' && id[9]=='F')	return(1);
	if(id[0]==0xff && id[1]==0xd8 && id[2]==0xff) return(1);
	return(0);
}


void jpeg_cb_error_exit(j_common_ptr cinfo)
{
//	dbout("jpeg_cd_error_exit {\n");
	struct r_jpeg_error_mgr *mptr;
	mptr=(struct r_jpeg_error_mgr*) cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(mptr->envbuffer,1);
//	 dbout("jpeg_cd_error_exit }\n");
}

int fh_jpeg_load(const char *filename,unsigned char **buffer,int* x,int* y)
{
	//dbout("fh_jpeg_load_local (%s/%d/%d) {\n",basename(filename),*x,*y);
	struct jpeg_decompress_struct cinfo;
	struct jpeg_decompress_struct *ciptr;
	struct r_jpeg_error_mgr emgr;
	unsigned char *bp;
	int px,py,c;
	FILE *fh;
	JSAMPLE *lb;

	ciptr=&cinfo;
	if(!(fh=fopen(filename,"rb"))) return(FH_ERROR_FILE);
	ciptr->err=jpeg_std_error(&emgr.pub);
	emgr.pub.error_exit=jpeg_cb_error_exit;
	if(setjmp(emgr.envbuffer)==1)
	{
		// FATAL ERROR - Free the object and return...
		jpeg_destroy_decompress(ciptr);
		fclose(fh);
//	dbout("fh_jpeg_load } - FATAL ERROR\n");
		return(FH_ERROR_FORMAT);
	}

	jpeg_create_decompress(ciptr);
	jpeg_stdio_src(ciptr,fh);
	jpeg_read_header(ciptr,TRUE);
	ciptr->out_color_space=JCS_RGB;
	ciptr->dct_method=JDCT_FASTEST;
	if(*x==(int)ciptr->image_width)
		ciptr->scale_denom=1;
#if __cplusplus < 201103
	else if(abs(*x*2 - ciptr->image_width) < 2)
		ciptr->scale_denom=2;
	else if(abs(*x*4 - ciptr->image_width) < 4)
		ciptr->scale_denom=4;
	else if(abs(*x*8 - ciptr->image_width) < 8)
		ciptr->scale_denom=8;
#else
	else if(std::abs(*x*2 - ciptr->image_width) < 2)
		ciptr->scale_denom=2;
	else if(std::abs(*x*4 - ciptr->image_width) < 4)
		ciptr->scale_denom=4;
	else if(std::abs(*x*8 - ciptr->image_width) < 8)
		ciptr->scale_denom=8;
#endif
	else
		ciptr->scale_denom=1;

	jpeg_start_decompress(ciptr);

	px=ciptr->output_width; py=ciptr->output_height;
	c=ciptr->output_components;
	if(px > *x || py > *y)
	{
		// pic act larger, e.g. because of not responding jpeg server
		free(*buffer);
		*buffer = (unsigned char*) malloc(px*py*3);
		*x = px;
		*y = py;
	}

	if(c==3)
	{
		lb=(JSAMPLE*)(*ciptr->mem->alloc_small)((j_common_ptr) ciptr,JPOOL_PERMANENT,c*px);
		bp=*buffer;
		while(ciptr->output_scanline < ciptr->output_height)
		{
			jpeg_read_scanlines(ciptr, &lb, 1);
			memmove(bp,lb,px*c);
			bp+=px*c;
		}                 

	}
	jpeg_finish_decompress(ciptr);
	jpeg_destroy_decompress(ciptr);
	fclose(fh);
	//dbout("fh_jpeg_load_local }\n");
	return(FH_ERROR_OK);
}

int fh_jpeg_getsize(const char *filename,int *x,int *y, int wanted_width, int wanted_height)
{
//	dbout("fh_jpeg_getsize {\n");
	struct jpeg_decompress_struct cinfo;
	struct r_jpeg_error_mgr emgr;

	FILE *fh;
	if(!(fh=fopen(filename,"rb"))) return(FH_ERROR_FILE);

	cinfo.err=jpeg_std_error(&emgr.pub);
	emgr.pub.error_exit=jpeg_cb_error_exit;
	if(setjmp(emgr.envbuffer)==1)
	{
		// FATAL ERROR - Free the object and return...
		jpeg_destroy_decompress(&cinfo);
		fclose(fh);
//	dbout("fh_jpeg_getsize } - FATAL ERROR\n");
		return(FH_ERROR_FORMAT);
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo,fh);
	jpeg_read_header(&cinfo,TRUE);
	cinfo.out_color_space=JCS_RGB;
	// should be more flexible...
	if((int)cinfo.image_width/8 >= wanted_width ||
      (int)cinfo.image_height/8 >= wanted_height)
		cinfo.scale_denom=8;
	else if((int)cinfo.image_width/4 >= wanted_width ||
      (int)cinfo.image_height/4 >= wanted_height)
		cinfo.scale_denom=4;
	else if((int)cinfo.image_width/2 >= wanted_width ||
           (int)cinfo.image_height/2 >= wanted_height)
		cinfo.scale_denom=2;
	else
		cinfo.scale_denom=1;

	jpeg_start_decompress(&cinfo);
	*x=cinfo.output_width; *y=cinfo.output_height;
	jpeg_destroy_decompress(&cinfo);
	fclose(fh);
//	 dbout("fh_jpeg_getsize }\n");
	return(FH_ERROR_OK);
}
#endif
