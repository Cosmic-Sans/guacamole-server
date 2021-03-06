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

int guacenc_handle_copy(guacenc_display* display, Guacamole::GuacServerInstruction::Reader instr) {

    /* Parse arguments */
    const auto copy = instr.getCopy();
    int sindex = copy.getSrcLayer();
    int sx = copy.getSrcX();
    int sy = copy.getSrcY();
    int width = copy.getSrcWidth();
    int height = copy.getSrcHeight();
    const auto mask = static_cast<guac_composite_mode>(copy.getMask());
    int dindex = copy.getDstLayer();
    int dx = copy.getDstX();
    int dy = copy.getDstY();

    /* Pull buffer of source layer/buffer */
    guacenc_buffer* src = guacenc_display_get_related_buffer(display, sindex);
    if (src == NULL)
        return 1;

    /* Pull buffer of destination layer/buffer */
    guacenc_buffer* dst = guacenc_display_get_related_buffer(display, dindex);
    if (dst == NULL)
        return 1;

    /* Expand the destination buffer as necessary to fit the draw operation */
    if (dst->autosize)
        guacenc_buffer_fit(dst, dx + width, dy + height);

    /* Copy rectangle from source to destination */
    if (src->surface != NULL && dst->cairo != NULL) {

        /* If surfaces are different, no need to copy */
        cairo_surface_t* surface;
        if (src != dst)
            surface = src->surface;

        /* Otherwise, copy to a temporary surface */
        else {

            /* Create new surface to hold the source rect */
            surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                    width, height);

            /* Copy relevant rectangle from source surface */
            cairo_t* cairo = cairo_create(surface);
            cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
            cairo_set_source_surface(cairo, src->surface, -sx, -sy);
            cairo_paint(cairo);
            cairo_destroy(cairo);

            /* Source coordinates are now (0, 0) */
            sx = sy = 0;

        }

        /* Perform copy */
        cairo_set_operator(dst->cairo, guacenc_display_cairo_operator(mask));
        cairo_set_source_surface(dst->cairo, surface, dx - sx, dy - sy);
        cairo_rectangle(dst->cairo, dx, dy, width, height);
        cairo_fill(dst->cairo);

        /* Destroy temporary surface if it was created */
        if (surface != src->surface)
            cairo_surface_destroy(surface);

    }

    return 0;

}

