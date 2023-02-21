#include "sensorRead.h"

void setup_Analog(){
  pinMode(vOn,OUTPUT);
  pinMode(iOn,OUTPUT);
  pinMode(relayPin,OUTPUT);
  pinMode(sw,INPUT);
}

float getV(){
  digitalWrite(vOn,HIGH);
  digitalWrite(iOn,LOW);

  int mn = 1200;
  int temp;
  stable();
  for(int i=0;i<count;i++){
    if((temp=analogRead(A0))<mn) mn = temp;
    yield();
  }

  digitalWrite(vOn,LOW);
  digitalWrite(iOn,LOW);

  float out = 0.6527*(756-mn)-0.9876;
  if(out<50) out = 0;

  return(out);   //(247*(756-mn)/380);
}

float getI(){
  float mx = 0;
  int v = 0;
  digitalWrite(vOn,LOW);
  digitalWrite(iOn,HIGH);
  
  for(int i = 0;i<10000;i++){
    v = analogRead(A0);
    if(v>mx) mx=v;    
  }

  digitalWrite(vOn,LOW);
  digitalWrite(iOn,LOW);
  
  //mx = ((mx)/sqrt(2));
  mx = mx*0.0111-0.271;
  if(mx<0.1) mx=0;
  return(mx);
}

bool relayOn(bool b){
  if(b){                          
    digitalWrite(relayPin, relay_active); 
    delay(1000);
    if(getV()<100) return true;
    else return false;
  }
  else{
    digitalWrite(relayPin, !relay_active);
    delay(1000);
    if(getV()>200) return true;
    else return false;
  }
}

int switchAct(){
  unsigned long t0 = millis();
  while(digitalRead(sw)==HIGH) yield();
  t0 = millis()-t0;
  if((t0>100)&&(t0<10000))return 1;   //toggle
  else if (t0>10000) return 2;        //reset
  else return 0;                      //passive
}

bool getState(){
  if(getV()>150) return true;
    else return false;
}

void stable(){
  for(int i = 0;i<5;i++){
    int v = analogRead(A0);  
    yield();
  }
}
