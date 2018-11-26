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

#include "config.h"

#include "rdpsnd_service.h"
#include "rdpsnd_messages.h"
#include "rdp.h"

#include <stdlib.h>
#include <string.h>

#include <freerdp/constants.h>
#include <guacamole/client.h>
#include "audio-fntypes.h"

#ifdef ENABLE_WINPR
#include <winpr/stream.h>
#else
#include "compat/winpr-stream.h"
#endif

/**
 * Entry point for RDPSND virtual channel.
 */
UINT guac_rdpsnd_VirtualChannelEntry(PFREERDP_RDPSND_DEVICE_ENTRY_POINTS pEntryPoints) {

    /* Allocate plugin */
    guac_rdpsndPlugin* rdpsnd =
        (guac_rdpsndPlugin*) calloc(1, sizeof(guac_rdpsndPlugin));

    /* Set callbacks */
    rdpsnd->device.Open = guac_rdpsnd_open;
    rdpsnd->device.FormatSupported = guac_rdpsnd_format_supported;
    rdpsnd->device.GetVolume = guac_rdpsnd_get_volume;
    rdpsnd->device.SetVolume = guac_rdpsnd_set_volume;
    rdpsnd->device.Start = guac_rdpsnd_start;
    rdpsnd->device.Play = guac_rdpsnd_play;
    rdpsnd->device.Close = guac_rdpsnd_close;
    rdpsnd->device.Free = guac_rdpsnd_free;
    rdpsnd->client = ((guac_rdpsndArgs*) pEntryPoints->args)->guac_client;

    pEntryPoints->pRegisterRdpsndDevice(pEntryPoints->rdpsnd, (rdpsndDevicePlugin*) rdpsnd);
    return CHANNEL_RC_OK;

}

/* 
 * Callbacks
 */

BOOL guac_rdpsnd_open(rdpsndDevicePlugin* device, const AUDIO_FORMAT* format,
  UINT32 latency)
{
  
}

BOOL guac_rdpsnd_format_supported(rdpsndDevicePlugin* plugin, const AUDIO_FORMAT* format)
{
    guac_rdpsndPlugin* rdpsnd = (guac_rdpsndPlugin*) plugin;
    guac_client* client = rdpsnd->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_audio_stream* audio = rdp_client->audio;

    const UINT32 rate     = format->nSamplesPerSec;
    const UINT16 channels = format->nChannels;
    const UINT16 bps      = format->wBitsPerSample;

    /* If PCM, accept */
    if (format->wFormatTag == WAVE_FORMAT_PCM) {

        /* If can fit another format, accept it */
        if (rdpsnd->format_count < GUAC_RDP_MAX_FORMATS) {

            /* Add channel */
            int current = rdpsnd->format_count++;
            rdpsnd->formats[current].rate = rate;
            rdpsnd->formats[current].channels = channels;
            rdpsnd->formats[current].bps = bps;

            /* Log format */
            guac_client_log(client, GUAC_LOG_INFO,
                    "Accepted format: %i-bit PCM with %i channels at "
                    "%i Hz",
                    bps, channels, rate);

            /* Ensure audio stream is configured to use accepted
             * format */
            guac_audio_stream_reset(audio, NULL, rate, channels, bps);

        }

        /* Otherwise, log that we dropped one */
        else
            guac_client_log(client, GUAC_LOG_INFO,
                    "Dropped valid format: %i-bit PCM with %i "
                    "channels at %i Hz",
                    bps, channels, rate);

    }
  return TRUE;
}

UINT32 guac_rdpsnd_get_volume(rdpsndDevicePlugin* device)
{
	DWORD dwVolume;
	UINT16 dwVolumeLeft;
	UINT16 dwVolumeRight;
	dwVolumeLeft = ((50 * 0xFFFF) / 100); /* 50% */
	dwVolumeRight = ((50 * 0xFFFF) / 100); /* 50% */
	dwVolume = (dwVolumeLeft << 16) | dwVolumeRight;
  return dwVolume;
}

BOOL guac_rdpsnd_set_volume(rdpsndDevicePlugin* device, UINT32 value)
{
  
}

void guac_rdpsnd_start(rdpsndDevicePlugin* device)
{
  
}

UINT guac_rdpsnd_play(rdpsndDevicePlugin* plugin, const BYTE* data, size_t size)
{
    guac_client* client = ((guac_rdpsndPlugin*) plugin)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;

    /* Get audio stream from client data */
    guac_audio_stream* audio = rdp_client->audio;

    guac_audio_stream_write_pcm(audio, data, size);
    guac_audio_stream_flush(audio);
}

void guac_rdpsnd_close(rdpsndDevicePlugin* device)
{
    /* Do nothing */
}

void guac_rdpsnd_free(rdpsndDevicePlugin* device)
{
    free(device);
}
