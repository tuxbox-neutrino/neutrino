#include "pv_config.h"

#ifdef FBV_SUPPORT_PNG
	#include <png.h>
	#include <sys/types.h>
	#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

	#include "pictureviewer.h"

	#define PNG_BYTES_TO_CHECK 4
	#define min(x,y) ((x) < (y) ? (x) : (y))

int fh_png_id(const char *name)
{
	int fd;
	char id[4];
	fd=open(name,O_RDONLY); if(fd==-1) return(0);
	read(fd,id,4);
	close(fd);
	if(id[1]=='P' && id[2]=='N' && id[3]=='G') return(1);
	return(0);
}

int fh_png_load(const char *name, unsigned char **buffer, int* xp, int* yp);

int int_png_load(const char *name, unsigned char **buffer, int* xp, int* yp, int* bpp, bool alpha)
{
	static const png_color_16 my_background = {0, 0, 0, 0, 0};
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	unsigned int i;
	int bit_depth, color_type, interlace_type, number_passes, pass, int_bpp = 3;
	png_byte * fbptr;
	FILE     * fh;

	if(!(fh=fopen(name,"rb")))
		return(FH_ERROR_FILE);
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(png_ptr == NULL) {
		fclose(fh);
		return(FH_ERROR_FORMAT);
	}
	info_ptr = png_create_info_struct(png_ptr);
	if(info_ptr == NULL)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		fclose(fh);
		return(FH_ERROR_FORMAT);
	}
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		fclose(fh);
		return(FH_ERROR_FORMAT);
	}
	png_init_io(png_ptr,fh);
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
	if (alpha)
	{
		*bpp = png_get_channels(png_ptr, info_ptr);
		if ((*bpp != 4) || !(color_type & PNG_COLOR_MASK_ALPHA))
		{
			png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
			fclose(fh);
			return fh_png_load(name, buffer, xp, yp);
		}
		// 24bit PNGs with alpha-channel
		int_bpp = 4;
//		png_set_swap_alpha(png_ptr);
		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(png_ptr);
	}else // All other PNGs
	{
		if (color_type == PNG_COLOR_TYPE_PALETTE)
		{
			png_set_palette_to_rgb(png_ptr);
			png_set_background(png_ptr, (png_color_16*)&my_background, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
			/* other possibility for png_set_background: use png_get_bKGD */
		}
		if (color_type == PNG_COLOR_TYPE_GRAY        ||
		    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		{
			png_set_gray_to_rgb(png_ptr);
			png_set_background(png_ptr, (png_color_16*)&my_background, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
		}
		/* this test does not trigger for 8bit-paletted PNGs with newer libpng (1.2.36 at least),
	   	   but the data delivered is with alpha channel anyway, so always strip alpha for now
		 */
#if PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR <= 2 && PNG_LIBPNG_VER_RELEASE < 36
		if (color_type & PNG_COLOR_MASK_ALPHA)
#endif
			png_set_strip_alpha(png_ptr);
		if (bit_depth < 8)
			png_set_packing(png_ptr);
	}
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);
	number_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr,info_ptr);
	unsigned long rowbytes = png_get_rowbytes(png_ptr, info_ptr);
	if (width * int_bpp != rowbytes)
	{
		printf("[png.cpp]: Error processing %s - please report (including image).\n", name);
		printf("           width: %lu rowbytes: %lu\n", (unsigned long)width, (unsigned long)rowbytes);
		fclose(fh);
		return(FH_ERROR_FORMAT);
	}
	for (pass = 0; pass < number_passes; pass++)
	{
		fbptr = (png_byte *)(*buffer);
		for (i = 0; i < height; i++, fbptr += width * int_bpp)
			png_read_row(png_ptr, fbptr, NULL);
	}
	png_read_end(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
	fclose(fh);
	return(FH_ERROR_OK);
}

int png_load_ext(const char *name, unsigned char **buffer, int* xp, int* yp, int* bpp)
{
	return int_png_load(name, buffer, xp, yp, bpp, true);
}

int fh_png_load(const char *name, unsigned char **buffer, int* xp, int* yp)
{
	return int_png_load(name, buffer, xp, yp, NULL, false);
}

int fh_png_getsize(const char *name,int *x,int *y, int /*wanted_width*/, int /*wanted_height*/)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height;
	int bit_depth, color_type, interlace_type;
	FILE *fh;

	if(!(fh=fopen(name,"rb")))	return(FH_ERROR_FILE);

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,NULL,NULL,NULL);
	if(png_ptr == NULL) {
		fclose(fh);
		return(FH_ERROR_FORMAT);
	}
	info_ptr = png_create_info_struct(png_ptr);
	if(info_ptr == NULL)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		fclose(fh);
		return(FH_ERROR_FORMAT);
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		fclose(fh);
		return(FH_ERROR_FORMAT);
	}

	png_init_io(png_ptr,fh);
	png_read_info(png_ptr, info_ptr);
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,&interlace_type, NULL, NULL);
	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
	*x=width;
	*y=height;
	fclose(fh);
	return(FH_ERROR_OK);
}
#endif
