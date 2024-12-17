#include "timerISR.h"
#include "helper.h"
#include "periph.h"
#include "serialATmega.h"
#include "spiAVR.h"
#include <stdlib.h>
#include <avr/pgmspace.h>
#include "characters.h"
#include "stdio.h"
#include <avr/eeprom.h>
using namespace std;



#define NUM_TASKS 6


unsigned int counter = 0;
unsigned int iterator = 0;
unsigned int passed = 0;
int _width = 128;
int _height = 128;

bool drop = false;
int moveShip = 56;
bool fire = false;

bool alienFire = false;

unsigned int user_score = 0;
unsigned int highest_score = 0;

#define EEADDR 0x3FF
#define EEDATA highest_score
volatile uint16_t EEByte;
volatile uint16_t EEByte1;


int height = 0;
bool armsOpen = false;

int alienXPos = 0;
int alienYPos = 30;

signed int xMove;


int cnt = 0;
int cnr;
bool gameOver = false;

bool didHit = false;

int scoreCntr = 0;
int scoreCntr2 = 0;
int scoreCntr3 = 0;

int dropAmount = 3;

unsigned int cannonHit;


//unsigned char numAliensKilled = 0;

unsigned int numAliensKilled = 0;
int formerSpeed = 0;

bool musicOn;


//Task struct for concurrent synchSMs implmentations
typedef struct _task{
	signed 	 char state; 		//Task's current state
	unsigned long period; 		//Task period
	unsigned long elapsedTime; 	//Time elapsed since last task tick
	int (*TickFct)(int); 		//Task tick function
} task;

struct AlienObject {
   int xPosition;
   int yPosition;
   int Status;
};


AlienObject aMissile[1];
AlienObject uMissile;
AlienObject cannon;
AlienObject AlienPos[4][4];


//TODO: Define Periods for each task
// e.g. const unsined long TASK1_PERIOD = <PERIOD>
//unsigned long buzzPeriod = 1;
// 0x0
unsigned long buzzPeriod = 1;
unsigned long laserPeriod = 50; //all was 30
unsigned long dispPeriod = 50;
unsigned long alienPeriod = 50;
unsigned long checkPeriod = 50;
const unsigned long GCD_PERIOD = 1/* TODO: Calulate GCD of tasks */;

task tasks[NUM_TASKS]; // declared task array with 5 tasks

void SetUpGame() {
    //highest_score = 200;
    char high_score[4];
    //eeprom_write_word((uint16_t*)EEADDR, EEDATA);

    EEByte = eeprom_read_word((uint16_t*)EEADDR);
    char *str0 = "SCORE:";
    GotoXY(1,1);
    WriteString(str0, COLOR_WHITE);
    char *str1 = "0";
    GotoXY(7,1);
    WriteString(str1, COLOR_WHITE);
    char *str2 = "HISCORE:";
    GotoXY(10,1);
    WriteString(str2, COLOR_WHITE);

    //char *str3="200";
   //int b = 270;
    sprintf(high_score,"%d", EEByte);
    GotoXY(18,1);
    WriteString(high_score, COLOR_WHITE);


    //GotoXY(18,1);
    //WriteString(str3, COLOR_WHITE);
   //  DrawBitmap(0, 30, &TopInvader1[0], 16, 10);
   //  DrawBitmap(20, 30, &TopInvader1[0], 16, 10);
   //  DrawBitmap(40, 30, &TopInvader1[0], 16, 10);
   //  DrawBitmap(60, 30, &TopInvader1[0], 16, 10);
  
   //  DrawBitmap(0, 45, &MiddleInvader1[0], 16, 10);
   //  DrawBitmap(20, 45, &MiddleInvader1[0], 16, 10);
   //  DrawBitmap(40, 45, &MiddleInvader1[0], 16, 10);
   //  DrawBitmap(60, 45, &MiddleInvader1[0], 16, 10);

   //  DrawBitmap(0, 60, &BottomInvader1[0], 16, 10);
   //  DrawBitmap(20, 60, &BottomInvader1[0], 16, 10);
   //  DrawBitmap(40, 60, &BottomInvader1[0], 16, 10);
   //  DrawBitmap(60, 60, &BottomInvader1[0], 16, 10);

   //  DrawBitmap(0, 75, &BottomInvader1[0], 16, 10);
   //  DrawBitmap(20, 75, &BottomInvader1[0], 16, 10);
   //  DrawBitmap(40, 75, &BottomInvader1[0], 16, 10);
   //  DrawBitmap(60, 75, &BottomInvader1[0], 16, 10);

   //  DrawBitmap(56, 120, &PlayerShip0[0], 18, 8);
   int xOFFSET = 0;
   int yOFFSET = 30;
   for (unsigned int i = 0; i < 4; i++) {
      for (unsigned int j = 0; j < 4; j++) {
         AlienPos[i][j].xPosition = xOFFSET;
         AlienPos[i][j].yPosition = yOFFSET;
         AlienPos[i][j].Status = 1;
         yOFFSET += 15;
      }
      if (xOFFSET >= 80) {
         xOFFSET = 80;
      }
      else {
         xOFFSET += 20;
      }
      yOFFSET = 30;
   }
   xMove = 1;
   cnr = 0;
   musicOn = true;

   cannon.Status = 1;
   cannon.xPosition = 56;
   cannon.yPosition = 120;
   cannonHit = 0;
   drop = 2;
}

