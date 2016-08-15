easyMesh readme

easyMesh is a library that takes care of the particulars for creating a simple mesh network using Arduino and esp8266.  The goal is to allow the programmer to work with a mesh network without having to worry about how the network is structured or managed.  

easyMesh is a true ad-hoc network, meaning that no-planning, central controller, or router is required.  Any system of 1 or more nodes will self-organize into fully functional mesh.  The maximum size of the mesh is limited (i think) by the amount of memory in the heap that can be allocated to the sub-connections buffer… and so should be really quite high.

easyMesh uses JSON objects for all its messaging.  There a couple of reasons for this.  First, it makes the code and the messages human readable and easy to understand and second, it makes it easy to integrate easyMesh with javascript front-ends, web applications, and other apps.  Some performance is lost, but I haven’t been running into performance issues yet.  Converting to binary messaging would be fairly straight forward if someone wants to contribute.

Wifi & Networking
easyMesh is designed to be used with Arduino, but it does not use the Arduino WiFi libraries, as I was running into performance issues (primarily latency) with them.  Rather the networking is all done using the native esp8266 SDK libraries, which are available through the Arduino IDE.  Hopefully though, which networking libraries are used won’t matter to most users much as you can just include the .h, run the init() and then work the library through the API.

easyMesh is not IP networking.
easyMesh does not create a TCP/IP network of nodes. Rather each of the nodes is uniquely identified by its 32bit chipId which is retrieved from the esp8266 using the system_get_chip_id() call in the SDK.  Every esp8266 will have a unique number.  Messages can either be broadcast to all of the nodes on the mesh, or sent specifically to an individual node which is identified by its chipId.

Examples
demoToy is currently the only example.  It is kind of complex, uses a web server, web sockets, and neopixel animations, so it is not really a great entry level example.  That said, it does some pretty cools stuff… here is a video of the demo.

https://www.youtube.com/watch?v=hqjOY8YHdlM&feature=youtu.be

More to come!
