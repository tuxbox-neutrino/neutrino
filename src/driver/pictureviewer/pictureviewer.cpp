#include "config.h"
#include <global.h>
#include <neutrino.h>
#include "pictureviewer.h"
#include "pv_config.h"
#include <system/debug.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <errno.h>
#include <cs_api.h>
#include <sys/sysinfo.h>

#ifdef FBV_SUPPORT_GIF
extern int fh_gif_getsize (const char *, int *, int *, int, int);
extern int fh_gif_load (const char *, unsigned char **, int *, int *);
extern int fh_gif_id (const char *);
#endif
#ifdef FBV_SUPPORT_JPEG
extern int fh_jpeg_getsize (const char *, int *, int *, int, int);
extern int fh_jpeg_load (const char *, unsigned char **, int *, int *);
extern int fh_jpeg_id (const char *);
#endif
#ifdef FBV_SUPPORT_PNG
extern int fh_png_getsize (const char *, int *, int *, int, int);
extern int fh_png_load (const char *, unsigned char **, int *, int *);
extern int png_load_ext (const char * name, unsigned char ** buffer, int * xp, int * yp, int * bpp);
extern int fh_png_id (const char *);
#endif
#ifdef FBV_SUPPORT_BMP
extern int fh_bmp_getsize (const char *, int *, int *, int, int);
extern int fh_bmp_load (const char *, unsigned char **, int *, int *);
extern int fh_bmp_id (const char *);
#endif
#ifdef FBV_SUPPORT_CRW
extern int fh_crw_getsize (const char *, int *, int *, int, int);
extern int fh_crw_load (const char *, unsigned char **, int *, int *);
extern int fh_crw_id (const char *);
#endif

double CPictureViewer::m_aspect_ratio_correction;

void CPictureViewer::add_format (int (*picsize) (const char *, int *, int *, int, int), int (*picread) (const char *, unsigned char **, int *, int *), int (*id) (const char *))
{
  CFormathandler *fhn;
  fhn = (CFormathandler *) malloc (sizeof (CFormathandler));
  fhn->get_size = picsize;
  fhn->get_pic = picread;
  fhn->id_pic = id;
  fhn->next = fh_root;
  fh_root = fhn;
}

void CPictureViewer::getSupportedImageFormats(std::vector<std::string>& exts)
{
#ifdef FBV_SUPPORT_JPEG
	exts.push_back(".jpg");
#endif
#ifdef FBV_SUPPORT_PNG
	exts.push_back(".png");
#endif
#ifdef FBV_SUPPORT_GIF
	exts.push_back(".gif");
#endif
#ifdef FBV_SUPPORT_JPEG
	exts.push_back(".jpeg");
#endif
#ifdef FBV_SUPPORT_BMP
	exts.push_back(".bmp");
#endif
#ifdef FBV_SUPPORT_CRW
	exts.push_back(".crw");
#endif
}

void CPictureViewer::init_handlers (void)
{
#ifdef FBV_SUPPORT_PNG
  add_format (fh_png_getsize, fh_png_load, fh_png_id);
#endif
#ifdef FBV_SUPPORT_GIF
  add_format (fh_gif_getsize, fh_gif_load, fh_gif_id);
#endif
#ifdef FBV_SUPPORT_JPEG
  add_format (fh_jpeg_getsize, fh_jpeg_load, fh_jpeg_id);
#endif
#ifdef FBV_SUPPORT_BMP
  add_format (fh_bmp_getsize, fh_bmp_load, fh_bmp_id);
#endif
#ifdef FBV_SUPPORT_CRW
  add_format (fh_crw_getsize, fh_crw_load, fh_crw_id);
#endif
}

