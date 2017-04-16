#ifndef   _PAINLESS_MESH_STA_H_
#define   _PAINLESS_MESH_STA_H_

#include <painlessScheduler.h>

#define SCAN_INTERVAL       10000 // AP scan period in ms

class painlessMesh;

class StationScan {
  public:
    Task task; // Station scanning for connections

    StationScan() {}
    void init(painlessMesh *pMesh, String &ssid, String &password, 
            uint8_t channel);

    void stationScan();
    void scanComplete(bss_info *bssInfo);
    void filterAPs();
    void connectToAP();
  private:
    String ssid;
    String password;
    painlessMesh *mesh;
    uint8_t channel;
    SimpleList<bss_info> aps;

};

#endif
