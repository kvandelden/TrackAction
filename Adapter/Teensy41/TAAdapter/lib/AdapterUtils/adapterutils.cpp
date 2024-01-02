
#include <vector>
#include <stdint.h>

#include "Arduino.h"
#include "adapterutils.h"
#include "jsmn.h"
#include "messagebus.h"

#define DEBUGTA 0

char   * adapterId;
uint32_t LowResClockHz = 0;
uint32_t HighResClockHz = 0;
uint32_t LowResTicks_HighResRollOver = 0;
void calcHighResRollOver();

uint32_t debounceMs = 0;
uint32_t debounceLowResTicks = 0;
void calcDebouncePeriod();

uint8_t  inputCount = 0;
struct InputSensorState *inputs = NULL;

uint32_t inputQueueSize = 0;
uint32_t inputQueueAddIdx = 0;
uint32_t inputQueueProcessIdx = 0;
uint32_t inputQueueOverrunCount = 0;

struct InputMessageQueueItem *inputQueue = NULL;
extern "C" double InputChange_getSeconds(InputMessageQueueItem *qi);

uint8_t outputCount = 0;
struct OutputSensorState *outputs = NULL;

struct MessageBus *messageBus;
uint32_t messageParseError = 0;
extern "C" char *TA_processCommandString(char *str, uint32_t lowResTick, uint32_t highResTick);


extern "C" void TAsetup_init(char *sAdapterId){
    //
    adapterId = sAdapterId;
     messageBus = createMessageBus(32*1024, "{","}", true);

}

extern "C" void TAsetup_setInputPortPins(uint8_t pins[], uint8_t count){
    inputCount = count;
    inputs = (InputSensorState *) malloc(inputCount * sizeof(InputSensorState));

    for(int idx = 0; idx < inputCount; idx++){
        inputs[idx].InputPin = pins[idx];
        inputs[idx].InputValue = 0;
        inputs[idx].LowResClockTick_Last_0 = 0;
        inputs[idx].HighResClockTick_Last_0 = 0;
        inputs[idx].LowResClockTick_Prev_0 = 0;
        inputs[idx].HighResClockTick_Prev_0 = 0;
        inputs[idx].LowResClockTick_Last_1 = 0;
        inputs[idx].HighResClockTick_Last_1 = 0;
        inputs[idx].LowResClockTick_Prev_1 = 0;
        inputs[idx].HighResClockTick_Prev_1 = 0;
        inputs[idx].IRQ_Count = 0;
        inputs[idx].IRQ_Debounce_Count = 0;
        inputs[idx].LowResClockTick_Last = 0;

        pinMode(pins[idx], INPUT);
    }

    #if DEBUGTA
    Serial.printf("Input Count: %d\n", inputCount);
    #endif
};

extern "C" void TAsetup_setOutputPortPins(uint8_t pins[], uint8_t count)
{
    outputCount = count;
    outputs = (OutputSensorState *)malloc(outputCount * sizeof(OutputSensorState));

    for (int idx = 0; idx < outputCount; idx++)
    {
        outputs[idx].OutputPin = pins[idx];
        outputs[idx].OutputValue = 0;
        outputs[idx].LowResClockTick_Last_0 = 0;
        outputs[idx].HighResClockTick_Last_0 = 0;
        outputs[idx].LowResClockTick_Last_1 = 0;
        outputs[idx].HighResClockTick_Last_1 = 0;
        outputs[idx].Set_Count = 0;
        pinMode(pins[idx], OUTPUT);
    }

#if DEBUGTA
    Serial.printf("Output Count: %d\n", outputCount);
#endif
}

extern "C" void TAsetup_setLowResClockHz(uint32_t Hz){
    LowResClockHz = Hz;
    calcDebouncePeriod();
    calcHighResRollOver();
};

extern "C" void TAsetup_setHighResClockHz(uint32_t Hz){
    HighResClockHz = Hz;
    calcHighResRollOver();
};

void calcHighResRollOver(){
    if(LowResClockHz && HighResClockHz)
       LowResTicks_HighResRollOver = ((uint64_t) UINT32_MAX * LowResClockHz) /(uint64_t) HighResClockHz; 
}

extern "C" void TAsetup_setIRQDebounceTime(uint32_t millis){
    debounceMs = millis;
    calcDebouncePeriod();
};

 
 void calcDebouncePeriod(){
    if(LowResClockHz && debounceMs)
        debounceLowResTicks = debounceMs * LowResClockHz / 1000;

 };

