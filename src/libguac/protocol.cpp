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

#include "Guacamole.capnp.h"
#include "config.h"

#include "error.h"
#include "layer.h"
#include "object.h"
#include "palette.h"
#include "protocol.h"
#include "socket.h"
#include "stream.h"
#include "unicode.h"

#include <cairo/cairo.h>

#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <capnp/message.h>

/* Protocol functions */

int guac_protocol_send_ack(guac_socket* socket, guac_stream* stream,
        const char* error, guac_protocol_status status) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto ack = message_builder.initRoot<Guacamole::GuacServerInstruction>().initAck();
    ack.setStream(stream->index);
    ack.setMessage(error);
    ack.setStatus(status);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);

    return ret_val;

}

template<typename T>
static uint32_t __array_len(T* array) {
		uint32_t size = 0;
		while (array++ != nullptr) {
			size++;
		}
		return size;
}

static void __copy_args_to_list(const char** args, capnp::List<capnp::Text>::Builder list) {
		uint32_t i = 0;
		auto arg = args;
		while (arg++) {
			list[i++].asReader() = capnp::Text::Reader(*arg);
		}
}

int guac_protocol_send_args(guac_socket* socket, const char** args) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
		const auto num_args = __array_len(args);
    auto args_message = message_builder.initRoot<Guacamole::GuacServerInstruction>().initArgs(num_args);
		__copy_args_to_list(args, args_message);
    ret_val = socket->write_handler(socket, &message_builder);
    guac_socket_instruction_end(socket);

    return ret_val;

}

int guac_protocol_send_arc(guac_socket* socket, const guac_layer* layer,
        int x, int y, int radius, double startAngle, double endAngle,
        int negative) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto arc = message_builder.initRoot<Guacamole::GuacServerInstruction>().initArc();
		arc.setLayer(layer->index);
		arc.setX(x);
		arc.setY(y);
		arc.setRadius(radius);
		arc.setStart(startAngle);
		arc.setEnd(endAngle);
		arc.setNegative(negative);
    ret_val = socket->write_handler(socket, &message_builder);
    guac_socket_instruction_end(socket);

    return ret_val;

}

int guac_protocol_send_audio(guac_socket* socket, const guac_stream* stream,
        const char* mimetype) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto audio = message_builder.initRoot<Guacamole::GuacServerInstruction>().initAudio();
		audio.setStream(stream->index);
		audio.setMimetype(mimetype);
    ret_val = socket->write_handler(socket, &message_builder);
    guac_socket_instruction_end(socket);

    return ret_val;

}

