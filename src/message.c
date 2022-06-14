#include <string.h>
#include <stdio.h>
#include "message.h"

struct Message {
    size_t payload_size;
    char payload[];
};

Message* message_create(const char data[const]) {
    if (data == NULL) {
        return NULL;
    }
    size_t data_len = strlen(data) + 1;
    Message *msg;
    msg = malloc(sizeof(*msg) + data_len);
    if (msg == NULL) {
        return NULL;
    }
    msg->payload_size = sizeof(*(msg->payload)) * data_len;
    memcpy(msg->payload, data, data_len); 
    return msg;
}

void message_destroy(Message* const msg) {
    if (msg == NULL) {
        return;
    }
    free(msg);
}

char* message_get_payload(Message* const msg) {
    if (msg == NULL) {
        return NULL;
    }
    return msg->payload;
}

const char* message_get_payload_const(const Message* const msg) {
    if (msg == NULL) {
        return NULL;
    }
    return msg->payload;
}

size_t message_get_payload_size(const Message* const msg) {
    if (msg == NULL) {
        return 0;
    }
    return msg->payload_size;
}
