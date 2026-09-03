/*
 * streamprobe -- open a stream the way Neutrino opens it, and say what
 * happened.
 *
 * The point of this tool is not that it can open a URL; ffprobe does that.
 * The point is that it opens it through the *same* shared stream input core
 * that neutrino's recording and stream-relay paths use, with the same input
 * policy and the same vocabulary for protocols, stages and failure classes.
 * A developer can therefore investigate a problem stream on a PC and the
 * result says something about the box: if the input fails here, no set-top
 * decoder is going to save it, and the deploy-test cycle can be skipped.
 *
 * What it deliberately does not do: decode, play, or reimplement any of the
 * logic it is testing. Everything it prints about policy and classification
 * comes out of libstb-hal, not out of this file.
 *
 * Copyright (C) 2026 Thilo Graf
 * License: GPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>

/* Pulls in streaminput.h as well. Only translation units that already
 * compile against libavutil may include this -- see the header. */
#include <streaminput_ffmpeg.h>

#include "streamprobe_version.h"

#ifndef PACKAGE_NAME
#define PACKAGE_NAME "Tuxbox-Neutrino"
#endif
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "unknown"
#endif

/* ------------------------------------------------------------------ exits */

#define EXIT_OK       0
#define EXIT_INTERNAL 1
#define EXIT_USAGE    2

/* One exit code per failure class, written out rather than computed as
 * 10 + (int)class: the enum order belongs to libstb-hal, and a reordering
 * there must not silently renumber this tool's exit codes. Adding a class to
 * the core trips -Wswitch in its own *_name() functions; adding it here is a
 * visible edit. 0/1/2 follow the shell convention, the classes start at 10
 * and stay well below 126 so they cannot be confused with 126/127/128+N. */
static const struct
{
	int code;
	stream_failure_class_t cls;
} exit_table[] =
{
	{ 10, STREAM_FAILURE_CONNECTION_FAILED  },
	{ 11, STREAM_FAILURE_CONNECTION_TIMEOUT },
	{ 12, STREAM_FAILURE_HTTP_4XX           },
	{ 13, STREAM_FAILURE_HTTP_5XX           },
	{ 14, STREAM_FAILURE_INVALID_MANIFEST   },
	{ 15, STREAM_FAILURE_UNSUPPORTED_PROTOCOL },
	{ 16, STREAM_FAILURE_TEMPORARY_NETWORK  },
	{ 17, STREAM_FAILURE_END_OF_STREAM      },
	{ 18, STREAM_FAILURE_ABORTED            },
	{ 19, STREAM_FAILURE_UNKNOWN            },
};

static int exit_code_for(stream_failure_class_t cls)
{
	size_t i;

	for (i = 0; i < sizeof(exit_table) / sizeof(exit_table[0]); i++)
		if (exit_table[i].cls == cls)
			return exit_table[i].code;
	return EXIT_INTERNAL;
}

/* ------------------------------------------------------------ interruption */

/* The only globals in this file, and they exist because a signal handler and
 * an FFmpeg callback have nowhere else to live. Two separate states on
 * purpose: FFmpeg answers both with AVERROR_EXIT, but a user pressing Ctrl-C
 * is an abort (exit 18) while a --open-time window running out is a normal
 * end. Only the first flag may reach the classification. */
static volatile sig_atomic_t g_signal_requested;
static volatile sig_atomic_t g_deadline_expired;
static int64_t g_deadline_us; /* 0 = no deadline for the current phase */

static void on_signal(int sig)
{
	g_signal_requested = 1;
	/* One interruption is a request; a second one is an order. FFmpeg only
	 * polls the callback between transfers, so a blocking read can sit past
	 * the first Ctrl-C -- the default handler has to stay reachable. */
	signal(sig, SIG_DFL);
}

static int probe_interrupt_cb(void *opaque)
{
	(void)opaque;
	if (g_signal_requested)
		return 1;
	if (g_deadline_us && av_gettime_relative() >= g_deadline_us)
	{
		g_deadline_expired = 1;
		return 1;
	}
	return 0;
}

/* Every blocking phase sets its own deadline, or clears it. A deadline left
 * over from the previous phase would abort the next one instantly. */
static void deadline_set(int64_t usec)
{
	g_deadline_us = usec ? av_gettime_relative() + usec : 0;
	g_deadline_expired = 0;
}

/* Clears the deadline and reports whether it had fired. Reading the flag
 * after clearing it would always say no, and the phase that just ended would
 * be reported as a user abort. */
static int deadline_clear(void)
{
	int expired = g_deadline_expired ? 1 : 0;

	g_deadline_us = 0;
	g_deadline_expired = 0;
	return expired;
}

/* ------------------------------------------------------------------ options */

typedef struct
{
	const char *url;
	const char *headers;
	int live;
	int repeat;
	int open_time_s;
	int json;
	int show_url;
	int show_headers;
	int av_dump;
	int log_level; /* AV_LOG_* actually handed to libav */
} opt_t;

typedef struct
{
	int ok;
	int internal;
	int open_ok;
	int deadline_hit;
	stream_stage_t stage;
	stream_error_code_t code;
	stream_failure_class_t failure;
	int ff;
	char ff_msg[AV_ERROR_MAX_STRING_SIZE];
	int64_t open_us;
	int64_t probe_us;
	int64_t read_us;
	long packets;
	int64_t bytes;
} iter_t;

typedef struct
{
	int have;
	int stable;
	const char *container;
	const char *container_long;
	int streams;
	int64_t duration_us;
	int64_t bit_rate;
	int have_video;
	const char *vcodec;
	int width;
	int height;
	double fps;
	int have_audio;
	const char *acodec;
	int sample_rate;
	int channels;
} media_t;

/* --------------------------------------------------------------- utilities */

static void warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fputs("streamprobe: ", stderr);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}

/* Redaction is on unless the caller asked for the raw value.
 *
 * The core hides the query string, which is where tokens usually sit. It does
 * not touch the authority, and "http://user:pass@host/..." is how more than
 * one resolver delivers an authenticated stream -- so the credentials are
 * dropped here before the core is asked. This is display only; the open
 * always uses the url the caller gave. Moving it into the core would also
 * cover neutrino's own log lines and is worth doing separately.
 *
 * The buffer is sized the way the core documents it, so the marker is never
 * truncated; dropping bytes first only makes it roomier. */
