#ifndef _BLE_NRF52_H
#define _BLE_NRF52_H

#include <Arduino.h>   //for Serial
#include <Tympan.h>
#include <SerialManager_UI.h>
#include "BLE/ble.h"


class BLE_nRF52: virtual public BLE
{
public:
	BLE_nRF52(HardwareSerial *sp) : serialToBLE(sp), serialFromBLE(sp) {};
	BLE_nRF52(TympanBase *_tympan) : serialToBLE(_tympan->BT_Serial), serialFromBLE(_tympan->BT_Serial) {};
	BLE_nRF52(void) {} //uses default serial ports set down in the protected section
	virtual int begin(void) {  return begin(false); }
	virtual int begin(int doFactoryReset) {
		while (serialFromBLE->available()) serialFromBLE->read(); //clear the incoming Serial buffer		
		return 0;
	}
	virtual void setupBLE(void) { setupBLE(10); } //default to firmware 10
	virtual void setupBLE(int BT_firmware) { setupBLE(BT_firmware,false); };            //to be called from the Arduino sketch's setup() routine.  Includes factory reset.
	virtual void setupBLE(int BT_firmware, bool printDebug) { setupBLE(BT_firmware, printDebug, false); };            //to be called from the Arduino sketch's setup() routine.  Includes factory reset.
	//virtual void setupBLE_noFactoryReset(int BT_firmware = 7, bool printDebug = true) { setupBLE(BT_firmware,printDebug, false);};  //to be called from the Arduino sketch's setup() routine.  Excludes factory reset.
	virtual void setupBLE(int BT_firmware, bool printDebug, int doFactoryReset) {
		//Serial.println("ble_nRF52: begin: PIN_IS_CONNECTED = " + String(PIN_IS_CONNECTED)); 
		if (PIN_IS_CONNECTED >= 0) pinMode(PIN_IS_CONNECTED, INPUT); 
		begin(doFactoryReset);
	};  //to be called from the Arduino sketch's setup() routine
	
	//send strings
	size_t send(const String &str);  //the main way to send a string 
	size_t send(const char c) { return send(String(c)); }
	size_t sendString(const String &s, bool print_debug);
	size_t sendString(const String &s) { return sendString(s,false);}
	size_t sendCommand(const String &cmd,const String &data);

	//send strings, but add formatting for TympanRemote App
	virtual size_t sendMessage(const String &s);
	virtual size_t sendMessage(char c) { return sendMessage(String(c)); }
	
	//receive reply from BLE unit (formatted with "OK" or "FAIL")
	virtual size_t recvReply(String *s_ptr, unsigned long timeout_mills);
	virtual size_t recvReply(String *s_ptr) { return recvReply(s_ptr, rx_timeout_millis); }

	//receive message, from Tympan Remote App (or otherwise)
	virtual int available(void) { return serialFromBLE->available(); }
	virtual int read(void) { return serialFromBLE->read(); };
	virtual int peek(void) { return serialFromBLE->peek(); }
	//virtual size_t recvMessage(String *s_ptr);

	//receive methods required by BLE interface
	virtual int recv(String *s_ptr);                      // grab serial response, by line
	virtual size_t recvBLE(String *s_ptr) { return recvBLE(s_ptr, false); };
  virtual size_t recvBLE(String *s_ptr, bool printResponse);

	int setBleName(const String &s);
	virtual int getBleName(String *reply_ptr);
	virtual int version(String *reply_ptr);
	virtual int isConnected(void) { return isConnected(GET_AUTO); }
	virtual int isConnected(int method);
	virtual int isAdvertising(void);
	int enableAdvertising(bool);
	
	// These do nothing but are needed for compatibility with ble.h
	virtual void updateAdvertising(unsigned long curTime_millis, unsigned long updatePeriod_millis = 5000) { updateAdvertising(curTime_millis, updatePeriod_millis, false); }
	virtual void updateAdvertising(unsigned long curTime_millis, unsigned long updatePeriod_millis, bool printDebugMsgs) {}; //do nothing, already auto-advertises after disconnect
	virtual bool setUseFasterBaudRateUponBegin(bool enable = true) { return enable; };
	//end do-nothing methods
	
	int setLedMode(int val);
	int getLedMode(void);

	unsigned long rx_timeout_millis = 2000UL;
	//bool simulated_Serial_to_nRF = true;

	int PIN_IS_CONNECTED = 39;  //Tympan RevF
	enum GET_TYPE { GET_AUTO=0, GET_VIA_SOFTWARE, GET_VIA_GPIO};
	
protected:
	//virtual bool useFasterBaudRateUponBegin = false; //default as to whether to use faster baud rate or not
	//virtual void setSerialBaudRate(int new_baud);
	//virtual int hardwareFactoryReset(bool printDebug = false);
	//virtual void switchToNewBaudRate(int new_baudrate);

	const String EOC = String('\r');
	#if defined(KINETISK)   //for Teensy 3.x only!  which should never happen for RevF
	  HardwareSerial *serialToBLE = &Serial1;  		//dummy value.  This branch should never happen.  This value is here just to avoid a compiler warning
	  HardwareSerial *serialFromBLE = &Serial1;   //dummy value.  This branch should never happen.  This value is here just to avoid a compiler warning
	#else
	  HardwareSerial *serialToBLE = &Serial7;    //Tympan design uses Teensy's Serial1 to connect to BLE
	  HardwareSerial *serialFromBLE = &Serial7;  //Tympan design uses Teensy's Serial1 to connect from BLE
	#endif
	static bool doesStartWithOK(const String &s);
	int isConnected_getViaSoftware(void);
	int isConnected_getViaGPIO(void);
	
	unsigned long _timeout = 2000; // timeout for recv
};

//class BLE_UI : public BLE, public SerialManager_UI
class BLE_nRF52_UI : public BLE_nRF52, virtual public BLE_UI 
{
	public:
		BLE_nRF52_UI(TympanBase *_tympan) : BLE_UI(), BLE_nRF52(_tympan) {}
		BLE_nRF52_UI(HardwareSerial *sp) : BLE_UI(), BLE_nRF52(sp) {}

		// ///////// here are the methods that you must implement from SerialManager_UI
		virtual void printHelp(void);
		//virtual bool processCharacter(char c); //not used here
		virtual bool processCharacterTriple(char mode_char, char chan_char, char data_char);
		virtual void setFullGUIState(bool activeButtonsOnly = false) {};
		// ///////// end of required methods
		virtual bool processSingleCharacter(char data_char);

};

#endif