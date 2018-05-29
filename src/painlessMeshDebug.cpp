//
//  painlessMeshDebug.cpp
//  
//
//  Created by Bill Gray on 8/18/16.
//
//

#include <Arduino.h>
#include <stdarg.h>

#include "painlessMesh.h"

uint16_t types = 0;
char str[200];

void ICACHE_FLASH_ATTR painlessMesh::setDebugMsgTypes(uint16_t newTypes) {
    // set the different kinds of debug messages you want to generate.
    types = newTypes;
    Serial.print(F("\nsetDebugTypes:"));
        if(types & ERROR)
        {
            Serial.print(F(" ERROR |"));
        }
        if (types & STARTUP)
        {
            Serial.print(F(" STARTUP |"));
        }
        if (types & MESH_STATUS)
        {
            Serial.print(F(" MESH_STATUS |"));
        }
        if (types & CONNECTION)
        {
            Serial.print(F(" CONNECTION |"));
        }
        if (types & SYNC)
        {
            Serial.print(F(" SYNC |"));
        }
        if (types & S_TIME)
        {
            Serial.print(F(" S_TIME |"));
        }
        if (types & COMMUNICATION)
        {
            Serial.print(F(" COMMUNICATION |"));
        }
        if (types & GENERAL)
        {
            Serial.print(F(" GENERAL |"));
        }
        if (types & MSG_TYPES)
        {
            Serial.print(F(" MSG_TYPES |"));
        }
        if (types & REMOTE)
        {
            Serial.print(F(" REMOTE |"));
        }
        if (types & APPLICATION)
        {
            Serial.print(F(" APPLICATION |"));
        }
        if (types & DEBUG)
        {
            Serial.print(F(" DEBUG |"));
        }
    Serial.println();
    return;
}

// To assign a debug message to several type use | (bitwise or) operator
// Example: debugMsg( GENERAL | CONNECTION , "Debug message");
void ICACHE_FLASH_ATTR painlessMesh::debugMsg(debugType_t type, const char* format ...) {
    if (type & types) {  //Print only the message types set for output
        va_list args;
        va_start(args, format);

        vsnprintf(str, 200, format, args);
        //perror(str);

        if (types && MSG_TYPES)
        {
            switch(type)
            {
                case ERROR:
                    Serial.print(F("ERROR: "));
                    break;
                case STARTUP:
                    Serial.print(F("STARTUP: "));
                    break;
                case MESH_STATUS:
                    Serial.print(F("MESH_STATUS: "));
                    break;
                case CONNECTION:
                    Serial.print(F("CONNECTION: "));
                    break;
                case SYNC:
                    Serial.print(F("SYNC: "));
                    break;
                case S_TIME:
                    Serial.print(F("S_TIME: "));
                    break;
                case COMMUNICATION:
                    Serial.print(F("COMMUNICATION: "));
                    break;
                case GENERAL:
                    Serial.print(F("GENERAL: "));
                    break;
                case MSG_TYPES:
                    Serial.print(F("MSG_TYPES: "));
                    break;
                case REMOTE:
                    Serial.print(F("REMOTE: "));
                    break;
                case APPLICATION:
                    Serial.print(F("APPLICATION: "));
                    break;
                case DEBUG:
                    Serial.print(F("DEBUG: "));
                    break;
            }
        }

        Serial.print(str);

        va_end(args);
    }
}