static char *redact_dup(const char *url, int show)
{
	const char *scheme_end;
	const char *at;
	const char *host;
	size_t n;
	char *out;
	char *stripped = NULL;

	static const char userinfo_marker[] = "<redacted>@";

	if (show)
		return strdup(url ? url : "");

	/* An '@' counts only inside the authority, i.e. after "://" and before
	 * the next '/', '?' or '#'; an '@' in a path or a query is just a
	 * character. */
	scheme_end = url ? strstr(url, "://") : NULL;
	if (scheme_end)
	{
		host = scheme_end + 3;
		at = host;
		while (*at && *at != '/' && *at != '?' && *at != '#' && *at != '@')
			at++;
		if (*at == '@')
		{
			size_t head = (size_t)(host - url);

			/* The replacement can be longer than what it replaces --
			 * "a@" is two bytes, the marker is eleven -- so the buffer
			 * is sized for the result, not for the input. */
			stripped = malloc(head + sizeof(userinfo_marker) + strlen(at + 1));
			if (!stripped)
				return NULL;
			memcpy(stripped, url, head);
			stripped[head] = '\0';
			strcat(stripped, userinfo_marker);
			strcat(stripped, at + 1);
			url = stripped;
		}
	}

	/* Sized from the string actually handed over, which may be the longer
	 * stripped one: the core documents this size as the one that never
	 * truncates its marker. */
	n = (url ? strlen(url) : 0) + sizeof(STREAMINPUT_REDACTED_MARKER);
	out = malloc(n);
	if (!out)
	{
		free(stripped);
		return NULL;
	}
	streaminput_redact_url(url, out, n);
	free(stripped);
	return out;
}

/* The tool version says which streamprobe; the neutrino version says
 * which tree it was cut from. A bug report needs both. */
static void version_line(FILE *out)
{
	fprintf(out, "streamprobe %s (%s %s)\n", STREAMPROBE_VERSION, PACKAGE_NAME, PACKAGE_VERSION);
}

static void print_version(void)
{
	unsigned rt[3];
	unsigned bt[3];
	int mismatch = 0;
	int i;
	static const char *const names[3] = { "libavformat", "libavcodec ", "libavutil  " };

	rt[0] = avformat_version();
	rt[1] = avcodec_version();
	rt[2] = avutil_version();
	bt[0] = LIBAVFORMAT_VERSION_INT;
	bt[1] = LIBAVCODEC_VERSION_INT;
	bt[2] = LIBAVUTIL_VERSION_INT;

	version_line(stdout);
	for (i = 0; i < 3; i++)
	{
		printf("%s runtime %u.%u.%u build %u.%u.%u%s\n", names[i],
			AV_VERSION_MAJOR(rt[i]), AV_VERSION_MINOR(rt[i]), AV_VERSION_MICRO(rt[i]),
			AV_VERSION_MAJOR(bt[i]), AV_VERSION_MINOR(bt[i]), AV_VERSION_MICRO(bt[i]),
			rt[i] != bt[i] ? "  MISMATCH" : "");
		if (rt[i] != bt[i])
			mismatch = 1;
	}
	/* A binary running against different libraries than it was built with is
	 * exactly the kind of local accident a desktop probe should surface. */
	if (mismatch)
		printf("note: runtime and build versions differ; results may not describe the box\n");
}

static void usage(FILE *out)
{
	size_t i;

	fprintf(out,
		"Usage: streamprobe [options] <url>\n"
		"\n"
		"Opens <url> through the shared streaminput core -- the same functions\n"
		"neutrino's recording and stream-relay paths use -- probes it and reports\n"
		"what happened, in the core's own vocabulary.\n"
		"\n"
		"Options:\n"
		"  --headers <blob>   opaque header blob, exactly as a resolver delivers it\n"
		"  --live             mark the source live; an orderly EOF inside the read\n"
		"                     window is then a failure (so it needs --open-time)\n"
		"  --profile <name>   input profile: record (default, and the only one today)\n"
		"  --repeat <n>       run the whole open n times (default 1, no delay between\n"
		"                     runs); stops early on an interrupt or a local error\n"
		"  --open-time <s>    bound each phase to s seconds and, after a successful\n"
		"                     probe, read packets for s seconds (default 0: no bound)\n"
		"  --json             emit one JSON object on stdout and nothing else; a bad\n"
		"                     command line is still reported on stderr, without it\n"
		"  --show-url         print URLs unredacted (default: the query string is hidden)\n"
		"  --show-headers     print header and policy values (default: they are hidden)\n"
		"  --av-dump          also run FFmpeg's av_dump_format() on stderr (format not stable)\n"
		"  -v, --verbose      let FFmpeg log; it may print full URLs and headers\n"
		"  -q, --quiet        silence FFmpeg logging (the default)\n"
		"  -h, --help         this text\n"
		"  -V, --version      streamprobe and the linked FFmpeg library versions\n"
		"\n"
		"Exit codes:\n"
		"  %3d  ok                   the stream opened and probed\n"
		"  %3d  internal             out of memory or an unexpected internal error\n"
		"  %3d  usage                bad command line\n",
		EXIT_OK, EXIT_INTERNAL, EXIT_USAGE);
	/* Printed from the table, so a renamed class in the core cannot leave a
	 * stale word here. */
	for (i = 0; i < sizeof(exit_table) / sizeof(exit_table[0]); i++)
		fprintf(out, "  %3d  %s\n", exit_table[i].code,
			streaminput_failure_class_name(exit_table[i].cls));
	fprintf(out,
		"\n"
		"Notes:\n"
		"  A URL and a header blob can carry tokens, cookies and session ids. The\n"
		"  query string and any user:password in the url are hidden by default,\n"
		"  as is the header blob; --show-url, --show-headers and --verbose turn\n"
		"  that off, so do not use them when the output leaves your machine. A\n"
		"  secret elsewhere in the path is not something this can recognise.\n"
		"  --headers is visible in the process list and in shell history.\n"
		"  An https:// URL reported as unsupported-protocol means the linked FFmpeg\n"
		"  was built without TLS support, not that the server is at fault.\n"
		"  --repeat has no backoff. Do not point it at someone else's server.\n"
		"  An interrupt (Ctrl-C) ends the run as 'aborted'; a second one is passed\n"
		"  to the default handler and kills the process without a report.\n"
		"  Iterations are numbered from 1 in the report and from 0 in the JSON,\n"
		"  where an index is an array position.\n"
		"  --open-time is a deadline FFmpeg checks between i/o operations, not a\n"
		"  hard kill: on a local file that blocks inside open() it can be missed\n"
		"  entirely. Network sources, the ones this is for, honour it promptly.\n"
		"  A phase can also finish successfully after the bound expired, with a\n"
		"  usable result; the report then says so rather than calling it an\n"
		"  error. Look for 'Deadline' in the report, deadline_hit in the JSON.\n");
}

