#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include "playback_td.h"
#include "dmx_td.h"
#include "audio_td.h"
#include "video_td.h"
#include "lt_debug.h"
#define lt_debug(args...) _lt_debug(TRIPLE_DEBUG_PLAYBACK, this, args)
#define lt_info(args...)  _lt_info(TRIPLE_DEBUG_PLAYBACK, this, args)
#define lt_info_c(args...) _lt_info(TRIPLE_DEBUG_PLAYBACK, NULL, args)

#include <tddevices.h>
#define DVR	"/dev/" DEVICE_NAME_PVR

static int mp_syncPES(uint8_t *, int, bool quiet = false);
static int sync_ts(uint8_t *, int);
static inline uint16_t get_pid(uint8_t *buf);
static void *start_playthread(void *c);
static void playthread_cleanup_handler(void *);

static pthread_cond_t playback_ready_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t playback_ready_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t currpos_mutex = PTHREAD_MUTEX_INITIALIZER;

static int dvrfd = -1;
static int streamtype;

extern cDemux *videoDemux;
extern cDemux *audioDemux;
extern cVideo *videoDecoder;
extern cAudio *audioDecoder;

static const char *FILETYPE[] = {
	"FILETYPE_UNKNOWN",
	"FILETYPE_TS",
	"FILETYPE_MPG",
	"FILETYPE_VDR"
};

cPlayback::cPlayback(int)
{
	lt_debug("%s\n", __FUNCTION__);
	thread_started = false;
	inbuf = NULL;
	pesbuf = NULL;
	filelist.clear();
	curr_fileno = -1;
	in_fd = -1;
	streamtype = 0;
}

cPlayback::~cPlayback()
{
	lt_debug("%s\n", __FUNCTION__);
	Close();
}


bool cPlayback::Open(playmode_t mode)
{
	static const char *PMODE[] = {
		"PLAYMODE_TS",
		"PLAYMODE_FILE"
	};

	lt_debug("%s: PlayMode = %s\n", __FUNCTION__, PMODE[mode]);
	thread_started = false;
	playMode = mode;
	filetype = FILETYPE_TS;
	playback_speed = 0;
	last_size = 0;
	_pts_end = 0;
	astreams.clear();
	memset(&cc, 0, 256);
	return true;
}

//Used by Fileplay
void cPlayback::Close(void)
{
	lt_info("%s\n", __FUNCTION__);
	playstate = STATE_STOP;
	if (thread_started)
	{
		lt_info("%s: before pthread_join\n", __FUNCTION__);
		pthread_join(thread, NULL);
	}
	thread_started = false;
	lt_info("%s: after pthread_join\n", __FUNCTION__);
	mf_close();
	filelist.clear();

	if (inbuf)
		free(inbuf);
	inbuf = NULL;
	if (pesbuf)
		free(pesbuf);
	pesbuf = NULL;
	//Stop();
}

bool cPlayback::Start(char *filename, unsigned short vp, int vtype, unsigned short ap, int _ac3, unsigned int)
{
	struct stat s;
	off_t r;
	vpid = vp;
	apid = ap;
	ac3 = _ac3;
	lt_info("%s name = '%s' vpid 0x%04hx vtype %d apid 0x%04hx ac3 %d filelist.size: %u\n",
		__FUNCTION__, filename, vpid, vtype, apid, ac3, filelist.size());
	if (!filelist.empty())
	{
		lt_info("filelist not empty?\n");
		return false;
	}
	if (stat(filename, &s))
	{
		lt_info("filename does not exist? (%m)\n");
		return false;
	}
	if (!inbuf)
		inbuf = (uint8_t *)malloc(INBUF_SIZE); /* 256 k */
	if (!inbuf)
	{
		lt_info("allocating input buffer failed (%m)\n");
		return false;
	}
	if (!pesbuf)
		pesbuf = (uint8_t *)malloc(PESBUF_SIZE); /* 128 k */
	if (!pesbuf)
	{
		lt_info("allocating PES buffer failed (%m)\n");
		return false;
	}
	filelist_t file;
	file.Name = std::string(filename);
	file.Size = s.st_size;
	if (file.Name.rfind(".ts") == file.Name.length() - 3 ||
	    file.Name.rfind(".TS") == file.Name.length() - 3)
		filetype = FILETYPE_TS;
	else
	{
		if (file.Name.rfind(".vdr") == file.Name.length() - 4)
		{
			filetype = FILETYPE_VDR;
			std::string::size_type p = file.Name.rfind("info.vdr");
			if (p == std::string::npos)
				p = file.Name.rfind("index.vdr");
			if (p != std::string::npos)
			{
				file.Name.replace(p, std::string::npos, "001.vdr");
				lt_info("replaced filename with '%s'\n", file.Name.c_str());
				if (stat(file.Name.c_str(), &s))
				{
					lt_info("filename does not exist? (%m)\n");
					return false;
				}
				file.Size = s.st_size;
			}
		}
		else
			filetype = FILETYPE_MPG;
		vpid = 0x40;
	}

	lt_info("detected (ok, guessed) filetype: %s\n", FILETYPE[filetype]);

	filelist.push_back(file);
	filelist_auto_add();
	if (mf_open(0) < 0)
		return false;

	pts_start = pts_end = pts_curr = -1;
	pesbuf_pos = 0;
	curr_pos = 0;
	inbuf_pos = 0;
	inbuf_sync = 0;
	r = mf_getsize();

	if (r > INBUF_SIZE)
	{
		if (mp_seekSync(r - INBUF_SIZE) < 0)
			return false;
		while(true) {
			if (inbuf_read() <= 0)
				break; // EOF
			if (curr_pos >= r) //just to make sure...
				break;
		}
		if (filetype == FILETYPE_TS)
			for (r = (inbuf_pos / 188) * 188; r > 0; r -= 188)
			{
				pts_end = get_pts(inbuf + r, false, inbuf_pos - r);
				if (pts_end > -1)
					break;
			}
		else
			pts_end = pts_curr;
	}
	else
		pts_end = -1; /* unknown */

	if (mp_seekSync(0) < 0)
		return false;

	pesbuf_pos = 0;
	inbuf_pos = 0;
	inbuf_sync = 0;
	while (inbuf_pos < INBUF_SIZE / 2 && inbuf_read() > 0) {};
	for (r = 0; r < inbuf_pos - 188; r += 188)
	{
		pts_start = get_pts(inbuf + r, false, inbuf_pos - r);
		if (pts_start > -1)
			break;
	}
	pts_curr = pts_start;
	bytes_per_second = -1;
	if (pts_end != -1 && pts_start > pts_end) /* PTS overflow during this file */
		pts_end += 0x200000000ULL;
	int duration = (pts_end - pts_start) / 90000;
	if (duration > 0)
		bytes_per_second = mf_getsize() / duration;
	lt_info("start: %lld end %lld duration %d bps %lld\n", pts_start, pts_end, duration, bytes_per_second);
	/* yes, we start in pause mode... */
	playback_speed = 0;
	if (pts_start == -1)
		playstate = STATE_INIT;
	else
		playstate = STATE_PAUSE;
	pthread_mutex_lock(&playback_ready_mutex);
	if (pthread_create(&thread, 0, start_playthread, this) != 0)
		lt_info("pthread_create failed\n");
	else
		pthread_cond_wait(&playback_ready_cond, &playback_ready_mutex);
	pthread_mutex_unlock(&playback_ready_mutex);
	return true;
}

