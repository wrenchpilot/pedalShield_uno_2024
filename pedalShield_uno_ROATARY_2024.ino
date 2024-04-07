/**
* James Shawn Carnley
* https://github.com/wrenchpilot
* Multiple effects for pedalShieldUno using debounced toggle switch to
* cycle through defined effects. Double flick the toggle to change effects.
* effect 1: Booster
* effect 2: Bitcrusher
* effect 3: Daft Punk Octaver
* effect 4: Better Tremello

* CC-by-www.Electrosmash.com
* Based on OpenMusicLabs previous works.

* Better Tremolo based on the stomp_tremolo.pde from openmusiclabs.com
* This program does a tremolo effect.  It uses a sinewave storedP
* in program memory to modulate the signal.  The rate at which
* it goes through the sinewave is set by the push buttons,
* which is min/maxed by the speed value.
* https://www.electrosmash.com/forum/pedalshield-uno/453-7-new-effects-for-the-pedalshield-uno#1718
*/

#include <SimpleRotary.h>

// defining hardware resources.
#define LED 13
#define FOOTSWITCH 12

// defining the output PWM parameters
#define PWM_FREQ 0x00FF // pwm frequency - 31.3KHz
#define PWM_MODE 0      // Fast (1) or Phase Correct (0)
#define PWM_QTY 2       // 2 PWMs in parallel

// Setup sine wave for tremolo
const char *const sinewave[] PROGMEM = {
#include "mult16x16.h"
#include "sinetable.h"
};

// Rotary
// Define interrupt pins for rotary encoder
const int ROTARY_CLK_PIN = 2; // ROTARY_CLK_PIN pin
const int ROTARY_DT_PIN = 3;  // ROTARY_DT_PIN pin
const int ROTARY_BTN_PIN = 4; // Button pin
int ctr = 0;
byte lastDir = 0;
volatile int rDir;
volatile int rBtn;
volatile int rLBtn;
volatile bool isLong = false;

SimpleRotary rotary(ROTARY_CLK_PIN, ROTARY_DT_PIN, ROTARY_BTN_PIN);

unsigned int location = 0; // incoming data buffer pointer

// effects variables:
int vol_variable = 16385;
unsigned int speed = 20;         // tremolo speed
unsigned int fractional = 0x00;  // fractional sample position
unsigned int dist_variable = 10; // octaver

// other variables
const int max_effects = 4;
int data_buffer; // temporary data storage to give a 1 sample buffer
int input, bit_crush_variable = 0;
unsigned int read_counter = 0;
unsigned int ocr_counter = 0;
unsigned int effect = 1; // let's start at 1
byte ADC_low, ADC_high;

void setup()
{
  // Serial.begin(9600);
  // setup IO
  pinMode(FOOTSWITCH, INPUT_PULLUP);
  pinMode(LED, OUTPUT);

  // Set Effect Defaults
  switch (effect)
  {
  case (1):
    vol_variable = 32768 / 2;
    ctr = vol_variable;
    break;

  case (2):
    bit_crush_variable = 16 / 2;
    ctr = bit_crush_variable;
    break;

  case (3):
    dist_variable = 250;
    ctr = dist_variable;
    break;

  case (4):
    speed = 512;
    ctr = speed;
    break;
  }

  // setup ADC
  ADMUX = 0x60;  // left adjust, adc0, internal vcc
  ADCSRA = 0xe5; // turn on adc, ck/32, auto trigger
  ADCSRB = 0x07; // t1 capture for trigger
  DIDR0 = 0x01;  // turn off digital inputs for adc0

  // setup PWM
  TCCR1A = (((PWM_QTY - 1) << 5) | 0x80 | (PWM_MODE << 1)); //
  TCCR1B = ((PWM_MODE << 3) | 0x11);                        // ck/1
  TIMSK1 = 0x20;                                            // interrupt on capture interrupt
  ICR1H = (PWM_FREQ >> 8);
  ICR1L = (PWM_FREQ & 0xff);
  DDRB |= ((PWM_QTY << 1) | 0x02); // turn on outputs
  sei();                           // turn on interrupts - not really necessary with arduino
}

void switchEffect(volatile bool isLong = false)
{

  switch (lastDir)
  {
  case 1:
    effect += 1;
    for (int i = 0; i <= effect; i++)
    {
      digitalWrite(LED, LOW);
      delay(300);
    }
    break;
  case 2:
    effect -= 1;
    for (int i = 0; i <= effect; i++)
    {
      digitalWrite(LED, LOW);
      delay(300);
    }
    break;
  }

  if (isLong == false)
  {
    switch (effect)
    {
    case (1):
      vol_variable = 32768 / 2;
      ctr = vol_variable;
      break;

    case (2):
      bit_crush_variable = 16 / 2;
      ctr = bit_crush_variable;
      break;

    case (3):
      dist_variable = 250;
      ctr = dist_variable;
      break;

    case (4):
      speed = 512;
      ctr = speed;
      break;
    }
  }
  else
  {
    switch (effect)
    {
    case (1):
      vol_variable = 32768;
      break;

    case (2):
      bit_crush_variable = 16;
      break;

    case (3):
      dist_variable = 250;
      break;

    case (4):
      speed = 512;
      break;
    }
  }
  if (effect > max_effects)
    effect = 1;
}

