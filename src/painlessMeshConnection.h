#ifndef   _PAINLESS_MESH_CONNECTION_H_
#define   _PAINLESS_MESH_CONNECTION_H_

#define _TASK_STD_FUNCTION
#include <painlessScheduler.h>

#include "espInterface.h"

class MeshConnection {
    public:
        tcp_pcb             *pcb;
        painlessMesh        *mesh;
        uint32_t            nodeId = 0;
        String              subConnections;
        timeSync            time;
        bool                newConnection = true;
        bool                connected = true;
        bool                station = true;

        uint32_t            timeDelayLastRequested = 0; // Timestamp to be compared in manageConnections() to check response for timeout

        bool                addMessage(String message, bool priority);
        bool                writeNext();
        SimpleList<String>  sendQueue;

        Task nodeTimeoutTask;
        Task nodeSyncTask;
        Task timeSyncTask;

        MeshConnection(tcp_pcb *tcp, painlessMesh *pMesh, bool station);
        ~MeshConnection();

        void close(bool close_pcb = true);
        friend class painlessMesh;
};
#endif