static void usage_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fputs("streamprobe: ", stderr);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
	fputs("Try 'streamprobe --help' for the full option list.\n", stderr);
	exit(EXIT_USAGE);
}

/* Accepts "--opt value" and "--opt=value". Returns 1 when argv[*i] is opt.
 * A missing value is a usage error naming the option, never a silent skip. */
static int opt_val(int argc, char **argv, int *i, const char *opt, const char **val)
{
	size_t n = strlen(opt);

	if (strncmp(argv[*i], opt, n) != 0)
		return 0;
	if (argv[*i][n] == '=')
	{
		*val = argv[*i] + n + 1;
		return 1;
	}
	if (argv[*i][n] != '\0')
		return 0;
	if (*i + 1 >= argc)
		usage_error("%s needs a value", opt);
	*val = argv[++(*i)];
	return 1;
}

static int opt_int(const char *opt, const char *val, int min, int max)
{
	char *end;
	long v;

	errno = 0;
	v = strtol(val, &end, 10);
	/* The value is not quoted back: a mistyped option can be given a url, and
	 * an error message is output like any other. */
	if (*val == '\0' || *end != '\0' || errno != 0 || v < min || v > max)
		usage_error("%s wants an integer between %d and %d", opt, min, max);
	return (int)v;
}

static void parse_args(int argc, char **argv, opt_t *o)
{
	int i;
	int end_of_options = 0;
	const char *val;

	memset(o, 0, sizeof(*o));
	o->repeat = 1;
	o->log_level = AV_LOG_QUIET;

	/* Called with nothing at all: say what this binary is before the usage
	 * error does. It goes to stderr like that error, so a failed run still
	 * leaves stdout empty. */
	if (argc == 1)
		version_line(stderr);

	for (i = 1; i < argc; i++)
	{
		if (!end_of_options && strcmp(argv[i], "--") == 0)
		{
			end_of_options = 1;
			continue;
		}
		if (!end_of_options && argv[i][0] == '-' && argv[i][1] != '\0')
		{
			if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			{
				usage(stdout);
				exit(EXIT_OK);
			}
			if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0)
			{
				print_version();
				exit(EXIT_OK);
			}
			if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
			{
				o->log_level = AV_LOG_VERBOSE;
				continue;
			}
			if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0)
			{
				o->log_level = AV_LOG_QUIET;
				continue;
			}
			if (strcmp(argv[i], "--live") == 0)
			{
				o->live = 1;
				continue;
			}
			if (strcmp(argv[i], "--json") == 0)
			{
				o->json = 1;
				continue;
			}
			if (strcmp(argv[i], "--show-url") == 0)
			{
				o->show_url = 1;
				continue;
			}
			if (strcmp(argv[i], "--show-headers") == 0)
			{
				o->show_headers = 1;
				continue;
			}
			if (strcmp(argv[i], "--av-dump") == 0)
			{
				o->av_dump = 1;
				continue;
			}
			if (opt_val(argc, argv, &i, "--headers", &val))
			{
				o->headers = val;
				continue;
			}
			if (opt_val(argc, argv, &i, "--profile", &val))
			{
				/* One profile exists in the core today; policy_build
				 * returns -1 for anything else. Refuse here, where the
				 * message can name the option. */
				if (strcmp(val, "record") != 0)
					usage_error("--profile knows only 'record'");
				continue;
			}
			if (opt_val(argc, argv, &i, "--repeat", &val))
			{
				o->repeat = opt_int("--repeat", val, 1, 100000);
				continue;
			}
			if (opt_val(argc, argv, &i, "--open-time", &val))
			{
				o->open_time_s = opt_int("--open-time", val, 0, 3600);
				continue;
			}
			usage_error("unknown option");
		}
		if (o->url)
			usage_error("more than one url given");
		o->url = argv[i];
	}
	/* Neither the value nor the count is echoed above: a url on the command
	 * line can carry a token, and an error message is output like any
	 * other. An empty url would reach the core as "unset" and FFmpeg as a
	 * NULL filename, so it is refused here rather than crashing there. */
	if (!o->url)
		usage_error("no url given");
	if (!*o->url)
		usage_error("the url is empty");
}

/* --------------------------------------------------------------- one probe */

/* The one place a transport failure is named. The code may be given
 * explicitly: when our own deadline stopped a phase, FFmpeg reports
 * AVERROR_EXIT, which the core would read as a user abort -- but the truth
 * is that we timed it out. */
static void record_failure_code(iter_t *it, stream_stage_t stage, int ret,
	stream_error_code_t code)
{
	it->ok = 0;
	it->stage = stage;
	it->ff = ret;
	it->code = code;
	/* The second argument is the HTTP status, and it stays 0 on purpose:
	 * FFmpeg keeps the response code in a private HTTPContext field with no
	 * AVOption on it, so at this level the status survives only inside
	 * AVERROR_HTTP_*, which the code above already carries. That is also
	 * why the report has no "HTTP:" line although the spec's example shows
	 * one -- and no "Manifest:" line either: a manifest that parsed is what
	 * "Probe ok" says. */
	it->failure = streaminput_classify(code, 0);
	if (av_strerror(ret, it->ff_msg, sizeof(it->ff_msg)) < 0)
		snprintf(it->ff_msg, sizeof(it->ff_msg), "unknown error %d", ret);
}

static void record_failure(iter_t *it, stream_stage_t stage, int ret)
{
	/* AVERROR_EXIT reaches us from exactly one place: our own interrupt
	 * callback. Without a signal it therefore means our deadline fired --
	 * possibly in an earlier phase, because a context whose callback said
	 * stop keeps returning AVERROR_EXIT from every later read without doing
	 * any i/o. Calling that a user abort would blame the user for our own
	 * timer. (One exception exists: the "async:" protocol can raise it from
	 * its own abort on close. A url that opts into that wrapper would be
	 * reported as timed out; not worth a guard, but worth knowing.)
	 *
	 * The reverse case is deliberate: a phase can return successfully even
	 * though the deadline fired -- avformat_find_stream_info() is content
	 * once every stream has parameters, and an interrupted mpegts scan
	 * still has them. A usable result stays a result; that the bound was
	 * exceeded is reported alongside it as deadline_hit rather than turned
	 * into a failure. */
	if (ret == AVERROR_EXIT && !g_signal_requested)
		record_failure_code(it, stage, ret, STREAM_ERR_TIMED_OUT);
	else
		record_failure_code(it, stage, ret, streaminput_error_from_averror(ret));
}