extern "C" void ISR_inputSensor(uint8_t inputIdx, uint32_t lowResTick, uint32_t highResTick){
    if((inputIdx < 0) || (inputIdx >= inputCount))
        return;

    InputSensorState *p = &(inputs[inputIdx]);
    uint8_t inputValue = digitalReadFast(p->InputPin);
    if(inputValue == p->InputValue)
        return;

    // Check debounce
    if(debounceLowResTicks){
        uint32_t change = lowResTick - p->LowResClockTick_Last;
        if(change < debounceLowResTicks){
            p->IRQ_Debounce_Count++;
            return;
        };
    };

    p->IRQ_Count++;
    p->InputValue = inputValue;
    p->LowResClockTick_Last = lowResTick;

    if(inputValue){
        p->LowResClockTick_Prev_1 = p->LowResClockTick_Last_1;
        p->HighResClockTick_Prev_1 = p->HighResClockTick_Last_1;
        p->LowResClockTick_Last_1 = lowResTick;
        p->HighResClockTick_Last_1 = highResTick;
    } else {
        p->LowResClockTick_Prev_0 = p->LowResClockTick_Last_0;
        p->HighResClockTick_Prev_0 = p->HighResClockTick_Last_0;
        p->LowResClockTick_Last_0 = lowResTick;
        p->HighResClockTick_Last_0 = highResTick;
    };

    // Add to Send Queue 
    if(inputQueueSize && inputValue){
        InputMessageQueueItem *addItem = &(inputQueue[inputQueueAddIdx]);
        if((!addItem->bSent) && (addItem->bReadyToSend)){
            inputQueueOverrunCount++;
        } else {
            addItem->InputNumber = p->InputPin;
            addItem->InputValue = inputValue;
            addItem->LowResClockTick = p->LowResClockTick_Last_1;
            addItem->LowResClockTick_Prev = p->LowResClockTick_Prev_1;
            addItem->HighResClockTick = p->HighResClockTick_Last_1;
            addItem->HighResClockTick_Prev = p->HighResClockTick_Prev_1;
            addItem->bReadyToSend = true;
            addItem->bSent = false;
            inputQueueAddIdx = (inputQueueAddIdx + 1) % inputQueueSize;
        }
        
    }

    #if DEBUGTA
    Serial.printf("\nInput%d: %d\n", p->InputPin, inputValue);
    if(inputValue)
        Serial.printf("\nInput%d Time LowRes: %d   HighRes: %d\n", 
                     p->InputPin, 
                     p->LowResClockTick_Last_1 - p->LowResClockTick_Prev_1,
                     p->HighResClockTick_Last_1 - p->HighResClockTick_Prev_1);
    #endif
     
};

extern "C" void TAsetup_setInputQueueSize(uint32_t size) {
    inputQueueSize = size;
    inputQueue = (InputMessageQueueItem *) malloc(inputQueueSize * sizeof(InputMessageQueueItem));
    TA_resetInputQueue();
}

extern "C" void TA_resetInputQueue(){
    inputQueueAddIdx = 0;
    inputQueueProcessIdx = 0;
    for(uint32_t idx = 0; idx < inputQueueSize; idx++){
        inputQueue[idx].bSent = false;
        inputQueue[idx].bReadyToSend = false;
        inputQueue[idx].InputNumber = 0;
        inputQueue[idx].InputValue = 0;
        inputQueue[idx].LowResClockTick = 0;
        inputQueue[idx].LowResClockTick_Prev = 0;
        inputQueue[idx].HighResClockTick = 0;
        inputQueue[idx].HighResClockTick_Prev = 0;
    }

}

extern "C" void TA_resetInputSensorState(){

     for(int idx = 0; idx < inputCount; idx++){    
        inputs[idx].InputValue = 0;
        inputs[idx].LowResClockTick_Last_0 = 0;
        inputs[idx].HighResClockTick_Last_0 = 0;
        inputs[idx].LowResClockTick_Prev_0 = 0;
        inputs[idx].HighResClockTick_Prev_0 = 0;
        inputs[idx].LowResClockTick_Last_1 = 0;
        inputs[idx].HighResClockTick_Last_1 = 0;
        inputs[idx].LowResClockTick_Prev_1 = 0;
        inputs[idx].HighResClockTick_Prev_1 = 0;
    
        inputs[idx].LowResClockTick_Last = 0;
    }
}

