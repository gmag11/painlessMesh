# Intro to painlessMesh

painlessMesh is a library that takes care of the particulars of creating a simple mesh network using Arduino and esp8266.  The goal is to allow the programmer to work with a mesh network without having to worry about how the network is structured or managed.

### True ad-hoc networking

painlessMesh is a true ad-hoc network, meaning that no-planning, central controller, or router is required.  Any system of 1 or more nodes will self-organize into fully functional mesh.  The maximum size of the mesh is limited (we think) by the amount of memory in the heap that can be allocated to the sub-connections buffer… and so should be really quite high.

### JSON based

painlessMesh uses JSON objects for all its messaging.  There are a couple of reasons for this.  First, it makes the code and the messages human readable and painless to understand and second, it makes it painless to integrate painlessMesh with javascript front-ends, web applications, and other apps.  Some performance is lost, but I haven’t been running into performance issues yet.  Converting to binary messaging would be fairly straight forward if someone wants to contribute.

### Wifi &amp; Networking

painlessMesh is designed to be used with Arduino, but it does not use the Arduino WiFi libraries, as I was running into performance issues (primarily latency) with them.  Rather the networking is all done using the native esp8266 SDK libraries, which are available through the Arduino IDE.  Hopefully though, which networking libraries are used won’t matter to most users much as you can just include the .h, run the init() and then work the library through the API.

### painlessMesh is not IP networking

painlessMesh does not create a TCP/IP network of nodes. Rather each of the nodes is uniquely identified by its 32bit chipId which is retrieved from the esp8266 using the `system_get_chip_id()` call in the SDK.  Every esp8266 will have a unique number.  Messages can either be broadcast to all of the nodes on the mesh, or sent specifically to an individual node which is identified by its `nodeId`.

### Examples

StartHere is a basic how to use example. It blinks built-in LED (in ESP-12) as many times as nodes are connected to the mesh. Further examples are under the examples directory and shown on the platformio [page](http://platformio.org/lib/show/1269/painlessMesh).

### Dependencies

painlessMesh makes use of the following library, which can be installed through the Arduino Library Manager

- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

If platformio is used to install the library, then the dependency will be installed automatically.

# Getting help

There is help available on the [wiki](https://gitlab.com/BlackEdder/painlessMesh/wikis/home) and you can also reach us on our [gitter channel](https://gitter.im/painlessMesh/Lobby)

# painlessMesh API

Using painlessMesh is painless!

First include the library and create an painlessMesh object like this…

```
#include <painlessMesh.h>
painlessMesh  mesh;
```

## Member Functions

### void painlessMesh::init(String ssid, String password, uint16_t port = 5555, enum nodeMode connectMode = STA_AP, _auth_mode authmode = AUTH_WPA2_PSK, uint8_t channel = 1, phy_mode_t phymode = PHY_MODE_11G, uint8_t maxtpw = 82, uint8_t hidden = 0, uint8_t maxconn = 4)

Add this to your setup() function.
Initialize the mesh network.  This routine does the following things.

- Starts a wifi network
- Begins searching for other wifi networks that are part of the mesh
- Logs on to the best mesh network node it finds… if it doesn’t find anything, it starts a new search in 5 seconds.

`ssid` = the name of your mesh.  All nodes share same AP ssid. They are distinguished by BSSID.
`password` = wifi password to your mesh.
`port` = the TCP port that you want the mesh server to run on. Defaults to 5555 if not specified.
[`connectMode`](https://gitlab.com/BlackEdder/painlessMesh/wikis/connect-mode:-ap_only,-sta_only,-sta_ap-mode) = switch between AP_ONLY, STA_ONLY and STA_AP (default) mode

### void painlessMesh::update( void )

Add this to your loop() function
This routine runs various maintainance tasks... Not super interesting, but things don't work without it.

### void painlessMesh::onReceive( &amp;receivedCallback )

Set a callback routine for any messages that are addressed to this node. Callback routine has the following structure.

`void receivedCallback( uint32_t from, String &amp;msg )`

Every time this node receives a message, this callback routine will the called.  “from” is the id of the original sender of the message, and “msg” is a string that contains the message.  The message can be anything.  A JSON, some other text string, or binary data.

### void painlessMesh::onNewConnection( &amp;newConnectionCallback )

This fires every time the local node makes a new connection.   The callback has the following structure.

`void newConnectionCallback( uint32_t nodeId )`

`nodeId` is new connected node ID in the mesh.

### void painlessMesh::onChangedConnections( &amp;changedConnectionsCallback )

This fires every time there is a change in mesh topology. Callback has the following structure.

`void onChangedConnections()`

There are no parameters passed. This is a signal only.

### bool painlessMesh::isConnected( nodeId )

Returns if a given node is currently connected to the mesh.

`nodeId` is node ID that the request refers to.

### void painlessMesh::onNodeTimeAdjusted( &amp;nodeTimeAdjustedCallback )

This fires every time local time is adjusted to synchronize it with mesh time. Callback has the following structure.

`void onNodeTimeAdjusted(int32_t offset)`

`offset` is the adjustment delta that has benn calculated and applied to local clock.

### void onNodeDelayReceived(nodeDelayCallback_t onDelayReceived)

This fires when a time delay masurement response is received, after a request was sent. Callback has the following structure.

`void onNodeDelayReceived(uint32_t nodeId, int32_t delay)`

`nodeId` The node that originated response.

`delay` One way network trip delay in nanoseconds.

### bool painlessMesh::sendBroadcast( String &amp;msg)

Sends msg to every node on the entire mesh network.

returns true if everything works, false if not.  Prints an error message to Serial.print, if there is a failure.

### bool painlessMesh::sendSingle(uint32_t dest, String &amp;msg)

Sends msg to the node with Id == dest.

returns true if everything works, false if not.  Prints an error message to Serial.print, if there is a failure.

### String painlessMesh::subConnectionJson()

Returns mesh topology in JSON format.

### SimpleList<uint32_t> painlessMesh::getNodeList()

Get a list of all known nodes. This includes nodes that are both directly and indirectly connected to the current node.

### uint32_t painlessMesh::getNodeId( void )

Return the chipId of the node that we are running on.

### uint32_t painlessMesh::getNodeTime( void )

Returns the mesh timebase microsecond counter. Rolls over 71 minutes from startup of the first node.

Nodes try to keep a common time base synchronizing to each other using [an SNTP based protocol](https://gitlab.com/BlackEdder/painlessMesh/wikis/mesh-protocol#time-sync)

### bool painlessMesh::startDelayMeas(uint32_t nodeId)

Sends a node a packet to measure network trip delay to that node. Returns true if nodeId is connected to the mesh, false otherwise. After calling this function, user program have to wait to the response in the form of a callback specified by `void painlessMesh::onNodeDelayReceived(nodeDelayCallback_t onDelayReceived)`.

nodeDelayCallback_t is a funtion in the form of `void (uint32_t nodeId, int32_t delay)`.