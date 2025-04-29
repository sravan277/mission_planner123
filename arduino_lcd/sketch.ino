#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <EEPROM.h>

// Initialize LCD and RTC
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Set the LCD address to 0x27 for a 16 chars and 2 line display
RTC_DS1307 rtc;  // DS1307 RTC module

// Button Pins - Remote style layout
#define UP_BTN 2      // Up navigation
#define RIGHT_BTN 3    // Right navigation
#define DOWN_BTN 4     // Down navigation
#define LEFT_BTN 5     // Left navigation
#define SELECT_BTN 6   // Center select button

// Output Pins
#define RELAY_PIN 8
#define LED_PIN 9

// Define Modes
enum Mode { AUTO_MODE, ON_MODE, OFF_MODE };
Mode currentMode = AUTO_MODE;

// Menu System
enum MenuState { 
  MAIN_SCREEN, 
  MAIN_MENU, 
  PROGRAM_SELECT, 
  EDIT_DAYS, 
  EDIT_START_TIME, 
  EDIT_END_TIME,
  CONFIRM_SAVE,
  CONFIRM_RESET,
  SET_TIME_MENU,
  SET_HOUR,
  SET_MINUTE,
  SET_DAY,
  SET_MONTH,
  SET_YEAR,
  CONFIRM_TIME_SAVE
};
MenuState currentMenu = MAIN_SCREEN;

// Menu Items
const int MENU_ITEMS = 5; // Updated count to include new time setting option
const char* menuOptions[MENU_ITEMS] = {
  "1. Edit Program",
  "2. Change Mode",
  "3. View Programs",
  "4. Set Time/Date", // New option
  "5. Reset All"
};
int selectedMenuItem = 0;
int selectedProgram = 0;
int viewingProgram = 0;

// Time setting variables
DateTime newDateTime;
int timeSetStep = 0; // Track which part of date/time we're editing

// Program Structure
struct Program {
  bool days[7];       // Sun, Mon, Tue, Wed, Thu, Fri, Sat
  byte startHour;
  byte startMinute;
  byte endHour;
  byte endMinute;
  bool isActive;
};

// Max programs
const int MAX_PROGRAMS = 5;
Program programs[MAX_PROGRAMS];

// Time editing
int editingValue = 0; // 0-hour, 1-minute for time editing
int tempDaySelection = 0; // Day selection in editing mode

// EEPROM Addresses
const int EEPROM_START = 0;
const int EEPROM_VALID_MARKER = EEPROM_START + (MAX_PROGRAMS * 6); // Fixing size calculation (6 bytes per program)

// Button debouncing
unsigned long buttonLastPressed[5] = {0};
const unsigned long DEBOUNCE_DELAY = 250; // ms

