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
#include "rdp.h"
#include "rdp_fs.h"
#include "rdp_stream.h"

#include <freerdp/freerdp.h>
#include <freerdp/channels/channels.h>
#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/stream.h>

#include <freerdp/client/cliprdr.h>

#include <winpr/stream.h>
#include <winpr/wtypes.h>

#include <stdlib.h>
#include "rdp_cliprdr.h"

/**
 * Writes the given filename to the given upload path, sanitizing the filename
 * and translating the filename to the root directory.
 */
static void __generate_upload_path(const char* filename, char* path) {

    int i;

    /* Add initial backslash */
    *(path++) = '\\';

    for (i=1; i<GUAC_RDP_FS_MAX_PATH; i++) {

        /* Get current, stop at end */
        char c = *(filename++);
        if (c == '\0')
            break;

        /* Replace special characters with underscores */
        if (c == '/' || c == '\\')
            c = '_';

        *(path++) = c;

    }

    /* Terminate path */
    *path = '\0';

}

int guac_rdp_clipboard_handler(guac_user* user, guac_stream* stream,
        char* mimetype) {

    guac_client* client = user->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_rdp_stream* rdp_stream;

    /* Init stream data */
    stream->data = rdp_stream = malloc(sizeof(guac_rdp_stream));
    stream->blob_handler = guac_rdp_clipboard_blob_handler;
    stream->end_handler = guac_rdp_clipboard_end_handler;
    rdp_stream->type = GUAC_RDP_INBOUND_CLIPBOARD_STREAM;

    guac_common_clipboard_reset(rdp_client->clipboard, mimetype);
    return 0;

}

int guac_rdp_clipboard_blob_handler(guac_user* user, guac_stream* stream,
        void* data, int length) {

    guac_client* client = user->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;
    guac_common_clipboard_append(rdp_client->clipboard, (char*) data, length);

    return 0;
}

int guac_rdp_clipboard_end_handler(guac_user* user, guac_stream* stream) {

    guac_client* client = user->client;
    guac_rdp_client* rdp_client = (guac_rdp_client*) client->data;

    /* Terminate clipboard data with NULL */
    guac_common_clipboard_append(rdp_client->clipboard, "", 1);

    /* Notify RDP server of new data, if connected */
		CliprdrClientContext* cliprdr = rdp_client->cliprdr;
    if (cliprdr != NULL)
				guac_cliprdr_send_client_format_list(cliprdr);

    return 0;
}