CPictureViewer::CFormathandler * CPictureViewer::fh_getsize (const char *name, int *x, int *y, int width_wanted, int height_wanted)
{
	CFormathandler *fh;
	for (fh = fh_root; fh != NULL; fh = fh->next) {
		if (fh->id_pic (name))
			if (fh->get_size (name, x, y, width_wanted, height_wanted) == FH_ERROR_OK)
				return (fh);
	}
	return (NULL);
}
std::string CPictureViewer::DownloadImage(std::string url)
{
	if (strstr(url.c_str(), "://")) {
		std::string tmpname = "/tmp/pictureviewer" + url.substr(url.find_last_of("."));
		FILE *tmpFile = fopen(tmpname.c_str(), "wb");
		if (tmpFile) {
			CURL *ch = curl_easy_init();
			if(ch)
			{
				curl_easy_setopt(ch, CURLOPT_VERBOSE, 0L);
				curl_easy_setopt(ch, CURLOPT_NOPROGRESS, 1L);
				curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1L);
				curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, NULL);
				curl_easy_setopt(ch, CURLOPT_WRITEDATA, tmpFile);
				curl_easy_setopt(ch, CURLOPT_FAILONERROR, 1L);
				curl_easy_setopt(ch, CURLOPT_URL, url.c_str());
				curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, 3);
				curl_easy_setopt(ch, CURLOPT_TIMEOUT, 4);
				curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 0L);
				CURLcode res = curl_easy_perform(ch);
				if (res != CURLE_OK){
					printf("[%s] curl_easy_perform() failed:%s\n",__func__, curl_easy_strerror(res));
				}
				curl_easy_cleanup(ch);
			}
			fclose(tmpFile);
			url = true;
		}
		url = tmpname;
	}
	return url;
}

bool CPictureViewer::DecodeImage (const std::string & _name, bool showBusySign, bool unscaled)
{
	// dbout("DecodeImage {\n"); 
#if 0 // quick fix for issue #245. TODO more smart fix for this problem
	if (name == m_NextPic_Name) {
		//      dbout("DecodeImage }\n"); 
		return true;
	}
#endif
	int x, y, imx, imy;

	// 	int xs = CFrameBuffer::getInstance()->getScreenWidth(true);
	// 	int ys = CFrameBuffer::getInstance()->getScreenHeight(true);

	// Show red block for "next ready" in view state
	if (showBusySign)
		showBusy (m_startx + 3, m_starty + 3, 10, 0xff, 00, 00);

	bool url = false;

	std::string name  = DownloadImage(_name);

	CFormathandler *fh;
	if (unscaled)
		fh = fh_getsize (name.c_str (), &x, &y, INT_MAX, INT_MAX);
	else
		fh = fh_getsize (name.c_str (), &x, &y, m_endx - m_startx, m_endy - m_starty);
	if (fh) {
		if (m_NextPic_Buffer != NULL) {
			free (m_NextPic_Buffer);
			m_NextPic_Buffer = NULL;
		}
		size_t bufsize = x * y * 3;
		size_t resizeBuf = (m_endx - m_startx) * (m_endy - m_starty)*3;
		if (!checkfreemem(bufsize + resizeBuf)){
			return false;
		}
		m_NextPic_Buffer = (unsigned char *) malloc (bufsize);
		if (m_NextPic_Buffer == NULL) {
			dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Error: malloc, %s\n", __func__, __LINE__, strerror(errno));
			return false;
		}
		//      dbout("---Decoding Start(%d/%d)\n",x,y);
		if (fh->get_pic (name.c_str (), &m_NextPic_Buffer, &x, &y) == FH_ERROR_OK) {
			//          dbout("---Decoding Done\n");
			if ((x > (m_endx - m_startx) || y > (m_endy - m_starty)) && m_scaling != NONE && !unscaled) {
				if ((m_aspect_ratio_correction * y * (m_endx - m_startx) / x) <= (m_endy - m_starty)) {
					imx = (m_endx - m_startx);
					imy = (int) (m_aspect_ratio_correction * y * (m_endx - m_startx) / x);
				} else {
					imx = (int) ((1.0 / m_aspect_ratio_correction) * x * (m_endy - m_starty) / y);
					imy = (m_endy - m_starty);
				}
				m_NextPic_Buffer = Resize(m_NextPic_Buffer, x, y, imx, imy, m_scaling);
				x = imx;
				y = imy;
			}
			m_NextPic_X = x;
			m_NextPic_Y = y;
			if (x < (m_endx - m_startx))
				m_NextPic_XPos = (m_endx - m_startx - x) / 2 + m_startx;
			else
				m_NextPic_XPos = m_startx;
			if (y < (m_endy - m_starty))
				m_NextPic_YPos = (m_endy - m_starty - y) / 2 + m_starty;
			else
				m_NextPic_YPos = m_starty;
			if (x > (m_endx - m_startx))
				m_NextPic_XPan = (x - (m_endx - m_startx)) / 2;
			else
				m_NextPic_XPan = 0;
			if (y > (m_endy - m_starty))
				m_NextPic_YPan = (y - (m_endy - m_starty)) / 2;
			else
				m_NextPic_YPan = 0;
		} else {
			dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Error: Unable to read file !, %s\n", __func__, __LINE__, strerror(errno));
			free (m_NextPic_Buffer);
			m_NextPic_Buffer = (unsigned char *) malloc (3);
			if (m_NextPic_Buffer == NULL) {
				dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Error: malloc, %s\n", __func__, __LINE__, strerror(errno));
				return false;
			}
			memset (m_NextPic_Buffer, 0, 3);
			m_NextPic_X = 1;
			m_NextPic_Y = 1;
			m_NextPic_XPos = 0;
			m_NextPic_YPos = 0;
			m_NextPic_XPan = 0;
			m_NextPic_YPan = 0;
		}
	} else {
		dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Unable to read file or format not recognized!\n", __func__, __LINE__);
		if (m_NextPic_Buffer != NULL) {
			free (m_NextPic_Buffer);
			m_NextPic_Buffer = NULL;
		}
		m_NextPic_Buffer = (unsigned char *) malloc (3);
		if (m_NextPic_Buffer == NULL) {
			dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Error: malloc, %s\n", __func__, __LINE__, strerror(errno));
			return false;
		}
		memset (m_NextPic_Buffer, 0, 3);
		m_NextPic_X = 1;
		m_NextPic_Y = 1;
		m_NextPic_XPos = 0;
		m_NextPic_YPos = 0;
		m_NextPic_XPan = 0;
		m_NextPic_YPan = 0;
	}
	m_NextPic_Name = name;
	if (url)
		unlink(name.c_str());
	hideBusy ();
	//   dbout("DecodeImage }\n"); 
	return (m_NextPic_Buffer != NULL);
}

