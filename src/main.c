/*****************************************************************************
 * \brief		Startup file. In this file is the main function, which is
 *				called at the startup of the controller.
 *
 * \file		main.c
 * \version		1.0
 * \date		2012-12-12
 * \author		fue1
 ****************************************************************************/
/*
 * functions global:
 *				main
 * functions local:
 *				GreenLEDtask
 *				IOtask
 *
 ****************************************************************************/

/*----- Header-Files --------------------- ---------------------------------*/
#include <stdio.h>					/* Standard input and output			*/
#include <stdlib.h>					/* Standard library        		        */

#include <stm32f4xx.h>				/* Processor STM32F407IG				*/
#include <carme.h>					/* CARME Module							*/
#include <uart.h>					/* CARME BSP UART port					*/
#include <can.h>					/* CARME BSP CAN  port					*/
#include <lcd.h>
#include <carme_io1.h>
#include <carme_io2.h>
#include <main.h>


#include <FreeRTOS.h>				/* FreeRTOS								*/
#include <FreeRTOS.h>				/* FreeRTOS								*/
#include <task.h>					/* FreeRTOS tasks						*/
#include <queue.h>					/* FreeRTOS queues						*/
#include <semphr.h>					/* FreeRTOS semaphores					*/
#include <memPoolService.h>			/* Memory pool manager service			*/
#include <messages.h>

/*----- Macros -------------------------------------------------------------*/
#define Pfad_Links 1
#define Pfad_Rechts 2

#define Lok_Init 9
#define Lok_Fehler 10
#define Lok_Roboter_Links 3
#define Lok_Roboter_Rechts 4
#define Lok_Foederband_Links 5
#define Lok_Foederband_Rechts 6
#define Lok_Foederband_Mitte 7
#define Lok_Shifter 8

#define wahr 1
#define falsch 0
/*----- Data types ---------------------------------------------------------*/

/*----- Function prototypes ------------------------------------------------*/
void TestTask (void *);

void sendCanMessage    (void *);
void writeCanMessage   (void *);
void readCanMessageIRQ (void);

/*----- Data ---------------------------------------------------------------*/
xSemaphoreHandle Muxtex_Can_Tx;
xSemaphoreHandle Muxtex_Can_Rx;
xSemaphoreHandle Muxtex_Display;
xSemaphoreHandle Muxtex_Roboter_Links;
xSemaphoreHandle Muxtex_Roboter_Rechts;
xSemaphoreHandle Muxtex_Foederband_Links;
xSemaphoreHandle Muxtex_Foederband_Rechts;
xSemaphoreHandle Muxtex_Foederband_Mitte;
xSemaphoreHandle Muxtex_Shifter;
xQueueHandle     Queue_Can_Rx;

/* Pointer kann die Adresse für Parameter übergeben.*/
void *ETCS_Task1_Parameter;
void *ETCS_Task2_Parameter;
void *ETCS_Task3_Parameter;
char Task1Name = 1;
char Task2Name = 2;
char Task3Name = 3;

/* Task Pointer um auf die Adresse des Stacks referenzieren zu können.*/
typedef void *TaskHandle_t ;
TaskHandle_t xHandle_ETCSTask1 = NULL;
TaskHandle_t xHandle_ETCSTask2 = NULL;
TaskHandle_t xHandle_ETCSTask3 = NULL;

/*----- Implementation -----------------------------------------------------*/
/**
 * @brief		main
 * @return		0 if success
 */