static void record_internal(iter_t *it, const char *what)
{
	/* A local failure -- memory, arguments, a policy that could not be
	 * built. It gets no failure class and no AVERROR: classifying it as a
	 * transport problem would blame the stream for our own trouble. */
	it->ok = 0;
	it->internal = 1;
	warn("%s", what);
}

static void collect_media(media_t *m, AVFormatContext *ctx)
{
	unsigned i;

	m->container = ctx->iformat ? ctx->iformat->name : "";
	m->container_long = (ctx->iformat && ctx->iformat->long_name) ? ctx->iformat->long_name : "";
	m->streams = (int)ctx->nb_streams;
	m->duration_us = ctx->duration;
	m->bit_rate = ctx->bit_rate;

	/* The first stream of each kind in container order. Deliberately not
	 * av_find_best_stream(): that can consult decoders, and this tool is
	 * about the input side only. */
	for (i = 0; i < ctx->nb_streams; i++)
	{
		AVCodecParameters *par = ctx->streams[i]->codecpar;

		if (!m->have_video && par->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			AVRational fr = ctx->streams[i]->avg_frame_rate;

			if (!fr.num || !fr.den)
				fr = ctx->streams[i]->r_frame_rate;
			m->have_video = 1;
			m->vcodec = avcodec_get_name(par->codec_id);
			m->width = par->width;
			m->height = par->height;
			m->fps = (fr.num && fr.den) ? av_q2d(fr) : 0.0;
		}
		else if (!m->have_audio && par->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			m->have_audio = 1;
			m->acodec = avcodec_get_name(par->codec_id);
			m->sample_rate = par->sample_rate;
			/* The channel count moved with the 5.1 layout API and the old
			 * field is gone in 7.0. Same boundary the tree already uses in
			 * src/driver/audiodec/ffmpegdec.cpp, but the old arm reads
			 * par->channels rather than deriving the count from
			 * par->channel_layout: on 4.4 the layout may be 0 (unknown)
			 * while channels carries the real number. Only the count is
			 * printed -- describing the layout would need the same
			 * version split a second time. */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100)
			m->channels = par->channels;
#else
			m->channels = par->ch_layout.nb_channels;
#endif
		}
	}
	m->have = 1;
}

static int str_same(const char *a, const char *b)
{
	return strcmp(a ? a : "", b ? b : "") == 0;
}

/* Compares what identifies the streams, not just the container: at an
 * unchanged stream count a source can still switch codec, resolution, frame
 * rate or channel count between iterations. Duration and bit rate are
 * published but deliberately left out -- both are estimates on a live source
 * and would mark every one of them unstable. */
static int media_same(const media_t *a, const media_t *b)
{
	return str_same(a->container, b->container) &&
		str_same(a->container_long, b->container_long) &&
		a->streams == b->streams &&
		a->have_video == b->have_video &&
		a->have_audio == b->have_audio &&
		str_same(a->vcodec, b->vcodec) &&
		a->width == b->width && a->height == b->height &&
		a->fps == b->fps &&
		str_same(a->acodec, b->acodec) &&
		a->sample_rate == b->sample_rate && a->channels == b->channels;
}

static void read_window(iter_t *it, AVFormatContext *ctx, AVPacket *pkt, const opt_t *o)
{
	int64_t start = av_gettime_relative();
	int ret = 0;
	int expired;

	deadline_set((int64_t)o->open_time_s * 1000000);
	for (;;)
	{
		ret = av_read_frame(ctx, pkt);
		if (ret < 0)
			break;
		it->packets++;
		it->bytes += pkt->size;
		av_packet_unref(pkt);
		if (g_signal_requested || g_deadline_expired)
			break;
	}
	it->read_us = av_gettime_relative() - start;
	expired = deadline_clear();
	it->deadline_hit |= expired;

	if (g_signal_requested)
	{
		record_failure(it, STREAM_STAGE_PLAYBACK, AVERROR_EXIT);
		return;
	}
	if (expired)
	{
		/* Our own deadline ended the window, so the window ended -- before
		 * asking what the demuxer made of the interruption. It does not
		 * always hand the callback's AVERROR_EXIT back: the HLS demuxer
		 * reads an interrupted segment as a finished one and reports EOF,
		 * which under --live would be filed as a stream that ended. */
		it->stage = STREAM_STAGE_PLAYBACK;
		return;
	}
	if (ret == AVERROR_EOF)
	{
		/* A live source that ends has ended for a reason; a finite file is
		 * simply over. Same distinction the core draws with live sources. */
		if (o->live)
		{
			record_failure(it, STREAM_STAGE_PLAYBACK, AVERROR_EOF);
			return;
		}
		it->stage = STREAM_STAGE_PLAYBACK;
		return;
	}
	if (ret < 0 && !expired)
	{
		record_failure(it, STREAM_STAGE_PLAYBACK, ret);
		return;
	}
	/* Window elapsed with packets still flowing: that is the success case. */
	it->stage = STREAM_STAGE_PLAYBACK;
}

/* Runs one complete open. Everything allocated here is released at out:,
 * on every path, so the repeat loop cannot leak. */
