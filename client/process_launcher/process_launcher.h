#ifndef ROOTKIT_PROCESS_LAUNCHER_H
#define ROOTKIT_PROCESS_LAUNCHER_H

#include "networking.h"

void run_process(const struct session_info * session_info, char * command);

#endif //ROOTKIT_PROCESS_LAUNCHER_H