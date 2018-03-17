/*
 * $Id: pzapit.cpp,v 1.50 2004/02/02 13:34:39 obi Exp $
 *
 * simple commandline client for zapit
 *
 * Copyright (C) 2002 by Andreas Oberritter <obi@tuxbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <cstdio>
#include <cstring>
#include <iostream>
#include <unistd.h> /* sleep */

#include <zapit/client/zapitclient.h>

int usage (const char * basename)
{
	std::cout << "get current channel id: " << basename << " -gi" << std::endl;
	std::cout << "bouquet list: " << basename << " [-ra]" << std::endl;
	std::cout << "channel list: " << basename << " [-ra] <bouquet-number>" << std::endl;
	std::cout << "zap by number: " << basename << " [-ra] <bouquet-number> <channel-number>" << std::endl;
	std::cout << "zap by name: " << basename << " [-ra] -n <channel-name>" << std::endl;
	std::cout << "zap by channel id: " << basename << " -zi <chanid (hex)>" << std::endl;
	std::cout << "set diseqc type: " << basename << " -dt <type>" << std::endl;
	std::cout << "set diseqc repeats: " << basename << " -dr <count>" << std::endl;
	std::cout << "(-ra toggles radio mode)" << std::endl;
	std::cout << "switch record mode on/off: " << basename << " -re" << std::endl;
	std::cout << "start/stop playback: " << basename << " -p" << std::endl
		  << std::endl;
	std::cout << "change audio pid: " << basename << " -a <audio-number>" << std::endl
		  << std::endl;
	std::cout << "reload channels bouquets: " << basename << " -c" << std::endl;
	std::cout << "save bouquets: " << basename << " -sb" << std::endl
		  << std::endl;
	std::cout << "show satellites: " << basename << " -sh" << std::endl;
	std::cout << "rezap " << basename << " -rz" << std::endl;
	std::cout << "select satellites: " << basename << " -se <satmask> <diseqc order>" << std::endl;
	std::cout << "start transponderscan: " << basename << " -st" << std::endl
		  << std::endl;
	std::cout << "mute audio: " << basename << " -mute" << std::endl;
	std::cout << "unmute audio: " << basename << " -unmute" << std::endl;
	std::cout << "set volume: " << basename << " -vol <0..64>" << std::endl;
	std::cout << "register neutrino as event client: " << basename << " -rn" << std::endl;
	std::cout << "shutdown zapit: " << basename << " -kill" << std::endl;
	std::cout << "enter standby: " << basename << " -esb" << std::endl;
	std::cout << "leave standby: " << basename << " -lsb" << std::endl;
	std::cout << "get aspect ratio: " << basename << " -var" << std::endl;
	std::cout << "set aspect ratio: " << basename << " -var <aspectratio>" << std::endl;
	std::cout << "get 4:3 mode: " << basename << " -vm43" << std::endl;
	std::cout << "set 4:3 mode: " << basename << " -vm43 <4:3mode>" << std::endl;
	std::cout << "switch to hd 1080i mode: " << basename << " --1080" << std::endl;
	std::cout << "switch to pal mode: " << basename << " --pal" << std::endl;
	std::cout << "switch to hd 720p mode: " << basename << " --720p" << std::endl;
	std::cout << "send diseqc 1.2 motor command: " << basename << " -m <cmdtype> <addr> <cmd> <number of parameters> <parameter 1> <parameter 2>" << std::endl;
	std::cout << "lock remote control: " << basename << " -lockrc" << std::endl;
	std::cout << "unlock remote control: " << basename << " -unlockrc" << std::endl;
	return -1;
}