static void probe_once(const stream_source_t *src, const opt_t *o, const char *display_url,
	AVPacket *pkt, iter_t *it, media_t *media, stream_kv_list_t *opts_out,
	AVDictionary **unconsumed, int *opened)
{
	stream_kv_list_t opts;
	AVDictionary *dict = NULL;
	AVFormatContext *ctx = NULL;
	int64_t phase_deadline = (int64_t)o->open_time_s * 1000000;
	int64_t t0;
	int ret;

	memset(it, 0, sizeof(*it));
	it->ok = 1;
	it->stage = STREAM_STAGE_OPEN;
	it->probe_us = -1;

	streaminput_kv_init(&opts);
	if (streaminput_policy_build(src, STREAM_PROFILE_RECORD, &opts) < 0)
	{
		record_internal(it, "could not build the input policy");
		goto out;
	}
	/* Unlike the playback paths, an incomplete policy is fatal here: this
	 * tool exists to report what happens under the policy, and an open
	 * under half of it would answer a question nobody asked. */
	if (streaminput_kv_to_avdict(&opts, &dict) < 0)
	{
		record_internal(it, "could not hand the input policy to FFmpeg");
		goto out;
	}
	if (opts_out && !opts_out->count)
	{
		size_t i;

		for (i = 0; i < opts.count; i++)
			if (streaminput_kv_set(opts_out, opts.items[i].key, opts.items[i].value) < 0)
			{
				record_internal(it, "could not keep the input policy for the report");
				goto out;
			}
	}

	ctx = avformat_alloc_context();
	if (!ctx)
	{
		record_internal(it, "could not allocate an FFmpeg context");
		goto out;
	}
	/* Armed before the open, so Ctrl-C works while connecting. */
	ctx->interrupt_callback.callback = probe_interrupt_cb;
	ctx->interrupt_callback.opaque = NULL;

	/* --open-time bounds every blocking phase, not only the read window: a
	 * hanging connect would otherwise sit out the policy's 20 second
	 * timeout while the user asked for one. A deadline that fires here is
	 * our timeout, so it is named one -- FFmpeg's AVERROR_EXIT would
	 * otherwise be read as a user abort. */
	deadline_set(phase_deadline);
	t0 = av_gettime_relative();
	ret = avformat_open_input(&ctx, src->url, NULL, &dict);
	it->open_us = av_gettime_relative() - t0;
	it->deadline_hit |= deadline_clear();
	if (ret < 0)
	{
		record_failure(it, STREAM_STAGE_OPEN, ret);
		goto out;
	}
	it->open_ok = 1;

	/* Only now does the dictionary hold what FFmpeg did not consume; on a
	 * failed open it discards its copy and ours is untouched, so claiming
	 * "unconsumed" there would be an invention. The flag, not the pointer,
	 * says whether an open ever succeeded: an empty leftover set is a
	 * result ("FFmpeg took everything"), not the absence of one. */
	if (opened && !*opened)
	{
		if (unconsumed && av_dict_copy(unconsumed, dict, 0) < 0)
		{
			record_internal(it, "could not keep FFmpeg's leftover options");
			goto out;
		}
		*opened = 1;
	}

	deadline_set(phase_deadline);
	t0 = av_gettime_relative();
	ret = avformat_find_stream_info(ctx, NULL);
	it->probe_us = av_gettime_relative() - t0;
	it->deadline_hit |= deadline_clear();
	if (ret < 0)
	{
		record_failure(it, STREAM_STAGE_PROBE, ret);
		goto out;
	}
	it->stage = STREAM_STAGE_PROBE;

	if (media)
	{
		media_t cur;

		memset(&cur, 0, sizeof(cur));
		collect_media(&cur, ctx);
		if (!media->have)
		{
			cur.stable = 1;
			*media = cur;
		}
		else if (!media_same(media, &cur))
		{
			/* Every field the report publishes takes part: a source that
			 * comes back as a different codec or resolution at the same
			 * stream count is exactly the flakiness --repeat looks for. */
			media->stable = 0;
		}
	}

	if (o->av_dump)
	{
		/* av_dump_format() writes through av_log at INFO, and FFmpeg is
		 * quiet by default here, so the level is raised for this one call
		 * and put back. It prints the string it is given, so it gets the
		 * redacted one. Its layout is not stable across FFmpeg versions,
		 * which is why this is opt-in and goes to stderr. */
		int prev = av_log_get_level();

		if (prev < AV_LOG_INFO)
			av_log_set_level(AV_LOG_INFO);
		av_dump_format(ctx, 0, display_url, 0);
		av_log_set_level(prev);
	}

	if (o->open_time_s > 0)
		read_window(it, ctx, pkt, o);

out:
	av_dict_free(&dict);
	avformat_close_input(&ctx); /* NULL-safe, including "allocated, never opened" */
	streaminput_kv_free(&opts);
	deadline_clear();
	/* A signal that arrived after the last check inside the phases must not
	 * leave a successful-looking iteration behind: the run did not finish. */
	if (g_signal_requested && it->ok && !it->internal)
		record_failure(it, it->stage, AVERROR_EXIT);
}

/* -------------------------------------------------------------- reporting */

typedef struct
{
	int attempts;
	int ok;
	int failed;
	int internal;
	int open_n;
	int64_t open_min;
	int64_t open_max;
	int64_t open_sum;
	int64_t probe_min;
	int64_t probe_max;
	int64_t probe_sum;
	int probe_n;
	long packets;
	int64_t bytes;
	/* Histogram indexed by the class enum. Local to this run, so the
	 * coupling to the enum's numbering costs nothing; the exit codes, which
	 * are a published contract, use the explicit table instead. */
	int cls[STREAM_FAILURE_UNKNOWN + 1];
	int internal_n;
	int have_failure;
	stream_failure_class_t first_failure;
} summary_t;

static void summary_add(summary_t *s, const iter_t *it)
{
	s->attempts++;
	if (it->internal)
	{
		s->internal = 1;
		s->internal_n++;
	}
	if (it->ok)
		s->ok++;
	else
	{
		s->failed++;
		if (!it->internal)
		{
			if ((size_t)it->failure < sizeof(s->cls) / sizeof(s->cls[0]))
				s->cls[it->failure]++;
			if (!s->have_failure)
			{
				s->have_failure = 1;
				s->first_failure = it->failure;
			}
		}
	}
	/* Only opens that succeeded: mixing a slow refusal into the same
	 * min/max/avg would describe neither the failures nor the successes. */
	if (it->open_ok)
	{
		if (!s->open_n || it->open_us < s->open_min)
			s->open_min = it->open_us;
		if (it->open_us > s->open_max)
			s->open_max = it->open_us;
		s->open_sum += it->open_us;
		s->open_n++;
	}
	if (it->probe_us >= 0)
	{
		if (!s->probe_n || it->probe_us < s->probe_min)
			s->probe_min = it->probe_us;
		if (it->probe_us > s->probe_max)
			s->probe_max = it->probe_us;
		s->probe_sum += it->probe_us;
		s->probe_n++;
	}
	s->packets += it->packets;
	s->bytes += it->bytes;
}

static int summary_exit_code(const summary_t *s)
{
	if (s->internal)
		return EXIT_INTERNAL;
	if (!s->have_failure)
		return EXIT_OK;
	return exit_code_for(s->first_failure);
}

static long ms(int64_t usec)
{
	return (long)(usec / 1000);
}