bool sameCoordinates(AlienObject Obj1,unsigned char Width1,unsigned char Height1,AlienObject Obj2,unsigned char Width2,unsigned char Height2) {
   return ((Obj1.xPosition + Width1 > Obj2.xPosition)&(Obj1.xPosition < Obj2.xPosition + Width2)&(Obj1.yPosition + Height1 > Obj2.yPosition)&(Obj1.yPosition < Obj2.yPosition + Height2));
}

void TimerISR() {
    //TODO: sample inputs here
	for ( unsigned int i = 0; i < NUM_TASKS; i++ ) {                   // Iterate through each task in the task array
		if ( tasks[i].elapsedTime == tasks[i].period ) {           // Check if the task is ready to tick
			tasks[i].state = tasks[i].TickFct(tasks[i].state); // Tick and set the next state for this task
			tasks[i].elapsedTime = 0;                          // Reset the elapsed time for the next tick
		}
		tasks[i].elapsedTime += GCD_PERIOD;                        // Increment the elapsed time by GCD_PERIOD
	}
}

int rightMost() {
   int currPos = 0;
   int width = 4;
   int height = 0;
   int largest = 0;

   while(width >= 0) {
      height = 0;
      while(height < 4) {
         if (AlienPos[width][height].Status == 1) {
            currPos = AlienPos[width][height].xPosition;
            if(currPos > largest) {
               largest = currPos;
            }
         }
         height++;
      }
      width--;
   }

   return largest;
}

int leftMost() {
   int width = 0;
   int height = 0;
   int smallest = (60 * 2);

   while(width < 4) {
      height = 0;
      while(height < 4) {
         if(AlienPos[width][height].Status == 1) {
            if (AlienPos[width][height].xPosition < smallest) {
               smallest = AlienPos[width][height].xPosition;
            }
         }
         height++;
      }
      width++;
   }

   return smallest;
}


//TODO: Define, for each task:
// (1) enums and
// (2) tick functions

enum soundBuzzer{INIT, buzzOn, buzzOff};
int soundBuzzer(int state) {
    switch(state) {
        case(INIT):
            if (musicOn == true) {
               state = buzzOn;
               counter = 0;
            }
            else if (musicOn == false) {
               state = INIT;
            }
            break;

        case(buzzOn):
            if (counter > 1750) {
                counter = 0;
                state = buzzOff;
            }
            else {
                state = buzzOn;
            }
            break;

        case(buzzOff):
           if (counter < 250) {
              
                state = buzzOff;
            }
            else {
                counter = 0;
                state = INIT;
            }
            break;

        default:
            break;

    }

    switch(state) {
         case(INIT):
            break;

         case(buzzOn):
            if (counter < 250) {
                ICR1 = 7096; //20ms pwm period
                OCR1A = ICR1/2;
            }
            else if (counter > 250 && counter < 500) {
                OCR1A = ICR1;
            }
            else if (counter < 750) {
                ICR1 = 8096; //20ms pwm period
                OCR1A = ICR1/2;
            }
            else if (counter > 750 && counter < 1000) {
                OCR1A = ICR1;
            }
            else if (counter < 1250) {
                ICR1 = 9096; //20ms pwm period
                OCR1A = ICR1/2;
            }
            else if (counter > 1250 && counter < 1500) {
                OCR1A = ICR1;
            }
            else if (counter < 1750) {
                ICR1 = 9896; //20ms pwm period
                OCR1A = ICR1/2;
            }
            counter++;
            break;
    
         case(buzzOff):
            OCR1A = ICR1;
            counter++;
            break;
    
        default:
            break;

    }

    return state;
}

