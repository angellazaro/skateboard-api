/*
   Copyright (c) 2015, Majenko Technologies
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice, this
     list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char *ssid = "Goku";
const char *password = "1234567a";

WebServer server(80);

// Indica si se está depurando
// 0 => no depurando
// 1 => depurando
const int debug = 0;

// Se incluye un delay para que no vaya tan rápido que ejecute correctamente 
// la función (en ms)
const int delay_time = 20;

// Setting PWM properties: https://randomnerdtutorials.com/esp32-dc-motor-l298n-motor-driver-control-speed-direction/
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 200;
const int pwm = 2;


// a1.a2 = 0,0 frenado
// a1.a2 = 1,0 adelante
// a1.a2 = 0,1 marcha atrás
const int a1 = 0;
const int a2 = 15;

int valA1 = 0;
int valA2 = 0;

String hostname = "skateboard";

// flag = 0 => desacelerar
// flag = 1 => acelerar
// flag = 2 => frenar
int flag = 0;

// Segundos que han transcurrido
double seconds = 0;

// Valor a escribir en PWM
double pwmValue = 0;

// Acción recibida
// 0 = up
// 1 = down
int action = 0;


// Modo de funcionamiento
// 0 = lineal
// 1 = exponencial
int functionMode = 0;
int *p_functionMode = &functionMode;

// Dirección de marcha
// -1 => atrás
//  1 => adelante
int moveDirection = 1;
int *p_moveDirection = &moveDirection;

// Valor máximo de PWM
int pwmMax = 50;
int *p_pwmMax = &pwmMax;

int getFunctionMode(){
  return *p_functionMode;
}

void setFuntionMode(int fm){
  *p_functionMode = fm;
}

int getMoveDirection(){
  return *p_moveDirection;
}

void setMoveDirection(int md){
  *p_moveDirection = md;
}

int getPwmMax(){
  return *p_pwmMax;
}

void setPwmMax(int pm){
  *p_pwmMax = pm;
}

void handleConnect() {
  StaticJsonDocument<300> data;
  data["acceleration"] = "lineal";
  if (getFunctionMode() == 1) {
    data["acceleration"] = "exponential";
  }

  data["way"] = "forward";
  if (getMoveDirection() == -1) {
    data["way"] = "backward";
  }

  data["limit"] = (String)getPwmMax();

  String response;
  serializeJson(data, response);

  server.send(200, "application/json", response);
}

void handleThrottle() {
  // Se configura el flag por defecto a desacelerar
  flag = 0;
  // Se comprueba si el parámetro es down para empezar a acelerar
  if (server.arg(String("action")).equals("down")) {
    flag = 1;
  }

  server.send(200, "text/html", "handleThrottle");
}

void handleBrake() {
  // Se configura el flag por defecto a frenar
  flag = 2;
  // Se comprueba si el parámetro es down para empezar a acelerar
  if (server.arg(String("action")).equals("up")) {
    flag = 0;
  }

  server.send(200, "text/html", "handleBrake");
}

void handleWrite() {

  // Modo de funcionamiento
  // 0 = lineal
  // 1 = exponencial
  int functionMode = 0;
  setFuntionMode(0);
  if (server.arg(String("acceleration")).equals("exponential")) {
    setFuntionMode(1);
  }

  // Dirección de marcha
  // -1 => atrás
  //  1 => adelante
  int moveDirection = 1;
  setMoveDirection(1);
  if (server.arg(String("way")).equals("backward")) {
    setMoveDirection(-1);
  }

  // Se comprueba el valor del límite
  int aux = server.arg(String("limit")).toInt();
  setPwmMax(aux);
  if (aux < 0 ) {
    setPwmMax(0);
  }

  if (aux > 100) {
    setPwmMax(100);
  }

  server.send(200, "text/html", "handleWrite");
}

void dethrottle() {
  // Se comienza a desacelerar
  seconds = seconds - 0.1;
  if (seconds < 0) {
    seconds = 0;
  }

  // Se calcula el valor de los pines en función de la dirección
  if (getMoveDirection() == 1) {
    valA1 = HIGH;
    valA2 = LOW;
  } else {
    valA1 = LOW;
    valA2 = HIGH;
  }
}

void throttle() {
  unsigned long currentTime = millis();

  // Se obtienen los segundos
  seconds = seconds + 0.1;

  // Se calcula el valor de los pines en función de la dirección
  if (getMoveDirection() == 1) {
    valA1 = HIGH;
    valA2 = LOW;
  } else {
    valA1 = LOW;
    valA2 = HIGH;
  }
}

void brake() {
  // Se comienza a frenar
  seconds = seconds - 0.1;
  if (seconds < 0) {
    seconds = 0;
  }

  valA1 = LOW;
  valA2 = LOW;
}

void calculatePWM() {
  // En función del modo de funcionamiento se hace un cálculo u otro
  if (functionMode == 0) {
    // Se hace un cálculo lineal
    pwmValue = (2 * seconds);
  } else {
    pwmValue = seconds * seconds;
  }

  // Se comprueba que no se sobrepase el límite
  if (pwmValue > getPwmMax()) {
    pwmValue = getPwmMax();

    // Si se pasa del pwm se quita el incremento de la acelaración para que no siga subiendo
    // y baje la aceleración
    seconds = seconds - 0.1;
  }

  // Si está frenando se pone a cero
  if (flag == 2) {
    pwmValue = 0;
  }

  pwmValue = map(pwmValue, 0, 100, 0, 255);
}

void writeToMotor() {

  // Si se está parado se frena
  if (pwmValue == 0) {
    valA1 = LOW;
    valA2 = LOW;
  }

  digitalWrite(a1, valA1);
  digitalWrite(a2, valA2);

  ledcWrite(pwmChannel, pwmValue);
}

void printLog() {
  if (debug) {
    /*
      Serial.print("FLAG: ");
      Serial.print(flag);
      Serial.print(" ACTION: ");
      Serial.print(action);
      Serial.print(" MODE: ");
      Serial.print(functionMode);
    */
    Serial.print(" FLAG: ");
    Serial.print(flag);
    Serial.print(" ACC: ");
    Serial.print(getFunctionMode());
    Serial.print(" DIR: ");
    Serial.print(getMoveDirection());
    Serial.print(" LIMIT: ");
    Serial.print(getPwmMax());
    Serial.print(" A1: ");
    Serial.print(valA1);
    Serial.print(" A2: ");
    Serial.print(valA2);
    Serial.print(" PWM: ");
    Serial.print(pwmValue);
    Serial.print(" SECONDS: ");
    Serial.println(seconds);
  }
}

void setup(void) {

  Serial.begin(9600);
  delay(10);

  // Configuración de los pines de control
  pinMode(a1, OUTPUT);
  pinMode(a2, OUTPUT);

  // Configuración del PWM
  pinMode(pwm, OUTPUT);
  // configure LED PWM functionalitites
  ledcSetup(pwmChannel, freq, resolution);
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(pwm, pwmChannel);

  // Configuración de la wifi
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(hostname.c_str());

  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/api/connect", handleConnect);
  server.on("/api/throttle", handleThrottle);
  server.on("/api/brake", handleBrake);
  server.on("/api/writeConfig", handleWrite);
  server.begin();

  Serial.println("HTTP server started");
}

void loop(void) {

  switch (flag) {
    case 0:
      dethrottle();
      break;
    case 1:
      throttle();
      break;
    case 2:
      brake();
      break;
    default:
      // if nothing else matches, do the default
      // default is optional
      break;
  }

  // Se calcula el PWM en función de la acción realizada
  calculatePWM();

  // Se escribe al motor
  writeToMotor();

  printLog();

  delay(delay_time);

  server.handleClient();
}