static void *start_playthread(void *c)
{
	cPlayback *obj = (cPlayback *)c;
	obj->playthread();
	return NULL;
}

void cPlayback::playthread(void)
{
	thread_started = true;
	int ret, towrite;
	dvrfd = open(DVR, O_WRONLY);
	if (dvrfd < 0)
	{
		lt_info("%s open tdpvr failed: %m\n", __FUNCTION__);
		pthread_exit(NULL);
	}
	fcntl(dvrfd, F_SETFD, FD_CLOEXEC);

	pthread_cleanup_push(playthread_cleanup_handler, 0);

	ioctl(audioDemux->getFD(), DEMUX_SELECT_SOURCE, INPUT_FROM_PVR);
	if (ac3)
		audioDecoder->SetStreamType(AUDIO_FMT_DOLBY_DIGITAL);
	else
	{
		if (streamtype == 1) /* mpeg 1 */
			audioDecoder->SetStreamType(AUDIO_FMT_MPG1);
		else /* default */
			audioDecoder->SetStreamType(AUDIO_FMT_MPEG);
	}

	audioDemux->pesFilter(apid);
	videoDemux->pesFilter(vpid);

//	audioDemux->Start();
	videoDemux->Start();

//	videoDecoder->setBlank(1);
//	videoDecoder->Start();
//	audioDecoder->Start();
	/* everything is set up now, signal ::Start() that it can return */
	pthread_mutex_lock(&playback_ready_mutex);
	pthread_cond_broadcast(&playback_ready_cond);
	pthread_mutex_unlock(&playback_ready_mutex);

	while (playstate != STATE_STOP)
	{
		if (playstate == STATE_INIT)
		{
			/* hack for timeshift to determine start PTS */
			if (inbuf_read() < 0)
				break;
			usleep(100000);
			if (pts_start == -1)
				continue;
		}

		if (playback_speed == 0)
		{
			playstate = STATE_PAUSE;
			usleep(1);
			continue;
		}
		if (inbuf_read() < 0)
			break;

		/* autoselect PID for PLAYMODE_FILE */
		if (apid == 0 && astreams.size() > 0)
		{
			for (std::map<uint16_t, AStream>::iterator aI = astreams.begin(); aI != astreams.end(); aI++)
			{
				if (!aI->second.ac3)
				{
					apid = aI->first;
					lt_info("%s setting Audio pid to 0x%04hx\n", __FUNCTION__, apid);
					SetAPid(apid, 0);
					break;
				}
			}
		}

		towrite = inbuf_pos / 188 * 188; /* TODO: smaller chunks? */
		if (towrite == 0)
			continue;
 retry:
		ret = write(dvrfd, inbuf, towrite);
		if (ret < 0)
		{
			if (errno == EAGAIN && playstate != STATE_STOP)
				goto retry;
			lt_info("%s write dvr failed: %m\n", __FUNCTION__);
			break;
		}
		memmove(inbuf, inbuf + ret, inbuf_pos - ret);
		inbuf_pos -= ret;
	}

	pthread_cleanup_pop(1);
	pthread_exit(NULL);
}

static void playthread_cleanup_handler(void *)
{
	lt_info_c("%s\n", __FUNCTION__);
	ioctl(audioDemux->getFD(), DEMUX_SELECT_SOURCE, INPUT_FROM_CHANNEL0);
	audioDemux->Stop();
	videoDemux->Stop();
	audioDecoder->Stop();
	videoDecoder->Stop();
	close(dvrfd);
	dvrfd = -1;
}

