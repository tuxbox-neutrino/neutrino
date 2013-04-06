/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright 2010 Carsten Juttner <carjay@gmx.net>
	Copyright 2012 Stefan Seyfried <seife@tuxboxcvs.slipkontur.de>

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define __STDC_CONSTANT_MACROS
#include <iostream>
#include <vector>
#include <deque>
#include "global.h"
#include "neutrinoMessages.h"

#include <sys/types.h>
#include <signal.h>

#if 0
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "GL/glew.h"
#include "GL/freeglut.h"
}
#include "decodethread.h"
#include <boost/shared_ptr.hpp>
#endif
#include <sys/types.h>
#include <unistd.h>
#include "glthread.h"

static GLThreadObj *gThiz = 0; /* GLUT does not allow for an arbitrary argument to the render func */

GLThreadObj::GLThreadObj(int x, int y) : mX(x), mY(y), mReInit(true), mShutDown(false), mInitDone(false)
{
	mState.width  = mX;
	mState.height = mY;
	mState.go3d   = false;

	initKeys();
}


GLThreadObj::~GLThreadObj()
{
}

#if 0
GLThreadObj::GLThreadObj(GLThreadObj &rhs)
{
	/* lock rhs only since this is not usable yet */
	// rhs.mMutex.lock();
	mX = rhs.mX;
	mY = rhs.mY;
	mReInit     = rhs.mReInit;
	mOSDBuffer  = rhs.mOSDBuffer;
	mInitDone   = rhs.mInitDone;
	mState      = rhs.mState;
	mKeyMap     = rhs.mKeyMap;
	mSpecialMap = rhs.mSpecialMap;
}

GLThreadObj const &GLThreadObj::operator= (GLThreadObj const &rhs)
{
	if(&rhs != this)
	{
#if 0
		/* but here we need to lock both */
		if (&mMutex < &rhs.mMutex)
		{
			mMutex.lock();
			rhs.mMutex.lock();
		}
		else
		{
			rhs.mMutex.lock();
			mMutex.lock();
		}
#endif
		mX = rhs.mX;
		mY = rhs.mY;
		mReInit     = rhs.mReInit;
		mOSDBuffer  = rhs.mOSDBuffer;
		mInitDone   = rhs.mInitDone;
		mState      = rhs.mState;
		mKeyMap     = rhs.mKeyMap;
		mSpecialMap = rhs.mSpecialMap;
	}
	return *this;
}
#endif

void GLThreadObj::initKeys()
{
	mSpecialMap[GLUT_KEY_UP]    = CRCInput::RC_up;
	mSpecialMap[GLUT_KEY_DOWN]  = CRCInput::RC_down;
	mSpecialMap[GLUT_KEY_LEFT]  = CRCInput::RC_left;
	mSpecialMap[GLUT_KEY_RIGHT] = CRCInput::RC_right;

	mSpecialMap[GLUT_KEY_F1] = CRCInput::RC_red;
	mSpecialMap[GLUT_KEY_F2] = CRCInput::RC_green;
	mSpecialMap[GLUT_KEY_F3] = CRCInput::RC_yellow;
	mSpecialMap[GLUT_KEY_F4] = CRCInput::RC_blue;

	mSpecialMap[GLUT_KEY_PAGE_UP]   = CRCInput::RC_page_up;
	mSpecialMap[GLUT_KEY_PAGE_DOWN] = CRCInput::RC_page_down;

	mKeyMap[0x0d] = CRCInput::RC_ok;
	mKeyMap[0x1b] = CRCInput::RC_home;
	mKeyMap['i']  = CRCInput::RC_info;
	mKeyMap['m']  = CRCInput::RC_setup;

	mKeyMap['+']  = CRCInput::RC_plus;
	mKeyMap['-']  = CRCInput::RC_minus;
	mKeyMap['.']  = CRCInput::RC_spkr;
	mKeyMap['h']  = CRCInput::RC_help;
	mKeyMap['p']  = CRCInput::RC_standby;

	mKeyMap['0']  = CRCInput::RC_0;
	mKeyMap['1']  = CRCInput::RC_1;
	mKeyMap['2']  = CRCInput::RC_2;
	mKeyMap['3']  = CRCInput::RC_3;
	mKeyMap['4']  = CRCInput::RC_4;
	mKeyMap['5']  = CRCInput::RC_5;
	mKeyMap['6']  = CRCInput::RC_6;
	mKeyMap['7']  = CRCInput::RC_7;
	mKeyMap['8']  = CRCInput::RC_8;
	mKeyMap['9']  = CRCInput::RC_9;
}

