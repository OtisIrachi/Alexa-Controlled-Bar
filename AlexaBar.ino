//*************************************************************************************
// AlexaBar.ino
// USES WEMOS D1 R32 board ONLY!!
// Bar and Closet Door Controller.
// Uses two stepper motors and CNC Shield.
// stepperX drives the outside door, stepperY drives the Bar itself.
// I/O tricky, different pin #'s for UNO, CNC Shield and WEMOS D1 R32 
// Use Board Manager "WEMOS D1 MINI ESP32"
// Cut A4 and A5 wire on WEMOS D1 R32 Board to allow SDA and SCL to work
// Jumper SDA to A4 and SCL to A5
// Cut resistor on CNC Sheild
// Commands:
// Open Bar - "Alexa turn on bar"
// Open Door Only - "Alexa turn on door"
// Close bar-  "Alexa turn off door"
//
// By RCI 12-19-2020
//*************************************************************************************

#define ESPALEXA_MAXDEVICES 3

#include <Wire.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h> 
#include <Espalexa.h>
// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
// if you don't know your display address, run an I2C scanner sketch
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);  

String messageStatic = "Static message";
String messageToScroll = "This is a scrolling message";

const char* ssid = "your_ssid";
const char* password = "your_password";

#define bitA4 38   // Dummy Input for SDA to allow CNC SDA to work
#define bitA5 39   // Dummy Input for SCL to allow CNC SCL to work
#define SDA 21     // WEMOS D1 R32 (SDA) = IO21
#define SCL 22     // WEMOS D1 R32 (SCL) = IO22

// I/O Definitions***************************************
#define LIGHTS             3   // RX Pin  ***used***
#define UNUSED             1   // unused TX Pin
#define X_STEP_PIN         26  // X-Step  ***used***
#define Y_STEP_PIN         25  // Y-Step  ***used***  
#define POS1               17  // Z-Step  ***used*** 
#define X_DIR_PIN          16  // X-Dir   ***used***
#define Y_DIR_PIN          27  // Y-Dir   ***used***
#define POS2               14  // Z-Dir   ***used***
#define X_ENABLE_PIN       12  // EN      ***used***

#define BarLeft            13  // X- LS   ***used***
#define BarRght            5   // Y- LS   ***used***
#define DoorLeft           23  // Z- LS   ***used***
#define DoorRght           19  // 12 = SpnEn  ***used***
#define POS3               18  // 13 = SpnDir ***used***
// E-Stop Jumper ties driectly to RESET Pin of Arduino

