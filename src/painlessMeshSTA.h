#ifndef   _PAINLESS_MESH_STA_H_
#define   _PAINLESS_MESH_STA_H_

#define _TASK_STD_FUNCTION
#include <painlessScheduler.h>

#define SCAN_INTERVAL       10000 // AP scan period in ms

class painlessMesh;

class StationScan {
  public:
    Task task; // Station scanning for connections

    StationScan() {}
    void init(painlessMesh *pMesh, String &ssid, String &password, 
            uint16_t port);

    void stationScan();
    void scanComplete(bss_info *bssInfo);
    void filterAPs();
    void connectToAP();

  private:
    String ssid;
    String password;
    painlessMesh *mesh;
    uint16_t port;
    SimpleList<bss_info> aps;

    void requestIP(bss_info* ap);

    // Manually configure network and ip 
    bool manual = false; 
    uint8_t manualIP[4] = {0, 0, 0, 0};
    friend class painlessMesh;
};

#endif
