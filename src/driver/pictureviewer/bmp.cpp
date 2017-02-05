#include <config.h>
#include "pv_config.h"
#ifdef FBV_SUPPORT_BMP
#include "pictureviewer.h"
#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BMP_TORASTER_OFFSET	10
#define BMP_SIZE_OFFSET		18
#define BMP_BPP_OFFSET		28
#define BMP_RLE_OFFSET		30
#define BMP_COLOR_OFFSET	54

#define fill4B(a)	( ( 4 - ( (a) % 4 ) ) & 0x03)

struct color {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};

int fh_bmp_id(const char *name)
{
	int fd;
	char id[2];

	fd = open(name, O_RDONLY);
	if (fd == -1) {
//		dbout("fh_bmp_id {\n");
		return(0);
	}

	read(fd, id, 2);
	close(fd);
	if ( id[0]=='B' && id[1]=='M' ) {
		return(1);
	}
	return(0);
}

void fetch_pallete(int fd, struct color pallete[], int count)
{
//	dbout("fetch_palette {\n");
	unsigned char buff[4];
	int i;

	lseek(fd, BMP_COLOR_OFFSET, SEEK_SET);
	for (i=0; i<count; i++) {
		read(fd, buff, 4);
		pallete[i].red = buff[2];
		pallete[i].green = buff[1];
		pallete[i].blue = buff[0];
	}
//	dbout("fetch_palette }\n");
	return;
}

