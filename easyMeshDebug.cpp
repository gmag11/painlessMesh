//
//  easyMeshDebug.cpp
//  
//
//  Created by Bill Gray on 8/18/16.
//
//

#include <Arduino.h>
#include <stdarg.h>

#include "easyMesh.h"

uint16_t types = 0;

void easyMesh::setDebugMsgTypes( uint16_t newTypes ) {
    // set the different kinds of debug messages you want to generate.
    types = newTypes;
    Serial.printf("setDebugTypes 0x%x\n", types);
}

void easyMesh::debugMsg( debugType type, const char* format ... ) {
    
    if ( type & types ) {  //Print only the message types set for output
        char str[200];
        
        va_list args;
        va_start(args, format);
        
        vsnprintf(str, sizeof(str), format, args);
    
        if ( types && MSG_TYPES)
            Serial.printf("0x%x\t", type, types );
        
        Serial.print( str );
        
        va_end(args);
    }
}

