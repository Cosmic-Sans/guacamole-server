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

#include "client.h"
#include "common/cursor.h"
#include "common/display.h"
#include "error.h"
#include "keyboard.h"
#include "rdp.h"
#include "rdp_bitmap.h"
#include "rdp_cliprdr.h"
#include "rdp_gdi.h"
#include "rdp_glyph.h"
#include "rdp_pointer.h"
#include "rdp_stream.h"
#include "guac_rdpsnd/rdpsnd_service.h"

#ifdef ENABLE_COMMON_SSH
#include "common-ssh/sftp.h"
#include "common-ssh/ssh.h"
#include "common-ssh/user.h"
#endif

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <freerdp/cache/bitmap.h>
#include <freerdp/cache/brush.h>
#include <freerdp/cache/glyph.h>
#include <freerdp/cache/offscreen.h>
#include <freerdp/cache/palette.h>
#include <freerdp/cache/pointer.h>
#include <freerdp/channels/channels.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#ifdef HAVE_FREERDP_REGISTER_ADDIN_PROVIDER
#include <freerdp/client/channels.h>
#endif
#include <guacamole/audio.h>
#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>

#include <freerdp/client/cliprdr.h>

#ifdef HAVE_FREERDP_CLIENT_DISP_H
#include <freerdp/client/disp.h>
#endif
#include <freerdp/event.h>

#ifdef LEGACY_FREERDP
#include "compat/rail.h"
#else
#include <freerdp/rail.h>
#endif

#include <winpr/wtypes.h>

#ifdef HAVE_FREERDP_ADDIN_H
#include <freerdp/addin.h>
#endif

#ifdef HAVE_FREERDP_CLIENT_CHANNELS_H
#include <freerdp/client/channels.h>
#endif

#ifdef HAVE_FREERDP_VERSION_H
#include <freerdp/version.h>
#endif

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Callback invoked by FreeRDP for data received along a channel. This is the
 * most recent version of the callback and uses a 16-bit unsigned integer for
 * the channel ID, as well as different type naming for the datatype of the
 * data itself. This function does nothing more than invoke
 * freerdp_channels_data() with the given arguments. The prototypes of these
 * functions are compatible in 1.2 and later, but not necessarily prior to
 * that, hence the conditional compilation of differing prototypes.
 *
 * Beware that the official purpose of these parameters is an undocumented
 * mystery. The meanings below are derived from looking at how the function is
 * used within FreeRDP.
 *
 * @param rdp_inst
 *     The RDP client instance associated with the channel receiving the data.
 *
 * @param channel_id
 *     The integer ID of the channel that received the data.
 *
 * @param data
 *     A buffer containing the received data.
 *
 * @param size
 *     The number of bytes received and contained in the given buffer (the
 *     number of bytes received within the PDU that resulted in this function
 *     being inboked).
 *
 * @param flags
 *     Channel control flags, as defined by the CHANNEL_PDU_HEADER in the RDP
 *     specification.
 *
 * @param total_size
 *     The total length of the chanel data being received, which may span
 *     multiple PDUs (see the "length" field of CHANNEL_PDU_HEADER).
 *
 * @return
 *     Zero if the received channel data was successfully handled, non-zero
 *     otherwise. Note that this return value is discarded in practice.
 */
static int __guac_receive_channel_data(freerdp* rdp_inst, UINT16 channel_id,
                                   BYTE* data, int size, int flags, int total_size) {

    return freerdp_channels_data(rdp_inst, channel_id,
            data, size, flags, total_size);

}

/**
 * Called whenever a channel connects via the PubSub event system within
 * FreeRDP.
 *
 * @param context
 *     The rdpContext associated with the active RDP session.
 *
 * @param e
 *     Event-specific arguments, mainly the name of the channel, and a
 *     reference to the associated plugin loaded for that channel by FreeRDP.
 */
