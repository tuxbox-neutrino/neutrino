/*
 * some definition
 */
#define TS_LEN			188
#define TS_SYNC_BYTE		0x47
#define TS_BUF_SIZE		(TS_LEN * 2048)		/* fix dmx buffer size */

static unsigned long timeval_to_ms(const struct timeval *tv)
{
        return (tv->tv_sec * 1000) + ((tv->tv_usec + 500) / 1000);
}

long delta_time_ms (struct timeval *tv, struct timeval *last_tv)
{
        return timeval_to_ms(tv) - timeval_to_ms(last_tv);
}

int ts_pidbandwidth (OPTION *opt)
{

  u_char 	 buf[TS_BUF_SIZE];
  struct pollfd  pfd;
  struct dmx_pes_filter_params flt;
  int 		 dmxfd;
  struct timeval tv,last_tv, first_tv;
  int		 pid;
  uint64_t      b_total;
  long           b;
  long           packets_bad;
  long           packets_total;
  struct {				// simple struct for storing last average bandwidth
	  unsigned long  kb_sec;
	  unsigned long  b_sec;
  } last_avg;




   if (opt->ts_raw_mode) {
	   pid = PID_FULL_TS;
   } else {
	   pid = opt->pid;
   }



   indent (0);
   out_nl (2,"");
   out_nl (2,"---------------------------------------------------------");
   out_nl (2,"PID bandwidth statistics...");
    if (opt->ts_raw_mode) {
	out (2,"Full Transponder");
    } else {
	out (2,"PID: %u (0x%04x)", pid, pid);
    }
    if (opt->rd_packet_count != 0) {
	out (2,"   - max packet count: %ld ", opt->rd_packet_count);
    }
    out_nl (2,"");
   out_nl (2,"---------------------------------------------------------");



	// -- open DVR device for reading
   	pfd.events = POLLIN | POLLPRI;
	if((pfd.fd = open(opt->devDvr,O_RDONLY|O_NONBLOCK)) < 0){
		IO_error(opt->devDvr);
		return -1;
   	}



	if ((dmxfd=open(opt->devDemux,O_RDWR)) < 0) {
		IO_error(opt->devDemux);
		close(pfd.fd);
		return -1;
	}
	ioctl (dmxfd,DMX_SET_BUFFER_SIZE, sizeof(buf));

		flt.pid = pid;
		flt.input = DMX_IN_FRONTEND;
		flt.output = DMX_OUT_TS_TAP;
		flt.pes_type = DMX_PES_OTHER;
		flt.flags = DMX_IMMEDIATE_START;
		if (ioctl(dmxfd, DMX_SET_PES_FILTER, &flt) < 0) {
			IO_error("DMX_SET_PES_FILTER");
			close(pfd.fd);
			close(dmxfd);
			return -1;
		}



	gettimeofday (&first_tv, NULL);
	last_tv.tv_sec  =  first_tv.tv_sec;
	last_tv.tv_usec =  first_tv.tv_usec;

	b_total = 0;
	packets_total = 0;
  	packets_bad = 0;

	while ( !isSigAbort() ) {
		int b_len, b_start;

		// -- we will poll the PID in 2 secs intervall
		int timeout = 2000;

		b_len = 0;
		b_start = 0;
		if (poll(&pfd, 1, timeout) > 0) {
			if (pfd.revents & POLLIN) {

				b_len = read(pfd.fd, buf, sizeof(buf));
				gettimeofday (&tv, NULL);


				if (b_len >= TS_LEN) {
					b_start = sync_ts (buf, b_len);
				} else {
					b_len = 0;
				}

				b = b_len - b_start;
				if (b == 0) continue;
				if (b < 0) {
				   IO_error("read");
				   continue;
				}

				b_total += b;



				// -- calc bandwidth

				{
				   uint64_t  bit_s;
				   long  d_tim_ms;
				   int   packets;

				   packets = b/TS_LEN;
				   packets_total += packets;


				   // output on different verbosity levels
				   // -- current bandwidth
				   d_tim_ms = delta_time_ms (&tv, &last_tv);
				   if (d_tim_ms <= 0) d_tim_ms = 1;   //  ignore usecs

				   out (3, "packets read: %3d/(%ld)   d_time: %2ld.%03ld s  = ",
					packets, packets_total, d_tim_ms / 1000UL, d_tim_ms % 1000UL);

				   // -- current bandwidth in kbit/sec
				   // --- cast to uint64_t so it doesn't overflow as
				   // --- early, add time / 2 before division for correct rounding
				   bit_s = (((uint64_t)b * 8000ULL) + ((uint64_t)d_tim_ms / 2ULL))
					   / (uint64_t)d_tim_ms;

				   out (1, "%5llu.%03llu kbit/s", bit_s / 1000ULL, bit_s % 1000ULL);


				   // -- average bandwidth
				   d_tim_ms = delta_time_ms (&tv,&first_tv);
				   if (d_tim_ms <= 0) d_tim_ms = 1;   //  ignore usecs

				   bit_s = ((b_total * 8000ULL) + ((uint64_t)d_tim_ms / 2ULL))
					   / (uint64_t)d_tim_ms;

				   last_avg.kb_sec = (unsigned long) (bit_s / 1000ULL);
				   last_avg.b_sec  = (unsigned long) (bit_s % 1000ULL);

				   out (2, "   (Avrg: %5lu.%03lu kbit/s)", last_avg.kb_sec, last_avg.b_sec);


				   // -- bad packet(s) check in buffer
				   {
				     int bp;

				     bp = ts_error_count (buf+b_start, b);
				     packets_bad += bp;
				     out (4, " [bad: %d]", bp);
				   }

				   out_NL (1);

				}


				last_tv.tv_sec  =  tv.tv_sec;
				last_tv.tv_usec =  tv.tv_usec;



				// count packets ?
				if (opt->rd_packet_count > 0) {
					opt->rd_packet_count -= b/TS_LEN;
					if (opt->rd_packet_count <= 0) break;
				}


			}
		}

	}


	// -- packets stats

	out (4, "## ");

	if (opt->ts_raw_mode) { out (2,"PID: <ALL>");
	} else {                out (2,"PID: %u (0x%04x)", pid, pid);
        }

	out (4, "   bad/total packets: %ld/%ld (= %1.1Lf%%)",
		packets_bad, packets_total,
                (((long double) packets_bad)*100)/packets_total );
        out (4, "   Avrg: %5lu.%03lu kbit/s",
		last_avg.kb_sec, last_avg.b_sec);
	out_NL(4);




	if (ioctl(dmxfd, DMX_STOP) < 0) {
		IO_error("DMX_STOP");
	}
	close(dmxfd);
	close(pfd.fd);


  return 0;

}

