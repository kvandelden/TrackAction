#include <stdint.h>
#include <vector>

struct MessageBus {
    
    uint32_t   bufferSize;
    uint8_t   *buffer;
    uint8_t   *bufferAux;
    uint32_t   startIdx;
    uint32_t   dataLength;

    uint32_t   overflowCount;

    const char      *startDelimiter;
    const char      *endDelimiter;
    bool         trimCRLF;

};





extern "C" struct MessageBus *createMessageBus(uint32_t  bufferSize, 
                                               const char *startDelimiter, 
                                               const char *endDelimiter,
                                               bool        trimCRLF);

extern "C" void MB_free(struct MessageBus *);

extern "C" void MB_reset(struct MessageBus *);
extern "C" void MB_add(struct MessageBus *mb, char *string);
extern "C" void MB_addData(struct MessageBus *mb, 
                           uint32_t  dataSize, 
                           uint8_t *data);
extern "C" std::vector<uint8_t *> MB_getMessages(struct MessageBus *mb);




