#ifndef   _MESH_WEB_SERVER_H_
#define   _MESH_WEB_SERVER_H_

#define WEB_PORT          80

void webServerInit( void );
void webServerConnectCb(void *arg);
void webServerRecvCb(void *arg, char *data, unsigned short length);
void webServerSentCb(void *arg);
void webServerDisconCb(void *arg);
void webServerReconCb(void *arg, sint8 err);

#endif //_MESH_WEB_SERVER_H_
