#ifdef USE_TINYUSB
  #include <Adafruit_TinyUSB.h>
  #include <MIDI.h>
#endif

#define USB_MFG_DESCRIPTOR "Schoolbus                     "
#define USB_PRODUCT_DESCRIPTOR "Schoolbus                     "
#define USB_MIDI_DESCRIPTOR "Schoolbus"

/* Pin mapping for the Adafruit Feather RP2040 and our custom PCB */
#define PIN_LED_1 29
#define PIN_LED_2 24
#define PIN_MUX_0 10
#define PIN_MUX_1 9
#define PIN_MUX_2 8
#define PIN_MUX_3 7
#define PIN_MUX_COM 26
#define PIN_BTN_BANK 25
#define PIN_MIDI_TX 0
#define PIN_MIDI_RX 1

/* You shouldn't need to change these unless you're doing your own board 
   in which case you probably have to rewrite some of the code anyway. */
#define NUM_BANKS 4
#define NUM_POTS 16

/* The analog signals don't quite hit max and min ADC values and there's some jitter
   so limit the actual min and max ADC values, and add some deadbanding + filtering.
   If you encounter issues with dead zones or jitter, try playing with these a bit. */
#define MIN_VAL 4
#define MAX_VAL 1015
#define DEADBAND_VAL 4
#define MOVING_AVG_LENGTH 32

Adafruit_USBD_MIDI usbMidi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usbMidi, usbMidiDevice);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, serialMidiDevice);

/* Change these to adjust the banks to your liking, then (re)upload. 
   The groups of 16 are individual banks, in format 
   {midiChannel, ccNumber, minValue, maxValue}. */
int banks[NUM_BANKS][NUM_POTS][4] = {
  {{1, 1, 0, 127},  {1, 2, 0, 127},  {1, 3, 0, 127},  {1, 4, 0, 127},  
   {1, 5, 0, 127},  {1, 6, 0, 127},  {1, 7, 0, 127},  {1, 8, 0, 127}, 
   {1, 9, 0, 127},  {1, 10, 0, 127}, {1, 11, 0, 127}, {1, 12, 0, 127}, 
   {1, 13, 0, 127}, {1, 14, 0, 127}, {1, 15, 0, 127}, {1, 16, 0, 127}},

  {{1, 17, 0, 127}, {1, 18, 0, 127}, {1, 19, 0, 127}, {1, 20, 0, 127},
   {1, 21, 0, 127}, {1, 22, 0, 127}, {1, 23, 0, 127}, {1, 24, 0, 127}, 
   {1, 25, 0, 127}, {1, 26, 0, 127}, {1, 27, 0, 127}, {1, 28, 0, 127}, 
   {1, 29, 0, 127}, {1, 30, 0, 127}, {1, 31, 0, 127}, {1, 32, 0, 127}},

  {{1, 33, 0, 127}, {1, 34, 0, 127}, {1, 35, 0, 127}, {1, 36, 0, 127}, 
   {1, 37, 0, 127}, {1, 38, 0, 127}, {1, 39, 0, 127}, {1, 40, 0, 127}, 
   {1, 41, 0, 127}, {1, 42, 0, 127}, {1, 43, 0, 127}, {1, 44, 0, 127}, 
   {1, 45, 0, 127}, {1, 46, 0, 127}, {1, 47, 0, 127}, {1, 48, 0, 127}},

  {{1, 49, 0, 127}, {1, 50, 0, 127}, {1, 51, 0, 127}, {1, 52, 0, 127}, 
   {1, 53, 0, 127}, {1, 54, 0, 127}, {1, 55, 0, 127}, {1, 56, 0, 127}, 
   {1, 57, 0, 127}, {1, 58, 0, 127}, {1, 59, 0, 127}, {1, 60, 0, 127}, 
   {1, 61, 0, 127}, {1, 62, 0, 127}, {1, 63, 0, 127}, {1, 64, 0, 127}}
};
bool bankChangeRequested = false;
int bank = -1;

/* The pots are hooked up via a single 4067 multiplexer controlled by 4 digital pins */
int muxControlPins[4] = {PIN_MUX_0, PIN_MUX_1, PIN_MUX_2, PIN_MUX_3};
int muxChannels[16][4] = {
  {0,0,0,0}, {1,0,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,1,0}, {1,0,1,0}, {0,1,1,0}, {1,1,1,0},
  {0,0,0,1}, {1,0,0,1}, {0,1,0,1}, {1,1,0,1}, {0,0,1,1}, {1,0,1,1}, {0,1,1,1}, {1,1,1,1} 
};

/* Keep a moving average over raw values to filter noisy analog signal */
int values[NUM_POTS][MOVING_AVG_LENGTH];
int valueIdx = 0;
bool valuesStable = false;

/* Actual scaled MIDI values, should be only sent when they change */
int midiValues[NUM_POTS];
bool midiValuesChanged[NUM_POTS];
bool midiValuesDirty = false;

