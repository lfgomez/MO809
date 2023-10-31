/*
  Exemplo aprendizado de maquina usando kmeans no ESP8266. A parte de comunicacao eh baseada 
  em https://github.com/knolleary/pubsubclient/blob/master/examples/mqtt_auth/mqtt_auth.ino. 
  Este exemplo se utiliza das bibliotecas do ESP8266 programado via Arduino IDE 
  (https://github.com/esp8266/Arduino) e a biblioteca PubSubClient que pode ser 
  obtida em: https://github.com/knolleary/pubsubclient/
*/

//Inserindo os dados do termistor usado
        
// Resistencia nominal a 25C (Estamos utilizando um MF52 com resistencia nominal de 1kOhm)
#define TERMISTORNOMINAL 1000      
// Temperatura na qual eh feita a medida nominal (25C)
#define TEMPERATURANOMINAL 25   
//Quantas amostras usaremos para calcular a tensao media (um numero entre 4 e 10 eh apropriado)
#define AMOSTRAS 4
// Coeficiente Beta (da equacao de Steinhart-Hart) do termistor (segundo o datasheet eh 3100)
#define BETA 3100
// Valor da resistencia utilizada no divisor de tensao (para temperatura ambiente, qualquer resistencia entre 470 e 2k2 pode ser usada)
#define RESISTOR 470   

#define NUM_DATA 100
#define NUM_CLUSTERS 2
#define MAX_ITERATIONS 1000

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> 

typedef struct {
    double value;
} Data;

typedef struct {
    Data centroid;
    Data points[NUM_DATA];
    int num_points;
} Cluster;

// Prototypes
double distance(Data* a, Data* b);
void kmeans(Data* dataset, int num_data, Cluster* clusters, int num_clusters);

// Vamos primeiramente conectar o ESP8266 com a rede Wireless (mude os parâmetros abaixo para sua rede).

const char* USER = "";
const char* PWD = "";

const char* PUB = "kmeans";

//Variaveis gloabais desse codigo
char bufferJ[256];
char *mensagem;
unsigned long intime;
unsigned int latency_us;

//Variaveis do termometro
float temperature;
float tensao;
float resistencia_termistor;
int i = 0;

//Variaveis do modelo
float threshold;
int cluster = 0;

//Vamos criar uma funcao para formatar os dados no formato JSON
char *jsonMQTTmsgDATA(const char *dataType, const char *topic, const char *login, const char *pass, float value, int cluster, float latency) {
	const int capacity = JSON_OBJECT_SIZE(7);
	StaticJsonDocument<capacity> jsonMSG;
	jsonMSG["dataType"] = dataType;
  jsonMSG["topic"] = topic;
  jsonMSG["login"] = login;
  jsonMSG["pass"] = pass;
	jsonMSG["temp"] = value;
  jsonMSG["cluster"] = cluster;
  jsonMSG["dev_latency"] = latency;
	serializeJson(jsonMSG, bufferJ);
	return bufferJ;
}

double distance(Data* a, Data* b) {
    return fabs(a->value - b->value);
}
//Funcao definindo o metodo kmeans
//Nota: voce consegue melhorar a inicializacao! Ela foi escrita para funcionar bem apenas com 2 classes.
void kmeans(Data* dataset, int num_data, Cluster* clusters, int num_clusters) {
    // Initialize clusters with first num_clusters datapoints
    for (int i = 0; i < num_clusters; i++) {
        clusters[i].centroid = dataset[i];
        clusters[i].num_points = 0;
    }
    clusters[1].centroid = dataset[int(NUM_DATA/2)];

    bool changes = true;
    int iterations = 0;
    
    while (changes && iterations < MAX_ITERATIONS) {
        changes = false;
        
        // Clear clusters
        for (int i = 0; i < num_clusters; i++) {
            clusters[i].num_points = 0;
        }

        // Assign data points to clusters
        for (int i = 0; i < num_data; i++) {
            int closest_cluster = 0;
            double min_distance = distance(&dataset[i], &clusters[0].centroid);
            for (int j = 1; j < num_clusters; j++) {
                double current_distance = distance(&dataset[i], &clusters[j].centroid);
                if (current_distance < min_distance) {
                    min_distance = current_distance;
                    closest_cluster = j;
                }
            }
            clusters[closest_cluster].points[clusters[closest_cluster].num_points] = dataset[i];
            clusters[closest_cluster].num_points++;
        }

        // Recalculate centroids
        for (int i = 0; i < num_clusters; i++) {
            Data new_centroid = {0};
            for (int j = 0; j < clusters[i].num_points; j++) {
                new_centroid.value += clusters[i].points[j].value;
            }
            new_centroid.value /= clusters[i].num_points;

            if (new_centroid.value != clusters[i].centroid.value) {
                changes = true;
                clusters[i].centroid = new_centroid;
            }
        }

        iterations++;
    }
}

void setup()
{
  //Configurando a porta Serial e escolhendo o servidor MQTT
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  //Encontrando o threshold para duas classes usando kmeans
  //Nota: voce consegue reescrever a funcao para mais classes passando o float para um vetor.
  threshold = trainning_kmeans(); 
}

float trainning_kmeans(){
  Data dataset[NUM_DATA];
  for (int i = 0; i < NUM_DATA; i++) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    dataset[i].value = get_temperature();
    delay(200);
    mensagem = jsonMQTTmsgDATA("temperature", PUB, USER, PWD, dataset[i].value, -1, -1);
    Serial.println(mensagem);
  }
  Cluster clusters[NUM_CLUSTERS];

    kmeans(dataset, NUM_DATA, clusters, NUM_CLUSTERS);

    // Encontrando thresholds
    double thresholds[NUM_CLUSTERS-1];
    for (int i = 0; i < NUM_CLUSTERS - 1; i++) {
        thresholds[i] = (clusters[i].centroid.value + clusters[i+1].centroid.value) / 2;
    }
    return thresholds[0];
}

//Funcao para calcular a temperatura baseada nos dados do termistor
float get_temperature(){
  tensao = 0;
  //Tirando a media do valor lido no ADC
  for (i=0; i< AMOSTRAS; i++) {
   tensao += analogRead(0)/AMOSTRAS;
   delay(10);
  }
  //Calculando a resistencia do Termistor
  resistencia_termistor = RESISTOR*tensao/(1023-tensao);
  //Equacao de Steinhart-Hart
  temperature = (1 / (log(resistencia_termistor/TERMISTORNOMINAL) * 1/BETA + 1/(TEMPERATURANOMINAL + 273.15))) - 273.15;
  return temperature;
}

void loop()
{
  //O programa em si eh muito simples: 
  //se nao estiver conectado no Broker MQTT, se conecte!
  temperature = get_temperature();
  intime = micros();
  //Calcule a temperatura e veja em que cluster ela se encontra
  if (temperature>threshold){
    cluster=1;
    digitalWrite(LED_BUILTIN, LOW);
  }
  else{
    digitalWrite(LED_BUILTIN, HIGH);
    cluster=0;
  }
  //Enviando via MQTT o resultado calculado da temperatura
  latency_us = micros()-intime;
  mensagem = jsonMQTTmsgDATA("temperature", PUB, USER, PWD, temperature, cluster, latency_us/1000.0);
  Serial.println(mensagem);
  //Gerando um delay de 2 segundos antes do loop recomecar
  delay(5000);
}
