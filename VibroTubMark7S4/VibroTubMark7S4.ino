/*******************************************************************************
  Tupperware Tub Music
  ------------------------------
  Touch sensitive musical vibration tub!
  For the students at Royal School Manchester, Seashell Trust

  By Rachael Moat - Based on Touch_MP3.ino

  Accepts input on track 11
  Music selection tracks 0-10


****IMPORTANT READ THIS! ****

  Must include default Bare Conductive files on microSD card to eliminate blips
  from previous tracks!



*******************************************************************************/

/*******************************************************************************

  Bare Conductive Touch MP3 player
  ------------------------------
  Touch_MP3.ino - touch triggered MP3 playback

  Based on code by Jim Lindblom and plenty of inspiration from the Freescale
  Semiconductor datasheets and application notes.

  Bare Conductive code written by Stefan Dzisiewski-Smith and Peter Krige.

  This work is licensed under a Creative Commons Attribution-ShareAlike 3.0
  Unported License (CC BY-SA 3.0) http://creativecommons.org/licenses/by-sa/3.0/
*******************************************************************************/

//test edit

// touch includes
#include <MPR121.h>
#include <Wire.h>
#define MPR121_ADDR 0x5C
#define MPR121_INT 4

// mp3 includes
#include <SPI.h>
#include <SdFat.h>
#include <SdFatUtil.h>
#include <SFEMP3Shield.h>

// mp3 variables
SFEMP3Shield MP3player;
byte result;
int lastPlayed = 11; // Needs to be different to selectedTrack
// RLM addition!
bool musicIsPaused = false;

/******** RLM CORE mode variables!!
  One of the core modes (startStop/playPause/triggerOnlyMode) needs to be true.
  Additional releaseMode functionality can be activated if desired.
  Any duplication or mixing of modes will lead to very strange behaviour
**************************************/
bool startStopMode = true;
bool startStopReleaseMode = true;

bool playPauseMode = false;
bool playPauseReleaseMode = false;
bool autoResetOn = false;

bool triggerOnlyMode = false;

bool loopOn = true;

bool sequenceMode = false;

byte sequenceMin = 22;
byte sequenceMax = 63;
byte sequenceNumber = sequenceMin;
bool sequenceChecker = true;

bool randomMode = false;


// RLM TupperwareTub variables
byte connectedElectrode = 11;
byte selectedTrack = 0;


// RLM Loop variables
long duration = 0; // length of time in (ms) the connectedElectrode has been active

long startTime; // the value returned from millis when the electrode is touched

//RLM autoreset variables

long stopTime; // the value returned from millis when the electrode is released
unsigned long standbyDuration; // the time since the last electrode was released
#define autoResetDuration 60000 // time in ms until device is reset. (5 mins equiv to 300000 ms)
bool standbyActive = false;

// touch behaviour definitions
#define firstPin 0
#define lastPin 11 // last electrode in use (not just track selection)

// sd card instantiation
SdFat sd;

// define LED_BUILTIN for older versions of Arduino
#ifndef LED_BUILTIN
#define LED_BUILTIN 13
#endif

//*****LIGHTS******

#include <FastLED.h>

#define DATA_PIN     11
#define CLOCK_PIN 13
#define NUM_LEDS    30
#define BRIGHTNESS  66
// #define LED_TYPE    WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define UPDATES_PER_SECOND 100


CRGBPalette16 currentPalette;
TBlendType    currentBlending;

bool lightUpMode = true;
bool lightStartStopMode = true;
// if lightStartStopMode is false and lightOnMode is true
// touching the top activates the moving of the LEDS

bool setupComplete = false; // used to avoid lights displaying on setup


