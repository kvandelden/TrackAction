
#include <stdlib.h>
#include "messagebus.h"
#include <string.h>

#define DEBUGMB 0

extern "C" struct MessageBus *createMessageBus(uint32_t    bufferSize, 
                                               const char *startDelimiter, 
                                               const char *endDelimiter,
                                               bool        trimCRLF)
{
    struct MessageBus *rv = (struct MessageBus *) malloc(sizeof(struct MessageBus));

    rv->bufferSize = bufferSize;
    rv->buffer = (uint8_t *) malloc(sizeof(uint8_t) * (bufferSize + 5));
    rv->bufferAux = (uint8_t *) malloc(sizeof(uint8_t) * (bufferSize + 5));
    rv->startDelimiter = startDelimiter;
    rv->endDelimiter = endDelimiter;
    rv->trimCRLF = trimCRLF;
    MB_reset(rv);
    return rv;
};

extern "C" void MB_free(struct MessageBus *mb){
    free(mb->buffer);
    free(mb->bufferAux);
    mb->bufferSize = 0;
}

extern "C" void MB_reset(struct MessageBus *mb){
    for(uint32_t i = 0; i < mb->bufferSize + 5;i++){
        mb->buffer[i] = 0;
        mb->buffer[i] = 0;
    }
    mb->startIdx = 0;
    mb->dataLength = 0;
    mb->overflowCount = 0;
}

extern "C" void MB_realignBuffer(struct MessageBus *mb){
    if(mb->startIdx == 0)
        return;

    memset(mb->bufferAux,0,mb->bufferSize + 5);
    uint32_t newDataIndexStartIdx = (mb->startIdx + mb->dataLength) % mb->bufferSize;

    uint8_t *start = &(mb->buffer[mb->startIdx]);
    bool bSplitWrite = (newDataIndexStartIdx) >= mb->bufferSize;
    if(bSplitWrite){
        auto headLength = mb->bufferSize - 1 - newDataIndexStartIdx;
        memcpy(mb->bufferAux, start, headLength);
        memcpy(&(mb->bufferAux[headLength]), mb->buffer, mb->bufferSize - headLength);
    } else {
        // Single Write
        memcpy(mb->bufferAux, start, mb->dataLength);        
    }

    auto swap = mb->buffer;
    mb->buffer = mb->bufferAux;
    mb->bufferAux = swap;
    mb->startIdx = 0;

}

extern "C" void MB_add(struct MessageBus *mb, char *str){
    if(str != NULL){
        auto len = strlen(str);
        MB_addData(mb, len, (uint8_t *)str);
    }
}

extern "C" void MB_addData(struct MessageBus *mb, 
                           uint32_t  dataLen, 
                           uint8_t *data)
{

    auto dataSize = dataLen;

    if (mb->trimCRLF)
    {
        uint8_t *src = data, *dst = data;
        
        for (uint32_t i = 0; i < dataLen; i++)
        {
            *dst = *src;
            if ((*dst != '\r') && (*dst != '\n'))
                dst++;
            else
                dataSize--;
            src++;
        }
        *dst = '\0';
    }

    if(dataSize == 0)
        return;

    if((mb->dataLength + dataSize) > mb->bufferSize){
        mb->overflowCount++;
        return;
    }

    uint32_t newDataIndexStartIdx = (mb->startIdx + mb->dataLength) % mb->bufferSize;

    uint8_t *start = &(mb->buffer[newDataIndexStartIdx]);
    bool bSplitWrite = (newDataIndexStartIdx + dataSize) >= mb->bufferSize;
    if(bSplitWrite){
        auto headLength = mb->bufferSize - 1 - newDataIndexStartIdx;
        memcpy(start, data, headLength);
        memcpy(mb->buffer, &(data[headLength]), dataSize - headLength);
        mb->dataLength += dataSize;
    } else {
        // Single Write
        memcpy(start, data, dataSize);
        mb->dataLength += dataSize;
        
    }


}

extern "C" std::vector<uint8_t *> MB_getMessages(struct MessageBus *mb){
    std::vector<uint8_t *> rv;
    // From the startIdx, search for the startDelimiter    
    //
    //
    // We can use string functions if we realign data to startIdx= 0
    MB_realignBuffer(mb);
    
    
    while(true){
        if(mb->dataLength == 0)
            return rv;
        auto searchPtr = &(mb->buffer[mb->startIdx]);
        auto startPtr = strstr((char *) searchPtr, mb->startDelimiter);
        if(!startPtr){
            MB_reset(mb);
            return rv;
        }

        // Found starting delimeter
        // Find the end delimiter
        
        auto endPtr = strstr((char *)startPtr, mb->endDelimiter);
        if(!endPtr){
            //  End not found yet, just leave in buffer
            return rv;
        }

        // create message string
        auto endDelimeterLength = strlen(mb->endDelimiter);
        auto len = (endPtr - startPtr) + endDelimeterLength;
        uint8_t *msg = (uint8_t *) malloc(sizeof(uint8_t) * (len + 1));
        memcpy(msg, startPtr, len);
        msg[len] = '\0';
        // push start idx forward
        //memset(startPtr, 0, len); // Zero?
        mb->startIdx += len;
        mb->dataLength -= len;
        rv.push_back(msg);

    }

    return rv;
}





