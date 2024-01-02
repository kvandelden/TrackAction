

#include <vector>

extern "C" void TAsetup_init(char *sAdapterId);
extern "C" void TAsetup_setLowResClockHz(uint32_t Hz);
extern "C" void TAsetup_setHighResClockHz(uint32_t Hz);
extern "C" void TAsetup_setInputPortPins(uint8_t pins[], uint8_t count);
extern "C" void TAsetup_setOutputPortPins(uint8_t pins[], uint8_t count);
extern "C" void TAsetup_setIRQDebounceTime(uint32_t millis);
extern "C" void TAsetup_setInputQueueSize(uint32_t size);



struct InputSensorState {
    uint8_t  InputPin;
    uint8_t  InputValue;
    uint32_t IRQ_Count;

    uint32_t LowResClockTick_Last_1;
    uint32_t HighResClockTick_Last_1;
    uint32_t LowResClockTick_Prev_1;
    uint32_t HighResClockTick_Prev_1;

    uint32_t LowResClockTick_Last_0;
    uint32_t HighResClockTick_Last_0;
    uint32_t LowResClockTick_Prev_0;
    uint32_t HighResClockTick_Prev_0;

    // Debounce
    uint32_t LowResClockTick_Last;
    uint32_t IRQ_Debounce_Count;

};

struct OutputSensorState {
    uint8_t  OutputPin;
    uint8_t  OutputValue;
    uint32_t Set_Count;

    uint32_t LowResClockTick_Last_1;
    uint32_t HighResClockTick_Last_1;
    uint32_t LowResClockTick_Last_0;
    uint32_t HighResClockTick_Last_0;

};


extern "C" void ISR_inputSensor(uint8_t inputIdx, uint32_t lowResTick, uint32_t highResTick);

extern "C" void TA_resetInputSensorState();



struct InputMessageQueueItem {
    bool     bSent;
    bool     bReadyToSend;
    uint8_t  InputNumber;
    uint8_t  InputValue;
    uint32_t LowResClockTick;
    uint32_t LowResClockTick_Prev;
    uint32_t HighResClockTick;
    uint32_t HighResClockTick_Prev;

};

extern "C" void TA_resetInputQueue();

extern "C" std::vector<char *> TA_getInputQueueMessages();

extern "C" std::vector<char *> TA_processCommandStream(char *str, uint32_t lowResTick, uint32_t highResTick);

//-- Command Messages, defaults to no Reponse
// Type: "SetRTC" Year: 2016, Month: 11, Day: 13, Hours: 23, Minutes: 23, Seconds: 22
// Type: "OutputsOn", Ports: [1,2,3,4]
// Type: "OutputsOff", Ports: [1,2,3,4]
// Type: "ResetInputTimers"
// Type: "ResetInputMessageQueue"

// If a command has an attribute ResponseId value
//-- Response
//  {Type: "CommandResponse", ResponseId: "ABCD..", LowResTick: 12323, HighResTick: 43324323443, Inputs: 123123123 }

// If your turning on dragster lights, you need to know when the green is lit vs start line


//-- Request/ Response Messages
// Type: "AdapterInfo"
//-- Response
//   {Type: "AdapterInfo", Name: "Teensy-MAC", AdapterType: "Teensy4.1", InputCount: 16, OutputCount: 16,  LowResClockHz: 10000, HighResClockHz: 200200  }

// Type: "OutputStatus"
//-- Response
//   {Type: "OutputStatus", Values: 1010001... }   //1=On, 0=Off

// Type: "OutputStatusHealth"
//-- Response
//   {Type: "OutputStatusHealth", Values: 1010001..., PinState: 1010000.... }   //1=On, 0=Off

// Type: "InputStatus"
//-- Response
//   {Type: "InputStatus", Values: 1010001... }   //1=On, 0=Off



//-- Async Messages Types  
// (Implemented)
//   {Type: "InputChange", Input: 4, Value: 1, Seconds: 1.23232, LowResTick: 12323, HighResTick: 43324323443}

// {Type: "ProtocolError", Msg: ""}
