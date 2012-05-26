/*****************************************************************************
 * Copyright (C) 2011-2012 Michael Krufky
 *
 * Author: Michael Krufky <mkrufky@linuxtv.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "feed.h"
#include "tune.h"

#include "atsctext.h"

typedef std::map<uint8_t, tune> map_tuners;

struct dvbtee_context
{
	feed _file_feeder;
	tune tuner;
};
typedef std::map<pid_t, struct dvbtee_context*> map_pid_to_context;

map_pid_to_context context_map;


void cleanup(struct dvbtee_context* context, bool quick = false)
{
	if (quick) {
		context->_file_feeder.stop_without_wait();

		context->tuner.feeder.stop_without_wait();
		context->tuner.feeder.close_file();
		context->tuner.close_demux();
	} else {
		context->_file_feeder.stop();

		context->tuner.stop_feed();
	}

	context->_file_feeder.close_file();

	context->tuner.close_fe();
#if 1 /* FIXME */
	ATSCMultipleStringsDeInit();
#endif
}


void signal_callback_handler(int signum)
{
	struct dvbtee_context* context = context_map[getpid()];

	const char *signal_desc;
	switch (signum) {
	case SIGINT:  /* Program interrupt. (ctrl-c) */
		signal_desc = "SIGINT";
		break;
	case SIGABRT: /* Process detects error and reports by calling abort */
		signal_desc = "SIGABRT";
		break;
	case SIGFPE:  /* Floating-Point arithmetic Exception */
		signal_desc = "SIGFPE";
		break;
	case SIGILL:  /* Illegal Instruction */
		signal_desc = "SIGILL";
		break;
	case SIGSEGV: /* Segmentation Violation */
		signal_desc = "SIGSEGV";
		break;
	case SIGTERM: /* Termination */
		signal_desc = "SIGTERM";
		break;
	case SIGHUP:  /* Hangup */
		signal_desc = "SIGHUP";
		break;
	default:
		signal_desc = "UNKNOWN";
		break;
	}
	fprintf(stderr, "%s: caught signal %d: %s\n", __func__, signum, signal_desc);

	cleanup(context, true);

	context->_file_feeder.parser.cleanup();
	context->tuner.feeder.parser.cleanup();

	exit(signum);
}


