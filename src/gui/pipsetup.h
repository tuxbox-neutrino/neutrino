#ifndef __PIP_SETUP_H_
#define __PIP_SETUP_H_

#include <gui/widget/menue.h>
#include <driver/framebuffer.h>
#include <string>

class CPipSetup : public CMenuTarget
{
	private:
		CFrameBuffer * frameBuffer;
		int x_coord;
		int y_coord;
		int width;
		int height;
		int maxw;
		int maxh;
		int minw;
		int minh;
		void paint();
		void hide();
		void clear();
	public:
		CPipSetup();
		void move(int x, int y, bool abs = false);
		void resize(int w, int h, bool abs = false);
		int exec(CMenuTarget* parent, const std::string & actionKey);
};
#endif
