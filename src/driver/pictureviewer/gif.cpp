#include "pv_config.h"

#include <cstring>
#include <cstdlib>

#ifdef FBV_SUPPORT_GIF
	#include "pictureviewer.h"
	#include <stdio.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <setjmp.h>
extern "C" {
#include <gif_lib.h>
}
	#include <signal.h>
	#define min(a,b) ((a) < (b) ? (a) : (b))
	#define gflush return(FH_ERROR_FILE);
	#define grflush { DGifCloseFile(gft); return(FH_ERROR_FORMAT); }
	#define mgrflush { free(lb); free(slb); DGifCloseFile(gft); return(FH_ERROR_FORMAT); }
	#define agflush return(FH_ERROR_FORMAT);
	#define agrflush { DGifCloseFile(gft); return(FH_ERROR_FORMAT); }


int fh_gif_id(const char *name)
{
	int fd;
	char id[4];
	fd=open(name,O_RDONLY); if(fd==-1) return(0);
	read(fd,id,4);
	close(fd);
	if(id[0]=='G' && id[1]=='I' && id[2]=='F') return(1);
	return(0);
}

inline void m_rend_gif_decodecolormap(unsigned char *cmb,unsigned char *rgbb,ColorMapObject *cm,int /*s*/,int l)
{
	GifColorType *cmentry;
	int i;
	for(i=0;i<l;i++)
	{
		cmentry=&cm->Colors[cmb[i]];
		*(rgbb++)=cmentry->Red;
		*(rgbb++)=cmentry->Green;
		*(rgbb++)=cmentry->Blue;
	}
}
int fh_gif_load(const char *name,unsigned char **buffer,int* /*xp*/,int* /*yp*/)
{
	int px,py,i,ibxs;
	int j;
	unsigned char *fbptr;
	unsigned char *lb;
	unsigned char *slb;
	GifFileType *gft;
	GifByteType *extension;
	int extcode;
	GifRecordType rt;
	ColorMapObject *cmap;
	int cmaps;

	gft=DGifOpenFileName(name);
	if(gft==NULL) gflush;
	do
	{
		if(DGifGetRecordType(gft,&rt) == GIF_ERROR) grflush;
		switch(rt)
		{
			case IMAGE_DESC_RECORD_TYPE:

				if(DGifGetImageDesc(gft)==GIF_ERROR) grflush;
				px=gft->Image.Width;
				py=gft->Image.Height;
				lb=(unsigned char*)malloc(px*3);
				slb=(unsigned char*) malloc(px);
//		printf("reading...\n");
				if(lb!=NULL && slb!=NULL)
				{
					cmap=(gft->Image.ColorMap ? gft->Image.ColorMap : gft->SColorMap);
					cmaps=cmap->ColorCount;

					ibxs=ibxs*3;
					fbptr=*buffer;
					if(!(gft->Image.Interlace))
					{
						for(i=0;i<py;i++,fbptr+=px*3)
						{
							if(DGifGetLine(gft,slb,px)==GIF_ERROR)	mgrflush;
							m_rend_gif_decodecolormap(slb,lb,cmap,cmaps,px);
							memmove(fbptr,lb,px*3);
						}
					}
					else
					{
						for(j=0;j<4;j++)
						{
							fbptr=*buffer;
							for(i=0;i<py;i++,fbptr+=px*3)
							{
								if(DGifGetLine(gft,slb,px)==GIF_ERROR)	mgrflush;
								m_rend_gif_decodecolormap(slb,lb,cmap,cmaps,px);
								memmove(fbptr,lb,px*3);
							}
						}
					}
				}
				if(lb) free(lb);
				if(slb) free(slb);
				break;
			case EXTENSION_RECORD_TYPE:
				if(DGifGetExtension(gft,&extcode,&extension)==GIF_ERROR)	grflush;
				while(extension!=NULL)
					if(DGifGetExtensionNext(gft,&extension)==GIF_ERROR) grflush;
				break;
			default:
				break;
		}
	}
	while( rt!= TERMINATE_RECORD_TYPE );
	DGifCloseFile(gft);
	return(FH_ERROR_OK);
}
int fh_gif_getsize(const char *name,int *x,int *y, int /*wanted_width*/, int /*wanted_height*/)
{
	int px,py;
	GifFileType *gft;
	GifByteType *extension;
	int extcode;
	GifRecordType rt;

	gft=DGifOpenFileName(name);
	if(gft==NULL) gflush;
	do
	{
		if(DGifGetRecordType(gft,&rt) == GIF_ERROR) grflush;
		switch(rt)
		{
			case IMAGE_DESC_RECORD_TYPE:

				if(DGifGetImageDesc(gft)==GIF_ERROR) grflush;
				px=gft->Image.Width;
				py=gft->Image.Height;
				*x=px; *y=py;
				DGifCloseFile(gft);
				return(FH_ERROR_OK);
				break;
			case EXTENSION_RECORD_TYPE:
				if(DGifGetExtension(gft,&extcode,&extension)==GIF_ERROR)	grflush;
				while(extension!=NULL)
					if(DGifGetExtensionNext(gft,&extension)==GIF_ERROR) grflush;
				break;
			default:
				break;
		}
	}
	while( rt!= TERMINATE_RECORD_TYPE );
	DGifCloseFile(gft);
	return(FH_ERROR_FORMAT);
}
#endif
