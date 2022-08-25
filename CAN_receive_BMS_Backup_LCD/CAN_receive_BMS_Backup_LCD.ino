  // CAN Receive Example
//

#include <LiquidCrystal_I2C.h>
#include <mcp_can.h>
#include <SPI.h>
#include <SD.h>


LiquidCrystal_I2C lcd(0x27, 16, 2);

// VARIAVEIS DE CONFIGURACAO DO BMS

#define SENDCONFIG 0
#define REQUEST_CONFIG 0

int NCH=8;                // [numero de celulas]
int NTH=4;                // [numero de termistores]   
int OV=3650;              // [Sobretensão - mV]
int UV=2850;              // [Subtensão - mV]
int SHORTCIRCUIT=50000;   //[corrente de curto - mA]
int OVERCURRENT=40000;    //[sobrecorrente - mA]
int OVERTEMPERATURE=45;   //[sobretemperatura - ºC]
int GAIN_I_SENSE=2083;    //[sensibilidade do sensor de corrente - uV/A]
int DVOLT=8;              //[delta de tensão de balanceamento - mV]
int LOWVOLT=3000;         //[tensão de alerta de baixa tensão mV]
int HIGHVOLT=3500;        //[tensão de alerta de alta tensão mV]
int CANCHARGEVOLT=3200;   //[não utilizado] 
int CHARGEDVOLT=3600;       //[tensão que define que bateria está carregada mV]
int OVERVOLT_HISTERESYS=4;  //[NÃO UTILIZADO]
int DELAY_HISTERESYS=60;    //[tempo maximo de balanceamento - minutos]
int BALANCE=1;              //[define se é permitido balanceamento - binario]


long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];
char msgString[128];                        // Array to store serial string

#define CAN0_INT 2                              // Set INT to pin 2
MCP_CAN CAN0(10);                               // Set CS to pin 10

int readcan=0;
int versao,power,maxtemp, mintemp, maxvolt, minvolt;
int temp[24]={0};
int voltage[48]={0};
int Vpack,   Current1,Current2,SoC;

int* backup[]={&versao,&power,&maxtemp, &mintemp, &maxvolt, &minvolt,&Vpack,&Current1,&Current2,&SoC};
const int chipSelect = 4;
File dataFile;
const char* header="Tensão;Temp;Ver.;Pow;TMax;TMin;VMax;VMin;VPack;Corrente 1;Corrente 2;SoC";

void setup()
{
  Serial.begin(500000);
 lcd.init();
 lcd.setBacklight(HIGH);
 lcd.setCursor(0,0);
  // Initialize MCP2515 running at 16MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if(CAN0.begin(MCP_ANY, CAN_125KBPS, MCP_8MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");
  
  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input
  
  Serial.println("MCP2515 Library Receive Example...");
  delay(3000);
  if(SENDCONFIG)
  {
        //**************ENVIO DE CONFIG1***********
        byte data[8] = {0x00, 0x01, 0x02, 0xFF, 0x04, 0x05, 0x06, 0x07};
        
        data[0]=0xFF&NCH;
        data[1]=NCH>>8;
        data[2]=0xFF&NTH;
        data[3]=NTH>>8;
        data[4]=0xFF&OV;
        data[5]=OV>>8;
        data[6]=0xFF&UV;
        data[7]=UV>>8;
        //send data:  ID = 0x10000000, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
        byte sndStat = CAN0.sendMsgBuf(0x10000000, 1, 8, data);
       Serial.println("Envio configuracao 1");
        //*****************************************************
        delay(3000);
        data[0]=0xFF&SHORTCIRCUIT;
        data[1]=SHORTCIRCUIT>>8;
        data[2]=0xFF&OVERCURRENT;
        data[3]=OVERCURRENT>>8;
        data[4]=0xFF&OVERTEMPERATURE;
        data[5]=OVERTEMPERATURE>>8;
        data[6]=0xFF&GAIN_I_SENSE;
        data[7]=GAIN_I_SENSE>>8;
        //send data:  ID = 0x10000001, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
        sndStat = CAN0.sendMsgBuf(0x10000001, 1, 8, data);
        Serial.println("Envio configuracao 2");
        //*****************************************************
        delay(3000);
        data[0]=0xFF&DVOLT;
        data[1]=DVOLT>>8;
        data[2]=0xFF&LOWVOLT;
        data[3]=LOWVOLT>>8;
        data[4]=0xFF&HIGHVOLT;
        data[5]=HIGHVOLT>>8;
        data[6]=0xFF&CANCHARGEVOLT;
        data[7]=CANCHARGEVOLT>>8;
        //send data:  ID = 0x10000000, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
        sndStat = CAN0.sendMsgBuf(0x10000002, 1, 8, data);
        Serial.println("Envio configuracao 3");
        //*****************************************************
        
         delay(3000);
        data[0]=0xFF&CHARGEDVOLT;
        data[1]=CHARGEDVOLT>>8;
        data[2]=0xFF&OVERVOLT_HISTERESYS;
        data[3]=OVERVOLT_HISTERESYS>>8;
        data[4]=0xFF&DELAY_HISTERESYS;
        data[5]=DELAY_HISTERESYS>>8;
        data[6]=0xFF&BALANCE;
        data[7]=BALANCE>>8;
        //send data:  ID = 0x10000000, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
        sndStat = CAN0.sendMsgBuf(0x10000003, 1, 8, data);
        Serial.println("Envio configuracao 4");
        //*****************************************************
  }//sendconfig

  Serial.print("Iniciando cartão SD...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Cartão SD falhou, remova o cartão e reinicie o sistema");
    // don't do anything more:
  }
  else  Serial.println("Cartão SD iniciado com sucesso!");
}