bool cPlayback::SetAPid(unsigned short pid, int _ac3)
{
	lt_info("%s pid: 0x%04hx ac3: %d\n", __FUNCTION__, pid, _ac3);
	apid = pid;
	ac3 = _ac3;

	audioDemux->Stop();
	audioDecoder->Stop();
	videoDemux->Stop();
	videoDecoder->Stop(false);

	if (ac3)
		audioDecoder->SetStreamType(AUDIO_FMT_DOLBY_DIGITAL);
	else
	{
		if (streamtype == 1) /* mpeg 1 */
			audioDecoder->SetStreamType(AUDIO_FMT_MPG1);
		else /* default */
			audioDecoder->SetStreamType(AUDIO_FMT_MPEG);
	}
	audioDemux->pesFilter(apid);

	videoDemux->Start();
	audioDemux->Start();
	audioDecoder->Start();
	videoDecoder->Start();
	return true;
}

bool cPlayback::SetSpeed(int speed)
{
	lt_info("%s speed = %d\n", __FUNCTION__, speed);
	if (speed < 0)
		speed = 1; /* fast rewind not yet implemented... */
	if (speed == 1 && playback_speed != 1)
	{
		if (playback_speed == 0)
		{
			videoDemux->Stop();
			videoDemux->Start();
			audioDemux->Start();
		}
		else
		{
			audioDecoder->Stop();
			videoDecoder->Stop();
		}
		audioDecoder->Start();
		videoDecoder->Start();
		playstate = STATE_PLAY;
	}
	if (playback_speed == 1 && speed > 1)
	{
		audioDecoder->mute(false);
		videoDecoder->FastForwardMode();
	}
	playback_speed = speed;
	if (playback_speed == 0)
	{
		audioDecoder->Stop();
		audioDemux->Stop();
		videoDecoder->Stop(false);
	}
	return true;
}

bool cPlayback::GetSpeed(int &speed) const
{
	lt_debug("%s\n", __FUNCTION__);
	speed = playback_speed;
	return true;
}

// in milliseconds
bool cPlayback::GetPosition(int &position, int &duration)
{
	int64_t tmppts;
	lt_debug("%s\n", __FUNCTION__);
	off_t currsize = mf_getsize();
	bool update = false;
	/* handle a growing file, e.g. for timeshift.
	   this might be pretty expensive... */
	if (filetype == FILETYPE_TS && filelist.size() == 1)
	{
		off_t tmppos = currsize - PESBUF_SIZE;
		if (currsize > last_size && (currsize - last_size) < 10485760 &&
		    bytes_per_second > 0 && _pts_end > 0)
		{
			/* guess the current endpts... */
			tmppts = (currsize - last_size) * 90000 / bytes_per_second;
			pts_end = _pts_end + tmppts;
		}
		else if (currsize != last_size && tmppos > 0)
		{
			pthread_mutex_lock(&currpos_mutex);
			off_t oldpos = curr_pos;
			ssize_t n, r;
			int s;
			mf_lseek(tmppos);
			n = read(in_fd, pesbuf, PESBUF_SIZE); /* abuse the pesbuf... */
			s = sync_ts(pesbuf, n);
			if (s >= 0)
			{
				n -= s;
				for (r = (n / 188) * 188; r > 0; r -= 188)
				{
					tmppts = get_pts(pesbuf + r + s, false, n - r);
					if (tmppts > -1)
					{
						lt_debug("n: %d s: %d endpts %lld size: %lld\n", n, s, tmppts, currsize);
						pts_end = tmppts;
						_pts_end = tmppts;
						update = true;
						/* file size has changed => update endpts */
						last_size = currsize;
						break;
					}
				}
			}
			mf_lseek(oldpos);
			pthread_mutex_unlock(&currpos_mutex);
		}
	}
	if (pts_end != -1 && pts_start > pts_end) /* should trigger only once ;) */
	{
		pts_end += 0x200000000ULL;
		update = true;
	}

	if (pts_curr != -1 && pts_curr < pts_start)
		tmppts = pts_curr + 0x200000000ULL - pts_start;
	else
		tmppts = pts_curr - pts_start;
	if (pts_end != -1 && pts_curr != -1)
	{
		position = tmppts / 90;
		duration = (pts_end - pts_start) / 90;
		if (update && duration >= 4000)
		{
			bytes_per_second = currsize / (duration / 1000);
			lt_debug("%s: updated bps: %lld size: %lld duration %d\n",
					__FUNCTION__, bytes_per_second, currsize, duration);
		}
		return true;
	}
	position = 0;
	duration = 0;
	return false;
}

bool cPlayback::SetPosition(int position, bool absolute)
{
	lt_info("%s pos = %d abs = %d\n", __FUNCTION__, position, absolute);
	int currpos, target, duration, oldspeed;
	bool ret;

	if (absolute)
		target = position;
	else
	{
		GetPosition(currpos, duration);
		target = currpos + position;
		lt_info("current position %d target %d\n", currpos, target);
	}

	oldspeed = playback_speed;
//	if (oldspeed != 0)
		SetSpeed(0);		/* request pause */

	while (playstate == STATE_PLAY)	/* playthread did not acknowledge pause */
		usleep(1);
	if (playstate == STATE_STOP)	/* we did get stopped by someone else */
		return false;

	ret = (seek_to_pts(target * 90) > 0);

	if (oldspeed != 0)
	{
		SetSpeed(oldspeed);
		/* avoid ugly artifacts */
		videoDecoder->Stop();
		videoDecoder->Start();
	}
	return ret;
}

void cPlayback::FindAllPids(uint16_t *apids, unsigned short *ac3flags, uint16_t *numpida, std::string *language)
{
	lt_info("%s\n", __FUNCTION__);
	int i = 0;
	for (std::map<uint16_t, AStream>::iterator aI = astreams.begin(); aI != astreams.end(); aI++)
	{
		apids[i] = aI->first;
		ac3flags[i] = aI->second.ac3 ? 1 : 0;
		language[i] = aI->second.lang;
		i++;
		if (i > 10)	/* REC_MAX_APIDS in vcrcontrol.h */
			break;
	}
	*numpida = i;
}