void CPictureViewer::SetVisible (int startx, int endx, int starty, int endy)
{
	m_startx = startx;
	m_endx = endx;
	m_starty = starty;
	m_endy = endy;
}


bool CPictureViewer::ShowImage (const std::string & filename, bool unscaled)
{
	//  dbout("Show Image {\n");
	// Wird eh ueberschrieben ,also schonmal freigeben... (wenig speicher)
	if (m_CurrentPic_Buffer != NULL) {
		free (m_CurrentPic_Buffer);
		m_CurrentPic_Buffer = NULL;
	}
	if (DecodeImage (filename, true, unscaled))
		DisplayNextImage ();
	//  dbout("Show Image }\n");
	return true;
}

bool CPictureViewer::DisplayNextImage ()
{
	//  dbout("DisplayNextImage {\n");
	if (m_CurrentPic_Buffer != NULL) {
		free (m_CurrentPic_Buffer);
		m_CurrentPic_Buffer = NULL;
	}
	if (m_NextPic_Buffer != NULL)
		//fb_display (m_NextPic_Buffer, m_NextPic_X, m_NextPic_Y, m_NextPic_XPan, m_NextPic_YPan, m_NextPic_XPos, m_NextPic_YPos);
		CFrameBuffer::getInstance()->displayRGB(m_NextPic_Buffer, m_NextPic_X, m_NextPic_Y, m_NextPic_XPan, m_NextPic_YPan, m_NextPic_XPos, m_NextPic_YPos);
	//  dbout("DisplayNextImage fb_disp done\n");
	m_CurrentPic_Buffer = m_NextPic_Buffer;
	m_NextPic_Buffer = NULL;
	m_CurrentPic_Name = m_NextPic_Name;
	m_CurrentPic_X = m_NextPic_X;
	m_CurrentPic_Y = m_NextPic_Y;
	m_CurrentPic_XPos = m_NextPic_XPos;
	m_CurrentPic_YPos = m_NextPic_YPos;
	m_CurrentPic_XPan = m_NextPic_XPan;
	m_CurrentPic_YPan = m_NextPic_YPan;
	//  dbout("DisplayNextImage }\n");
	return true;
}

