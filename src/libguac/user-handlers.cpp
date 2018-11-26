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
#include "object.h"
#include "protocol.h"
#include "stream.h"
#include "timestamp.h"
#include "user.h"
#include "user-handlers.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <algorithm>
#include <iterator>
#include <utility>

/* Guacamole instruction handler map */
// NOTE: the order of instructions must correspond to their enum values

using __guac_instruction_handler_mapping = std::pair<Guacamole::GuacClientInstruction::Which, __guac_instruction_handler*>;
constexpr
__guac_instruction_handler_mapping __guac_instruction_handler_map[] = {
    {Guacamole::GuacClientInstruction::SYNC,       __guac_handle_sync},
    {Guacamole::GuacClientInstruction::MOUSE,      __guac_handle_mouse},
    {Guacamole::GuacClientInstruction::KEY,        __guac_handle_key},
    {Guacamole::GuacClientInstruction::CLIPBOARD,  __guac_handle_clipboard},
    {Guacamole::GuacClientInstruction::DISCONNECT, __guac_handle_disconnect},
    {Guacamole::GuacClientInstruction::SIZE,       __guac_handle_size},
    {Guacamole::GuacClientInstruction::FILE,       __guac_handle_file},
    {Guacamole::GuacClientInstruction::PIPE,       __guac_handle_pipe},
    {Guacamole::GuacClientInstruction::ACK,        __guac_handle_ack},
    {Guacamole::GuacClientInstruction::BLOB,       __guac_handle_blob},
    {Guacamole::GuacClientInstruction::END,        __guac_handle_end},
    {Guacamole::GuacClientInstruction::GET,        __guac_handle_get},
    {Guacamole::GuacClientInstruction::PUT,        __guac_handle_put},
    {Guacamole::GuacClientInstruction::AUDIO,      __guac_handle_audio},
};

static_assert([](const auto& instr_map){
        auto index = std::size_t(0);
        for (auto& [opcode, handler] : instr_map) {
            if (opcode != index++) {
                return false;
            }
        }
        return true;
    }(__guac_instruction_handler_map),
    "Instruction handler map not in order");

int guac_call_instruction_handler(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    const auto instr_index = instr.which();
    if (instr_index >= std::size(__guac_instruction_handler_map)) {
      return 0;
    }

		const auto& handler_pair = __guac_instruction_handler_map[instr_index];
		const auto handler = std::get<__guac_instruction_handler*>(handler_pair);

		return handler(user, instr);
}

/* Guacamole instruction handlers */

int __guac_handle_sync(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    int frame_duration;

    guac_timestamp current = guac_timestamp_current();
    guac_timestamp timestamp = instr.getSync();

    /* Error if timestamp is in future */
    if (timestamp > user->client->last_sent_timestamp)
        return -1;

    /* Only update lag calculations if timestamp is sane */
    if (timestamp >= user->last_received_timestamp) {

        /* Update stored timestamp */
        user->last_received_timestamp = timestamp;

        /* Calculate length of frame, including network and processing lag */
        frame_duration = current - timestamp;

        /* Update lag statistics if at least one frame has been rendered */
        if (user->last_frame_duration != 0) {

            /* Calculate lag using the previous frame as a baseline */
            int processing_lag = frame_duration - user->last_frame_duration;

            /* Adjust back to zero if cumulative error leads to a negative
             * value */
            if (processing_lag < 0)
                processing_lag = 0;

            user->processing_lag = processing_lag;

        }

        /* Record baseline duration of frame by excluding lag */
        user->last_frame_duration = frame_duration - user->processing_lag;

    }

    /* Log received timestamp and calculated lag (at TRACE level only) */
    guac_user_log(user, GUAC_LOG_TRACE,
            "User confirmation of frame %" PRIu64 "ms received "
            "at %" PRIu64 "ms (processing_lag=%ims)",
            timestamp, current, user->processing_lag);

    if (user->sync_handler)
        return user->sync_handler(user, timestamp);
    return 0;
}