int fh_bmp_load(const char *name,unsigned char **buffer,int* xp,int* yp)
{
//	dbout("fh_bmp_load {\n");
	int fd, bpp, raster, i, j, k, skip, x=*xp, y=*yp;
	unsigned char buff[4];
	unsigned char *wr_buffer = *buffer + x*(y-1)*3;
	struct color pallete[256];

	fd = open(name, O_RDONLY);
	if (fd == -1) {
		return(FH_ERROR_FILE);
	}

	if (lseek(fd, BMP_TORASTER_OFFSET, SEEK_SET) == -1) {
		close(fd);//Resource leak: fd
		return(FH_ERROR_FORMAT);
	}

	read(fd, buff, 4);
	raster = buff[0] + (buff[1]<<8) + (buff[2]<<16) + (buff[3]<<24);

	if (lseek(fd, BMP_BPP_OFFSET, SEEK_SET) == -1) {
		close(fd);
		return(FH_ERROR_FORMAT);
	}

	read(fd, buff, 2);
	bpp = buff[0] + (buff[1]<<8);

	switch (bpp){
		case 1: /* monochrome */
			skip = fill4B(x/8 + ((x%8) ? 1 : 0));
			lseek(fd, raster, SEEK_SET);
			{
				int bytes=x/8;
				if(x%8 >0)
					bytes++;
				unsigned char* tbuffer = (unsigned char*) malloc(bytes);
				if(tbuffer==NULL)
				{
					printf("Error: malloc\n");
					close(fd);
					return (FH_ERROR_MALLOC);
				}
				for (i=0; i<y; i++) {
					read(fd, tbuffer, bytes);
					for (j=0; j<x/8; j++) {
						for (k=0; k<8; k++) {
							if (tbuffer[j] & 0x80) {
								*wr_buffer++ = 0xff;
								*wr_buffer++ = 0xff;
								*wr_buffer++ = 0xff;
							} else {
								*wr_buffer++ = 0x00;
								*wr_buffer++ = 0x00;
								*wr_buffer++ = 0x00;
							}
							tbuffer[j] = tbuffer[j]<<1;
						}

					}
					if (x%8) {
						for (k=0; k<x%8; k++) {
							if (tbuffer[j] & 0x80) {
								*wr_buffer++ = 0xff;
								*wr_buffer++ = 0xff;
								*wr_buffer++ = 0xff;
							} else {
								*wr_buffer++ = 0x00;
								*wr_buffer++ = 0x00;
								*wr_buffer++ = 0x00;
							}
							tbuffer[j] = tbuffer[j]<<1;
						}

					}
					if (skip) {
						read(fd, tbuffer, skip);
					}
					wr_buffer -= x*6; /* backoff 2 lines - x*2 *3 */
				}
				free(tbuffer);
			}
			break;
		case 4: /* 4bit palletized */
		{
			skip = fill4B(x/2+x%2);
			fetch_pallete(fd, pallete, 16);
			lseek(fd, raster, SEEK_SET);
			unsigned char* tbuffer = (unsigned char*) malloc(x/2+1);
			if(tbuffer==NULL)
			{
				printf("Error: malloc\n");
				close(fd);
				return (FH_ERROR_MALLOC);
			}
			unsigned char c1,c2;
			for (i=0; i<y; i++) {
				read(fd, tbuffer, x/2 + x%2);
				for (j=0; j<x/2; j++) {
					c1 = tbuffer[j]>>4;
					c2 = tbuffer[j] & 0x0f;
					*wr_buffer++ = pallete[c1].red;
					*wr_buffer++ = pallete[c1].green;
					*wr_buffer++ = pallete[c1].blue;
					*wr_buffer++ = pallete[c2].red;
					*wr_buffer++ = pallete[c2].green;
					*wr_buffer++ = pallete[c2].blue;
				}
				if (x%2) {
					c1 = tbuffer[j]>>4;
					*wr_buffer++ = pallete[c1].red;
					*wr_buffer++ = pallete[c1].green;
					*wr_buffer++ = pallete[c1].blue;
				}
				if (skip) {
					read(fd, buff, skip);
				}
				wr_buffer -= x*6; /* backoff 2 lines - x*2 *3 */
			}
			free(tbuffer);
		}
		break;
		case 8: /* 8bit palletized */
		{
			skip = fill4B(x);
			fetch_pallete(fd, pallete, 256);
			lseek(fd, raster, SEEK_SET);
			unsigned char* tbuffer = (unsigned char*) malloc(x);
			if(tbuffer==NULL)
			{
				printf("Error: malloc\n");
				close(fd);
				return (FH_ERROR_MALLOC);
			}
			for (i=0; i<y; i++)
		   {
				read(fd, tbuffer, x);
				for (j=0; j<x; j++) {
					wr_buffer[j*3] = pallete[tbuffer[j]].red;
					wr_buffer[j*3+1] = pallete[tbuffer[j]].green;
					wr_buffer[j*3+2] = pallete[tbuffer[j]].blue;
				}
				if (skip) {
					read(fd, buff, skip);
				}
				wr_buffer -= x*3; /* backoff 2 lines - x*2 *3 */
			}
			free(tbuffer);
		}
		break;
		case 16: /* 16bit RGB */
			close(fd);
			return(FH_ERROR_FORMAT);
			break;
		case 24: /* 24bit RGB */
			skip = fill4B(x*3);
			lseek(fd, raster, SEEK_SET);
			unsigned char c;
			for (i=0; i<y; i++)
			{
				read(fd,wr_buffer,x*3);
				for(j=0; j < x*3 ; j=j+3)
				{
					c=wr_buffer[j];
					wr_buffer[j]=wr_buffer[j+2];
					wr_buffer[j+2]=c;
				}
				if (skip) {
					read(fd, buff, skip);
				}
				wr_buffer -= x*3; // backoff 1 lines - x*3
			}
			break;
		default:
			close(fd);
			return(FH_ERROR_FORMAT);
	}

	close(fd);
//	dbout("fh_bmp_load }\n");
	return(FH_ERROR_OK);
}
int fh_bmp_getsize(const char *name,int *x,int *y, int /*wanted_width*/, int /*wanted_height*/)
{
//	dbout("fh_bmp_getsize {\n");
	int fd;
	unsigned char size[4];

	fd = open(name, O_RDONLY);
	if (fd == -1) {
		return(FH_ERROR_FILE);
	}
	if (lseek(fd, BMP_SIZE_OFFSET, SEEK_SET) == -1) {
		close(fd);//Resource leak: fd
		return(FH_ERROR_FORMAT);
	}

	read(fd, size, 4);
	*x = size[0] + (size[1]<<8) + (size[2]<<16) + (size[3]<<24);
//	*x-=1;
	read(fd, size, 4);
	*y = size[0] + (size[1]<<8) + (size[2]<<16) + (size[3]<<24);

	close(fd);
//	dbout("fh_bmp_getsize }\n");
	return(FH_ERROR_OK);
}
#endif