void GLThreadObj::run()
{
	setupCtx();
	setupOSDBuffer();

	initDone(); /* signal that setup is finished */

	/* init the good stuff */
	GLenum err = glewInit();
	if(err == GLEW_OK)
	{
		if((!GLEW_VERSION_1_5)||(!GLEW_EXT_pixel_buffer_object)||(!GLEW_ARB_texture_non_power_of_two))
		{
			std::cout << "Sorry, your graphics card is not supported. Needs at least OpenGL 1.5, pixel buffer objects and NPOT textures." << std::endl;
			perror("incompatible graphics card");
			_exit(1);
		}
		else
		{
			/* start decode thread */
#if 0
			mpSWDecoder = boost::shared_ptr<SWDecoder>(new SWDecoder());
			if(mpSWDecoder)
			{ /* kick off the GL thread for the window */
				mSWDecoderThread = boost::thread(boost::ref(*mpSWDecoder));
			}
#endif
			gThiz = this;
			glutDisplayFunc(GLThreadObj::rendercb);
			glutKeyboardFunc(GLThreadObj::keyboardcb);
			glutSpecialFunc(GLThreadObj::specialcb);
			setupGLObjects(); /* needs GLEW prototypes */
			glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);
			glutMainLoop();
			releaseGLObjects();
#if 0
			if(mpSWDecoder)
			{
				mpSWDecoder->doFinish();
				mSWDecoderThread.join();
			}
#endif
		}
	}
	else
	{
		printf("GLThread: error initializing glew: %d\n", err);
	}
	if(g_RCInput)
	{
		g_RCInput->postMsg(NeutrinoMessages::SHUTDOWN, 0);
	}
	std::cout << "GL thread stopping" << std::endl;
}


void GLThreadObj::setupCtx()
{
	int argc = 1;
	/* some dummy commandline for GLUT to be happy */
	char const *argv[2] = { "neutrino", 0 };
	std::cout << "GL thread starting" << std::endl;
	glutInit(&argc, const_cast<char **>(argv));
	glutInitWindowSize(mX, mY);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutCreateWindow("Neutrino");
}


void GLThreadObj::setupOSDBuffer()
{	/* the OSD buffer size can be decoupled from the actual
	   window size since the GL can blit-stretch with no
	   trouble at all, ah, the luxury of ignorance... */
	// mMutex.lock();
	if(mState.width && mState.height)
	{
		/* 32bit FB depth, *2 because tuxtxt uses a shadow buffer */
		int fbmem = mState.width * mState.height * 4 * 2;
		mOSDBuffer.resize(fbmem);
		printf("OSD buffer set to %d bytes\n", fbmem);
	}
}


void GLThreadObj::setupGLObjects()
{
	glGenTextures(1, &mState.osdtex);
	// glGenTextures(1, &mState.displaytex);
	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mState.width, mState.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	// glBindTexture(GL_TEXTURE_2D, mState.displaytex); /* we do not yet know the size so will set that inline */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glGenBuffers(1, &mState.pbo);
	// glGenBuffers(1, &mState.displaypbo);
}


void GLThreadObj::releaseGLObjects()
{
	glDeleteTextures(1, &mState.osdtex);
	// glDeleteTextures(1, &mState.displaytex);
	glDeleteBuffers(1, &mState.pbo);
	// glDeleteBuffers(1, &mState.displaypbo);
}


/* static */ void GLThreadObj::rendercb()
{
	gThiz->render();
}


/* static */ void GLThreadObj::keyboardcb(unsigned char key, int /*x*/, int /*y*/)
{
	std::map<unsigned char, neutrino_msg_t>::const_iterator i = gThiz->mKeyMap.find(key);
	if(i != gThiz->mKeyMap.end())
	{ /* let's assume globals are thread-safe */
		if(g_RCInput)
		{
			g_RCInput->postMsg(i->second, 0);
		}
	}

}


/* static */ void GLThreadObj::specialcb(int key, int /*x*/, int /*y*/)
{
	std::map<int, neutrino_msg_t>::const_iterator i = gThiz->mSpecialMap.find(key);
	if(key == GLUT_KEY_F12)
	{
		gThiz->mState.go3d = gThiz->mState.go3d ? false : true;
		gThiz->mReInit = true;
	}
	else if(i != gThiz->mSpecialMap.end())
	{
		if(g_RCInput)
		{
			g_RCInput->postMsg(i->second, 0);
		}
	}
}

