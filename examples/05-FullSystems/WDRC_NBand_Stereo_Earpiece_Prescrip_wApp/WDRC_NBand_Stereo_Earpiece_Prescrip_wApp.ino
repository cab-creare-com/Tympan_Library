/*
  WDRC_NBand_Stereo_Earpieces_Prescrip_wApp

  Created: Chip Audette (OpenAudio), Oct 2021
    Primarly built upon CHAPRO "Generic Hearing Aid" from
    Boys Town National Research Hospital (BTNRH): https://github.com/BTNRH/chapro

  Purpose: Implements Multi-band WDRC compressor.  One for each ear.
  
  Features: 
    * A Multi-band WDRC compressor is:
        * Multi-channel IIR Filterbank (2-16 channels)
        * Each filter channel has its own WDRC compressor
        * All channels mixed back together
        * Broadband gain element
        * Broadband WDRC compressor (used as a limiter)
    * Adds a notching filter prior to the WDRC for mitigating stationary feedback (bypassed by default)
    * Stereo (each ear is indepenedent)
    * Uses Tympan digital earpieces (or any of the other built-in audio sorces))
    * Can control via TympanRemote App or via USB Serial
    * Can write raw and processed audio to SD card
    * Includes prescription saving
    * It can switch between two presets, both of which you can change
        * a NORMAL preset ("Preset_16_00.h")
        * a FULL-ON GAIN preset ("Preset_16_01.h")
    * Does not include feedback cancellation

  Hardware Controls:
    Potentiometer on Tympan controls the broadband gain.

  Changing Number of Channels:
    As written, you can use 16 channels or fewer.  Simply change MAX_N_CHAN.
    If you want more than 16 channels, that'll take a bit more effort. Ask the
    question in the Tympan forum!
    
  TYMPAN REV E ONLY!  Please be aware that this example defaults to 16 channels per ear.
  As a result, this requires the processing power of the Tympan RevE.  If you want to 
  run this example on a Tympan RevD, you will have to reduce the number of channels
  (maybe down to 6-8 channels per ear?).
  
  DEFAULT IS ANDROID ONLY!  Please be aware that, because there are so many
  parameter values being excanged with the mobile App, we have switched to a
  higher speed communication mode.  This only seems to work with Anrdoid and
  seems to fail with iOS (Apple).  If you want to run with an iOS device, you
  simply need to comment out one line of code here in this file.  Comment out: 
  ble.setUseFasterBaudRateUponBegin(true); 
  
  MIT License.  use at your own risk.
*/


// Include all the of the needed libraries
#include <Tympan_Library.h>

// Define the audio settings
const float sample_rate_Hz = 24000.0f ; //24000 or 32000 or 44100 (or other frequencies in the table in AudioOutputI2S_F32
const int audio_block_samples = 16;     //do not make bigger than AUDIO_BLOCK_SAMPLES from AudioStream.h (which is 128)
AudioSettings_F32   audio_settings(sample_rate_Hz, audio_block_samples);


// Define the number of channels! Make sure Preset_16_0.h and Preset_16_01.h have enough values
// It needs MAX_N_CHAN or more values.  If not, it'll not work well (or at all) at runtime.
#define MAX_N_CHAN 16             //should function if you choose any number 2-16...though if less than 16, you'll want to adjust the presets for better sound
#define USE_FIR_FILTERBANK false  //set to true for FIR and false for IIR (Cascaded Biquad)

//More includes
#include      "BTNRH_PresetManager_UI.h"  //must be after N_CHAN is defined
#include      "StereoContainer_Biquad_WDRC_UI.h"
#include      "SerialManager.h"     //must be after BTNRH_PresetManager_UI is defined
#include      "State.h"             //must be after N_CHAN is defined
#include      "AudioConnections.h"  //let's put them in their own file for clarity

// Create audio classes and make audio connections
Tympan           myTympan(TympanRev::F, audio_settings);   //do TympanRev::D or E or F
EarpieceShield   earpieceShield(TympanRev::F, AICShieldRev::A); //in the Tympan_Library, EarpieceShield is defined in AICShield.h