enum laserCannon{INIT2, leftMove, rightMove, fireButton, buttonInterm};
int laserCannon(int state) {
    switch(state) {
      case INIT2:
         if (ADC_read(0) <= 450 && cannonHit < 15) {
            if (ADC_read(1) >= 800) {
               fire = true;
               state = buttonInterm;
               //state = fireButton;
            }
            else {
               state = leftMove;
            }
         }
         else if (ADC_read(0) >= 800 && cannonHit < 15) {
            if (ADC_read(1) >= 800) {
               state = fireButton;
            }
            else {
               state = rightMove;
            }
         }
         else if (ADC_read(1) >= 800 && cannonHit < 15) {
             state = fireButton;
         }
         break;

      case leftMove:
         if (ADC_read(0) <= 450) {
            if (ADC_read(1) >= 800) {
               fire = true;
               state = leftMove;
               //state = buttonInterm;
               //state = fireButton;
            }
            else {
               state = leftMove;
            }
         }
         else {
            state = INIT2;
         }
         break;

      case rightMove:
         if (ADC_read(0) >= 800) {
            if (ADC_read(1) >= 800 ) {
               fire = true;
               //state = buttonInterm;
               state = rightMove;
               //state = fireButton;
            }
            else {
               state = rightMove;
            }
         }
         else {
            state = INIT2;
         }
         break;
      
      case fireButton:
         fire = false;
         state = buttonInterm;
         break;
      
      case buttonInterm:
         if (ADC_read(1) >= 800) {
            fire = false;
            state = buttonInterm;
         }
         else if (ADC_read(1) < 800) {
            state = INIT2;
         }
         break;

      default:
         break;
    }

    switch(state) {
       case INIT2:
         break;

       case leftMove:
         if (cannon.xPosition == 0) {
            cannon.xPosition = 0;
         }
         else {
            cannon.xPosition -= 2;
         }
         break;

      case rightMove:
         if (cannon.xPosition >= 112) {
            cannon.xPosition = 112;
         }
         else {
            cannon.xPosition += 2;
         }
         break;
      
      case fireButton:
         fire = true;
         break;

      case buttonInterm:
         break;

      default:
         break;

    }

    return state;
}

enum updateAliens{UPDATE};
int updateAliens(int state) {
   switch(state) {
      case UPDATE:
         state = UPDATE;
         break;

      default:
         break;
   }

   switch(state) {
      case UPDATE:
         drop = false;
         if (rightMost() + xMove >= 114 || leftMost() + xMove < 0) {
            xMove = -xMove;
            drop = true;
         }
         for (unsigned int i = 0; i < 4; i++) {
            for (unsigned int k = 0; k < 4; k++) {
               if (drop == true) {
                  if (AlienPos[i][k].yPosition >= 110 || numAliensKilled == 20) {
                     gameOver = true;
                     cannon.Status = 0;
                  }
               }
               else {
                  AlienPos[i][k].xPosition += xMove;
               }
            }
         }
         if (armsOpen) {
            armsOpen = false;
         }
         else if (!armsOpen) {
            armsOpen = true;
         }
         break;

      default:
         break;
   }

   return state;
}

