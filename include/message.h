#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdlib.h>
#include <stdbool.h>

typedef struct Message Message;

Message* message_create(const char data[]);

void message_destroy(Message* msg);

void message_show(const Message* msg);

char* message_get_payload(Message* msg);

size_t message_get_payload_size(const Message* msg);

#endif // !MESSAGE_H