void setup() {
  Serial.begin(115200);

  initPins();
  initMidi();
  initValues();

  /* Read a few values until the averages stabilize, so we don't end up
     sending a bunch of unintended CC messages on startup. */
  for (int i = 0; i < 256; i++) {
    readValues();
    updateMidiValues();
  }
  valuesStable = true;

  indicateStartup();
  doBankChange(); // Init to bank 0 so we get proper LED state.
}

void loop() {
  readValues();
  updateMidiValues();
  sendMidiValues();

  if (bankChangeRequested) {
    doBankChange();
  }
}

/* Reads values from multiplexer / analog input to next moving average slot.
   Does some deadbanding to prevent value jitter. */
void readValues() {
  for(int i = 0; i < NUM_POTS; i++) {
    int rawVal = constrain(readMux(i), MIN_VAL, MAX_VAL);
    
    if (rawVal < (values[i][valueIdx] - DEADBAND_VAL) || 
        rawVal > (values[i][valueIdx] + DEADBAND_VAL)) {
      values[i][valueIdx] = rawVal;
    }
  }

  valueIdx = (valueIdx + 1) % MOVING_AVG_LENGTH;
}

/* Sends the changed midi values thru both USB and serial MIDI */
void sendMidiValues() {
  if (!midiValuesDirty) { return; }

  for (int i = 0; i < NUM_POTS; i++) {
    if (midiValuesChanged[i]) {
      int ccNumber = banks[bank][i][1];
      int channel  = banks[bank][i][0];
      int ccValue  = map(midiValues[i], 0, 127, banks[bank][i][2], banks[bank][i][3]);

      usbMidiDevice.sendControlChange(ccNumber, ccValue, channel);
      serialMidiDevice.sendControlChange(ccNumber, ccValue, channel);
      midiValuesChanged[i] = false;
    }
  }
  
  midiValuesDirty = false;
}

/* Once raw values have been read, do some averaging and scale to MIDI CCs.
   If any values have been changed since last write, mark dirty for sending. */
void updateMidiValues() {
  for (int i = 0; i < NUM_POTS; i++) {
    int newVal = 0;
    
    for (int j = 0; j < MOVING_AVG_LENGTH; j++) {
      newVal += values[i][j];
    }
    newVal = map(newVal / MOVING_AVG_LENGTH, MIN_VAL, MAX_VAL, 0, 128);

    int midiVal = constrain(newVal, 0, 127);
    if (midiValues[i] != midiVal) {
      midiValues[i] = midiVal;

      if (valuesStable) {
        midiValuesChanged[i] = true;
        midiValuesDirty = true;
      }
    }
  }
}

int readMux(int channel) {
  for(int i = 0; i < 4; i ++) {
    digitalWrite(muxControlPins[i], muxChannels[channel][i]);
  }
  return analogRead(PIN_MUX_COM);
}

void doBankChange() {
  bank = (bank + 1) % NUM_BANKS;

  // "Binary" LED indicators for the active bank.
  if (bank & 1) { digitalWrite(PIN_LED_2, HIGH); }
  else { digitalWrite(PIN_LED_2, LOW); }

  if (bank & 2) { digitalWrite(PIN_LED_1, HIGH); }
  else { digitalWrite(PIN_LED_1, LOW); }

  bankChangeRequested = false;
}

void initPins() {
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);
  pinMode(PIN_MUX_0, OUTPUT);
  pinMode(PIN_MUX_1, OUTPUT);
  pinMode(PIN_MUX_2, OUTPUT);
  pinMode(PIN_MUX_3, OUTPUT);
  pinMode(PIN_MUX_COM, INPUT);
  pinMode(PIN_BTN_BANK, INPUT);
  pinMode(PIN_MIDI_TX, OUTPUT);
  pinMode(PIN_MIDI_RX, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_BTN_BANK),
                  bankChangeInterrupt,
                  FALLING);
}

void initMidi() {
  Serial1.setTX(PIN_MIDI_TX);
  Serial1.setRX(PIN_MIDI_RX);

  USBDevice.setProductDescriptor(USB_PRODUCT_DESCRIPTOR);
  USBDevice.setManufacturerDescriptor(USB_MFG_DESCRIPTOR);
  usbMidi.setStringDescriptor(USB_MIDI_DESCRIPTOR);

  usbMidiDevice.begin(MIDI_CHANNEL_OMNI);
  serialMidiDevice.begin(MIDI_CHANNEL_OMNI);
}

void initValues() {
  for (int i = 0; i < NUM_POTS; i++) {
    midiValues[i] = 0;
    midiValuesChanged[i] = false;

    for (int j = 0; j < MOVING_AVG_LENGTH; j++) {
      values[i][j] = 0;
    }
  }
}

// Blink LEDs once the init is ready
void indicateStartup() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_1, HIGH);
    delay(100);
    digitalWrite(PIN_LED_1, LOW);
    delay(100);
    digitalWrite(PIN_LED_2, HIGH);
    delay(100);
    digitalWrite(PIN_LED_2, LOW);
    delay(100);
  }

  Serial.println("0x10pb ready.");
}

void bankChangeInterrupt() {
  bankChangeRequested = true;
}