void CPictureViewer::Zoom (float factor)
{
	//  dbout("Zoom %f\n",factor);
	showBusy (m_startx + 3, m_starty + 3, 10, 0xff, 0xff, 00);

	int oldx = m_CurrentPic_X;
	int oldy = m_CurrentPic_Y;
	unsigned char *oldBuf = m_CurrentPic_Buffer;
	m_CurrentPic_X = int(factor * (float)m_CurrentPic_X);
	m_CurrentPic_Y = int(factor * (float)m_CurrentPic_Y);

	m_CurrentPic_Buffer = Resize(m_CurrentPic_Buffer, oldx, oldy, m_CurrentPic_X, m_CurrentPic_Y, m_scaling);

	if (m_CurrentPic_Buffer == oldBuf) {
		// resize failed
		hideBusy ();
		return;
	}

	if (m_CurrentPic_X < (m_endx - m_startx))
		m_CurrentPic_XPos = (m_endx - m_startx - m_CurrentPic_X) / 2 + m_startx;
	else
		m_CurrentPic_XPos = m_startx;
	if (m_CurrentPic_Y < (m_endy - m_starty))
		m_CurrentPic_YPos = (m_endy - m_starty - m_CurrentPic_Y) / 2 + m_starty;
	else
		m_CurrentPic_YPos = m_starty;
	if (m_CurrentPic_X > (m_endx - m_startx))
		m_CurrentPic_XPan = (m_CurrentPic_X - (m_endx - m_startx)) / 2;
	else
		m_CurrentPic_XPan = 0;
	if (m_CurrentPic_Y > (m_endy - m_starty))
		m_CurrentPic_YPan = (m_CurrentPic_Y - (m_endy - m_starty)) / 2;
	else
		m_CurrentPic_YPan = 0;
	//fb_display (m_CurrentPic_Buffer, m_CurrentPic_X, m_CurrentPic_Y, m_CurrentPic_XPan, m_CurrentPic_YPan, m_CurrentPic_XPos, m_CurrentPic_YPos);
	CFrameBuffer::getInstance()->displayRGB(m_CurrentPic_Buffer, m_CurrentPic_X, m_CurrentPic_Y, m_CurrentPic_XPan, m_CurrentPic_YPan, m_CurrentPic_XPos, m_CurrentPic_YPos);
}

void CPictureViewer::Move (int dx, int dy)
{
	int xs, ys;

	//  dbout("Move %d %d\n",dx,dy);
	showBusy (m_startx + 3, m_starty + 3, 10, 0x00, 0xff, 00);

	xs = CFrameBuffer::getInstance()->getScreenWidth(true);
	ys = CFrameBuffer::getInstance()->getScreenHeight(true);

	m_CurrentPic_XPan += dx;
	if (m_CurrentPic_XPan + xs >= m_CurrentPic_X)
		m_CurrentPic_XPan = m_CurrentPic_X - xs - 1;
	if (m_CurrentPic_XPan < 0)
		m_CurrentPic_XPan = 0;

	m_CurrentPic_YPan += dy;
	if (m_CurrentPic_YPan + ys >= m_CurrentPic_Y)
		m_CurrentPic_YPan = m_CurrentPic_Y - ys - 1;
	if (m_CurrentPic_YPan < 0)
		m_CurrentPic_YPan = 0;

	if (m_CurrentPic_X < (m_endx - m_startx))
		m_CurrentPic_XPos = (m_endx - m_startx - m_CurrentPic_X) / 2 + m_startx;
	else
		m_CurrentPic_XPos = m_startx;
	if (m_CurrentPic_Y < (m_endy - m_starty))
		m_CurrentPic_YPos = (m_endy - m_starty - m_CurrentPic_Y) / 2 + m_starty;
	else
		m_CurrentPic_YPos = m_starty;
	//  dbout("Display x(%d) y(%d) xpan(%d) ypan(%d) xpos(%d) ypos(%d)\n",m_CurrentPic_X, m_CurrentPic_Y, 
	//          m_CurrentPic_XPan, m_CurrentPic_YPan, m_CurrentPic_XPos, m_CurrentPic_YPos);

	//fb_display (m_CurrentPic_Buffer, m_CurrentPic_X, m_CurrentPic_Y, m_CurrentPic_XPan, m_CurrentPic_YPan, m_CurrentPic_XPos, m_CurrentPic_YPos);
	CFrameBuffer::getInstance()->displayRGB(m_CurrentPic_Buffer, m_CurrentPic_X, m_CurrentPic_Y, m_CurrentPic_XPan, m_CurrentPic_YPan, m_CurrentPic_XPos, m_CurrentPic_YPos);
}

CPictureViewer::CPictureViewer ()
{
	int xs, ys;

	fh_root = NULL;
	m_scaling = COLOR;
	//m_aspect = 4.0 / 3;
	m_aspect = float(16.0 / 9.0);
	m_CurrentPic_Name = "";
	m_CurrentPic_Buffer = NULL;
	m_CurrentPic_X = 0;
	m_CurrentPic_Y = 0;
	m_CurrentPic_XPos = 0;
	m_CurrentPic_YPos = 0;
	m_CurrentPic_XPan = 0;
	m_CurrentPic_YPan = 0;
	m_NextPic_Name = "";
	m_NextPic_Buffer = NULL;
	m_NextPic_X = 0;
	m_NextPic_Y = 0;
	m_NextPic_XPos = 0;
	m_NextPic_YPos = 0;
	m_NextPic_XPan = 0;
	m_NextPic_YPan = 0;

	xs = CFrameBuffer::getInstance()->getScreenWidth(true);
	ys = CFrameBuffer::getInstance()->getScreenHeight(true);

	m_startx = 0;
	m_endx = xs - 1;
	m_starty = 0;
	m_endy = ys - 1;
	m_aspect_ratio_correction = m_aspect / ((double) xs / ys);

	m_busy_buffer = NULL;

	init_handlers ();
}

