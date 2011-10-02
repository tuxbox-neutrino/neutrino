#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/types.h>
#include <inttypes.h>
#include <cstdio>
#include <cstring>
#include "record_td.h"
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_RECORD, this, args)
#define lt_info(args...) _lt_info(TRIPLE_DEBUG_RECORD, this, args)

/* helper function to call the cpp thread loop */
void *execute_record_thread(void *c)
{
	cRecord *obj = (cRecord *)c;
	obj->RecordThread();
	return NULL;
}

cRecord::cRecord(int /*num*/)
{
	lt_info("%s\n", __func__);
	dmx = NULL;
	record_thread_running = false;
	file_fd = -1;
	exit_flag = RECORD_STOPPED;
}

cRecord::~cRecord()
{
	lt_info("%s: calling ::Stop()\n", __func__);
	Stop();
	lt_info("%s: end\n", __func__);
}

bool cRecord::Open(void)
{
	lt_info("%s\n", __func__);
	exit_flag = RECORD_STOPPED;
	return true;
}

#if 0
// unused
void cRecord::Close(void)
{
	lt_info("%s: \n", __func__);
}
#endif

bool cRecord::Start(int fd, unsigned short vpid, unsigned short * apids, int numpids)
{
	lt_info("%s: fd %d, vpid 0x%03x\n", __func__, fd, vpid);
	int i;

	if (!dmx)
		dmx = new cDemux(1);

	dmx->Open(DMX_TP_CHANNEL, NULL, 0);
	dmx->pesFilter(vpid);

	for (i = 0; i < numpids; i++)
		dmx->addPid(apids[i]);

	file_fd = fd;
	exit_flag = RECORD_RUNNING;
	if (posix_fadvise(file_fd, 0, 0, POSIX_FADV_DONTNEED))
		perror("posix_fadvise");

	i = pthread_create(&record_thread, 0, execute_record_thread, this);
	if (i != 0)
	{
		exit_flag = RECORD_FAILED_READ;
		errno = i;
		lt_info("%s: error creating thread! (%m)\n", __func__);
		delete dmx;
		dmx = NULL;
		return false;
	}
	record_thread_running = true;
	return true;
}

bool cRecord::Stop(void)
{
	lt_info("%s\n", __func__);

	if (exit_flag != RECORD_RUNNING)
		lt_info("%s: status not RUNNING? (%d)\n", __func__, exit_flag);

	exit_flag = RECORD_STOPPED;
	if (record_thread_running)
		pthread_join(record_thread, NULL);
	record_thread_running = false;

	/* We should probably do that from the destructor... */
	if (!dmx)
		lt_info("%s: dmx == NULL?\n", __func__);
	else
		delete dmx;
	dmx = NULL;

	if (file_fd != -1)
		close(file_fd);
	else
		lt_info("%s: file_fd not open??\n", __func__);
	file_fd = -1;
	return true;
}

bool cRecord::ChangePids(unsigned short /*vpid*/, unsigned short *apids, int numapids)
{
	std::vector<pes_pids> pids;
	int j;
	bool found;
	unsigned short pid;
	lt_info("%s\n", __func__);
	if (!dmx) {
		lt_info("%s: DMX = NULL\n", __func__);
		return false;
	}
	pids = dmx->getPesPids();
	/* the first PID is the video pid, so start with the second PID... */
	for (std::vector<pes_pids>::const_iterator i = pids.begin() + 1; i != pids.end(); ++i) {
		found = false;
		pid = (*i).pid;
		for (j = 0; j < numapids; j++) {
			if (pid == apids[j]) {
				found = true;
				break;
			}
		}
		if (!found)
			dmx->removePid(pid);
	}
	for (j = 0; j < numapids; j++) {
		found = false;
		for (std::vector<pes_pids>::const_iterator i = pids.begin() + 1; i != pids.end(); ++i) {
			if ((*i).pid == apids[j]) {
				found = true;
				break;
			}
		}
		if (!found)
			dmx->addPid(apids[j]);
	}
	return true;
}