static void guac_rdp_channel_connected(rdpContext* context,
        ChannelConnectedEventArgs* e) {

    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_settings* settings = rdp_client->settings;

    if (settings->resize_method == GUAC_RESIZE_DISPLAY_UPDATE) {
#ifdef HAVE_RDPSETTINGS_SUPPORTDISPLAYCONTROL
        /* Store reference to the display update plugin once it's connected */
        if (strcmp(e->name, DISP_DVC_CHANNEL_NAME) == 0) {

            DispClientContext* disp = (DispClientContext*) e->pInterface;

            /* Init module with current display size */
            guac_rdp_disp_set_size(rdp_client->disp, rdp_client->settings,
                    context->instance, guac_rdp_get_width(context->instance),
                    guac_rdp_get_height(context->instance));

            /* Store connected channel */
            guac_rdp_disp_connect(rdp_client->disp, disp);
            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Display update channel connected.");

        }
#endif
    }
		if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
		{
			CliprdrClientContext* cliprdr = (CliprdrClientContext*) e->pInterface;
			rdp_client->cliprdr = cliprdr;
			cliprdr->custom = (void*) client;
			cliprdr->MonitorReady = guac_rdp_process_cb_monitor_ready;
			cliprdr->ServerFormatList = guac_rdp_process_cb_format_list;
			cliprdr->ServerFormatListResponse = guac_rdp_process_cb_format_list_response;
			cliprdr->ServerFormatDataRequest = guac_rdp_process_cb_data_request;
			cliprdr->ServerFormatDataResponse = guac_rdp_process_cb_data_response;
		}

}
static void guac_rdp_channel_disconnected(
	rdpContext* context, ChannelDisconnectedEventArgs* e) {

		if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0) {
			
			CliprdrClientContext* cliprdr = (CliprdrClientContext*) e->pInterface;
			ClipboardDestroy(cliprdr->custom);
		}

}

static BOOL rdp_freerdp_client_load_static_channel_addin(rdpChannels* channels,
        rdpSettings* settings, char* name, void* data)
{
	PVIRTUALCHANNELENTRY entry = NULL;
	PVIRTUALCHANNELENTRYEX entryEx = NULL;
	entryEx = (PVIRTUALCHANNELENTRYEX) freerdp_load_channel_addin_entry(name, NULL, NULL,
	          FREERDP_ADDIN_CHANNEL_STATIC | FREERDP_ADDIN_CHANNEL_ENTRYEX);

	if (!entryEx)
		entry = freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

	if (entryEx)
	{
		if (freerdp_channels_client_load_ex(channels, settings, entryEx, data) == 0)
		{
			//WLog_INFO(TAG, "loading channelEx %s", name);
			return TRUE;
		}
	}
	else if (entry)
	{
		if (freerdp_channels_client_load(channels, settings, entry, data) == 0)
		{
			//WLog_INFO(TAG, "loading channel %s", name);
			return TRUE;
		}
	}

	return FALSE;
}

PVIRTUALCHANNELENTRY guac_channels_load_static_addin_entry(LPCSTR pszName,
                                                           LPSTR pszSubsystem,
                                                           LPSTR pszType,
                                                           DWORD dwFlags)
{
  if (strcmp(pszName, "rdpsnd") == 0
      && pszSubsystem != NULL && strcmp(pszSubsystem, "guac_snd") == 0) {
    return guac_rdpsnd_VirtualChannelEntry;
  }
  return freerdp_channels_load_static_addin_entry(pszName, pszSubsystem, pszType, dwFlags);
}