void setup() {
  Serial.begin(9600);
  
  // Initialize LCD - with error handling
  Wire.begin();
  lcd.init();  // Use this for newer library versions
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Timer Starting..");
  
  // Initialize RTC with better error handling
  if (!rtc.begin()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC ERROR!");
    lcd.setCursor(0, 1);
    lcd.print("Check wiring!");
    while (1) {
      delay(100); // Keep the error message visible
    }
  }
  
  // Check if RTC is running
  if (!rtc.isrunning()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC not running!");
    lcd.setCursor(0, 1);
    lcd.print("Setting time...");
    
    // Set RTC to compilation time
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    delay(2000);
  }
  
  // Setup pins with explicit mode
  pinMode(UP_BTN, INPUT_PULLUP);
  pinMode(RIGHT_BTN, INPUT_PULLUP);
  pinMode(DOWN_BTN, INPUT_PULLUP);
  pinMode(LEFT_BTN, INPUT_PULLUP);
  pinMode(SELECT_BTN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Initial states
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  loadPrograms(); // Load programs from EEPROM
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready!");
  delay(1000);
  updateMainDisplay();
}

void loop() {
  // Handle button presses based on current menu state
  if (checkButton(UP_BTN)) handleUpButton();
  if (checkButton(RIGHT_BTN)) handleRightButton();
  if (checkButton(DOWN_BTN)) handleDownButton();
  if (checkButton(LEFT_BTN)) handleLeftButton();
  if (checkButton(SELECT_BTN)) handleSelectButton();
  
  // Update relay status in auto mode
  updateRelay();
  
  // Update main display periodically
  static unsigned long lastDisplayUpdate = 0;
  if (currentMenu == MAIN_SCREEN && millis() - lastDisplayUpdate >= 1000) {
    updateMainDisplay();
    lastDisplayUpdate = millis();
  }
  
  delay(50); // Small delay for stability
}

bool checkButton(int pin) {
  // Calculate array index (convert pin number to 0-based index)
  int index = pin - 2;
  if (index < 0 || index >= 5) return false; // Safety check
  
  // Read the button state (active LOW with pull-up)
  bool pressed = (digitalRead(pin) == LOW);
  unsigned long currentMillis = millis();
  
  // If button is pressed and enough time has passed since last press
  if (pressed && (currentMillis - buttonLastPressed[index] > DEBOUNCE_DELAY)) {
    // Update the last press time
    buttonLastPressed[index] = currentMillis;
    
    // Wait a bit for debouncing
    delay(10);
    
    // Double-check if button is still pressed
    return (digitalRead(pin) == LOW);
  }
  
  return false;
}

// Button handler functions based on menu state
void handleUpButton() {
  switch (currentMenu) {
    case MAIN_SCREEN:
      // Enter menu system
      currentMenu = MAIN_MENU;
      selectedMenuItem = 0;
      displayMenu();
      break;
      
    case MAIN_MENU:
      // Navigate menu up
      selectedMenuItem = (selectedMenuItem > 0) ? selectedMenuItem - 1 : MENU_ITEMS - 1;
      displayMenu();
      break;
      
    case PROGRAM_SELECT:
      // Select previous program
      selectedProgram = (selectedProgram > 0) ? selectedProgram - 1 : MAX_PROGRAMS - 1;
      displayProgramSelect();
      break;
      
    case EDIT_DAYS:
      // Move day selection up (previous day)
      tempDaySelection = (tempDaySelection > 0) ? tempDaySelection - 1 : 6;
      displayDayEditor();
      break;
      
    case EDIT_START_TIME:
    case EDIT_END_TIME:
      // Increase hour or minute
      if (editingValue == 0) {
        // Editing hour - ensure range 0-23
        if (currentMenu == EDIT_START_TIME) {
          programs[selectedProgram].startHour = (programs[selectedProgram].startHour + 1) % 24;
        } else {
          programs[selectedProgram].endHour = (programs[selectedProgram].endHour + 1) % 24;
        }
      } else {
        // Editing minute - increment by 5, ensure range 0-55
        if (currentMenu == EDIT_START_TIME) {
          programs[selectedProgram].startMinute = (programs[selectedProgram].startMinute + 5) % 60;
        } else {
          programs[selectedProgram].endMinute = (programs[selectedProgram].endMinute + 5) % 60;
        }
      }
      displayTimeEditor();
      break;
      
    // Time setting states
    case SET_HOUR:
      newDateTime = DateTime(newDateTime.year(), newDateTime.month(), newDateTime.day(), 
                           (newDateTime.hour() + 1) % 24, newDateTime.minute(), 0);
      displayTimeSetEditor();
      break;
      
    case SET_MINUTE:
      newDateTime = DateTime(newDateTime.year(), newDateTime.month(), newDateTime.day(), 
                           newDateTime.hour(), (newDateTime.minute() + 1) % 60, 0);
      displayTimeSetEditor();
      break;
      
    case SET_DAY:
      // Calculate next valid day (1-31, respecting months)
      int maxDays = daysInMonth(newDateTime.month(), newDateTime.year());
      int nextDay = newDateTime.day() + 1;
      if (nextDay > maxDays) nextDay = 1;
      newDateTime = DateTime(newDateTime.year(), newDateTime.month(), nextDay, 
                           newDateTime.hour(), newDateTime.minute(), 0);
      displayTimeSetEditor();
      break;
      
    case SET_MONTH:
      // Increment month (1-12)
      int nextMonth = newDateTime.month() + 1;
      if (nextMonth > 12) nextMonth = 1;
      
      // Adjust day if needed for new month
      int maxDaysInNewMonth = daysInMonth(nextMonth, newDateTime.year());
      int adjustedDay = min((int)newDateTime.day(), maxDaysInNewMonth);
      
      newDateTime = DateTime(newDateTime.year(), nextMonth, adjustedDay, 
                           newDateTime.hour(), newDateTime.minute(), 0);
      displayTimeSetEditor();
      break;
      
    case SET_YEAR:
      // Increment year (keeps between 2023-2099)
      int nextYear = newDateTime.year() + 1;
      if (nextYear > 2099) nextYear = 2023;
      newDateTime = DateTime(nextYear, newDateTime.month(), newDateTime.day(), 
                           newDateTime.hour(), newDateTime.minute(), 0);
      displayTimeSetEditor();
      break;
  }
}

void handleDownButton() {
  switch (currentMenu) {
    case MAIN_SCREEN:
      // Cycle through operation modes
      currentMode = static_cast<Mode>((currentMode + 1) % 3);
      updateMainDisplay();
      break;
      
    case MAIN_MENU:
      // Navigate menu down
      selectedMenuItem = (selectedMenuItem < MENU_ITEMS - 1) ? selectedMenuItem + 1 : 0;
      displayMenu();
      break;
      
    case PROGRAM_SELECT:
      // Select next program
      selectedProgram = (selectedProgram < MAX_PROGRAMS - 1) ? selectedProgram + 1 : 0;
      displayProgramSelect();
      break;
      
    case EDIT_DAYS:
      // Move day selection down (next day)
      tempDaySelection = (tempDaySelection < 6) ? tempDaySelection + 1 : 0;
      displayDayEditor();
      break;
      
    case EDIT_START_TIME:
    case EDIT_END_TIME:
      // Decrease hour or minute
      if (editingValue == 0) {
        // Editing hour
        if (currentMenu == EDIT_START_TIME) {
          programs[selectedProgram].startHour = (programs[selectedProgram].startHour > 0) ? 
            programs[selectedProgram].startHour - 1 : 23;
        } else {
          programs[selectedProgram].endHour = (programs[selectedProgram].endHour > 0) ? 
            programs[selectedProgram].endHour - 1 : 23;
        }
      } else {
        // Editing minute - decrease by 5
        if (currentMenu == EDIT_START_TIME) {
          programs[selectedProgram].startMinute = (programs[selectedProgram].startMinute >= 5) ? 
            programs[selectedProgram].startMinute - 5 : 55;
        } else {
          programs[selectedProgram].endMinute = (programs[selectedProgram].endMinute >= 5) ? 
            programs[selectedProgram].endMinute - 5 : 55;
        }
      }
      displayTimeEditor();
      break;
      
    // Time setting states
    case SET_HOUR:
      newDateTime = DateTime(newDateTime.year(), newDateTime.month(), newDateTime.day(), 
                           (newDateTime.hour() > 0) ? newDateTime.hour() - 1 : 23, 
                           newDateTime.minute(), 0);
      displayTimeSetEditor();
      break;
      
    case SET_MINUTE:
      newDateTime = DateTime(newDateTime.year(), newDateTime.month(), newDateTime.day(), 
                           newDateTime.hour(), 
                           (newDateTime.minute() > 0) ? newDateTime.minute() - 1 : 59, 0);
      displayTimeSetEditor();
      break;
      
    case SET_DAY:
      // Calculate previous valid day
      int prevDay = newDateTime.day() - 1;
      if (prevDay < 1) prevDay = daysInMonth(newDateTime.month(), newDateTime.year());
      newDateTime = DateTime(newDateTime.year(), newDateTime.month(), prevDay, 
                           newDateTime.hour(), newDateTime.minute(), 0);
      displayTimeSetEditor();
      break;
      
    case SET_MONTH:
      // Decrement month
      int prevMonth = newDateTime.month() - 1;
      if (prevMonth < 1) prevMonth = 12;
      
      // Adjust day if needed for new month
      int maxDaysInPrevMonth = daysInMonth(prevMonth, newDateTime.year());
      int adjustedDay = min((int)newDateTime.day(), maxDaysInPrevMonth);
      
      newDateTime = DateTime(newDateTime.year(), prevMonth, adjustedDay, 
                           newDateTime.hour(), newDateTime.minute(), 0);
      displayTimeSetEditor();
      break;
      
    case SET_YEAR:
      // Decrement year
      int prevYear = newDateTime.year() - 1;
      if (prevYear < 2023) prevYear = 2099;
      newDateTime = DateTime(prevYear, newDateTime.month(), newDateTime.day(), 
                           newDateTime.hour(), newDateTime.minute(), 0);
      displayTimeSetEditor();
      break;
  }
}

void handleLeftButton() {
  switch (currentMenu) {
    case MAIN_SCREEN:
      // View previous program
      viewingProgram = (viewingProgram > 0) ? viewingProgram - 1 : MAX_PROGRAMS - 1;
      updateMainDisplay();
      break;
      
    case MAIN_MENU:
    case PROGRAM_SELECT:
      // Go back to previous screen
      if (currentMenu == MAIN_MENU) {
        currentMenu = MAIN_SCREEN;
        updateMainDisplay();
      } else {
        currentMenu = MAIN_MENU;
        displayMenu();
      }
      break;
      
    case EDIT_DAYS:
      // Go back to program selection
      currentMenu = PROGRAM_SELECT;
      displayProgramSelect();
      break;
      
    case EDIT_START_TIME:
    case EDIT_END_TIME:
      // Toggle between hour and minute
      editingValue = (editingValue == 0) ? 1 : 0;
      displayTimeEditor();
      break;
      
    case CONFIRM_SAVE:
    case CONFIRM_RESET:
    case CONFIRM_TIME_SAVE:
      // Cancel confirmation
      if (currentMenu == CONFIRM_SAVE) {
        currentMenu = EDIT_END_TIME;
        displayTimeEditor();
      } else if (currentMenu == CONFIRM_RESET) {
        currentMenu = MAIN_MENU;
        displayMenu();
      } else if (currentMenu == CONFIRM_TIME_SAVE) {
        // Cancel time setting
        currentMenu = SET_TIME_MENU;
        displayTimeSetMenu();
      }
      break;
      
    // Time setting navigation - go back to previous field
    case SET_MINUTE:
      currentMenu = SET_HOUR;
      displayTimeSetEditor();
      break;
      
    case SET_DAY:
      currentMenu = SET_MINUTE;
      displayTimeSetEditor();
      break;
      
    case SET_MONTH:
      currentMenu = SET_DAY;
      displayTimeSetEditor();
      break;
      
    case SET_YEAR:
      currentMenu = SET_MONTH;
      displayTimeSetEditor();
      break;
      
    case SET_TIME_MENU:
      // Exit time setting menu
      currentMenu = MAIN_MENU;
      displayMenu();
      break;
  }
}

void handleRightButton() {
  switch (currentMenu) {
    case MAIN_SCREEN:
      // View next program
      viewingProgram = (viewingProgram < MAX_PROGRAMS - 1) ? viewingProgram + 1 : 0;
      updateMainDisplay();
      break;
      
    case EDIT_DAYS:
      // Toggle current day selection
      programs[selectedProgram].days[tempDaySelection] = !programs[selectedProgram].days[tempDaySelection];
      displayDayEditor();
      break;
      
    case EDIT_START_TIME:
      // Move to end time editing
      currentMenu = EDIT_END_TIME;
      editingValue = 0; // Start with hour
      displayTimeEditor();
      break;
      
    case EDIT_END_TIME:
      // Confirm save
      currentMenu = CONFIRM_SAVE;
      displayConfirmation("Save program?", "Yes=SEL No=LEFT");
      break;
      
    // Time setting navigation - move to next field
    case SET_HOUR:
      currentMenu = SET_MINUTE;
      displayTimeSetEditor();
      break;
      
    case SET_MINUTE:
      currentMenu = SET_DAY;
      displayTimeSetEditor();
      break;
      
    case SET_DAY:
      currentMenu = SET_MONTH;
      displayTimeSetEditor();
      break;
      
    case SET_MONTH:
      currentMenu = SET_YEAR;
      displayTimeSetEditor();
      break;
      
    case SET_YEAR:
      // Finished setting - confirm
      currentMenu = CONFIRM_TIME_SAVE;
      displayConfirmation("Save new time?", "Yes=SEL No=LEFT");
      break;
  }
}

void handleSelectButton() {
  switch (currentMenu) {
    case MAIN_SCREEN:
      // Quick access to program viewing
      currentMenu = MAIN_MENU;
      selectedMenuItem = 2; // View Programs option
      displayMenu();
      break;
      
    case MAIN_MENU:
      // Select menu item
      switch (selectedMenuItem) {
        case 0: // Edit Program
          currentMenu = PROGRAM_SELECT;
          selectedProgram = 0;
          displayProgramSelect();
          break;
          
        case 1: // Change Mode
          currentMode = static_cast<Mode>((currentMode + 1) % 3);
          updateMainDisplay();
          currentMenu = MAIN_SCREEN;
          break;
          
        case 2: // View Programs
          currentMenu = PROGRAM_SELECT;
          selectedProgram = 0;
          displayProgramSelect();
          break;
          
        case 3: // Set Time/Date (NEW OPTION)
          currentMenu = SET_TIME_MENU;
          // Initialize with current time
          newDateTime = rtc.now();
          displayTimeSetMenu();
          break;
          
        case 4: // Reset All
          currentMenu = CONFIRM_RESET;
          displayConfirmation("Reset all?", "Yes=SEL No=LEFT");
          break;
      }
      break;
      
    case PROGRAM_SELECT:
      // Select program to edit
      if (selectedMenuItem == 0) { // Edit mode
        currentMenu = EDIT_DAYS;
        tempDaySelection = 0;
        displayDayEditor();
      } else { // View mode
        // Just display program details
        displayProgramDetails(selectedProgram);
        delay(3000);
        currentMenu = PROGRAM_SELECT;
        displayProgramSelect();
      }
      break;
      
    case EDIT_DAYS:
      // Move to start time editing
      currentMenu = EDIT_START_TIME;
      editingValue = 0; // Start with hour
      displayTimeEditor();
      break;
      
    case CONFIRM_SAVE:
      // Save program
      programs[selectedProgram].isActive = true;
      savePrograms();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Program saved!");
      delay(1500);
      currentMenu = MAIN_SCREEN;
      updateMainDisplay();
      break;
      
    case CONFIRM_RESET:
      // Reset all programs
      resetPrograms();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("All programs");
      lcd.setCursor(0, 1);
      lcd.print("reset!");
      delay(1500);
      currentMenu = MAIN_SCREEN;
      updateMainDisplay();
      break;
      
    // Time setting options
    case SET_TIME_MENU:
      // Start with hour setting
      currentMenu = SET_HOUR;
      displayTimeSetEditor();
      break;
      
    case CONFIRM_TIME_SAVE:
      // Save new time to RTC
      rtc.adjust(newDateTime);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Time saved!");
      delay(1500);
      currentMenu = MAIN_SCREEN;
      updateMainDisplay();
      break;
  }
}

void displayMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MENU:");
  
  // Show selected item on second line
  lcd.setCursor(0, 1);
  lcd.print(menuOptions[selectedMenuItem]);
  
  // Indicate navigation
  lcd.setCursor(15, 1);
  lcd.write(0x7E); // Right arrow character
}