static const char *policy_value(const char *key, const char *value, const opt_t *o)
{
	/* The header blob is the one policy value that routinely carries
	 * cookies and bearer tokens. */
	if (!o->show_headers && strcmp(key, "headers") == 0)
		return "<hidden>";
	return value;
}

static void print_kv_list(const stream_kv_list_t *opts, const opt_t *o)
{
	size_t i;

	for (i = 0; i < opts->count; i++)
		printf("%s%s=%s", i ? " " : "", opts->items[i].key,
			policy_value(opts->items[i].key, opts->items[i].value, o));
}

static void report_human(const opt_t *o, const stream_source_t *src, const char *display_url,
	const stream_kv_list_t *policy, AVDictionary *unconsumed, int opened, const iter_t *iters,
	const media_t *media, const summary_t *s)
{
	int i;

	printf("URL          %s\n", display_url);
	printf("Protocol     %s\n", streaminput_protocol_name(src->protocol));
	printf("Live         %d\n", src->live);
	printf("Profile      record\n");
	printf("Policy       ");
	print_kv_list(policy, o);
	printf("\n");
	if (opened)
	{
		const AVDictionaryEntry *e = NULL;
		int any = 0;

		printf("Not consumed ");
		while ((e = av_dict_get(unconsumed, "", e, AV_DICT_IGNORE_SUFFIX)))
		{
			printf("%s%s=%s", any ? " " : "", e->key, policy_value(e->key, e->value, o));
			any = 1;
		}
		printf("%s\n", any ? "" : "(none)");
	}
	if (o->headers && *o->headers)
		printf("Headers      %d bytes%s\n", (int)strlen(o->headers),
			o->show_headers ? "" : " (hidden)");

	if (o->repeat == 1)
	{
		const iter_t *it = &iters[0];

		if (it->internal)
		{
			printf("Result       internal error (exit %d)\n", EXIT_INTERNAL);
			return;
		}
		if (it->stage == STREAM_STAGE_OPEN && !it->ok)
			printf("Open         FAILED after %ld ms\n", ms(it->open_us));
		else
			printf("Open         ok %ld ms\n", ms(it->open_us));
		if (it->probe_us >= 0)
			printf("Probe        %s %ld ms\n", it->ok || it->stage != STREAM_STAGE_PROBE ? "ok" : "FAILED",
				ms(it->probe_us));
		if (media->have)
		{
			printf("Container    %s (%s)%s\n", media->container, media->container_long,
				media->stable ? "" : " [UNSTABLE across iterations]");
			printf("Streams      %d\n", media->streams);
			if (media->have_video)
				printf("Video        %s %dx%d %.2f fps\n", media->vcodec, media->width,
					media->height, media->fps);
			if (media->have_audio)
				printf("Audio        %s %d Hz %d ch\n", media->acodec, media->sample_rate,
					media->channels);
		}
		if (it->packets)
			printf("Packets      %ld (%lld bytes) in %ld ms\n", it->packets,
				(long long)it->bytes, ms(it->read_us));
		if (it->deadline_hit)
			printf("Deadline     --open-time expired\n");
		if (!it->ok)
		{
			printf("Stage        %s\n", streaminput_stage_name(it->stage));
			printf("Error        %s\n", streaminput_error_code_name(it->code));
			printf("Failure      %s\n", streaminput_failure_class_name(it->failure));
			printf("FFmpeg       %d (%s)\n", it->ff, it->ff_msg);
			printf("Result       failed (exit %d)\n", summary_exit_code(s));
		}
		else
			printf("Result       ok\n");
		return;
	}

	printf("\nIter  Result  Open     Probe    Stage      Failure\n");
	for (i = 0; i < s->attempts; i++)
	{
		const iter_t *it = &iters[i];

		printf("%4d  %-6s %6ld ms  ", i + 1,
			it->internal ? "INTERN" : (it->ok ? "ok" : "FAIL"), ms(it->open_us));
		if (it->probe_us >= 0)
			printf("%6ld ms  ", ms(it->probe_us));
		else
			printf("       -  ");
		printf("%-10s %s%s\n", streaminput_stage_name(it->stage),
			it->internal ? "internal error" :
			streaminput_failure_class_name(it->ok ? STREAM_FAILURE_NONE : it->failure),
			it->deadline_hit ? " (deadline)" : "");
	}
	if (media->have)
	{
		printf("\nContainer    %s (%s)%s\n", media->container, media->container_long,
			media->stable ? "" : " [UNSTABLE across iterations]");
		if (media->have_video)
			printf("Video        %s %dx%d %.2f fps\n", media->vcodec, media->width,
				media->height, media->fps);
		if (media->have_audio)
			printf("Audio        %s %d Hz %d ch\n", media->acodec, media->sample_rate,
				media->channels);
	}
	printf("\nAttempts     %d\n", s->attempts);
	printf("Succeeded    %d\n", s->ok);
	printf("Failed       %d\n", s->failed);
	if (s->open_n)
		printf("Open ms      min %ld max %ld avg %ld\n", ms(s->open_min), ms(s->open_max),
			ms(s->open_sum / s->open_n));
	if (s->probe_n)
		printf("Probe ms     min %ld max %ld avg %ld\n", ms(s->probe_min), ms(s->probe_max),
			ms(s->probe_sum / s->probe_n));
	if (s->packets)
		printf("Packets      %ld (%lld bytes)\n", s->packets, (long long)s->bytes);
	if (s->internal_n)
		printf("Internal     %d (no failure class: the trouble was local)\n", s->internal_n);
	if (s->failed > s->internal_n)
	{
		size_t c;
		int any = 0;

		printf("Failures     ");
		for (c = 0; c < sizeof(s->cls) / sizeof(s->cls[0]); c++)
			if (s->cls[c])
			{
				printf("%s%s=%d", any ? " " : "",
					streaminput_failure_class_name((stream_failure_class_t)c), s->cls[c]);
				any = 1;
			}
		printf("\n");
		printf("Result       failed (exit %d)\n", summary_exit_code(s));
	}
	else
		printf("Result       ok\n");
}

/* ------------------------------------------------------------------- json */