CPictureViewer::~CPictureViewer ()
{
	Cleanup();
	CFormathandler *fh = fh_root;
	while (fh) {
		CFormathandler *tmp = fh->next;
		free(fh);
		fh = tmp;
	}
}

void CPictureViewer::showBusy (int sx, int sy, int width, char r, char g, char b)
{
	//  dbout("Show Busy{\n");
	unsigned char rgb_buffer[3];
	unsigned char *fb_buffer;
	unsigned char *busy_buffer_wrk;
	int cpp = 4;

	rgb_buffer[0] = r;
	rgb_buffer[1] = g;
	rgb_buffer[2] = b;

	fb_buffer = (unsigned char *) CFrameBuffer::getInstance()->convertRGB2FB (rgb_buffer, 1, 1);
	if (fb_buffer == NULL) {
		dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Error: malloc\n", __func__, __LINE__);
		return;
	}
	if (m_busy_buffer != NULL) {
		free (m_busy_buffer);
		m_busy_buffer = NULL;
	}
	size_t bufsize = width * width * cpp;
	if (!checkfreemem(bufsize)){
		cs_free_uncached (fb_buffer);
		return;
	}
	m_busy_buffer = (unsigned char *) malloc (bufsize);
	if (m_busy_buffer == NULL) {
		dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Error: malloc\n", __func__, __LINE__);
		cs_free_uncached (fb_buffer);
		return;
	}
	busy_buffer_wrk = m_busy_buffer;
	unsigned char *fb = (unsigned char *) CFrameBuffer::getInstance()->getFrameBufferPointer();
	unsigned int stride = CFrameBuffer::getInstance ()->getStride ();

	for (int y = sy; y < sy + width; y++) {
		for (int x = sx; x < sx + width; x++) {
			memmove (busy_buffer_wrk, fb + y * stride + x * cpp, cpp);
			busy_buffer_wrk += cpp;
			memmove (fb + y * stride + x * cpp, fb_buffer, cpp);
		}
	}
	m_busy_x = sx;
	m_busy_y = sy;
	m_busy_width = width;
	m_busy_cpp = cpp;
	cs_free_uncached (fb_buffer);
	//  dbout("Show Busy}\n");
}

void CPictureViewer::hideBusy ()
{
	//  dbout("Hide Busy{\n");
	if (m_busy_buffer != NULL) {
		unsigned char *fb = (unsigned char *) CFrameBuffer::getInstance ()->getFrameBufferPointer ();
		unsigned int stride = CFrameBuffer::getInstance ()->getStride ();
		unsigned char *busy_buffer_wrk = m_busy_buffer;

		for (int y = m_busy_y; y < m_busy_y + m_busy_width; y++) {
			for (int x = m_busy_x; x < m_busy_x + m_busy_width; x++) {
				memmove (fb + y * stride + x * m_busy_cpp, busy_buffer_wrk, m_busy_cpp);
				busy_buffer_wrk += m_busy_cpp;
			}
		}
		free (m_busy_buffer);
		m_busy_buffer = NULL;
	}
	//  dbout("Hide Busy}\n");
}
void CPictureViewer::Cleanup ()
{
	if (m_busy_buffer != NULL) {
		free (m_busy_buffer);
		m_busy_buffer = NULL;
	}
	if (m_NextPic_Buffer != NULL) {
		free (m_NextPic_Buffer);
		m_NextPic_Buffer = NULL;
	}
	if (m_CurrentPic_Buffer != NULL) {
		free (m_CurrentPic_Buffer);
		m_CurrentPic_Buffer = NULL;
	}
}

void CPictureViewer::getSize(const char* name, int* width, int *height)
{
	CFormathandler *fh;

	fh = fh_getsize(name, width, height, INT_MAX, INT_MAX);
	if (fh == NULL) {
		*width = 0;
		*height = 0;
	}
}