BOOL rdp_freerdp_pre_connect(freerdp* instance) {

    rdpContext* context = instance->context;
    rdpChannels* channels = context->channels;

    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_settings* settings = rdp_client->settings;

    freerdp_register_addin_provider(guac_channels_load_static_addin_entry, 0);

    /* Subscribe to and handle channel connected events */
    PubSub_SubscribeChannelConnected(context->pubSub,
            (pChannelConnectedEventHandler) guac_rdp_channel_connected);

    PubSub_SubscribeChannelDisconnected(context->pubSub,
            (pChannelDisconnectedEventHandler) guac_rdp_channel_disconnected);

#ifdef HAVE_FREERDP_DISPLAY_UPDATE_SUPPORT
    /* Load "disp" plugin for display update */
    if (settings->resize_method == GUAC_RESIZE_DISPLAY_UPDATE)
        guac_rdp_disp_load_plugin(instance->context, dvc_list);
#endif

    /* Load clipboard plugin */
    if (freerdp_channels_load_plugin(channels, instance->settings,
                "cliprdr", NULL))
        guac_client_log(client, GUAC_LOG_WARNING,
                "Failed to load cliprdr plugin. Clipboard will not work.");

    /* If RDPSND/RDPDR required, load them */
    if (settings->printing_enabled
        || settings->drive_enabled
        || settings->audio_enabled) {

        /* Load RDPDR plugin */
        if (!rdp_freerdp_client_load_static_channel_addin(
              channels, instance->settings, "rdpdr", instance->settings))
          return FALSE;

        /* Load RDPSND plugin */
        if (!rdp_freerdp_client_load_static_channel_addin(channels,
                instance->settings, "rdpsnd",
                &((rdp_freerdp_context*) context)->rdpsnd_args))
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to load guacsnd alongside guacdr plugin. Sound "
                    "will not work. Drive redirection and printing MAY not "
                    "work.");
    }

    /* Load RAIL plugin if RemoteApp in use */
    if (settings->remote_app != NULL) {

#ifdef LEGACY_FREERDP
        RDP_PLUGIN_DATA* plugin_data = malloc(sizeof(RDP_PLUGIN_DATA) * 2);

        plugin_data[0].size = sizeof(RDP_PLUGIN_DATA);
        plugin_data[0].data[0] = settings->remote_app;
        plugin_data[0].data[1] = settings->remote_app_dir;
        plugin_data[0].data[2] = settings->remote_app_args;
        plugin_data[0].data[3] = NULL;

        plugin_data[1].size = 0;

        /* Attempt to load rail */
        if (freerdp_channels_load_plugin(channels, instance->settings,
                    "rail", plugin_data))
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to load rail plugin. RemoteApp will not work.");
#else
        /* Attempt to load rail */
        if (freerdp_channels_load_plugin(channels, instance->settings,
                    "rail", instance->settings))
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to load rail plugin. RemoteApp will not work.");
#endif

    }

    return TRUE;

}

/**
 * Callback invoked by FreeRDP just after the connection is established with
 * the RDP server.
 *
 * @param instance
 *     The FreeRDP instance that has just connected.
 *
 * @return
 *     TRUE if successful, FALSE if an error occurs.
 */
static BOOL rdp_freerdp_post_connect(freerdp* instance) {

    rdpContext* context = instance->context;
		rdpGraphics* graphics = context->graphics;
    rdpBitmap bitmap;
    rdpGlyph glyph;
    rdpPointer pointer;
    rdpPrimaryUpdate* primary;

		if (!gdi_init(instance, PIXEL_FORMAT_XRGB32))
			return FALSE;

    /* Init color conversion structure */
    ((rdp_freerdp_context*) context)->clrconv = calloc(1, sizeof(rdpPalette));

    /* Init FreeRDP cache */
    instance->context->cache = cache_new(instance->settings);

    /* Set up bitmap handling */
    bitmap = *graphics->Bitmap_Prototype;
    bitmap.New = guac_rdp_bitmap_new;
    bitmap.Free = guac_rdp_bitmap_free;
    bitmap.Paint = guac_rdp_bitmap_paint;
//    bitmap.Decompress = guac_rdp_bitmap_decompress;
    bitmap.SetSurface = guac_rdp_bitmap_setsurface;
    graphics_register_bitmap(context->graphics, &bitmap);

    /* Set up glyph handling */
    glyph = *graphics->Glyph_Prototype;
    glyph.size = sizeof(guac_rdp_glyph);
    glyph.New = guac_rdp_glyph_new;
    glyph.Free = guac_rdp_glyph_free;
    glyph.Draw = guac_rdp_glyph_draw;
    glyph.BeginDraw = guac_rdp_glyph_begindraw;
    glyph.EndDraw = guac_rdp_glyph_enddraw;
    graphics_register_glyph(context->graphics, &glyph);

    /* Set up pointer handling */
		ZeroMemory(&pointer, sizeof(rdpPointer));
    pointer.size = sizeof(guac_rdp_pointer);
    pointer.New = guac_rdp_pointer_new;
    pointer.Free = guac_rdp_pointer_free;
    pointer.Set = guac_rdp_pointer_set;
#ifdef HAVE_RDPPOINTER_SETNULL
    pointer->SetNull = guac_rdp_pointer_set_null;
#endif
#ifdef HAVE_RDPPOINTER_SETDEFAULT
    pointer->SetDefault = guac_rdp_pointer_set_default;
#endif
    graphics_register_pointer(context->graphics, &pointer);

    /* Set up GDI */
    instance->update->DesktopResize = guac_rdp_gdi_desktop_resize;
    instance->update->EndPaint = guac_rdp_gdi_end_paint;
    instance->update->Palette = guac_rdp_gdi_palette_update;
    instance->update->SetBounds = guac_rdp_gdi_set_bounds;

    primary = instance->update->primary;
    primary->DstBlt = guac_rdp_gdi_dstblt;
    primary->PatBlt = guac_rdp_gdi_patblt;
    primary->ScrBlt = guac_rdp_gdi_scrblt;
    primary->MemBlt = guac_rdp_gdi_memblt;
    primary->OpaqueRect = guac_rdp_gdi_opaquerect;

    pointer_cache_register_callbacks(instance->update);
    glyph_cache_register_callbacks(instance->update);
    brush_cache_register_callbacks(instance->update);
    bitmap_cache_register_callbacks(instance->update);
    offscreen_cache_register_callbacks(instance->update);
    palette_cache_register_callbacks(instance->update);

		return TRUE;

}

