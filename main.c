#include <MK60D10.H>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <cstddef>
#include <math.h>

void PIT1_IRQHandler(void); //PIT handler from lab 4
void PDB0_IRQHandler(void); //Programmable delay block handler
void ADC0_IRQHandler(void); //Analog to Dig handler
void TIMR_CONFIG (void); //set up NVIC values for timers (ADC, PIT, PDB)
void DAC_CONFIG (void); //set enables for Digital to Analog Convertor
void SET_PINS_GPIO(void); //set GPIO characteristics of pins
void PDB_INIT(void); //set enable/initial values for PDB0
void ADC_CONFIG (void); //set enable/initial values for ADC0
void INIT_CONFIG (void); //set gate clocks for ADC, PDB, DAC

uint32_t ADC_Val_Hex = 0;
uint32_t ADC_Val_Hex_Parsed = 0;
uint32_t counter = 0;
uint32_t ADC_Val_HexSum = 0;
uint32_t Hex_Val_Avg = 0;
uint32_t PIT_Count = 0;
uint32_t digits[3] = {0};

uint32_t BCD_Vals[20] =
{0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90,0x40,0x79,0x24,0x30,0x19,0x12,0x02,0x78,0x00
,0x10};

/****** Initial configuration for gate clocks ADC/DAC/PDB **********/
void INIT_CONFIG (void)
{
	SIM->SCGC6 = 0x08400000; //ADC & PDB clock gate control (clock enable)
	SIM->SCGC6 |= (SIM->SCGC6 | 1UL << 23);
	SIM->SCGC2 = 0x1000; //DAC clock gate control (clock enable)
	SIM->SOPT7 = 0x00; //System operations ->PDB external trigger pin input (PDB0_EXTRG)
}

/****** configuration for ADC **********/
void ADC_CONFIG (void)
{
	ADC0->CFG1 = 0x7D; ;//DIV ratio = 8, long sample time, single ended 16bit, bus_clock/2
	ADC0->CFG2 = 0x00; //ADxxa channels are selected
	ADC0->SC2 = 0x40; //Set to hardware trigger
	ADC0->SC3 = 0x00;
	//Continuous conversions or sets of conversions if the hardware average function is enabled
	//AVGE=1, after initiating a conversion. (and hardware average function IS enabled here)
	ADC0->SC1[0] = 0x4A;
	//interrupt enabled, ADC10 as single ended, input from primary connector B40 pin
}

/************* Initialize PDB *****************/
void PDB_INIT(void)
{
	PDB0->MOD = 0x1000; //(8UL << 4 ); period of PDB counter 
	PDB0->IDLY = 0x0;
	//(0UL << 0); specifies delay value to schedule the PDB
	PDB0->CH[0].C1 = 0x00000101;
	//0x00000101 //CH0 pretrigger selected and enabled
	PDB0->PODLY[0] =0x0101000;
	//(72 << 7 );//sets pulse output to 1 after 0x100 pulses and 0 after 0x1000 pulses
	PDB0->SC = 0xFA3; ;// set prescaler to 0 and MULT to 1
	//SWTRG - THIS ALSO resets and restarts counter
	PDB0->SC |= (1UL << 16); //start counter
}

/************* Initialize output Pins as GPIO *****************/
void SET_PINS_GPIO(void)
{
	int i;
	
	SIM->SCGC5 |= (1UL << 11 | 1UL << 9); //clock enable for PORTC & PORTA
	
	for (i=0; i<16; i++)
	{
		PORTC->PCR[i] = (1UL << 8);
		//config pins 0 - 10 as general GPIO
	}

	PTC->PDDR = (2047UL);
	PORTA->PCR[7] = (0UL << 0); //set this pin as analog input
}

/****** Initialization & Enables for DAC **********/
void DAC_CONFIG (void)
{
	DAC0->DAT[0].DATL = 0x00; //clear DAT0 values
	DAC0->DAT[0].DATH = 0x00; //clear SR
	DAC0->C0 = 0x00; //(0UL << 0);//clear C0
	DAC0->C1 = 0x00; //(0UL << 0);//clear C1
	DAC0->C2 = 0x0F; //(15UL << 4);//initilaize C2
	DAC0->C0 = 0xF0;
	//(240UL << 4);//set C0 value->start DAC0, software trigger, use DACREF_2
}

/****** wait **********/
void wait(uint32_t mSec) 
{
	uint32_t cpuConstant = (1/50000000);
	uint32_t counter = 0;
	uint32_t result = (mSec)/cpuConstant;
	while (counter <= result)
	{
		counter++;
	}
}