static void json_str(const char *str)
{
	/* Header blobs carry CRLF, container long names come from FFmpeg: both
	 * have to survive as valid JSON. Walked as unsigned char so the escape
	 * decision below is not made on a sign-extended byte. */
	const unsigned char *s = (const unsigned char *)str;

	putchar('"');
	while (s && *s)
	{
		unsigned char c = *s;
		int len;

		if (c == '"' || c == '\\')
		{
			printf("\\%c", c);
			s++;
			continue;
		}
		if (c == '\n' || c == '\r' || c == '\t')
		{
			fputs(c == '\n' ? "\\n" : (c == '\r' ? "\\r" : "\\t"), stdout);
			s++;
			continue;
		}
		if (c < 0x20)
		{
			printf("\\u%04x", c);
			s++;
			continue;
		}
		if (c < 0x80)
		{
			putchar(c);
			s++;
			continue;
		}
		/* A header blob or a url is a byte string, and JSON is text: a byte
		 * that is not part of a valid UTF-8 sequence would make the whole
		 * document unparseable. Valid sequences pass through untouched,
		 * anything else becomes one replacement character. The lead-byte
		 * ranges and the first continuation byte are both constrained,
		 * which is what rejects overlong forms (0xc0/0xc1, 0xe0 0x80),
		 * the surrogate block (0xed 0xa0) and everything above U+10FFFF
		 * (0xf5 and up) -- a plain "two high bits set" test would let all
		 * three through and json.load() would refuse the document. */
		len = 0;
		if (c >= 0xc2 && c <= 0xdf)
			len = 2;
		else if (c >= 0xe0 && c <= 0xef)
			len = 3;
		else if (c >= 0xf0 && c <= 0xf4)
			len = 4;
		if (len)
		{
			unsigned char lo = 0x80;
			unsigned char hi = 0xbf;
			int i;

			if (c == 0xe0)
				lo = 0xa0;         /* no overlong three-byte forms */
			else if (c == 0xed)
				hi = 0x9f;         /* no UTF-16 surrogates */
			else if (c == 0xf0)
				lo = 0x90;         /* no overlong four-byte forms */
			else if (c == 0xf4)
				hi = 0x8f;         /* nothing above U+10FFFF */
			if (s[1] < lo || s[1] > hi)
				len = 0;
			for (i = 2; len && i < len; i++)
				if ((s[i] & 0xc0) != 0x80)
					len = 0;
		}
		if (!len)
		{
			fputs("\\ufffd", stdout);
			s++;
			continue;
		}
		fwrite(s, 1, (size_t)len, stdout);
		s += len;
	}
	putchar('"');
}

static void json_kv_list(const stream_kv_list_t *opts, const opt_t *o)
{
	size_t i;

	fputs("{", stdout);
	for (i = 0; i < opts->count; i++)
	{
		if (i)
			fputs(", ", stdout);
		json_str(opts->items[i].key);
		fputs(": ", stdout);
		json_str(policy_value(opts->items[i].key, opts->items[i].value, o));
	}
	fputs("}", stdout);
}

static void json_dict(AVDictionary *d, const opt_t *o)
{
	const AVDictionaryEntry *e = NULL;
	int any = 0;

	fputs("{", stdout);
	while ((e = av_dict_get(d, "", e, AV_DICT_IGNORE_SUFFIX)))
	{
		if (any)
			fputs(", ", stdout);
		json_str(e->key);
		fputs(": ", stdout);
		json_str(policy_value(e->key, e->value, o));
		any = 1;
	}
	fputs("}", stdout);
}

static void json_lib(const char *name, unsigned rt, unsigned bt)
{
	printf("    \"%s\": {\"runtime\": \"%u.%u.%u\", \"build\": \"%u.%u.%u\"}", name,
		AV_VERSION_MAJOR(rt), AV_VERSION_MINOR(rt), AV_VERSION_MICRO(rt),
		AV_VERSION_MAJOR(bt), AV_VERSION_MINOR(bt), AV_VERSION_MICRO(bt));
}

