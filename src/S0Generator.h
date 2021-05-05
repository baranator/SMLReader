#ifndef S0_GENERATOR_H
#define S0_GENERATOR_H

#include "config.h"
#include "debug.h"
#include "Sensor.h"
#include <sml/sml_file.h>

//s0 stuff
//active pulse length must be between 30ms and 120ms (see https://de.wikipedia.org/wiki/S0-Schnittstelle); so we choose 75ms ;)
const uint8_t S0_ACTIVE_MS=75;

class S0Generator{
private:
  unsigned long current_since;
  unsigned long current_idle_ms=60*1000;  //start with long idle until first data arrives
  unsigned long current_target=current_idle_ms;
  bool current_active = false;
public:
  uint16_t ppkwh;
  char* mode;
  int pin;
  
  S0Generator(int pin, char* mode, uint16_t ppkwh){
    this->ppkwh = ppkwh;
    this->mode = mode;
    this->pin = pin;
    //setup pin
  }

  void loop(){
    if(strcmp(mode,"off") != 0){
      

      if(millis()-current_since>=current_target){

        if(current_active){
            //switch pin to LOW

            current_target=S0_ACTIVE_MS;
        }else{
            //switch pin to high

            current_target=current_idle_ms;
        }

        //toggle output
        current_since=millis();
        current_active=!current_active;

      }
    }else{
      //set pin low

    }


  }

  void set_pulse(sml_file *file){

    for (int i = 0; i < file->messages_len; i++){
        sml_message *message = file->messages[i];
        if (*message->message_body->tag == SML_MESSAGE_GET_LIST_RESPONSE){
            sml_list *entry;
            sml_get_list_response *body;
            body = (sml_get_list_response *)message->message_body->data;
            for (entry = body->val_list; entry != NULL; entry = entry->next){
                if (!entry->value){   
                    // do not crash on null value
                    continue;
                }

                //skip except for 16.7.0 
                if(entry->obj_name->str[2]!=16 || entry->obj_name->str[2]!=7 || entry->obj_name->str[2]!=0){
                    continue;
                }

                DEBUG("s0-data found, processing");

                //char buffer[255];

                
                if (((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_INTEGER) ||
                         ((entry->value->type & SML_TYPE_FIELD) == SML_TYPE_UNSIGNED)){
                    double value = sml_value_to_double(entry->value);
                    int scaler = (entry->scaler) ? *entry->scaler : 0;
                    int prec = -scaler;
                    if (prec < 0)
                        prec = 0;
                    value = value * pow(10, scaler);


                    //TODO handle different modes

                    //todo: create pulse based on settings
                    //value is in Watt, so convert to kW and multiply by pulses per kWh to get pulses per hour:
                    double pulses_per_hour = value * this->ppkwh/1000.0;
                    
                    
                    //calc length of idle in duty-cycle; put last pulse at the very end
                    current_idle_ms = (((60*60*1000) - pulses_per_hour*S0_ACTIVE_MS))/(pulses_per_hour-1);


                    // if s0 pulse active, no further actions are needed, next idle phase will be of correct length. 
                    // If in idle phase the current_idle_ms is updated (shortened or prolonged) and will take effect immediatly
                    

                }
            }
        }
    }
  }


};

#endif