void multiscan(struct dvbtee_context* context, int num_tuners, unsigned int scan_method,
	       unsigned int scan_flags, unsigned int scan_min, unsigned int scan_max, bool scan_epg)
{
	int count = 0;
	int partial_redundancy = 0;
#if 0
	map_tuners tuners;
#else
	tune tuners[num_tuners];
#endif
	int channels_to_scan = scan_max - scan_min + 1;

	for (int i = 0; i < num_tuners; i++) {
		int dvb_adap = i; /* ID X, /dev/dvb/adapterX/ */
		int demux_id = 0; /* ID Y, /dev/dvb/adapterX/demuxY */
		int dvr_id   = 0; /* ID Y, /dev/dvb/adapterX/dvrY */
		int fe_id    = 0; /* ID Y, /dev/dvb/adapterX/frontendY */

		tuners[i].set_device_ids(dvb_adap, fe_id, demux_id, dvr_id);
	}
	switch (scan_method) {
	case 1: /* speed */
		for (int i = 0; i < num_tuners; i++) {
			int scan_start = scan_min + ((0 + i) * (unsigned int)channels_to_scan/num_tuners);
			int scan_end   = scan_min + ((1 + i) * (unsigned int)channels_to_scan/num_tuners);
			fprintf(stderr, "speed scan: tuner %d scanning from %d to %d\n", i, scan_start, scan_end);
			tuners[i].start_scan(scan_flags, scan_start, scan_end, scan_epg);
			sleep(1); // FIXME
		}
		break;
	case 2: /* redundancy */
		for (int i = 0; i < num_tuners; i++) {
			fprintf(stderr, "redundancy scan: tuner %d scanning from %d to %d\n", i, scan_min, scan_max);
			tuners[i].start_scan(scan_flags, scan_min, scan_max, scan_epg);
			sleep(5); // FIXME
		}
		break;
	case 4: /* speed AND partial redundancy */
		partial_redundancy = (num_tuners > 2) ? (num_tuners - 2) : 0;
		/* FALL-THRU */
	default:
	case 3: /* speed AND redundancy */
		for (int j = 0; j < num_tuners - partial_redundancy; j++) {
			for (int i = 0; i < num_tuners; i++) {
				if (j) {
					tuners[i].wait_for_scan_complete();
				}
				int scan_start, scan_end;
				if (i + j < num_tuners) {
					scan_start = scan_min + ((0 + (i + j)) * (unsigned int)channels_to_scan/num_tuners);
					scan_end   = scan_min + ((1 + (i + j)) * (unsigned int)channels_to_scan/num_tuners);
				} else {
					scan_start = scan_min + ((0 + ((i + j) - num_tuners)) * (unsigned int)channels_to_scan/num_tuners);
					scan_end   = scan_min + ((1 + ((i + j) - num_tuners)) * (unsigned int)channels_to_scan/num_tuners);
				}
				fprintf(stderr, "speed & %sredundancy scan: pass %d of %d, tuner %d scanning from %d to %d\n",
					(partial_redundancy) ? "partial " : "",
					j + 1, num_tuners - partial_redundancy,
					i, scan_start, scan_end);
				tuners[i].start_scan(scan_flags, scan_start, scan_end, scan_epg);
				sleep(1); // FIXME
			}
		}
		break;
	}
	for (int i = 0; i < num_tuners; i++)
		tuners[i].wait_for_scan_complete();
#if 0
	for (int i = 0; i < num_tuners; i++) {
		count += tuners[i].feeder.parser.xine_dump();
		//fprintf(stderr, "tuner %d found %d services\n", i, count);
	}
#else
	count += tuners[0].feeder.parser.xine_dump();
#endif
	fprintf(stderr, "found %d services in total\n", count);
}