bool CPictureViewer::GetLogoName(const uint64_t& channel_id, const std::string& ChannelName, std::string & name, int *width, int *height)
{
	std::string fileType[] = { ".png", ".jpg" , ".gif" };

	//get channel id as string
	char strChnId[16];
	snprintf(strChnId, 16, "%llx", channel_id & 0xFFFFFFFFFFFFULL);
	strChnId[15] = '\0';

	for (size_t i = 0; i<(sizeof(fileType) / sizeof(fileType[0])); i++){
		std::vector<std::string> v_path;
		std::string id_tmp_path;

		//create filename with channel name (logo_hdd_dir)
		id_tmp_path = g_settings.logo_hdd_dir + "/";
		id_tmp_path += ChannelName + fileType[i];
		v_path.push_back(id_tmp_path);

		//create filename with id (logo_hdd_dir)
		id_tmp_path = g_settings.logo_hdd_dir + "/";
		id_tmp_path += strChnId + fileType[i];
		v_path.push_back(id_tmp_path);

		if(g_settings.logo_hdd_dir != LOGODIR_VAR){
			//create filename with channel name (LOGODIR_VAR)
			id_tmp_path = LOGODIR_VAR "/";
			id_tmp_path += ChannelName + fileType[i];
			v_path.push_back(id_tmp_path);

			//create filename with id (LOGODIR_VAR)
			id_tmp_path = LOGODIR_VAR "/";
			id_tmp_path += strChnId + fileType[i];
			v_path.push_back(id_tmp_path);
		}
		if(g_settings.logo_hdd_dir != LOGODIR){
			//create filename with channel name (LOGODIR)
			id_tmp_path = LOGODIR "/";
			id_tmp_path += ChannelName + fileType[i];
			v_path.push_back(id_tmp_path);

			//create filename with id (LOGODIR)
			id_tmp_path = LOGODIR "/";
			id_tmp_path += strChnId + fileType[i];
			v_path.push_back(id_tmp_path);
		}
		//check if file is available, name with real name is preferred, return true on success
		for (size_t j = 0; j < v_path.size(); j++){
			if (access(v_path[j].c_str(), R_OK) != -1){
				if(width && height)
					getSize(v_path[j].c_str(), width, height);
				name = v_path[j];
				return true;
			}
		}	
	}
	return false;
}
#if 0
bool CPictureViewer::DisplayLogo (uint64_t channel_id, int posx, int posy, int width, int height)
{
	char fname[255];
	bool ret = false;

	sprintf(fname, "%s/%llx.jpg", g_settings.logo_hdd_dir.c_str(), channel_id & 0xFFFFFFFFFFFFULL);
//	printf("logo file: %s\n", fname);
	if(access(fname, F_OK))
		sprintf(fname, "%s/%llx.gif", g_settings.logo_hdd_dir.c_str(), channel_id & 0xFFFFFFFFFFFFULL);

	if(!access(fname, F_OK)) {
		ret = DisplayImage(fname, posx, posy, width, height);
#if 0
		//ret = DisplayImage(fname, posx, posy, width, height);
		fb_pixel_t * data = getImage(fname, width, height);
		//fb_pixel_t * data = getIcon(fname, &width, &height);
		if(data) {
			CFrameBuffer::getInstance()->blit2FB(data, width, height, posx, posy);
			cs_free_uncached(data);
		}
#endif
	}
	return ret;
}
#endif
void CPictureViewer::rescaleImageDimensions(int *width, int *height, const int max_width, const int max_height, bool upscale)
{
	float aspect;

	if ((!upscale) && (*width <= max_width) && (*height <= max_height))
		return;

	aspect = (float)(*width) / (float)(*height);
	if (((float)(*width) / (float)max_width) > ((float)(*height) / (float)max_height)) {
		*width = max_width;
		*height = int((float)max_width / aspect);
	}else{
		*height = max_height;
		*width = int((float)max_height * aspect);
	}
}

bool CPictureViewer::DisplayImage(const std::string & name, int posx, int posy, int width, int height, int transp)
{
	if(width < 1 || height < 1){
		dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Error: width %i height %i \n", __func__, __LINE__, width, height);
		return false;
	}

	CFrameBuffer* frameBuffer = CFrameBuffer::getInstance();
	if (transp > CFrameBuffer::TM_EMPTY)
		frameBuffer->SetTransparent(transp);

	/* TODO: cache or check for same */
	fb_pixel_t * data = getImage(name, width, height);

	if (data){
		if (transp > CFrameBuffer::TM_EMPTY)
			frameBuffer->SetTransparentDefault();

		if(data) {
			frameBuffer->blit2FB(data, width, height, posx, posy);
			cs_free_uncached(data);
			return true;
		}
	}
	return false;
}