/**
 * Callback invoked by FreeRDP when authentication is required but a username
 * and password has not already been given. In the case of Guacamole, this
 * function always succeeds but does not populate the usename or password. The
 * username/password must be given within the connection parameters.
 *
 * @param instance
 *     The FreeRDP instance associated with the RDP session requesting
 *     credentials.
 *
 * @param username
 *     Pointer to a string which will receive the user's username.
 *
 * @param password
 *     Pointer to a string which will receive the user's password.
 *
 * @param domain
 *     Pointer to a string which will receive the domain associated with the
 *     user's account.
 *
 * @return
 *     Always TRUE.
 */
static BOOL rdp_freerdp_authenticate(freerdp* instance, char** username,
        char** password, char** domain) {

    rdpContext* context = instance->context;
    guac_client* client = ((rdp_freerdp_context*) context)->client;

    /* Warn if connection is likely to fail due to lack of credentials */
    guac_client_log(client, GUAC_LOG_INFO,
            "Authentication requested but username or password not given");
    return TRUE;

}

/**
 * Callback invoked by FreeRDP when the SSL/TLS certificate of the RDP server
 * needs to be verified. If this ever happens, this function implementation
 * will always fail unless the connection has been configured to ignore
 * certificate validity.
 *
 * @param instance
 *     The FreeRDP instance associated with the RDP session whose SSL/TLS
 *     certificate needs to be verified.
 *
 * @param subject
 *     The subject to whom the certificate was issued.
 *
 * @param issuer
 *     The authority that issued the certificate,
 *
 * @param fingerprint
 *     The cryptographic fingerprint of the certificate.
 *
 * @return
 *     TRUE if the certificate passes verification, FALSE otherwise.
 */
static DWORD rdp_freerdp_verify_certificate(freerdp* instance,
                                    const char* common_name,
                                    const char* subject,
                                    const char* issuer,
                                    const char* fingerprint,
                                    BOOL host_mismatch) {

    rdpContext* context = instance->context;
    guac_client* client = ((rdp_freerdp_context*) context)->client;
    guac_rdp_client* rdp_client =
        (guac_rdp_client*) client->data;

    /* Bypass validation if ignore_certificate given */
    if (rdp_client->settings->ignore_certificate) {
        guac_client_log(client, GUAC_LOG_INFO, "Certificate validation bypassed");
        return TRUE;
    }

    guac_client_log(client, GUAC_LOG_INFO, "Certificate validation failed");
    return FALSE;

}

static ADDIN_ARGV rdpsnd_args = { 2, (char*[]){ "rdpsnd", "sys:guac_snd" } };

