# include <Wire.h>
# include <Adafruit_RGBLCDShield.h>
# include <utility/Adafruit_MCP23017.h>
# include <EEPROM.h>
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
//The next few define lines replace the values of UP, DOWN etc with the integer values so we don't have to remember what UP & DOWN are.
#define UP 8
#define DOWN 4
#define LEFT 16
#define RIGHT 2
#define SELECT 1




//These are the structures contained within a house
typedef struct Property  //For values like 'Level' or 'On Time'
{
  char names[6];
  int value;
} Properties;

typedef struct Device  //For devices like 'Light' or 'Heat'
{
  char names[13];
  uint8_t propertyCount;
  Property* property;
} Devices;

typedef struct Room  //For devices like 'Light' or 'Heat'
{
  char names[11];
  uint8_t deviceCount;
  Device* device;
} Rooms;

typedef struct Floor  //For the individual floors
{
  char names[11];
  uint8_t roomCount;
  Room* rooms;
} Floors;

typedef struct House
{
  uint8_t floorCount;
  Floor* floors;  //Each house contains X floors
} Houses;

typedef enum menuState { floorLevel, roomLevel, deviceLevel, propertyLevel} menuStates;  //The seperate states of the finite state machine

void setup() {  //We state the serial monitor and LCD in this setup
  Serial.begin(9600);
  lcd.begin(16, 2);
  Serial.println(F("ENHANCED"));
  lcd.print("Menu");
  String _s = "";     //Not necessary when using the alternative to free memory
}
Rooms defaultRooms(Rooms room, uint8_t deviceCount, boolean lightHeat) { //This sets the default values that all rooms will have

  room.device =  (Devices*)malloc(sizeof(Devices) * deviceCount);
  room.deviceCount = deviceCount;
  if (lightHeat) {
    strcpy(room.device[0].names, "Light");  //Sets room names
    strcpy(room.device[1].names, "Heat");
    room.device[1] = defaultLH(room.device[1]);
  } else {
    strcpy(room.device[0].names, "Water");
  }
  room.device[0] = defaultLH(room.device[0]);

  return room;
}
Devices defaultLH(Devices device) { //Sets the default values for all devices
  //Default Light and Heat values
  if (strcmp(device.names, "Light") == 0 || strcmp(device.names, "Heat") == 0 || (strstr(device.names, "Lamp") != 0)) { //Compares strings
    device.property = (Properties*)malloc(sizeof(Properties) * 3);
    device.propertyCount = 3;
    strcpy(device.property[0].names, "Level");
    strcpy(device.property[1].names, "On");
    strcpy(device.property[2].names, "Off");
    device.property[0].value = 50;
    device.property[1].value = 0;
    device.property[2].value = 0;
  } else if ((strstr(device.names, "Water") != 0) ) { //Default Lamp and Water values
    device.property = (Properties*)malloc(sizeof(Properties) * 2);
    device.propertyCount = 2;
    strcpy(device.property[0].names, "On");
    strcpy(device.property[1].names, "Off");
    device.property[0].value = 0;
    device.property[1].value = 0;
  }
  return device; //Returns the device structure
}