int count=0;

void loop()
{
  //while(1);
  //delay(50);
  
  //**************REQUISICAO DE VARIAVEIS***********

 if(REQUEST_CONFIG)
 {
  count++;
  if(count>=60)
  {
  byte data[8] = {0x00, 0x01, 0x02, 0xFF, 0x04, 0x05, 0x06, 0x07};
  // send data:  ID = 0x11000000, Standard CAN Frame, Data length = 8 bytes, 'data' = array of data bytes to send
  byte sndStat = CAN0.sendMsgBuf(0x11000000, 1, 8, data);
  if(sndStat == CAN_OK)
  {
      Serial.println("Message Sent Successfully!");
    } else 
    {
      Serial.println("Error Sending Message...");
    }
    count=0;
  }
 }
  
  //*****************************************************
 if(readcan>=16) //se leu valores pelo CAN então printa o que possível na ordem certa:
 {
//    Serial.print("Versao BMS: ");
//    Serial.print(versao, DEC);  // print as an ASCII-encoded decimal
//    Serial.print("Power: ");
//    Serial.println(power, DEC);  // print as an ASCII-encoded decimal
//    Serial.print("Max Temp: ");
//    Serial.print(maxtemp/10, DEC);  // print as an ASCII-encoded decimal
//    Serial.print("\tMinTemp: ");
//    Serial.print(mintemp/10, DEC);  // print as an ASCII-encoded decimal     
//    Serial.print("\tMax Volt: ");
//    Serial.print(maxvolt/1000, DEC);  // print as an ASCII-encoded decimal
//    Serial.print("\tMin Volt: ");
//    Serial.println(minvolt/1000, DEC);  // print as an ASCII-encoded decimal 

    Serial.print("Current1: ");
    Serial.println(Current1, DEC);  // print as an ASCII-encoded decimal  
    Serial.print("Current2: ");
    Serial.println(Current2, DEC);  // print as an ASCII-encoded decimal
    Serial.print("Vpack: ");
    Serial.println(Vpack, DEC);  // print as an ASCII-encoded decimal
    Serial.print("SoC: ");
    Serial.println(SoC, DEC);  // print as an ASCII-encoded decimal   
  
    char j=1;
    Serial.print("\nTemp CMU1 1-8:\t");
    for(byte i = 1; i<=8; i++)
    { 
      Serial.print(temp[i], DEC);  // print as an ASCII-encoded decimal
      Serial.print("\t");
    }

    
    Serial.print("\n\n Voltages CMU1 1-16:\n");
    for(byte i = 1; i<=16; i++)
    { 
      if(i==9)
      Serial.print("\n");
      Serial.print(voltage[i], DEC);  // print as an ASCII-encoded decimal
      Serial.print("\t");
    }

   Serial.print("\n\n\n");
   readcan=0;
 }
  
  while(!digitalRead(CAN0_INT))                         // If CAN0_INT pin is low, read receive buffer
  {
    
    readcan++;
    
    CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
    
   /* if((rxId & 0x80000000) == 0x80000000)     // Determine if ID is standard (11 bits) or extended (29 bits)
      sprintf(msgString, "Extended ID: 0x%.8lX  DLC: %1d  Data:", (rxId & 0x1FFFFFFF), len);
    else
      sprintf(msgString, "Standard ID: 0x%.3lX       DLC: %1d  Data:", rxId, len);
  
    //Serial.print(msgString);
    
  
    if((rxId & 0x40000000) == 0x40000000){    // Determine if message is a remote request frame.
      sprintf(msgString, " REMOTE REQUEST FRAME");
      //Serial.print(msgString);
    } */

    if((rxId & 0x1FFFFFFF)==0x186455F4) //
     {    
         versao= (int)rxBuf[0]|((int)rxBuf[1]<<8);
         power= (int)rxBuf[6]|((int)rxBuf[7]<<8);     
     }
     if((rxId & 0x1FFFFFFF)==0x186955F4) //
     {   maxtemp= (int)rxBuf[0]|((int)rxBuf[1]<<8);   
         mintemp= (int)rxBuf[2]|((int)rxBuf[3]<<8);
         maxvolt= (int)rxBuf[4]|((int)rxBuf[5]<<8);
         minvolt= (int)rxBuf[6]|((int)rxBuf[7]<<8);
      }

     if((rxId & 0x1FFFFFFF)==0x186555F4) //
     {   Vpack= (int)rxBuf[0]|((int)rxBuf[1]<<8);   
         Current1= (int)rxBuf[2]|((int)rxBuf[3]<<8);
         Current2= (int)rxBuf[4]|((int)rxBuf[5]<<8);
         SoC= (int)rxBuf[6]|((int)rxBuf[7]<<8);
      }
      
     if((rxId & 0x1FFFFFFF)==0x18AC55F4) //temp1-4 CMU1
     {  //Serial.print("Temperaturas CMU1:");
       int j=1;
        for(byte i = 0; i<len; i=i+2)
        { temp[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
          j++;
        }
      }

       if((rxId & 0x1FFFFFFF)==0x18AD55F4) //temp5-8 CMU1
     {  //Serial.print("Temperaturas CMU1:");
       int j=5;
        for(byte i = 0; i<len; i=i+2)
        { temp[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
         // Serial.print("\tTemp ");
         // Serial.print(j,DEC);
         // Serial.print(":");
         // Serial.print(Val, DEC);  // print as an ASCII-encoded decimal
          j++;
        }//Serial.println();
      }

        if((rxId & 0x1FFFFFFF)==0x18AE55F4) //temp9-12 da CMU2
     {  //Serial.print("Temperaturas CMU2:");
       int j=9;
        for(byte i = 0; i<len; i=i+2)
        { temp[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
         // Serial.print("\tTemp ");
         /// Serial.print(j,DEC);
          //Serial.print(":");
          //Serial.print(Val, DEC);  // print as an ASCII-encoded decimal
          j++;
        }//Serial.println();
      }

        if((rxId & 0x1FFFFFFF)==0x18AF55F4) //temp13-16 da CMU2
     {  //Serial.print("Temperaturas CMU2:");
       int j=13;
        for(byte i = 0; i<len; i=i+2)
        { temp[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
         // Serial.print("\tTemp ");
         // Serial.print(j,DEC);
         // Serial.print(":");
         // Serial.print(Val, DEC);  // print as an ASCII-encoded decimal
          j++;
        }//Serial.println();
      }

       if((rxId & 0x1FFFFFFF)==0x186B55F4) //tensao 1-4 CMU1
     { // Serial.print("Tensões CMU1:");
       int j=1;
        for(byte i = 0; i<len; i=i+2)
        { voltage[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
         // Serial.print("\tVcell ");
         // Serial.print(j,DEC);
         // Serial.print(":");
         // Serial.print(Val, DEC);  // print as an ASCII-encoded decimal
          j++;
        }//Serial.println();
      }

        if((rxId & 0x1FFFFFFF)==0x186C55F4) //tensoes CMU1
     {  
       int j=5;
        for(byte i = 0; i<len; i=i+2)
        { voltage[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
          j++;
        }
      }
        if((rxId & 0x1FFFFFFF)==0x186D55F4) //tensoes CMU1
     {  
       int j=9;
        for(byte i = 0; i<len; i=i+2)
        {  voltage[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
           j++;
        }Serial.println();
      }

     if((rxId & 0x1FFFFFFF)==0x186E55F4) //tensoes CMU1
     {  
       int j=13;
        for(byte i = 0; i<len; i=i+2)
        { voltage[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
          j++;
        }
      }
     if((rxId & 0x1FFFFFFF)==0x186F55F4) //tensoes CMU2
     {  
       int j=17;
        for(byte i = 0; i<len; i=i+2)
        { voltage[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
           j++;
        }
      }
     if((rxId & 0x1FFFFFFF)==0x187055F4) //tensoes CMU2
     { 
       int j=21;
        for(byte i = 0; i<len; i=i+2)
        { voltage[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
          j++;
        }
      }

      if((rxId & 0x1FFFFFFF)==0x187155F4) //tensoes CMU2
     { 
       int j=25;
        for(byte i = 0; i<len; i=i+2)
        { voltage[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
          j++;
        }
      }

      if((rxId & 0x1FFFFFFF)==0x187255F4) //tensoes CMU2
     { 
       int j=29;
        for(byte i = 0; i<len; i=i+2)
        { voltage[j]= (int)rxBuf[i]|((int)rxBuf[i+1]<<8);
          j++;
        }
      }
      if((rxId & 0x1FFFFFFF)==0x11000001) //configs
       {  Serial.print("Configurações:1");
        
         int Val= (int)rxBuf[0]|((int)rxBuf[1]<<8);
         Serial.print("\tN_CHANNELS: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[2]|((int)rxBuf[3]<<8);
         Serial.print("\t\tN_THERMISTOR: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[4]|((int)rxBuf[5]<<8);
         Serial.print("\tOVERVOLTAGE: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[6]|((int)rxBuf[7]<<8);
         Serial.print("\tUNDERVOLTAGE:: ");
         Serial.println(Val,DEC);         
      }
        if((rxId & 0x1FFFFFFF)==0x11000002) //temp1-4 CMU1
       {  Serial.print("Configurações:2");
        
         unsigned int Val= (int)rxBuf[0]|((int)rxBuf[1]<<8);
         Serial.print("\tSHORT_CIRCUIT: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[2]|((int)rxBuf[3]<<8);
         Serial.print("\tOVERCURRENT: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[4]|((int)rxBuf[5]<<8);
         Serial.print("\tOVERTEMPERATURE: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[6]|((int)rxBuf[7]<<8);
         Serial.print("\tGAIN_I_SENSE:: ");
         Serial.println(Val,DEC);         
      }
         if((rxId & 0x1FFFFFFF)==0x11000003) //temp1-4 CMU1
       {  Serial.print("Configurações:3");
        
         int Val= (int)rxBuf[0]|((int)rxBuf[1]<<8);
         Serial.print("\tDVOLT: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[2]|((int)rxBuf[3]<<8);
         Serial.print("\t\tLOWVOLTAGE: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[4]|((int)rxBuf[5]<<8);
         Serial.print("\tHIGHVOLTAGE: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[6]|((int)rxBuf[7]<<8);
         Serial.print("\tCANCHARGEVLT: ");
         Serial.println(Val,DEC);         
      }
        if((rxId & 0x1FFFFFFF)==0x11000004) //temp1-4 CMU1
       {  Serial.print("Configurações:4");
        
         int Val= (int)rxBuf[0]|((int)rxBuf[1]<<8);
         Serial.print("\tCHARGEDVOLT: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[2]|((int)rxBuf[3]<<8);
         Serial.print("\tOV_HISTERESYS: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[4]|((int)rxBuf[5]<<8);
         Serial.print("\t DelayHisteresys: ");
         Serial.print(Val,DEC);
         Val= (int)rxBuf[6]|((int)rxBuf[7]<<8);
         Serial.print("\tBALANCE: ");
         Serial.println(Val,DEC);         
      }
      
    //Serial.println();
   // delay(1);
  }
  
   //Serial.println("..");
  // delay(10);

  //backup dos dados recebidos na CAN
  //abre o arquivo de backup
  dataFile = SD.open("datalog.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.println(*header);
    for(size_t i=0; i<48; i++){
          dataFile.print(voltage[i]);
    }
    dataFile.print(";");
    for(size_t i=0; i<24; i++){
          dataFile.print(temp[i]);
    }
    dataFile.print(";");
    for(int* &j: backup){
          dataFile.print(*j);
    }     
    dataFile.println(" ");
    dataFile.close();
 }

//controlador LCD

    lcd.setCursor(0, 0);
    lcd.print("V: ");
    lcd.setCursor(3, 0);
    lcd.print(Vpack);
    lcd.setCursor(7, 0);
    lcd.print("V");
    lcd.setCursor(9, 0); 
    lcd.print(SoC);
    lcd.setCursor(11, 0);
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print("C1: ");
    lcd.setCursor(3, 1);
    lcd.print(Current1);
    lcd.setCursor(9, 1);
    lcd.print("A");
  
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