fb_pixel_t * CPictureViewer::int_getImage(const std::string & name, int *width, int *height, bool GetImage)
{
	if (access(name.c_str(), R_OK) == -1)
		return NULL;

	int x, y, load_ret, bpp = 0;
	CFormathandler *fh = NULL;
	unsigned char * buffer = NULL;
	fb_pixel_t * ret = NULL;
	std::string mode_str;

	if (GetImage)
		mode_str = "getImage";
	else
		mode_str = "getIcon";

  	fh = fh_getsize(name.c_str(), &x, &y, INT_MAX, INT_MAX);
	size_t bufsize = x * y * 4;
	if (!checkfreemem(bufsize))
		return NULL;

  	if (fh)
  	{
		buffer = (unsigned char *) malloc(bufsize);//x * y * 4
		if (buffer == NULL)
		{
			dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] mode %s: Error: malloc\n", __func__, __LINE__, mode_str.c_str());
			return NULL;
		}
#ifdef FBV_SUPPORT_PNG
		if ((name.find(".png") == (name.length() - 4)) && (fh_png_id(name.c_str())))
			load_ret = png_load_ext(name.c_str(), &buffer, &x, &y, &bpp);
		else
#endif
			load_ret = fh->get_pic(name.c_str (), &buffer, &x, &y);
		dprintf(DEBUG_INFO,  "[CPictureViewer] [%s - %d] load_result: %d \n", __func__, __LINE__, load_ret);

		if (load_ret == FH_ERROR_OK)
		{
			dprintf(DEBUG_INFO,  "[CPictureViewer] [%s - %d] mode %s, decoded %s, (Pos: %d %d) ,bpp = %d \n", __func__, __LINE__, mode_str.c_str(), name.c_str(), x, y, bpp);
			// image size error
			if((GetImage) && (*width < 1 || *height < 1)){
				dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] mode: %s, file: %s (Pos: %d %d, Dim: %d x %d)\n", __func__, __LINE__, mode_str.c_str(), name.c_str(), x, y, *width, *height);
				free(buffer);
				buffer = NULL;
				return NULL;
			}
			// resize only getImage
			if ((GetImage) && (x != *width || y != *height))
			{
				dprintf(DEBUG_INFO,  "[CPictureViewer] [%s - %d] resize  %s to %d x %d \n", __func__, __LINE__, name.c_str(), *width, *height);
				if (bpp == 4)
					buffer = ResizeA(buffer, x, y, *width, *height);
				else
					buffer = Resize(buffer, x, y, *width, *height, COLOR);
				x = *width;
				y = *height;
			}
			if (bpp == 4)
				ret = (fb_pixel_t *) CFrameBuffer::getInstance()->convertRGBA2FB(buffer, x, y);
			else
				ret = (fb_pixel_t *) CFrameBuffer::getInstance()->convertRGB2FB(buffer, x, y, convertSetupAlpha2Alpha(g_settings.theme.infobar_alpha));
			*width = x;
			*height = y;
		}else{
			dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] mode %s: Error decoding file %s\n", __func__, __LINE__, mode_str.c_str(), name.c_str());
			free(buffer);
			buffer = NULL;
			return NULL;
		}
		free(buffer);
		buffer = NULL;
  	}else
		dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] mode: %s, file: %s Error: %s, buffer = %p (Pos: %d %d, Dim: %d x %d)\n", __func__, __LINE__, mode_str.c_str(), name.c_str(), strerror(errno), buffer, x, y, *width, *height);
	return ret;
}

fb_pixel_t * CPictureViewer::getImage(const std::string & name, int width, int height)
{
	return int_getImage(name, &width, &height, true);
}

fb_pixel_t * CPictureViewer::getIcon(const std::string & name, int *width, int *height)
{
	return int_getImage(name, width, height, false);
}

