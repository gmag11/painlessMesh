#include<map>

#include "painlessMesh.h"
using namespace painlessmesh;

typedef std::function<void(String &from, String &msg)> namedReceivedCallback_t;

class namedMesh : public painlessMesh {
    public:
        namedMesh() {
          auto cb = [this](uint32_t from, String &msg) {
          // Try to parse it.. Need to test it with non json function
#if ARDUINOJSON_VERSION_MAJOR==6
            DynamicJsonDocument jsonBuffer(1024 + msg.length());
            deserializeJson(jsonBuffer, msg);
            JsonObject root = jsonBuffer.as<JsonObject>();
#else
            DynamicJsonBuffer jsonBuffer;
            JsonObject &root = jsonBuffer.parseObject(msg);
#endif
            if (root.containsKey("topic") &&
                String("nameBroadCast").equals(root["topic"].as<String>())) {
              nameMap[from] = root["name"].as<String>();
            } else {
              if (userReceivedCallback)
                // If needed send it on to userReceivedCallback
                userReceivedCallback(from, msg);
              if (userNamedReceivedCallback) {
                String name;
                // If needed look up name and send it on to
                // userNamedReceivedCallback
                if (nameMap.count(from) > 0) {
                  name = nameMap[from];
                } else {
                  name = String(from);
                }
                userNamedReceivedCallback(name, msg);
              }
            }
          };
          painlessMesh::onReceive(cb);
          changedConnectionCallbacks.push_back([this](uint32_t id) {
            if (nameBroadCastTask.isEnabled())
              nameBroadCastTask.forceNextIteration();
          });
        }

        String getName() {
            return nodeName;
        }

        void setName(String &name) {
            nodeName = name;
            // Start broadcast task if not done yet
            if (!nameBroadCastInit) {
                // Initialize
                nameBroadCastTask.set(5*TASK_MINUTE, TASK_FOREVER,
                        [this]() {
                            String msg;
                            // Create arduinoJson msg
#if ARDUINOJSON_VERSION_MAJOR==6
                            DynamicJsonDocument jsonBuffer(1024);
                            JsonObject root = jsonBuffer.to<JsonObject>();
                            root["topic"] = "nameBroadCast";
                            root["name"] = this->getName();
                            serializeJson(root, msg);
#else
                            DynamicJsonBuffer jsonBuffer;
                            JsonObject& root = jsonBuffer.createObject();
                            root["topic"] = "nameBroadCast";
                            root["name"] = this->getName();
                            root.printTo(msg);
#endif
                            this->sendBroadcast(msg);
                        }
                );
                // Add it
                mScheduler->addTask(nameBroadCastTask);
                nameBroadCastTask.enableDelayed();

                nameBroadCastInit = true;
            }
            nameBroadCastTask.forceNextIteration();
        }

        using painlessMesh::sendSingle;
        bool sendSingle(String &name, String &msg) {
            // Look up name
            for (auto && pr : nameMap) {
                if (name.equals(pr.second)) {
                    uint32_t to = pr.first;
                    return painlessMesh::sendSingle(to, msg);
                }
            }
            return false;
        }

        virtual void stop() {
            nameBroadCastTask.disable();
            mScheduler->deleteTask(nameBroadCastTask);
            painlessMesh::stop();
        }

        virtual void onReceive(receivedCallback_t onReceive) {
            userReceivedCallback = onReceive;
        }
        void onReceive(namedReceivedCallback_t  onReceive) {
            userNamedReceivedCallback = onReceive;
        }
    protected:
        String nodeName;
        std::map<uint32_t, String> nameMap;

        receivedCallback_t              userReceivedCallback;
        namedReceivedCallback_t         userNamedReceivedCallback;

        bool nameBroadCastInit = false;
        Task nameBroadCastTask;
};