off_t cPlayback::seek_to_pts(int64_t pts)
{
	off_t newpos = curr_pos;
	int64_t tmppts, ptsdiff;
	int count = 0;
	if (pts_start < 0 || pts_end < 0 || bytes_per_second < 0)
	{
		lt_info("%s pts_start (%lld) or pts_end (%lld) or bytes_per_second (%lld) not initialized\n",
			__FUNCTION__, pts_start, pts_end, bytes_per_second);
		return -1;
	}
	/* sanity check: buffer is without locking, so we must only seek while in pause mode */
	if (playstate != STATE_PAUSE)
	{
		lt_info("%s playstate (%d) != STATE_PAUSE, not seeking\n", __FUNCTION__, playstate);
		return -1;
	}

	/* tmppts is normalized current pts */
	if (pts_curr < pts_start)
		tmppts = pts_curr + 0x200000000ULL - pts_start;
	else
		tmppts = pts_curr - pts_start;
	while (abs(pts - tmppts) > 90000LL && count < 10)
	{
		count++;
		ptsdiff = pts - tmppts;
		newpos += ptsdiff * bytes_per_second / 90000;
		lt_info("%s try #%d seek from %lldms to %lldms dt %lldms pos %lldk newpos %lldk kB/s %lld\n",
			__FUNCTION__, count, tmppts / 90, pts / 90, ptsdiff / 90, curr_pos / 1024, newpos / 1024, bytes_per_second / 1024);
		if (newpos < 0)
			newpos = 0;
		newpos = mp_seekSync(newpos);
		if (newpos < 0)
			return newpos;
		inbuf_pos = 0;
		inbuf_sync = 0;
		while (inbuf_pos < INBUF_SIZE * 8 / 10) {
			if (inbuf_read() <= 0)
				break; // EOF
		}
		if (pts_curr < pts_start)
			tmppts = pts_curr + 0x200000000ULL - pts_start;
		else
			tmppts = pts_curr - pts_start;
	}
	lt_info("%s end after %d tries, ptsdiff now %lld sec\n", __FUNCTION__, count, (pts - tmppts) / 90000);
	return newpos;
}

bool cPlayback::filelist_auto_add()
{
	if (filelist.size() != 1)
		return false;

	const char *filename = filelist[0].Name.c_str();
	const char *ext;
	ext = strrchr(filename, '.');	// FOO-xxx-2007-12-31.001.ts <- the dot before "ts"
					// 001.vdr <- the dot before "vdr"
	// check if there is something to do...
	if (! ext)
		return false;
	if (!((ext - 7 >= filename && !strcmp(ext, ".ts") && *(ext - 4) == '.') ||
	      (ext - 4 >= filename && !strcmp(ext, ".vdr"))))
		return false;

	int num = 0;
	struct stat s;
	size_t numpos = strlen(filename) - strlen(ext) - 3;
	sscanf(filename + numpos, "%d", &num);
	do {
		num++;
		char nextfile[strlen(filename) + 1]; /* todo: use fixed buffer? */
		memcpy(nextfile, filename, numpos);
		sprintf(nextfile + numpos, "%03d%s", num, ext);
		if (stat(nextfile, &s))
			break; // file does not exist
		filelist_t file;
		file.Name = std::string(nextfile);
		file.Size = s.st_size;
		lt_info("%s auto-adding '%s' to playlist\n", __FUNCTION__, nextfile);
		filelist.push_back(file);
	} while (true && num < 999);

	return (filelist.size() > 1);
}

/* the mf_* functions are wrappers for multiple-file I/O */
int cPlayback::mf_open(int fileno)
{
	if (filelist.empty())
		return -1;

	if (fileno >= (int)filelist.size())
		return -1;

	mf_close();

	in_fd = open(filelist[fileno].Name.c_str(), O_RDONLY);
	fcntl(in_fd, F_SETFD, FD_CLOEXEC);
	if (in_fd != -1)
		curr_fileno = fileno;

	return in_fd;
}

int cPlayback::mf_close(void)
{
	int ret = 0;
	lt_info("%s in_fd = %d curr_fileno = %d\n", __FUNCTION__, in_fd, curr_fileno);
	if (in_fd != -1)
		ret = close(in_fd);
	in_fd = curr_fileno = -1;

	return ret;
}

off_t cPlayback::mf_getsize(void)
{
	off_t ret = 0;
	if (filelist.size() == 1 && in_fd != -1)
	{
		/* for timeshift, we need to deal with a growing file... */
		struct stat st;
		if (fstat(in_fd, &st) == 0)
			return st.st_size;
		/* else, fallback to filelist.size() */
	}
	for (unsigned int i = 0; i < filelist.size(); i++)
		ret += filelist[i].Size;
	return ret;
}

off_t cPlayback::mf_lseek(off_t pos)
{
	off_t offset = 0, lpos = pos, ret;
	unsigned int fileno;
	/* this is basically needed for timeshifting - to allow
	   growing files to be handled... */
	if (filelist.size() == 1 && filetype == FILETYPE_TS)
	{
		if (lpos > mf_getsize())
			return -2;
		fileno = 0;
	}
	else
	{
		for (fileno = 0; fileno < filelist.size(); fileno++)
		{
			if (lpos < filelist[fileno].Size)
				break;
			offset += filelist[fileno].Size;
			lpos   -= filelist[fileno].Size;
		}
		if (fileno == filelist.size())
			return -2;	// EOF
	}

	if ((int)fileno != curr_fileno)
	{
		lt_info("%s old fileno: %d new fileno: %d, offset: %lld\n", __FUNCTION__, curr_fileno, fileno, (long long)lpos);
		in_fd = mf_open(fileno);
		if (in_fd < 0)
		{
			lt_info("cannot open file %d:%s (%m)\n", fileno, filelist[fileno].Name.c_str());
			return -1;
		}
	}

	ret = lseek(in_fd, lpos, SEEK_SET);
	if (ret < 0)
		return ret;

	curr_pos = offset + ret;
	return curr_pos;
}