void setup() {
  delay( 500 ); // power-up safety delay ok as min if tupperware activated!

  Serial.begin(57600);

  pinMode(LED_BUILTIN, OUTPUT);

  //while (!Serial) ; {} //uncomment when using the serial monitor
  //Serial.println("Bare Conductive Touch MP3 player");

  /********* DON"T TOUCH THIS CODE!! *********/
  if (!sd.begin(SD_SEL, SPI_HALF_SPEED)) sd.initErrorHalt();
  if (!MPR121.begin(MPR121_ADDR)) Serial.println("error setting up MPR121");
  MPR121.setInterruptPin(MPR121_INT);
  /*****************************************/


  /******
     FastLED Code!

  */

  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS);
  FastLED.setBrightness(  BRIGHTNESS );


  /* COLOR PALETTE HOW TO

     FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
     OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.

     currentPalette = CRGB(0,0,0);  can be used for deactivating lights on a particular electrode

     Pure colours work best for Short Trigger only tracks e.g. currentPalette = CRGB(0,0,255) (0-255)

     Additional functions include
     SetupBlueAndYellowPalette();
     SetupPurpleAndGreenPalette();

      See Color Palette Example in FastLED lib for further details on customisation
  */


  currentPalette = PartyColors_p;
  currentBlending = LINEARBLEND;


  /*
     END OF LIGHT CODE
  */

  result = MP3player.begin();
  MP3player.setVolume(0, 0); //-3dB

  /******** Set individual volume settings ********
    MP3player.setVolume(X,Y) where X = L channel, Y = R channel
    Range from 0 (maximum) to 254 (muted). Volume units -0.5dB per count
    e.g. 10 = -5dB
    Can be changed anywhere in code after MP3player.begin()
  **************************************************/


  if (result != 0) {
    Serial.print("Error code: ");
    Serial.print(result);
    Serial.println(" when trying to start MP3 player");
  }

  /******** Set indiviudal electrode thresholds ********
    // individual settings can be set by (x, y) where (x = electrode, y = threshold)
    // defaults: TouchPad -: Touch:40 , Release: 20 | ProximityPad -: Touch: 8, Release 4
  *******************************************************/

  // Tupperware top threshold
  MPR121.setTouchThreshold(connectedElectrode, 40);
  MPR121.setReleaseThreshold(connectedElectrode, 39);

  // Track selection electrodes (Electrodes 0-10)
  int x;
  for (x = 0; x < 11; x++) {
    MPR121.setTouchThreshold(x, 20);
    MPR121.setReleaseThreshold(x, 10);
  }
  selectedTrack = 0; //DEFAULT TRACK
  studentS4(selectedTrack); // DEFAULT BEHAVIOURS FOR TRACK

  stopLights();
}

void loop() {
  readTouchInputs();
  if (loopOn)loopTrack();
  if (autoResetOn)autoReset();

  //startLights();

  //START LIGHTS IF MUSIC IS ACTIVATED (complicated due to different start modes!)
  // lastPlayed==selectedTrack fix might cause all sorts of pain for sequential triggers

  if ((lightUpMode && playPauseMode && setupComplete && !musicIsPaused && lastPlayed == selectedTrack) ||
      (!playPauseMode && MP3player.isPlaying() && lastPlayed == selectedTrack)&& lightUpMode)startLights();

  // Stop lights if nowt's happening

  if (!MP3player.isPlaying()) stopLights();



}

/*
void studentOne(int selectedTrack) {

  /*****studentOne Info*********
    Specific behaviour for studentOne's project
    Insert in electrodeTouchBehaviours() of relevent electrodes.

    Track list
    00 Gamelan Drum Loop (startStop behaviour)
    01 Circle of Life (playPause behaviour)
    02 Lion King percussion (playPause behaviour)
    03 I Just Can't Wait to be King (playPause behaviour)
    04 Hakuna Matata (playPause behaviour)
    05 Lion Sleeps tonight (playPause behaviour)
    06 Shadowland (playPause behaviour)
    07 Be Prepared (playPause behaviour)
    08 Twinkle Twinkle (vocal track - playPause behaviour)
    09 Old Macdonald (playPause behaviour)
    10 Wheels on a Bus (playPause behaviour)
    11 <ACTIVATE TRACK>

  *************************/
