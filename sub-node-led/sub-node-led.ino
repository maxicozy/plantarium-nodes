 #include <Wire.h>

//pins für die verschiedenen led panele, sind in 3 aufgeteilt weil der strom von allen 3 in einem relais dieses durchschmoren würde
int led1 = 3;
int led2 = 4;
int led3 = 5;

int ledAddr = 0;
int ledState;

void setup() {
  pinMode (led1, OUTPUT);
  pinMode (led2, OUTPUT);
  pinMode (led3, OUTPUT);
  Wire.begin(ledAddr);
  Wire.onReceive(receiveEvent);
}

void receiveEvent(int bytes) {
  ledState = Wire.read();
}

void loop() {
  if (ledState == 1) {
    digitalWrite(led1, LOW);
    delay(500);
    digitalWrite(led2, LOW);
    delay(500);
    digitalWrite(led3, LOW);
  }
  if (ledState == 2) {
    digitalWrite(led1, HIGH); 
    delay(500);   
    digitalWrite(led2, HIGH);
    delay(500);
    digitalWrite(led3, HIGH);
  }
}