/* gets the PTS at a specific file position from a PES
   ATTENTION! resets buf!  */
int64_t cPlayback::get_PES_PTS(uint8_t *buf, int len, bool last)
{
	int64_t pts = -1;
	int off, plen;
	uint8_t *p;

	off = mp_syncPES(buf, len);

	if (off < 0)
		return off;

	p = buf + off;
	while (off < len - 14 && (pts == -1 || last))
	{
		plen = ((p[4] << 8) | p[5]) + 6;

		switch(p[3])
		{
			int64_t tmppts;
			case 0xe0 ... 0xef:	// video!
				tmppts = get_pts(p, true, len - off);
				if (tmppts >= 0)
					pts = tmppts;
				break;
			case 0xbb:
			case 0xbe:
			case 0xbf:
			case 0xf0 ... 0xf3:
			case 0xff:
			case 0xc0 ... 0xcf:
			case 0xd0 ... 0xdf:
				break;
			case 0xb9:
			case 0xba:
			case 0xbc:
			default:
				plen = 1;
				break;
		}
		p += plen;
		off += plen;
	}
	return pts;
}

ssize_t cPlayback::inbuf_read()
{
	if (filetype == FILETYPE_UNKNOWN)
		return -1;
	if (filetype == FILETYPE_TS)
		return read_ts();
	/* FILETYPE_MPG or FILETYPE_VDR */
	return read_mpeg();
}

ssize_t cPlayback::read_ts()
{
	ssize_t toread, ret = 0, sync, off;
	toread = INBUF_SIZE - inbuf_pos;
	bool retry = true;
	uint8_t *buf;
	/* fprintf(stderr, "%s:%d curr_pos %lld, inbuf_pos: %ld, toread: %ld\n",
		__FUNCTION__, __LINE__, (long long)curr_pos, (long)inbuf_pos, (long)toread); */

	if (playback_speed > 1)
	{
		sync = 0;
		ssize_t tmpread = PESBUF_SIZE / 188 * 188;
		int n, skipped = 0;
		bool skip = false;
		bool eof = true;
		pthread_mutex_lock(&currpos_mutex);
		while (toread > 0)
		{
			ssize_t done = 0;
			while (done < tmpread)
			{
				ret = read(in_fd, pesbuf, tmpread - done);
				if (ret == 0 && retry) /* EOF */
				{
					mf_lseek(curr_pos);
					retry = false;
					continue;
				}
				if (ret < 0)
				{
					lt_info("%s failed1: %m\n", __FUNCTION__);
					pthread_mutex_unlock(&currpos_mutex);
					return ret;
				}
				if (ret == 0 && eof)
					goto out;
				eof = false;
				done += ret;
				curr_pos += ret;
			}
			sync = sync_ts(pesbuf, ret);
			if (sync != 0)
			{
				lt_info("%s out of sync: %d\n", __FUNCTION__, sync);
				if (sync < 0)
				{
					pthread_mutex_unlock(&currpos_mutex);
					return -1;
				}
				memmove(pesbuf, pesbuf + sync, ret - sync);
				if (pesbuf[0] != 0x47)
					lt_info("%s:%d??????????????????????????????\n", __FUNCTION__, __LINE__);
			}
			for (n = 0; n < done / 188 * 188; n += 188)
			{
				buf = pesbuf + n;
				if (buf[1] & 0x40) // PUSI
				{
					/* only video packets... */
					int of = 4;
					if (buf[3] & 0x20) // adaptation field
						of += buf[4] + 1;
					if ((buf[of + 3] & 0xF0) == 0xE0 && // Video stream
					    buf[of + 2] == 0x01 && buf[of + 1] == 0x00 && buf[of] == 0x00) // PES
					{
						skip = true;
						skipped++;
						if (skipped >= playback_speed)
						{
							skipped = 0;
							skip = false;
						}
					}
				}
				if (! skip)
				{
					memcpy(inbuf + inbuf_pos, buf, 188);
					inbuf_pos += 188;
					toread -= 188;
					if (toread <= 0)
					{
						/* the output buffer is full, discard the input :-( */
						if (done - n > 0)
						{
							lt_debug("%s not done: %d, resetting filepos\n",
								__FUNCTION__, done - n);
							mf_lseek(curr_pos - (done - n));
						}
						break;
					}
				}
			}
		}
 out:
		pthread_mutex_unlock(&currpos_mutex);
		if (eof)
			return 0;
	}
	else
	{
		pthread_mutex_lock(&currpos_mutex);
		while(true)
		{
			ret = read(in_fd, inbuf + inbuf_pos, toread);
			if (ret == 0 && retry) /* EOF */
			{
				mf_lseek(curr_pos);
				retry = false;
				continue;
			}
			break;
		}
		if (ret <= 0)
		{
			pthread_mutex_unlock(&currpos_mutex);
			if (ret < 0)
				lt_info("%s failed2: %m\n", __FUNCTION__);
			return ret;
		}
		inbuf_pos += ret;
		curr_pos += ret;
		pthread_mutex_unlock(&currpos_mutex);

		sync = sync_ts(inbuf + inbuf_sync, INBUF_SIZE - inbuf_sync);
		if (sync < 0)
		{
			lt_info("%s cannot sync\n", __FUNCTION__);
			return ret;
		}
		inbuf_sync += sync;
	}
	/* check for A/V PIDs */
	uint16_t pid;
	int i;
	int64_t pts;
	//fprintf(stderr, "inbuf_pos: %ld - sync: %ld, inbuf_syc: %ld\n", (long)inbuf_pos, (long)sync, (long)inbuf_sync);
	int synccnt = 0;
	for (i = 0; i < inbuf_pos - inbuf_sync - 13;) {
		buf = inbuf + inbuf_sync + i;
		if (*buf != 0x47)
		{
			synccnt++;
			i++;
			continue;
		}
		if (synccnt)
			lt_info("%s TS went out of sync %d\n", __FUNCTION__, synccnt);
		synccnt = 0;
		if (!(buf[1] & 0x40))	/* PUSI */
		{
			i += 188;
			continue;
		}
		off = 0;
		if (buf[3] & 0x20)	/* adaptation field? */
			off = buf[4] + 1;
		pid = get_pid(buf + 1);
		/* PES signature is at buf + 4, streamtype is after 00 00 01 */
		switch (buf[4 + 3 + off])
		{
		case 0xe0 ... 0xef:	/* video stream */
			if (vpid == 0)
				vpid = pid;
			pts = get_pts(buf + 4 + off, true, inbuf_pos - inbuf_sync - i - off - 4);
			if (pts < 0)
				break;
			pts_curr = pts;
			if (pts_start < 0)
			{
				lt_info("%s updating pts_start to %lld ", __FUNCTION__, pts);
				pts_start = pts;
				if (pts_end > -1)
				{
					if (pts_end < pts_start)
					{
						pts_end += 0x200000000ULL;
						fprintf(stderr, "pts_end to %lld ", pts_end);
					}
					int duration = (pts_end - pts_start) / 90000;
					if (duration > 0)
					{
						bytes_per_second = (mf_getsize() - curr_pos) / duration;
						fprintf(stderr, "bytes_per_second to %lldk duration to %ds at %lldk",
						bytes_per_second / 1024, duration, curr_pos / 1024);
					}
				}
				fprintf(stderr, "\n");
			}
			break;
		case 0xbd:		/* private stream 1 - ac3 */
		case 0xc0 ... 0xdf:	/* audio stream */
			if (astreams.find(pid) != astreams.end())
				break;
			AStream tmp;
			if (buf[7 + off] == 0xbd)
			{
				if (buf[12 + off] == 0x24)	/* 0x24 == TTX */
					break;
				tmp.ac3 = true;
			}
			else
				tmp.ac3 = false;
			tmp.lang = "";
			astreams.insert(std::make_pair(pid, tmp));
			lt_info("%s found apid #%d 0x%04hx ac3:%d\n", __func__, astreams.size(), pid, tmp.ac3);
			break;
		}
		i += 188;
	}

	// fprintf(stderr, "%s:%d ret %ld\n", __FUNCTION__, __LINE__, (long long)ret);
	return ret;
}

