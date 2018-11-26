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
}
#include "instructions.h"

#include <capnp/dynamic.h>
#include <guacamole/client.h>

#include <iterator>
#include <string.h>

// NOTE: the order of instructions must correspond to their enum values
constexpr
guacenc_instruction_handler_mapping guacenc_instruction_handler_map[] = {
    {Guacamole::GuacServerInstruction::ARC,        nullptr},
    {Guacamole::GuacServerInstruction::CFILL,      guacenc_handle_cfill},
    {Guacamole::GuacServerInstruction::CLIP,       nullptr},
    {Guacamole::GuacServerInstruction::CLOSE,      nullptr},
    {Guacamole::GuacServerInstruction::COPY,       guacenc_handle_copy},
    {Guacamole::GuacServerInstruction::CSTROKE,    nullptr},
    {Guacamole::GuacServerInstruction::CURSOR,     guacenc_handle_cursor},
    {Guacamole::GuacServerInstruction::CURVE,      nullptr},
    {Guacamole::GuacServerInstruction::DISPOSE,    guacenc_handle_dispose},
    {Guacamole::GuacServerInstruction::DISTORT,    nullptr},
    {Guacamole::GuacServerInstruction::IDENTITY,   nullptr},
    {Guacamole::GuacServerInstruction::LFILL,      nullptr},
    {Guacamole::GuacServerInstruction::LINE,       nullptr},
    {Guacamole::GuacServerInstruction::LSTROKE,    nullptr},
    {Guacamole::GuacServerInstruction::MOVE,       guacenc_handle_move},
    {Guacamole::GuacServerInstruction::POP,        nullptr},
    {Guacamole::GuacServerInstruction::PUSH,       nullptr},
    {Guacamole::GuacServerInstruction::RECT,       guacenc_handle_rect},
    {Guacamole::GuacServerInstruction::RESET,      nullptr},
    {Guacamole::GuacServerInstruction::SET,        nullptr},
    {Guacamole::GuacServerInstruction::SHADE,      guacenc_handle_shade},
    {Guacamole::GuacServerInstruction::SIZE,       guacenc_handle_size},
    {Guacamole::GuacServerInstruction::START,      nullptr},
    {Guacamole::GuacServerInstruction::TRANSFER,   guacenc_handle_transfer},
    {Guacamole::GuacServerInstruction::TRANSFORM,  nullptr},
    {Guacamole::GuacServerInstruction::ACK,        nullptr},
    {Guacamole::GuacServerInstruction::AUDIO,      nullptr},
    {Guacamole::GuacServerInstruction::BLOB,       guacenc_handle_blob},
    {Guacamole::GuacServerInstruction::CLIPBOARD,  nullptr},
    {Guacamole::GuacServerInstruction::END,        guacenc_handle_end},
    {Guacamole::GuacServerInstruction::FILE,       nullptr},
    {Guacamole::GuacServerInstruction::IMG,        guacenc_handle_img},
    {Guacamole::GuacServerInstruction::NEST,       nullptr},
    {Guacamole::GuacServerInstruction::PIPE,       nullptr},
    {Guacamole::GuacServerInstruction::VIDEO,      nullptr},
    {Guacamole::GuacServerInstruction::BODY,       nullptr},
    {Guacamole::GuacServerInstruction::FILESYSTEM, nullptr},
    {Guacamole::GuacServerInstruction::UNDEFINE,   nullptr},
    {Guacamole::GuacServerInstruction::ARGS,       nullptr},
    {Guacamole::GuacServerInstruction::DISCONNECT, nullptr},
    {Guacamole::GuacServerInstruction::ERROR,      nullptr},
    {Guacamole::GuacServerInstruction::LOG,        nullptr},
    {Guacamole::GuacServerInstruction::MOUSE,      guacenc_handle_mouse},
    {Guacamole::GuacServerInstruction::KEY,        nullptr},
    {Guacamole::GuacServerInstruction::NOP,        nullptr},
    {Guacamole::GuacServerInstruction::READY,      nullptr},
    {Guacamole::GuacServerInstruction::SYNC,       guacenc_handle_sync},
    {Guacamole::GuacServerInstruction::NAME,       nullptr},
};

static_assert([](const auto& instr_map){
        auto index = std::size_t(0);
        for (auto& [opcode, handler] : instr_map) {
            if (opcode != index++) {
                return false;
            }
        }
        return true;
    }(guacenc_instruction_handler_map),
    "Instruction handler map not in order");

int guacenc_handle_instruction(guacenc_display* display,
                               Guacamole::GuacServerInstruction::Reader instr) {

    const auto instr_index = instr.which();
    if (instr_index >= std::size(guacenc_instruction_handler_map)) {
      /* Ignore any unknown instructions */
      return 0;
    }

		const auto& handler_pair = guacenc_instruction_handler_map[instr_index];
		const auto handler = std::get<guacenc_instruction_handler*>(handler_pair);
    if (!handler) {
      /* Log defined but unimplemented instructions */
      KJ_IF_MAYBE(field, capnp::DynamicStruct::Reader(instr).which()) {
        if (field) {
          guacenc_log(GUAC_LOG_DEBUG, "\"%s\" not implemented", field->getProto().getName());
        }
      }
      return 0;
    }

		return handler(display, instr);

}

