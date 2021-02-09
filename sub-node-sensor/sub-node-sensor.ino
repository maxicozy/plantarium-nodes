#include <Wire.h>

#define levelPin 14
#define phPin 15
#define tdsPin 16

//das ist die adresse des moduls und ist in jedem modul unterschiedlich. irgendwann müssen wir noch herausfinden wie neue Adressen zugewiesen werden für den setup prozess
const int tomatoAddr = 4;

//ph sensor variablen
float calibration_value = 21.34 - 0.7;
unsigned long int avgval;
int buffer_arr[10], temp;
float phVal;

//tds sensor variablen
#define VREF 5.0                    // analog reference voltage(Volt) of the ADC
#define SCOUNT 30                   // sum of sample point
int analogBuffer[SCOUNT];           // store the analog value in the array, read from ADC
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0, copyIndex = 0;
float averageVoltage = 0, temperature = 25;
float tdsVal;

//water level variablen
int waterLevel;


void setup() {
  Serial.begin(115200);
  pinMode(levelPin, INPUT);
  pinMode(tdsPin, INPUT);
  pinMode(phPin, INPUT);
  Wire.begin(tomatoAddr);                
  Wire.onRequest(requestEvent); 
}

void requestEvent() {
  byte data[2];
  //um einen int über i2c zu senden muss dieser in einzelne bytes aufgeteilt werden
  data[0] = (waterLevel >> 8)& 0xFF;
  data[1] = waterLevel & 0xFF;

  //einen float über i2c zu verschicken geht am einfachsten indem man den float über union mit einem buffer an der gleichen stelle speichert und dann den buffer verschickt
  union floatToBytes {
    char buffer[4];
    float value;
  } ph, tds;

  ph.value = phVal;
  tds.value = tdsVal;

  Wire.write(data, 2);
  Wire.write(ph.buffer, 4);
  Wire.write(tds.buffer, 4);
}

void loop() { 
  waterLevel = analogRead(levelPin);
  phSensor();
  tdsSensor();
}

//das ist einfach code von unserem ph sensor der den ph wert ermittelt
void phSensor() {
  
  for (int i = 0; i < 10; i++) {
    buffer_arr[i] = analogRead(phPin);
    delay(30);
  }
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }
  avgval = 0;
  for (int i = 2; i < 8; i++) {
    avgval += buffer_arr[i];
  }
  float volt = (float)avgval * 5.0 / 1024 / 6;
  phVal = -5.70 * volt + calibration_value;

  Serial.print("pH Val: ");
  Serial.println(phVal);
  delay(1000);
}


//das ist einfach code von unserem tds sensor der den tds wert ermittelt
void tdsSensor() {
  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) { //every 40 milliseconds,read the analog value from the ADC
 
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(tdsPin);    //read the analog value and store into the buffer
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT)
      analogBufferIndex = 0;
  }
  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * (float)VREF / 1024.0; // read the analog value more stable by the median filtering algorithm, and convert to voltage value
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0); //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
    float compensationVolatge = averageVoltage / compensationCoefficient; //temperature compensation
    tdsVal = (133.42 * compensationVolatge * compensationVolatge * compensationVolatge - 255.86 * compensationVolatge * compensationVolatge + 857.39 * compensationVolatge) * 0.5; //convert voltage value to tds value
    Serial.print("TDS Value: ");
    Serial.print(tdsVal);
    Serial.println("ppm");
  }
}

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++)
    bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0)
    bTemp = bTab[(iFilterLen - 1) / 2];
  else
    bTemp = (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
  return bTemp;
}