int main(void)
{
	/* Ensure all priority bits are assigned as preemption priority bits. */
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	/* initialize the mutex variables */
	Muxtex_Can_Tx  				= xSemaphoreCreateMutex();
	Muxtex_Display 				= xSemaphoreCreateMutex();
	Muxtex_Roboter_Links 		= xSemaphoreCreateMutex();
	Muxtex_Roboter_Rechts 		= xSemaphoreCreateMutex();
	Muxtex_Foederband_Links 	= xSemaphoreCreateMutex();
	Muxtex_Foederband_Rechts 	= xSemaphoreCreateMutex();
	Muxtex_Foederband_Mitte 	= xSemaphoreCreateMutex();
	Muxtex_Shifter				= xSemaphoreCreateMutex();

	Queue_Can_Rx   = xQueueCreate(20, sizeof (CARME_CAN_MESSAGE *));

	/* BEGIN TEST */
	ETCS_Task1_Parameter = &Task1Name;
	ETCS_Task2_Parameter = &Task2Name;
	ETCS_Task3_Parameter = &Task3Name;
	/* END TeST */

	/* initialize the LCD display */
	LCD_Init();
	LCD_SetFont(&font_8x16);
	LCD_DisplayStringXY(10, 10, "Can Template CARME-M4");

	/* initialize the CAN interface */
	CARME_CAN_InitI(CARME_CAN_BAUD_250K, CARME_CAN_DF_RESET, CARME_CAN_INT_RX);
	CARME_CAN_RegisterIRQCallback(CARME_CAN_IRQID_RX_INTERRUPT, readCanMessageIRQ);
	CARME_CAN_SetMode(CARME_CAN_DF_NORMAL);

	/* create tasks */
	//xTaskCreate(sendCanMessage,  (const signed char * const)"Send Can Message",  1024, NULL, 4, NULL);
	//xTaskCreate(writeCanMessage, (const signed char * const)"Write Can Message", 1024, NULL, 4, NULL);
	xTaskCreate(TestTask, (const signed char * const)"HelloWorld_1", 1024, ETCS_Task1_Parameter, 3, xHandle_ETCSTask1);
	//xTaskCreate(TestTask, (const signed char * const)"HelloWorld_2", 1024, ETCS_Task2_Parameter, 3, xHandle_ETCSTask2);
	//xTaskCreate(TestTask, (const signed char * const)"HelloWorld_3", 1024, ETCS_Task3_Parameter, 3, xHandle_ETCSTask3);

	vTaskStartScheduler();

	for(;;){}

	return 0;
}

/**
 * @brief		task sendCanMessage
 */
void TestTask (void *pvData) {
	char u8_State = Lok_Foederband_Mitte;
	char u8_Pfad = Pfad_Links;
	char u8_Start_Lokalisation = Lok_Foederband_Mitte;  // Soll übergeben werden.
	char u8_Destination = 0;
	char u8_AutoPfadWechsel = 1;
	int  i16_Semaphor_Timer = 100;

	char i = 9;
	char counterli = 0;
	CARME_CAN_MESSAGE msg;
	char string[] = "BLA BLA";


	/* Übergabe der Werte logisch und Gültig */
	if((u8_Pfad == Pfad_Links) && ((u8_Lokalisation == Lok_Roboter_Rechts)||(u8_Lokalisation == Lok_Foederband_Rechts)))
	{/* Nix Gut */}
	else if ((u8_Pfad == Pfad_Rechts) && ((u8_Lokalisation == Lok_Roboter_Links)||(u8_Lokalisation == Lok_Foederband_Links)))
	{/* Nix Gut */}
	else
	{/* Alles Gut */ u8_State = Lok_Fehler;}

	while(1){
		i++;
		counterli++;
		////sprintf(string,"MyStackAdressIs%d", pvData);
		////sprintf(string,"Counterli:%d", counterli);
		//TestSemaphore = 100;

		// State Machine




		switch(u8_State)
			case Lok_Init:
				// Mögliche Startpositionen
				if () // Position Föderband Mitte
				{
					resultTake = xSemaphoreTake(Muxtex_Foederband_Mitte, i16_Semaphor_Timer); // Semaphore nehmen vom Föderband
					if(resultTake != 0)
					{

					}
					else
					{

					}
				}
				else if () // Position Föderband Rechts
				{
					resultTake = xSemaphoreTake(Muxtex_Foederband_Rechts , i16_Semaphor_Timer); // Semaphore nehmen vom Föderband
					if(resultTake != 0)
					{

					}
					else
					{
						// Display Föderband nicht frei
					}
				}
				else if () // Position Föderband Links
				{
					resultTake = xSemaphoreTake(Muxtex_Foederband_Links, i16_Semaphor_Timer); // Semaphore nehmen vom Föderband
					if(resultTake != 0)
					{

					}
					else
					{
						// Display Föderband nicht frei
					}
				}
				else // Position im Nirevana
				{
					//Displays Error
				}


			break;
			case Lok_Foederband_Mitte:

				resultTake = xSemaphoreTake(xSemaphore[d_INDEXFORK_LEFT],randomTime); // Semaphore nehmen vom Föderband
				if()
				{
					//Selbstverweisend Föderband in arbeit

				}
				else if((u8_AutoPfadWechsel == wahr) && (u8_Pfad == Pfad_Rechts))
				{
					//Auf Linken Pfad wechseln
					if()
					{
						//Semaphore frei von Shifter
					}
					else
					{
						//Warte auf Semaphore
					}
				}
				else if((u8_AutoPfadWechsel == wahr) && (u8_Pfad == Pfad_Links))
				{
					//Auf Rechten Pfad wechseln
					//Semaphore frei von Shifter
				}
			break;

			case Lok_Foederband_Rechts:
				if()
				{
					//Warten bis Föderband bei Distanz ankommt
				}
				else if()
				{
					//Semaphore Roboter Rechts nehmen
					//Roboter Rechts steuern
				}
			break;

			case Lok_Foederband_Links:
				if()
				{
					//Warten bis Föderband bei Distanz ankommt
				}
				else if()
				{
					//Semaphore Roboter Rechts nehmen
					//Roboter Rechts steuern
				}
			break;

			case Lok_Shifter:
				if()
				{
					// Warten bis Shifter fertig
				}
				else if()
				{

				}
			break;

			case Lok_Roboter_Rechts:
				if()
				{

				}
				else if()
				{

				}
			break;

			case Lok_Roboter_Links:
				if()
				{

				}
				else if()
				{

				}
			break;

			default:

			break;

		// End State Machine

		LCD_DisplayStringXY(30, 30, string);
		vTaskDelay(800);
	}
}

