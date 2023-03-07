#include "MKL05Z4.h"
#include "LCD1602.h"
#include "frdm_bsp.h"
#include "i2c.h"
#include "leds.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "math.h"
#include "klaw.h"
#include "adc.h"

#define	ZYXDR_Mask	1<<3	// Maska bitu ZYXDR w rejestrze STATUS

static uint8_t arrayXYZ[6];
static uint8_t sens; //czulosc
static uint8_t status; //rejestr stanu
double X1, X2 , XP1, XP2;   //do wartosci przyspieszenia

volatile uint8_t S2_press=0;	
volatile uint8_t S3_press=0;

uint8_t wynik_ok=0;
uint16_t temp;
float	wynik;


uint32_t czas=0;				// Licznik czasu, zliczajacy sekundy od handlera
uint8_t sekunda=0;			// Licznik przerwan ( do 10)
uint8_t sekunda_OK=0;		// "1"oznacza, ze handler od SysTick zliczyl 10 przerwan, kazde po 0.1s, czyli jedna sekunde

void SysTick_Handler(void)	// Podprogram obslugi przerwania od SysTick'a
{ 
	sekunda+=1;				// Licz interwaly równe 100ms
	if(sekunda==10)
	{
		sekunda=0;
		sekunda_OK=1;		// Daj znac, ze minela sekunda
	}
}

void ADC0_IRQHandler()
{	
	temp = ADC0->R[0];	// Odczyt danej i skasowanie flagi COCO
	if(!wynik_ok)				// Sprawdz, czy wynik skonsumowany przez petle glówna
	{
		wynik = temp;			// Wyslij nowa dana do petli glównej
		wynik_ok=1;
	}
}

void PORTA_IRQHandler(void)	// Podprogram obslugi przerwania od klawiszy S2, S3 
{
	uint32_t buf;
	buf=PORTA->ISFR & (S2_MASK | S3_MASK);

	switch(buf)
	{
		case S2_MASK:	DELAY(10)
									if(!(PTA->PDIR&S2_MASK))		// Minimalizacja drgan zestyków
									{
										if(!(PTA->PDIR&S2_MASK))	// Minimalizacja drgan zestyków (c.d.)
										{
											if(!S2_press)
											{
												S2_press=1;
											}
										}
									}
									break;
		case S3_MASK:	DELAY(10)
									if(!(PTA->PDIR&S3_MASK))		// Minimalizacja drgan zestyków
									{
										if(!(PTA->PDIR&S3_MASK))	// Minimalizacja drgan zestyków (c.d.)
										{
											if(!S3_press)
											{
												S3_press=1;
											}
										}
									}
									break;
		default:			break;
	}	
	PORTA->ISFR |=  S2_MASK | S3_MASK ;	// Kasowanie wszystkich bitów ISF
	NVIC_ClearPendingIRQ(PORTA_IRQn);
}

