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
//char *str = NULL;

void ICACHE_FLASH_ATTR painlessMesh::setDebugMsgTypes(uint16_t newTypes) {
    // set the different kinds of debug messages you want to generate.
    types = newTypes;
    Serial.printf("\nsetDebugTypes 0x%x\n", types);
    //if (!str)
    //    str = (char*) malloc(200 * sizeof(char));
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
                    Serial.print("ERROR: ");
                    break;
                case STARTUP:
                    Serial.print("STARTUP: ");
                    break;
                case MESH_STATUS:
                    Serial.print("MESH_STATUS: ");
                    break;
                case CONNECTION:
                    Serial.print("CONNECTION: ");
                    break;
                case SYNC:
                    Serial.print("SYNC: ");
                    break;
                case S_TIME:
                    Serial.print("S_TIME: ");
                    break;
                case COMMUNICATION:
                    Serial.print("COMMUNICATION: ");
                    break;
                case GENERAL:
                    Serial.print("GENERAL: ");
                    break;
                case MSG_TYPES:
                    Serial.print("MSG_TYPES: ");
                    break;
                case REMOTE:
                    Serial.print("REMOTE: ");
                    break;
                case APPLICATION:
                    Serial.print("APPLICATION: ");
                    break;
                case DEBUG:
                    Serial.print("DEBUG: ");
                    break;
            }
        }

        Serial.print(str);

        va_end(args);
    }
}