ssize_t cPlayback::read_mpeg()
{
	ssize_t toread, ret, sync;
	//toread = PESBUF_SIZE - pesbuf_pos;
	/* experiments found, that 80kB is the best buffer size, otherwise a/v sync seems
	   to suffer and / or audio stutters */
	toread = 80 * 1024 - pesbuf_pos;
	bool retry = true;

	if (INBUF_SIZE - inbuf_pos < toread)
	{
		lt_info("%s inbuf full, setting toread to %d (old: %zd)\n", __FUNCTION__, INBUF_SIZE - inbuf_pos, toread);
		toread = INBUF_SIZE - inbuf_pos;
	}
	pthread_mutex_lock(&currpos_mutex);
	while(true)
	{
		ret = read(in_fd, pesbuf + pesbuf_pos, toread);
		if (ret == 0 && retry) /* EOF */
		{
			mf_lseek(curr_pos);
			retry = false;
			continue;
		}
		break;
	}
	if (ret < 0)
	{
		pthread_mutex_unlock(&currpos_mutex);
		lt_info("%s failed: %m, pesbuf_pos: %zd, toread: %zd\n", __FUNCTION__, pesbuf_pos, toread);
		return ret;
	}
	pesbuf_pos += ret;
	curr_pos += ret;
	pthread_mutex_unlock(&currpos_mutex);

	int count = 0;
	uint16_t pid = 0;
	bool resync = true;
	while (count < pesbuf_pos - 10)
	{
		if (resync)
		{
			sync = mp_syncPES(pesbuf + count, pesbuf_pos - count - 10);
			if (sync < 0)
			{
				if (pesbuf_pos - count - 10 > 4)
					lt_info("%s cannot sync (count=%d, pesbuf_pos=%zd)\n",
						__FUNCTION__, count, pesbuf_pos);
				break;
			}
			if (sync)
				lt_info("%s needed sync %zd\n", __FUNCTION__, sync);
			count += sync;
		}
		uint8_t *ppes = pesbuf + count;
		int av = 0; // 1 = video, 2 = audio
		int64_t pts;
		switch(ppes[3])
		{
			case 0xba: //pack header;
				// fprintf(stderr, "pack start code, 0x%02x\n", ppes[4]);
				if ((ppes[4] & 0xf0) == 0x20)		/* mpeg 1 */
				{
					streamtype = 1;	/* for audio setup */
					count += 12;
				}
				else if ((ppes[4] & 0xc0) == 0x40)	/* mpeg 2 */
				{
					streamtype = 0;
					count += 14; /* correct: 14 + (ppes[13] & 0x07) */
				}
				else
				{
					lt_info("%s weird pack header: 0x%2x\n", __FUNCTION__, ppes[4]);
					count++;
				}
				resync = true;
				continue;
				break;
			case 0xbd: // AC3
			{
				int off = ppes[8] + 8 + 1; // ppes[8] is often 0
				if (count + off >= pesbuf_pos)
					break;
				uint16_t subid = ppes[off];
				// if (offset == 0x24 && subid == 0x10 ) // TTX?
				if (subid < 0x80 || subid > 0x87)
					break;
				lt_debug("AC3: ofs 0x%02x subid 0x%02x\n", off, subid);
				//subid -= 0x60; // normalize to 32...39 (hex 0x20..0x27)

				if (astreams.find(subid) == astreams.end())
				{
					AStream tmp;
					tmp.ac3 = true;
					tmp.lang = "";
					astreams.insert(std::make_pair(subid, tmp));
					lt_info("%s found aid: %02x\n", __FUNCTION__, subid);
				}
				pid = subid;
				av = 2;
				break;
			}
			case 0xbb:
			case 0xbe:
			case 0xbf:
			case 0xf0 ... 0xf3:
			case 0xff:
				//skip = (ppes[4] << 8 | ppes[5]) + 6;
				//DBG("0x%02x header, skip = %d\n", ppes[3], skip);
				break;
			case 0xc0 ... 0xcf:
			case 0xd0 ... 0xdf:
			{
				// fprintf(stderr, "audio stream 0x%02x\n", ppes[3]);
				uint16_t id = ppes[3];
				if (astreams.find(id) == astreams.end())
				{
					AStream tmp;
					tmp.ac3 = false;
					tmp.lang = "";
					astreams.insert(std::make_pair(id, tmp));
					lt_info("%s found aid: %02x\n", __FUNCTION__, id);
				}
				pid = id;
				av = 2;
				break;
			}
			case 0xe0 ... 0xef:
				// fprintf(stderr, "video stream 0x%02x, %02x %02x \n", ppes[3], ppes[4], ppes[5]);
				pid = 0x40;
				av = 1;
				pts = get_pts(ppes, true, pesbuf_pos - count);
				if (pts < 0)
					break;
				pts_curr = pts;
				if (pts_start < 0)
					pts_start = pts;
				break;
			case 0xb9:
			case 0xbc:
				lt_debug("%s:%d %s\n", __FUNCTION__, __LINE__,
					(ppes[3] == 0xb9) ? "program_end_code" : "program_stream_map");
				//resync = true;
				// fallthrough. TODO: implement properly.
			default:
				//if (! resync)
				//	DBG("Unknown stream id: 0x%X.\n", ppes[3]);
				count++;
				resync = true;
				continue;
				break;
		}

		int pesPacketLen = ((ppes[4] << 8) | ppes[5]) + 6;
		if (count + pesPacketLen >= pesbuf_pos)
		{
			lt_debug("buffer len: %ld, pesPacketLen: %d :-(\n", pesbuf_pos - count, pesPacketLen);
			break;
		}

		int tsPacksCount = pesPacketLen / 184;
		if ((tsPacksCount + 1) * 188 > INBUF_SIZE - inbuf_pos)
		{
			lt_info("not enough size in inbuf (needed %d, got %d)\n", (tsPacksCount + 1) * 188, INBUF_SIZE - inbuf_pos);
			break;
		}

		if (av)
		{
			int rest = pesPacketLen % 184;

			// divide PES packet into small TS packets
			uint8_t pusi = 0x40;
			int j;
			uint8_t *ts = inbuf + inbuf_pos;
			for (j = 0; j < tsPacksCount; j++)
			{
				ts[0] = 0x47;				// SYNC Byte
				ts[1] = pusi;				// Set PUSI if first packet
				ts[2] = pid;				// PID (low)
				ts[3] = 0x10 | (cc[pid] & 0x0F);	// No adaptation field, payload only, continuity counter
				cc[pid]++;
				memcpy(ts + 4, ppes + j * 184, 184);
				pusi = 0x00;				// clear PUSI
				ts += 188;
				inbuf_pos += 188;
			}

			if (rest > 0)
			{
				ts[0] = 0x47;				// SYNC Byte
				ts[1] = pusi;				// Set PUSI or
				ts[2] = pid;				// PID (low)
				ts[3] = 0x30 | (cc[pid] & 0x0F);	// adaptation field, payload, continuity counter
				cc[pid]++;
				ts[4] = 183 - rest;
				if (ts[4] > 0)
				{
					ts[5] = 0x00;
					memset(ts + 6, 0xFF, ts[4] - 1);
				}
				memcpy(ts + 188 - rest, ppes + j * 184, rest);
				inbuf_pos += 188;
			}
		} //if (av)

		count += pesPacketLen;
	}
	memmove(pesbuf, pesbuf + count, pesbuf_pos - count);
	pesbuf_pos -= count;
	return ret;
}