/*

  if (selectedTrack <= 6 && selectedTrack != 3 && selectedTrack != 4) {
    // set the appropriate mode for the track

    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = true;
    playPauseReleaseMode = true;
    loopOn = true;
    autoResetOn = true;

    triggerOnlyMode = false;
    sequenceMode = false;
    randomMode = false;

    currentPalette = RainbowColors_p;
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)

  }

  if (selectedTrack == 3) {


    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = false;
    sequenceMode = true;
    randomMode = false;
    loopOn = false;


    currentPalette = OceanColors_p;
    //SetupPurpleAndGreenPalette();
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)
  }


  if (selectedTrack == 4) {


    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = false; // for repeated trigger only, use sequenceMode for variable trigger
    loopOn = false;

    sequenceMode = false;
    randomMode = true;

    currentPalette = OceanColors_p;
    // SetupPurpleAndGreenPalette();
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)
  }


  if (selectedTrack <= lastPin && selectedTrack >= 7) {



    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = true;
    loopOn = false;

    sequenceMode = false;
    randomMode = false;
    currentPalette = CRGB(0, 0, 255);
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)

  }
}

/*

/*
void studentTwo(int selectedTrack)
{
if (selectedTrack < 11) {
    // set the appropriate mode for the track

    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = true;
    playPauseReleaseMode = true;
    loopOn = true;
    autoResetOn = true;

    triggerOnlyMode = false;
    sequenceMode = false;
    randomMode = false;

    currentPalette = RainbowColors_p;
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)

  }

  
  }

  */


/*  
void studentPiper(int selectedTrack) {

  /*****studentPiper Info*********
    Specific behaviour for studentPiper's project
    Insert in electrodeTouchBehaviours() of relevent electrodes.

    Track list
    00  (startStop behaviour)
    01  (playPause behaviour)
    02  (playPause behaviour)
    03 (playPause behaviour)
    04 (playPause behaviour)
    05  (playPause behaviour)
    06  (playPause behaviour)
    07  (playPause behaviour)
    08  (vocal track - playPause behaviour)
    09  (playPause behaviour)
    10  (playPause behaviour)
    11 <ACTIVATE TRACK>

  *************************/
/*

  if (selectedTrack <= 5) {
    // set the appropriate mode for the track

    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = true;
    playPauseReleaseMode = true;
    loopOn = true;
    autoResetOn = true;

    triggerOnlyMode = false;
    sequenceMode = false;
    randomMode = false;

    currentPalette = RainbowColors_p;
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)

  }

  if (selectedTrack == 6) {


    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = false;
    sequenceMode = true;
    randomMode = false;
    loopOn = false;


    currentPalette = OceanColors_p;
    //SetupPurpleAndGreenPalette();
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)
  }


  if (selectedTrack == 7) {


    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = false; // for repeated trigger only, use sequenceMode for variable trigger
    loopOn = false;

    sequenceMode = false;
    randomMode = true;

    currentPalette = CRGB(0, 255, 0);;
    // SetupPurpleAndGreenPalette();
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)
  }


  if (selectedTrack <= lastPin && selectedTrack >= 8) {



    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = true;
    loopOn = false;

    sequenceMode = false;
    randomMode = false;
    currentPalette = CRGB(0, 0, 255);
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)

  }
}
*/