void displayProgramSelect() {
  lcd.clear();
  lcd.setCursor(0, 0);
  
  if (selectedMenuItem == 0) {
    lcd.print("Edit Program:");
  } else {
    lcd.print("View Program:");
  }
  
  lcd.setCursor(0, 1);
  lcd.print("P");
  lcd.print(selectedProgram + 1);
  
  if (programs[selectedProgram].isActive) {
    lcd.print(" (Active)");
  } else {
    lcd.print(" (Inactive)");
  }
}

void displayDayEditor() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Days: P");
  lcd.print(selectedProgram + 1);
  
  // Display days with current selection highlighted
  lcd.setCursor(0, 1);
  String dayNames[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  
  // Calculate how many days we can fit
  int displayDays = min(3, 7);
  
  for (int i = 0; i < displayDays; i++) {
    int dayIndex = (tempDaySelection + i) % 7;
    
    if (i == 0) {
      // Current day being edited
      if (programs[selectedProgram].days[dayIndex]) {
        lcd.print("[");
        lcd.print(dayNames[dayIndex]);
        lcd.print("]");
      } else {
        lcd.print("[");
        lcd.print("--");
        lcd.print("]");
      }
    } else {
      // Show other days if space allows
      if (programs[selectedProgram].days[dayIndex]) {
        lcd.print(dayNames[dayIndex]);
      } else {
        lcd.print("--");
      }
    }
    
    if (i < displayDays - 1) lcd.print(" ");
  }
  
  // Add navigation hint
  lcd.setCursor(13, 1);
  lcd.print("^v");
}

