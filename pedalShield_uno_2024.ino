/**
 * CC-by-www.Electrosmash.com
 * Based on OpenMusicLabs previous works.
 *
 * Rotary Encoder Implementation
 * James Shawn Carnley
 * https://github.com/wrenchpilot
 *
 * Multiple effects for pedalShieldUno using a rotary encoder.
 *
 * BANK 1: Flanger2 - https://www.electrosmash.com/forum/pedalshield-uno/453-7-new-effects-for-the-pedalshield-uno#1718
 * BANK 2: Up Down - https://www.electrosmash.com/forum/pedalshield-uno/453-7-new-effects-for-the-pedalshield-uno#1718
 * BANK 3: Daft Punk Octaver - https://github.com/ElectroSmash/pedalshield-uno
 * BANK 4: Better Tremello - https://www.electrosmash.com/forum/pedalshield-uno/453-7-new-effects-for-the-pedalshield-uno#1718
 */

#define ENCODER_DO_NOT_USE_INTERRUPTS
#include <Encoder.h>

// defining hardware resources.
#define LED 13
#define FOOTSWITCH 12
#define EFFECTSBUTTON 7
#define ROTARY_CK 3
#define ROTARY_DT 4

// defining the output PWM parameters
#define PWM_FREQ 0x00FF // pwm frequency - 31.3KHz
#define PWM_MODE 0      // Fast (1) or Phase Correct (0)
#define PWM_QTY 2       // 2 PWMs in parallel

#define MAX_EFFECTS 4

// Up Down
#define SIZE 750  // buffer size, make lower if it clicks
int buffer[SIZE]; // data buffer

// Flanger 2
// defining FX parameters
#define F2MIN 2           // min delay of ~60us
#define F2MAX 200         // max delay of ~6ms
#define F2SIZE F2MAX + 10 // data buffer size - must be more than MAX

// Setup sine wave for tremolo/flanger2
const char *const sinewave[] PROGMEM = {
#include "mult16x16.h"
#include "mult16x8.h"
#include "sinetable.h"
};

// effects variables:
volatile unsigned int speed = 20;        // tremolo speed
volatile unsigned int location = 0;      // incoming data buffer pointer
unsigned int offset = 0;                 // distance to current location
volatile unsigned int fractional = 0x00; // fractional sample position
volatile unsigned int daft_octave = 10;  // octaver
volatile unsigned int ocr_counter = 0;   // Octaver counter
volatile unsigned int input;

// other variables
volatile int data_buffer, data_bufferUD = 0x8000; // temporary data storage to give a 1 sample buffer
volatile int position = -999;                     // Encoder start position
volatile int newPos = 0;                          // Encoder new position
uint8_t effect = 1;                               // let's start at 1
byte dir = 0;                                     // direction of travel in buffer
byte counter = 4;                                 // flanger 2

byte ADC_low, ADC_high;

Encoder rotaryEncoder(ROTARY_CK, ROTARY_DT);

void setup() {
    // setup IO
    pinMode(FOOTSWITCH, INPUT_PULLUP);
    pinMode(EFFECTSBUTTON, INPUT_PULLUP);
    pinMode(ROTARY_CK, INPUT_PULLUP);
    pinMode(ROTARY_DT, INPUT_PULLUP);
    pinMode(LED, OUTPUT);
    setupADC();
}

void setupADC() {
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
    sei();
}

void loop() {

    // Start Loop on Footswitch
    if (digitalRead(FOOTSWITCH)) {
        // Turn on the LED if the effect is ON.
        digitalWrite(LED, HIGH);
        swapEffect();
    } else {
        digitalWrite(LED, LOW);
    }
}

void swapEffect() {
    static byte effects_btn_memory = 0;
    // Check for keypress
    if (!digitalRead(EFFECTSBUTTON)) { // Pulled up so zero = pushed.

        position = 1;
        rotaryEncoder.write(position);

        if (!digitalRead(EFFECTSBUTTON)) { // if it is still pushed after a delay.
            effects_btn_memory = !effects_btn_memory;

            if (effects_btn_memory) {
                digitalWrite(LED, HIGH);
                effect = effect + 1;
                if (effect > MAX_EFFECTS)
                    effect = 1; // loop back to 1st effect
            } else
                digitalWrite(LED, LOW);
        }
        while (!digitalRead(EFFECTSBUTTON))
            ; // wait for low
    }
}

ISR(TIMER1_CAPT_vect) {

    newPos = rotaryEncoder.read();

    switch (effect) {
    case 1: // FLANGER2
        flanger2();
        break;
    case 2: // UP DOWN (No controls)
        upDown();
        break;
    case 3: // DAFT PUNK OCTAVER
        daft_punk_octaver();
        break;
    case 4: // BETTER TREMOLO EFFECT
        better_tremolo();
        break;
    }
}

/* Variation based on the flanger from openmusiclabs.com
this program does a flanger effect, and interpolates between samples
for a smoother sound output.  a rampwave is used to set the variable
delay.  the min and max delay times it swing between is set by MIN
and MAX.  these are in samples, divide by 31.25ksps to get ms delay
times.  the push buttons determines how much the ramp increments
by each time.  this sets the frequency of the delay sweep, which is
min/maxed by B_MIN/B_MAX. In this variant the input signal has been
added to the output.
*/