void readRotaryEncoder()
{

  rDir = rotary.rotate();
  rBtn = rotary.push();
  rLBtn = rotary.pushLong(1000);

  // Check direction
  if (rDir == 1)
  {
    // CW
    ctr++;
    lastDir = rDir;
  }
  if (rDir == 2)
  {
    // CCW
    ctr--;
    lastDir = rDir;
  }

  if (rBtn == 1)
  {
    switchEffect();
  }

  if (rLBtn == 1)
  {
    switchEffect(true);
  }

  // // Only Serial.print when there is user input.
  // // Constant serial printing without delay can introduce lag where rotary encoder rotation pulses can be missed.
  // if (rDir == 1 || rDir == 2 || rBtn == 1 || rLBtn == 1) {
  //   Serial.print("Counter = ");
  //   Serial.print(ctr);
  //   Serial.println();
  // }
}

void loop()
{

  // Start Loop on Footswitch
  if (1 == 1 /*digitalRead(FOOTSWITCH)*/)
  {
    // Turn on the LED if the effect is ON.
    digitalWrite(LED, HIGH);

    readRotaryEncoder();
  }
  else
  {
    digitalWrite(LED, LOW);
  }
}

ISR(TIMER1_CAPT_vect)
{

  readRotaryEncoder();

  switch (effect)
  {

  case 1: // BOOSTER
    vol_variable = ctr;
    if (rDir == 1)
    {
      if (vol_variable > 0)
        vol_variable - 10; // anti-clockwise rotation
      digitalWrite(LED, LOW);
    }
    if (rDir == 2)
    {
      if (vol_variable < 32768)
        vol_variable + 10;
      digitalWrite(LED, LOW);
    }

    // get ADC data
    ADC_low = ADCL; // you need to fetch the low byte first
    ADC_high = ADCH;
    // construct the input sumple summing the ADC low and high byte.
    input = ((ADC_high << 8) | ADC_low) + 0x8000; // make a signed 16b value

    // the amplitude of the input signal is modified following the vol_variable value
    input = map(input, -32768, +32768, -vol_variable, vol_variable);

    // write the PWM signal
    OCR1AL = ((input + 0x8000) >> 8); // convert to unsigned, send out high byte
    OCR1BL = input;                   // send out low byte
    break;

  case 2: // BITCRUSHER

    bit_crush_variable = ctr;
    if (rDir == 1)
    {
      if (bit_crush_variable < 16)
        bit_crush_variable = bit_crush_variable + 1; // increase the vol
      digitalWrite(LED, LOW);                        // blinks the led
    }
    if (rDir == 2)
    {
      if (bit_crush_variable > 0)
        bit_crush_variable = bit_crush_variable - 1; // decrease vol
      digitalWrite(LED, LOW);                        // blinks the led
    }

    // get ADC data
    ADC_low = ADCL; // you need to fetch the low byte first
    ADC_high = ADCH;
    // construct the input sumple summing the ADC low and high byte.
    input = ((ADC_high << 8) | ADC_low) + 0x8000; // make a signed 16b value

    //// All the Digital Signal Processing happens here: ////
    // The bit_crush_variable goes from 0 to 16 and the input signal is crushed in the next instruction:
    input = input << bit_crush_variable;

    // write the PWM signal
    OCR1AL = ((input + 0x8000) >> 8); // convert to unsigned, send out high byte
    OCR1BL = input;                   // send out low byte
    break;

  case 3: // DAFT PUNK OCTAVER

    dist_variable = ctr;

    if (rDir == 1)
    {
      if (dist_variable < 500)
        dist_variable = dist_variable + 1;
      digitalWrite(LED, LOW); // blinks the led
    }
    if (rDir == 2)
    {
      if (dist_variable > 0)
        dist_variable = dist_variable - 1;
      digitalWrite(LED, LOW); // blinks the led
    }

    ocr_counter++;
    if (ocr_counter >= dist_variable)
    {
      ocr_counter = 0;
      // get ADC data
      ADC_low = ADCL; // you need to fetch the low byte first
      ADC_high = ADCH;
      // construct the input sumple summing the ADC low and high byte.
      input = ((ADC_high << 8) | ADC_low) + 0x8000; // make a signed 16b value

      // write the PWM signal
      OCR1AL = ((input + 0x8000) >> 8); // convert to unsigned, send out high byte
      OCR1BL = input;                   // send out low byte
    }
    break;

  case 4: // BETTER TREMOLO EFFECT

    speed = ctr;

    if (rDir == 1)
    {
      if (speed < 1024)
        speed = speed + 1;    // increase the tremolo
      digitalWrite(LED, LOW); // blinks the led
    }
    if (rDir == 2)
    {
      if (speed > 0)
        speed = speed - 1;    // decrease the tremelo
      digitalWrite(LED, LOW); // blinks the led
    }

    // output the last value calculated
    OCR1AL = ((data_buffer + 0x8000) >> 8); // convert to unsigned, send out high byte
    OCR1BL = data_buffer;                   // send out low byte

    // get ADC data
    ADC_low = ADCL; // you need to fetch the low byte first
    ADC_high = ADCH;
    // construct the input sumple summing the ADC low and high byte.
    input = ((ADC_high << 8) | ADC_low) + 0x8000; // make a signed 16b value

    fractional += speed; // increment sinewave lookup counter
    if (fractional >= 0x0100)
    {                       // if its large enough to go to next sample
      fractional &= 0x00ff; // round off
      location += 1;        // go to next location
      location &= 0x03ff;   // fast boundary wrap for 2^n boundaries
    }
    // fetch current sinewave value
    int amplitude = pgm_read_word_near(sinewave + location);
    amplitude += 0x8000; // convert to unsigned
    int output;
    MultiSU16X16toH16(output, input, amplitude);
    // save value for playback next interrupt
    data_buffer = output;
    break;

  default:
    effect = 1;
    break;
  }
}
