// name:     Networked_HVAC_Controller.ino
// author:   Chad Hobbs
// created:  130121
// last edit: 130121
//
// description: This program controls multiple temperature sensors via the 1-wire protocol, and turns off and on an HVAC system based on
// presets. It communicates with a server to provide data and remote access/control.
// Surface mount 1 address: 103EB683000800F1
// Short lead address: 10 18 40 44 0 8 0 70
// Long lead address: 10 CE A6 82 2 8 0 3F



// -------------LIBRARIES---------------- THESE ARE REQUIRED TO BE IN YOUR SKETCHBOOK\LIBRARY FOLDER FOR COMPILING ----
#include <OneWire.h>              // Protocol to communicate with onewire bus devices
#include <DallasTemperature.h>    // Temperature sensor protocol
#include <Wire.h>                 // Protocol to communicate with I2C devices
#include <Adafruit_MCP23017.h>    // LCD protocol
#include <Adafruit_RGBLCDShield.h>// LCD + Button shield protocol
#include <SPI.h>                  // Needed for the Ethernet shield
#include <Ethernet.h>             // Enables internet connectivity, needs pins 10 11 12 13
#include <EthernetUdp.h>          // Needed for the Ethernet shield








// ----------------------------------------------------------------------------------------
// Variables and Parameters
// ----------------------------------------------------------------------------------------
// --------------- DS1820 Variables ---------------
byte addrs[1][8] = {{16,206,166,130,2,8,0,63}};  // THIS IS PARTICULAR TO THE SPECIFIC DS1820 USED
                                  // {16,24,64,68,0,8,0,112} Temp Sensor 1
                                  // {16,206,166,130,2,8,0,63} Temp Sensor 2 unused at this time
int busPin = 4;                   // Data bus for One Wire Comms
int numOfDevices = 1;             // How many sensors are in loop
long currentTemp = 0.0;           // Current room temperature


// --------------- Relay Shield Variables ---------------
int heatPin = 7;                  // Digital pin used for turning on the heater relay
int coolPin = 8;                  // Digital pin used for turning on the AC relay



// --------------- Ethernet Shield Variables ---------------
// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // TODO _________________NEED TO UPDATE WITH ACTUAL______________
unsigned int localPort = 8888;              // local port to listen for UDP packets
IPAddress timeServer(132, 163, 4, 101);     // time-a.timefreq.bldrdoc.gov NTP server
// IPAddress timeServer(132, 163, 4, 102);  // time-b.timefreq.bldrdoc.gov NTP server
// IPAddress timeServer(132, 163, 4, 103);  // time-c.timefreq.bldrdoc.gov NTP server
const int NTP_PACKET_SIZE= 48;              // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE];        //buffer to hold incoming and outgoing packets
EthernetUDP Udp;                            // A UDP instance to let us send and receive packets over UDP
unsigned long hoursSeconds = 0;             // Return variable for the get_Time function
unsigned long timeDelay = 0;                // Counter to prevent time from getting retrieved too often


// --------------- LCD Shield Variables ---------------
unsigned long timeOut;            // Backlight timeout variable
char* menu[] = {"Cooling", "Heating"};  // Menu display for either setting the high point or the low point of the temp range
int menuPosition = 1;             // Current position in the setting menu
boolean editable = FALSE;         // Determines whether or not button presses will do anything, used to avoid accidental changes



// --------------- General Variables ---------------
int heat = 70;                    // This is the default heater trigger temp setting
int cool = 80;                    // This is the default cooling trigger temp setting
int buffer = 2;                   // This is the range away from the setpoint the heat or AC will overcool/heat to prevent shorter cycles
int cycleTime = 600000;           // The length of time to delay running the AC or heat to prevent short boolean
boolean heatRunning = FALSE;      // Stores whether the heater is currently running or not
boolean coolRunning = FALSE;      // Stores whether the AC is currently running or not
int serialSpeed = 9600;           // Default serial comm speed
long oldTemp = 0;                 // Used to only update the currentTemp if the temp has changed
unsigned long heatLastRan;        // Stores the time the heater last ran
unsigned long coolLastRan;        // Stores the time the AC last ran











// -------------Library Interaction--------------

#define ONE_WIRE_BUS busPin       // Data wire is plugged into busPin on the Arduino

// Setup oneWire to communicate with OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
//void wdt_reset(void);