boolean wifiConnected = false;
unsigned long time_now = 0;
// Stepper Variables ****************************************************
#define open 1
#define closed 0
#define OFF 1
#define ON 0
#define motorInterfaceType 1
float fast_speed; 
float creep_speed; 
int limitsw_speed; 
int coast;             
long motor_steps;
boolean door_r_latch, door_l_latch, bar_r_latch, bar_l_latch;
boolean pos1flag, pos2flag, pos3flag;
long barlength;
long doorlength;
long runfullspd, runrampspd;
long timer;
int case_num;
float accelfull, accelcreep;
// Create a new instance of the AccelStepper class:
AccelStepper stepperX = AccelStepper(motorInterfaceType, X_STEP_PIN, X_DIR_PIN);
AccelStepper stepperY = AccelStepper(motorInterfaceType, Y_STEP_PIN, Y_DIR_PIN);
Espalexa espalexa;
//*****************************************************************************
void move_bar(byte dirc, long num_of_steps, float targetspeed) 
{
digitalWrite(X_ENABLE_PIN, LOW); // disable all steppers

if(dirc == open)
  {
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Bar is Opening  ");     
  // Run CW ********************  
  stepperY.setMaxSpeed(targetspeed);
  stepperY.setCurrentPosition(0);
  stepperY.setAcceleration(accelfull);
  stepperY.moveTo(runfullspd - 4600);
  
  while (stepperY.currentPosition() < runfullspd - 4600) 
     {    
     stepperY.run();  // Move Stepper into position 
     espalexa.loop();  
     }
  stepperY.setCurrentPosition(runfullspd - 4600);   
  stepperY.moveTo(runrampspd); 
  stepperY.setAcceleration(accelcreep);   // a larger number is faster
  stepperY.setMaxSpeed(creep_speed); 
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Waiting for LtLS");   
  while ((stepperY.distanceToGo() != 0))  
     { 
     stepperY.run();  // Move Stepper into position 
     if((digitalRead(BarLeft) == 0) && (dirc == open))
        {
        stepperY.stop();
        delay(1000);
        return;
        }
     }
  }// End If dirc

if(dirc == closed)
  {
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Bar is Closing  "); 
  
  // Run CCW ********************** 
  stepperY.setMaxSpeed(targetspeed);
//  stepperX.setCurrentPosition(0);
  stepperY.setAcceleration(accelfull);
  stepperY.moveTo(500);
  
  while (stepperY.currentPosition() > 500) 
     {
     espalexa.loop();     
     stepperY.run();  // Move Stepper into position  
     }
  stepperY.setCurrentPosition(500);   
  stepperY.moveTo(0); 
  stepperY.setAcceleration(accelcreep);   // a larger number is faster
  stepperY.setMaxSpeed(creep_speed); 
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Waiting for RtLS");      
  while ((stepperY.distanceToGo() != 0))  
     { 
     stepperY.run();  // Move Stepper into position 
     if((digitalRead(BarRght) == 0) && (dirc == closed))
        {
        stepperY.stop();
        delay(1000);
        return;
        }
     }
  }// End If dirc
 
  return;
}
//*****************************************************************************
void move_door(byte dirc, long num_of_steps, float targetspeed) 
{

digitalWrite(X_ENABLE_PIN, LOW); // disable all steppers

if(dirc == open)
  {
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Door is Opening ");     
  // Run CW ********************  
  stepperX.setMaxSpeed(targetspeed);
  stepperX.setCurrentPosition(0);
  stepperX.setAcceleration(accelfull);
  stepperX.moveTo(runfullspd);
  
  while (stepperX.currentPosition() < runfullspd) 
     {
     espalexa.loop();      
     stepperX.run();  // Move Stepper into position  
     }
  stepperX.setCurrentPosition(runfullspd);   
  stepperX.moveTo(runrampspd); 
  stepperX.setAcceleration(accelcreep);   
  stepperX.setMaxSpeed(creep_speed); 
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Waiting for LtLS");   
  while ((stepperX.distanceToGo() != 0))  
     { 
     stepperX.run();  // Move Stepper into position 
     if((digitalRead(DoorLeft) == 0) && (dirc == open))
        {
        stepperX.stop();
        delay(1000);
        return;
        }
     }
  }// End If dirc

if(dirc == closed)
  {
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Door is Closing"); 

    
  // Run CCW ********************** 
  stepperX.setMaxSpeed(targetspeed);
//  stepperX.setCurrentPosition(0);
  stepperX.setAcceleration(accelfull);
  stepperX.moveTo(500);
  
  while (stepperX.currentPosition() > 500) 
     {
     espalexa.loop();     
     stepperX.run();  // Move Stepper into position  
     }
  stepperX.setCurrentPosition(500);   
  stepperX.moveTo(0); 
  stepperX.setAcceleration(accelcreep);   // a larger number is faster
  stepperX.setMaxSpeed(creep_speed); 
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Waiting for RtLS");      
  while ((stepperX.distanceToGo() != 0))  
     { 
     stepperX.run();  // Move Stepper into position 
     if((digitalRead(DoorRght) == 0) && (dirc == closed))
        {
        stepperX.stop();
        delay(1000);
        return;
        }
     }
  }// End If dirc
 
  return;
      
}
//*************************************************************************************
void mdelay(int period)
{
    time_now = millis();
    while(millis() < time_now + period)
       {
       espalexa.loop(); 
       }
}
//*************************************************************************************
// Open Bar (POS3)
void openbar(uint8_t brightness)
{
  if (brightness == 255) 
     {
     pos3flag = 1;
     }
  else 
     {
     pos3flag = 1;
     }
}
//*************************************************************************************
// Open Closet (POS3)
void opendoor(uint8_t brightness)
{
  if (brightness == 255) 
     {
     pos2flag = 1;
     }
  if (brightness == 0) 
     {
     pos1flag = 1;
     }
}
//*************************************************************************************
// Close Barn Door (POS3)
void closedoor(uint8_t brightness)
{
  if (brightness == 255) 
     {
     pos1flag = 1;
     }
  else 
     {
     pos1flag = 1;
     }
}
//*****************************************************************************
void read_switches() 
{

   lcd.setCursor(0,0); 
   lcd.print("BL");
   lcd.setCursor(2,0); 
   lcd.print(bar_l_latch);
   lcd.setCursor(4,0); 
   lcd.print("BR");
   lcd.setCursor(6,0); 
   lcd.print(bar_r_latch);
   lcd.setCursor(8,0); 
   lcd.print("DL");
   lcd.setCursor(10,0); 
   lcd.print(door_l_latch);
   lcd.setCursor(12,0); 
   lcd.print("DR");
   lcd.setCursor(14,0); 
   lcd.print(door_r_latch);  
       
   mdelay(500);       
           
   //digitalWrite(X_ENABLE_PIN, HIGH); // disable all Steppers
}
//*****************************************************************************
void read_latch_state() 
{

   if(digitalRead(BarLeft) == 0) bar_l_latch = 1;
   if(digitalRead(BarRght) == 0) bar_r_latch = 1;   
   if(digitalRead(DoorLeft) == 0) door_l_latch = 1;
   if(digitalRead(DoorRght) == 0) door_r_latch = 1;   
   if(digitalRead(BarLeft) == 1) bar_l_latch = 0;
   if(digitalRead(BarRght) == 1) bar_r_latch = 0;   
   if(digitalRead(DoorLeft) == 1) door_l_latch = 0;
   if(digitalRead(DoorRght) == 1) door_r_latch = 0; 
   read_switches();
     
}
//*****************************************************************************
void initialize_steppers() 
{
  digitalWrite(LIGHTS, OFF);
  digitalWrite(X_ENABLE_PIN, LOW); // disable all steppers
  stepperX.setMaxSpeed(2000);
  stepperY.setMaxSpeed(2000);
  
           //1234567890123456          
  lcd.setCursor(0,1); 
  lcd.print("Init Bar...     "); 

  stepperY.setSpeed(-500);     
  while(digitalRead(BarRght) != 0)
     { 
     stepperY.runSpeed();  // Move Stepper into position 
     if(digitalRead(BarRght) == 0) 
        {
        stepperX.stop();
        break;
        }
     }
  mdelay(1000);    
  if(digitalRead(BarRght) == 0) bar_r_latch = 1;
  lcd.setCursor(0,1); 
  lcd.print("Bar is Away!    ");  
  mdelay(1000);
  
           //1234567890123456  
  lcd.setCursor(0,1); 
  lcd.print("Init Door...    "); 
  stepperX.setSpeed(-500);     
  while(digitalRead(DoorRght) != 0)
     { 
     stepperX.runSpeed();  // Move Stepper into position 
     if(digitalRead(DoorRght) == 0) 
        {
        stepperX.stop();
        break;
        }
     }
  mdelay(1000);
  if(digitalRead(DoorRght) == 0) door_r_latch = 1; 
  lcd.setCursor(0,1);
           //1234567890123456 
  lcd.print("Door is Closed! "); 
  lcd.setCursor(0,0); 
           //1234567890123456
  lcd.print("                ");
  read_switches(); 
  timer = 0;
  
}
//***************************************************************************
void setup()
{
  pinMode( bitA4, INPUT); 
  pinMode( bitA5, INPUT); 
  Wire.begin(SDA, SCL); 
    //*****Init LCD Display*****************
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(0,0); 
           //1234567890123456
  lcd.print("BarMover by RCI "); 
  delay(2000);
  //**************************************  
   
  pinMode(X_STEP_PIN   , OUTPUT);
  pinMode(X_DIR_PIN    , OUTPUT);
  pinMode(Y_STEP_PIN   , OUTPUT);
  pinMode(Y_DIR_PIN    , OUTPUT);
  pinMode(X_ENABLE_PIN , OUTPUT);
  pinMode(POS1         , INPUT_PULLUP);
  pinMode(POS2         , INPUT_PULLUP);
  pinMode(POS3         , INPUT_PULLUP);  
  pinMode(BarLeft      , INPUT_PULLUP);
  pinMode(BarRght      , INPUT_PULLUP);
  pinMode(DoorLeft     , INPUT_PULLUP);
  pinMode(DoorRght     , INPUT_PULLUP);
  pinMode(LIGHTS       , OUTPUT);
  
  digitalWrite(LIGHTS, OFF);
  digitalWrite(X_ENABLE_PIN, HIGH); // disable all steppers
  
  case_num = 99;
  coast = 50;
  runfullspd = 66000;
  runrampspd = 80000;
  accelfull = 1000;
  accelcreep = 300;
  fast_speed = 6000;
  creep_speed = 600;
  limitsw_speed = 100;
  door_r_latch = 0; 
  door_l_latch = 0; 
  bar_r_latch = 0; 
  bar_l_latch = 0;
  pos1flag = 0;
  pos2flag = 0;
  pos3flag = 0;


  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
    {   
    mdelay(100);
    } 

  lcd.setCursor(0,1);
           //1234567890123456
  lcd.print(WiFi.localIP()); 
  lcd.print("   ");
  delay(2000);

  espalexa.addDevice("bar", openbar, 255); 
  espalexa.addDevice("door", opendoor, 255); 
  //espalexa.addDevice("bar closed", closedoor, 255);  

  espalexa.begin(); 

  initialize_steppers();
   
}
//***************************************************************************
void loop()
{
  
   espalexa.loop(); 
//   mdelay(1); 


   //******************************************* 
   // Move Bar Away and Close door
   // Read the off switch only if bar is right, or door is left
   if ((digitalRead(POS1) == LOW) || (pos1flag == 1)) 
      {
      while ((digitalRead(POS1) == LOW) || (pos1flag == 1)) 
         {
         pos1flag = 0;        
         case_num = 0;  
         }
      }
   //*******************************************    
   // Open Barn door Only 
   if (((digitalRead(POS2) == LOW) && (bar_r_latch == 1)) || ((pos2flag == 1) && (bar_r_latch == 1)))
      {
      //lcd.setCursor(0,1); 
               //1234567890123456
      //lcd.print("Open Door Only  ");        
      while ((digitalRead(POS2) == LOW) || (pos2flag == 1))
         {
         // if door is not open, open door
         pos2flag = 0;
         case_num = 1; 
         }
       }
   //*******************************************   
   // Open Barn Door and Move Bar IN     
   if (((digitalRead(POS3) == LOW) && (door_r_latch == 1) && (bar_r_latch == 1)) || ((pos3flag == 1) && (door_r_latch == 1) && (bar_r_latch == 1)))
      {
      //lcd.setCursor(0,1); 
               //1234567890123456
      //lcd.print("Open BD Bar In  ");         
      while ((digitalRead(POS3) == LOW) || (pos3flag == 1))
         {
         pos3flag = 0;
         case_num = 4;    
         } 
      } 
   //******************************************* 
   // Open Barn Door and Move Bar IN     
   if (((digitalRead(POS3) == LOW) && (door_l_latch == 1)) || ((pos3flag == 1) && (door_l_latch == 1)))
      {  
      while ((digitalRead(POS3) == LOW) || (pos3flag == 1))
         {
         pos3flag = 0;
         case_num = 2;    
         } 
      }
   //*******************************************  
   // Open Barn door Only 
   if (((digitalRead(POS2) == LOW) && (bar_l_latch == 1)) || ((pos2flag == 1) && (bar_l_latch == 1)))
      {
      while ((digitalRead(POS2) == LOW) || (pos2flag == 1))
         {  
         // if door is not open, open door
         pos2flag = 0;
         case_num = 3; 
         }
      }  

// **********Motor Motion Moves*****************             
  switch(case_num)
  {
  //********************************************    
  case 0:  // Close Both Doors

   read_switches();
   // if Bar is In, move it away
   if((bar_l_latch == 1) && (door_l_latch == 1))
   {
   digitalWrite(LIGHTS, OFF); 
            //1234567890123456 
   lcd.setCursor(0,1); 
   lcd.print("Moving Bar Away."); 
   motor_steps = barlength;  
   move_bar(closed, motor_steps, fast_speed); 
   read_latch_state(); 
   mdelay(300);
   lcd.setCursor(0,1); 
   lcd.print("Bar is Away!    ");       
   case_num = 99;
   delay(2000);
   }

   // if door is open, close it
   if(door_l_latch == 1)
   {   
            //1234567890123456 
   lcd.setCursor(0,1); 
   lcd.print("Closing Barn Dr ");  
   motor_steps = doorlength;         
   move_door(closed, motor_steps, fast_speed);
   read_latch_state(); 
   mdelay(300);  
   lcd.setCursor(0,1); 
   lcd.print("Door & Bar Clsd!");      
   case_num = 99;
   delay(2000);
   }
   
  break;
  //*******************************************     
  case 1:  // Open Barn Door Only
           // If Bar is In, Send to Out
           // If Barn Door is already Open, leave alone
   read_switches();        
   // if Bar is IN, move it out
   if(bar_l_latch == 1)
   {
   digitalWrite(LIGHTS, OFF);  
            //1234567890123456
   lcd.setCursor(0,1); 
   lcd.print("Moving Bar Away."); 
   motor_steps = barlength;      
   move_bar(closed, motor_steps, fast_speed); 
   read_latch_state(); 
   mdelay(300);        
   lcd.setCursor(0,1); 
   lcd.print("Bar is Away!    ");  
   case_num = 99;
   delay(2000);
   }

      // if door is closed, open it
   if((door_r_latch == 1) && (bar_r_latch == 1))
   {   
            //1234567890123456
   lcd.setCursor(0,1); 
   lcd.print("Opening Door... ");  
   motor_steps = doorlength;         
   move_door(open, motor_steps, fast_speed); 
   read_latch_state();   
   mdelay(300);   
   lcd.setCursor(0,1); 
   lcd.print("Door is Open!   ");      
   case_num = 99;
   delay(2000);
   }
      
  break;
  //*******************************************     
  case 2:

   read_switches();
   // if door is open, move bar In
   if((door_l_latch == 1) && (bar_r_latch == 1)) 
   {
            //1234567890123456 
   lcd.setCursor(0,1); 
   lcd.print("Moving Bar In...");  
   motor_steps = barlength;                       
   move_bar(open, motor_steps, fast_speed);
   read_latch_state(); 
   mdelay(300);    
   lcd.setCursor(0,1); 
   digitalWrite(LIGHTS, ON); 
   lcd.print("Bar is Ready!   ");  
   case_num = 99;
   delay(2000);
   }   
     
  break;
  //*******************************************     
  case 3:

   read_switches();
   // if Bar is IN, move it out
   //if((bar_r_latch == 1) && (door_l_latch == 1))
   if((bar_l_latch == 1) && (door_l_latch == 1))
   {
   digitalWrite(LIGHTS, OFF); 
            //1234567890123456 
   lcd.setCursor(0,1); 
   lcd.print("Moving Bar Away."); 
   motor_steps = barlength;      
   move_bar(closed, motor_steps, fast_speed);
   read_latch_state();    
   mdelay(300);   
   lcd.setCursor(0,1); 
   lcd.print("Bar is Away!    ");  
   case_num = 99;
   delay(2000);
   }
   
  break;
  //*******************************************     
  case 4:

   read_switches();
      // if door is closed, open it
   if((door_r_latch == 1) && (bar_r_latch == 1))
   {   
            //1234567890123456 
   lcd.setCursor(0,1); 
   lcd.print("Opening Barn Dr ");  
   motor_steps = doorlength;         
   move_door(open, motor_steps, fast_speed);
   read_latch_state();  
   mdelay(300);   
   lcd.setCursor(0,1); 
   lcd.print("Door is Open!   ");      
   case_num = 99;
   delay(2000);
   }

   // if door is open, move bar In
   if((door_l_latch == 1) && (bar_r_latch == 1)) 
   {
            //1234567890123456
   lcd.setCursor(0,1); 
   lcd.print("Moving Bar In...");  
   motor_steps = barlength;                       
   move_bar(open, motor_steps, fast_speed);
   read_latch_state();   
   mdelay(300);
   digitalWrite(LIGHTS, ON); 
   lcd.setCursor(0,1); 
   lcd.print("Bar is Ready!   ");  
   case_num = 99;
   delay(2000);
   }  
   
  break; 
  //*******************************************     
  case 99:
  
   digitalWrite(X_ENABLE_PIN, HIGH); // disable all Steppers
   
  break; 
           
  }// End switch

}// End Loop
