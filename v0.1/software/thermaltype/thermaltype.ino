#include <SPI.h>
#include <FastGPIO.h>

// ============================
// User configurations

// Default font size is too small to read.
// Set scale to 2 for normal use. Set to 3 for demonstration.
constexpr int TEXT_SCALE = 2;

// This is the margin between actual heating points and printer case.
// Increase this value if you use your own case.
constexpr int FEED_MARGIN = 40;

// Half-step cycle of step motor, in microseconds.
// May need to change this for different printheads.
constexpr unsigned int MOTOR_HALF_STEP_TIME = 500;


// ============================
// Hardware configurations

// Pins
#define MOT1_PIN        FastGPIO::Pin<10>
#define MOT2_PIN        FastGPIO::Pin<9>
#define MOT3_PIN        FastGPIO::Pin<6>
#define MOT4_PIN        FastGPIO::Pin<5>
#define PRNPOWER_PIN    FastGPIO::Pin<4>
#define LATCH_PIN       FastGPIO::Pin<7>
#define STROBE_PIN      FastGPIO::Pin<8>
#define PAPERSNS_PIN    FastGPIO::Pin<12>
#define PRNTEMP_PIN     A0

// IO operations
#define PRNPOWER_L  PRNPOWER_PIN::setOutputValueLow()
#define PRNPOWER_H  PRNPOWER_PIN::setOutputValueHigh()
#define LATCH_L     LATCH_PIN::setOutputValueLow()
#define LATCH_H     LATCH_PIN::setOutputValueHigh()
#define STROBE_L    STROBE_PIN::setOutputValueLow()
#define STROBE_H    STROBE_PIN::setOutputValueHigh()


#define ARRAY_SIZE(x)  (sizeof(x) / sizeof((x)[0]))


// ============================
// Step motor driver

// Step motor phase signals, half-step method
static constexpr uint8_t MOTOR_PHASES[][4] = {
    {1, 0, 0, 0},
    {1, 0, 0, 1},
    {0, 0, 0, 1},
    {0, 1, 0, 1},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {1, 0, 1, 0},
};

class StepMotor
{
public:
    StepMotor() {
        MOT1_PIN::setOutputLow();
        MOT2_PIN::setOutputLow();
        MOT3_PIN::setOutputLow();
        MOT4_PIN::setOutputLow();
    }

public:
    // Idle state consumes no power
    void idle() {
        if (isIdle) {
            return;
        }

        const uint8_t IDLE_PHASES[] = {0, 0, 0, 0};
        setPhases(IDLE_PHASES);
        isIdle = true;
    }

    // One half-step forward or backward
    void step(bool forward = true) {
        isIdle = false;

        const int totalSteps = ARRAY_SIZE(MOTOR_PHASES);
        currStep = (currStep + totalSteps + (forward ? 1 : -1)) % totalSteps;
        setPhases(MOTOR_PHASES[currStep]);
    }

private:
    void setPhases(const uint8_t values[4]) {
        MOT1_PIN::setOutputValue(values[0]);
        MOT2_PIN::setOutputValue(values[1]);
        MOT3_PIN::setOutputValue(values[2]);
        MOT4_PIN::setOutputValue(values[3]);
    }

private:
    int currStep = -1;
    bool isIdle = true;
};


// ============================
// Printing

// First character value in table
constexpr char CHAR_BEGIN = 32;
// This is a fixed-width font
constexpr int CHAR_WIDTH  = 8;
constexpr int CHAR_HEIGHT = 16;

// 2 inch thermal printhead, 384 points
constexpr int ROW_WIDTH = 384;