void studentS4(int selectedTrack) {

  /*****studentS4 Info*********
    Specific behaviour for studentPiper's project
    Insert in electrodeTouchBehaviours() of relevent electrodes.

    Track list
    00  (startStop behaviour)
    01  (playPause behaviour)
    02  (playPause behaviour)
    03 (playPause behaviour)
    04 (playPause behaviour)
    05  (sequence track - twinkle)
    06  (random track - siund effects)
    07  (playPause behaviour)
    08  (vocal track - playPause behaviour)
    09  (playPause behaviour)
    10  (playPause behaviour)
    11 <ACTIVATE TRACK>

  *************************/


  if (selectedTrack <= 4) {
    // set the appropriate mode for the track

    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = true;
    playPauseReleaseMode = true;
    loopOn = true;
    autoResetOn = true;

    triggerOnlyMode = false;
    sequenceMode = false;
    randomMode = false;

    currentPalette = OceanColors_p;
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)

  }

  if (selectedTrack == 5) {


    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = false;
    sequenceMode = true;
    randomMode = false;
    loopOn = false;


    currentPalette = CRGB(0,0,0);
    //SetupPurpleAndGreenPalette();
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)
  }



  if (selectedTrack == 6)
  {
    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = false;
    sequenceMode = false;
    randomMode = true;
    loopOn = false;


    currentPalette = CRGB(0,255,0);
    //SetupPurpleAndGreenPalette();
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)
  }

  if (selectedTrack == 7) {


    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = true; // for repeated trigger only, use sequenceMode for variable trigger
    loopOn = false;

    sequenceMode = false;
    randomMode = false;

    currentPalette = CRGB(0, 0, 0);;
    // SetupPurpleAndGreenPalette();
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)
  }


  if (selectedTrack <= lastPin && selectedTrack >= 8) {



    startStopMode = false;
    startStopReleaseMode = false;

    playPauseMode = false;
    playPauseReleaseMode = false;
    autoResetOn = false;

    triggerOnlyMode = true;
    loopOn = false;

    sequenceMode = false;
    randomMode = false;
    currentPalette = CRGB(0, 0, 255);
    // currentPalette = CRGB(0,0,0); // disable lights for particular electrode(s)

  }
}



void loopTrack() {

  duration = millis() - startTime; // see how long the electrode has been activated

  if (!MP3player.isPlaying() && (!musicIsPaused) && (lastPlayed == selectedTrack)) { // if the MP3 is not currently playing check to see whether it has reached the end of a loop
    if (duration > 0) {
      playMusic(lastPlayed);
      Serial.print("looping track ");
      Serial.println(lastPlayed);

    }
  }
}

void timerReset() {
  startTime = 0; //reset timer
  duration = 0;  // reset duration
  startTime = millis();
}

void autoReset() {
  standbyDuration = millis() - stopTime; // see how long the device has been passive
  if ((standbyDuration > autoResetDuration) && musicIsPaused) {
    if (!standbyActive)Serial.println("Standby activated"); //show the message first time it's true
    standbyActive = true;

  }

}

void electrodeTouchBehaviours(int electrode) {
  /******electrodeTouchBehaviours() how-to ****************
    Insert in readTouchInputs() after if(MPR121.isNewTouch(i))
  ********************************************/



  /****** Tupperware Tub behaviour ****************
    For Tupperware Tub behaviour:
    Track Selection electrodes 1-10
    Do action on activation of connectedElectrode
  ********************************************/
  if (electrode <= lastPin && electrode >= firstPin && electrode != connectedElectrode) {
    // lastPin defined as last used pin on touchboard.

    if (electrode == 3) { // temp change!
      lightUpMode = !lightUpMode;
    } else {
      selectedTrack = electrode;
    }


    if (lastPlayed != selectedTrack) {
      MP3player.stopTrack();// BOB TESTED
    }



    studentS4(selectedTrack); //project specific behaviours
  }



  if (electrode == connectedElectrode) {
    setupComplete = true;


    // pick a mode style dependent on desired behaviour

    if (startStopMode)startStopTracks(electrode);
    if (playPauseMode)playPauseTracks(electrode);
    if (triggerOnlyMode)triggerOnlyTracks(electrode);
    if (sequenceMode)startStopTracks(sequenceTracks());
    if (randomMode)startStopTracks(randomTracks());




  }
}

void electrodeReleaseBehaviours(int electrode) {
  /******electrodeReleaseBehaviours() how-to ****************
    Insert in readTouchInputs() after if(MPR121.isNewRelease(i))
  ********************************************/

  if (electrode == connectedElectrode)
  {

    // pick a mode style dependent on desired behaviour

    if (playPauseReleaseMode)pauseMusic(selectedTrack);
    if (startStopReleaseMode)stopMusic(selectedTrack);
    if (lightStartStopMode)stopLights();
  }
}