enum showAliens{INIT3};
int showAliens(int state) {
   switch(state) {
      case INIT3:
         state = INIT3;
         break;

      default:
         break;
   }

   switch(state) {
      case INIT3:
   
         char *str0 = "SCORE:";
         GotoXY(1,1);
         WriteString(str0, COLOR_WHITE);

         char use_score[4];
         sprintf(use_score, "%d", user_score);
         GotoXY(7,1);
         WriteString(use_score, COLOR_WHITE);

         for (unsigned int k = 0; k < 4; k++) {
            for (unsigned int j = 0; j < 4; j++) {
               if (AlienPos[k][j].Status == 1) {
                  DrawBitmap(AlienPos[k][j].xPosition - xMove, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                  if (drop == true) {
                     DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                     AlienPos[k][j].yPosition += 2;
                  }
                  switch(j)  {
                     case 0: 
                        if(armsOpen) {
                           //DrawBitmap(AlienPos[k][j].xPosition - xMove, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                           DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &TopInvader2[0], 16, 10);
                        }else {
                          // DrawBitmap(AlienPos[k][j].xPosition - xMove, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                           DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &TopInvader1[0], 16, 10);
                        }
                        break;
                     case 1: 
                        if(armsOpen) {
                          // DrawBitmap(AlienPos[k][j].xPosition - xMove, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                           DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &MiddleInvader2[0], 16, 10);
                        }else {
                          // DrawBitmap(AlienPos[k][j].xPosition - xMove, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                           DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &MiddleInvader1[0], 16, 10);
                        }
                        break;
                     case 2: 
                        if(armsOpen) {
                          // DrawBitmap(AlienPos[k][j].xPosition - xMove, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                           DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &BottomInvader2[0], 16, 10);
                        } else {
                           DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &BottomInvader1[0], 16, 10);
                        }
                        break;
                     default: 
                        if(armsOpen) {
                          // DrawBitmap(AlienPos[k][j].xPosition - xMove, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                           DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &BottomInvader2[0], 16, 10);
                        }
                        else {
                          // DrawBitmap(AlienPos[k][j].xPosition - xMove, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
                           DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &BottomInvader1[0], 16, 10);
                        }
                     }

                }
               //  else if (AlienPos[k][j].Status == 0) {
               //     DrawBitmap(AlienPos[k][j].xPosition, AlienPos[k][j].yPosition, &Gone[0], 16, 10);
               //  }
             }
         }
         if (cannon.Status == 1) {
            DrawBitmap(cannon.xPosition, cannon.yPosition, &PlayerShip0[0], 18, 8);
         }
         else if (cannon.Status == 0){
            DrawBitmap(cannon.xPosition, cannon.yPosition, &GoneShip[0], 18, 8);
            gameOver = true;
         }
         break;

      default:
         break;

   }

   return state;
}