void sendCanMessage (void *pvargs) {
	int i;
	CARME_CAN_MESSAGE msg;

	for(;;) {
		msg.id  = 0x100;
		msg.ext = 0;
		msg.rtr = 0;
		msg.dlc = 8;

		for (i = 0; i < 8; i++) {
			msg.data[i] = i * 0x11;
		}

		xSemaphoreTake(Muxtex_Can_Tx, portMAX_DELAY);
		CARME_CAN_Write(&msg);
		xSemaphoreGive(Muxtex_Can_Tx);

		vTaskDelay(1000/portTICK_RATE_MS);
	}
}

void writeCanMessage () {


	for(;;) {

	}
}

/**
 * @brief		task writeCanMessage
 */
void writeCanMessage (void *pvargs) {
	CARME_CAN_MESSAGE *pMsg;
	char text [60];
	int i;

	for(;;) {
		xQueueReceive(Queue_Can_Rx, &pMsg, portMAX_DELAY);
		// set the unused transmitted data bytes to 0
		for (i = pMsg->dlc; i < 8; i++) {
			pMsg->data[i] = 0;
		}
		sprintf(text, "ID=%xh,DLC=%d,Data=%xh,%xh,%xh,%xh,%xh,%xh,%xh,%xh",
				       (unsigned)pMsg->id, pMsg->dlc,
				       pMsg->data[0], pMsg->data[1], pMsg->data[2], pMsg->data[3],
				       pMsg->data[4], pMsg->data[5], pMsg->data[6], pMsg->data[7]);
		xSemaphoreTake(Muxtex_Display, portMAX_DELAY);
		LCD_SetFont(&font_8x13);
		LCD_DisplayStringXY(10, 30, "New Can Message received");
		LCD_SetFont(&font_5x8);
		LCD_DisplayStringXY(10, 50, text);
		xSemaphoreGive(Muxtex_Display);
		free(pMsg);
	}
}

/**
 * @brief		IRQ readCanMessageIRQ
 */
void readCanMessageIRQ () {
	CARME_CAN_MESSAGE *pMsg;
	ERROR_CODES error;

	pMsg = malloc (sizeof (CARME_CAN_MESSAGE));

	if (pMsg != NULL) {
        // read the new message
		error = CARME_CAN_Read(pMsg);

		if (error == CARME_NO_ERROR) {
			xQueueSend(Queue_Can_Rx, &pMsg, portMAX_DELAY);
		}
		else {
			free (pMsg);
		}
	}
}
