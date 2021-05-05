#include <list>
#include "config.h"
#include "debug.h"
#include <sml/sml_file.h>
#include "Sensor.h"
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfOptionalGroup.h>
#include "MqttPublisher.h"
#include "EEPROM.h"
#include <ESP8266WiFi.h>

#ifdef ESP8266
# include <ESP8266HTTPUpdateServer.h>
#elif defined(ESP32)
# include <IotWebConfESP32HTTPUpdateServer.h>
#endif


#define STRING_LEN 128

SensorUiGroup *sensors[NUM_OF_SENSORS];
//std::list<Sensor*> *sensors = new std::list<Sensor*>();

void wifiConnected();
void configSaved();

DNSServer dnsServer;
WebServer server(80);

#ifdef ESP8266
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
HTTPUpdateServer httpUpdater;
#endif

WiFiClient net;

MqttConfig mqttConfig;
MqttPublisher publisher;

IotWebConf iotWebConf(WIFI_AP_SSID, &dnsServer, &server, WIFI_AP_DEFAULT_PASSWORD, CONFIG_VERSION);

iotwebconf::ParameterGroup paramgMqtt ("MQTT", "MQTT");
iotwebconf::TextParameter paramMqttServer ("Server", "mqttServer", mqttConfig.server, sizeof(mqttConfig.server));
iotwebconf::TextParameter paramMqttPort ("Port", "mqttPort", mqttConfig.port, sizeof(mqttConfig.port));
iotwebconf::TextParameter paramMqttUsername ("Username", "mqttUsername", mqttConfig.username, sizeof(mqttConfig.username));
iotwebconf::TextParameter paramMqttPassword ("Password", "mqttPassword", mqttConfig.password, sizeof(mqttConfig.password));
iotwebconf::TextParameter paramMqttTopic ("Topic", "mqttTopic", mqttConfig.topic, sizeof(mqttConfig.topic));

iotwebconf::OptionalGroupHtmlFormatProvider optionalGroupHtmlFormatProvider;

boolean needReset = false;
boolean connected = false;


void process_s0_out(Sensor *sensor, sml_file *file){
	//TODO
}

void process_message(sml_file *file, Sensor *sensor)
{
	// Parse
	

	DEBUG_SML_FILE(file);

	//mqtt
	if (connected) {
		publisher.publish(sensor, file);
	}

	// free the malloc'd memory
	
}

void setup()
{
	// Setup debugging stuff
	SERIAL_DEBUG_SETUP(115200);

#ifdef DEBUG
	// Delay for getting a serial console attached in time
	delay(2000);
#endif


	paramgMqtt.addItem(&paramMqttServer);
	paramgMqtt.addItem(&paramMqttPort);
	paramgMqtt.addItem(&paramMqttUsername);
	paramgMqtt.addItem(&paramMqttPassword);
	iotWebConf.addParameterGroup(&paramgMqtt);
	iotWebConf.setHtmlFormatProvider(&optionalGroupHtmlFormatProvider);


	// Setup reading heads
	DEBUG("Setting up %d configured sensors...", NUM_OF_SENSORS);
	//SensorConfig *config  = SENSOR_CONFIGS;
	for (uint8_t i = 0; i < NUM_OF_SENSORS; i++){
		sensors[i] = new SensorUiGroup(i);
		sensors[i]->sensor = new Sensor(sensors[i]->sensor_config, process_message);
		iotWebConf.addParameterGroup(sensors[i]->ogroup);
	}
	DEBUG("Sensor setup done.");

	// Initialize publisher
	// Setup WiFi and config stuff
	DEBUG("Setting up WiFi and config stuff.");


	iotWebConf.setConfigSavedCallback(&configSaved);
	iotWebConf.setWifiConnectionCallback(&wifiConnected);
	

	// register callbacks performing Update Server hooks. 
	iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); }
	);

	boolean validConfig = iotWebConf.init();
	if (!validConfig)
	{
		DEBUG("Missing or invalid config. MQTT publisher disabled.");
		MqttConfig defaults;
		// Resetting to default values
		strcpy(mqttConfig.server, defaults.server);
		strcpy(mqttConfig.port, defaults.port);
		strcpy(mqttConfig.username, defaults.username);
		strcpy(mqttConfig.password, defaults.password);
		strcpy(mqttConfig.topic, defaults.topic);
	}
	else
	{
		// Setup MQTT publisher
		publisher.setup(mqttConfig);
	}

	server.on("/", [] { iotWebConf.handleConfig(); });
	server.onNotFound([]() { iotWebConf.handleNotFound(); });

	DEBUG("Setup done.");
}

void loop()
{
	// Publisher
	if (connected) {
		publisher.loop();
		yield();
	}

	if (needReset){
		// Doing a chip reset caused by config changes
		DEBUG("Rebooting after 1 second.");
		delay(1000);
		ESP.restart();
	}

	// Execute sensor state machines
	for (int i=0; i<NUM_OF_SENSORS;i++){
		if(sensors[i]->ogroup->isActive())
			sensors[i]->sensor->loop();
	}
	iotWebConf.doLoop();
	yield();
}

void configSaved()
{
	DEBUG("Configuration was updated.");
	needReset = true;
}

void wifiConnected()
{
	DEBUG("WiFi connection established.");
	connected = true;
	publisher.connect();
}