enum updateFire{INIT4};
int updateFire(int state) {
   switch(state) {
      case INIT4:
         state = INIT4;
         break;

      default:
         break;

   }
   switch(state) {
      case INIT4:
         bool drop = false;
         unsigned int numMissiles = 0;
         
         unsigned int fireColumn = 0;
         int columnArr[4];

         while(drop == false && numMissiles < 1) {
            if(aMissile[numMissiles].Status == 0) {
               drop = true;
            }
            else {
               numMissiles++;
            }
         }

         if (drop) {
            unsigned int c = 0;
            unsigned int r = 0;
            unsigned int columnIndex = 0;
            while (c < 4) {
            r = 3;
               while(r >= 0) {
                  if (AlienPos[c][r].Status == 1) {
                     columnArr[columnIndex] = c;
                     columnIndex++;
                     break;
                  }
                  r--;
               }
               c++;
            }

            fireColumn = rand() % columnIndex + 1;

            r = 3;
            while(r >= 0) {
               if (AlienPos[fireColumn][r].Status == 1) {
                  aMissile[numMissiles].Status = 1;
                  aMissile[numMissiles].xPosition = AlienPos[fireColumn][r].xPosition + 8;
                  aMissile[numMissiles].yPosition = AlienPos[fireColumn][r].yPosition + 10;
                  break;
               }
               r--;
            }

            while(aMissile[numMissiles].Status == 1) {
               DrawBitmap(aMissile[numMissiles].xPosition, aMissile[numMissiles].yPosition, &Missile[0], 4, 14);
               if(aMissile[numMissiles].yPosition < 125) {
                   aMissile[numMissiles].yPosition += 1;
               }
               else if (sameCoordinates(aMissile[numMissiles], 4, 14, cannon, 16, 10)) {
                  DrawBitmap(aMissile[numMissiles].xPosition, aMissile[numMissiles].yPosition, &GoneMissile[0], 4, 14);
                  aMissile[numMissiles].Status = 0;
                  cannonHit++;
                  if (cannonHit> 15) {
                     aMissile[numMissiles].Status = 0;
                     gameOver = true;
                     cannon.Status = 0;
                  }
               }
               else if(aMissile[numMissiles].yPosition == 125) {
                  aMissile[numMissiles].Status = 0;
                  DrawBitmap(aMissile[numMissiles].xPosition, aMissile[numMissiles].yPosition, &GoneMissile[0], 4, 14);
               }
            }
         }

         if (fire == true) {
               uMissile.Status = 1;
               uMissile.xPosition = cannon.xPosition + 8;
               uMissile.yPosition = 112;
               while (uMissile.Status == 1 && uMissile.yPosition >= 30) {
                  DrawBitmap(uMissile.xPosition, uMissile.yPosition, &Missile[0], 4, 14);
                  for (unsigned int i = 0; i < 4; i++) {
                        for (unsigned j = 0; j < 4; j++ ) {
                           if (uMissile.Status == 1 && AlienPos[i][j].Status == 1 && numAliensKilled < 20) {
                              if (sameCoordinates(uMissile,4,14,AlienPos[i][j],16,10)) {
                                 uMissile.Status = 0;
                                 AlienPos[i][j].Status = 0;
                                 DrawBitmap(AlienPos[i][j].xPosition, AlienPos[i][j].yPosition, &Gone[0], 16, 10);
                                 DrawBitmap(uMissile.xPosition, uMissile.yPosition, &GoneMissile[0], 4, 14);
                                 numAliensKilled++;

                                 if (numAliensKilled < 7) {
                                    user_score += 20;
                                    break;
                                 }

                                 else if (numAliensKilled >= 7 && numAliensKilled < 12) {
                                    scoreCntr++;
                                    formerSpeed = xMove;
                                    // if (xMove > 0) {
                                    //     xMove += 1;
                                    // }
                                    
                                    if (scoreCntr == 1) {
                                       xMove = xMove * 2;
                                    }
                                   
                                    user_score += 40;
                                    break;
                                 }
                                 
                                 else if (numAliensKilled >= 12 && numAliensKilled < 14) {
                                    // if (xMove > 0) {
                                    //     xMove += 2;
                                    // }
                                    scoreCntr2++;
                                    if (scoreCntr2 == 1) {
                                       xMove = xMove * 2;
                                    }
                     
                                   user_score += 60;
                                   break;
                                 }
                                 
                                 else if (numAliensKilled >= 14 && numAliensKilled < 16) {
                                    // if (xMove > 0) {
                                    //     xMove += 2;
                                    // }
                                    scoreCntr3++;
                                    if (scoreCntr3 == 1) {
                                       xMove = xMove * 3;
                                    }
                     
                                    user_score += 80;
                                    break;
                                 }

                                 else if (numAliensKilled == 16) {
                                    gameOver = true;
                                    cannon.Status = 0;
                                    break;
                                 }
                                 fire = false;
                              }
                             }
                           }
                        }

                        if (uMissile.yPosition == 30) {
                           DrawBitmap(uMissile.xPosition, uMissile.yPosition, &GoneMissile[0], 4, 14);
                        }

                        uMissile.yPosition -= 1;
                     }
              fire = false; 
            }
         break;

      default:
         break;
   }

   return state;
}