extern "C" std::vector<char *> TA_getInputQueueMessages(){
    std::vector<char *> rv;

    if(!inputQueueSize)
        return rv;

    while(true){
        auto p = &(inputQueue[inputQueueProcessIdx]);
        if(p->bSent)
            break;
        if(!p->bReadyToSend)
            break;
        char *s = (char *) malloc(350);
        auto seconds = InputChange_getSeconds(p);
        
        char firstLapMsg[2][50] = {"",", FirstLap: true "};
        int firstLapMsgIdx = 0;

        if(seconds == 0)
          firstLapMsgIdx = 1;
        
        sprintf(s, "{\"Type\": \"InputChange\", \"Input\": %d, \"Value\": %d, \"Seconds\": %.8lf, \"LowResTick\": %lu, \"HighResTick\": %lu%s}",
            p->InputNumber,
            p->InputValue,
            seconds,
            p->LowResClockTick,
            p->HighResClockTick,
            firstLapMsg[firstLapMsgIdx]
        );
        rv.push_back(s);
        p->bSent = true;
        p->bReadyToSend = false;

        inputQueueProcessIdx = (inputQueueProcessIdx + 1) % inputQueueSize;
    };

    return rv;
}


extern "C" double InputChange_getSeconds(InputMessageQueueItem *qi) {
    double rv = 0;
    // The low resolution clock is intended to never role over
    // The high resolution clock rolls over quickly
    if(qi->LowResClockTick_Prev == 0)
        return 0;

    // Calculate using the Low Res Clock
    auto lowResTicks = (qi->LowResClockTick - qi->LowResClockTick_Prev);
    rv = ((double)lowResTicks)/(double)LowResClockHz;

    // Check if we can use the High Resolution Clock
    if(lowResTicks < LowResTicks_HighResRollOver){
        // Use High Resolution Timer
        if(qi->HighResClockTick > qi->HighResClockTick_Prev){
            rv= (qi->HighResClockTick - qi->HighResClockTick_Prev)/(double)HighResClockHz;
        } else {
            rv = (qi->HighResClockTick + UINT32_MAX - qi->HighResClockTick_Prev)/(double)HighResClockHz;
        }
    } else {
        // Possibly use the High resolution timer to determine the Lower Part of the fraction ?
    }


    return rv;
};


