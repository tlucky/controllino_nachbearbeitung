#include <Controllino.h> 
#include <OneWire.h>
#include <Servo.h>

// Variablen
int event = 0;
bool terminated = false;
double temperature = 0;  // defaultwert
double flow = 100;  // defaultwert
int dist = 100;  // defaultwert
// 0: kein Error, 1:kritische Temperatur überschritten
// 2:kritischer Durchfluss überschritten, 3:Listen-Timeout
int errorcode = 0;


//Hilfsvariable
bool criticalTiltReachedHelper = false;
                         



//Konstanten
int rotatingSpeed = 1;                // 1: Grob, 50 U/min; 2: Mittel, 40 U/min;
                                      // 3: Fein 35 U/min
double criticalTemperature = 60.0;    // Grad Celsius
double criticalFlow = 6.0;            // Liter/Stunde


//für Temperatursensor 
OneWire ds(20); // on pin 20 (a 4.7K resistor is necessary)


//für Servo
Servo ruettelservo;




void setup() 
{
  //Frequenzumrichter Trommelmotor 
  pinMode(CONTROLLINO_D0, OUTPUT);
  pinMode(CONTROLLINO_D1, OUTPUT);
  pinMode(CONTROLLINO_D2, OUTPUT);
  
  //Frequenzumrichter Kippmotor
  pinMode(CONTROLLINO_D3, OUTPUT);
  pinMode(CONTROLLINO_D4, OUTPUT);
  pinMode(CONTROLLINO_D5, OUTPUT);
  
  //Statuslampen
  pinMode(CONTROLLINO_D7, OUTPUT);
  pinMode(CONTROLLINO_D8, OUTPUT);
  pinMode(CONTROLLINO_D9, OUTPUT);

  //Transportmotor
  pinMode(CONTROLLINO_R0, OUTPUT);
 
  //Pumpenmotor
  pinMode(CONTROLLINO_R2, OUTPUT);

  //Rüttelservo
  ruettelservo.attach(13);

  //Abstandsensor
  pinMode(CONTROLLINO_A0, INPUT);

  //Durchflusssensor
  pinMode(CONTROLLINO_A6, INPUT_PULLUP);

  //Taster
  pinMode(CONTROLLINO_A4, INPUT);

  //Kippsensor
  pinMode(CONTROLLINO_A1, INPUT);

  

  
  Serial.begin(9600);

  //Timeout auf 90ms damit die Kommunikation schneller funktioniert
  Serial.setTimeout(90);
  Serial.begin(9600);
  //Uebertragen der Drehfrequenz
  while (Serial.available() == 0){ delay(1000);}
  int frequency = (Serial.readString()).toInt();
  
  switch (frequency){
    case 50 : rotatingSpeed = 1;
              break;
    case 40 : rotatingSpeed = 2;
              break;
    case 35 : rotatingSpeed = 3;
              break;
  }
}


void loop() {  
  //Listen for Event without Timeout
  if(event==0 || event==11)
  {
    setOperatingLamp();
    recieveData(false);
    setOperatingLamp();
  }
  //Listen for Event with Timeout
  else
  {
    setOperatingLamp();
    recieveData(true);
    setOperatingLamp();
  }

  eventbasedControl();

  
  sendData();
}

//SENSORFUNKTIONEN