/****** configuration for interrupts **********/
void NVIC_CONFIG (void)
{
	NVIC_EnableIRQ(ADC0_IRQn); //enablePDB timer interrupt for ADC0_IRQ57 i think
	NVIC_EnableIRQ(PDB0_IRQn); // activate PDB0_IRQ72
	NVIC_EnableIRQ(PIT1_IRQn); //enable PIT timer interrupt handler
}

/****** configuration for PIT **********/
void PIT1_CONFIG (void)
{
	PIT->MCR = (0UL << 1); //Enable PIT timer
	PIT->CHANNEL[1].LDVAL = 50000000; //Load value for PIT timer
	PIT->CHANNEL[1].TCTRL = (3UL << 0); //Turn on interrupt and timer enable
	PIT->CHANNEL[1].TFLG = (1UL << 0); //Clear flag
}

/****** Interrupt Handler for ADC0 **********/
void ADC0_IRQHandler(void) 
{
	int volt;
	__disable_irq();
	ADC_Val_Hex = ADC0->R[0]; //read input value 16 bits
	ADC_Val_Hex_Parsed = ADC_Val_Hex;
	ADC_Val_Hex_Parsed >>= 4; //convert input value to 12 bits
	DAC0->DAT[0].DATL = ADC_Val_Hex_Parsed & 0XFF; //store 8 LSBs to DAC0 low region
	ADC_Val_Hex_Parsed >>= 8;
	DAC0->DAT[0].DATH = ADC_Val_Hex_Parsed & 0XF;
	//store half byte to DAC0 high region (MSB)
	ADC_Val_HexSum += ADC_Val_Hex; //take avg summation
	counter++;
	
	if (counter == 0x1000) //count to 4096 b/c resolution is 12 bit (2^12)
	{
		Hex_Val_Avg = ADC_Val_HexSum/0x1000; //once triggered does avg calculation
		volt = (Hex_Val_Avg * 330)/(pow(2,16)-1);
		//((ADC avg val * Volt reference ))/(Resolution) (SLIDE 14)!!
		digits[0] = volt % 10;
		digits[1] = (volt/10) % 10;
		digits[2] = (volt/100) %10;
		counter = 0;
		ADC_Val_HexSum = 0;
	}

	__enable_irq();
}

/****** Interupt Handler for PDB0 **********/
void PDB0_IRQHandler(void) 
{
	ADC0_IRQHandler();
	PDB0->SC &= 0xFFFFFFBF; //0xFFFFFFBF to clear PDB interrupt flag
	PDB0->CH[0].S = 0x0;
	//clearing & Sequence error not detected on PDB channel's corresponding pre-trigger
}

/****** Interrupt Handler for PIT1 **********/
void PIT1_IRQHandler(void) 
{
	PIT->CHANNEL[1].TFLG = 0x1; //clear TIF flag
	PIT_Count++;
}

int main (void) 
{
	/* --------------- DECLARE CONSTS ---------- */
	int zero = 192UL;
	int one = 249UL;
	int two = 164UL;
	int three = 176UL;
	int four = 153UL;
	int five = 146UL;
	int six = 130UL;
	int seven = 120UL;
	int eight = 128UL;
	int nine = 152UL;
	int a = 136UL;
	int b = 131UL;
	int c = 198UL;
	int d = 161Ul;
	int e = 134UL;
	int f = 142UL;
	int i=0;
	int LD3 = 1024UL;
	int LD2 = 512UL;
	int LD1 = 256UL;
	int dot= 128UL;
	SET_PINS_GPIO();
	INIT_CONFIG();
	PDB_INIT();
	ADC_CONFIG();
	DAC_CONFIG();
	//TIMR_CONFIG();
	PIT1_CONFIG();
	//NVIC_EnableIRQ(ADC0_IRQn); //enablePDB timer
	//interrupt
	//NVIC->ISER[1] = (33554432UL << 26); // puts a 1 in bit pos 26 for ADC0
	//interrupt #57
	//NVIC_EnableIRQ(PDB0_IRQn); // activate PDB0_IRQ72
	//NVIC->ISER[2] = (8UL << 8); // set bit 8 in NVIC_ISER2 for PDB0
	//interrupt IRQ#72

	//controlling 7 segment display
	while (1)
	{
		//wait(1);
		PTC->PDOR = 0x00; //clear
		PTC->PDOR = 0x100;
		PTC->PDOR |= BCD_Vals[digits[2]+10]; //equation used to send HEX to LCD
		//wait(1);
		PTC->PDOR = 0x00; //clear
		PTC->PDOR = 0x200;
		PTC->PDOR |= BCD_Vals[digits[1]]; //equation used to send HEX to LCD
		//wait(1);
		PTC->PDOR = 0x00; //clear
		PTC->PDOR = 0x400;
		PTC->PDOR |= BCD_Vals[digits[0]]; //equation used to send HEX to LCD
	}
	return 0;
}