void displayTimeEditor() {
  lcd.clear();
  lcd.setCursor(0, 0);
  
  if (currentMenu == EDIT_START_TIME) {
    lcd.print("Start time: P");
    lcd.print(selectedProgram + 1);
    
    lcd.setCursor(0, 1);
    // Show hour with highlighting if selected
    if (editingValue == 0) lcd.print("[");
    if (programs[selectedProgram].startHour < 10) lcd.print("0");
    lcd.print(programs[selectedProgram].startHour);
    if (editingValue == 0) lcd.print("]");
    
    lcd.print(":");
    
    // Show minute with highlighting if selected
    if (editingValue == 1) lcd.print("[");
    if (programs[selectedProgram].startMinute < 10) lcd.print("0");
    lcd.print(programs[selectedProgram].startMinute);
    if (editingValue == 1) lcd.print("]");
  } else {
    lcd.print("End time: P");
    lcd.print(selectedProgram + 1);
    
    lcd.setCursor(0, 1);
    // Show hour with highlighting if selected
    if (editingValue == 0) lcd.print("[");
    if (programs[selectedProgram].endHour < 10) lcd.print("0");
    lcd.print(programs[selectedProgram].endHour);
    if (editingValue == 0) lcd.print("]");
    
    lcd.print(":");
    
    // Show minute with highlighting if selected
    if (editingValue == 1) lcd.print("[");
    if (programs[selectedProgram].endMinute < 10) lcd.print("0");
    lcd.print(programs[selectedProgram].endMinute);
    if (editingValue == 1) lcd.print("]");
  }
  
  // Navigation hint
  lcd.setCursor(9, 1);
  lcd.print("^v");
}