// Create classes for controlling the system
BLE_UI&       ble = myTympan.getBLE_UI();  //myTympan owns the ble object, but we have a reference to it here
SerialManager serialManager(&ble);                 //create the serial manager for real-time control (via USB or App)
State         myState(&audio_settings, &myTympan, &serialManager); //keeping one's state is useful for the App's GUI
BTNRH_StereoPresetManager_UI presetManager;  //initializes all presets to the built-in defaults...which are defined by Preset_00.h and Preset_01.h


//function to setup the hardware
void setupTympanHardware(void) {

  //activate the Tympan's audio interface
  myTympan.enable();
  earpieceShield.enable();                                      // activate the AIC on the earpiece shield

  //do some software setup that I'd prefer to do in AudioConnections.h but I can't due to scope limitations.  So, I guess here is good enough
  earpieceMixer.setTympanAndShield(&myTympan, &earpieceShield); //the earpiece mixer must interact with the hardware, so point it to the hardware
  connectClassesToOverallState();
  
  //setup DC-blocking highpass filter running in the ADC hardware itself
  float cutoff_Hz = 40.0;  //set the default cutoff frequency for the highpass filter
  myTympan.setHPFonADC(true,cutoff_Hz,audio_settings.sample_rate_Hz); //set to false to disble
  earpieceShield.setHPFonADC(true,cutoff_Hz,audio_settings.sample_rate_Hz); //set to false to disble
    

  //Choose the default input
  if (1) {
    //default to the digital PDM mics within the Tympan earpieces
    earpieceMixer.setAnalogInputSource(EarpieceMixerState::INPUT_PCBMICS);  //Choose the desired audio analog input on the Typman...this will be overridden by the serviceMicDetect() in loop(), if micDetect is enabled
    earpieceMixer.setInputAnalogVsPDM(EarpieceMixerState::INPUT_PDM);       // ****but*** then activate the PDM mics
    Serial.println("setup: PDM Earpiece is the active input.");
  } else {
    //default to an analog input
    earpieceMixer.setAnalogInputSource(EarpieceMixerState::INPUT_PCBMICS);  //Choose the desired audio analog input on the Typman
    //earpieceMixer.setAnalogInputSource(EarpieceMixerState::INPUT_MICJACK_MIC);  //Choose the desired audio analog input on the Typman
    earpieceMixer.setInputAnalogVsPDM(EarpieceMixerState::INPUT_ANALOG);       // ****but*** then activate the PDM mics
    Serial.println("setup: analog input is the active input.");
  }
  
  //Set the Bluetooth audio to go straight to the headphone amp, not through the Tympan software
  //myTympan.mixBTAudioWithOutput(true);
  
  //set volumes
  setOutputGain_dB(myState.output_gain_dB);  // -63.6 to +24 dB in 0.5dB steps.  uses signed 8-bit
  myTympan.setInputGain_dB(myState.earpieceMixer->inputGain_dB); // set MICPGA volume, 0-47.5dB in 0.5dB setps
  setDigitalGain_dB(myState.digital_gain_dB); // set gain low
}


void connectClassesToOverallState(void) {
  for (int i=0; i<2; i++) {
    myState.filterbank[i] = &multiBandWDRC[i].filterbank.state;
    myState.compbank[i]   = &multiBandWDRC[i].compbank.state;
  }
  myState.earpieceMixer = &earpieceMixer.state;
}

//set up the serial manager
void setupSerialManager(void) {
  //register all the UI elements here  
  serialManager.add_UI_element(&myState);
  serialManager.add_UI_element(&ble);
  serialManager.add_UI_element(&earpieceMixer); 
  serialManager.add_UI_element(&stereoContainerWDRC);
  serialManager.add_UI_element(&presetManager);
  serialManager.add_UI_element(&audioSDWriter);
}

// ///////////////// Main setup() and loop() as required for all Arduino programs

