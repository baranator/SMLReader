#ifndef SENSOR_H
#define SENSOR_H

#include <SoftwareSerial.h>
#include <jled.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfOptionalGroup.h>
#include "debug.h"
#include "S0Generator.h"

//UI stuff
const char s0modes[][8]={"off","draw","feed_in","both"};


// SML constants
const byte START_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01};
const byte END_SEQUENCE[] = {0x1B, 0x1B, 0x1B, 0x1B, 0x1A};
const size_t BUFFER_SIZE = 3840; // Max datagram duration 400ms at 9600 Baud
const uint8_t READ_TIMEOUT = 30;



// States
enum State
{
    INIT,
    WAIT_FOR_START_SEQUENCE,
    READ_MESSAGE,
    PROCESS_MESSAGE,
    READ_CHECKSUM
};

class SensorConfig
{
public:
    uint8_t id;

    char name[20+1];
    char numeric_only[1+1];
    char status_led_enabled[1+1];
    char status_led_inverted[1+1];
    char status_led_pin[2+1];
    char interval[4+1];
    
    char sml_in_pin[2+1];
    
    char s0_mode[1+1];
    char s0_ppkwh[6+1];
    char s0_out_pin[2+1];

};


class Sensor
{
public:
    SensorConfig *config;
    Sensor(SensorConfig *config, void (*callback)(sml_file* file,  Sensor *sensor))
    {
        this->config = config;
        if(strcmp(config->s0_mode,"off")!=0){
            this->s0gen=new S0Generator(atoi(this->config->s0_out_pin),this->config->s0_mode, atoi(this->config->s0_ppkwh) );
        }

        DEBUG("Initializing sensor %s...", this->config->name);
        this->callback = callback;
        this->serial = new SoftwareSerial();
        this->serial->begin(9600, SWSERIAL_8N1, atoi(this->config->sml_in_pin), -1, false);
        this->serial->enableTx(false);
        this->serial->enableRx(true);
        DEBUG("Initialized sensor %s.", this->config->name);

        if (this->config->status_led_enabled) {
            this->status_led = new JLed(atoi(this->config->status_led_pin));
            if (this->config->status_led_inverted) {
                this->status_led->LowActive();
            }
        }

        this->init_state();
    }

    void loop()
    {
        this->run_current_state();
        yield();
        if(strcmp(config->s0_mode,"off")!=0){
            this->s0gen->loop();
        }
        
        if (this->config->status_led_enabled) {
            this->status_led->Update();
            yield();
        }
    }

private:
    SoftwareSerial *serial;
    byte buffer[BUFFER_SIZE];
    size_t position = 0;
    unsigned long last_state_reset = 0;
    unsigned long last_callback_call = 0;
    uint8_t bytes_until_checksum = 0;
    uint8_t loop_counter = 0;
    State state = INIT;
    void (*callback)(sml_file *file, Sensor *sensor) = NULL;
    JLed *status_led;
    S0Generator *s0gen;







    void run_current_state()
    {
        if (this->state != INIT)
        {
            if ((millis() - this->last_state_reset) > (READ_TIMEOUT * 1000))
            {
                DEBUG("Did not receive an SML message within %d seconds, starting over.", READ_TIMEOUT);
                this->reset_state();
            }
            switch (this->state)
            {
            case WAIT_FOR_START_SEQUENCE:
                this->wait_for_start_sequence();
                break;
            case READ_MESSAGE:
                this->read_message();
                break;
            case PROCESS_MESSAGE:
                this->process_message();
                break;
            case READ_CHECKSUM:
                this->read_checksum();
                break;
            default:
                break;
            }
        }
    }

    // Wrappers for sensor access
    int data_available()
    {
        return this->serial->available();
    }
    int data_read()
    {
        return this->serial->read();
    }


    // Set state
    void set_state(State new_state)
    {
        if (new_state == WAIT_FOR_START_SEQUENCE)
        {
            DEBUG("State of sensor %s is 'WAIT_FOR_START_SEQUENCE'.", this->config->name);
            this->last_state_reset = millis();
            this->position = 0;
        }
        else if (new_state == READ_MESSAGE)
        {
            DEBUG("State of sensor %s is 'READ_MESSAGE'.", this->config->name);
        }
        else if (new_state == READ_CHECKSUM)
        {
            DEBUG("State of sensor %s is 'READ_CHECKSUM'.", this->config->name);
            this->bytes_until_checksum = 3;
        }
        else if (new_state == PROCESS_MESSAGE)
        {
            DEBUG("State of sensor %s is 'PROCESS_MESSAGE'.", this->config->name);
        };
        this->state = new_state;
    }

    // Initialize state machine
    void init_state()
    {
        this->set_state(WAIT_FOR_START_SEQUENCE);
    }

    // Start over and wait for the start sequence
    void reset_state(const char *message = NULL)
    {
        if (message != NULL && strlen(message) > 0)
        {
            DEBUG(message);
        }
        this->init_state();
    }