void playMusic(int selectedTrack) {
  /*** playMusic how-to ****
    Function mainly for convienience
    plays the track using MP3player.playTrack()
    But ALSO updates global variables which need to be kept tabs on
    these are:
    lastPlayed (always updated to selectedTrack here)
    musicIsPaused (false when the music is playing!)
  *************************/

  // play selected track
  MP3player.playTrack(selectedTrack);
  Serial.print("playing track ");
  Serial.println(selectedTrack);

  // update global variables
  lastPlayed = selectedTrack;
  musicIsPaused = false;
  standbyActive = false;
  sequenceChecker = true;
  

  if (autoResetOn)timerReset();
}

void resumeMusic(int selectedTrack) {
  MP3player.resumeMusic();

  Serial.print("now resume ");
  Serial.print("track ");
  Serial.println(selectedTrack);
  musicIsPaused = false;
  standbyActive = false;

  if (autoResetOn)timerReset();
}

void pauseMusic(int selectedTrack) {
  /*** pauseMusic how-to ****
    Function mainly for convienience
    plays the track using MP3player.pauseMusic()
    But ALSO updates global variables which need to be kept tabs on
    these are:
    musicIsPaused (true when the music is paused!)
  *************************/

  // pause selected track
  MP3player.pauseMusic();
  Serial.print("pausing track ");
  Serial.println(selectedTrack);

  // update global variables
  musicIsPaused = true;
  if (autoResetOn) {
    stopTime = millis();
    standbyDuration = 0; // reset the values required for standby
  }

}

void stopMusic(int selectedTrack) {
  /* Purely for convienience - nothing fancy here!
  ***********************************************/
  MP3player.stopTrack();
  Serial.print("stopping track ");
  Serial.println(selectedTrack);
  musicIsPaused = true;
  if (autoResetOn) {
    stopTime = millis();
    standbyDuration = 0; // reset the values required for standby
  }

}

void triggerOnlyTracks(int electrode) {

  /****** FUNCTION how-to ********
      Starts a track then ignores input until track is finished.
      You can overide this by changing to a different track!

      insert in electrodeTouchBehaviours() function
      Ensure to test for triggerOnlyMode request
        e.g. if(triggerOnlyMode)triggerOnlyTracks(i);
      Remember to update desired mode first!
      Remember to set loopOn variable to false.
      With loopOn = true, looping will continue
      until another electrode is pressed.
      N.B. This function does not affect MPR121.isNewRelease

  *************************************************************/

    

  if (MP3player.isPlaying()) {
    if (lastPlayed == selectedTrack) {
      // if we're already playing the requested track - do nothing!
    } else {
      // if we're already playing a different track, stop that
      // one and play the newly requested one
      MP3player.stopTrack();
    }
  } else {
    // if we're playing nothing, play the requested track
    playMusic(selectedTrack);

  }

}


void playPauseTracks(int electrode) {

  /****** FUNCTION how-to ********
      insert in electrodeTouchBehaviours() function
      Ensure to test for playPauseMode request
        e.g. if(playPauseMode)playPauseTracks(i);
      Remember to update desired mode first!
      N.B. This function does not (currently!) affect MPR121.isNewRelease
  *************************************************************/


  if (MP3player.isPlaying()) {

    if (lastPlayed == selectedTrack)
    {

      if (!musicIsPaused) {
        // if we're already playing the requested track, pause it
        pauseMusic(selectedTrack);
      }


      else {

        if (standbyActive) {
          //if standby is active restart the track.
          MP3player.stopTrack();
          playMusic(selectedTrack);
        }

        else {
          // if the track is paused, resume it.

          resumeMusic(selectedTrack);


        }
      }

    }

    else {
      // if we're already playing a different track, stop that
      // one and play the newly requested one
      MP3player.stopTrack();
      playMusic(selectedTrack);
    }

  } else {
    // if we're playing nothing play the requested track
    playMusic(selectedTrack);
  }

}

