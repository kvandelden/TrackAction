#include <Arduino.h>
#include "../../lib/AdapterUtils/adapterutils.h"
#include "../../lib/AdapterUtils/messagebus.h"



// Teensy 4.1 Provides 54 GPIO pins
//  - this adapter code provides
//   16 input pins for inputs (lap counter, speed traps, track markers, etc)
//    --- GPIO pins 0-12, 24-26 ...  13 + 3 pina
//   16 output pins for relays (track power, starting lights, general lights, etc)
//    --- GPIO pins 14-23, 36-41  ... 10 + 6 pins



FASTRUN void irq_inp0(void){ ISR_inputSensor(0, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp1(void){ ISR_inputSensor(1, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp2(void){ ISR_inputSensor(2, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp3(void){ ISR_inputSensor(3, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp4(void){ ISR_inputSensor(4, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp5(void){ ISR_inputSensor(5, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp6(void){ ISR_inputSensor(6, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp7(void){ ISR_inputSensor(7, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp8(void){ ISR_inputSensor(8, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp9(void){ ISR_inputSensor(9, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp10(void){ ISR_inputSensor(10, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp11(void){ ISR_inputSensor(11, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp12(void){ ISR_inputSensor(12, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp13(void){ ISR_inputSensor(13, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp14(void){ ISR_inputSensor(14, millis(), ARM_DWT_CYCCNT); }
FASTRUN void irq_inp15(void){ ISR_inputSensor(15, millis(), ARM_DWT_CYCCNT); }

char *getTensyId(){
  char   sId[32];
  uint32_t m1 = HW_OCOTP_MAC1;
  uint32_t m2 = HW_OCOTP_MAC0;
  sprintf(sId, "%04X:%04X", m1, m2);
  return strdup(sId);
}

void setup() {
  // USB Serial, doesn't use GPIO pins
  Serial.begin(115200);

  // Teensy 4.1 defaults to Fast Mode GPIO
  TAsetup_init(getTensyId());
  
  uint8_t inputPorts[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,24,25,26};
  TAsetup_setInputPortPins(inputPorts, sizeof(inputPorts));

  uint8_t outputPorts[] = {14,15,16,17,18,19,20,21,22,23,36,37,38,39,40,41};
  TAsetup_setOutputPortPins(outputPorts, sizeof(outputPorts));

  TAsetup_setLowResClockHz(1000);
  TAsetup_setHighResClockHz(F_CPU);
  TAsetup_setIRQDebounceTime(10);
  TAsetup_setInputQueueSize(20); // Low value for testing rolling behavior


  
  // Setup IRQ for handling GPIO Input Changes
  attachInterrupt(0, &irq_inp0, CHANGE);  
  attachInterrupt(1, &irq_inp1, CHANGE);
  attachInterrupt(2,&irq_inp2, CHANGE);
  attachInterrupt(3,&irq_inp3, CHANGE);
  attachInterrupt(4,&irq_inp4, CHANGE);
  attachInterrupt(5,&irq_inp5, CHANGE);
  attachInterrupt(6,&irq_inp6, CHANGE);
  attachInterrupt(7,&irq_inp7, CHANGE);
  attachInterrupt(8,&irq_inp8, CHANGE);
  attachInterrupt(9,&irq_inp9, CHANGE);
  attachInterrupt(10,&irq_inp10, CHANGE);
  attachInterrupt(11,&irq_inp11, CHANGE);
  attachInterrupt(12,&irq_inp12, CHANGE);
  attachInterrupt(24,&irq_inp13, CHANGE);
  attachInterrupt(25,&irq_inp14, CHANGE);
  attachInterrupt(26,&irq_inp15, CHANGE);

}



// The main loop sends outbound messages and request responses to the host
// See adapterutils.h for fill protocol spec
//
//  Async Outbound Messages:
//    Input Sensor Change
//  
//  Request/ Response
//    Sensor Info,  Set RTC, Sensor Status 
//

// Async Messages are initiated through interupts
// Requests are inituated through inbound serial data
// 

// The main loop checks queues to determine what to send outbound 
// & checks for inbound data if not handled by an ISR.

void loop() {
  
  auto inputMessages  = TA_getInputQueueMessages();
  
    for(auto i = inputMessages.begin(); i != inputMessages.end(); ++i){
      Serial.println(*i);
      free(*i);
    }

    inputMessages.clear();

    
    auto dataLen = Serial.available();
    if(dataLen){
      char sData[dataLen + 10];
      Serial.readBytes(sData, dataLen);
      sData[dataLen] = '\0';
      
      auto responseMessages = TA_processCommandStream(sData, millis(), ARM_DWT_CYCCNT);
      for (auto i = responseMessages.begin(); i != responseMessages.end(); ++i)
      {
        Serial.println(*i);
        free(*i);
      }
      
    }

}





