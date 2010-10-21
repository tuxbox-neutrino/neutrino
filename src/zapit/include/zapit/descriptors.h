/*
 * $Id: descriptors.h,v 1.19 2004/02/17 16:26:04 thegoodguy Exp $
 *
 * (C) 2002-2003 Andreas Oberritter <obi@tuxbox.org>
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

#ifndef __zapit_descriptors_h__
#define __zapit_descriptors_h__

#include "types.h"

void generic_descriptor(const unsigned char * const buffer);
void video_stream_descriptor(const unsigned char * const buffer);
void audio_stream_descriptor(const unsigned char * const buffer);
void hierarchy_descriptor(const unsigned char * const buffer);
void registration_descriptor(const unsigned char * const buffer);
void data_stream_alignment_descriptor(const unsigned char * const buffer);
void target_background_grid_descriptor(const unsigned char * const buffer);
void Video_window_descriptor(const unsigned char * const buffer);
void CA_descriptor(const unsigned char * const buffer, uint16_t ca_system_id, uint16_t* ca_pid);
void ISO_639_language_descriptor(const unsigned char * const buffer);
void System_clock_descriptor(const unsigned char * const buffer);
void Multiplex_buffer_utilization_descriptor(const unsigned char * const buffer);
void Copyright_descriptor(const unsigned char * const buffer);
void Maximum_bitrate_descriptor(const unsigned char * const buffer);
void Private_data_indicator_descriptor(const unsigned char * const buffer);
void Smoothing_buffer_descriptor(const unsigned char * const buffer);
void STD_descriptor(const unsigned char * const buffer);
void IBP_descriptor(const unsigned char * const buffer);
void MPEG4_video_descriptor(const unsigned char * const buffer);
void MPEG4_audio_descriptor(const unsigned char * const buffer);
void IOD_descriptor(const unsigned char * const buffer);
void SL_descriptor(const unsigned char * const buffer);
void FMC_descriptor(const unsigned char * const buffer);
void External_ES_ID_descriptor(const unsigned char * const buffer);
void MuxCode_descriptor(const unsigned char * const buffer);
void FmxBufferSize_descriptor(const unsigned char * const buffer);
void MultiplexBuffer_descriptor(const unsigned char * const buffer);
void FlexMuxTiming_descriptor(const unsigned char * const buffer);
void network_name_descriptor(const unsigned char * const buffer);
void service_list_descriptor(const unsigned char * const buffer, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
void stuffing_descriptor(const unsigned char * const buffer);
int satellite_delivery_system_descriptor(const unsigned char * const buffer, t_transport_stream_id transport_stream_id, t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
int cable_delivery_system_descriptor(const unsigned char * const buffer, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq);
void VBI_data_descriptor(const unsigned char * const buffer);
void VBI_teletext_descriptor(const unsigned char * const buffer);
void bouquet_name_descriptor(const unsigned char * const buffer);
void service_descriptor(const unsigned char * const buffer, const t_service_id service_id, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq, bool free_ca);
void current_service_descriptor(const unsigned char * const buffer, const t_service_id service_id, const t_transport_stream_id transport_stream_id, const t_original_network_id original_network_id, t_satellite_position satellitePosition, freq_id_t freq, bool free_ca);
void country_availability_descriptor(const unsigned char * const buffer);
void linkage_descriptor(const unsigned char * const buffer);
int NVOD_reference_descriptor(const unsigned char * const buffer, const unsigned int num, t_transport_stream_id * const, t_original_network_id * const, t_service_id * const);
void time_shifted_service_descriptor(const unsigned char * const buffer);
void short_event_descriptor(const unsigned char * const buffer);
void extended_event_descriptor(const unsigned char * const buffer);
void time_shifted_event_descriptor(const unsigned char * const buffer);
void component_descriptor(const unsigned char * const buffer);
void mosaic_descriptor(const unsigned char * const buffer);
void stream_identifier_descriptor(const unsigned char * const buffer);
void CA_identifier_descriptor(const unsigned char * const buffer);
void content_descriptor(const unsigned char * const buffer);
void parental_rating_descriptor(const unsigned char * const buffer);
void teletext_descriptor(const unsigned char * const buffer);
void telephone_descriptor(const unsigned char * const buffer);
void local_time_offset_descriptor(const unsigned char * const buffer);
void subtitling_descriptor(const unsigned char * const buffer);
int terrestrial_delivery_system_descriptor(const unsigned char * const buffer);
void multilingual_network_name_descriptor(const unsigned char * const buffer);
void multilingual_bouquet_name_descriptor(const unsigned char * const buffer);
void multilingual_service_name_descriptor(const unsigned char * const buffer);
void multilingual_component_descriptor(const unsigned char * const buffer);
void private_data_specifier_descriptor(const unsigned char * const buffer);
void service_move_descriptor(const unsigned char * const buffer);
void short_smoothing_buffer_descriptor(const unsigned char * const buffer);
void frequency_list_descriptor(const unsigned char * const buffer);
void partial_transport_stream_descriptor(const unsigned char * const buffer);
void data_broadcast_descriptor(const unsigned char * const buffer);
void CA_system_descriptor(const unsigned char * const buffer);
void data_broadcast_id_descriptor(const unsigned char * const buffer);
void transport_stream_descriptor(const unsigned char * const buffer);
void DSNG_descriptor(const unsigned char * const buffer);
void PDC_descriptor(const unsigned char * const buffer);
void AC3_descriptor(const unsigned char * const buffer);
void ancillary_data_descriptor(const unsigned char * const buffer);
void cell_list_descriptor(const unsigned char * const buffer);
void cell_frequency_link_descriptor(const unsigned char * const buffer);
void announcement_support_descriptor(const unsigned char * const buffer);

#endif /* __zapit_descriptors_h__ */
