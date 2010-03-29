#ifndef COMETD_MESSAGE_H
#define COMETD_MESSAGE_H

struct evbuffer;

struct p_message {

	struct evbuffer	*data;
	struct p_message *next;
};

struct p_message *
message_new(const char *data, size_t length);

void
message_free(struct p_message *);

#endif /* COMETD_MESSAGE_H */

