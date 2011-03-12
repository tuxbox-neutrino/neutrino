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

#define BUFFERLEN 8192
int fh_jpeg_load_via_server(const char *filename,unsigned char *buffer,int x,int y)
{
	struct sockaddr_in si_other;
	int s, slen=sizeof(si_other);
	int bytes, port;
	char path[PICV_CLIENT_SERVER_PATHLEN];
	struct pic_data pd;

	strncpy(path, filename, PICV_CLIENT_SERVER_PATHLEN-1);
	path[PICV_CLIENT_SERVER_PATHLEN - 1] = 0;
	
	dbout("fh_jpeg_load_via_server (%s/%d/%d) {\n",basename(filename),x,y);
	if ((s=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))==-1)
	{
		perror("socket:");
		return(FH_ERROR_FILE);
	}
	port=atoi(g_settings.picviewer_decode_server_port);
	printf("Server %s [%d]\n",g_settings.picviewer_decode_server_ip.c_str(),port);
	
	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(port);
	if (inet_aton(g_settings.picviewer_decode_server_ip.c_str(), 
					  &si_other.sin_addr)==0)
	{
		fprintf(stderr, "inet_aton() failed\n");
		return(FH_ERROR_FILE);
	}
	if (connect(s, (struct sockaddr *) &si_other, slen)==-1)
	{
		perror("connect()");
		return(FH_ERROR_FILE);
	}			
	if (send(s, path, PICV_CLIENT_SERVER_PATHLEN, 0)==-1)
	{
		perror("send()");
		return(FH_ERROR_FILE);
	}
	pd.width=htonl(x);
	pd.height=htonl(y);
	if (send(s, &pd, sizeof(pd), 0)==-1)
	{
		perror("send()");
		return(FH_ERROR_FILE);
	}
	if (recv(s, &pd, sizeof(pd), 0) < (int)sizeof(pd))
	{
		perror("recv pic desc");
		return(FH_ERROR_FILE);
	}
	if ((int)ntohl(pd.width) != x || (int)ntohl(pd.height) != y)
	{
		fprintf(stderr,"ugh, decoded pic has wrong size [%d/%d]<>[%d/%d]\n", ntohl(pd.width), ntohl(pd.height), x, y);
		return(FH_ERROR_FILE);
	}
	unsigned char buf2[BUFFERLEN];
	unsigned char* workptr=buffer;
	int rest = x*y*2;
	while (rest > 0)
	{
		bytes=recv(s, buf2, MIN(BUFFERLEN,rest), 0);
		if(bytes % 2 ==1)
			bytes+=recv(s, buf2+bytes, 1, 0);

		for(int i=0 ; i < bytes ; i+=2)
		{
			*workptr = (buf2[i] >> 2) << 3;
			workptr++;
			*workptr = (buf2[i] << 6) | ((buf2[i+1] >> 5) << 3);
			workptr++;
			*workptr = (buf2[i+1] << 3);
			workptr++;
		}
		rest-=bytes;
	}
	dbout("fh_jpeg_load_via_server }\n");
	return(FH_ERROR_OK);
}
int fh_jpeg_load_local(const char *filename,unsigned char **buffer,int* x,int* y)
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
	else if(abs(*x*2 - ciptr->image_width) < 2)
		ciptr->scale_denom=2;
	else if(abs(*x*4 - ciptr->image_width) < 4)
		ciptr->scale_denom=4;
	else if(abs(*x*8 - ciptr->image_width) < 8)
		ciptr->scale_denom=8;
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

int fh_jpeg_load(const char *filename,unsigned char **buffer,int* x,int* y)
{
	int ret=FH_ERROR_FILE;
#if 1
	if(!g_settings.picviewer_decode_server_ip.empty())
		ret=fh_jpeg_load_via_server(filename, *buffer, *x, *y);
	if(ret!=FH_ERROR_OK)
#endif
		ret=fh_jpeg_load_local(filename, buffer, x, y);
	return ret;
}

int fh_jpeg_getsize(const char *filename,int *x,int *y, int wanted_width, int wanted_height)
{
//	dbout("fh_jpeg_getsize {\n");
	struct jpeg_decompress_struct cinfo;
	struct jpeg_decompress_struct *ciptr;
	struct r_jpeg_error_mgr emgr;

	int px,py,c;
	FILE *fh;
	ciptr=&cinfo;
	if(!(fh=fopen(filename,"rb"))) return(FH_ERROR_FILE);

	ciptr->err=jpeg_std_error(&emgr.pub);
	emgr.pub.error_exit=jpeg_cb_error_exit;
	if(setjmp(emgr.envbuffer)==1)
	{
		// FATAL ERROR - Free the object and return...
		jpeg_destroy_decompress(ciptr);
		fclose(fh);
//	dbout("fh_jpeg_getsize } - FATAL ERROR\n");
		return(FH_ERROR_FORMAT);
	}

	jpeg_create_decompress(ciptr);
	jpeg_stdio_src(ciptr,fh);
	jpeg_read_header(ciptr,TRUE);
	ciptr->out_color_space=JCS_RGB;
	// should be more flexible...
	if((int)ciptr->image_width/8 >= wanted_width ||
      (int)ciptr->image_height/8 >= wanted_height)
		ciptr->scale_denom=8;
	else if((int)ciptr->image_width/4 >= wanted_width ||
      (int)ciptr->image_height/4 >= wanted_height)
		ciptr->scale_denom=4;
	else if((int)ciptr->image_width/2 >= wanted_width ||
           (int)ciptr->image_height/2 >= wanted_height)
		ciptr->scale_denom=2;
	else
		ciptr->scale_denom=1;

	jpeg_start_decompress(ciptr);
	px=ciptr->output_width; py=ciptr->output_height;
	c=ciptr->output_components;
#if 1
	if(!g_settings.picviewer_decode_server_ip.empty())
	{
		// jpeg server resizes pic to desired size
		if( px > wanted_width || py > wanted_height)
		{
			if( (CPictureViewer::m_aspect_ratio_correction*py*wanted_width/px) <= wanted_height)
			{
				*x=wanted_width;
				*y=(int)(CPictureViewer::m_aspect_ratio_correction*py*wanted_width/px);
			}
			else
			{
				*x=(int)((1.0/CPictureViewer::m_aspect_ratio_correction)*px*wanted_height/py);
				*y=wanted_height;
			}
		}
		else if(CPictureViewer::m_aspect_ratio_correction >=1)
		{
			*x=px;
			*y=(int)(py/CPictureViewer::m_aspect_ratio_correction);
		}
		else
		{
			*x=(int)(px/CPictureViewer::m_aspect_ratio_correction);
			*y=py;
		}

	}
	else
#endif
	{
		*x=px; 
		*y=py;
	}
//	jpeg_finish_decompress(ciptr);
	jpeg_destroy_decompress(ciptr);
	fclose(fh);
//	 dbout("fh_jpeg_getsize }\n");
	return(FH_ERROR_OK);
}
#endif
