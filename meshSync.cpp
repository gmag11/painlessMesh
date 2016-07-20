#include <Arduino.h>
#include <SimpleList.h>
#include <ArduinoJson.h>

extern "C" {
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "uart.h"
    
#include "c_types.h"
#include "espconn.h"
#include "mem.h"
}

#include "meshSync.h"

uint32_t timeAdjuster = 0;

uint32_t getNodeTime( void ) {
    return system_get_time() + timeAdjuster;
}

String timeSync::buildTimeStamp( void ) {
//    Serial.printf("buildTimeStamp(): num=%d\n", num);
    
    if ( num > TIME_SYNC_CYCLES )
        Serial.printf("buildTimeStamp(): timeSync not started properly\n");
    
    StaticJsonBuffer<75> jsonBuffer;
    JsonObject& timeStampObj = jsonBuffer.createObject();
    times[num] = getNodeTime();
    timeStampObj["time"] = times[num];
    timeStampObj["num"] = num;
    bool remoteAdopt = !adopt;
    timeStampObj["adopt"] = remoteAdopt;
    
    String timeStampStr;
    timeStampObj.printTo( timeStampStr );
    
//    Serial.printf("buildTimeStamp(): timeStamp=%s\n", timeStampStr.c_str() );
    return timeStampStr;
}

bool timeSync::processTimeStamp( String &str ) {
//    Serial.printf("processTimeStamp(): str=%s\n", str.c_str());
    
    DynamicJsonBuffer jsonBuffer(50 );
    JsonObject& timeStampObj = jsonBuffer.parseObject(str);
    
    if ( !timeStampObj.success() ) {
        Serial.printf("processTimeStamp(): out of memory1?\n" );
        return false;
    }

    num = timeStampObj.get<uint32_t>("num");
    
    times[num] = timeStampObj.get<uint32_t>("time");
    adopt = timeStampObj.get<bool>("adopt");
    
    num++;
    
    if ( num < TIME_SYNC_CYCLES ) {
        str = buildTimeStamp();
        return true;
    }
    else {
        return false;
    }
}

void timeSync::calcAdjustment ( bool odd ) {
//    Serial.printf("calcAdjustment(): odd=%u\n", odd);

    uint32_t    bestInterval = 0xFFFFFFFF;
    uint8_t     bestIndex;
    uint32_t    temp;

    for (int i = 0; i < TIME_SYNC_CYCLES; i++) {
  //      Serial.printf("times[%d]=%u\n", i, times[i]);
        
        if ( i % 2 == odd ) {
            temp = times[i + 2] - times[i];
            
            if ( i < TIME_SYNC_CYCLES - 2 ){
    //            Serial.printf("\tinterval=%u\n", temp);
                
                if ( temp < bestInterval ) {
                    bestInterval = temp;
                    bestIndex = i;
                }
            }
        }
    }
//    Serial.printf("best interval=%u, best index=%u\n", bestInterval, bestIndex);
    
    // find number that turns local time into remote time
    uint32_t adopterTime = times[ bestIndex ] + (bestInterval / 2);
    uint32_t adjustment = times[ bestIndex + 1 ] - adopterTime;
    
 //   Serial.printf("new calc time=%u, adoptedTime=%u\n", adopterTime + adjustment, times[ bestIndex + 1 ]);

    timeAdjuster += adjustment;
}