bool cRecord::AddPid(unsigned short pid)
{
	std::vector<pes_pids> pids;
	lt_info("%s: \n", __func__);
	if (!dmx) {
		lt_info("%s: DMX = NULL\n", __func__);
		return false;
	}
	pids = dmx->getPesPids();
	for (std::vector<pes_pids>::const_iterator i = pids.begin(); i != pids.end(); ++i) {
		if ((*i).pid == pid)
			return true; /* or is it an error to try to add the same PID twice? */
	}
	return dmx->addPid(pid);
}

void cRecord::RecordThread()
{
	lt_info("%s: begin\n", __func__);
#define BUFSIZE (1 << 19) /* 512 kB */
	ssize_t r = 0;
	int buf_pos = 0;
	uint8_t *buf;
	buf = (uint8_t *)malloc(BUFSIZE);

	if (!buf)
	{
		exit_flag = RECORD_FAILED_MEMORY;
		lt_info("%s: unable to allocate buffer! (out of memory)\n", __func__);
	}

	dmx->Start();
	while (exit_flag == RECORD_RUNNING)
	{
		if (buf_pos < BUFSIZE)
		{
			r = dmx->Read(buf + buf_pos, BUFSIZE - 1 - buf_pos, 100);
			lt_debug("%s: buf_pos %6d r %6d / %6d\n", __func__,
				buf_pos, (int)r, BUFSIZE - 1 - buf_pos);
			if (r < 0)
			{
				if (errno != EAGAIN)
				{
					lt_info("%s: read failed: %m\n", __func__);
					exit_flag = RECORD_FAILED_READ;
					break;
				}
				lt_info("%s: EAGAIN\n", __func__);
			}
			else
				buf_pos += r;
		}
		else
			lt_info("%s: buffer full! Overflow?\n", __func__);
		if (buf_pos > (BUFSIZE / 3)) /* start writeout */
		{
			size_t towrite = BUFSIZE / 2;
			if (buf_pos < BUFSIZE / 2)
				towrite = buf_pos;
			r = write(file_fd, buf, towrite);
			if (r < 0)
			{
				exit_flag = RECORD_FAILED_FILE;
				lt_info("%s: write error: %m\n", __func__);
				break;
			}
			buf_pos -= r;
			memmove(buf, buf + r, buf_pos);
			lt_debug("%s: buf_pos %6d w %6d / %6d\n", __func__, buf_pos, (int)r, (int)towrite);
#if 0
			if (fdatasync(file_fd))
				perror("cRecord::FileThread() fdatasync");
#endif
			if (posix_fadvise(file_fd, 0, 0, POSIX_FADV_DONTNEED))
				perror("posix_fadvise");
		}
	}
	dmx->Stop();
	while (buf_pos > 0) /* write out the unwritten buffer content */
	{
		r = write(file_fd, buf, buf_pos);
		if (r < 0)
		{
			exit_flag = RECORD_FAILED_FILE;
			lt_info("%s: write error: %m\n", __func__);
			break;
		}
		buf_pos -= r;
		memmove(buf, buf + r, buf_pos);
	}
	free(buf);

#if 0
	// TODO: do we need to notify neutrino about failing recording?
	CEventServer eventServer;
	eventServer.registerEvent2(NeutrinoMessages::EVT_RECORDING_ENDED, CEventServer::INITID_NEUTRINO, "/tmp/neutrino.sock");
	stream2file_status2_t s;
	s.status = exit_flag;
	strncpy(s.filename,basename(myfilename),512);
	s.filename[511] = '\0';
	strncpy(s.dir,dirname(myfilename),100);
	s.dir[99] = '\0';
	eventServer.sendEvent(NeutrinoMessages::EVT_RECORDING_ENDED, CEventServer::INITID_NEUTRINO, &s, sizeof(s));
	printf("[stream2file]: pthreads exit code: %i, dir: '%s', filename: '%s' myfilename: '%s'\n", exit_flag, s.dir, s.filename, myfilename);
#endif

	lt_info("%s: end", __func__);
	pthread_exit(NULL);
}

