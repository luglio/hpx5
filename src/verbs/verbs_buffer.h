#ifndef VERBS_BUFFER_H
#define VERBS_BUFFER_H

#include "photon_buffer.h"

int __verbs_buffer_register(photonBuffer dbuffer);
int __verbs_buffer_unregister(photonBuffer dbuffer);

extern struct photon_buffer_interface_t verbs_buffer_interface;

#endif
