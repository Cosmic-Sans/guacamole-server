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

int guacenc_handle_cfill(guacenc_display* display, Guacamole::GuacServerInstruction::Reader instr) {
 
    /* Parse arguments */
    const auto cfill = instr.getCfill();
    const auto mask = static_cast<guac_composite_mode>(cfill.getMask());
    int index = cfill.getLayer();
    double r = cfill.getR() / 255.0;
    double g = cfill.getG() / 255.0;
    double b = cfill.getB() / 255.0;
    double a = cfill.getA() / 255.0;

    /* Pull buffer of requested layer/buffer */
    guacenc_buffer* buffer = guacenc_display_get_related_buffer(display, index);
    if (buffer == NULL)
        return 1;

    /* Fill with RGBA color */
    if (buffer->cairo != NULL) {
        cairo_set_operator(buffer->cairo, guacenc_display_cairo_operator(mask));
        cairo_set_source_rgba(buffer->cairo, r, g, b, a);
        cairo_fill(buffer->cairo);
    }

    return 0;

}