// arrays to hold device address
DeviceAddress insideThermometer = { 0x10, 0x39, 0xe2, 0x82, 0x02, 0x08, 0x00, 0x3e }; // THIS IS PARTICULAR TO THE SPECIFIC DS1820 USED

// Create our LCD Shield instance
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
#define ON 0x7            // For single color LCD, set on to white
#define OFF 0x0           // For single color LCD, set off to black


















// ----------------------------------------------------------------------------------------
// Function Name: setup()
// Parameters:    None
// Returns:       None
// Description:   This function executes housekeeping duties and staging for the loop() function; executed once
// ----------------------------------------------------------------------------------------
void setup(){

  setup_network();           // Do all the things necessary to get the network protocol going

  // --------------- Relay Setup ---------------
  pinMode(heatPin,OUTPUT);   // Heat circuit relay 
  pinMode(coolPin,OUTPUT);   // Cooling circuit relay


  // --------------- Temperature Setup ---------------
  pinMode(busPin,INPUT);     // Designate temperature bus pin data direction
  sensors.begin();            // Start up the library. IC Default is 9 bit. If you have troubles consider upping it 12. Ups the delay

  // --------------- LCD Setup ---------------
  lcd.begin(16, 2);           // set up the LCD's number of columns and rows
  lcd.setBacklight(ON);       // Start off with backlight on until time-out
  timeOut = millis();         // Set the initial backlight time

}