void loop() {

  static uint8_t oldButtonState = lcd.readButtons();
  static uint8_t newButtonState;
  static boolean unreleased;
  static uint8_t menuPos[] = {0, 0, 0, 0}; //The 4 different positions in the FSM saved to an array for simplicity
  static menuStates state = floorLevel;  //Default State
  static boolean extraItem = false;
  static Houses house = initHouse();   // < - If for any reason EEPROM breaks, this should work fine but the code works on my end
  //Calls a class to initialise the house structure

  checkQuery(house);


  newButtonState = lcd.readButtons(); //Reads buttons
  if (newButtonState != oldButtonState) { //Compares with original button value
    oldButtonState = newButtonState;
    unreleased = true;

    while (unreleased) { //Waits for button to be released before continuing program
      newButtonState = lcd.readButtons();
      if (newButtonState == 0) { //This waits until the button is released before doing anything else
        unreleased = false;
      }
    }//When released the program can resume and it will take the new button state into account
  }

  switch (state) {  //This is the main FSM. There are 4 states for each level (of depth) in the menu)
    case floorLevel: //Menu for Floors (& Print Data)
      if (oldButtonState == DOWN && (house.floorCount - 1) > menuPos[0]) {  //If we press down and we arent on the final value. Go to the next value down
        menuPos[0] = menuPos[0] + 1; //Menu pos is an integer array with 4 values. This is so when we go forward then back, it stays on the same menu value (i.e. Ground -> Kitchen and then pressing back puts us back at Ground instead of the default value kitchen)
        lcdPrint(house.floors[menuPos[0]].names);

      } else if (oldButtonState == UP && menuPos[0] > 0) { //If we press up and it isnt the max value, go up
        if (extraItem) {
          extraItem = false;  //Lets the program know we arent on the 'Print Data' selection anymore, if we already were
        } else {
          menuPos[0] = menuPos[0] - 1;
        }
        lcdPrint(house.floors[menuPos[0]].names);

      }
      else if (oldButtonState == RIGHT) { //If we pressed RIGHT
        if (extraItem) {  //If we are on 'Print Data' print values to the serial monitor
          housePrint(house);
        } else {//else, we go another layer deeper into the menu and the state changes
          state = roomLevel;
          lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].names);
        }

      }
      else if (oldButtonState == DOWN && (house.floorCount) > menuPos[0] && !extraItem) { //If we press down on the final value. Drop to an exra option of print data
        extraItem = true;
        lcd.clear();
        lcd.print("Menu");
        lcd.setCursor(0, 1);
        lcd.print("Print Data");
      }
      break;
    case roomLevel: //Menu for Rooms
      if (oldButtonState == DOWN && (house.floors[menuPos[0]].roomCount - 1) > menuPos[1]) {
        menuPos[1] = menuPos[1] + 1;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].names);
      }
      else if (oldButtonState == UP && menuPos[1] > 0) {
        menuPos[1] = menuPos[1] - 1;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].names);
      }
      else if (oldButtonState == LEFT) {
        state = floorLevel;
        menuPos[1] = 0; //Resets it to the default menu position for this value
        lcdPrint(house.floors[menuPos[0]].names);

      }
      else if (oldButtonState == RIGHT) {
        state = deviceLevel;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].names);
      }
      break;
    case deviceLevel: //Menu for Devices (i.e. Lamps, Lights)
      if (oldButtonState == DOWN && (house.floors[menuPos[0]].rooms[menuPos[1]].deviceCount - 1) > menuPos[2]) {
        menuPos[2] = menuPos[2] + 1;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].names);
      }
      else if (oldButtonState == UP && menuPos[2] > 0) {
        menuPos[2] = menuPos[2] - 1;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].names);
      }
      else if (oldButtonState == LEFT) {
        state = roomLevel;
        menuPos[2] = 0;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].names);
      }
      else if (oldButtonState == RIGHT) {
        state = propertyLevel;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].property[menuPos[3]].names);
      }
      break;
    case propertyLevel: //Menu for Device properties (i.e. level, on time)
      if (oldButtonState == DOWN && (house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].propertyCount - 1) > menuPos[3]) {
        menuPos[3] = menuPos[3] + 1;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].property[menuPos[3]].names);
      }
      else if (oldButtonState == UP && menuPos[3] > 0) {
        menuPos[3] = menuPos[3] - 1;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].property[menuPos[3]].names);
      } else if (oldButtonState == LEFT) {
        state = deviceLevel;
        menuPos[3] = 0;
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].names);
      }
      else if (oldButtonState == RIGHT) {
        //The below code puts us into a function to modify the levels of the device
        house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].property[menuPos[3]].value = adjustLevel(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].property[menuPos[3]].names, house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].property[menuPos[3]].value);
        house = EEPROM.put(0, house); //Updates EEPROM
        lcdPrint(house.floors[menuPos[0]].rooms[menuPos[1]].device[menuPos[2]].property[menuPos[3]].names);
      }

  }
  oldButtonState = 0; //Resets the buttons to their default value

}


