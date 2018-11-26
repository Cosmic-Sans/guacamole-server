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

int guacenc_handle_transfer(guacenc_display* display, Guacamole::GuacServerInstruction::Reader instr) {

    /* Parse arguments */
    const auto transfer = instr.getTransfer();
    int src_index = transfer.getSrcLayer();
    int src_x = transfer.getSrcX();
    int src_y = transfer.getSrcY();
    int src_w = transfer.getSrcWidth();
    int src_h = transfer.getSrcHeight();
    int function = transfer.getFunction();
    int dst_index = transfer.getDstLayer();
    int dst_x = transfer.getDstX();
    int dst_y = transfer.getDstY();

    /* TODO: Unimplemented for now (rarely used) */
    guacenc_log(GUAC_LOG_DEBUG, "transform: src_layer=%i (%i, %i) %ix%i "
            "function=0x%X dst_layer=%i (%i, %i)", src_index, src_x, src_y,
            src_w, src_h, function, dst_index, dst_x, dst_y);

    return 0;

}

