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

extern "C" {
#include "config.h"
#include "display.h"
#include "log.h"

#include <guacamole/client.h>
}
#include "Guacamole.capnp.h"

#include <stdlib.h>

int guacenc_handle_size(guacenc_display* display, Guacamole::GuacServerInstruction::Reader instr) {

    /* Parse arguments */
    const auto size = instr.getSize();
    int index = size.getLayer();
    int width = size.getWidth();
    int height = size.getHeight();

    /* Retrieve requested layer/buffer */
    guacenc_buffer* buffer = guacenc_display_get_related_buffer(display, index);
    if (buffer == NULL)
        return 1;

    /* Resize layer/buffer */
    return guacenc_buffer_resize(buffer, width, height);

}

