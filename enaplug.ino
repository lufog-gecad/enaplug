#include <Ethernet.h>
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire(A15);
DallasTemperature sensors(&oneWire);

const unsigned long enaplug_id = 180717001;  //aa mm dd ccc - 9 digitos
const String description = "waterHeater";
const byte serverDB[] = { 192, 168, 2, 5 };
const int port = 8520;

String sendParams;

int pin_control_1_rs = 23;
int pin_rele_control = 22;

long startTime, sleepingTime;
long sendingTime = 5000;

byte inChar;

byte mac[] = {
  0xa4, 0x4a, 0xea, 0x34, 0x2E, 0xa2
};

EthernetClient client;
EthernetServer server(80);
String msg;

double temp = 0;
double act = 0;
double react = 0;
double curr = 0;
double volt = 0;

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600);
  
  pinMode(4,OUTPUT);
  digitalWrite(4,HIGH);
  
  pinMode(pin_control_1_rs, OUTPUT);
  digitalWrite(pin_control_1_rs, LOW);
  
  pinMode(pin_rele_control, OUTPUT);
  digitalWrite(pin_rele_control, LOW);
  
  Serial.println("STARTING!");
  startEthernet();
  startTime = millis();
}

void loop() {
  if((millis() - startTime) > sendingTime){
    Ethernet.maintain();
    startTime = millis();
    requestRTU();
    readRTU(500);
    readSensors();
    publish();
    delay(50);
  }
  
  /*if(Serial2.available())
    Serial.println(Serial2.read(), HEX);*/
  
  //Ethernet
  if(server.available()){
    EthernetClient client = server.available();
    
    if(client){
      Serial.println("novo cliente");
      boolean clientTrustworthy = false;
      char c;
    
      while (client) {
        msg = "";
        while(client.available() > 0){
          c = client.read();
          msg += c;
        }
        
        if(clientTrustworthy){
          if(msg.charAt(0) == 'c'){
            Serial.println("Order received");
            if(msg.charAt(1)-48 == 0)
              digitalWrite(pin_rele_control, HIGH);
            else
              digitalWrite(pin_rele_control, LOW);
            client.stop();
          }
        }else if(msg == "startCom"){
          clientTrustworthy = true;
          delay(25);
          client.print("OKI");
          delay(25);
        }
      }
    }
  }
}

void readSensors(){
  sensors.requestTemperatures();
  temp = sensors.getTempCByIndex(0);
}

void readRTU(int timeout){
  int cont = 0;
  int index = 0;
  byte resp[50];
  boolean readSomething = false;
  
  while(cont < timeout){
    while(Serial2.available()){
      readSomething = true;
      byte c = Serial2.read();
      resp[index] = c;
      //Serial.println(c, HEX);
      index++;
      if(!Serial2.available())
        delay(10);
    }
    if(readSomething){
      break;
    }
    delay(1);
    cont++;
  }
  
  Serial.println("Resposta: ");
  for(int i=0; i<index; i++){
    Serial.print(resp[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
  
  volt = ((resp[3] << 24) | (resp[4] << 16) | (resp[5] << 8) | (resp[6]));
  volt = volt/10;
  curr = ((resp[7] << 24) | (resp[8] << 16) | (resp[9] << 8) | (resp[10]));
  curr = curr/100;
  act = ((resp[11] << 24) | (resp[12] << 16) | (resp[13] << 8) | (resp[14]))*10;
  react = ((resp[15] << 24) | (resp[16] << 16) | (resp[17] << 8) | (resp[18]))*10;
  
  Serial.print("Voltagem: ");
  Serial.println(volt);
  Serial.print("curr: ");
  Serial.println(curr);
  Serial.print("act: ");
  Serial.println(act);
  Serial.print("react: ");
  Serial.println(react);
  Serial.print("temp: ");
  Serial.println(temp);
}

void requestRTU(){
  byte request[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x08, 0x44, 0x0C};
  digitalWrite(pin_control_1_rs, HIGH);
  for (int i = 0; i < 8; i++)
  {
    Serial2.write(request[i]);
  }
  Serial2.flush();
  digitalWrite(pin_control_1_rs, LOW);
}

void startEthernet(){
  
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed Ethernet");

  }else{
    Serial.print("IP: ");
    Serial.println(Ethernet.localIP());
  }
  server.begin();
}

String DisplayAddress(IPAddress address)
{
 return String(address[0]) + "." + 
        String(address[1]) + "." + 
        String(address[2]) + "." + 
        String(address[3]);
}

void publish(){
  //ALTERAR O TAMANHO (45+9+15+50+6+6+6+6+6)
  /*char params[160];
  
  Serial.println("F");
  String Sact(act, 2);
  String Sreact(react, 2);
  String Samp(curr, 2);
  String Svolt(volt, 2);
  String Stemp(temp, 2);
  Serial.println("D");*/
  
  Serial.println("--");
  sendParams = "GET /enaplug/store/?enaplug_id=" + String(enaplug_id) + 
                    "&ip=" + DisplayAddress(Ethernet.localIP()) + 
                    "&description=" + description + 
                    "&act=" + String(act, 2) + 
                    "&react=" + String(react, 2) + 
                    "&curr=" + String(curr, 2) + 
                    "&volt=" + String(volt, 2) + 
                    "&temp=" + String(temp, 2) +
                    "&state=" + digitalRead(pin_rele_control) +
                    " HTTP/1.0";
  /*char params[sendParams.length()];
  sprintf(params,"%s",sendParams.c_str());*/
  
  /*sprintf(params,"id=%s&ip=%s&d=%s&act=%s&react=%s&curr=%s&volt=%s&temp=%s", 
            String(enaplug_id).c_str(), DisplayAddress(Ethernet.localIP()).c_str(), description.c_str(),
            Sact.c_str(), Sreact.c_str(), Samp.c_str(), Svolt.c_str(), Stemp.c_str());*/
  
  Serial.println(sendParams);
  
  //if(!postPage(DBserverName,DBserverPort,DBpageName,params)){
  if(!postPage(sendParams)){
    Serial.println("ERRO IN DB");
  }else{
    Serial.println("Published!");
  }
}

byte postPage(String thisData)
{
  //byte inChar;
  //char outBuf[200];
  client.setTimeout(500);
  if(client.connect(serverDB, port))
  {
    // send the header
    /*client.println(F("POST /uGIM/saver/saviour_plug.php HTTP/1.1"));
    client.println(F("Host: 192.168.2.116"));
    client.println(F("Connection: close\r\nContent-Type: application/x-www-form-urlencoded"));
    client.print(F("Content-Length: "));
    client.println(thisData.length());
    client.println();*/
    
    client.println(thisData);
    client.println();

    // send the body (variables)
    //client.print(thisData);
  } 
  else
  {
    return 0;
  }
  Serial.println("oi");

  int connectLoop = 0;
  int res = 0;

  while(client.connected())
  {
    while(client.available())
    {
      inChar = client.read();
      Serial.write(inChar);
      connectLoop = 0;
      if(inChar == 'O'){
        if(client.available()){
          inChar = client.read();
          Serial.write(inChar);
          if(inChar == 'K'){
            res = 1;
          }
        }
      }
    }

    delay(1);
    connectLoop++;
    if(connectLoop > 2000)
    {
      client.stop();
      return res;
    }
  }

  client.stop();
  return res;
}
