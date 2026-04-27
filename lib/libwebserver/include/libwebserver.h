#ifndef LIBWEBSERVER_H
#define LIBWEBSERVER_H

#include "webserverinterface.h"
/*
    WebSocket Data Protocol

       <FLAG, 4B>  <CHANNEL, 8B>    <DATA>
        '@WSC'       char[8]           X
*/

IWebServer* WebServerCreate();
void        WebServerRelease(IWebServer* pWebServer);

#endif // LIBWEBSERVER_H