Houses initHouse() //Initialises the house
{
  /*The room define lines call a function to set default values
    The number is how many devices they will have (e.g. 1 lamp, heat & light = 3 devices)
    The True/False value determines if it has a Light and Heat (all rooms except garden do by default)*/
  Houses house;
  //Debugging the EEPROM: If the EEPROM breaks due to a power outage at a bad time, change the value FirstFloor to literally anything else, run the code once and then reload it like normal and it should work fine
  if (strcmp(EEPROM.get(0, house).floors[2].names, "FirstFloor") == 0) { //Checks if the house is already in the EEPROM 
    
    Serial.println("VALID");  //If it is, valid
    house = EEPROM.get(0, house);
    
  } else {
    Houses house;
    Floors outside;
    Floors ground;
    Floors firstFloor;
    
    outside.rooms = (Rooms *)malloc(sizeof(Rooms) * 2);
    outside.roomCount = 2;
    ground.rooms = (Rooms *)malloc(sizeof(Rooms) * 3);
    ground.roomCount = 3;
    firstFloor.rooms = (Rooms *)malloc(sizeof(Rooms) * 3);
    firstFloor.roomCount = 3;
         
    strcpy(outside.names, "Outside");
    strcpy(ground.names, "Ground");
    strcpy(firstFloor.names, "FirstFloor");

    outside.rooms[0] = defaultRooms(outside.rooms[0], 1, false);  //Defines the room
    strcpy(outside.rooms[0].names, "Garden");   //Names said room
    outside.rooms[1] = defaultRooms(outside.rooms[1], 2, true);
    strcpy(outside.rooms[1].names, "Garage");
    ground.rooms[0] = defaultRooms(ground.rooms[0], 3, true);
    strcpy(ground.rooms[0].names, "Kitchen");
    /*The next few lines define the lamps in the structure
      The structure supports as many lamps as you want with the location of the lamp to be put after the lamp name
      To add lamps you just need to add a line here and increase the value of devices by 1 per lamp*/
    strcpy(ground.rooms[0].device[2].names, "Lamp Table");  //Defines lamps for the structure
    ground.rooms[0].device[2] = defaultLH(ground.rooms[0].device[2]);
    ground.rooms[1] = defaultRooms(ground.rooms[1], 2, true);
    strcpy(ground.rooms[1].names, "Hall");
    ground.rooms[2] = defaultRooms(ground.rooms[2], 2, true);
    strcpy(ground.rooms[2].names, "LivingRoom");
    firstFloor.rooms[0] = defaultRooms(firstFloor.rooms[0], 2, true);
    strcpy(firstFloor.rooms[0].names, "Bedroom1"); //Defines the names for each room.
    firstFloor.rooms[1] = defaultRooms(firstFloor.rooms[1], 4, true);
    strcpy(firstFloor.rooms[1].names, "Bedroom2");
    strcpy(firstFloor.rooms[1].device[2].names, "Lamp Table"); //This adds a lamp to the bedroom2 table
    firstFloor.rooms[1].device[2] = defaultLH(firstFloor.rooms[1].device[2]);
    strcpy(firstFloor.rooms[1].device[3].names, "Lamp Cabinet");
    firstFloor.rooms[1].device[3] = defaultLH(firstFloor.rooms[1].device[3]);
    firstFloor.rooms[2] = defaultRooms(firstFloor.rooms[2], 2, true);
    strcpy(firstFloor.rooms[2].names, "Bathroom");


    house.floors = (Floors *)malloc(sizeof(Floors) * 3);
    house.floorCount = 3;
    
    house.floors[0] = outside;
    house.floors[1] = ground;
    house.floors[2] = firstFloor;

    EEPROM.put(0, house);
  }
  lcd.setCursor(0, 1);
  lcd.print(house.floors[0].names);
  return house;
}