//== seek to pos with sync to next proper TS packet ==
//== returns offset to start of TS packet or actual ==
//== pos on failure.                                ==
//====================================================
off_t cPlayback::mp_seekSync(off_t pos)
{
	off_t npos = pos;
	off_t ret;
	uint8_t pkt[1024];

	pthread_mutex_lock(&currpos_mutex);
	ret = mf_lseek(npos);
	if (ret < 0)
		lt_info("%s:%d lseek ret < 0 (%m)\n", __FUNCTION__, __LINE__);

	if (filetype != FILETYPE_TS)
	{
		int offset = 0;
		int s;
		ssize_t r;
		bool retry = false;
		while (true)
		{
			r = read(in_fd, &pkt[offset], 1024 - offset);
			if (r < 0)
			{
				lt_info("%s read failed: %m\n", __FUNCTION__);
				break;
			}
			if (r == 0) // EOF?
			{
				if (retry)
					break;
				if (mf_lseek(npos) < 0) /* next file in list? */
				{
					lt_info("%s:%d lseek ret < 0 (%m)\n", __FUNCTION__, __LINE__);
					break;
				}
				retry = true;
				continue;
			}
			s = mp_syncPES(pkt, r + offset, true);
			if (s < 0)
			{
				/* if the last 3 bytes of the buffer were 00 00 01, then
				   mp_sync_PES would not find it. So keep them and check
				   again in the next iteration */
				memmove(pkt, &pkt[r + offset - 3], 3);
				npos += r;
				offset = 3;
			}
			else
			{
				npos += s;
				lt_info("%s sync after %lld\n", __FUNCTION__, npos - pos);
				ret = mf_lseek(npos);
				pthread_mutex_unlock(&currpos_mutex);
				if (ret < 0)
					lt_info("%s:%d lseek ret < 0 (%m)\n", __FUNCTION__, __LINE__);
				return ret;
			}
			if (npos > (pos + 0x20000)) /* 128k enough? */
				break;
		}
		lt_info("%s could not sync to PES offset: %d r: %zd\n", __FUNCTION__, offset, r);
		ret = mf_lseek(pos);
		pthread_mutex_unlock(&currpos_mutex);
		return ret;
	}

	/* TODO: use bigger buffer here, too and handle EOF / next splitfile */
	while (read(in_fd, pkt, 1) > 0)
	{
		//-- check every byte until sync word reached --
		npos++;
		if (*pkt == 0x47)
		{
			//-- if found double check for next sync word --
			if (read(in_fd, pkt, 188) == 188)
			{
				if(pkt[188-1] == 0x47)
				{
					ret = mf_lseek(npos - 1); // assume sync ok
					pthread_mutex_unlock(&currpos_mutex);
					if (ret < 0)
						lt_info("%s:%d lseek ret < 0 (%m)\n", __FUNCTION__, __LINE__);
					return ret;
				}
				else
				{
					ret = mf_lseek(npos); // oops, next pkt doesn't start with sync
					if (ret < 0)
						lt_info("%s:%d lseek ret < 0 (%m)\n", __FUNCTION__, __LINE__);
				}
			}
		}

		//-- check probe limits --
		if (npos > (pos + 100 * 188))
			break;
	}

	//-- on error stay on actual position --
	ret = mf_lseek(pos);
	pthread_mutex_unlock(&currpos_mutex);
	return ret;
}