extern "C" std::vector<char *> TA_processCommandStream(char *str, uint32_t lowResTick, uint32_t highResTick){
    std::vector<char *> rv;

      auto len = strlen(str);  
      MB_addData(messageBus, len, (uint8_t *)str);
      

    auto messageStrings = MB_getMessages(messageBus);
    for(auto i = messageStrings.begin(); i != messageStrings.end(); ++i){
      char *s = (char *) *i;
    
      Serial.printf("Command: %s\n", s);
      auto resp = TA_processCommandString(s, lowResTick, highResTick);

        if(resp != NULL){
            // Add to return vector
            rv.push_back(resp);
        }

      Serial.printf("%s\n",s);
      free(*i);

    }

    return rv;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

extern "C" char *TA_processCommandString(char *str, uint32_t lowResTick, uint32_t highResTick){
    //--
    //- Command strings are JSON formatted.

     jsmn_parser jsonParser;
     jsmn_init(&jsonParser);
     jsmntok_t tokens[128]; /* We expect no more than 128 tokens */
    auto stringSize = strlen(str);
    auto tokenCount = jsmn_parse(&jsonParser, str, stringSize, tokens, 128);
    if(tokenCount < 0){
        messageParseError++;
        return NULL;
    }

    /* Assume the top-level element is an object */
    if (tokenCount < 1 || tokens[0].type != JSMN_OBJECT) {
        messageParseError++;
        return NULL;
    }

    // JSON Attributes storage for Known Attributes
    bool      typeFound = false;
    char      type[50];
    
    uint8_t   rtcYear, rtcMonth, rtcDay, rtcHours, rtcMinutes, rtcSeconds;
    
    bool      portsFound = false;
    uint8_t   ports[64];
    uint8_t   portCount = 0;

    bool      responseIdFound = false;
    char      responseId[50];
    char      responseAttrOut[256];

    char      inputValues[64];
    char      outputValues[64];
    char      outputPinValues[64];

    responseId[0] = '\0';
    responseAttrOut[0] = ' '; 
    responseAttrOut[1] = '\0'; 
    inputValues[0] = '\0'; 
    outputValues[0] = '\0'; 
    outputPinValues[0] = '\0'; 

    Serial.printf("Token Count %d\n", tokenCount);
    for (int i = 0; i < tokenCount; i++)
    {
        auto       curTok = &tokens[i];
        Serial.printf("Token Idx %d, Type: %d, St: %d, Ed: %d, Sz: %d, Prnt: %d\n",
                i, curTok->type, curTok->start, curTok->end, curTok->size, curTok->parent);
    }

    /* Loop over all keys of the root object */
    for (int i = 1; i < tokenCount; i++){
        auto       curTok = &tokens[i];
        jsmntok_t *nextTok = NULL;

        if ((i + 1) < tokenCount)
            nextTok = &tokens[i + 1];

        //
        if ((jsoneq(str, curTok, "Type") == 0) && nextTok)
        {
            typeFound = true;
            strncpy(type, str + nextTok->start, nextTok->end - nextTok->start);
            type[nextTok->end - nextTok->start] = '\0';
            i++;  // Skip next token 
            continue;
        }

        if ((jsoneq(str, curTok, "ResponseId") == 0) && nextTok)
        {
            responseIdFound = true;
            strncpy(responseId, str + nextTok->start, nextTok->end - nextTok->start);
            responseId[nextTok->end - nextTok->start] = '\0';
            i++;  // Skip next token 
            continue;
        }

        if ((jsoneq(str, curTok, "Ports") == 0) && nextTok)
        {
            if(nextTok->type != JSMN_ARRAY)
                continue;
            
            portsFound = true;

            for(int portIdx = 0; portIdx < nextTok->size;portIdx++){
                auto item = &tokens[i + portIdx + 2];
                uint8_t pinValue = 0;
            
                for(int itemIdx = item->start; itemIdx < item->end; itemIdx++){
                    int digit = str[itemIdx] - '0';
                    if ((digit >= 0) && (digit <= 9))
                        pinValue = (pinValue * 10) + digit;
                }
                
                if(pinValue > 0){
                    ports[portCount] = pinValue;
                    portCount++;
                }

            }

            i += nextTok->size + 1;
            continue;
        }


    } // Next Token

    if (!typeFound)
        return NULL;
    
    Serial.printf("Type %s\n", type);
    if(responseIdFound)
        Serial.printf("ResponseId %s\n", responseId);


    // Setup Input Values
    if(responseIdFound || (strcmp(type, "InputStatus") == 0)){
        for(uint8_t idx = 0; idx < inputCount; idx++){
            if(inputs[idx].InputValue)
                inputValues[idx] = '1';
            else
                inputValues[idx] = '0';
        }
        inputValues[inputCount] = '\0';
    }

    if((strcmp(type, "OutputStatus") == 0) || 
       (strcmp(type, "OutputStatusHealth") == 0)){
        for(uint8_t idx = 0; idx < outputCount; idx++){
            if(outputs[idx].OutputValue)
                outputValues[idx] = '1';
            else
                outputValues[idx] = '0';
        }
        outputValues[outputCount] = '\0';

        if(strcmp(type, "OutputStatusHealth") == 0){
            for(uint8_t idx = 0; idx < outputCount; idx++){
                auto pinValue = digitalReadFast(outputs[idx].OutputPin);
            if(pinValue)
                outputPinValues[idx] = '1';
            else
                outputPinValues[idx] = '0';
        }
        outputPinValues[outputCount] = '\0';
        }

    }

    if(responseIdFound){
        sprintf(responseAttrOut, ", \"ResponseId\": \"%s\","
                                 " \"LowResTick\": %lu,"
                                 " \"HighResTick\": %lu,"
                                 " \"Inputs\": \"%s\" ",
                                 responseId,
                                 lowResTick, 
                                 highResTick, 
                                 inputValues);
                                
    }

    char resp[1024];

    if (strcmp(type, "SetRTC") == 0) {
        // Ex:   {"Type": "SetRTC" Year: 2016, Month: 11, Day: 13, Hours: 23, Minutes: 23, Seconds: 22}
        if(responseIdFound){
            sprintf(resp, "{\"Type\": \"SetRTC\""
                          "%s}",
                    responseAttrOut);
            return strdup(resp);
        }
    } else if (strcmp(type, "OutputsOn") == 0) {
        // Ex:   {"Type": "OutputsOn", "Ports": [1,2,3,4]}
        if (portsFound){
            for (uint8_t idx = 0; idx < portCount; idx++)
            {
                auto port = ports[idx];
                if ((port >= 0) && (port < outputCount))
                {
                    auto op = &(outputs[port]);
                    op->OutputValue = 1;
                    op->HighResClockTick_Last_1 = highResTick;
                    op->LowResClockTick_Last_1 = lowResTick;
                    digitalWriteFast(op->OutputPin, 1);
                }
            }
        } else {
            sprintf(resp, "{\"Type\": \"OutputsOn\", \"Error\": \"Ports Attribute not found\" "
                          "%s}",
                    responseAttrOut);
                    return strdup(resp);
        }

        if (responseIdFound) {
            sprintf(resp, "{\"Type\": \"OutputsOn\""
                          "%s}",
                    responseAttrOut);
            return strdup(resp);
        }
    }
    else if (strcmp(type, "OutputsOff") == 0)
    {
        // Ex:   {"Type": "OutputsOff", "Ports": [1,2,3,4]}
        if (portsFound) {
            for (uint8_t idx = 0; idx < portCount; idx++)
            {
                auto port = ports[idx];
                if ((port >= 0) && (port < outputCount))
                {
                    auto op = &(outputs[port]);
                    op->OutputValue = 0;
                    op->HighResClockTick_Last_0 = highResTick;
                    op->LowResClockTick_Last_0 = lowResTick;
                    digitalWriteFast(op->OutputPin, 0);
                }
            }
        } else {
            sprintf(resp, "{\"Type\": \"OutputsOff\", \"Error\": \"Ports Attribute not found\" "
                          "%s}",
                    responseAttrOut);
            return strdup(resp);

        }

        if (responseIdFound) {
            sprintf(resp, "{\"Type\": \"OutputsOff\""
                          "%s}",
                    responseAttrOut);
            return strdup(resp);
        }
    } else if (strcmp(type, "ResetInputTimers") == 0) {
        // Ex:   {"Type": "ResetInputTimers"}
        TA_resetInputSensorState();
        if (responseIdFound) {
            sprintf(resp, "{\"Type\": \"ResetInputTimers\""
                          "%s}",
                    responseAttrOut);
            return strdup(resp);
        }
    }
    else if (strcmp(type, "ResetInputMessageQueue") == 0)
    {
        // Ex:   {"Type": "ResetInputMessageQueue"}
        TA_resetInputQueue();
        if (responseIdFound)
        {
            sprintf(resp, "{\"Type\": \"ResetInputMessageQueue\""
                          "%s}",
                    responseAttrOut);
            return strdup(resp);
        }
    }
    else if (strcmp(type, "AdapterInfo") == 0)
    {
        // Ex:   {"Type": "AdapterInfo"}
        // Ex:   {"Type": "AdapterInfo", "ResponseId": "TestId"}
        sprintf(resp, "{\"Type\": \"AdapterInfo\","
                      " \"Name\": \"Teensy-%s\","
                      " \"AdapterType\": \"Teensy4.1\","
                      " \"InputCount\": %u,"
                      " \"OutputCount\": %u,"
                      " \"LowResClockHz\": %lu,"
                      " \"HighResClockHz\": %lu"
                      "%s"
                      "}",
                adapterId, inputCount, outputCount,
                LowResClockHz, HighResClockHz,
                responseAttrOut);
        return strdup(resp);
    }
    else if (strcmp(type, "OutputStatus") == 0)
    {
        // Ex:   {"Type": "OutputStatus"}
        // Ex:   {"Type": "OutputStatus", "ResponseId": "TestId"}
        sprintf(resp, "{\"Type\": \"OutputStatus\","
                      " \"Values\": \"%s\""
                      "%s}",
                outputValues,
                responseAttrOut);
        return strdup(resp);
    }
    else if (strcmp(type, "OutputStatusHealth") == 0)
    {
        // Ex:   {"Type": "OutputStatusHealth"}
        // Ex:   {"Type": "OutputStatusHealth", "ResponseId": "TestId"}
        sprintf(resp, "{\"Type\": \"OutputStatusHealth\","
                      " \"Values\": \"%s\","
                      " \"PinValues\": \"%s\","
                      "%s}",
                outputValues,
                outputPinValues,
                responseAttrOut);
        return strdup(resp);
    }
    else if (strcmp(type, "InputStatus") == 0)
    {
        // Ex:   {"Type": "InputStatus"}
        // Ex:   {"Type": "InputStatus", "ResponseId": "TestId"}
        sprintf(resp, "{\"Type\": \"InputStatus\","
                      " \"Values\": \"%s\""
                      "%s}",
                inputValues,
                responseAttrOut);
        return strdup(resp);
    }

    return NULL;
}