//This function is an alternative to freeMemory. It works the same as it but is done in a cleaner looking way
//This is all contained within the function and looks clean so I used it instead of freeMemory (however freememory is below to show the original implementation)

int memQuery() {
  int i;
  extern int __heap_start, *__brkval;

  if (__brkval == 0)
  {
    return ((int) &i - (int) &__heap_start);
  }
  return ((int) &i - (int)__brkval);
}
/* Original Implementation of freeMemory() before adding in memQuery. They work the exact same
  #ifdef __arm__
  // should use uinstd.h to define sbrk but Due causes a conflict
  extern "C" char* sbrk(int incr);
  #else // __ARM__
  extern char *__brkval;
  #endif // __arm__

  int freeMemory() {
  char top;
  #ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
  #elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
  #else // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start; //returns the free memory between the stack & heap
  #endif // __arm__
  }
*/
void checkQuery(Houses house) {
  static char serialChar;
  if (Serial.available()) {
    serialChar = Serial.read();
    if (serialChar == 'M') {
      Serial.print(memQuery());
      Serial.println(" bytes left");
      /*
         if(Serial.available()){
          if(Serial.read() == '\n'){
          This was in both sections of code to check for newlines
          However, it would occasionally not take inputs well, even when they were exactly the same
          Due to this strange bug I removed checking for a newline but I'm not sure why as I cant see
          a single reason for it to do it
      */

    } else if (serialChar == 'Q') {
      housePrint(house);

    }
  }
}

void housePrint(Houses house) { //This has 4 nests. One to go through all floors, then rooms, then device, property and it prints out all devices

  int level;
  for (int i = 0; i < house.floorCount; i++) {
    for (int j = 0; j < house.floors[i].roomCount; j++) {
      for (int k = 0; k < house.floors[i].rooms[j].deviceCount; k++) {
        for (int l = 0; l < house.floors[i].rooms[j].device[k].propertyCount; l++) {
          Serial.print(house.floors[i].names);
          Serial.print("/");
          Serial.print(house.floors[i].rooms[j].names);
          Serial.print("/");
          char *lampPos = strstr(house.floors[i].rooms[j].device[k].names, "Lamp");
          if (lampPos != 0) {
            Serial.print("Lamp");
            Serial.print("/");
            int z = strlen(house.floors[i].rooms[j].device[k].names);
            for (int m = 5 ; m < z; m++) {
              Serial.print(house.floors[i].rooms[j].device[k].names[m]);
            }
          } else {
            Serial.print(house.floors[i].rooms[j].device[k].names);
            Serial.print("/");
            Serial.print("Main");
          }
          Serial.print("/");
          Serial.print(house.floors[i].rooms[j].device[k].property[l].names);
          level = house.floors[i].rooms[j].device[k].property[l].value;
          Serial.print(": ");
          if (strcmp(house.floors[i].rooms[j].device[k].property[l].names, "Level") == 0) {
            Serial.println(level);
          } else { //Prints in time format
            if (((level / 60) % 24) < 10) {
              Serial.print("0");
            }
            Serial.print((level / 60) % 24);
            Serial.print(":");
            if ((level % 60) < 10) {
              Serial.print("0");
            }
            Serial.println(level % 60);
          }
        }
      }
    }
  }
}

void lcdPrint(char *printer) {  //A simple function to clear and print the input to the LCD
  lcd.clear();
  lcd.print("Menu");
  lcd.setCursor(0, 1);
  lcd.print(printer);
}