int main (int argc, char** argv)
{
	int i;
	uint32_t  j;
	//uint32_t  k;
	uint64_t ii;

	int bouquet = -1;
	unsigned int channel = 0;
	unsigned int count;
	int diseqcRepeats = -1;
	int diseqcType = -1;
	uint64_t satmask = 0xFFFF;
	int audio = 0;
	int mute = -1;
	int volume = -1;
	int nvod = -1;
	int arat = -1;
	int m43 = -1;
	int lockrc = -1;
	const char * channelName = NULL;

	bool playback = false;
	bool recordmode = false;
	bool radio = false;
	bool reload = false;
	bool register_neutrino = false;
	bool savebouquets = false;
	bool show_satellites = false;
	bool set_pal = false;
	int  set_hd = 0;
	bool scan = false;
	bool rezap = false;
	bool zapByName = false;
	bool killzapit = false;
	bool enterStandby = false;
	bool leaveStandby = false;
	bool sendMotorCommand = false;
	bool quiet = false;
	bool getchannel = false;
	bool aspectratio = false;
	bool mode43 = false;
	uint8_t motorCmdType = 0;
	uint8_t motorCmd = 0;
	uint8_t motorNumParameters = 0;
	uint8_t motorParam1 = 0;
	uint8_t motorParam2 = 0;
	uint8_t motorAddr = 0;
	uint32_t  diseqc[5];
	unsigned int tmp;
	int scan_mode = 1;
	t_channel_id zapsid = 0;
	/* command line */
	for (i = 1; i < argc; i++)
	{
		if (!strncmp(argv[i], "-a", 2))
		{
			if (i < argc - 1)
			{
				sscanf(argv[++i], "%d", &audio);
				continue;
			}
		}
		else if (!strncmp(argv[i], "-dr", 3))
		{
			if (i < argc - 1)
			{
				sscanf(argv[++i], "%d", &diseqcRepeats);
				continue;
			}
		}
		else if (!strncmp(argv[i], "-dt", 3))
		{
			if (i < argc - 1)
			{
				sscanf(argv[++i], "%d", &diseqcType);
				continue;
			}
		}
		else if (!strncmp(argv[i], "-q", 2))
		{
			quiet = true;
			continue;
		}
		else if (!strncmp(argv[i], "-c", 2))
		{
			reload = true;
			continue;
		}
		else if (!strncmp(argv[i], "-esb", 4))
		{
			enterStandby = true;
			continue;
		}
		else if (!strncmp(argv[i], "-kill", 5))
		{
			killzapit = true;
			continue;
		}
		else if (!strncmp(argv[i], "-lsb", 4))
		{
			leaveStandby = true;
			continue;
		}
		else if (!strncmp(argv[i], "-rz", 3))
		{
			rezap = true;
			continue;
		}
		else if (!strncmp(argv[i], "-mute", 5))
		{
			mute = 1;
			continue;
		}
		else if (!strncmp(argv[i], "-m", 2))
		{
			if (i < argc - 6)
			{
				sscanf(argv[++i], "%x", &tmp);
				motorCmdType = tmp;
				sscanf(argv[++i], "%x", &tmp);
				motorAddr = tmp;
				sscanf(argv[++i], "%x", &tmp);
				motorCmd = tmp;
				sscanf(argv[++i], "%x", &tmp);
				motorNumParameters = tmp;
				sscanf(argv[++i], "%x", &tmp);
				motorParam1 = tmp;
				sscanf(argv[++i], "%x", &tmp);
				motorParam2 = tmp;
				printf("[pzapit] motor command = %x %x %x %x %x %x\n", motorCmdType, motorAddr, motorCmd, motorNumParameters, motorParam1, motorParam2);
				sendMotorCommand = true;
				continue;
			}
		}
		else if (!strncmp(argv[i], "-rn", 3))
		{
			register_neutrino = true;
			continue;
		}
		else if (!strncmp(argv[i], "-nvod", 5))
		{
			if (i < argc - 1)
			{
				sscanf(argv[++i], "%d", &nvod);
				continue;
			}
		}
		else if (!strncmp(argv[i], "-n", 2))
		{
			if (i < argc - 1)
			{
				zapByName = true;
				channelName = argv[++i];
				continue;
			}
		}
		else if (!strncmp(argv[i], "-p", 2))
		{
			playback = true;
			continue;
		}
		else if (!strncmp(argv[i], "-ra", 3))
		{
			radio = true;
			continue;
		}
		else if (!strncmp(argv[i], "-re", 3))
		{
			recordmode = true;
			continue;
		}
		else if (!strncmp(argv[i], "-var", 4))
		{
			aspectratio = true;
			if (i < argc - 1)
				sscanf(argv[++i], "%d", &arat);
			continue;
		}
		else if (!strncmp(argv[i], "-vm43", 5))
		{
			mode43 = true;
			if (i < argc - 1)
			{
				sscanf(argv[++i], "%d", &m43);
				continue;
			}
			continue;
		}
		else if (!strncmp(argv[i], "-sb", 3))
		{
			savebouquets = true;
			continue;
		}
		else if (!strncmp(argv[i], "-se", 3))
		{
			if (i < argc - 2)
			{
				sscanf(argv[++i], "%" SCNd64 "", &satmask);
				sscanf(argv[++i], "%d", &diseqc[0]);
				/*
								diseqc[0] = strlen(argv[i+1]);
								for (i++, j = 0; j <= diseqc[0]; j++)
								{
									diseqc[j+1] = argv[i][j] - 48;
								}
				*/
				continue;
			}
		}
		else if (!strncmp(argv[i], "-sh", 3))
		{
			show_satellites = true;
			continue;
		}
		else if (!strncmp(argv[i], "-st", 3))
		{
			scan = true;
			if (i < argc - 1)
			{
				sscanf(argv[++i], "%d", &scan_mode);
			}
			continue;
		}
		else if (!strncmp(argv[i], "--pal", 4))
		{
			set_pal = true;
			continue;
		}

		else if (!strncmp(argv[i], "--1080", 6))
		{
			set_hd = 8;
			continue;
		}
		else if (!strncmp(argv[i], "--1083", 6))
		{
			set_hd = 9;
			continue;
		}
		else if (!strncmp(argv[i], "--1082", 6))
		{
			set_hd = 10;
			continue;
		}
		else if (!strncmp(argv[i], "--720", 5))
		{
			set_hd = 7;
			continue;
		}
		else if (!strncmp(argv[i], "-unmute", 7))
		{
			mute = 0;
			continue;
		}
		else if (!strncmp(argv[i], "-vol", 4))
		{
			if (i < argc - 1)
			{
				sscanf(argv[++i], "%d", &volume);
				continue;
			}
		}
		else if (!strncmp(argv[i], "-gi", 3))
		{
			getchannel = true;
			continue;
		}
		else if (!strncmp(argv[i], "-zi", 3))
		{
			if (i < argc - 1)
			{
				sscanf(argv[++i], "%" SCNx64 "", &zapsid);
				continue;
			}
		}
		else if (!strncmp(argv[i], "-lockrc", 7))
		{
			lockrc = 1;
			continue;
		}
		else if (!strncmp(argv[i], "-unlockrc", 9))
		{
			lockrc = 0;
			continue;
		}
		else if (i < argc - 1)
		{
			if ((sscanf(argv[i], "%d", &bouquet) > 0) && (sscanf(argv[++i], "%u", &channel) > 0))
				continue;
		}
		else if (sscanf(argv[i], "%d", &bouquet) > 0)
			continue;

		return usage(argv[0]);
	}

	/* create zapit client */
	CZapitClient zapit;

#if 0
	TP_params TP;
	TP.TP_id = 12345;
	TP.polarization = 1;
	TP.feparams.Frequency = 11727000;
	TP.feparams.u.qpsk.SymbolRate = 27500000;
	TP.feparams.u.qpsk.FEC_inner = (CodeRate) 3;

	zapit.scan_TP(TP);
	exit(0);
#endif

	/* send diseqc 1.2 motor command */
	if (sendMotorCommand)
	{
		zapit.sendMotorCommand(motorCmdType, motorAddr, motorCmd, motorNumParameters, motorParam1, motorParam2);
		return 0;
	}

	/* kill zapit*/
	if (killzapit)
	{
		zapit.shutdown();
		std::cout << "zapit shot down :)" << std::endl;
		return 0;
	}

	if (enterStandby)
	{
		zapit.setStandby(true);
		return 0;
	}

	if (leaveStandby)
	{
		zapit.setStandby(false);
		return 0;
	}

	/* audio mute */
	if (mute != -1)
	{
		std::cout << "mute/unmute" << std::endl;
		zapit.muteAudio(mute);
		return 0;
	}

	if (volume != -1)
	{
		std::cout << "set volume" << std::endl;
		zapit.setVolume(volume, volume);
		return 0;
	}
	if (lockrc != -1)
	{
		zapit.lockRc(lockrc);
		return 0;
	}
	if (rezap)
	{
		zapit.Rezap();
		return 0;
	}

	/* reload services */
	if (reload)
	{
		std::cout << "reloading channels" << std::endl;
		zapit.reinitChannels();
		return 0;
	}

	if (register_neutrino)
	{
#define NEUTRINO_UDS_NAME "/tmp/neutrino.sock"
		std::cout << "registering neutrino" << std::endl;
		for (int ic = CZapitClient::FIRST_EVENT_MARKER; ic < CZapitClient::LAST_EVENT_MARKER; ic++)
			zapit.registerEvent(ic, 222, NEUTRINO_UDS_NAME);
		return 0;
	}

	if (diseqcType != -1)
	{
		zapit.setDiseqcType((diseqc_t) diseqcType);

		if (diseqcRepeats == -1)
			return 0;
	}

	if (diseqcRepeats != -1)
	{
		zapit.setDiseqcRepeat(diseqcRepeats);
		return 0;
	}

	if (playback)
	{
		if (zapit.isPlayBackActive())
			zapit.stopPlayBack();
		else
			zapit.startPlayBack();

		if (!recordmode)
			return 0;
	}

	if (recordmode)
	{
		zapit.setRecordMode(!zapit.isRecordModeActive());
		return 0;
	}

	if (aspectratio)
	{
		if(arat >= 0)
			zapit.setAspectRatio(arat);
		else
		{
			zapit.getAspectRatio(&arat);
			printf("%d\n", arat);
		}
		return 0;
	}

	if (mode43)
	{
		if(m43 >= 0)
			zapit.setMode43(m43);
		else
		{
			zapit.getMode43(&m43);
			printf("%d\n",m43);
		}
		return 0;
	}

	if (savebouquets)
	{
		zapit.saveBouquets();
		return 0;
	}

	if (show_satellites)
	{
		std::vector<CZapitClient::responseGetSatelliteList> satelliteList;
		zapit.getScanSatelliteList(satelliteList);

		std::vector<CZapitClient::responseGetSatelliteList>::const_iterator rI;
		for ( ii = 0, rI = satelliteList.begin(); rI != satelliteList.end(); ii++, rI++)
			printf("%" PRId64 " : %s %d\n", ii, rI->satName, rI->satPosition);
		//std::cout << (1 << ii) << ": " << rI->satName << std::endl;

		return 0;
	}
	else if (satmask != 0xFFFF)
	{
		std::vector<CZapitClient::responseGetSatelliteList> satelliteList;
		zapit.getScanSatelliteList(satelliteList);

		std::vector<CZapitClient::commandSetScanSatelliteList> newSatelliteList;
		CZapitClient::commandSetScanSatelliteList item;

		for (j = 0; j < satelliteList.size(); j++)
		{
			if (satmask == j)
			{
				std::cout << "diseqc " << diseqc[0] << ": " << satelliteList[j].satName << std::endl;

				strcpy(item.satName, satelliteList[j].satName);
				item.position = diseqc[0];
				newSatelliteList.push_back(item);
				break;
			}
		}

		zapit.setScanSatelliteList(newSatelliteList);

		return 0;
	}

	/* transponderscan */
	if (scan)
	{
		unsigned int satellite;
		unsigned int processed_transponder;
		unsigned int transponder;
		unsigned int services;
		printf("Start scan, mode %d\n", scan_mode);
		zapit.startScan(scan_mode);

		while (zapit.isScanReady(satellite, processed_transponder, transponder, services) == false)
		{
			std::cout << "satellite: " << satellite << ", transponder: " << processed_transponder <<", of: " << transponder << ", services: " << services << std::endl;
			sleep(1);
		}

		return 0;
	}

	if (set_pal)
	{
		//zapit.stopPlayBack();
		zapit.setVideoSystem(2);
		//zapit.startPlayBack();
		return 0;
	}

	if (set_hd)
	{
		//zapit.stopPlayBack();
		zapit.setVideoSystem(set_hd);
		//zapit.startPlayBack();
		return 0;
	}
	if (getchannel)
	{
		t_channel_id channelid = zapit.getCurrentServiceID();
		printf("%" PRIx64 " (%s)\n", channelid, (zapit.getChannelName(channelid)).c_str());
		return 0;
	}


	/* choose source mode */
	zapit.setMode(radio ? CZapitClient::MODE_RADIO : CZapitClient::MODE_TV);

	if (zapsid > 0)
	{
		printf("Zapping to: %" PRIx64 " (%s) ", zapsid, (zapit.getChannelName(zapsid)).c_str());
		tmp = zapit.zapTo_serviceID(zapsid);
		if (!tmp)
		  printf("failed");
		printf("\n");
		return tmp;
	}
	/* set audio channel */
	if (audio)
	{
		zapit.setAudioChannel(audio - 1);
		return 0;
	}

	if (nvod != -1)
	{
		zapit.zaptoNvodSubService(nvod);
		return 0;
	}
	else
	{
		std::vector<CZapitClient::responseGetBouquetChannels> channels;

		if (zapByName)
		{
			zapit.getChannels(channels);

			std::vector<CZapitClient::responseGetBouquetChannels>::const_iterator ch_resp;
			for (ch_resp = channels.begin(), channel = 1; ch_resp != channels.end(); ch_resp++, ++channel)
			{
				if (!strcasecmp(ch_resp->name, channelName))
				{
					std::cout << "found channel number: " << channel << std::endl;
					goto channel_found;
				}
			}

			std::cout << "channel not found." << std::endl;
			return 0;
		}
		else /* zap by bouquet number and channel number */
		{
			/* read channel list */
			if (bouquet != -1)
				zapit.getBouquetChannels(bouquet - 1, channels, CZapitClient::MODE_CURRENT, true);

			/* display bouquet list */
			else
			{
				std::vector<CZapitClient::responseGetBouquets> bouquets;
				std::vector<CZapitClient::responseGetBouquets>::const_iterator b_resp;

				zapit.getBouquets(bouquets, false);

				for (b_resp = bouquets.begin(); b_resp != bouquets.end(); ++b_resp)
					std::cout << (b_resp->bouquet_nr + 1) << ": " << b_resp->name << std::endl;

				return 0;
			}

			/* display channel list */
			if (!channel)
			{
				std::vector<CZapitClient::responseGetBouquetChannels>::const_iterator ch_resp;
				for (ch_resp = channels.begin(), channel = 1; ch_resp != channels.end(); ch_resp++, ++channel)
					//std::cout << channel << ": " << ch_resp->name << ": " << ch_resp->channel_id<< std::endl;
					printf("%3u: %s (%04x)\n", channel, ch_resp->name, (short) (ch_resp->channel_id &0xFFFF));
				return 0;
			}
		}

		/* zap */
		if (channel > channels.size())
		{
			std::cout << "Only " << channels.size() << " channels in bouquet " << bouquet << std::endl;
			return 0;
		}

channel_found:
		zapit.zapTo(channels[channel-1].nr);
		std::cout << "zapped to " << channels[channel-1].name << std::endl;
	}

	if (!quiet)
	{
		CZapitClient::responseGetPIDs pids;
		zapit.getPIDS(pids);

		if (pids.PIDs.vpid)
			std::cout << "   video: 0x" << std::hex << pids.PIDs.vpid << std::endl;

		if (pids.PIDs.vtxtpid)
			std::cout << "teletext: 0x" << std::hex << pids.PIDs.vtxtpid << std::endl;

		if (pids.PIDs.pcrpid)
			std::cout << "     pcr: 0x" << std::hex << pids.PIDs.pcrpid << std::endl;

		for (count = 0; count < pids.APIDs.size(); count++)
		{
			std::cout << " audio " << std::dec << count + 1 << ": 0x" << std::hex << pids.APIDs[count].pid << " (" << pids.APIDs[count].desc;
			if (pids.APIDs[count].is_ac3)
				std::cout << ", ac3";
			else if (pids.APIDs[count].is_aac)
				std::cout << ", aac";
			else
				std::cout << ", unknown";

			std::cout << ")" << std::endl;
		}
	}

	return 0;
}
