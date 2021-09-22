/*
 * Copyright (C) 2011 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
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

#include <zapit/debug.h>
#include <zapit/getservices.h>
#include <hardware/dmx.h>
#include <zapit/scanait.h>
#include <zapit/scan.h>
#include <dvbsi++/descriptor_tag.h>
#include <dvbsi++/linkage_descriptor.h>
#include <dvbsi++/private_data_specifier_descriptor.h>
#include <dvbsi++/transport_protocol_descriptor.h>
#include <dvbsi++/simple_application_location_descriptor.h>
#include <dvbsi++/simple_application_boundary_descriptor.h>
#include <math.h>
#include <eitd/edvbstring.h>
#include <system/set_threadname.h>

#define DEBUG_AIT
#define DEBUG_AIT_UNUSED
#define DEBUG_LCN

CAit::CAit(int dnum)
{
	dmxnum = dnum;
	pid = 0;
}

bool CAit::Start()
{
	int ret = start();
	return (ret == 0);
}

bool CAit::Stop()
{
	int ret = join();
	return (ret == 0);
}

void CAit::run()
{
	set_threadname("zap:ait");
	if(Parse())
		printf("[scan] AIT finished.\n");
	else
		printf("[scan] AIT failed !\n");
}

CAit::~CAit()
{
}

bool CAit::Read()
{
	bool ret = true;

	cDemux * dmx = new cDemux(dmxnum);
	dmx->Open(DMX_PSI_CHANNEL);

	unsigned char buffer[AIT_SECTION_SIZE];

	unsigned char filter[DMX_FILTER_SIZE];
	unsigned char mask[DMX_FILTER_SIZE];

	memset(filter, 0x00, DMX_FILTER_SIZE);
	memset(mask, 0x00, DMX_FILTER_SIZE);

	int flen = 1;
	filter[0] = 0x74;
	mask[0] = 0xFF;

	if ((!dmx->sectionFilter(pid, filter, mask, flen)) || (dmx->Read(buffer, AIT_SECTION_SIZE) < 0))
	{
		printf("CAit::Read: pid %x failed\n", pid);
		ret = false;
	}
	else
	{
		printf("CAit::Read: pid %x found\n", pid);
		ApplicationInformationSection * ait = new ApplicationInformationSection(buffer);
		sections.push_back(ait);
	}
	dmx->Stop();
	delete dmx;
	return ret;
}

bool CAit::Parse()
{
	printf("[ait] trying to parse AIT\n");

	if(!Read())
		return false;

	FILE * pFile = fopen ("/tmp/ait.txt","w");
	if (pFile)
		fprintf(pFile, "Channel: %s\n", name.c_str());
	short int profilecode = 0;
	int orgid = 0, appid = 0, profileVersion = 0;

	int sectionLength = 0;
	ApplicationInformationSectionConstIterator sit;
	for (sit = sections.begin(); sit != sections.end(); ++sit)
	{

		if (CServiceScan::getInstance()->Aborted())
			return false;

		std::list<ApplicationInformation *>::const_iterator i = (*sit)->getApplicationInformation()->begin();
		sectionLength = (*sit)->getSectionLength() + 3;

		for (; i != (*sit)->getApplicationInformation()->end(); ++i)
		{
			std::string hbbtvUrl = "", applicationName = "";
			std::string TPDescPath = "", SALDescPath = "";
			int controlCode = (*i)->getApplicationControlCode();
			ApplicationIdentifier * applicationIdentifier = (ApplicationIdentifier *)(*i)->getApplicationIdentifier();
			profilecode = 0;
			orgid = applicationIdentifier->getOrganisationId();
			appid = applicationIdentifier->getApplicationId();
			//printf("[ait] found applicaions ids (pid : 0x%x) : orgid : %d, appid : %d\n", pid, orgid, appid);
			if (controlCode == 1 || controlCode == 2) /* 1:AUTOSTART, 2:PRESENT  */
			{
				for (DescriptorConstIterator desc = (*i)->getDescriptors()->begin();
				        desc != (*i)->getDescriptors()->end(); ++desc)
				{
					switch ((*desc)->getTag())
					{
					case APPLICATION_DESCRIPTOR:
					{
						ApplicationDescriptor* applicationDescriptor = (ApplicationDescriptor*)(*desc);
						const ApplicationProfileList* applicationProfiles = applicationDescriptor->getApplicationProfiles();
						ApplicationProfileConstIterator interactionit = applicationProfiles->begin();
						for(; interactionit != applicationProfiles->end(); ++interactionit)
						{
							profilecode = (*interactionit)->getApplicationProfile();
							profileVersion = PACK_VERSION(
							                     (*interactionit)->getVersionMajor(),
							                     (*interactionit)->getVersionMinor(),
							                     (*interactionit)->getVersionMicro()
							                 );
						}
						break;
					}
					case APPLICATION_NAME_DESCRIPTOR:
					{
						ApplicationNameDescriptor *nameDescriptor  = (ApplicationNameDescriptor*)(*desc);
						ApplicationNameConstIterator interactionit = nameDescriptor->getApplicationNames()->begin();
						for(; interactionit != nameDescriptor->getApplicationNames()->end(); ++interactionit)
						{
							applicationName = (*interactionit)->getApplicationName();
							//if(controlCode == 1) printf("[ait] applicationname: %s\n", applicationName.c_str());
							break;
						}
						break;
					}
					case TRANSPORT_PROTOCOL_DESCRIPTOR:
					{
						TransportProtocolDescriptor *transport = (TransportProtocolDescriptor*)(*desc);
						switch (transport->getProtocolId())
						{
						case 1: /* object carousel */
							break;
						case 2: /* ip */
							break;
						case 3: /* interaction */
						{
							InterActionTransportConstIterator interactionit = transport->getInteractionTransports()->begin();
							for(; interactionit != transport->getInteractionTransports()->end(); ++interactionit)
							{
								TPDescPath = (*interactionit)->getUrlBase()->getUrl();
								break;
							}
							break;
						}
						}
						break;
					}
					case GRAPHICS_CONSTRAINTS_DESCRIPTOR:
						break;
					case SIMPLE_APPLICATION_LOCATION_DESCRIPTOR:
					{
						SimpleApplicationLocationDescriptor *applicationlocation = (SimpleApplicationLocationDescriptor*)(*desc);
						SALDescPath = applicationlocation->getInitialPath();
						break;
					}
					case APPLICATION_USAGE_DESCRIPTOR:
						break;
					case SIMPLE_APPLICATION_BOUNDARY_DESCRIPTOR:
						break;
					}
				}
				hbbtvUrl = TPDescPath + SALDescPath;
			}
			if(!hbbtvUrl.empty())
			{
				printf("[ait] detected AppID: %d, AppName: %s, Url: %s\n", appid, applicationName.c_str(), hbbtvUrl.c_str());
				if (pFile)
				{
					fprintf(pFile, "AppID: %d, AppName: %s, Url: %s\n", appid, applicationName.c_str(), hbbtvUrl.c_str());
				}
			}
		}
	}
	if (pFile)
		fclose(pFile);
	return true;
}

bool CAit::Parse(CZapitChannel * const channel)
{
	pid = channel->getAitPid();
	name = channel->getName();
	unlink("/tmp/ait.txt");
	if(pid > 0)
	{
		Parse();
		return true;
	}
	return false;
}