int adjustLevel(char *type, int level) { //This adjusts the level of either a specified light or Heat. It is not allowed to leave the range of 0-100
  int maxVal; //Max value of property
  uint8_t del;  //Time delay inbetween value changes
  uint8_t oldButtonState = 0;
  uint8_t newButtonState = 0; //lcd.readButtons()
  boolean hourMode = false;
  lcd.clear();
  lcd.print("Menu");
  lcd.setCursor(0, 1);
  if (strcmp(type, "Level") == 0) {
    lcd.print(level);
    maxVal = 100;  //max level value is 100
    del = 40;
  } else { //Prints in time format
    if (((level / 60) % 24) < 10) {
      lcd.print("0");
    }
    lcd.print((level / 60) % 24);
    lcd.print(":");
    if ((level % 60) < 10) {
      lcd.print("0");
    }
    lcd.print(level % 60);
    maxVal = 1439;
    del = 60;
  }
  static unsigned long beginTime = 0;
  static unsigned long newTime = 0;

  while (true) {
    newButtonState = lcd.readButtons(); //Reads buttons
    if (newButtonState & BUTTON_SELECT & !(strcmp(type, "Level") == 0)) { //If you press and release select you now alter hours, as opposed to minutes. This is just a simple function to make browsing faster and easier
      hourMode = !hourMode;  //Flips hour mode from ON to OFF and vice versa
      while (newButtonState & BUTTON_SELECT) {
        newButtonState = lcd.readButtons();
      }
    }
    if (newButtonState & BUTTON_UP) { //If button up
      beginTime = millis(); //Start time
      while (newButtonState & BUTTON_UP) { //Until released
        newButtonState = lcd.readButtons();
        if (millis() - beginTime > 500) {
          if (level < maxVal) {
            lcd.clear();
            lcd.print("Menu");
            lcd.setCursor(0, 1);
            if (hourMode && level < 1379) {
              level = level + 60;
            } else {
              level++;
            }
            if (strcmp(type, "Level") == 0) {
              lcd.print(level);
            } else {
              if (((level / 60) % 24) < 10) {  //If time is less than 10 hours, put a 0 infront (so it goes 09:59 instead of 9:59)
                lcd.print("0");
              }
              lcd.print((level / 60) % 24); //Time format
              lcd.print(":");
              if ((level % 60) < 10) {
                lcd.print("0");
              }
              lcd.print(level % 60);
            }
            newTime = millis(); //Initialises a time for when we get into this loop (for comparison with millis)
            if (millis() - beginTime > 3000) { //If it has been more than 3 seconds of holding, change the time between increments to del
              while (millis() - newTime < del) {
                //This loop makes us wait until we have surpassed del milliseconds as a better alternative to delay
              }
            } else {
              while (millis() - newTime < 250) {
              }
            }
          }
        }
      }
    } else if (newButtonState & BUTTON_DOWN) { //Same as before but decrementing and not below 0
      beginTime = millis();
      while (newButtonState) {
        newButtonState = lcd.readButtons();
        if (millis() - beginTime > 500) {
          if (level > 0) {
            lcd.clear();
            lcd.print("Menu");
            lcd.setCursor(0, 1);
            if (hourMode && level > 59) {
              level = level - 60;
            } else {
              level--;
            }
            if (strcmp(type, "Level") == 0) {
              lcd.print(level);
            } else {
              if (((level / 60) % 24) < 10) {
                lcd.print("0");
              }
              lcd.print((level / 60) % 24);
              lcd.print(":");
              if ((level % 60) < 10) {
                lcd.print("0");
              }
              lcd.print(level % 60);
            }
            newTime = millis(); //Initialises a time for when we get into this loop (for comparison with millis)
            if (millis() - beginTime > 3000) {
              while (millis() - newTime < del) {
              }
            } else {
              while (millis() - newTime < 250) {
              }
            }
          }
        }
      }
    } else if (newButtonState & BUTTON_LEFT) {
      while (newButtonState & BUTTON_LEFT) {
        newButtonState = lcd.readButtons();
      }
      break;
    }
  }
  return (level); //Returns the new level when we back out to save it
}