/**
 * Callback invoked by FreeRDP after a new rdpContext has been allocated and
 * associated with the current FreeRDP instance.
 *
 * @param instance
 *     The FreeRDP instance whose context has just been allocated.
 *
 * @param context
 *     The newly-allocated FreeRDP context.
 */
static BOOL rdp_freerdp_context_new(freerdp* instance, rdpContext* context) {
//    context->channels = freerdp_channels_new();
    ((rdp_freerdp_context*)context)->rdpsnd_args.addin_argv = rdpsnd_args;

		return TRUE;
}

/**
 * Callback invoked by FreeRDP when the rdpContext is being freed.
 *
 * @param instance
 *     The FreeRDP instance whose context is being freed.
 *
 * @param context
 *     The FreeRDP context being freed.
 */
static void rdp_freerdp_context_free(freerdp* instance, rdpContext* context) {
    /* EMPTY */
}

/**
 * Waits for messages from the RDP server for the given number of milliseconds.
 *
 * @param client
 *     The client associated with the current RDP session.
 *
 * @param timeout_msecs
 *     The maximum amount of time to wait, in milliseconds.
 *
 * @return
 *     A positive value if messages are ready, zero if the specified timeout
 *     period elapsed, or a negative value if an error occurs.
 */
static int rdp_guac_client_wait_for_messages(guac_client* client,
        int timeout_msecs) {

    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    freerdp* rdp_inst = rdp_client->rdp_inst;
		HANDLE handles[GUAC_RDP_MAX_FILE_DESCRIPTORS];
			DWORD nCount = freerdp_get_event_handles(rdp_inst->context, handles, sizeof(handles));

			if (nCount == 0)
			{
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR,
                "freerdp_get_event_handles failed");
				return -1;
			}

		DWORD status = WaitForMultipleObjects(nCount, handles, FALSE, timeout_msecs);

		if (status == WAIT_FAILED)
		{
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR,
			"%s: WaitForMultipleObjects failed with %"PRIu32"", __FUNCTION__,
			         status);
				return -1;
		}

			if (!freerdp_check_event_handles(rdp_inst->context))
			{
//				if (wf_auto_reconnect(instance))
//					continue;

        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR,
					"Failed to check FreeRDP file descriptor");
				return -1;
			}

    return 1;

}

/**
 * Connects to an RDP server as described by the guac_rdp_settings structure
 * associated with the given client, allocating and freeing all objects
 * directly related to the RDP connection. It is expected that all objects
 * which are independent of FreeRDP's state (the clipboard, display update
 * management, etc.) will already be allocated and associated with the
 * guac_rdp_client associated with the given guac_client. This function blocks
 * for the duration of the RDP session, returning only after the session has
 * completely disconnected.
 *
 * @param client
 *     The guac_client associated with the RDP settings describing the
 *     connection that should be established.
 *
 * @return
 *     Zero if the connection successfully terminated and a reconnect is
 *     desired, non-zero if an error occurs or the connection was disconnected
 *     and a reconnect is NOT desired.
 */