// New functions for time setting
void displayTimeSetMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Set Time/Date");
  lcd.setCursor(0, 1);
  lcd.print("Press SELECT");
}

void displayTimeSetEditor() {
  lcd.clear();
  lcd.setCursor(0, 0);
  
  // Show different prompts based on what we're editing
  switch (currentMenu) {
    case SET_HOUR:
      lcd.print("Set Hour:");
      break;
    case SET_MINUTE:
      lcd.print("Set Minute:");
      break;
    case SET_DAY:
      lcd.print("Set Day:");
      break;
    case SET_MONTH:
      lcd.print("Set Month:");
      break;
    case SET_YEAR:
      lcd.print("Set Year:");
      break;
  }
  
  lcd.setCursor(0, 1);
  
  // Display the date/time values appropriate for this step
  switch (currentMenu) {
    case SET_HOUR:
    case SET_MINUTE:
      // Display the time with proper formatting and highlighting
      if (currentMenu == SET_HOUR) lcd.print("[");
      if (newDateTime.hour() < 10) lcd.print("0");
      lcd.print(newDateTime.hour());
      if (currentMenu == SET_HOUR) lcd.print("]");
      
      lcd.print(":");
      
      if (currentMenu == SET_MINUTE) lcd.print("[");
      if (newDateTime.minute() < 10) lcd.print("0");
      lcd.print(newDateTime.minute());
      if (currentMenu == SET_MINUTE) lcd.print("]");
      break;
      
    case SET_DAY:
    case SET_MONTH:
    case SET_YEAR:
      // Display the date with proper formatting and highlighting
      if (currentMenu == SET_DAY) lcd.print("[");
      if (newDateTime.day() < 10) lcd.print("0");
      lcd.print(newDateTime.day());
      if (currentMenu == SET_DAY) lcd.print("]");
      
      lcd.print("/");
      
      if (currentMenu == SET_MONTH) lcd.print("[");
      if (newDateTime.month() < 10) lcd.print("0");
      lcd.print(newDateTime.month());
      if (currentMenu == SET_MONTH) lcd.print("]");
      
      lcd.print("/");
      
      if (currentMenu == SET_YEAR) lcd.print("[");
      lcd.print(newDateTime.year());
      if (currentMenu == SET_YEAR) lcd.print("]");
      break;
  }
  
  // Navigation hint
  lcd.setCursor(14, 1);
  lcd.print("^v");
}