void flanger2() {
    if (newPos != position) {
        if (newPos > F2MIN && newPos < F2MAX) {
            counter = newPos;
            position = newPos;
            digitalWrite(LED, LOW); // blinks the led
        } else {
            counter = F2MIN;
            position = F2MIN;
            rotaryEncoder.write(F2MIN);
            digitalWrite(LED, LOW); // blinks the led
            digitalWrite(LED, LOW); // blinks the led
        }
    }

    // output the last value calculated
    OCR1AL = ((data_buffer + 0x8000) >> 8); // convert to unsigned, send out high byte
    OCR1BL = data_buffer;                   // send out low byte

    // get ADC data
    byte temp1 = ADCL;                           // you need to fetch the low byte first
    byte temp2 = ADCH;                           // yes it needs to be done this way
    int input = ((temp2 << 8) | temp1) + 0x8000; // make a signed 16b value

    // fetch/store data
    buffer[location] = input; // store current sample
    location++;               // go to next sample position
    if (location >= F2SIZE)
        location = 0;                        // deal with buffer wrap
    int temp = location - (fractional >> 8); // find delayed sample
    if (temp < 0)
        temp += F2SIZE;                // deal with buffer wrap
    int output = buffer[temp] + input; // fetch delayed sample
    temp -= 1;                         // find adjacent sample
    if (temp < 0)
        temp += F2SIZE;                 // deal with buffer wrap
    int output2 = buffer[temp] + input; // get adjacent sample

    // interpolate between adjacent samples
    int temp4; // create some temp variables
    int temp5;

    // multiply by distance to fractional position
    MultiSU16X8toH16(temp4, output, (0xff - (fractional & 0x00ff)));
    MultiSU16X8toH16(temp5, output2, (fractional & 0x00ff));
    output = temp4 + temp5; // sum weighted samples

    // save value for playback next interrupt
    data_buffer = output;

    // up or down count as necessary till MIN/MAX is reached
    if (dir) {
        if ((fractional >> 8) >= F2MAX)
            dir = 0;
        fractional += counter;
    } else {
        if ((fractional >> 8) <= F2MIN)
            dir = 1;
        fractional -= counter;
        ;
    }
}

/*
 CC-by-www.Electrosmash.com
 Based on OpenMusicLabs previous works.
 pedalshield_uno_daftpunk_distortion_octaver.ino creates a distortion effect similar to the used
 in Television Rules the Nation by Daft Punk. It also octaves the signal up/down presing the pushbuttons
*/
void daft_punk_octaver() {

    if (newPos != position) {
        if (newPos > 0 && newPos < 500) {
            daft_octave = newPos;
            position = newPos;
            digitalWrite(LED, LOW); // blinks the led
        } else {
            daft_octave = 0;
            position = 0;
            rotaryEncoder.write(0);
            digitalWrite(LED, LOW); // blinks the led
            digitalWrite(LED, LOW); // blinks the led
        }
    }

    ocr_counter++;
    if (ocr_counter >= daft_octave) {
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
}

/*
Based on the stomp_tremolo.pde from openmusiclabs.com
this program does a tremolo effect.  it uses a sinewave stored
in program memory to modulate the signal.  the rate at which
it goes through the sinewave is set by the rotary encoder,
which is min/maxed by the speed value.
*/
void better_tremolo() {

    if (newPos != position) {
        if (newPos > 0 && newPos < 1024) {
            speed = newPos; // change the vol
            position = newPos;
            digitalWrite(LED, LOW); // blinks the led
        } else {
            speed = 0;
            position = 0;
            rotaryEncoder.write(0);
            digitalWrite(LED, LOW); // blinks the led
            digitalWrite(LED, LOW); // blinks the led
        }
    }

    // output the last value calculated
    OCR1AL = ((data_buffer + 0x8000) >> 8); // convert to unsigned, send out high byte
    OCR1BL = data_buffer;                   // send out low byte

    // get ADC data
    ADC_low = ADCL; // you need to fetch the low byte first
    ADC_high = ADCH;
    // construct the input sumple summing the ADC low and high byte.
    input = ((ADC_high << 8) | ADC_low) + 0x8000; // make a signed 16b value

    fractional += speed;        // increment sinewave lookup counter
    if (fractional >= 0x0100) { // if its large enough to go to next sample
        fractional &= 0x00ff;   // round off
        location += 1;          // go to next location
        location &= 0x03ff;     // fast boundary wrap for 2^n boundaries
    }
    // fetch current sinewave value
    int amplitude = pgm_read_word_near(sinewave + location);
    amplitude += 0x8000; // convert to unsigned
    int output;
    MultiSU16X16toH16(output, input, amplitude);
    // save value for playback next interrupt
    data_buffer = output;
}

/*
Based on the 'stomp_updown" from openmusiclabs.com
this program plays through a sample buffer, first forward at
double rate, and then backwards at single rate.  it changes
direction at the buffer boundary.
*/
void upDown() {
    // output the last value calculated
    OCR1AL = ((data_bufferUD + 0x8000) >> 8); // convert to unsigned, send out high byte
    OCR1BL = data_bufferUD;                   // send out low byte

    // get ADC data
    byte temp1 = ADCL;                           // you need to fetch the low byte first
    byte temp2 = ADCH;                           // yes it needs to be done this way
    int input = ((temp2 << 8) | temp1) + 0x8000; // make a signed 16b value

    buffer[location] = input; // store current sample
    location++;               // go to next location
    if (location >= SIZE)
        location = 0;                      // deal with boundary
    unsigned int temp = location + offset; // find playback location
    if (temp >= SIZE)
        temp -= SIZE;             // boundary wrap
    data_bufferUD = buffer[temp]; // fetch sample
    if (dir) {                    // increment until at buffer boundary
        if (offset >= (SIZE - 4)) {
            dir = 0;
            offset--;
        } else
            offset++;
    } else { // decrement till reaching boundary from other side
        if (offset <= 4) {
            dir = 1;
            offset--;
        } else
            offset -= 2;
    }
}
