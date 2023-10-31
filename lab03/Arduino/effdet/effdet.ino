#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 

const char* USER = "";
const char* PWD = "";
const char* PUB = "dogcat";

//Variaveis gloabais desse codigo
char bufferJ[256];
char *mensagem;
unsigned long intime;
unsigned int latency_us;

char *jsonMQTTmsgDATA(const char *dataType, const char *topic, const char *login, const char *pass, int classe) {
	const int capacity = JSON_OBJECT_SIZE(5);
	StaticJsonDocument<capacity> jsonMSG;
	jsonMSG["dataType"] = dataType;
  jsonMSG["topic"] = topic;
  jsonMSG["login"] = login;
  jsonMSG["pass"] = pass;
	jsonMSG["class"] = classe;
	serializeJson(jsonMSG, bufferJ);
	return bufferJ;
}

void setup() {
  Serial.begin(115200);

}

void loop() {
  mensagem = jsonMQTTmsgDATA("dogcat", PUB, USER, PWD, random(2));
  Serial.println(mensagem);
  delay(10000);
}