// ----------------------------------------------------------------------------------------
// Function Name: loop()
// Parameters:    None
// Returns:       None
// Description:   This is the main executing block of the program, calling all ancillary functions to operate the Arduino
// ----------------------------------------------------------------------------------------
void loop(){

  if ((millis() - timeOut) > 30000) {   // Turn on backlight for 30 seconds, else turn it off
    lcd.setBacklight(OFF);
    editable == FALSE;
  }else {
    lcd.setBacklight(ON);
  }

  
  
  uint8_t buttons = lcd.readButtons();  // Constantly check to see if something has been put on the bus
  
  // --------------Handle updating Temp and Time change display---------------------
  if ((millis() - timeDelay) > 10000) {    // Update every 10 seconds
    lcd.setCursor(0, 0);                  // Starting postion of character printing
    lcd.clear();
    lcd.print("Current Temp: ");
    get_temp();
    lcd.print(currentTemp);
    
    hoursSeconds = get_time();
    lcd.setCursor(5, 1);
    // print the hour and minute:
    lcd.print((hoursSeconds  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    lcd.print(':');  
    if ( ((hoursSeconds % 3600) / 60) < 10 ) {
        // In the first 10 minutes of each hour, we'll want a leading '0'
        lcd.print('0');
    }
    lcd.print((hoursSeconds  % 3600) / 60); // print the minute (3600 equals secs per minute)
    
    timeDelay = millis();
  }
  

  
  // --------------Handle Button Presses---------------------
  if (buttons) {
    timeOut = millis();
    lcd.setBacklight(ON);
    

    if (buttons & BUTTON_SELECT) {      // Allow parameters to be changed only if the Select button has been pressed first
      editable = TRUE;
    }
    if (editable) {                     // go to button_handler to handle menu switching and parameter manipulations
      button_handler(buttons);
    }
  }
  
  // --------------Handle Heat and Cooling Cycles---------------------
  power_control();
  
}

// ----------------------------------------------------------------------------------------
// Function Name: power_controlHandler()
// Parameters:    None
// Returns:       None
// Description:   This function will handle turning AC or Heat off and on based on current temp, timers, and setpoints
// ----------------------------------------------------------------------------------------
void power_control(){
  
  // TODO: __________________________Add in fan relay control____________________


  // This executes if the heater is running and it gets warm enough to turn off.
  if (heatRunning && (currentTemp > (heat + buffer))) {
      heatRunning = FALSE;
      heatLastRan = millis();
      digitalWrite(heatPin,HIGH);
      return;
    }

  // This executes if the AC is running and it gets cool enough to turn off
  if (coolRunning && (currentTemp < (cool - buffer))) {
      coolRunning = FALSE;
      coolLastRan = millis();
      digitalWrite(coolPin,HIGH);
      return;
    }

  // This executes if it gets cold enough and the heater has not run for a minimum time (short cycle protection)
  if ((currentTemp < heat) && ((millis() - heatLastRan) > cycleTime)) {
    digitalWrite(heatPin,HIGH);
    heatRunning = TRUE;
    return;
  }

  // This executes if it gets hot enough and the AC has not run for 10 minutes (short cycle protection)
  if ((currentTemp > cool) && ((millis() - coolLastRan) > 600000)) {
    digitalWrite(coolPin,HIGH);
    coolRunning = TRUE;
    return;
  }

  return;
}


// ----------------------------------------------------------------------------------------
// Function Name: button_handler()
// Parameters:    None
// Returns:       Boolean, True if the temperature has changed, else False
// Description:   This function polls the temp sensors, retrieves the values, and then returns
// ----------------------------------------------------------------------------------------
boolean button_handler(uint8_t buttons){

  // --------------PARAMETERS---------------------
  int heatModifier = 0;
  int coolModifier = 0;
    

  // --------------SETUP---------------------
  lcd.clear();

  // If the UP Button is pushed, increasing the setpoint
  if (buttons & BUTTON_UP) {
    if (menuPosition == 0) {
      coolModifier = 1;
    }
    if (menuPosition == 1) {
      heatModifier = 1;
    }
  }
    
  // If the Down Button is pushed, decreasing the setpoint
  if (buttons & BUTTON_DOWN) {
    if (menuPosition == 0) {
      coolModifier = -1;
    }
    if (menuPosition == 1) {
      heatModifier = -1;
    }
  }

  // If the Left Button is pushed, change the menu context
  if (buttons & BUTTON_LEFT) {
    if (menuPosition == 1){
      menuPosition = 0;
    }else {
      menuPosition = 1;
    }
  }

  // If the Right Button is pushed, change the menu context
  if (buttons & BUTTON_RIGHT) {
    if (menuPosition == 1){
      menuPosition = 0;
    }else {
      menuPosition = 1;
    }
  }
  
  // If anything has been modified, update accordingly
  heat = heat + heatModifier;
  cool = cool + coolModifier;
  
  // Print proper menu title and setpoint
  lcd.setCursor(0, 0);
  lcd.print(menu[menuPosition]);
  lcd.setCursor(0, 1);
  lcd.print("Setpoint: ");

  if (menuPosition == 0) {
    lcd.print(cool);
  }
  if (menuPosition == 1) {
    lcd.print(heat);
  }

}

// ----------------------------------------------------------------------------------------
// Function Name: get_temp()
// Parameters:    None
// Returns:       Boolean, True if the temperature has changed, else False
// Description:   This function polls the temp sensors, retrieves the values, and then returns
// ----------------------------------------------------------------------------------------
boolean get_temp(){
  currentTemp = 0;
  long temp = 0;
  sensors.requestTemperatures(); // Send the command to the device to get temperatures
  
  // Go through devices and read the temperatures
  for(int x = 0; x < numOfDevices; x++){
    temp = sensors.getTempF(addrs[x]);
    if(temp != currentTemp){
      currentTemp = temp;
    }      
  }

  // Check to see if temperature has changed
  if (currentTemp != oldTemp) {
    oldTemp = currentTemp;
    return true;
  }
  else {
    return false;
  }
}


// ----------------------------------------------------------------------------------------
// Function Name: setup_network()
// Parameters:    None
// Returns:       None
// Description:   This function does all the prep work to create a network connection
// ----------------------------------------------------------------------------------------
void setup_network() {
  
  // TODO: _________________________REMOVE ALL Serial stuff when it works________________________

  // Open serial communications and wait for port to open:
  Serial.begin(serialSpeed);

  // start Ethernet and UDP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    while(1){}
  }
  Udp.begin(localPort);
}
  
// ----------------------------------------------------------------------------------------
// Function Name: get_time()
// Parameters:    None
// Returns:       An unsigned long representing time in hours and minutes 
// Description:   TODO ____________________________________________________
// ----------------------------------------------------------------------------------------
char* get_time()
{
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  unsigned long epoch;

    // wait to see if a reply is available
  delay(1000);  
  if ( Udp.parsePacket() ) {  
    // We've received a packet, read the data from it
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);  
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;               

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;    
    // subtract seventy years:
    epoch = secsSince1900 - seventyYears - (5 * 3600);

  }
  return timeString;
}

// ----------------------------------------------------------------------------------------
// Function Name: sendNTPpacket(IPAddress& address)
// Parameters:    IP Address
// Returns:       Unsigned Long
// Description:   send an NTP request to the time server at the given address
// ----------------------------------------------------------------------------------------
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:         
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
}