// Helper function to calculate days in month
int daysInMonth(int month, int year) {
  const uint8_t daysPerMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  
  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  
  return daysPerMonth[month];
}

// Helper function to check for leap years
bool isLeapYear(int year) {
  return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

void displayConfirmation(const char* message, const char* options) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(message);
  lcd.setCursor(0, 1);
  lcd.print(options);
}

void displayProgramDetails(int progIndex) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("P");
  lcd.print(progIndex + 1);
  
  if (programs[progIndex].isActive) {
    // Show times
    lcd.print(" ");
    if (programs[progIndex].startHour < 10) lcd.print("0");
    lcd.print(programs[progIndex].startHour);
    lcd.print(":");
    if (programs[progIndex].startMinute < 10) lcd.print("0");
    lcd.print(programs[progIndex].startMinute);
    lcd.print("-");
    if (programs[progIndex].endHour < 10) lcd.print("0");
    lcd.print(programs[progIndex].endHour);
    lcd.print(":");
    if (programs[progIndex].endMinute < 10) lcd.print("0");
    lcd.print(programs[progIndex].endMinute);
  } else {
    lcd.print(" (Inactive)");
  }
  
  // Show days on second line
  lcd.setCursor(0, 1);
  lcd.print("Days:");
  String dayChars = "SMTWTFS";
  for (int i = 0; i < 7; i++) {
    if (programs[progIndex].days[i]) {
      lcd.print(dayChars[i]);
    } else {
      lcd.print("-");
    }
  }
}

