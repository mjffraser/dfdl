#pragma once

#include <sys/types.h>

namespace dfd {

int openSocket();
void closeSocket();
int connect();
ssize_t sendMessage();
ssize_t recvMessage();


}