static int guac_rdp_handle_connection(guac_client* client) {

    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_settings* settings = rdp_client->settings;

    /* Create display */
    rdp_client->display = guac_common_display_alloc(client,
            rdp_client->settings->width,
            rdp_client->settings->height);

    rdp_client->current_surface = rdp_client->display->default_surface;

    rdp_client->requested_clipboard_format = CF_TEXT;
    rdp_client->available_svc = guac_common_list_alloc();

#ifdef HAVE_FREERDP_CHANNELS_GLOBAL_INIT
    freerdp_channels_global_init();
#endif

    /* Init client */
    freerdp* rdp_inst = freerdp_new();
    rdp_inst->PreConnect = rdp_freerdp_pre_connect;
    rdp_inst->PostConnect = rdp_freerdp_post_connect;
    rdp_inst->Authenticate = rdp_freerdp_authenticate;
    rdp_inst->VerifyCertificate = rdp_freerdp_verify_certificate;
    rdp_inst->ReceiveChannelData = __guac_receive_channel_data;

    /* Allocate FreeRDP context */
#ifdef LEGACY_FREERDP
    rdp_inst->context_size = sizeof(rdp_freerdp_context);
#else
    rdp_inst->ContextSize = sizeof(rdp_freerdp_context);
#endif
    rdp_inst->ContextNew  = (pContextNew) rdp_freerdp_context_new;
    rdp_inst->ContextFree = (pContextFree) rdp_freerdp_context_free;

    freerdp_context_new(rdp_inst);
    ((rdp_freerdp_context*) rdp_inst->context)->client = client;
    ((rdp_freerdp_context*) rdp_inst->context)->rdpsnd_args.guac_client = client;

    /* Load keymap into client */
    rdp_client->keyboard = guac_rdp_keyboard_alloc(client,
            settings->server_layout);

    /* Set default pointer */
    guac_common_cursor_set_pointer(rdp_client->display->cursor);

    /* Push desired settings to FreeRDP */
    guac_rdp_push_settings(settings, rdp_inst);

    /* Connect to RDP server */
    if (!freerdp_connect(rdp_inst)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_NOT_FOUND,
                "Error connecting to RDP server");
        return 1;
    }

    /* Connection complete */
    rdp_client->rdp_inst = rdp_inst;
    rdpChannels* channels = rdp_inst->context->channels;

    guac_timestamp last_frame_end = guac_timestamp_current();

    /* Handle messages from RDP server while client is running */
    while (client->state == GUAC_CLIENT_RUNNING) {

        /* Wait for data and construct a reasonable frame */
        int wait_result = rdp_guac_client_wait_for_messages(client,
                GUAC_RDP_FRAME_START_TIMEOUT);
        if (wait_result > 0) {

            int processing_lag = guac_client_get_processing_lag(client);
            guac_timestamp frame_start = guac_timestamp_current();

            /* Read server messages until frame is built */
            do {

                guac_timestamp frame_end;
                int frame_remaining;

                pthread_mutex_lock(&(rdp_client->rdp_lock));

                /* Handle RDP disconnect */
                if (freerdp_shall_disconnect(rdp_inst)) {
                    guac_rdp_client_abort(client);
                    pthread_mutex_unlock(&(rdp_client->rdp_lock));
                    return 1;
                }

                pthread_mutex_unlock(&(rdp_client->rdp_lock));

                /* Calculate time remaining in frame */
                frame_end = guac_timestamp_current();
                frame_remaining = frame_start + GUAC_RDP_FRAME_DURATION
                                - frame_end;

                /* Calculate time that client needs to catch up */
                int time_elapsed = frame_end - last_frame_end;
                int required_wait = processing_lag - time_elapsed;

                /* Increase the duration of this frame if client is lagging */
                if (required_wait > GUAC_RDP_FRAME_TIMEOUT)
                    wait_result = rdp_guac_client_wait_for_messages(client,
                            required_wait);

                /* Wait again if frame remaining */
                else if (frame_remaining > 0)
                    wait_result = rdp_guac_client_wait_for_messages(client,
                            GUAC_RDP_FRAME_TIMEOUT);
                else
                    break;

            } while (wait_result > 0);

            /* Record end of frame, excluding server-side rendering time (we
             * assume server-side rendering time will be consistent between any
             * two subsequent frames, and that this time should thus be
             * excluded from the required wait period of the next frame). */
            last_frame_end = frame_start;

        }

        /* Test whether the RDP server is closing the connection */
        pthread_mutex_lock(&(rdp_client->rdp_lock));
        int connection_closing = freerdp_shall_disconnect(rdp_inst);
        pthread_mutex_unlock(&(rdp_client->rdp_lock));

        /* Close connection cleanly if server is disconnecting */
        if (connection_closing)
            guac_rdp_client_abort(client);

        /* If a low-level connection error occurred, fail */
        else if (wait_result < 0)
            guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_UNAVAILABLE,
                    "Connection closed.");

        /* Flush frame only if successful */
        else {
            guac_common_display_flush(rdp_client->display);
            guac_client_end_frame(client);
            guac_socket_flush(client->socket);
        }

    }

    pthread_mutex_lock(&(rdp_client->rdp_lock));

    freerdp_disconnect(rdp_inst);

    /* Clean up RDP client context */