int main(void){
	char display[]={0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
	uint8_t	kal_error;
	LED_Init();									//Inicjalizacja LED
	
	Klaw_Init();								// Inicjalizacja klawiatury
	Klaw_S2_4_Int();						// Klawisze S2 i S3 zglaszaja przerwanie
	
	LCD1602_Init();		 					// Inicjalizacja LCD
	LCD1602_Backlight(TRUE);  	// Wlaczenie podswietlenia
	
	kal_error=ADC_Init();				// Inicjalizacja i kalibracja przetwornika A/C
	if(kal_error)
	{	
		while(1);									// Klaibracja sie nie powiodla
	}
	
	ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(8);		// Pierwsze wyzwolenie przetwornika ADC0 w kanale 8 i odblokowanie przerwania
	
	SysTick_Config(SystemCoreClock/10 );	// Start licznika SysTick ( przerwanie co 100ms)
	
	LCD1602_ClearAll();					// Wyczysc ekran
	//Ekran startowy
	LCD1602_Print("Nacisnij S1");	
	LCD1602_SetCursor(0,1);
	LCD1602_Print("Aby zaczac");
	
	while(PTA->PDIR&S1_MASK);  	// Czy klawisz S1 wcisniety? (oczekiwanie na wcisniecie klawisza)
	
	LCD1602_ClearAll();					// Wyczysc ekran
	
	sens=0;	// Wybór czulosci: 0 - 2g
	I2C_WriteReg(0x1d, 0x2a, 0x0);	// ACTIVE=0 - stan czuwania
	I2C_WriteReg(0x1d, 0xe, sens);	 		// Ustaw czulosc zgodnie ze zmienna sens
	I2C_WriteReg(0x1d, 0x2a, 0x1);	 		// ACTIVE=1 - stan aktywny
	
	int wynik1 = 0; //wynik 1 gracz
	int wynik2 = 0; //wynik 2 gracza
	int r1,r2; // wartosci losowane na kostce
	int OK=0; //zmienna, która mówi czy chcemy rzucic ponownie czy nie
	int player=1; //ktory gracz wykonuje teraz rzut
	int losowanie=0; //losowa wartosc 
	
	while(1) {
		
		I2C_ReadReg(0x1d, 0x0, &status);
		status&=ZYXDR_Mask;
		if(status)	// Czy dane gotowe do odczytu?
		{
			while((wynik1 < 50) && (wynik2 < 50)){
				//pseudogenerator liczb 
				if(wynik_ok)
				{
					wynik = wynik*512315;
					wynik_ok=0;
				}
				if(player==1){
					OK=0;
					XP1=0;
					I2C_ReadRegBlock(0x1d, 0x1, 6, arrayXYZ);
					X1=((double)((int16_t)((arrayXYZ[0]<<8)|arrayXYZ[1])>>2)/(4096>>sens));
					
					LCD1602_SetCursor(0,0);
					LCD1602_Print("G1 - Rzuc kostka");	
		
					PTB->PCOR|=LED_R_MASK; //Gracz 1 - czerwony; Gracz 2 - zielony
					PTB->PSOR|=LED_G_MASK; 
					//Rzucamy w osi X
					if(fabs(X1)>1.5){
						//losowanie oczek
						losowanie = wynik*1299;
						r1 = losowanie %6 +1;
						losowanie = wynik*1523;
						r2 = losowanie %6 +1;
						
						LCD1602_ClearAll();
						LCD1602_SetCursor(0,0);
						sprintf(display,"Wylosowano %d i %d",r1,r2);
						LCD1602_Print(display);
						LCD1602_SetCursor(0,1);
						LCD1602_Print("Powt? S2-T S3-N"); //Pytamy czy chcemy rzucic ponownie
					
						while(OK<1){
							if(S2_press){	
								OK=1;
								S2_press=0;
							}
							if(S3_press){	
								OK=2;
								S3_press=0;
							}
						}
						if(OK == 1){
							LCD1602_ClearAll();
							LCD1602_SetCursor(0,0);
							LCD1602_Print("Rzuc ponownie");
							while(fabs(XP1) < 2){
								I2C_ReadRegBlock(0x1d, 0x1, 6, arrayXYZ);
								XP1=((double)((int16_t)((arrayXYZ[0]<<8)|arrayXYZ[1])>>2)/(4096>>sens));
							}
							losowanie = wynik*23452;
							r1 = losowanie%6 +1;
							losowanie = wynik*52135;
							r2 = losowanie%6 +1;
							LCD1602_ClearAll();
							sprintf(display,"Nowe cyfry %d i %d",r1,r2);
							LCD1602_Print(display);
						}
						if(OK==2){
							LCD1602_ClearAll();
							sprintf(display,"Wylosowano %d i %d",r1,r2);
							LCD1602_Print(display);
						}
						wynik1 += r1+r2;
						LCD1602_SetCursor(0,1);
						sprintf(display,"Wynik: %d",wynik1);
						LCD1602_Print(display);
						//wyswitlaj wynik przez 5 sekund
						while(czas<5)		
						{
							if(sekunda_OK)
							{
								czas+=1;			// Licz sekundy
								sekunda_OK=0;
							}
						}
						czas=0;
						LCD1602_ClearAll();
						player =2; //zmiana gracza
					}
					
				}
				if(player==2){
					OK=0;
					XP2=0;
					I2C_ReadRegBlock(0x1d, 0x1, 6, arrayXYZ);
					X2=((double)((int16_t)((arrayXYZ[0]<<8)|arrayXYZ[1])>>2)/(4096>>sens));
					
					LCD1602_SetCursor(0,0);
					LCD1602_Print("G2 - Rzuc kostka");
					
					PTB->PCOR|=LED_G_MASK; // Wlaczenie diody zielonej
					PTB->PSOR|=LED_R_MASK; // Wylacz inne
					
					if(fabs(X2)>1.5){
						//losowanie oczek
						losowanie = wynik*61251;
						r1 = losowanie%6 +1;
						losowanie = wynik*81418;
						r2 = losowanie%6 +1;
					  
						LCD1602_ClearAll();
						LCD1602_SetCursor(0,0);
						sprintf(display,"Wylosowano %d i %d",r1,r2);
						LCD1602_Print(display);
						LCD1602_SetCursor(0,1);
						LCD1602_Print("Powt? S2-T S3-N");
					
						while(OK<1){
							if(S2_press){	
								OK=1;
								S2_press=0;
							}
							if(S3_press){	
								OK=2;
								S3_press=0;
							}
						}
						if(OK == 1){
							LCD1602_ClearAll();
							LCD1602_SetCursor(0,0);
							LCD1602_Print("Rzuc ponownie");
							while(fabs(XP2) < 2){
								I2C_ReadRegBlock(0x1d, 0x1, 6, arrayXYZ);
								XP2=((double)((int16_t)((arrayXYZ[0]<<8)|arrayXYZ[1])>>2)/(4096>>sens));
							}
							losowanie = wynik*91512;
							r1 = losowanie%6 +1;
							losowanie = wynik*14123;
							r2 = losowanie%6 +1;
							LCD1602_ClearAll();
							sprintf(display,"Nowe cyfry %d i %d",r1,r2);
							LCD1602_Print(display);
						}
						if(OK==2){
							LCD1602_ClearAll();
							sprintf(display,"Wylosowano %d i %d",r1,r2);
							LCD1602_Print(display);							
						}
						wynik2 += r1+r2;
						LCD1602_SetCursor(0,1);
						sprintf(display,"Wynik: %d",wynik2);
						LCD1602_Print(display);
						//wyswietlaj wynik przez 5 sekund
						while(czas<5)		
						{
							if(sekunda_OK)
							{
								czas+=1;			// Licz sekundy
								sekunda_OK=0;
							}
						}
						czas=0;
						LCD1602_ClearAll();
						player =1;
					}
					
				}
			}
			//jezeli ktorys z graczy zdobedzie wymagana liczbe punktow to koniec gry
			PTB->PSOR|=(LED_G_MASK | LED_R_MASK); //wylaczenie diod
			///Pokazanie odpowiedniego komunikatu w zaleznosci od wyniku
			if(wynik1>wynik2){
				LCD1602_ClearAll();
				LCD1602_Print("Koniec gry!");
				LCD1602_SetCursor(0,1);
				LCD1602_Print("G1 Wygral!");
				while(czas<30)		
				{
					if(sekunda_OK)
					{
						czas+=1;			// Licz sekundy
						sekunda_OK=0;							}
					}
				czas=0;
				LCD1602_ClearAll();
				
				wynik1=0;
				wynik2=0;
				player=1;
			}
			else{
				LCD1602_ClearAll();
				LCD1602_Print("Koniec gry!");
				LCD1602_SetCursor(0,1);
				LCD1602_Print("G2 Wygral!");
				while(czas<30)		
				{
					if(sekunda_OK)
					{
						czas+=1;			// Licz sekundy
						sekunda_OK=0;							}
					}
				czas=0;
				LCD1602_ClearAll();
				
				wynik1=0;
				wynik2=0;
				player=1;
			}	
		}
		
	}
}