void updateMainDisplay() {
  DateTime now = rtc.now();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  
  // Current time with seconds
  if (now.hour() < 10) lcd.print("0");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  lcd.print(":");
  if (now.second() < 10) lcd.print("0");
  lcd.print(now.second());
  
  // Display current mode
  lcd.print(" ");
  switch (currentMode) {
    case AUTO_MODE:
      lcd.print("AUTO");
      break;
    case ON_MODE:
      lcd.print("ON");
      break;
    case OFF_MODE:
      lcd.print("OFF");
      break;
  }
  
  // Second row - alternating between day/date and active program
  lcd.setCursor(0, 1);
  
  static bool showDate = true;
  showDate = !showDate;
  
  if (showDate) {
    // Show day and date
    const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    lcd.print(dayNames[now.dayOfTheWeek()]);
    lcd.print(" ");
    if (now.day() < 10) lcd.print("0");
    lcd.print(now.day());
    lcd.print("/");
    if (now.month() < 10) lcd.print("0");
    lcd.print(now.month());
    lcd.print("/");
    lcd.print(now.year());
  } else {
    // Show viewing program number
    lcd.print("P");
    lcd.print(viewingProgram + 1);
    
    // Show program status
    if (programs[viewingProgram].isActive) {
      lcd.print(" ");
      if (programs[viewingProgram].startHour < 10) lcd.print("0");
      lcd.print(programs[viewingProgram].startHour);
      lcd.print(":");
      if (programs[viewingProgram].startMinute < 10) lcd.print("0");
      lcd.print(programs[viewingProgram].startMinute);
      lcd.print("-");
      if (programs[viewingProgram].endHour < 10) lcd.print("0");
      lcd.print(programs[viewingProgram].endHour);
      lcd.print(":");
      if (programs[viewingProgram].endMinute < 10) lcd.print("0");
      lcd.print(programs[viewingProgram].endMinute);
    } else {
      lcd.print(" (Inactive)");
    }
  }
  
  // Show relay status with custom character
  lcd.setCursor(15, 0);
  if (digitalRead(RELAY_PIN) == HIGH) {
    lcd.write('*'); // Output symbol
  } else {
    lcd.write(' '); // No output
  }
}