// define the setup() function, the function that is called once when the device is booting
int USE_VOLUME_KNOB = 0;  //set to 1 to use volume knob to override the default vol_knob_gain_dB set a few lines below
void setup() {
  myTympan.beginBothSerial();
  if (Serial) Serial.print(CrashReport);
  Serial.println("WDRC_NBand_Stereo_Earpiece_Prescrip_wApp: setup():...");
  Serial.print("Sample Rate (Hz): "); Serial.println(audio_settings.sample_rate_Hz);
  Serial.print("Audio Block Size (samples): "); Serial.println(audio_settings.audio_block_samples);

  // Audio connections require memory
  AudioMemory_F32(80,audio_settings);  //allocate Float32 audio data blocks (primary memory used for audio processing)

  // Enable the audio shield, select input, and enable output
  setupTympanHardware();  //see code earlier in this file

  //setup filters and mixers
  setupAudioProcessing(); //see function in ConfigureAlgorithms.h

  //setup BLE
  ble.setUseFasterBaudRateUponBegin(true); //speeds up baudrate to 115200.  ONLY WORKS FOR ANDROID.  If iOS, you must set to false.
	myTympan.setupBLE(); delay(500); //Assumes the default Bluetooth firmware. You can override!
  
	//setup the serial manager
  setupSerialManager();

  //prepare the SD writer for the format that we want and any error statements
  audioSDWriter.setSerial(&myTympan);
  audioSDWriter.setNumWriteChannels(4);     //can record 2 or 4 channels
  Serial.println("Setup: SD configured for writing " + String(audioSDWriter.getNumWriteChannels()) + " audio channels.");
  
  //enable the algorithms...but is this really needed?
  multiBandWDRC[0].filterbank.enable(true);
  multiBandWDRC[1].filterbank.enable(true);
  multiBandWDRC[0].enable(true);
  multiBandWDRC[1].enable(true);

  //End of setup
  Serial.println("Setup complete.");
  serialManager.printHelp();

} //end setup()


// define the loop() function, the function that is repeated over and over for the life of the device
void loop() {

  //respond to Serial commands
  while (Serial.available()) serialManager.respondToByte((char)Serial.read());   //USB...respondToByte is in SerialManagerBase...it then calls SerialManager.processCharacter(c)

  //respond to BLE
  if (ble.available() > 0) {
    String msgFromBle; int msgLen = ble.recvBLE(&msgFromBle);
    for (int i=0; i < msgLen; i++) serialManager.respondToByte(msgFromBle[i]);  //respondToByte is in SerialManagerBase...it then calls SerialManager.processCharacter(c)
  }

  //service the BLE advertising state
  ble.updateAdvertising(millis(),5000); //check every 5000 msec to ensure it is advertising (if not connected)

  //service the SD recording
  audioSDWriter.serviceSD_withWarnings(i2s_in); //needs some info from i2s_in to provide some of the warnings

  //service the LEDs...blink slow normally, blink fast if recording
  myTympan.serviceLEDs(millis(),audioSDWriter.getState() == AudioSDWriter::STATE::RECORDING);

  //service the potentiometer...if enough time has passed
  if (USE_VOLUME_KNOB) servicePotentiometer(millis());
  
  //periodically print the CPU and Memory Usage
  if (myState.flag_printCPUandMemory) myState.printCPUandMemory(millis(), 3000); //print every 3000msec  (method is built into TympanStateBase.h, which myState inherits from)
  if (myState.flag_printCPUandMemory) myState.printCPUtoGUI(millis(), 3000);     //send to App every 3000msec (method is built into TympanStateBase.h, which myState inherits from)

  //print info about the signal processing
  updateAveSignalLevels(millis());
  if (myState.enable_printAveSignalLevels) myState.printAveSignalLevels(millis(),3000);
 

} //end loop()


// ///////////////// Servicing routines

