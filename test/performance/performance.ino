//************************************************************
// this is a simple example that uses the easyMesh library
//
// 1. blinks led once for every node on the mesh
// 2. blink cycle repeats every BLINK_PERIOD
// 3. sends a silly message to every node on the mesh at a random time between 1
// and 5 seconds
// 4. prints anything it receives to Serial.print
//
//
//************************************************************
#include <painlessMesh.h>

#include "painlessmesh/ota.hpp"
#include "painlessmesh/protocol.hpp"
#include "plugin/performance.hpp"

#define MESH_SSID "otatest"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555

using namespace painlessmesh;

painlessMesh mesh;


void setup() {
  Serial.begin(115200);

  mesh.setDebugMsgTypes(
      ERROR | CONNECTION |
      DEBUG);  // set before init() so that you can see error messages

  mesh.init(MESH_SSID, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, 6);
  mesh.initOTA("performance");
  plugin::performance::begin(mesh, 2);
}

void loop() { mesh.update(); }
