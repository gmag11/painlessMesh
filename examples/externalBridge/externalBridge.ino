//************************************************************
// this is a simple example that uses the painlessMesh library to
// connect to another network.
// for more details on the original branch of the project
// https://gitlab.com/BlackEdder/painlessMesh/wikis/bridge-between-mesh-and-another-network
// for more details on my version
// https://gitlab.com/Assassyn__V/painlessMesh
//************************************************************
//Modification in painlessMesh
//in painlessMesh.cpp -> painlessMesh::init
//in painlessMeshConnection.cpp -> painlessMesh::wifiEventCb (added some if statements)
//in painlessMesh.h, added a variable

#include "painlessMesh.h"
#include "IPAddress.h"

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

#define   STATION_SSID     "SFR_D240"
#define   STATION_PASSWORD "eterseleyervolle5den"

#define HOSTNAME "BRIDGE"

painlessMesh  mesh;
IPAddress myIP(0,0,0,0);

void setup() {
  Serial.begin(115200);

  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, AP_ONLY, AUTH_WPA2_PSK, 11 );
  mesh.onReceive(&receivedCallback);

  //now intiialize wifi station thanks to the esp8266 sdk
  mesh.stationManual(STATION_SSID, STATION_PASSWORD);
  mesh.setHostname(HOSTNAME);

}

void loop() {
  mesh.update();
  if(myIP != getlocalIP()){
    myIP = getlocalIP();
    Serial.println("My IP is " + myIP.toString());
    Serial.println("My channel is " + String(wifi_get_channel()));
  }
}

void receivedCallback( const uint32_t &from, const String &msg ) {
  Serial.printf("bridge: Received from %u msg=%s\n", from, msg.c_str());
}

IPAddress getlocalIP() {
  return IPAddress(mesh.getStaIp().ip.addr);
}