void updateRelay() {
  // Fixed relay control based on mode
  if (currentMode == ON_MODE) {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    return;
  } else if (currentMode == OFF_MODE) {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    return;
  }
  
  // AUTO mode - check program schedules
  DateTime now = rtc.now();
  int currentHour = now.hour();
  int currentMinute = now.minute();
  int currentDay = now.dayOfTheWeek(); // 0 = Sunday, 6 = Saturday
  
  // Initialize to OFF by default
  bool shouldBeOn = false;
  
  // Check if any program should turn on the relay
  for (int i = 0; i < MAX_PROGRAMS; i++) {
    if (!programs[i].isActive) continue;
    
    // Check if today is scheduled
    if (!programs[i].days[currentDay]) continue;
    
    // Convert times to minutes for easier comparison
    int currentTimeMinutes = currentHour * 60 + currentMinute;
    int startTimeMinutes = programs[i].startHour * 60 + programs[i].startMinute;
    int endTimeMinutes = programs[i].endHour * 60 + programs[i].endMinute;
    
    // Handle overnight programs (end time less than start time)
    if (endTimeMinutes < startTimeMinutes) {
      // Program runs overnight
      if (currentTimeMinutes >= startTimeMinutes || currentTimeMinutes < endTimeMinutes) {
        shouldBeOn = true;
        break;
      }
    } else {
      // Normal program within the same day
      if (currentTimeMinutes >= startTimeMinutes && currentTimeMinutes < endTimeMinutes) {
        shouldBeOn = true;
        break;
      }
    }
  }
  
  // Set relay based on schedule check
  digitalWrite(RELAY_PIN, shouldBeOn ? HIGH : LOW);
  digitalWrite(LED_PIN, shouldBeOn ? HIGH : LOW);
}

void savePrograms() {
  int addr = EEPROM_START;
  
  // Save each program
  for (int i = 0; i < MAX_PROGRAMS; i++) {
    // Pack days into a single byte (bit 0 = Sunday, bit 6 = Saturday)
    byte daysByte = 0;
    for (int d = 0; d < 7; d++) {
      if (programs[i].days[d]) {
        daysByte |= (1 << d);
      }
    }
    
    // Write program data
    EEPROM.write(addr++, daysByte);
    EEPROM.write(addr++, programs[i].startHour);
    EEPROM.write(addr++, programs[i].startMinute);
    EEPROM.write(addr++, programs[i].endHour);
    EEPROM.write(addr++, programs[i].endMinute);
    EEPROM.write(addr++, programs[i].isActive ? 1 : 0);
  }
  
  // Write validation marker to indicate data is valid
  EEPROM.write(EEPROM_VALID_MARKER, 0xAA);
}

void loadPrograms() {
  // Check if EEPROM has valid data
  if (EEPROM.read(EEPROM_VALID_MARKER) != 0xAA) {
    // No valid data - initialize defaults
    resetPrograms();
    return;
  }
  
  int addr = EEPROM_START;
  
  // Load each program
  for (int i = 0; i < MAX_PROGRAMS; i++) {
    // Read packed days byte
    byte daysByte = EEPROM.read(addr++);
    
    // Unpack days
    for (int d = 0; d < 7; d++) {
      programs[i].days[d] = (daysByte & (1 << d)) ? true : false;
    }
    
    // Read other program data
    programs[i].startHour = EEPROM.read(addr++);
    programs[i].startMinute = EEPROM.read(addr++);
    programs[i].endHour = EEPROM.read(addr++);
    programs[i].endMinute = EEPROM.read(addr++);
    programs[i].isActive = (EEPROM.read(addr++) == 1);
    
    // Validate time values in case of corruption
    programs[i].startHour %= 24;
    programs[i].startMinute %= 60;
    programs[i].endHour %= 24;
    programs[i].endMinute %= 60;
  }
}

void resetPrograms() {
  // Set default values for all programs
  for (int i = 0; i < MAX_PROGRAMS; i++) {
    // All days off by default
    for (int d = 0; d < 7; d++) {
      programs[i].days[d] = false;
    }
    
    // Default times
    programs[i].startHour = 8;  // 8:00 AM
    programs[i].startMinute = 0;
    programs[i].endHour = 17;   // 5:00 PM
    programs[i].endMinute = 0;
    programs[i].isActive = false;
  }
  
  // Save defaults to EEPROM
  savePrograms();
}