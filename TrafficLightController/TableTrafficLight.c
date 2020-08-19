// ***** 0. Documentation Section *****
// TableTrafficLight.c for Lab 10
// Runs on LM4F120/TM4C123
// Index implementation of a Moore finite state machine to operate a traffic light.  
// Daniel Valvano, Jonathan Valvano
// January 15, 2016

// east/west red light connected to PB5
// east/west yellow light connected to PB4
// east/west green light connected to PB3
// north/south facing red light connected to PB2
// north/south facing yellow light connected to PB1
// north/south facing green light connected to PB0
// pedestrian detector connected to PE2 (1=pedestrian present)
// north/south car detector connected to PE1 (1=car present)
// east/west car detector connected to PE0 (1=car present)
// "walk" light connected to PF3 (built-in green LED)
// "don't walk" light connected to PF1 (built-in red LED)

	// PB5-0 light outputs
	// PF3 walk (green), PF1 don't walk (red)	
	
	// PE2-0 sensor inputs
	// PE0 west road, PE1 south road, PE2 walk/pedestrian

// ***** 1. Pre-processor Directives Section *****
#include "TExaS.h"
#include "tm4c123gh6pm.h"

// ***** 2. Global Declarations Section *****
#define SENSOR  (GPIO_PORTE_DATA_R & 0x00000007)  // PE2-0
#define LIGHT   (*((volatile unsigned long *)0x400050FC))
#define WALK_LIGHT (*((volatile unsigned long *)0x400253FC))

struct State {
	unsigned long Out;
	unsigned long WalkOut;
	unsigned long Time;     // delay in 10ms units
	unsigned long Next[8];  // next state, 8 input types
};
typedef const struct State STyp;
unsigned long S;  // index of current state
unsigned long Input;

#define GO_WEST     0
#define WAIT_WEST   1
#define GO_SOUTH    2
#define WAIT_SOUTH  3
#define GO_WALK     4
#define FLASH_ON1   5
#define FLASH_OFF1  6
#define FLASH_ON2   7
#define FLASH_OFF2  8
#define FLASH_ON3   9
#define FLASH_OFF3 10
#define NUM_STATES 11

// 1001 = 0x09
// 0001 = 0x02 // red
// 1000 = 0x08 // green

// {6-LED-output, PF3-output, time, {states} }
STyp FSM[NUM_STATES] = {
	// 0) GO_WEST
	{0x0C, 0x02, 300, {GO_WEST, GO_WEST, WAIT_WEST, WAIT_WEST, WAIT_WEST, WAIT_WEST, WAIT_WEST, WAIT_WEST} },	
	
	// 1) WAIT_WEST
  {0x14, 0x02, 500, {GO_SOUTH, GO_SOUTH, GO_SOUTH, GO_SOUTH, GO_WALK, GO_WALK, GO_SOUTH, GO_SOUTH} },
	
  // 2) GO_SOUTH
  {0x21, 0x02, 300, {GO_SOUTH, WAIT_SOUTH, GO_SOUTH, WAIT_SOUTH, WAIT_SOUTH, WAIT_SOUTH, WAIT_SOUTH, WAIT_SOUTH} },
	
	// 3) WAIT_SOUTH
  {0x22, 0x02, 500, {GO_WEST, GO_WEST, GO_WEST, GO_WEST, GO_WALK, GO_WEST, GO_WALK, GO_WALK} },
	
	// 4) GO_WALK
	{0x24, 0x08, 200, {GO_WALK, FLASH_ON1, FLASH_ON1, FLASH_ON1, GO_WALK, FLASH_ON1, FLASH_ON1, FLASH_ON1} },
	
	// 5) FLASH_ON1
	{0x24, 0x02, 50, {FLASH_OFF1, FLASH_OFF1, FLASH_OFF1, FLASH_OFF1, FLASH_OFF1, FLASH_OFF1, FLASH_OFF1, FLASH_OFF1} }, // flash off-on 3x
	
  // 6) FLASH_OFF1
  {0x24, 0x00, 50, {FLASH_ON2, FLASH_ON2, FLASH_ON2, FLASH_ON2, FLASH_ON2, FLASH_ON2, FLASH_ON2, FLASH_ON2} },
	
  // 7) FLASH_ON2
  {0x24, 0x02, 50, {FLASH_OFF2, FLASH_OFF2, FLASH_OFF2,	FLASH_OFF2, FLASH_OFF2,	FLASH_OFF2, FLASH_OFF2, FLASH_OFF2} },
	
  // 8) FLASH_OFF2
  {0x24, 0x00, 50, {FLASH_ON3, FLASH_ON3, FLASH_ON3, FLASH_ON3, FLASH_ON3, FLASH_ON3, FLASH_ON3, FLASH_ON3} },
	
  // 9) FLASH_ON3
  {0x24, 0x02, 50, {FLASH_OFF3, FLASH_OFF3, FLASH_OFF3, FLASH_OFF3, FLASH_OFF3, FLASH_OFF3, FLASH_OFF3, FLASH_OFF3} },
	
	// 10) FLASH_OFF3
  {0x24, 0x00, 50, {GO_WEST, GO_WEST, GO_SOUTH, GO_WEST, GO_WALK, GO_WEST, GO_SOUTH, GO_WEST} }
};