static void report_json(const opt_t *o, const stream_source_t *src, const char *display_url,
	const stream_kv_list_t *policy, AVDictionary *unconsumed, int opened, const iter_t *iters,
	const media_t *media, const summary_t *s)
{
	int i;
	size_t c;
	int any;

	fputs("{\n", stdout);
	fputs("  \"tool\": \"streamprobe\",\n", stdout);
	fputs("  \"tool_version\": ", stdout);
	json_str(STREAMPROBE_VERSION);
	fputs(",\n  \"neutrino_version\": ", stdout);
	json_str(PACKAGE_VERSION);
	fputs(",\n", stdout);
	fputs("  \"libav\": {\n", stdout);
	json_lib("avformat", avformat_version(), LIBAVFORMAT_VERSION_INT);
	fputs(",\n", stdout);
	json_lib("avcodec", avcodec_version(), LIBAVCODEC_VERSION_INT);
	fputs(",\n", stdout);
	json_lib("avutil", avutil_version(), LIBAVUTIL_VERSION_INT);
	fputs(",\n", stdout);
	printf("    \"version_mismatch\": %s\n",
		(avformat_version() != LIBAVFORMAT_VERSION_INT ||
			avcodec_version() != LIBAVCODEC_VERSION_INT ||
			avutil_version() != LIBAVUTIL_VERSION_INT) ? "true" : "false");
	fputs("  },\n", stdout);

	fputs("  \"url\": ", stdout);
	json_str(display_url);
	fputs(",\n", stdout);
	/* Whether this url was actually shortened, not merely whether the
	 * switch was absent: a url without a query looks the same either way. */
	printf("  \"url_redacted\": %s,\n", strcmp(display_url, o->url) != 0 ? "true" : "false");
	printf("  \"protocol\": \"%s\",\n", streaminput_protocol_name(src->protocol));
	printf("  \"live\": %d,\n", src->live);
	fputs("  \"profile\": \"record\",\n", stdout);
	fputs("  \"policy\": ", stdout);
	json_kv_list(policy, o);
	fputs(",\n", stdout);
	fputs("  \"policy_unconsumed\": ", stdout);
	if (opened)
		json_dict(unconsumed, o); /* may legitimately be {} */
	else
		fputs("null", stdout); /* no successful open: FFmpeg said nothing */
	fputs(",\n", stdout);
	printf("  \"headers\": {\"present\": %s, \"bytes\": %d},\n",
		(o->headers && *o->headers) ? "true" : "false",
		o->headers ? (int)strlen(o->headers) : 0);
	printf("  \"repeat\": %d,\n", o->repeat);
	printf("  \"open_time_s\": %d,\n", o->open_time_s);

	fputs("  \"iterations\": [\n", stdout);
	for (i = 0; i < s->attempts; i++)
	{
		const iter_t *it = &iters[i];

		printf("    {\"index\": %d, \"result\": \"%s\", \"stage\": \"%s\", ", i,
			it->internal ? "internal" : (it->ok ? "ok" : "failed"),
			streaminput_stage_name(it->stage));
		printf("\"error_code\": \"%s\", \"failure_class\": \"%s\", ",
			streaminput_error_code_name(it->ok ? STREAM_ERR_NONE : it->code),
			streaminput_failure_class_name(it->ok ? STREAM_FAILURE_NONE : it->failure));
		printf("\"ffmpeg_error\": %d, \"ffmpeg_error_string\": ", it->ff);
		json_str(it->ff_msg);
		printf(", \"open_ms\": %ld, \"probe_ms\": %ld, \"read_ms\": %ld, ",
			ms(it->open_us), it->probe_us >= 0 ? ms(it->probe_us) : -1, ms(it->read_us));
		printf("\"packets\": %ld, \"bytes\": %lld, \"deadline_hit\": %s}%s\n",
			it->packets, (long long)it->bytes, it->deadline_hit ? "true" : "false",
			i + 1 < s->attempts ? "," : "");
	}
	fputs("  ],\n", stdout);

	fputs("  \"summary\": {\n", stdout);
	/* internal is reported next to the histogram: those failures carry no
	 * class, so without it a consumer summing the classes would come up
	 * short of "failed" and have no way to see why. */
	printf("    \"attempts\": %d, \"ok\": %d, \"failed\": %d, \"internal\": %d,\n",
		s->attempts, s->ok, s->failed, s->internal_n);
	if (s->open_n)
		printf("    \"open_ms\": {\"min\": %ld, \"max\": %ld, \"avg\": %ld},\n",
			ms(s->open_min), ms(s->open_max), ms(s->open_sum / s->open_n));
	else
		fputs("    \"open_ms\": null,\n", stdout);
	if (s->probe_n)
		printf("    \"probe_ms\": {\"min\": %ld, \"max\": %ld, \"avg\": %ld},\n",
			ms(s->probe_min), ms(s->probe_max), ms(s->probe_sum / s->probe_n));
	else
		fputs("    \"probe_ms\": null,\n", stdout);
	printf("    \"packets\": %ld, \"bytes\": %lld,\n", s->packets, (long long)s->bytes);
	fputs("    \"failure_classes\": {", stdout);
	any = 0;
	for (c = 0; c < sizeof(s->cls) / sizeof(s->cls[0]); c++)
		if (s->cls[c])
		{
			printf("%s\"%s\": %d", any ? ", " : "",
				streaminput_failure_class_name((stream_failure_class_t)c), s->cls[c]);
			any = 1;
		}
	fputs("}\n", stdout);
	fputs("  },\n", stdout);

	if (media->have)
	{
		fputs("  \"media\": {\n", stdout);
		fputs("    \"container\": ", stdout);
		json_str(media->container);
		fputs(", \"container_long\": ", stdout);
		json_str(media->container_long);
		printf(",\n    \"streams\": %d, \"duration_us\": ", media->streams);
		if (media->duration_us == AV_NOPTS_VALUE)
			fputs("null", stdout); /* unknown is not a very negative number */
		else
			printf("%lld", (long long)media->duration_us);
		printf(", \"bit_rate\": %lld, \"stable\": %s,\n",
			(long long)media->bit_rate, media->stable ? "true" : "false");
		if (media->have_video)
			printf("    \"video\": {\"codec\": \"%s\", \"width\": %d, \"height\": %d, \"fps\": %.2f},\n",
				media->vcodec, media->width, media->height, media->fps);
		else
			fputs("    \"video\": null,\n", stdout);
		if (media->have_audio)
			printf("    \"audio\": {\"codec\": \"%s\", \"sample_rate\": %d, \"channels\": %d}\n",
				media->acodec, media->sample_rate, media->channels);
		else
			fputs("    \"audio\": null\n", stdout);
		fputs("  },\n", stdout);
	}
	else
		fputs("  \"media\": null,\n", stdout);

	printf("  \"exit_code\": %d\n", summary_exit_code(s));
	fputs("}\n", stdout);
}

/* ------------------------------------------------------------------- main */

int main(int argc, char **argv)
{
	opt_t o;
	stream_source_t src;
	stream_kv_list_t policy;
	AVDictionary *unconsumed = NULL;
	int opened = 0;
	AVPacket *pkt = NULL;
	iter_t *iters = NULL;
	media_t media;
	summary_t s;
	char *display_url = NULL;
	int rc = EXIT_INTERNAL;
	int i;

	parse_args(argc, argv, &o);

	/* Redacting our own output is not enough: libavformat prints full URLs
	 * and, at debug level, whole requests including the headers we set. So
	 * FFmpeg stays quiet unless the caller asks for its log and accepts what
	 * that means -- the help text says so. */
	av_log_set_level(o.log_level);

	streaminput_source_init(&src);
	streaminput_kv_init(&policy);
	memset(&media, 0, sizeof(media));
	memset(&s, 0, sizeof(s));

	if (streaminput_source_set_url(&src, o.url) < 0 ||
		streaminput_source_set_headers(&src, o.headers) < 0)
	{
		warn("out of memory while describing the source");
		goto out;
	}
	src.live = o.live;

	display_url = redact_dup(o.url, o.show_url);
	if (!display_url)
	{
		warn("out of memory");
		goto out;
	}

	iters = calloc((size_t)o.repeat, sizeof(*iters));
	pkt = av_packet_alloc();
	if (!iters || !pkt)
	{
		warn("out of memory");
		goto out;
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	if (avformat_network_init() < 0)
	{
		warn("FFmpeg network initialisation failed");
		goto out;
	}

	for (i = 0; i < o.repeat; i++)
	{
		probe_once(&src, &o, display_url, pkt, &iters[i], &media, &policy, &unconsumed, &opened);
		summary_add(&s, &iters[i]);
		/* Repeating an internal failure only repeats the warning: out of
		 * memory does not get better on the second try. */
		if (g_signal_requested || iters[i].internal)
			break;
	}

	if (o.json)
		report_json(&o, &src, display_url, &policy, unconsumed, opened, iters, &media, &s);
	else
		report_human(&o, &src, display_url, &policy, unconsumed, opened, iters, &media, &s);
	rc = summary_exit_code(&s);

	avformat_network_deinit();
out:
	av_packet_free(&pkt);
	av_dict_free(&unconsumed);
	free(iters);
	free(display_url);
	streaminput_kv_free(&policy);
	streaminput_source_free(&src);
	return rc;
}