int main(int argc, char **argv)
{
	dvbtee_context context;
	int opt, channel = 0;
	bool b_read_dvr = false;
	bool b_scan     = false;
	bool scan_epg   = false;

	/* LinuxDVB context: */
	int dvb_adap = 0; /* ID X, /dev/dvb/adapterX/ */
	int demux_id = 0; /* ID Y, /dev/dvb/adapterX/demuxY */
	int dvr_id   = 0; /* ID Y, /dev/dvb/adapterX/dvrY */
	int fe_id    = 0; /* ID Y, /dev/dvb/adapterX/frontendY */

	unsigned int scan_flags  = 0;
	unsigned int scan_min    = 0;
	unsigned int scan_max    = 0;
	unsigned int scan_method = 0;

	int num_tuners           = -1;
	unsigned int timeout     = 0;

	char filename[256];
	memset(&filename, 0, sizeof(filename));

	fprintf(stderr, "\n"
			"dvbtee\n");

        while ((opt = getopt(argc, argv, "a:A:c:C:f:F:t:T:s::E")) != -1) {
		switch (opt) {
		case 'a': /* adapter */
			dvb_adap = strtoul(optarg, NULL, 0);
			b_read_dvr = true;
			break;
		case 'A': /* ATSC / QAM */
			scan_flags = strtoul(optarg, NULL, 0);
			break;
		case 'c': /* channel / scan min */
			channel = strtoul(optarg, NULL, 0);
			scan_min = strtoul(optarg, NULL, 0);
			b_read_dvr = true;
			break;
		case 'C': /* channel / scan max */
			scan_max = strtoul(optarg, NULL, 0);
			b_read_dvr = true;
			break;
		case 'E': /* enable EPG scan */
			b_scan = true; // FIXME
			scan_epg = true;
			break;
		case 'f': /* frontend */
			fe_id = strtoul(optarg, NULL, 0);
			b_read_dvr = true;
			break;
		case 'F': /* Filename */
			strcpy(filename, optarg);
			break;
		case 't': /* timeout */
			timeout = strtoul(optarg, NULL, 0);
			break;
		case 'T': /* number of tuners (dvb adapters) allowed to use, 0 for all */
			num_tuners = strtoul(optarg, NULL, 0);
			b_read_dvr = true;
			break;
		case 's': /* scan, optional arg when using multiple tuners: 1 for speed, 2 for redundancy, 3 for speed AND redundancy, 4 for optimized speed / partial redundancy */
			b_scan = true;
			scan_method = (optarg) ? strtoul(optarg, NULL, 0) : 0;
			fprintf(stderr, "MULTISCAN: %d...\n", scan_method);
			break;
		default:  /* bad cmd line option */
			return -1;
		}
	}

	context_map[getpid()] = &context;

	signal(SIGINT,  signal_callback_handler); /* Program interrupt. (ctrl-c) */
	signal(SIGABRT, signal_callback_handler); /* Process detects error and reports by calling abort */
	signal(SIGFPE,  signal_callback_handler); /* Floating-Point arithmetic Exception */
	signal(SIGILL,  signal_callback_handler); /* Illegal Instruction */
	signal(SIGSEGV, signal_callback_handler); /* Segmentation Violation */
	signal(SIGTERM, signal_callback_handler); /* Termination */
	signal(SIGHUP,  signal_callback_handler); /* Hangup */
#if 1 /* FIXME */
	ATSCMultipleStringsInit();
#endif
	if ((b_scan) || (b_read_dvr))
		context.tuner.set_device_ids(dvb_adap, fe_id, demux_id, dvr_id);
	if (b_scan) {
		if (num_tuners >= 0)
			multiscan(&context, num_tuners, scan_method, scan_flags, scan_min, scan_max, scan_epg);
		else
			context.tuner.scan_for_services(scan_flags, scan_min, scan_max, scan_epg);
		goto exit;
	}

	if (strlen(filename)) {
		if (0 <= context._file_feeder.open_file(filename)) {
			if (0 == context._file_feeder.start()) {
				context._file_feeder.wait_for_streaming_or_timeout(timeout);
				context._file_feeder.stop();
			}
			context._file_feeder.close_file();
		}
		goto exit;
	}

	if (channel) {
		fprintf(stderr, "TUNE to channel %d...\n", channel);
		int fe_fd = context.tuner.open_fe();
		if (fe_fd < 0)
			return fe_fd;

		if (!scan_flags)
			scan_flags = SCAN_VSB;

		if (context.tuner.tune_atsc(
				(scan_flags == SCAN_VSB) ? VSB_8 : QAM_256, channel)) {
			if (!context.tuner.wait_for_lock_or_timeout(2000)) {
				context.tuner.close_fe();
				goto exit; /* NO LOCK! */
			}
			context.tuner.feeder.parser.set_channel_info(channel,
				(scan_flags == SCAN_VSB) ? atsc_vsb_chan_to_freq(channel) : atsc_qam_chan_to_freq(channel),
				(scan_flags == SCAN_VSB) ? "8VSB" : "QAM256");
		}
	}

	if (b_read_dvr) {
		/* assume frontend is already streaming,
		   all we have to do is read from the DVR device */
		if (0 == context.tuner.start_feed()) {
			context.tuner.feeder.wait_for_streaming_or_timeout(timeout);
			context.tuner.stop_feed();
		}
		if (channel) /* if we tuned the frontend ourselves then close it */
			context.tuner.close_fe();
	}
	else
	if (0 == context._file_feeder.parser.get_fed_pkt_count()) {
		fprintf(stderr, "reading from STDIN\n");
		if (0 == context._file_feeder.start_stdin()) {
			context._file_feeder.wait_for_streaming_or_timeout(timeout);
			context._file_feeder.stop();
		}
	}
exit:
//	cleanup(&context);
#if 1 /* FIXME */
	ATSCMultipleStringsDeInit();
#endif
	return 0;
}