//    freerdp_clrconv_free(((rdp_freerdp_context*) rdp_inst->context)->clrconv);
    cache_free(rdp_inst->context->cache);
    freerdp_context_free(rdp_inst);

    /* Clean up RDP client */
    freerdp_free(rdp_inst);
    rdp_client->rdp_inst = NULL;

    /* Free SVC list */
    guac_common_list_free(rdp_client->available_svc);

    /* Free RDP keyboard state */
    guac_rdp_keyboard_free(rdp_client->keyboard);

    /* Free display */
    guac_common_display_free(rdp_client->display);

    pthread_mutex_unlock(&(rdp_client->rdp_lock));

    /* Client is now disconnected */
    guac_client_log(client, GUAC_LOG_INFO, "Internal RDP client disconnected");

    return 0;

}

void* guac_rdp_client_thread(void* data) {

    guac_client* client = (guac_client*) data;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_settings* settings = rdp_client->settings;

    /* If audio enabled, choose an encoder */
    if (settings->audio_enabled) {

        rdp_client->audio = guac_audio_stream_alloc(client, NULL,
                GUAC_RDP_AUDIO_RATE,
                GUAC_RDP_AUDIO_CHANNELS,
                GUAC_RDP_AUDIO_BPS);

        /* Warn if no audio encoding is available */
        if (rdp_client->audio == NULL)
            guac_client_log(client, GUAC_LOG_INFO,
                    "No available audio encoding. Sound disabled.");

    } /* end if audio enabled */

#ifdef ENABLE_COMMON_SSH
    /* Connect via SSH if SFTP is enabled */
    if (settings->enable_sftp) {

        /* Abort if username is missing */
        if (settings->sftp_username == NULL) {
            guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR,
                    "A username or SFTP-specific username is required if "
                    "SFTP is enabled.");
            return NULL;
        }

        guac_client_log(client, GUAC_LOG_DEBUG,
                "Connecting via SSH for SFTP filesystem access.");

        rdp_client->sftp_user =
            guac_common_ssh_create_user(settings->sftp_username);

        /* Import private key, if given */
        if (settings->sftp_private_key != NULL) {

            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Authenticating with private key.");

            /* Abort if private key cannot be read */
            if (guac_common_ssh_user_import_key(rdp_client->sftp_user,
                        settings->sftp_private_key,
                        settings->sftp_passphrase)) {
                guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR,
                        "Private key unreadable.");
                return NULL;
            }

        }

        /* Otherwise, use specified password */
        else {

            guac_client_log(client, GUAC_LOG_DEBUG,
                    "Authenticating with password.");

            guac_common_ssh_user_set_password(rdp_client->sftp_user,
                    settings->sftp_password);

        }

        /* Attempt SSH connection */
        rdp_client->sftp_session =
            guac_common_ssh_create_session(client, settings->sftp_hostname,
                    settings->sftp_port, rdp_client->sftp_user, settings->sftp_server_alive_interval,
                    settings->sftp_host_key);

        /* Fail if SSH connection does not succeed */
        if (rdp_client->sftp_session == NULL) {
            /* Already aborted within guac_common_ssh_create_session() */
            return NULL;
        }

        /* Load and expose filesystem */
        rdp_client->sftp_filesystem =
            guac_common_ssh_create_sftp_filesystem(rdp_client->sftp_session,
                    settings->sftp_root_directory, NULL);

        /* Expose filesystem to connection owner */
        guac_client_for_owner(client,
                guac_common_ssh_expose_sftp_filesystem,
                rdp_client->sftp_filesystem);

        /* Abort if SFTP connection fails */
        if (rdp_client->sftp_filesystem == NULL) {
            guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_UNAVAILABLE,
                    "SFTP connection failed.");
            return NULL;
        }

        guac_client_log(client, GUAC_LOG_DEBUG,
                "SFTP connection succeeded.");

    }
#endif

#ifdef _WIN32
		WSADATA wsa_data;
		/* Not sure why FreeRDP doesn't call this function for us,
		 * since it already abstracts all socket I/O. */
		WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

    /* Continue handling connections until error or client disconnect */
    while (client->state == GUAC_CLIENT_RUNNING) {
        if (guac_rdp_handle_connection(client))
            break;
    }

    return NULL;

}