void startStopTracks(int electrode) {

  /****** FUNCTION how-to ********
      insert in electrodeTouchBehaviours() function
      Ensure to test for startStopMode request
        e.g. if(startStopMode)startStopTracks(i);
      Remember to update desired mode first!
      N.B. This function does not (currently!) affect MPR121.isNewRelease
  *************************************************************/

  if (MP3player.isPlaying()) {
    if (lastPlayed == selectedTrack) {
      // if we're already playing the requested track, stop it
      stopMusic(selectedTrack);
    } else {
      // if we're already playing a different track, stop that
      // one and play the newly requested one
      MP3player.stopTrack();
      playMusic(selectedTrack);


    }
  } else {
    // if we're playing nothing, play the requested track
    playMusic(selectedTrack);

  }

}

void readTouchInputs() {
  if (MPR121.touchStatusChanged()) {

    MPR121.updateTouchData();


    if (MPR121.getNumTouches() <= 1) { // Ignores multiple touches

      /*****************
        Don't touch the top of the tub whilst changing the track!
      ******************************************************/


      for (int i = 0; i < 12; i++) { // Check which electrodes were pressed
        if (MPR121.isNewTouch(i)) {

          //pin i was just touched
          Serial.print("pin ");
          Serial.print(i);
          Serial.println(" was just touched");
          digitalWrite(LED_BUILTIN, HIGH);

          
          electrodeTouchBehaviours(i);



        } else {
          if (MPR121.isNewRelease(i)) {
            Serial.print("pin ");
            Serial.print(i);
            Serial.println(" is no longer being touched");
            digitalWrite(LED_BUILTIN, LOW);

            electrodeReleaseBehaviours(i);

          }
        }
      }
    } MPR121.getNumTouches();
  }
}

void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
  uint8_t brightness = BRIGHTNESS;

  for ( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
    colorIndex += 3;
  }
}

// This function sets up a palette of blue and yellow stripes.
void SetupBlueAndYellowPalette()
{
  CRGB blue = CHSV( HUE_BLUE, 255, 255);
  CRGB yellow  = CHSV( HUE_YELLOW, 255, 255);
  CRGB black  = CRGB::Black;

  currentPalette = CRGBPalette16(
                     yellow,  yellow,  yellow,  black,
                     blue, blue, blue,  black,
                     yellow,  yellow,  yellow,  black,
                     blue, blue, blue,  black );
}


// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette()
{
  CRGB purple = CHSV( HUE_PURPLE, 255, 255);
  CRGB green  = CHSV( HUE_GREEN, 255, 255);
  CRGB black  = CRGB::Black;

  currentPalette = CRGBPalette16(
                     green,  green,  green,  black,
                     purple, purple, purple,  black,
                     green,  green,  green,  black,
                     purple, purple, purple,  black );
}



void startLights()
{
  static uint8_t startIndex = 0;
  startIndex = startIndex + 1; /* motion speed */

  FillLEDsFromPaletteColors( startIndex);

  FastLED.show();
  FastLED.delay(1000 / UPDATES_PER_SECOND);
}

void stopLights()
{
  FastLED.clear();
  FastLED.show();
}


int sequenceTracks()
{

   // if (!(lastPlayed >= sequenceMin) && !(lastPlayed <= sequenceMax)) setConnectedElectrodeThreshold();
   // Use if need to adjust thresholds on the fly
    
    if(sequenceChecker)
    {
    selectedTrack = sequenceNumber;
    sequenceNumber++;
    if (sequenceNumber > sequenceMax)sequenceNumber = sequenceMin; //
    sequenceChecker = false;
    return selectedTrack;
    }
  
}


int randomTracks()
{
  //randomSeed(analogRead(0)); // read from an analog port with nothing attached
  int randomNumber = random(11, 21);
  selectedTrack = randomNumber;
  return selectedTrack;
    
}


/*
 * |||| NOT USED||||
 * Use if need to change threshold of connected electrode when swapping modes. Will need to 
 * add corresponding reset modes to playPause/startStop etc if used
void setConnectedElectrodeThreshold()
{
  MPR121.setTouchThreshold(connectedElectrode, 20);
  MPR121.setReleaseThreshold(connectedElectrode, 19);
  }
  */
