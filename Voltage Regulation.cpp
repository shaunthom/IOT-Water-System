//VCC OF I2C DISPLAY TO 5V
//GND OF I2C DISPLAY TO GND OF NODEMCU
//SDA OF I2C DISPLAY TO D1 OF NODEMCU
//SCL TO D2 OF NODEMCU

//OUTPUT OF ASC712 MODULE TO A0 OF NODEMCU
//VCC OF ACS MODULE TO +3V OF NODEMCU
//GND OF ACS MODULE TO GND OF NODEMCU


#define PUMP D4            // PUMP in NodeMCU at pin GPIO2 (D4).
//int PUMP = 2;      // this is also correct as D4 of nodemcu is pin 2 (GPIO2) of esp8266
#define ONSW D3           //Manual on SW at pin GPIO5 (D3).
#define OFFSW D5            //Manual on SW at pin GPIO4 (D5).
#define PUMP D4           //Manual on SW at pin GPIO2 (D4).
#define LOWV D7           //Manual on SW at pin GPIO14 (D7).
#define HIGHV D6          //Manual on SW at pin GPIO0 (D6).

#include <Wire.h>           
 #include <LiquidCrystal_I2C.h>    
 LiquidCrystal_I2C lcd(0x27,16,2);   //3F
float AcsValue=0.0;
float Samples=0.0;
float Average=0.0;
float AcsValueF=0.0; 
void setup() 
{
   
pinMode(ONSW, INPUT_PULLUP); 
pinMode(OFFSW, INPUT_PULLUP); 
pinMode(LOWV, INPUT_PULLUP); 
pinMode(HIGHV, INPUT_PULLUP); 
pinMode(PUMP, OUTPUT); 
digitalWrite(PUMP,LOW); // INITIALLY PUMP OFF
lcd.begin();      
lcd.backlight();  
// lcd.noBacklight();  //to turn off backlight
 lcd.setCursor(0,0);  
  // lcd.print("   GOD IS KIND  ");  
  // lcd.setCursor(0,1);  
 //  lcd.print(" AND HELPFUL ");  
 //   delay(1000);     
   lcd.setCursor(0,0);  
   lcd.print("   WELCOME  ");  
   lcd.setCursor(0,1);  
   lcd.print("                  ");  
   digitalWrite(PUMP, HIGH);
                       
delay(1000);            
digitalWrite(PUMP, LOW); 
delay(1000); 
 digitalWrite(PUMP, HIGH);
                        
delay(1000);           
digitalWrite(PUMP, LOW);  // THIS LOOP INSERTED FOR TEST

}
   


void loop() 
{
unsigned int x=0;
Samples=0.0;

  for (int x = 0; x < 150; x++)  ////Get 150 samples
  { 
  AcsValue = analogRead(A0);     //Read current sensor output  
  Samples = Samples + AcsValue;  //Add read samples together
  delay (3); // delay for ADC settling before reading next sample 
  }
Average=Samples/150.0; //Taking Average of Samples 

AcsValueF = (2.5 - (Average * (5 / 1024.0)) )/0.66; //[(OFFSET - READ VOLTAGE)/SENSITIVITY]
// As per the datasheet sensitivity of module is 185 FOR 5A MOULE,100 for 20A Module and 66 for 30A Module
// here we are using 30 A module and hence sensitivity is 66

        
   
    lcd.setCursor(0, 0);  // first position first line
    lcd.print("Current = ");
    lcd.print(AcsValueF);
    lcd.print("A ");
    delay(300);   //for reading display
    if ( AcsValueF > 3)  //>3 Ampere
    {
      lcd.setCursor(0, 0); //  
      lcd.print("    OVER LOAD       ");
      digitalWrite (PUMP,LOW);
      lcd.setCursor(0, 1);  //
      lcd.print("    PUMP OFF     ");
    }
    if (digitalRead(LOWV)==HIGH)
      {
       lcd.setCursor(0, 0); // 
       lcd.print("    UNDER VOLTAGE       ");
       digitalWrite (PUMP,LOW);
       lcd.setCursor(0, 1);  //
       lcd.print("    PUMP OFF     ");
      
      }
    if (digitalRead(HIGHV)==HIGH)

      {
       lcd.setCursor(0, 0); // 
       lcd.print("   OVER VOLTAGE       ");
       digitalWrite (PUMP,LOW);
       lcd.setCursor(0, 1);  //
        lcd.print("    PUMP OFF     ");
      }
      if ((digitalRead(LOWV)==LOW)&& (digitalRead(HIGHV)==LOW)) // if voltage is above low V and less than high V setting
      {
        if (digitalRead(ONSW)==LOW)  // ON SW pressed
        {delay(10); // FOR CHECKING VALID KEY PRESS
        }
        if (digitalRead(ONSW)==LOW)
        {
        digitalWrite (PUMP,HIGH);
        lcd.setCursor(0, 1); // FIRST POS(0),SECOND LINE(0)
        lcd.print("     PUMP ON       ");
        lcd.setCursor(0, 0); // FIRST POS(0),FIRST LINE(1)
        lcd.print("  VOLTAGE NORMAL    ");
        delay(300);   //for reading display
        }
      }

      if (digitalRead(OFFSW)==LOW)  //OFF SW PRESSED
        {delay(10); // FOR CHECKING VALID KEY PRESS
        }
        if (digitalRead(OFFSW)==LOW)
        {
        digitalWrite (PUMP,LOW);
        lcd.setCursor(0, 1); //
        lcd.print("     PUMP OFF       ");
        lcd.setCursor(0, 0); // 
        lcd.print("                ");
        delay(300);   //for reading display
        }

}