static int sync_ts(uint8_t *p, int len)
{
	int count;
	if (len < 189)
		return -1;

	count = 0;
	while (*p != 0x47 || *(p + 188) != 0x47)
	{
		count++;
		p++;
		if (count + 188 > len)
			return -1;
	}
	return count;
}

/* get the pts value from a TS or PES packet
   pes == true selects PES mode. */
int64_t cPlayback::get_pts(uint8_t *p, bool pes, int bufsize)
{
	const uint8_t *end = p + bufsize; /* check for overflow */
	if (bufsize < 14)
		return -1;
	if (!pes)
	{
		if (p[0] != 0x47)
			return -1;
		if (!(p[1] & 0x40))
			return -1;
		if (get_pid(p + 1) != vpid)
			return -1;
		if (!(p[3] & 0x10))
			return -1;

		if (p[3] & 0x20)
			p += p[4] + 4 + 1;
		else
			p += 4;

		if (p + 13 > end)
			return -1;
		/* p is now pointing at the PES header. hopefully */
		if (p[0] || p[1] || (p[2] != 1))
			return -1;
	}

	if ((p[6] & 0xC0) != 0x80) // MPEG1
	{
		p += 6;
		while (*p == 0xff)
		{
			p++;
			if (p > end)
				return -1;
		}
		if ((*p & 0xc0) == 0x40)
			p += 2;
		p -= 9; /* so that the p[9]...p[13] matches the below */
		if (p + 13 > end)
			return -1;
	}
	else
	{
		/* MPEG2 */
		if ((p[7] & 0x80) == 0) // packets with both pts, don't care for dts
		// if ((p[7] & 0xC0) != 0x80) // packets with only pts
		// if ((p[7] & 0xC0) != 0xC0) // packets with pts and dts
			return -1;
		if (p[8] < 5)
			return -1;
	}

	if (!(p[9] & 0x20))
		return -1;

	int64_t pts =
		((p[ 9] & 0x0EULL) << 29) |
		((p[10] & 0xFFULL) << 22) |
		((p[11] & 0xFEULL) << 14) |
		((p[12] & 0xFFULL) << 7) |
		((p[13] & 0xFEULL) >> 1);

	//int msec = pts / 90;
	//INFO("time: %02d:%02d:%02d\n", msec / 3600000, (msec / 60000) % 60, (msec / 1000) % 60);
	return pts;
}

/* returns: 0 == was already synchronous, > 0 == is now synchronous, -1 == could not sync */
static int mp_syncPES(uint8_t *buf, int len, bool quiet)
{
	int ret = 0;
	while (ret < len - 4)
	{
		if (buf[ret + 2] != 0x01)
		{
			ret++;
			continue;
		}
		if (buf[ret + 1] != 0x00)
		{
			ret += 2;
			continue;
		}
		if (buf[ret] != 0x00)
		{
			ret += 3;
			continue;
		}
		/* all stream IDs are > 0x80 */
		if ((buf[ret + 3] & 0x80) != 0x80)
		{
			/* we already checked for 00 00 01, if the stream ID
			   is not valid, we can skip those 3 bytes */
			ret += 3;
			continue;
		}
		return ret;
	}

	if (!quiet && len > 5) /* only warn if enough space was available... */
		lt_info_c("%s No valid PES signature found. %d Bytes deleted.\n", __FUNCTION__, ret);
	return -1;
}

static inline uint16_t get_pid(uint8_t *buf)
{
	return (*buf & 0x1f) << 8 | *(buf + 1);
}