unsigned char * CPictureViewer::int_Resize(unsigned char *orgin, int ox, int oy, int dx, int dy, ScalingMode type, unsigned char * dst, bool alpha)
{
	unsigned char * cr;
	if(dst == NULL)
	{
		size_t bufsize = dx * dy * ((alpha) ? 4 : 3);
		if (!checkfreemem(bufsize)){
			return orgin;
		}
		cr = (unsigned char*) malloc(bufsize);
		if(cr == NULL)
		{
			dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Resize Error: malloc\n", __func__, __LINE__);
			return orgin;
		}
	}else
		cr = dst;

	if(type == SIMPLE)
	{
		unsigned char *p,*l;
		int i,j,k,ip;
		l=cr;

		for(j=0;j<dy;j++,l+=dx*3)
		{
			p=orgin+(j*oy/dy*ox*3);
			for(i=0,k=0;i<dx;i++,k+=3)
			{
				ip=i*ox/dx*3;
				memmove(l+k, p+ip, 3);
			}
		}
	}else
	{
		unsigned char *p,*q;
		int i,j,k,l,ya,yb;
		int sq,r,g,b,a;

		p=cr;

		int xa_v[dx];
		for(i=0;i<dx;i++)
			xa_v[i] = i*ox/dx;
		int xb_v[dx+1];
		for(i=0;i<dx;i++)
		{
			xb_v[i]= (i+1)*ox/dx;
			if(xb_v[i]>=ox)
				xb_v[i]=ox-1;
		}

		if (alpha)
		{
			for(j=0;j<dy;j++)
			{
				ya= j*oy/dy;
				yb= (j+1)*oy/dy; if(yb>=oy) yb=oy-1;
				for(i=0;i<dx;i++,p+=4)
				{
					for(l=ya,r=0,g=0,b=0,a=0,sq=0;l<=yb;l++)
					{
						q=orgin+((l*ox+xa_v[i])*4);
						for(k=xa_v[i];k<=xb_v[i];k++,q+=4,sq++)
						{
							r+=q[0]; g+=q[1]; b+=q[2]; a+=q[3];
						}
					}
					int sq_tmp = sq ? sq : 1;//avoid division by zero
					p[0]= uint8_t(r/sq_tmp);
					p[1]= uint8_t(g/sq_tmp);
					p[2]= uint8_t(b/sq_tmp);
					p[3]= uint8_t(a/sq_tmp);
				}
			}
		}else
		{
			for(j=0;j<dy;j++)
			{
				ya= j*oy/dy;
				yb= (j+1)*oy/dy; if(yb>=oy) yb=oy-1;
				for(i=0;i<dx;i++,p+=3)
				{
					for(l=ya,r=0,g=0,b=0,sq=0;l<=yb;l++)
					{
						q=orgin+((l*ox+xa_v[i])*3);
						for(k=xa_v[i];k<=xb_v[i];k++,q+=3,sq++)
						{
							r+=q[0]; g+=q[1]; b+=q[2];
						}
					}
					int sq_tmp = sq ? sq : 1;//avoid division by zero
					p[0]= uint8_t(r/sq_tmp);
					p[1]= uint8_t(g/sq_tmp);
					p[2]= uint8_t(b/sq_tmp);
				}
			}
		}
	}
	free(orgin);
	orgin = NULL;
	return(cr);
}

unsigned char * CPictureViewer::Resize(unsigned char *orgin, int ox, int oy, int dx, int dy, ScalingMode type, unsigned char * dst)
{
	return int_Resize(orgin, ox, oy, dx, dy, type, dst, false);
}

unsigned char * CPictureViewer::ResizeA(unsigned char *orgin, int ox, int oy, int dx, int dy)
{
	return int_Resize(orgin, ox, oy, dx, dy, COLOR, NULL, true);
}

static size_t getCachedMemSize(void)
{
	FILE *procmeminfo = fopen("/proc/meminfo", "r");
	size_t cached = 0;
	if (procmeminfo) {
		char buf[80] = {0}, a[80] = {0};
		long long unsigned int v = 0;
		while (fgets(buf, sizeof(buf), procmeminfo)) {
			char unit[10];
			*unit = 0;
			if ((3 == sscanf(buf, "%[^:]: %llu %s", a, &v, unit))
			 || (2 == sscanf(buf, "%[^:]: %llu", a, &v))) {
				if (*unit == 'k')
					v <<= 10;
				if (!strcasecmp(a, "Cached")){
					cached = v;
					break;
				}
			}
		}
		fclose(procmeminfo);
	}
	return cached;
}

bool CPictureViewer::checkfreemem(size_t bufsize)
{
	struct sysinfo info;
	sysinfo( &info );
	size_t cached = getCachedMemSize();
	if(bufsize + sysconf(_SC_PAGESIZE) > (size_t)info.freeram + (size_t)info.freeswap + (size_t)info.bufferram + cached){
		dprintf(DEBUG_NORMAL,  "[CPictureViewer] [%s - %d] Error: Out of memory: need %zu > free %zu\n", __func__, __LINE__,bufsize,(size_t)info.freeram + (size_t)info.freeswap + (size_t)info.bufferram + cached);
		return false;
	}
	return true;
}
