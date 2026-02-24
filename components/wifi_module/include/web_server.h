#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_err.h"

void start_webserver(void);
void url_decode(char *dst, const char *src);

#endif