enum checkGame{INIT5};
int checkGame(int state) {
   switch(state) {
      case INIT5:
         state = INIT5;
         break;

      default:
         break;
   }
   switch(state) {
      case INIT5:
         while (gameOver) {
            cnr++;
            for (unsigned int i = 0; i < 4; i++) {
               for (unsigned int j = 0; j < 4; j++) {
                  AlienPos[i][j].Status = 0;
               }
            }
            
            if (cnr == 1) {
               ClearScreen();
               SetScreen(COLOR_BLACK);
            }
            //musicOn = false;
            OCR1A = ICR1;
            //PORTB = SetBit(PORTB, 1, 0);

            EEByte = eeprom_read_word((uint16_t*)EEADDR);
            if (user_score > EEByte) {
               highest_score = user_score;
               eeprom_write_word((uint16_t*)EEADDR, EEDATA);
            }

            char *str7 = "GAMEOVER";
            GotoXY(7,5);
            WriteString(str7,COLOR_RED);

            EEByte1 = eeprom_read_word((uint16_t*)EEADDR);
            char hScore[4];
            sprintf(hScore, "%d", EEByte1);

            char uScore[4];
            sprintf(uScore, "%d", user_score);

            char *str2 = "HISCORE:";
            GotoXY(5,7);
            WriteString(str2, COLOR_WHITE);

            GotoXY(13,7);
            WriteString(hScore, COLOR_WHITE);

            char *str3 = "CURRSCORE:";
            GotoXY(5,9);
            WriteString(str3, COLOR_WHITE);

            GotoXY(15,9);
            WriteString(uScore, COLOR_WHITE);

            if (ADC_read(2) > 800) {
               numAliensKilled = 0;
               user_score = 0;
               gameOver = false;
               xMove = 1;
               armsOpen = false;
               formerSpeed = 0;
               scoreCntr = 0;
               scoreCntr2 = 0;
               scoreCntr3 = 0;
               SetScreen(COLOR_BLACK);
               SetUpGame();
            }
         }
         break;

      default:
         break;
   }

   return state;
}



int main(void) {
    //TODO: initialize all your inputs and ouputs
     DDRC = 0x00; PORTC = 0xFF;  //PORTC input 
     DDRB = 0xFF; PORTB = 0x00;  //PORTB output
    // DDRD = 0xFF; PORTD = 0x00;  //PORTD output

     ADC_init();   // initializes ADC
    //init_sonar(); // initializes sonar
     serial_init(9600);

   //   while (true) {
   //     serial_println(ADC_read(0));
   //   }
   // while (true) {
   //    serial_println(ADC_read(2));
   // }

     SPI_INIT();
     Init_Display();
     SetScreen(COLOR_BLACK);
     SetUpGame();
     
    // GameInit();
    // char *str = "Hello World!";
    // GotoXY(1,1);
    // WriteString(str, COLOR_WHITE);

    //DrawPixel(60,60, YELLOW);

     //FillRect(60,60,61,61, COLOR_WHITE);
     //FillRect(56,120,72,128, COLOR_WHITE);
    //  DrawBitmap(0, 25, &SmallEnemy10pointA[0], 16, 10);
    //  DrawBitmap(20, 25, &SmallEnemy10pointA[0], 16, 10);
    //  //GotoXY(60,75);
    //  //DrawRect(60,75, 100, 50, COLOR_WHITE);
    //  DrawBitmap(56, 116, &PlayerShip0[0], 18, 8);

    //TODO: Initialize tasks here
    // e.g. tasks[0].period = TASK1_PERIOD
    // tasks[0].state = ...
    // tasks[0].timeElapsed = ...
    // tasks[0].TickFct = &task1_tick_function;
    // while (true) {
    
    TCCR1A |= (1 << WGM11) | (1 << COM1A1); //COM1A1 sets it to channel A
    TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11); //CS11 sets the prescaler to be 8

   //  while (true) {
   //    TCCR1A |= (1 << WGM11) | (1 << COM1A1); //COM1A1 sets it to channel A
   //    TCCR1B |= (1 << WGM12) | (1 << WGM13) | (1 << CS11); //CS11 sets the prescaler to be 8

   //      ICR1 = 20096; //20ms pwm period

   //      OCR1A = ICR1/2;
   //  }

    unsigned int i = 0;
    tasks[i].state = INIT;
    tasks[i].period = buzzPeriod;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &soundBuzzer;
    i++;
    tasks[i].state = INIT2;
    tasks[i].period = laserPeriod;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &laserCannon;
    i++;
    tasks[i].state = UPDATE;
    tasks[i].period = alienPeriod;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &updateAliens;
    i++;
    tasks[i].state = INIT3;
    tasks[i].period = dispPeriod;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &showAliens;
    i++;
    tasks[i].state = INIT4;
    tasks[i].period = dispPeriod;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &updateFire;
    i++;
    tasks[i].state = INIT5;
    tasks[i].period = checkPeriod;
    tasks[i].elapsedTime = tasks[i].period;
    tasks[i].TickFct = &checkGame;
    

    TimerSet(GCD_PERIOD);
    TimerOn();

    while (1) {}

    return 0;
}