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
#include "common/clipboard.h"
#include "common/iconv.h"
#include "rdp.h"
#include "rdp_cliprdr.h"

#include <freerdp/channels/channels.h>
#include <freerdp/freerdp.h>
#include <guacamole/client.h>
#include <winpr/wtypes.h>
#include <winpr/clipboard.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/cliprdr.h>

#include <stdlib.h>
#include <string.h>

static UINT guac_cliprdr_send_client_capabilities(CliprdrClientContext*
        cliprdr) {

	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	if (!cliprdr || !cliprdr->ClientCapabilities)
		return ERROR_INVALID_PARAMETER;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &
	                              (generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	return cliprdr->ClientCapabilities(cliprdr, &capabilities);

}

UINT guac_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr) {

	UINT rc = ERROR_INTERNAL_ERROR;
	CLIPRDR_FORMAT* formats;
	CLIPRDR_FORMAT_LIST formatList;

	formats = (CLIPRDR_FORMAT*) calloc(2, sizeof(CLIPRDR_FORMAT));

	if (!formats)
		goto fail;

	formats[0].formatId = CF_TEXT;
	formats[1].formatId = CF_UNICODETEXT;

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));
	formatList.msgFlags = CB_RESPONSE_OK;
	formatList.numFormats = 2;
	formatList.formats = formats;

	if (!cliprdr->ClientFormatList)
		goto fail;

	rc = cliprdr->ClientFormatList(cliprdr, &formatList);
fail:
	free(formats);
	return rc;

}

UINT guac_rdp_process_cb_monitor_ready(CliprdrClientContext* cliprdr,
                                     CLIPRDR_MONITOR_READY* monitor_ready) {
	UINT rc;

	if (!cliprdr || !monitor_ready)
		return ERROR_INVALID_PARAMETER;

	if ((rc = guac_cliprdr_send_client_capabilities(cliprdr)) != CHANNEL_RC_OK)
		return rc;

	if ((rc = guac_cliprdr_send_client_format_list(cliprdr)) != CHANNEL_RC_OK)
		return rc;

	return CHANNEL_RC_OK;

}

/**
 * Sends a clipboard data request for the given format.
 *
 * @param client
 *     The guac_client associated with the current RDP session.
 *
 * @param format
 *     The clipboard format to request. This format must be one of the
 *     documented values used by the CLIPRDR channel for clipboard format IDs.
 */
static UINT __guac_rdp_cb_request_format(guac_rdp_client* rdp_client,
	CliprdrClientContext* context, UINT32 format) {

	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = 0;
	formatDataRequest.dataLen = 0;
	formatDataRequest.requestedFormatId = format;

	/* Set to requested format */
	rdp_client->requested_clipboard_format = format;

	return context->ClientFormatDataRequest(context, &formatDataRequest);

}

UINT guac_rdp_process_cb_format_list(CliprdrClientContext* context,
	CLIPRDR_FORMAT_LIST* format_list) {

	guac_client* client = (guac_client*) context->custom;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    int formats = 0;

    /* Received notification of available data */

    int i;
    for (i=0; i<format_list->numFormats; i++) {

        /* If plain text available, request it */
        if (format_list->formats[i].formatId == CF_TEXT)
            formats |= GUAC_RDP_CLIPBOARD_FORMAT_CP1252;
        else if (format_list->formats[i].formatId == CF_UNICODETEXT)
            formats |= GUAC_RDP_CLIPBOARD_FORMAT_UTF16;

    }

    /* Prefer Unicode to plain text */
    if (formats & GUAC_RDP_CLIPBOARD_FORMAT_UTF16)
        return __guac_rdp_cb_request_format(rdp_client, context, CF_UNICODETEXT);

    /* Use plain text if Unicode unavailable */
    if (formats & GUAC_RDP_CLIPBOARD_FORMAT_CP1252)
        return __guac_rdp_cb_request_format(rdp_client, context, CF_TEXT);

    /* Ignore if no supported format available */
    guac_client_log(client, GUAC_LOG_INFO, "Ignoring unsupported clipboard data");
		return CHANNEL_RC_OK;

}

UINT guac_rdp_process_cb_format_list_response(CliprdrClientContext* cliprdr,
    CLIPRDR_FORMAT_LIST_RESPONSE* format_list_response) {

	if (!cliprdr || !format_list_response)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;

}

UINT guac_rdp_process_cb_data_request(CliprdrClientContext* context,
	CLIPRDR_FORMAT_DATA_REQUEST* format_data_request) {

		guac_client* client = (guac_client*) context->custom;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;

    guac_iconv_write* writer;
    const char* input = rdp_client->clipboard->buffer;
    char* output = malloc(GUAC_RDP_CLIPBOARD_MAX_LENGTH);

		CLIPRDR_FORMAT_DATA_RESPONSE data_response;

    /* Determine output encoding */
    switch (format_data_request->requestedFormatId) {

        case CF_TEXT:
            writer = GUAC_WRITE_CP1252;
            break;

        case CF_UNICODETEXT:
            writer = GUAC_WRITE_UTF16;
            break;

        default:
            guac_client_log(client, GUAC_LOG_ERROR, 
                    "Server requested unsupported clipboard data type");
            free(output);
            return 1;

    }

    /* Set data and size */
		data_response.msgFlags = CB_RESPONSE_OK;
    data_response.requestedFormatData = (BYTE*) output;
    guac_iconv(GUAC_READ_UTF8, &input, rdp_client->clipboard->length,
               writer, &output, GUAC_RDP_CLIPBOARD_MAX_LENGTH);
    data_response.dataLen = ((BYTE*) output) - data_response.requestedFormatData;

    /* Send response */
		UINT rc = context->ClientFormatDataResponse(context, &data_response);
		free(output);
		return rc;

}

UINT guac_rdp_process_cb_data_response(CliprdrClientContext* context,
	CLIPRDR_FORMAT_DATA_RESPONSE* format_data_response) {

		guac_client* client = (guac_client*) context->custom;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    char received_data[GUAC_RDP_CLIPBOARD_MAX_LENGTH];

    guac_iconv_read* reader;
    const char* input = (char*) format_data_response->requestedFormatData;
    char* output = received_data;

    /* Find correct source encoding */
    switch (rdp_client->requested_clipboard_format) {

        /* Non-Unicode */
        case CF_TEXT:
            reader = GUAC_READ_CP1252;
            break;

        /* Unicode (UTF-16) */
        case CF_UNICODETEXT:
            reader = GUAC_READ_UTF16;
            break;

        default:
            guac_client_log(client, GUAC_LOG_ERROR, "Requested clipboard data in "
                    "unsupported format %i",
                    rdp_client->requested_clipboard_format);
            return 1;

    }

    /* Convert send clipboard data */
    if (guac_iconv(reader, &input, format_data_response->dataLen,
            GUAC_WRITE_UTF8, &output, sizeof(received_data))) {

        int length = strnlen(received_data, sizeof(received_data));
        guac_common_clipboard_reset(rdp_client->clipboard, "text/plain");
        guac_common_clipboard_append(rdp_client->clipboard, received_data, length);
        guac_common_clipboard_send(rdp_client->clipboard, client);

    }

		return CHANNEL_RC_OK;
}

