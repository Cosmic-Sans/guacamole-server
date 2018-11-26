/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#ifndef __GUAC_RDPSND_SERVICE_H
#define __GUAC_RDPSND_SERVICE_H

#include "config.h"

//#include <freerdp/utils/svc_plugin.h>
#include <freerdp/types.h>
#include <freerdp/channels/rdpsnd.h>
#include <freerdp/client/rdpsnd.h>
#include <guacamole/client.h>

#ifdef ENABLE_WINPR
#include <winpr/stream.h>
#else
#include "compat/winpr-stream.h"
#endif

/**
 * The maximum number of PCM formats to accept during the initial RDPSND
 * handshake with the RDP server.
 */
#define GUAC_RDP_MAX_FORMATS 16

/**
 * Abstract representation of a PCM format, including the sample rate, number
 * of channels, and bits per sample.
 */
typedef struct guac_pcm_format {

    /**
     * The sample rate of this PCM format.
     */
    int rate;

    /**
     * The number off channels used by this PCM format. This will typically
     * be 1 or 2.
     */
    int channels;

    /**
     * The number of bits per sample within this PCM format. This should be
     * either 8 or 16.
     */
    int bps;

} guac_pcm_format;

/**
 * Structure representing the current state of the Guacamole RDPSND plugin for
 * FreeRDP.
 */
typedef struct guac_rdpsndPlugin {

    /**
     * The FreeRDP parts of this plugin. This absolutely MUST be first.
     * FreeRDP depends on accessing this structure as if it were an instance
     * of rdpSvcPlugin.
     */
	  rdpsndDevicePlugin device;

    /**
     * The Guacamole client associated with the guac_audio_stream that this
     * plugin should use to stream received audio packets.
     */
    guac_client* client;

    /**
     * The block number of the last SNDC_WAVE (WaveInfo) PDU received.
     */
    int waveinfo_block_number;

    /**
     * Whether the next PDU coming is a SNDWAVE (Wave) PDU. Wave PDUs do not
     * have headers, and are indicated by the receipt of a WaveInfo PDU.
     */
    int next_pdu_is_wave;

    /**
     * The wave data received within the last SNDC_WAVE (WaveInfo) PDU.
     */
    unsigned char initial_wave_data[4];

    /**
     * The size, in bytes, of the wave data in the coming Wave PDU, if any.
     * This does not include the initial wave data received within the last
     * SNDC_WAVE (WaveInfo) PDU, which is always the first four bytes of the
     * actual wave data block.
     */
    int incoming_wave_size;

    /**
     * The last received server timestamp.
     */
    int server_timestamp;

    /**
     * All formats agreed upon by server and client during the initial format
     * exchange. All of these formats will be PCM, which is the only format
     * guaranteed to be supported (based on the official RDP documentation).
     */
    guac_pcm_format formats[GUAC_RDP_MAX_FORMATS];

    /**
     * The total number of formats.
     */
    int format_count;

} guac_rdpsndPlugin;

typedef struct guac_rdpsndArgs {
    ADDIN_ARGV addin_argv;
    guac_client* guac_client;
} guac_rdpsndArgs;

UINT guac_rdpsnd_VirtualChannelEntry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints);

BOOL guac_rdpsnd_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
  UINT32 latency);

BOOL guac_rdpsnd_format_supported(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format);

UINT32 guac_rdpsnd_get_volume(rdpsndDevicePlugin* device);

BOOL guac_rdpsnd_set_volume(rdpsndDevicePlugin* device, UINT32 value);

void guac_rdpsnd_start(rdpsndDevicePlugin* device);

UINT guac_rdpsnd_play(rdpsndDevicePlugin* device, const BYTE* data, size_t size);

void guac_rdpsnd_close(rdpsndDevicePlugin* device);

void guac_rdpsnd_free(rdpsndDevicePlugin* device);

#endif