// Font raster images. Each character is 16 bytes
static constexpr uint8_t CHAR_RASTERS[][CHAR_HEIGHT] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*" ",0*/
    {0x00,0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},/*"!",1*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*""",2*/
    {0x00,0x00,0x00,0x36,0x36,0x7F,0x36,0x36,0x36,0x7F,0x36,0x36,0x00,0x00,0x00,0x00},/*"#",3*/
    {0x00,0x18,0x18,0x3C,0x66,0x60,0x30,0x18,0x0C,0x06,0x66,0x3C,0x18,0x18,0x00,0x00},/*"$",4*/
    {0x00,0x00,0x70,0xD8,0xDA,0x76,0x0C,0x18,0x30,0x6E,0x5B,0x1B,0x0E,0x00,0x00,0x00},/*"%",5*/
    {0x00,0x00,0x00,0x38,0x6C,0x6C,0x38,0x60,0x6F,0x66,0x66,0x3B,0x00,0x00,0x00,0x00},/*"&",6*/
    {0x00,0x00,0x00,0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"'",7*/
    {0x00,0x00,0x00,0x0C,0x18,0x18,0x30,0x30,0x30,0x30,0x30,0x18,0x18,0x0C,0x00,0x00},/*"(",8*/
    {0x00,0x00,0x00,0x30,0x18,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x18,0x30,0x00,0x00},/*")",9*/
    {0x00,0x00,0x00,0x00,0x00,0x36,0x1C,0x7F,0x1C,0x36,0x00,0x00,0x00,0x00,0x00,0x00},/*"*",10*/
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00},/*"+",11*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x0C,0x18,0x00,0x00},/*",",12*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"-",13*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x00,0x00,0x00,0x00},/*".",14*/
    {0x00,0x00,0x00,0x06,0x06,0x0C,0x0C,0x18,0x18,0x30,0x30,0x60,0x60,0x00,0x00,0x00},/*"/",15*/
    {0x00,0x00,0x00,0x1E,0x33,0x37,0x37,0x33,0x3B,0x3B,0x33,0x1E,0x00,0x00,0x00,0x00},/*"0",16*/
    {0x00,0x00,0x00,0x0C,0x1C,0x7C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x00,0x00,0x00,0x00},/*"1",17*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00,0x00,0x00,0x00},/*"2",18*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x06,0x1C,0x06,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"3",19*/
    {0x00,0x00,0x00,0x30,0x30,0x36,0x36,0x36,0x66,0x7F,0x06,0x06,0x00,0x00,0x00,0x00},/*"4",20*/
    {0x00,0x00,0x00,0x7E,0x60,0x60,0x60,0x7C,0x06,0x06,0x0C,0x78,0x00,0x00,0x00,0x00},/*"5",21*/
    {0x00,0x00,0x00,0x1C,0x18,0x30,0x7C,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"6",22*/
    {0x00,0x00,0x00,0x7E,0x06,0x0C,0x0C,0x18,0x18,0x30,0x30,0x30,0x00,0x00,0x00,0x00},/*"7",23*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x76,0x3C,0x6E,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"8",24*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x3E,0x0C,0x18,0x38,0x00,0x00,0x00,0x00},/*"9",25*/
    {0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x00,0x00,0x00,0x1C,0x1C,0x00,0x00,0x00,0x00},/*":",26*/
    {0x00,0x00,0x00,0x00,0x00,0x1C,0x1C,0x00,0x00,0x00,0x1C,0x1C,0x0C,0x18,0x00,0x00},/*";",27*/
    {0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00},/*"<",28*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"=",29*/
    {0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00},/*">",30*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x0C,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00},/*"?",31*/
    {0x00,0x00,0x00,0x7E,0xC3,0xC3,0xCF,0xDB,0xDB,0xCF,0xC0,0x7F,0x00,0x00,0x00,0x00},/*"@",32*/
    {0x00,0x00,0x00,0x18,0x3C,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"A",33*/
    {0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},/*"B",34*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x60,0x60,0x60,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"C",35*/
    {0x00,0x00,0x00,0x78,0x6C,0x66,0x66,0x66,0x66,0x66,0x6C,0x78,0x00,0x00,0x00,0x00},/*"D",36*/
    {0x00,0x00,0x00,0x7E,0x60,0x60,0x60,0x7C,0x60,0x60,0x60,0x7E,0x00,0x00,0x00,0x00},/*"E",37*/
    {0x00,0x00,0x00,0x7E,0x60,0x60,0x60,0x7C,0x60,0x60,0x60,0x60,0x00,0x00,0x00,0x00},/*"F",38*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x60,0x60,0x6E,0x66,0x66,0x3E,0x00,0x00,0x00,0x00},/*"G",39*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"H",40*/
    {0x00,0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00},/*"I",41*/
    {0x00,0x00,0x00,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"J",42*/
    {0x00,0x00,0x00,0x66,0x66,0x6C,0x6C,0x78,0x6C,0x6C,0x66,0x66,0x00,0x00,0x00,0x00},/*"K",43*/
    {0x00,0x00,0x00,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00,0x00,0x00,0x00},/*"L",44*/
    {0x00,0x00,0x00,0x63,0x63,0x77,0x6B,0x6B,0x6B,0x63,0x63,0x63,0x00,0x00,0x00,0x00},/*"M",45*/
    {0x00,0x00,0x00,0x63,0x63,0x73,0x7B,0x6F,0x67,0x63,0x63,0x63,0x00,0x00,0x00,0x00},/*"N",46*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"O",47*/
    {0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0x00,0x00,0x00,0x00},/*"P",48*/
    {0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x0C,0x06,0x00,0x00},/*"Q",49*/
    {0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"R",50*/
    {0x00,0x00,0x00,0x3C,0x66,0x60,0x30,0x18,0x0C,0x06,0x66,0x3C,0x00,0x00,0x00,0x00},/*"S",51*/
    {0x00,0x00,0x00,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},/*"T",52*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"U",53*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0x00,0x00},/*"V",54*/
    {0x00,0x00,0x00,0x63,0x63,0x63,0x6B,0x6B,0x6B,0x36,0x36,0x36,0x00,0x00,0x00,0x00},/*"W",55*/
    {0x00,0x00,0x00,0x66,0x66,0x34,0x18,0x18,0x2C,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"X",56*/
    {0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00},/*"Y",57*/
    {0x00,0x00,0x00,0x7E,0x06,0x06,0x0C,0x18,0x30,0x60,0x60,0x7E,0x00,0x00,0x00,0x00},/*"Z",58*/
    {0x00,0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00},/*"[",59*/
    {0x00,0x00,0x00,0x60,0x60,0x30,0x30,0x18,0x18,0x0C,0x0C,0x06,0x06,0x00,0x00,0x00},/*"\",60*/
    {0x00,0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00},/*"]",61*/
    {0x00,0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"^",62*/
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00},/*"_",63*/
    {0x00,0x38,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"`",64*/
    {0x00,0x00,0x00,0x00,0x00,0x3C,0x06,0x06,0x3E,0x66,0x66,0x3E,0x00,0x00,0x00,0x00},/*"a",65*/
    {0x00,0x00,0x00,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00},/*"b",66*/
    {0x00,0x00,0x00,0x00,0x00,0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00,0x00,0x00,0x00},/*"c",67*/
    {0x00,0x00,0x00,0x06,0x06,0x3E,0x66,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x00,0x00},/*"d",68*/
    {0x00,0x00,0x00,0x00,0x00,0x3C,0x66,0x66,0x7E,0x60,0x60,0x3C,0x00,0x00,0x00,0x00},/*"e",69*/
    {0x00,0x00,0x00,0x1E,0x30,0x30,0x30,0x7E,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00},/*"f",70*/
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x66,0x66,0x66,0x66,0x66,0x3E,0x06,0x06,0x7C,0x00},/*"g",71*/
    {0x00,0x00,0x00,0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"h",72*/
    {0x00,0x00,0x18,0x18,0x00,0x78,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},/*"i",73*/
    {0x00,0x00,0x0C,0x0C,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x78,0x00},/*"j",74*/
    {0x00,0x00,0x00,0x60,0x60,0x66,0x66,0x6C,0x78,0x6C,0x66,0x66,0x00,0x00,0x00,0x00},/*"k",75*/
    {0x00,0x00,0x00,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00},/*"l",76*/
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x6B,0x6B,0x6B,0x6B,0x6B,0x63,0x00,0x00,0x00,0x00},/*"m",77*/
    {0x00,0x00,0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00},/*"n",78*/
    {0x00,0x00,0x00,0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00},/*"o",79*/
    {0x00,0x00,0x00,0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},/*"p",80*/
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x66,0x66,0x66,0x66,0x66,0x3E,0x06,0x06,0x06,0x00},/*"q",81*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x6E,0x70,0x60,0x60,0x60,0x60,0x00,0x00,0x00,0x00},/*"r",82*/
    {0x00,0x00,0x00,0x00,0x00,0x3E,0x60,0x60,0x3C,0x06,0x06,0x7C,0x00,0x00,0x00,0x00},/*"s",83*/
    {0x00,0x00,0x00,0x30,0x30,0x7E,0x30,0x30,0x30,0x30,0x30,0x1E,0x00,0x00,0x00,0x00},/*"t",84*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x3E,0x00,0x00,0x00,0x00},/*"u",85*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00,0x00,0x00,0x00},/*"v",86*/
    {0x00,0x00,0x00,0x00,0x00,0x63,0x6B,0x6B,0x6B,0x6B,0x36,0x36,0x00,0x00,0x00,0x00},/*"w",87*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00,0x00,0x00,0x00},/*"x",88*/
    {0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x0C,0x18,0xF0,0x00},/*"y",89*/
    {0x00,0x00,0x00,0x00,0x00,0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00,0x00,0x00,0x00},/*"z",90*/
    {0x00,0x00,0x00,0x0C,0x18,0x18,0x18,0x30,0x60,0x30,0x18,0x18,0x18,0x0C,0x00,0x00},/*"{",91*/
    {0x00,0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},/*"|",92*/
    {0x00,0x00,0x00,0x30,0x18,0x18,0x18,0x0C,0x06,0x0C,0x18,0x18,0x18,0x30,0x00,0x00},/*"}",93*/
    {0x00,0x00,0x00,0x71,0xDB,0x8E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},/*"~",94*/
};

// Backspace glyph (dotted block)
constexpr char BS_CHAR_CODE = 0x08;
static constexpr uint8_t BS_CHAR_RASTER[CHAR_HEIGHT] = {
    0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,/*backspace*/
};


// Text layout format
struct TextFormat
{
    uint8_t scale = 1;
    enum : uint8_t {
        AlignLeft,
        AlignMiddle,
        AlignRight,
    } align = AlignLeft;
};


// Thermal printhead driver
class ThermalPrinter
{
public:
    ThermalPrinter() {
        // use SPI to send print data
        SPI.begin();

        PRNPOWER_PIN::setOutputLow();
        LATCH_PIN::setOutputHigh();
        STROBE_PIN::setOutputLow();
        PAPERSNS_PIN::setInput();

        reset();
    }

public:
    // Heating power on/off
    void powerOn() {
        PRNPOWER_H;
    }

    void powerOff() {
        PRNPOWER_L;
    }

    // Idle the step motor to reduce power consumption
    void idle() {
        motor.idle();
    }

    // Whether paper is loaded
    bool isPaperReady() {
        return !PAPERSNS_PIN::isInputHigh();
    }

    // Stop heating and idle the step motor
    void reset() {
        // according to datasheet, it's good to clear printing data after powered on
        STROBE_L;

        LATCH_L;
        for (int i = 0; i < ROW_WIDTH / 8; i++) {
            SPI.transfer(0);
        }
        LATCH_H;

        motor.idle();
    }

    // Feed paper for `count` lines, may be positive or negative
    void feedLine(int count) {
        if (!isPaperReady() || count == 0) {
            return;
        }

        const int stepsPerLine = ARRAY_SIZE(MOTOR_PHASES) / 2;
        for (int i = 0; i < stepsPerLine * abs(count); i++) {
            motor.step(count > 0);
            delayMicroseconds(MOTOR_HALF_STEP_TIME);
        }
    }

    // Print one line of pixels, 1bpp data format (1 is black).
    // After calling this, `data` will be cleared to 0's
    void printLine1b(uint8_t* data) {
        if (!isPaperReady()) {
            return;
        }
        
        beginPrintLine(data);
        feedLine(1);
        endPrintLine();
    }

    // Print a line of text with specified format.
    // Long text will be clipped
    void printText(const char* text, TextFormat format = {}) {
        if (!isPaperReady() || !text) {
            return;
        }

        // cut long text
        int lineChars = ROW_WIDTH / CHAR_WIDTH / format.scale;
        lineChars = min(lineChars, (int)strlen(text));
        if (lineChars < 1) {
            return;
        }

        // convert to raster image indexes.
        // unknown characters are replaced with '?'
        const uint8_t* rasters[lineChars];
        for (int i = 0; i < lineChars; i++) {
            int c = text[i];
            if (c == BS_CHAR_CODE) {
                rasters[i] = BS_CHAR_RASTER;
                continue;
            }

            c -= CHAR_BEGIN;
            if (c < 0 || c >= (int)ARRAY_SIZE(CHAR_RASTERS)) {
                c = '?' - CHAR_BEGIN;
            }
            rasters[i] = CHAR_RASTERS[c];
        }

        // alignment
        int startX;
        int paddingX = ROW_WIDTH - lineChars * CHAR_WIDTH * format.scale;
        switch (format.align) {
            case TextFormat::AlignMiddle:
                startX = paddingX / 2;
                break;
            case TextFormat::AlignRight:
                startX = paddingX;
                break;
            default:
                startX = 0;
                break;
        }

        // rasterize row by row
        uint8_t rowBitData[ROW_WIDTH / 8];
        for (int y = 0; y < CHAR_HEIGHT; y++) {
            memset(rowBitData, 0, sizeof(rowBitData));

            int x = startX;
            for (int chIndex = 0; chIndex < lineChars; chIndex++) {
                // simply copy and place images, 1bpp format
                uint8_t raster = rasters[chIndex][y];
                for (int i = 0; i < 8; i++) {
                    if ((raster & (1 << (7 - i))) == 0) {
                        x += format.scale;
                        continue;
                    }

                    // scale in X direction
                    for (int j = 0; j < format.scale; j++) {
                        uint8_t& b = rowBitData[x / 8];
                        b |= (1 << (7 - x % 8));
                        x++;
                    }
                }
            }

            // print multiply times to scale in Y direction
            uint8_t rowCopy[sizeof(rowBitData)];
            for (int i = 0; i < format.scale; i++) {
                memcpy(rowCopy, rowBitData, sizeof(rowBitData));
                printLine1b(rowCopy);
            }
        }
    }

private:
    // send 1bpp data to printhead and begin heating
    void beginPrintLine(uint8_t* data) {
        if (!isPaperReady()) {
            return;
        }

        STROBE_L;
        LATCH_L;
        SPI.transfer(data, ROW_WIDTH / 8);
        LATCH_H;
        STROBE_H;
    }

    // stop heating
    void endPrintLine() {
        STROBE_L;
    }

private:
    StepMotor motor;
};


// ============================
// Typewriter

class Typewriter
{
public:
    Typewriter() {}

public:
    void init() {
        // connect to PC USB serial
        Serial.begin(115200);

        printer.powerOn();
        printer.feedLine(50);

        format.scale = TEXT_SCALE;
    }

    void loop() {
        while (newPos < ROW_CHARS) {
            int c = Serial.read();
            if (c < 0) {
                break;
            }

            // force break line on receiving Enter
            if (c == '\n') {
                flush();
                newLine();
                break;
            }

            // ^H = backspace
            if (c == BS_CHAR_CODE) {
                flush();
                backspace();
                break;
            }

            // buffer valid characters
            if (c >= CHAR_BEGIN && c <= CHAR_BEGIN + ARRAY_SIZE(CHAR_RASTERS)) {
                textBuf[newPos++] = c;
            }
        }

        flush();
        
        printer.idle();  // idle when possible to save power
    }

private:
    void newLine() {
        lastPos = newPos = 0;
        bsPos = lastPos - 1;
        bsLine = 0;
        printer.feedLine(SCALED_CHAR_HEIGHT);
    }

    // print a block to cover previous character
    void backspace() {
        if (bsPos < 0) {
            // goto previous line end
            bsLine++;
            bsPos = ROW_CHARS - 1;
        }

        memset(textBuf, ' ', bsPos);
        textBuf[bsPos] = BS_CHAR_CODE;
        textBuf[bsPos + 1] = '\0';

        // roll back current line to heating points
        printer.feedLine(-FEED_MARGIN - SCALED_CHAR_HEIGHT * (bsLine + 1));
        printer.printText(textBuf, format);
        //FIXME: 1-off bug, don't know why but this works
        printer.feedLine(FEED_MARGIN - 1 + (SCALED_CHAR_HEIGHT - 1) * bsLine);

        textBuf[bsPos] = textBuf[bsPos + 1] = ' ';
        bsPos--;
    }

    // print out buffered text 
    void flush() {
        if (newPos == lastPos) {
            return;
        }

        // roll back current line to heating points
        printer.feedLine(-FEED_MARGIN - SCALED_CHAR_HEIGHT);
        textBuf[newPos] = '\0';
        printer.printText(textBuf, format);
        //FIXME: 1-off bug, don't know why but this works
        printer.feedLine(FEED_MARGIN - 1);

        if (newPos < ROW_CHARS) {
            // clear printed characters
            memset(textBuf + lastPos, ' ', newPos - lastPos);
            lastPos = newPos;
            bsPos = lastPos - 1;
            bsLine = 0;
        } else {
            // line full
            newLine();
        }
    }

private:
    constexpr static int SCALED_CHAR_WIDTH  = CHAR_WIDTH * TEXT_SCALE;
    constexpr static int SCALED_CHAR_HEIGHT = CHAR_HEIGHT * TEXT_SCALE;
    constexpr static int ROW_CHARS = ROW_WIDTH / SCALED_CHAR_WIDTH;

    ThermalPrinter printer;
    TextFormat format;
    char textBuf[ROW_CHARS + 1];    // input text buffer

    int lastPos = 0;                // last printed position
    int newPos = 0;                 // valid character position

    int bsPos = -1;                 // backspace position
    int bsLine = 0;                 // backspace to previous lines
};


// ============================
// main
// 

Typewriter typewriter;

void setup()
{
    typewriter.init();
}

void loop()
{
    typewriter.loop();
}