// FUNCTION PROTOTYPES: Each subroutine defined
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
void PLL_Init(void);
void SysTick_Init(void);
void SysTick_Wait10ms(unsigned long delay);
void Port_Init(void);

// ***** 3. Subroutines Section *****

int main(void){  
  TExaS_Init(SW_PIN_PE210, LED_PIN_PB543210, ScopeOff); // activate grader and set system clock to 80 MHz
  
	PLL_Init();  // Activating the PLL, Low Power Design 80MHz
	SysTick_Init();
	Port_Init();
	
	S = GO_WEST;
  
  EnableInterrupts();
	
  while(1){
		LIGHT = FSM[S].Out;
		WALK_LIGHT = FSM[S].WalkOut;
    SysTick_Wait10ms(FSM[S].Time);
    Input = SENSOR;
		S = FSM[S].Next[Input];
  }
}
void Port_Init(void) {
	// initialize ports
	volatile unsigned long delay;
	SYSCTL_RCGC2_R |= 0x32;  // 1) active clock for ports B,E,F
	delay = SYSCTL_RCGC2_R;  // 2) delay
	// PORT E
	GPIO_PORTE_AMSEL_R &= ~0x07; // 3) disable analog function on PE1-0
  GPIO_PORTE_PCTL_R &= ~0x000000FF; // 4) enable regular GPIO
  GPIO_PORTE_DIR_R &= ~0x07;   // 5) inputs on PE1-0
  GPIO_PORTE_AFSEL_R &= ~0x07; // 6) regular function on PE1-0
  GPIO_PORTE_DEN_R |= 0x07;    // 7) enable digital on PE1-0
	// PORT B
  GPIO_PORTB_AMSEL_R &= ~0x3F; // 3) disable analog function on PB5-0
  GPIO_PORTB_PCTL_R &= ~0x00FFFFFF; // 4) enable regular GPIO
  GPIO_PORTB_DIR_R |= 0x3F;    // 5) outputs on PB5-0
  GPIO_PORTB_AFSEL_R &= ~0x3F; // 6) regular function on PB5-0
  GPIO_PORTB_DEN_R |= 0x3F;    // 7) enable digital on PB5-0
	// PORT F
	//GPIO_PORTF_AMSEL_R &= ~0x05;  // disable analog on PF1 and PF3
	GPIO_PORTF_PCTL_R &= ~0x00FFFFFF;  // enable regular GPIO
	GPIO_PORTF_DIR_R |= 0x0A; // make outputs on PF1 and PF3
	GPIO_PORTF_AFSEL_R &= ~0x3F; // 6) regular function on PB5-0
  GPIO_PORTF_DEN_R |= 0x3F;
}

// Use of SysTick to delay for a specified amount of time (C10_SysTick_Wait).
#define NVIC_ST_CTRL_R      (*((volatile unsigned long *)0xE000E010))
#define NVIC_ST_RELOAD_R    (*((volatile unsigned long *)0xE000E014))
#define NVIC_ST_CURRENT_R   (*((volatile unsigned long *)0xE000E018))

void SysTick_Init(void){
  NVIC_ST_CTRL_R = 0;               // disable SysTick during setup
  NVIC_ST_CTRL_R = 0x00000005;      // enable SysTick with core clock
}
// The delay parameter is in units of the 80 MHz core clock. (12.5 ns)
void SysTick_Wait(unsigned long delay){
  NVIC_ST_RELOAD_R = delay-1;  // number of counts to wait
  NVIC_ST_CURRENT_R = 0;       // any value written to CURRENT clears
  while((NVIC_ST_CTRL_R&0x00010000)==0){ // wait for count flag
  }
}
// 800000*12.5ns equals 10ms
void SysTick_Wait10ms(unsigned long delay){
  unsigned long i;
  for(i=0; i<delay; i++){
    SysTick_Wait(800000);  // wait 10ms
  }
}