void readAndWriteTemperature()
{
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius;

  if ( !ds.search(addr)) 
  {
  ds.reset_search();
  delay(250);
  return;
  }


  if (OneWire::crc8(addr, 7) != addr[7]) 
  {
  return;
  }

  // the first ROM byte indicates which chip
  switch (addr[0]) {
  case 0x10:
  type_s = 1;
  break;
  
  case 0x28:
  type_s = 0;
  break;
  
  case 0x22:
  type_s = 0;
  break;
  
  return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1); // start conversion, with parasite power on at the end

  delay(1000); // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr); 
  ds.write(0xBE); // Read Scratchpad

  for ( i = 0; i < 9; i++) { // we need 9 bytes
  data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) 
  {
  raw = raw << 3; // 9 bit resolution default
  if (data[7] == 0x10) 
  {
  // "count remain" gives full 12 bit resolution
  raw = (raw & 0xFFF0) + 12 - data[6];
  }
  } 
  else 
  {
  byte cfg = (data[4] & 0x60);
  // at lower res, the low bits are undefined, so let's zero them
  if (cfg == 0x00) raw = raw & ~7; // 9 bit resolution, 93.75 ms
  else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
  else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;

  temperature = celsius;

  if(temperature > criticalTemperature) errorcode = 1;

}


void readAndWriteFlow() 
{
  
}

bool criticalTilt()
{
  int sensorValue = analogRead(CONTROLLINO_A1);
  //Serial.println(sensorValue);

  if(sensorValue>=150)
  {
    return true;
  }
  else if(sensorValue<150)
  {
    return false;
  }
}

bool buttonPressed()
{
  int sensorValue;
  sensorValue = analogRead(CONTROLLINO_A4);
  //Serial.println(sensorValue);
  
  if(sensorValue>=50)
  {
    return true;
  }
  else if(sensorValue<50)
  {
    return false;
  }
}


bool criticalDistanceReached()
{
  double sensorValue = analogRead(CONTROLLINO_A0);
  //Serial.println(sensorValue);
  //eingeteachte kritische Distanz erreicht?
  if(sensorValue>500.0)
  {
    return true;
  }
  else
  {
    return false;
  }
}



//KOMMUNIKATIONSFUNKTIONEN

void recieveData(bool timeout)
{
  int timeoutcounter = 0;
  if(timeout)
  {
    while(Serial.available()==0)
    {
      timeoutcounter++;
      delay(50);
      if(timeoutcounter==40)  //Timeout nach 2 Sekunden
      {
        event=11;
        errorcode=3;
        break;
      }
    }
    if(timeoutcounter!=40)
    {
      String incoming = Serial.readString();
      int tempevent = incoming.toInt();
      if(tempevent!=event)
      {
        terminated = false;
        event=tempevent;
      }
    }
    
  }
  else
  {
    while(Serial.available()==0)
    {
      delay(50);
    }
    
    String incoming = Serial.readString();
    int tempevent = incoming.toInt();
    if(tempevent!=event)
    {
      terminated = false;
      event=tempevent;
    }
  }
}

void sendData()
{
  int terminatedstatus;
  if(terminated) terminatedstatus = 1;
  else terminatedstatus = 0;

  int tempdistance;
  if(criticalDistanceReached()) tempdistance = 1;
  else tempdistance = 0;
  
  //Status, event sind globale Variablen
  String out = "S;"+ String(terminatedstatus) +";"
              + "C;"+ String(errorcode) +";"
              + "T;" + String(temperature) + ";"
              + "D;" + String(flow) + ";"
              + "A;" + String(tempdistance) + ";"
              + "E;" + String(event);
  Serial.println(out);

  //TODO flow
}


//STEUERUNGSFUNKTIONEN
void setOperatingLamp()
{
  digitalWrite(CONTROLLINO_D7, LOW);
  digitalWrite(CONTROLLINO_D8, LOW);
  digitalWrite(CONTROLLINO_D9, LOW);

  if(event==0)
  {
    digitalWrite(CONTROLLINO_D8, HIGH);
  }
  else if(event==11)
  {
    if(errorcode==0) digitalWrite(CONTROLLINO_D8, HIGH);
    else digitalWrite(CONTROLLINO_D9, HIGH);
      
  }
  else 
  {
    digitalWrite(CONTROLLINO_D7, HIGH);
  }
}

void eventbasedControl()
{
  switch(event) 
  {
    //Schleifkörper einfüllen
    case 1:
    if(!criticalDistanceReached())
    {
      digitalWrite(CONTROLLINO_R0, HIGH);
    }
    else
    {
      digitalWrite(CONTROLLINO_R0, LOW);
      terminated=true;
    }
    break;
    
    
    //Vorpumpen
    case 2:
    digitalWrite(CONTROLLINO_R2, HIGH);
    
    terminated=true;
    readAndWriteTemperature();
    readAndWriteFlow();
    break;


    //Trommel starten
    case 3:  
    if(rotatingSpeed==1)
    {
      //10Herz
      digitalWrite(CONTROLLINO_D0, LOW);
      digitalWrite(CONTROLLINO_D1, HIGH);
      digitalWrite(CONTROLLINO_D2, HIGH);
      terminated=true;
    }
    else if(rotatingSpeed==2)
    {
      //20Herz
      digitalWrite(CONTROLLINO_D0, HIGH);
      digitalWrite(CONTROLLINO_D1, LOW);
      digitalWrite(CONTROLLINO_D2, HIGH);
      terminated=true;
    }
    else if(rotatingSpeed==3)
    {
      //30Herz
      digitalWrite(CONTROLLINO_D0, HIGH);
      digitalWrite(CONTROLLINO_D1, HIGH);
      digitalWrite(CONTROLLINO_D2, HIGH);
      terminated=true;
    }
    readAndWriteTemperature();
    readAndWriteFlow();
    break;
    

    //Pumpe stoppen
    case 4:
    digitalWrite(CONTROLLINO_R2, LOW);
    terminated=true;
    readAndWriteTemperature();
    readAndWriteFlow();
    break;


    //Trommel stoppen
    case 5:
    digitalWrite(CONTROLLINO_D0, LOW);
    digitalWrite(CONTROLLINO_D1, LOW);
    digitalWrite(CONTROLLINO_D2, LOW);
    terminated=true;
    readAndWriteTemperature();
    readAndWriteFlow();
    break;


    //Kippen unten
    case 6:
    if(criticalTilt())
    {
      //stopp
      digitalWrite(CONTROLLINO_D3, LOW);
      digitalWrite(CONTROLLINO_D4, HIGH);
      digitalWrite(CONTROLLINO_D5, LOW);
      terminated = true;
      criticalTiltReachedHelper = true;
    }
    else
    {
      if(!criticalTiltReachedHelper)
      {
        //unten
        digitalWrite(CONTROLLINO_D3, LOW);
        digitalWrite(CONTROLLINO_D4, LOW);
        digitalWrite(CONTROLLINO_D5, HIGH);
      }
    }
    break;
    

    //Kippen oben
    case 7:
    criticalTiltReachedHelper = false;
    
    if(buttonPressed())
    {
      //stopp
      digitalWrite(CONTROLLINO_D3, LOW);
      digitalWrite(CONTROLLINO_D4, HIGH);
      digitalWrite(CONTROLLINO_D5, LOW);
      terminated=true;
    }
    else
    {
      //oben
      digitalWrite(CONTROLLINO_D3, HIGH);
      digitalWrite(CONTROLLINO_D4, LOW);
      digitalWrite(CONTROLLINO_D5, LOW);
    }
    break;


    //Rüttler starten
    case 8:
    ruettelservo.write(0); //Position 1 ansteuern mit dem Winkel 0°
    delay(200); //Das Programm stoppt für 0.2 Sekunden
    ruettelservo.write(180); //Position 2 ansteuern mit dem Winkel 180°
    delay(200);//Das Programm stoppt für 0.2 Sekunden
    terminated=true;
    break;


    //Rüttler stoppen
    case 9:
    ruettelservo.write(0);
    terminated=true;
    break;


    //Ausgangszustand
    case 10:
    //Stoppe Umlaufmotor
    digitalWrite(CONTROLLINO_R0, LOW);
    
    //Stoppe Pumpe
    digitalWrite(CONTROLLINO_R2, LOW);
    
    //Stoppe Trommelmotor
    digitalWrite(CONTROLLINO_D0, LOW);
    digitalWrite(CONTROLLINO_D1, LOW);
    digitalWrite(CONTROLLINO_D2, LOW);

    //nach oben Kippen bis taster gedrückt ist
    if(buttonPressed())
    {
      //stopp
      digitalWrite(CONTROLLINO_D3, LOW);
      digitalWrite(CONTROLLINO_D4, HIGH);
      digitalWrite(CONTROLLINO_D5, LOW);
      terminated=true;

      //resete alle Variablen
      temperature = 0;     //defaultwert
      flow = 100;             //defaultwert
      dist = 100;             //defaultwert
      errorcode = 0;

      terminated = true;
    }
    else
    {
      //oben
      digitalWrite(CONTROLLINO_D3, HIGH);
      digitalWrite(CONTROLLINO_D4, LOW);
      digitalWrite(CONTROLLINO_D5, LOW);
    }

    break;
  }

  //Stopp
  if(event==11)
  {
    
    //Stoppe Trommelmotor
    digitalWrite(CONTROLLINO_D0, LOW);
    digitalWrite(CONTROLLINO_D1, LOW);
    digitalWrite(CONTROLLINO_D2, LOW);

    //Stoppe Umlaufmotor
    digitalWrite(CONTROLLINO_R0, LOW);
    
    //Stoppe Pumpe
    digitalWrite(CONTROLLINO_R2, LOW);

    //Stoppe Kippmotor
    digitalWrite(CONTROLLINO_D3, LOW);
    digitalWrite(CONTROLLINO_D4, HIGH);
    digitalWrite(CONTROLLINO_D5, LOW);

    terminated = true;
  }
}
