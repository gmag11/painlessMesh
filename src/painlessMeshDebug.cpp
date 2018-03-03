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
void ICACHE_FLASH_ATTR painlessMesh::debugMsg(debugType type, const char* format ...) {
    if (type & types) {  //Print only the message types set for output
        va_list args;
        va_start(args, format);

        vsnprintf(str, 200, format, args);
        //perror(str);

        if (types && MSG_TYPES)
            Serial.printf("0x%x\t", type);

        Serial.print(str);

        va_end(args);
    }
}
