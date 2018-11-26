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

#include "common/recording.h"

#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/timestamp.h>

#ifdef __MINGW32__
#include <direct.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

guac_common_recording* guac_common_recording_create(guac_client* client,
        const char* path, const char* name, int create_path,
        int include_output, int include_mouse, int include_keys) {

    return NULL;

}

void guac_common_recording_free(guac_common_recording* recording) {

    /* If not including broadcast output, the output socket is not associated
     * with the client, and must be freed manually */
    if (!recording->include_output)
        guac_socket_free(recording->socket);

    /* Free recording itself */
    free(recording);

}

void guac_common_recording_report_mouse(guac_common_recording* recording,
        int x, int y, int button_mask) {

    /* Report mouse location only if recording should contain mouse events */
    if (recording->include_mouse)
        guac_protocol_send_mouse(recording->socket, x, y, button_mask,
                guac_timestamp_current());

}

void guac_common_recording_report_key(guac_common_recording* recording,
        int keysym, int pressed) {

    /* Report key state only if recording should contain key events */
    if (recording->include_keys)
        guac_protocol_send_key(recording->socket, keysym, pressed,
                guac_timestamp_current());

}

