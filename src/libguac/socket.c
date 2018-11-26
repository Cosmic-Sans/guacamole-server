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

#include "error.h"
#include "protocol.h"
#include "socket.h"
#include "timestamp.h"

#include <inttypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

char __guac_socket_BASE64_CHARACTERS[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's',
    't', 'u', 'v', 'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/'
};

static void* __guac_socket_keep_alive_thread(void* data) {

    /* Socket keep-alive loop */
    guac_socket* socket = (guac_socket*) data;
    while (socket->state == GUAC_SOCKET_OPEN) {

        /* Send NOP keep-alive if it's been a while since the last output */
        guac_timestamp timestamp = guac_timestamp_current();
        if (timestamp - socket->last_write_timestamp >
                GUAC_SOCKET_KEEP_ALIVE_INTERVAL) {

            /* Send NOP */
            if (guac_protocol_send_nop(socket)
                || guac_socket_flush(socket))
                break;

        }

        /* Sleep until next keep-alive check */
        guac_timestamp_msleep(GUAC_SOCKET_KEEP_ALIVE_INTERVAL);

    }

    return NULL;

}

ssize_t guac_socket_read(guac_socket* socket, void* buf, size_t count) {

    /* If handler defined, call it. */
    if (socket->read_handler)
        return socket->read_handler(socket, buf, count);

    /* Otherwise, pretend nothing was read. */
    return 0;

}

int guac_socket_select(guac_socket* socket, int usec_timeout) {

    /* Call select handler if defined */
    if (socket->select_handler)
        return socket->select_handler(socket, usec_timeout);

    /* Otherwise, assume ready. */
    return 1;

}

guac_socket* guac_socket_alloc() {

    guac_socket* socket = malloc(sizeof(guac_socket));

    /* If no memory available, return with error */
    if (socket == NULL) {
        guac_error = GUAC_STATUS_NO_MEMORY;
        guac_error_message = "Could not allocate memory for socket";
        return NULL;
    }

    socket->__ready = 0;
    socket->data = NULL;
    socket->state = GUAC_SOCKET_OPEN;
    socket->last_write_timestamp = guac_timestamp_current();

    /* No keep alive ping by default */
    socket->__keep_alive_enabled = 0;

    /* No handlers yet */
    socket->read_handler   = NULL;
    socket->write_handler  = NULL;
    socket->select_handler = NULL;
    socket->free_handler   = NULL;
    socket->flush_handler  = NULL;
    socket->lock_handler   = NULL;
    socket->unlock_handler = NULL;

    return socket;

}

void guac_socket_require_keep_alive(guac_socket* socket) {

    /* Start keep-alive thread */
    socket->__keep_alive_enabled = 1;
    pthread_create(&(socket->__keep_alive_thread), NULL,
                __guac_socket_keep_alive_thread, (void*) socket);

}

void guac_socket_instruction_begin(guac_socket* socket) {

    /* Call instruction begin handler if defined */
    if (socket->lock_handler)
        socket->lock_handler(socket);

}

void guac_socket_instruction_end(guac_socket* socket) {

    /* Call instruction end handler if defined */
    if (socket->unlock_handler)
        socket->unlock_handler(socket);

}

void guac_socket_free(guac_socket* socket) {

    guac_socket_flush(socket);

    /* Call free handler if defined */
    if (socket->free_handler)
        socket->free_handler(socket);

    /* Mark as closed */
    socket->state = GUAC_SOCKET_CLOSED;

    /* Wait for keep-alive, if enabled */
    if (socket->__keep_alive_enabled)
        pthread_join(socket->__keep_alive_thread, NULL);

    free(socket);
}

ssize_t guac_socket_flush(guac_socket* socket) {

    /* If handler defined, call it. */
    if (socket->flush_handler)
        return socket->flush_handler(socket);

    /* Otherwise, do nothing */
    return 0;

}