int guac_protocol_send_blob(guac_socket* socket, const guac_stream* stream,
        const void* data, int count) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto blob = message_builder.initRoot<Guacamole::GuacServerInstruction>().initBlob();
		blob.setStream(stream->index);
		blob.setData(capnp::Data::Reader(static_cast<const capnp::byte*>(data), count));
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_body(guac_socket* socket, const guac_object* object,
        const guac_stream* stream, const char* mimetype, const char* name) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto body = message_builder.initRoot<Guacamole::GuacServerInstruction>().initBody();
        body.setObject(object->index);
        body.setStream(stream->index);
        body.setMimetype(mimetype);
        body.setName(name);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_cfill(guac_socket* socket,
        guac_composite_mode mode, const guac_layer* layer,
        int r, int g, int b, int a) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto cfill = message_builder.initRoot<Guacamole::GuacServerInstruction>().initCfill();
		cfill.setMask(mode);
		cfill.setLayer(layer->index);
		cfill.setR(r);
		cfill.setG(g);
		cfill.setB(b);
		cfill.setA(a);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_close(guac_socket* socket, const guac_layer* layer) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setClose(layer->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_clip(guac_socket* socket, const guac_layer* layer) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setClip(layer->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_clipboard(guac_socket* socket, const guac_stream* stream,
        const char* mimetype) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto clipboard = message_builder.initRoot<Guacamole::GuacServerInstruction>().initClipboard();
		clipboard.setStream(stream->index);
		clipboard.setMimetype(mimetype);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_copy(guac_socket* socket,
        const guac_layer* srcl, int srcx, int srcy, int w, int h,
        guac_composite_mode mode, const guac_layer* dstl, int dstx, int dsty) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto copy = message_builder.initRoot<Guacamole::GuacServerInstruction>().initCopy();
		copy.setSrcLayer(srcl->index);
		copy.setSrcX(srcx);
		copy.setSrcY(srcy);
		copy.setSrcWidth(w);
		copy.setSrcHeight(h);
		copy.setMask(mode);
		copy.setDstLayer(dstl->index);
		copy.setDstX(dstx);
		copy.setDstY(dsty);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_cstroke(guac_socket* socket,
        guac_composite_mode mode, const guac_layer* layer,
        guac_line_cap_style cap, guac_line_join_style join, int thickness,
        int r, int g, int b, int a) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto cstroke = message_builder.initRoot<Guacamole::GuacServerInstruction>().initCstroke();
		cstroke.setMask(mode);
		cstroke.setLayer(layer->index);
		cstroke.setCap(cap);
		cstroke.setJoin(join);
		cstroke.setThickness(thickness);
		cstroke.setR(r);
		cstroke.setG(g);
		cstroke.setB(b);
		cstroke.setA(a);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_cursor(guac_socket* socket, int x, int y,
        const guac_layer* srcl, int srcx, int srcy, int w, int h) {
    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto cursor = message_builder.initRoot<Guacamole::GuacServerInstruction>().initCursor();
		cursor.setX(x);
		cursor.setY(y);
		cursor.setSrcLayer(srcl->index);
		cursor.setSrcX(srcx);
		cursor.setSrcY(srcy);
		cursor.setSrcWidth(w);
		cursor.setSrcHeight(h);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_curve(guac_socket* socket, const guac_layer* layer,
        int cp1x, int cp1y, int cp2x, int cp2y, int x, int y) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto curve = message_builder.initRoot<Guacamole::GuacServerInstruction>().initCurve();
		curve.setLayer(layer->index);
		curve.setCp1x(cp1x);
		curve.setCp1y(cp1y);
		curve.setCp2x(cp2x);
		curve.setCp2y(cp2y);
		curve.setX(x);
		curve.setY(y);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_disconnect(guac_socket* socket) {
    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setDisconnect();
    ret_val = socket->write_handler(socket, &message_builder);
    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_dispose(guac_socket* socket, const guac_layer* layer) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setDispose(layer->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_distort(guac_socket* socket, const guac_layer* layer,
        double a, double b, double c,
        double d, double e, double f) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto distort = message_builder.initRoot<Guacamole::GuacServerInstruction>().initDistort();
		distort.setLayer(layer->index);
		distort.setA(a);
		distort.setB(b);
		distort.setC(c);
		distort.setD(d);
		distort.setE(e);
		distort.setF(f);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_end(guac_socket* socket, const guac_stream* stream) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setEnd(stream->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_error(guac_socket* socket, const char* error,
        guac_protocol_status status) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto error_message = message_builder.initRoot<Guacamole::GuacServerInstruction>().initError();
		error_message.setText(error);
		error_message.setStatus(status);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int vguac_protocol_send_log(guac_socket* socket, const char* format,
        va_list args) {

    int ret_val;

    /* Copy log message into buffer */
    char message[4096];
    vsnprintf(message, sizeof(message), format, args);

    /* Log to instruction */
    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setLog(message);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_log(guac_socket* socket, const char* format, ...) {

    int ret_val;

    va_list args;
    va_start(args, format);
    ret_val = vguac_protocol_send_log(socket, format, args);
    va_end(args);

    return ret_val;

}

int guac_protocol_send_file(guac_socket* socket, const guac_stream* stream,
        const char* mimetype, const char* name) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto file = message_builder.initRoot<Guacamole::GuacServerInstruction>().initFile();
		file.setStream(stream->index);
		file.setMimetype(mimetype);
		file.setFilename(name);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_filesystem(guac_socket* socket,
        const guac_object* object, const char* name) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto filesystem = message_builder.initRoot<Guacamole::GuacServerInstruction>().initFilesystem();
		filesystem.setObject(object->index);
		filesystem.setName(name);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_identity(guac_socket* socket, const guac_layer* layer) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setIdentity(layer->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_key(guac_socket* socket, int keysym, int pressed,
        guac_timestamp timestamp) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto mouse = message_builder.initRoot<Guacamole::GuacServerInstruction>().initKey();
		mouse.setKeysym(keysym);
    mouse.setPressed(pressed);
    mouse.setTimestamp(timestamp);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_lfill(guac_socket* socket,
        guac_composite_mode mode, const guac_layer* layer,
        const guac_layer* srcl) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto lfill = message_builder.initRoot<Guacamole::GuacServerInstruction>().initLfill();
		lfill.setMask(mode);
		lfill.setLayer(layer->index);
		lfill.setSrcLayer(srcl->index);
		ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_line(guac_socket* socket, const guac_layer* layer,
        int x, int y) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto line = message_builder.initRoot<Guacamole::GuacServerInstruction>().initLine();
		line.setLayer(layer->index);
		line.setX(x);
		line.setY(y);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_lstroke(guac_socket* socket,
        guac_composite_mode mode, const guac_layer* layer,
        guac_line_cap_style cap, guac_line_join_style join, int thickness,
        const guac_layer* srcl) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto lstroke = message_builder.initRoot<Guacamole::GuacServerInstruction>().initLstroke();
		lstroke.setMask(mode);
		lstroke.setLayer(layer->index);
		lstroke.setCap(static_cast<Guacamole::Lstroke::LineCap>(cap));
		lstroke.setJoin(join);
		lstroke.setThickness(thickness);
		lstroke.setSrcLayer(srcl->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_mouse(guac_socket* socket, int x, int y,
        int button_mask, guac_timestamp timestamp) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto mouse = message_builder.initRoot<Guacamole::GuacServerInstruction>().initMouse();
		mouse.setX(x);
		mouse.setY(y);
    mouse.setButtonMask(button_mask);
    mouse.setTimestamp(timestamp);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_move(guac_socket* socket, const guac_layer* layer,
        const guac_layer* parent, int x, int y, int z) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto move = message_builder.initRoot<Guacamole::GuacServerInstruction>().initMove();
		move.setLayer(layer->index);
		move.setParent(parent->index);
		move.setX(x);
		move.setY(y);
		move.setZ(z);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_name(guac_socket* socket, const char* name) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setName(name);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

/*
int guac_protocol_send_nest(guac_socket* socket, int index,
        const char* data) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto nest = message_builder.initRoot<Guacamole::GuacServerInstruction>().initNest();
		nest.setIndex(index);
		nest.setData(data);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}
*/

int guac_protocol_send_nop(guac_socket* socket) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setNop();
    ret_val = socket->write_handler(socket, &message_builder);
    guac_socket_instruction_end(socket);

    return ret_val;

}

int guac_protocol_send_pipe(guac_socket* socket, const guac_stream* stream,
        const char* mimetype, const char* name) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto pipe = message_builder.initRoot<Guacamole::GuacServerInstruction>().initPipe();
		pipe.setStream(stream->index);
		pipe.setMimetype(mimetype);
		pipe.setName(name);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_img(guac_socket* socket, const guac_stream* stream,
        guac_composite_mode mode, const guac_layer* layer,
        const char* mimetype, int x, int y) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto img = message_builder.initRoot<Guacamole::GuacServerInstruction>().initImg();
		img.setStream(stream->index);
		img.setMode(mode);
		img.setLayer(layer->index);
		img.setMimetype(mimetype);
		img.setX(x);
		img.setY(y);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_pop(guac_socket* socket, const guac_layer* layer) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setPop(layer->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_push(guac_socket* socket, const guac_layer* layer) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setPush(layer->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_ready(guac_socket* socket, const char* id) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setReady(id);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_rect(guac_socket* socket,
        const guac_layer* layer, int x, int y, int width, int height) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto rect = message_builder.initRoot<Guacamole::GuacServerInstruction>().initRect();
		rect.setLayer(layer->index);
		rect.setX(x);
		rect.setY(y);
		rect.setWidth(width);
		rect.setHeight(height);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_reset(guac_socket* socket, const guac_layer* layer) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setReset(layer->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_set(guac_socket* socket, const guac_layer* layer,
        const char* name, const char* value) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto set = message_builder.initRoot<Guacamole::GuacServerInstruction>().initSet();
		set.setLayer(layer->index);
		set.setProperty(name);
		set.setValue(value);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_shade(guac_socket* socket, const guac_layer* layer,
        int a) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto shade = message_builder.initRoot<Guacamole::GuacServerInstruction>().initShade();
		shade.setLayer(layer->index);
		shade.setOpacity(a);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_size(guac_socket* socket, const guac_layer* layer,
        int w, int h) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto size = message_builder.initRoot<Guacamole::GuacServerInstruction>().initSize();
		size.setLayer(layer->index);
		size.setWidth(w);
		size.setHeight(h);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_start(guac_socket* socket, const guac_layer* layer,
        int x, int y) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto start = message_builder.initRoot<Guacamole::GuacServerInstruction>().initStart();
		start.setLayer(layer->index);
		start.setX(x);
		start.setY(y);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_sync(guac_socket* socket, guac_timestamp timestamp) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setSync(timestamp);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_transfer(guac_socket* socket,
        const guac_layer* srcl, int srcx, int srcy, int w, int h,
        guac_transfer_function fn, const guac_layer* dstl, int dstx, int dsty) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto transfer = message_builder.initRoot<Guacamole::GuacServerInstruction>().initTransfer();
		transfer.setSrcLayer(srcl->index);
		transfer.setSrcX(srcx);
		transfer.setSrcY(srcy);
		transfer.setSrcWidth(w);
		transfer.setSrcHeight(h);
		transfer.setFunction(fn);
		transfer.setDstLayer(dstl->index);
		transfer.setDstX(dstx);
		transfer.setDstY(dsty);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_transform(guac_socket* socket, const guac_layer* layer,
        double a, double b, double c,
        double d, double e, double f) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto transform = message_builder.initRoot<Guacamole::GuacServerInstruction>().initTransform();
		transform.setLayer(layer->index);
		transform.setA(a);
		transform.setB(b);
		transform.setC(c);
		transform.setD(d);
		transform.setE(e);
		transform.setF(f);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_undefine(guac_socket* socket,
        const guac_object* object) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    message_builder.initRoot<Guacamole::GuacServerInstruction>().setUndefine(object->index);
    ret_val = socket->write_handler(socket, &message_builder);

    guac_socket_instruction_end(socket);
    return ret_val;

}

int guac_protocol_send_video(guac_socket* socket, const guac_stream* stream,
        const guac_layer* layer, const char* mimetype) {

    int ret_val;

    guac_socket_instruction_begin(socket);

    capnp::MallocMessageBuilder message_builder;
    auto video = message_builder.initRoot<Guacamole::GuacServerInstruction>().initVideo();
		video.setStream(stream->index);
		video.setLayer(layer->index);
		video.setMimetype(mimetype);
    ret_val = socket->write_handler(socket, &message_builder);
    guac_socket_instruction_end(socket);

    return ret_val;

}

