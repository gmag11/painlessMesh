#ifndef   _PAINLESS_MESH_CONNECTION_H_
#define   _PAINLESS_MESH_CONNECTION_H_

#define _TASK_PRIORITY // Support for layered scheduling priority
#define _TASK_STD_FUNCTION
#include <TaskSchedulerDeclarations.h>

#ifdef ESP32
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif // ESP32

#include<string>

// Temporary buffer used by ReceiveBuffer and SentBuffer
struct temp_buffer_t {
    size_t length = TCP_MSS;
    char buffer[TCP_MSS];
};

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
        std::list<String> jsonStrings;

        ReceiveBuffer();
        
        void   push(const char * cstr, size_t length, temp_buffer_t &buf);

        String front();
        void   pop_front();

        bool   empty();
        void   clear();
};

class SentBuffer {
    public:
        size_t last_read_size = 0;

        std::list<String> jsonStrings;

        SentBuffer();
        
        void   push(String &message, bool priority = false);

        size_t requestLength(size_t buffer_size);

        void   read(size_t length, temp_buffer_t &buf);

        void   freeRead();

        bool   empty();
        void   clear();

        bool   clean = true;
};

class MeshConnection {
    public:
        AsyncClient   *client;
        painlessMesh  *mesh;
        uint32_t      nodeId = 0;
        String        subConnections = "[]";
        timeSync      time;
        bool          newConnection = true;
        bool          connected = true;
        bool          station = true;

        uint32_t      timeDelayLastRequested = 0;   //Timestamp to be compared in manageConnections() to check response for timeout

        bool          addMessage(String &message, bool priority = false);
        bool          writeNext();
        ReceiveBuffer receiveBuffer;
        SentBuffer    sentBuffer;

        Task          nodeSyncTask;
        Task          timeSyncTask;
        Task          readBufferTask;
        Task          sentBufferTask;

        // Is this connection a root or rooted
        bool root = false;
        bool rooted = false;

#ifdef UNITY // Facilitate testing
        MeshConnection() {};
#endif
        MeshConnection(AsyncClient *client, painlessMesh *pMesh, bool station);
#ifndef UNITY
        ~MeshConnection();
#endif

        void handleMessage(String &msg, uint32_t receivedAt);

        void close();
        
        friend class painlessMesh;
};
#endif
