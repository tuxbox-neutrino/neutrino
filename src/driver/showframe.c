#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include </dream/driver/include/ost/dmx.h>
#include </dream/driver/include/ost/video.h>

#define VIDEO_DEV "/dev/dvb/card0/video0"
#define VIDEO_SET_AUTOFLUSH     _IOW('o', 2, int)

void showframe(char * fname)
{
	int file, size;
	char *buffer;

        file=open(fname, O_RDONLY);
        if (file < 0)
                return;
        size=lseek(file, 0, SEEK_END);
        lseek(file, 0, SEEK_SET);
        if (size < 0)
        {
                close(file);
                return;
        }
        buffer = malloc(size);
        read(file, buffer, size);
        close(file);
        displayIFrame(buffer, size);
        free(buffer);
	return;
}
static int displayIFrame(const char *frame, int len)
{
	int fdv, fdvideo, i;
        fdv=open("/dev/video", O_WRONLY);
        if (fdv < 0) {
		printf("cant open /dev/video\n");
                return -1;
	}

        fdvideo = open(VIDEO_DEV, O_RDWR);
        if (fdvideo < 0) {
		colose(fdv);//Resource leak: fdv
		printf("cant open %s\n", VIDEO_DEV);
                return -1;
	}
        if (ioctl(fdvideo, VIDEO_SELECT_SOURCE, VIDEO_SOURCE_MEMORY ) <0)
                printf("VIDEO_SELECT_SOURCE failed \n");
        if (ioctl(fdvideo, VIDEO_CLEAR_BUFFER) <0 )
                printf("VIDEO_CLEAR_BUFFER failed \n");
        if (ioctl(fdvideo, VIDEO_PLAY) < 0 )
                printf("VIDEO_PLAY failed\n");

        for (i=0; i < 2; i++ )
                write(fdv, frame, len);

        unsigned char buf[128];
        memset(&buf, 0, 128);
        write(fdv, buf, 128);

        if ( ioctl(fdv, VIDEO_SET_AUTOFLUSH, 0) < 0 )
                printf("VIDEO_SET_AUTOFLUSH off failed\n");

        if (ioctl(fdvideo, VIDEO_SET_BLANK, 0) < 0 )
                printf("VIDEO_SET_BLANK failed\n");

        close(fdvideo);
        if ( ioctl(fdv, VIDEO_SET_AUTOFLUSH, 1) < 0 )
                printf("VIDEO_SET_AUTOFLUSH on failed\n");

        close(fdv);
        return 0;
}