//servicePotentiometer: listens to the blue potentiometer and sends the new pot value
//  to the audio processing algorithm as a control parameter
void servicePotentiometer(unsigned long curTime_millis) {
  static unsigned long updatePeriod_millis = 100; //how many milliseconds between updating the potentiometer reading?
  static unsigned long lastUpdate_millis = 0;
  static float prev_val = -1.0;

  //has enough time passed to update everything?
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?

    //read potentiometer
    float val = float(myTympan.readPotentiometer()) / 1023.0; //0.0 to 1.0
    val = (1.0/9.0) * (float)((int)(9.0 * val + 0.5)); //quantize so that it doesn't chatter...0 to 1.0

    //send the potentiometer value to your algorithm as a control parameter
    //float scaled_val = val / 3.0; scaled_val = scaled_val * scaled_val;
    if (abs(val - prev_val) > 0.05) { //is it different than befor?
      prev_val = val;  //save the value for comparison for the next time around
      setDigitalGain_dB(val*45.0f - 25.0f,false); //the "false" is to suppress printing the current gain settings to the USB Serial
      Serial.print("servicePotentiometer: digital gain (dB) = ");Serial.println(myState.digital_gain_dB);
    }
    lastUpdate_millis = curTime_millis;
  } // end if
} //end servicePotentiometer();



void updateAveSignalLevels(unsigned long curTime_millis) {
  static unsigned long updatePeriod_millis = 100; //how often to perform the averaging
  static unsigned long lastUpdate_millis = 0;
  float update_coeff = 0.2; //Smoothing filter.  Choose 1.0 (for no smoothing) down to 0.0 (which would never update)

  //is it time to update the calculations
  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0; //handle wrap-around of the clock
  if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis) { //is it time to update the user interface?
    for (int i=0; i<MAX_N_CHAN; i++) { //loop over each band
      myState.aveSignalLevels_dBFS[i] = (1.0f-update_coeff)*myState.aveSignalLevels_dBFS[i] + update_coeff*multiBandWDRC[0].compbank.compressors[i].getCurrentLevel_dB(); //running average
    }
    lastUpdate_millis = curTime_millis; //we will use this value the next time around.
  }
}

// ////////////////////////////// Functions to set the parameters and maintain the state

// Control the System Gains
float setOutputGain_dB(float gain_dB) {  return myState.output_gain_dB = myTympan.volume_dB(gain_dB); }
float setDigitalGain_dB(float gain_dB) { return setDigitalGain_dB(gain_dB, true); }
float setDigitalGain_dB(float gain_dB, bool printToUSBSerial) {  
  myState.digital_gain_dB = multiBandWDRC[0].broadbandGain.setGain_dB(gain_dB); //this actually sets the gain
  myState.digital_gain_dB = multiBandWDRC[1].broadbandGain.setGain_dB(gain_dB); //this actually sets the gain
  
  serialManager.updateGainDisplay();
  if (printToUSBSerial) printGainSettings();
  return myState.digital_gain_dB;
}
float incrementDigitalGain(float increment_dB) { return setDigitalGain_dB(myState.digital_gain_dB + increment_dB); }

void printGainSettings(void) {
  for (int I_LR=0; I_LR <=1; I_LR++) {
   Serial.print("Chan " + String(I_LR));
    Serial.print(", Gain (dB): ");
    //Serial.print("Input PGA = "); Serial.print(myState.input_gain_dB,1);
    Serial.print(" Per-Channel = ");
    int n_chan = multiBandWDRC[I_LR].get_n_chan();
    for (int i=0; i<n_chan; i++) {
      Serial.print(multiBandWDRC[I_LR].compbank.getLinearGain_dB(i),1); //gets the linear gain setting
      Serial.print(", ");
    }
    Serial.print("Knob = "); Serial.print(myState.digital_gain_dB,1);
    Serial.println();
  }
}

// Control Printing of Signal Levels
void togglePrintAveSignalLevels(bool as_dBSPL) { 
  myState.enable_printAveSignalLevels = !myState.enable_printAveSignalLevels; 
  myState.printAveSignalLevels_as_dBSPL = as_dBSPL;
};