    // Wait for the start_sequence to appear
    void wait_for_start_sequence()
    {
        while (this->data_available())
        {
            this->buffer[this->position] = this->data_read();
            yield();

            this->position = (this->buffer[this->position] == START_SEQUENCE[this->position]) ? (this->position + 1) : 0;
            if (this->position == sizeof(START_SEQUENCE))
            {
                // Start sequence has been found
                DEBUG("Start sequence found.");
                if (this->config->status_led_enabled) {
                    this->status_led->Blink(50,50).Repeat(3);
                }
                this->set_state(READ_MESSAGE);
                return;
            }
        }
    }

    // Read the rest of the message
    void read_message()
    {
        while (this->data_available())
        {
            // Check whether the buffer is still big enough to hold the number of fill bytes (1 byte) and the checksum (2 bytes)
            if ((this->position + 3) == BUFFER_SIZE)
            {
                this->reset_state("Buffer will overflow, starting over.");
                return;
            }
            this->buffer[this->position++] = this->data_read();
            yield();

            // Check for end sequence
            int last_index_of_end_seq = sizeof(END_SEQUENCE) - 1;
            for (int i = 0; i <= last_index_of_end_seq; i++)
            {
                if (END_SEQUENCE[last_index_of_end_seq - i] != this->buffer[this->position - (i + 1)])
                {
                    break;
                }
                if (i == last_index_of_end_seq)
                {
                    DEBUG("End sequence found.");
                    this->set_state(READ_CHECKSUM);
                    return;
                }
            }
        }
    }

    // Read the number of fillbytes and the checksum
    void read_checksum()
    {
        while (this->bytes_until_checksum > 0 && this->data_available())
        {
            this->buffer[this->position++] = this->data_read();
            this->bytes_until_checksum--;
            yield();
        }

        if (this->bytes_until_checksum == 0)
        {
            DEBUG("Message has been read.");
            DEBUG_DUMP_BUFFER(this->buffer, this->position);
            this->set_state(PROCESS_MESSAGE);
        }
    }

    void process_message()
    {
        sml_file *file;
        DEBUG("Message is being processed.");
        bool callback_due = this->callback != NULL && (this->config->interval == 0 || ((millis() - this->last_callback_call) > ((uint16_t)atoi(this->config->interval) * 1000)));

        if(!callback_due && strcmp(config->s0_mode,"off")==0)
            return;

        file = sml_file_parse(this->buffer + 8, this->position - 16);
        // Call listener
        if (callback_due) {    
            this->last_callback_call = millis();
            this->callback(file,this);
        }

        if(strcmp(config->s0_mode,"off")==0){
            
            this->s0gen->set_pulse(file);
        }
        sml_file_free(file);

        // Start over
        this->reset_state();
    }
};

class SensorUiGroup{
private:
    char* genid(const char* attr){
        char *target = (char*)malloc(7+1+1+strlen(attr)+1);
        sprintf(target, "sensor_%d_%s", this->sensor_config->id, attr);
        return target;
    }
public:
    Sensor *sensor;
    SensorConfig *sensor_config;
    iotwebconf::OptionalParameterGroup *ogroup;
    iotwebconf::NumberParameter *param_s0_impulses;
    iotwebconf::NumberParameter *param_sml_in;
    iotwebconf::NumberParameter *param_s0_out;
    iotwebconf::SelectParameter *param_s0_mode;


    SensorUiGroup(uint8_t nr){
        this->sensor_config=new SensorConfig();
        this->sensor_config->id=nr;

        char *grpnm = (char*)malloc(13+1);
        sprintf(grpnm, "SML-Sensor %d", nr);
        this->ogroup=new iotwebconf::OptionalParameterGroup (genid("sml_sensor"), grpnm, false);

        this->param_sml_in=new iotwebconf::NumberParameter("Pin for SML input",genid("sml_in"), (*(sensor_config)).sml_in_pin, 2+1, "1", "1..30", "min='1' max='30' step='1'");
        this->ogroup->addItem(param_sml_in);

        this->param_s0_mode=new iotwebconf::SelectParameter("Mode of the S0 Output", genid("s0mode"), sensor_config->s0_mode, 2, (char*)s0modes, (char*)s0modes, 4, 8,"o");
        this->ogroup->addItem(param_s0_mode);

        this->param_s0_out=new iotwebconf::NumberParameter("Pin for S0 out", genid("s0_out"),(*(sensor_config)).s0_out_pin, 2+1, "1", "1..30", "min='1' max='30' step='1'");
        this->ogroup->addItem(param_s0_out);

        this->param_s0_impulses=new iotwebconf::NumberParameter("Pulses per kWh", genid("pulsesperkwatt"), (*(sensor_config)).s0_ppkwh, 6+1, "1", "1..10000", "min='1' max='10000' step='1'");
        this->ogroup->addItem(param_s0_impulses);

 
        


 
       
        



    }

};

#endif