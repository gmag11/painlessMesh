#ifndef   _PAINLESS_MESH_CONNECTION_H_
#define   _PAINLESS_MESH_CONNECTION_H_

#define _TASK_STD_FUNCTION
#include <painlessScheduler.h>

#include "espInterface.h"

#include<string>

/** 
 * \brief ReceivedBuffer handles pbuf pointers.
 *
 * Behaviour:
 * When a split is encountered, it will store all the preceding text into the jsonObjects. 
 * The pbuffer is copied.
 */
class ReceiveBuffer {
    public:
        String buffer;
        SimpleList<String> jsonStrings;

        ReceiveBuffer();
        
        void push(const char * cstr, size_t length);
        void push(pbuf *p);

        String front();
        void pop_front();

        bool empty();
        void clear();
};

class SentBuffer {
    public:
        char* buffer;
        char* current_ptr;
        size_t buffer_length = 0;
        size_t total_buffer_length = 0;
        size_t last_read_size = 0;

        SimpleList<String> jsonStrings;

        SentBuffer(size_t defaultSize = 200);
        
        void push(String &message, bool priority = false);

        size_t requestLength();

        char* read(size_t length);

        void freeRead();

        bool empty();
        void clear();
};

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

        bool                addMessage(String &message, bool priority = false);
        bool                writeNext();
        bool                sendReady = true;
        SimpleList<String>  sendQueue;
        ReceiveBuffer       receiveBuffer;

        Task nodeTimeoutTask;
        Task nodeSyncTask;
        Task timeSyncTask;
        Task readBufferTask;

        MeshConnection(tcp_pcb *tcp, painlessMesh *pMesh, bool station);
        ~MeshConnection();

        void handleMessage(String &msg, uint32_t receivedAt);

        void close(bool close_pcb = true);
        friend class painlessMesh;
};
#endif