void GLThreadObj::render() {
	if(!mReInit)
	{   /* for example if window is resized */
		checkReinit();
	}

	if(mShutDown)
	{
		glutLeaveMainLoop();
	}

	if(mReInit)
	{
		mReInit = false;
		glViewport(0, 0, mX, mY);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		float aspect = static_cast<float>(mX)/mY;
		float osdaspect = 1.0/(static_cast<float>(mState.width)/mState.height);
		if(!mState.go3d)
		{
			glOrtho(aspect*-osdaspect, aspect*osdaspect, -1.0, 1.0, -1.0, 1.0 );
			glClearColor(0.0, 0.0, 0.0, 1.0);
		}
		else
		{	/* carjay is crazy... :-) */
			gluPerspective(45.0, static_cast<float>(mX)/mY, 0.05, 1000.0);
			glTranslatef(0.0, 0.0, -2.0);
			glClearColor(0.25, 0.25, 0.25, 1.0);
		}
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	// bltDisplayBuffer(); /* decoded video stream */
	if (mState.blit) {
		/* only blit manually after fb->blit(), this helps to find missed blit() calls */
		mState.blit = false;
		//fprintf(stderr, "blit!\n");
		bltOSDBuffer(); /* OSD */
	}

	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// cube test
	if(mState.go3d)
	{
		glEnable(GL_DEPTH_TEST);
		static float ydeg = 0.0;
		glPushMatrix();
		glRotatef(ydeg, 0.0, 1.0, 0.0);
		// glBindTexture(GL_TEXTURE_2D, mState.displaytex);
		// drawCube(0.5);
		glScalef(1.01, 1.01, 1.01);
		glBindTexture(GL_TEXTURE_2D, mState.osdtex);
		drawCube(0.5);
		glPopMatrix();
		ydeg += 0.75f;
	}
	else
	{
		// glBindTexture(GL_TEXTURE_2D, mState.displaytex);
		// drawSquare(1.0);
		glBindTexture(GL_TEXTURE_2D, mState.osdtex);
		drawSquare(1.0);
	}

	glFlush();
	glutSwapBuffers();

	GLuint err = glGetError();
	if(err != 0)
	{
		printf("GLError:%d 0x%04x\n", err, err);
	}

	/* simply limit to 30 Hz, if anyone wants to do this properly, feel free */
	// boost::thread::sleep(boost::get_system_time() + boost::posix_time::milliseconds(34));
	usleep(34000);
	glutPostRedisplay();
}


void GLThreadObj::checkReinit()
{
	int x = glutGet(GLUT_WINDOW_WIDTH);
	int y = glutGet(GLUT_WINDOW_HEIGHT);
	if( x != mX || y != mY )
	{
		mX = x;
		mY = y;
		mReInit = true;
	}
}


void GLThreadObj::drawCube(float size)
{
	GLfloat vertices[] = {
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f
	};

	GLubyte indices[] = {
		0, 1, 2, 3, /* front  */
		0, 3, 4, 5, /* right  */
		0, 5, 6, 1, /* top    */
		1, 6, 7, 2, /* left   */
		7, 4, 3, 2, /* bottom */
		4, 7, 6, 5  /* back   */
	};

	GLfloat texcoords[] = {
		1.0, 0.0, // v0
		0.0, 0.0, // v1
		0.0, 1.0, // v2
		1.0, 1.0, // v3
		0.0, 1.0, // v4
		0.0, 0.0, // v5
		1.0, 0.0, // v6
		1.0, 1.0  // v7
	};

	glPushMatrix();
	glScalef(size, size, size);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glDrawElements(GL_QUADS, 24, GL_UNSIGNED_BYTE, indices);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glPopMatrix();
}


void GLThreadObj::drawSquare(float size)
{
	GLfloat vertices[] = {
		 1.0f,  1.0f,
		-1.0f,  1.0f,
		-1.0f, -1.0f,
		 1.0f, -1.0f,
	};

	GLubyte indices[] = { 0, 1, 2, 3 };

	GLfloat texcoords[] = {
		 1.0, 0.0,
		 0.0, 0.0,
		 0.0, 1.0,
		 1.0, 1.0,
	};

	glPushMatrix();
	glScalef(size, size, size);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glDrawElements(GL_QUADS, 4, GL_UNSIGNED_BYTE, indices);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glPopMatrix();
}


void GLThreadObj::initDone()
{
	// mMutex.lock();
	mInitDone = true;
	// mInitCond.notify_all();
}


void GLThreadObj::waitInit()
{
	// mMutex.lock();
	while(!mInitDone)
	{
		// mInitCond.wait(lock);
		usleep(1);
	}
}


void GLThreadObj::bltOSDBuffer()
{
	/* FIXME: copy each time  */
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mState.pbo);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, mOSDBuffer.size(), &mOSDBuffer[0], GL_STREAM_DRAW_ARB);

	glBindTexture(GL_TEXTURE_2D, mState.osdtex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mState.width, mState.height, GL_BGRA, GL_UNSIGNED_BYTE, 0);

	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

#if 0
void GLThreadObj::bltDisplayBuffer()
{
	if(mpSWDecoder)
	{
		SWDecoder::pBuffer_t displaybuffer = mpSWDecoder->acquireDisplayBuffer();
		if(displaybuffer != 0)
		{
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mState.displaypbo);
			glBufferData(GL_PIXEL_UNPACK_BUFFER, displaybuffer->size(), &(*displaybuffer)[0], GL_STREAM_DRAW_ARB);

			glBindTexture(GL_TEXTURE_2D, mState.displaytex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, displaybuffer->width(), displaybuffer->height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			mpSWDecoder->returnDisplayBuffer(displaybuffer);
		}
	}
}
#endif

void GLThreadObj::clear()
{
	memset(&mOSDBuffer[0], 0, mOSDBuffer.size());
}