int __guac_handle_mouse(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {
    if (user->mouse_handler) {
        auto mouse = instr.getMouse();
        return user->mouse_handler(user, mouse.getX(), mouse.getY(), mouse.getButtonMask());
    }
    return 0;
}

int __guac_handle_key(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {
    if (user->key_handler) {
        auto key = instr.getKey();
        return user->key_handler(user, key.getKeysym(), key.getPressed());
    }
    return 0;
}

/**
 * Retrieves the existing user-level input stream having the given index. These
 * will be streams which were created by the remotely-connected user. If the
 * index is invalid or too large, this function will automatically respond with
 * an "ack" instruction containing an appropriate error code.
 *
 * @param user
 *     The user associated with the stream being retrieved.
 *
 * @param stream_index
 *     The index of the stream to retrieve.
 *
 * @return
 *     The stream associated with the given user and having the given index,
 *     or NULL if the index is invalid.
 */
static guac_stream* __get_input_stream(guac_user* user, int stream_index) {

    /* Validate stream index */
    if (stream_index < 0 || stream_index >= GUAC_USER_MAX_STREAMS) {

        guac_stream dummy_stream;
        dummy_stream.index = stream_index;

        guac_protocol_send_ack(user->socket, &dummy_stream,
                "Invalid stream index", GUAC_PROTOCOL_STATUS_CLIENT_BAD_REQUEST);
        return NULL;
    }

    return &(user->__input_streams[stream_index]);

}

/**
 * Retrieves the existing, in-progress (open) user-level input stream having
 * the given index. These will be streams which were created by the
 * remotely-connected user. If the index is invalid, too large, or the stream
 * is closed, this function will automatically respond with an "ack"
 * instruction containing an appropriate error code.
 *
 * @param user
 *     The user associated with the stream being retrieved.
 *
 * @param stream_index
 *     The index of the stream to retrieve.
 *
 * @return
 *     The in-progress (open)stream associated with the given user and having
 *     the given index, or NULL if the index is invalid or the stream is
 *     closed.
 */
static guac_stream* __get_open_input_stream(guac_user* user, int stream_index) {

    guac_stream* stream = __get_input_stream(user, stream_index);

    /* Fail if no such stream */
    if (stream == NULL)
        return NULL;

    /* Validate initialization of stream */
    if (stream->index == GUAC_USER_CLOSED_STREAM_INDEX) {

        guac_stream dummy_stream;
        dummy_stream.index = stream_index;

        guac_protocol_send_ack(user->socket, &dummy_stream,
                "Invalid stream index", GUAC_PROTOCOL_STATUS_CLIENT_BAD_REQUEST);
        return NULL;
    }

    return stream;

}

/**
 * Initializes and returns a new user-level input stream having the given
 * index, clearing any values that may have been assigned by a past use of the
 * underlying stream object storage. If the stream was already open, it will
 * first be closed and its end handlers invoked as if explicitly closed by the
 * user.
 *
 * @param user
 *     The user associated with the stream being initialized.
 *
 * @param stream_index
 *     The index of the stream to initialized.
 *
 * @return
 *     A new initialized user-level input stream having the given index, or
 *     NULL if the index is invalid.
 */
static guac_stream* __init_input_stream(guac_user* user, int stream_index) {

    guac_stream* stream = __get_input_stream(user, stream_index);

    /* Fail if no such stream */
    if (stream == NULL)
        return NULL;

    /* Force end of previous stream if open */
    if (stream->index != GUAC_USER_CLOSED_STREAM_INDEX) {

        /* Call stream handler if defined */
        if (stream->end_handler)
            stream->end_handler(user, stream);

        /* Fall back to global handler if defined */
        else if (user->end_handler)
            user->end_handler(user, stream);

    }

    /* Initialize stream */
    stream->index = stream_index;
    stream->data = NULL;
    stream->ack_handler = NULL;
    stream->blob_handler = NULL;
    stream->end_handler = NULL;

    return stream;

}

int __guac_handle_audio(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    /* Pull corresponding stream */
    auto audio = instr.getAudio();
    int stream_index = audio.getStream();
    guac_stream* stream = __init_input_stream(user, stream_index);
    if (stream == NULL)
        return 0;

    /* If supported, call handler */
    if (user->audio_handler)
        return user->audio_handler(
            user,
            stream,
            const_cast<char*>(audio.getMimetype().cStr())
        );

    /* Otherwise, abort */
    guac_protocol_send_ack(user->socket, stream,
            "Audio input unsupported", GUAC_PROTOCOL_STATUS_UNSUPPORTED);
    return 0;

}

int __guac_handle_clipboard(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    /* Pull corresponding stream */
    auto clipboard = instr.getClipboard();
    int stream_index = clipboard.getStream();
    guac_stream* stream = __init_input_stream(user, stream_index);
    if (stream == NULL)
        return 0;

    /* If supported, call handler */
    if (user->clipboard_handler)
        return user->clipboard_handler(
            user,
            stream,
            const_cast<char*>(clipboard.getMimetype().cStr())
        );

    /* Otherwise, abort */
    guac_protocol_send_ack(user->socket, stream,
            "Clipboard unsupported", GUAC_PROTOCOL_STATUS_UNSUPPORTED);
    return 0;

}

int __guac_handle_size(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {
    if (user->size_handler) {
				auto size = instr.getSize();
				return user->size_handler(user, size.getWidth(), size.getHeight());
    }
    return 0;
}

int __guac_handle_file(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    /* Pull corresponding stream */
    auto file = instr.getFile();
    int stream_index = file.getStream();
    guac_stream* stream = __init_input_stream(user, stream_index);
    if (stream == NULL)
        return 0;

    /* If supported, call handler */
    if (user->file_handler)
        return user->file_handler(
            user,
            stream,
            const_cast<char*>(file.getMimetype().cStr()),
            const_cast<char*>(file.getFilename().cStr())
        );

    /* Otherwise, abort */
    guac_protocol_send_ack(user->socket, stream,
            "File transfer unsupported", GUAC_PROTOCOL_STATUS_UNSUPPORTED);
    return 0;
}

int __guac_handle_pipe(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    /* Pull corresponding stream */
    auto pipe = instr.getPipe();
    int stream_index = pipe.getStream();
    guac_stream* stream = __init_input_stream(user, stream_index);
    if (stream == NULL)
        return 0;

    /* If supported, call handler */
    if (user->pipe_handler)
        return user->pipe_handler(
            user,
            stream,
            const_cast<char*>(pipe.getMimetype().cStr()),
            const_cast<char*>(pipe.getName().cStr())
        );

    /* Otherwise, abort */
    guac_protocol_send_ack(user->socket, stream,
            "Named pipes unsupported", GUAC_PROTOCOL_STATUS_UNSUPPORTED);
    return 0;
}

int __guac_handle_ack(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    guac_stream* stream;
    auto ack = instr.getAck();

    /* Parse stream index */
    int stream_index = ack.getStream();

    /* Ignore indices of client-level streams */
    if (stream_index % 2 != 0)
        return 0;

    /* Determine index within user-level array of streams */
    stream_index /= 2;

    /* Validate stream index */
    if (stream_index < 0 || stream_index >= GUAC_USER_MAX_STREAMS)
        return 0;

    stream = &(user->__output_streams[stream_index]);

    /* Validate initialization of stream */
    if (stream->index == GUAC_USER_CLOSED_STREAM_INDEX)
        return 0;

    /* Call stream handler if defined */
    if (stream->ack_handler)
        return stream->ack_handler(user, stream, const_cast<char*>(ack.getMessage().cStr()),
                static_cast<guac_protocol_status>(ack.getStatus()));

    /* Fall back to global handler if defined */
    if (user->ack_handler)
        return user->ack_handler(user, stream, const_cast<char*>(ack.getMessage().cStr()),
                static_cast<guac_protocol_status>(ack.getStatus()));

    return 0;
}

int __guac_handle_blob(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    auto blob = instr.getBlob();
    int stream_index = blob.getStream();
    guac_stream* stream = __get_open_input_stream(user, stream_index);

    /* Fail if no such stream */
    if (stream == NULL)
        return 0;

    auto data = blob.getData();

    /* Call stream handler if defined */
    if (stream->blob_handler) {
        return stream->blob_handler(user, stream,
            const_cast<capnp::byte*>(data.begin()), data.size());
    }

    /* Fall back to global handler if defined */
    if (user->blob_handler) {
        return user->blob_handler(user, stream,
            const_cast<capnp::byte*>(data.begin()), data.size());
    }

    guac_protocol_send_ack(user->socket, stream,
            "File transfer unsupported", GUAC_PROTOCOL_STATUS_UNSUPPORTED);
    return 0;
}

int __guac_handle_end(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    int result = 0;
    int stream_index = instr.getEnd();
    guac_stream* stream = __get_open_input_stream(user, stream_index);

    /* Fail if no such stream */
    if (stream == NULL)
        return 0;

    /* Call stream handler if defined */
    if (stream->end_handler)
        result = stream->end_handler(user, stream);

    /* Fall back to global handler if defined */
    else if (user->end_handler)
        result = user->end_handler(user, stream);

    /* Mark stream as closed */
    stream->index = GUAC_USER_CLOSED_STREAM_INDEX;
    return result;
}

int __guac_handle_get(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    guac_object* object;
    auto get = instr.getGet();

    /* Validate object index */
    int object_index = get.getObject();
    if (object_index < 0 || object_index >= GUAC_USER_MAX_OBJECTS)
        return 0;

    object = &(user->__objects[object_index]);

    /* Validate initialization of object */
    if (object->index == GUAC_USER_UNDEFINED_OBJECT_INDEX)
        return 0;

    /* Call object handler if defined */
    if (object->get_handler)
        return object->get_handler(
            user,
            object,
            const_cast<char*>(get.getName().cStr())
        );

    /* Fall back to global handler if defined */
    if (user->get_handler)
        return user->get_handler(
            user,
            object,
            const_cast<char*>(get.getName().cStr())
        );

    return 0;
}

int __guac_handle_put(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {

    guac_object* object;
    auto put = instr.getPut();

    /* Validate object index */
    int object_index = put.getObject();
    if (object_index < 0 || object_index >= GUAC_USER_MAX_OBJECTS)
        return 0;

    object = &(user->__objects[object_index]);

    /* Validate initialization of object */
    if (object->index == GUAC_USER_UNDEFINED_OBJECT_INDEX)
        return 0;

    /* Pull corresponding stream */
    int stream_index = put.getStream();
    guac_stream* stream = __init_input_stream(user, stream_index);
    if (stream == NULL)
        return 0;

    /* Call object handler if defined */
    if (object->put_handler)
        return object->put_handler(
            user,
            object, 
            stream,
            const_cast<char*>(put.getMimetype().cStr()),
            const_cast<char*>(put.getName().cStr())
        );

    /* Fall back to global handler if defined */
    if (user->put_handler)
        return user->put_handler(
            user,
            object,
            stream,
            const_cast<char*>(put.getMimetype().cStr()),
            const_cast<char*>(put.getName().cStr())
        );

    /* Otherwise, abort */
    guac_protocol_send_ack(user->socket, stream,
            "Object write unsupported", GUAC_PROTOCOL_STATUS_UNSUPPORTED);
    return 0;
}

int __guac_handle_disconnect(guac_user* user, Guacamole::GuacClientInstruction::Reader instr) {
    guac_user_stop(user);
    return 0;
}

