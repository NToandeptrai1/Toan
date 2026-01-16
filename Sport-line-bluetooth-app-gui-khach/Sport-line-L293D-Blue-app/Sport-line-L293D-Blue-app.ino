#include <AFMotor.h>
//#include <NewPing.h>
//#include <Servo.h>

#include <SoftwareSerial.h>
/*************************Bluetooth************************************************/
SoftwareSerial Bluetooth(18,19); // Arduino(RX, TX) - HC-05 Bluetooth (TX, RX)
int dataIn;

/*************************Define Line Track pins************************************/
//Line Tracking IO define
#define LT_R digitalRead(A0)
#define LT_M digitalRead(A1) 
#define LT_L digitalRead(A2)

#define LECH_L -1
#define LECH_R  1
int     LECH = 0;

AF_DCMotor motor1(2);  //motor-trai-noi pin 2 L293D
AF_DCMotor motor2(1); //motor- phai- noi pin 3 L293D

int speedCar    = 220;     // toc do 50 - 255.
int MAX_SPEED   = 255;
int lech_A = 0;
int lech_B = 15;
bool goesForward = false;

void line();
void setup() 
{   
      Bluetooth.begin(9600); // Default baud rate of the Bluetooth module
      Bluetooth.setTimeout(5);
      delay(20);
      
      Serial.begin(9600);

      pinMode(LT_R,INPUT);
      pinMode(LT_M,INPUT);
      pinMode(LT_L,INPUT);
}
void loop() {
          // Check for incoming data
  if (Bluetooth.available() > 0) {
      dataIn = Bluetooth.read();      // Read the data
      Serial.println(dataIn);
          //Move car
      if (dataIn == 0) {stopRobot();}
      if (dataIn == 2) {goAhead(200,200);}
      if (dataIn == 7) {goBack(200,200);}
      if (dataIn == 4) {turnLeft(200,200);}
      if (dataIn == 5) {turnRight(200,200);}
      if (dataIn == 9)  {turnLeft(200,200);}
      if (dataIn == 10) {turnRight(200,200);}
              
          //line, ultra
      while (dataIn == 24) {
        line();
        stopRobot();         
      }
      if (dataIn == 26) {
        stopRobot();
      }
          // Mecanum wheels speed
      if (dataIn > 30 & dataIn < 100) {
        speedCar =map(dataIn,30,100,60,250);
      }
    
    
  }              //END IF BLUETOOTH

}
void goAhead(int speedCar1, int speedCar2)
{
  motor1.setSpeed(speedCar1 + lech_A); //Define maximum velocity
  motor1.run(FORWARD); //rotate the motor clockwise
  motor2.setSpeed(speedCar2 + lech_B); //Define maximum velocity
  motor2.run(FORWARD); //rotate the motor clockwise
}
void goBack(int speedCar1, int speedCar2)
{
  motor1.setSpeed(speedCar1 + lech_A); 
  motor1.run(BACKWARD); 
  motor2.setSpeed(speedCar2 + lech_B); 
  motor2.run(BACKWARD); 
}
void turnRight(int speedCar1, int speedCar2)
{
  motor1.setSpeed(speedCar1 + lech_A); 
  motor1.run(FORWARD); 
  motor2.setSpeed(speedCar2 + lech_B);
  motor2.run(BACKWARD); 
}
void turnLeft(int speedCar1, int speedCar2)
{
  motor1.setSpeed(speedCar1 + lech_A);
  motor1.run(BACKWARD); 
  motor2.setSpeed(speedCar2 + lech_B);  
  motor2.run(FORWARD); 
}

void stopRobot()
{
  motor1.setSpeed(0);
  motor2.run(RELEASE); 
  motor2.setSpeed(0);
  motor2.run(RELEASE); 
}

void line(){
  do{
          if(LT_M){
              goAhead(150,150);
          }
          else if(LT_R) {
              LECH = LECH_R;
              turnRight(150,150);
              while(LT_R);                             
          }   
          else if(LT_L) {
              LECH = LECH_L;
              turnLeft(150,150);
              while(LT_L);  
          } 
          else if (LECH == LECH_R){
              turnRight(100,100);
          } 
          else if (LECH == LECH_L){
              turnLeft(100,100);
          } 
          
          if (Bluetooth.available() > 0) {
              dataIn = Bluetooth.read();
          }  
   }while(dataIn !=25);
}
