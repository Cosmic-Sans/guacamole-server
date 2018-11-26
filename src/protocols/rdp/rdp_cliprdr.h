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


#ifndef __GUAC_RDP_RDP_CLIPRDR_H
#define __GUAC_RDP_RDP_CLIPRDR_H

#include "config.h"

#include <guacamole/client.h>

#ifdef ENABLE_WINPR
#include <winpr/stream.h>
#else
#include "compat/winpr-stream.h"
#endif

#include <freerdp/client/cliprdr.h>

/**
 * Clipboard format for text encoded in Windows CP1252.
 */
#define GUAC_RDP_CLIPBOARD_FORMAT_CP1252 1

/**
 * Clipboard format for text encoded in UTF-16.
 */
#define GUAC_RDP_CLIPBOARD_FORMAT_UTF16 2

UINT guac_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr);

/**
 * Handles the given CLIPRDR event, which MUST be a Monitor Ready event. It
 * is the responsibility of this function to respond to the Monitor Ready
 * event with a list of supported clipboard formats.
 *
 * @param client
 *     The guac_client associated with the current RDP session.
 *
 * @param event
 *     The received CLIPRDR message, which must be a Monitor Ready event.
 */
//void guac_rdp_process_cb_monitor_ready(guac_client* client, wMessage* event);
UINT guac_rdp_process_cb_monitor_ready(CliprdrClientContext* context,
	CLIPRDR_MONITOR_READY* monitor_ready);

/**
 * Handles the given CLIPRDR event, which MUST be a Format List event. It
 * is the responsibility of this function to respond to the Format List 
 * event with a request for clipboard data in one of the enumerated formats.
 * This event is fired whenever remote clipboard data is available.
 *
 * @param client
 *     The guac_client associated with the current RDP session.
 *
 * @param event
 *     The received CLIPRDR message, which must be a Format List event.
 */
UINT guac_rdp_process_cb_format_list(CliprdrClientContext* context,
	CLIPRDR_FORMAT_LIST* format_list);

UINT guac_rdp_process_cb_format_list_response(CliprdrClientContext* cliprdr,
	CLIPRDR_FORMAT_LIST_RESPONSE* format_list_response);

/**
 * Handles the given CLIPRDR event, which MUST be a Data Request event. It
 * is the responsibility of this function to respond to the Data Request
 * event with a data response containing the current clipoard contents.
 *
 * @param client
 *     The guac_client associated with the current RDP session.
 *
 * @param event
 *     The received CLIPRDR message, which must be a Data Request event.
 */
UINT guac_rdp_process_cb_data_request(CliprdrClientContext* context,
	CLIPRDR_FORMAT_DATA_REQUEST* format_data_request);

/**
 * Handles the given CLIPRDR event, which MUST be a Data Response event. It
 * is the responsibility of this function to read and forward the received
 * clipboard data to connected clients.
 *
 * @param client
 *     The guac_client associated with the current RDP session.
 *
 * @param event
 *     The received CLIPRDR message, which must be a Data Response event.
 */
UINT guac_rdp_process_cb_data_response(CliprdrClientContext* context,
	CLIPRDR_FORMAT_DATA_RESPONSE* format_data_response);

#endif

