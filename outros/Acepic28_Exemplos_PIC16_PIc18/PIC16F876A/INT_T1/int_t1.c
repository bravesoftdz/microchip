//******************************************************************************
//Kit de desenvolvimento ACEPIC 18_28
//Projeto: int_t0.c
//Descri��o: Ao pressionar o bot�o TMR1 por 6 vezes, ser� gerada a interrup��o
//           pelo sinal externo no TIMER1 fazendo com que o LED se
//           alternem entre aceso e apagado dependendo   
//Desenvolvimento: Eng.: Carlos Eduardo Sandrini Luz
//                 ACEPIC Tecnologia e Treinamento LTDA
//
//Obs.: Colocar em ON as chaves 4 (RC0-TMR1), 6 (MCL1-MCLR) e 8 (LED) do DIP SW2
//      e chaves 7 (RA6-OSC1) e 8 (RA7-OSC2)
//******************************************************************************

#include <16F876A.h>
#use delay(clock=8000000)
#fuses HS,NOWDT,PUT,NOBROWNOUT,NOLVP

#bit TIMER1IF = 0x0c.0         //Define o bit 0 do resgistrador PIR1 como TIMER1IF

int led=0;         //Declara e inicia vari�vel leds


void main ()
{
OUTPUT_B(led);       // Apaga todos os leds da porta D 

/* Configura o Timer 1 para contagem interna e prescaler de 1:8 */
SETUP_TIMER_1(T1_EXTERNAL|T1_DIV_BY_1);

/* Inicializa Timer 1 com o valor 3036 */   
SET_TIMER1(65530);



while(true)                  //Loop principal
        {
        if (TIMER1IF)             //Se o bit T0IF for igual a 1
           {
           TIMER1IF = 0;      //Faz o bit igual a 0
           SET_TIMER1(65530);   //Retoma o valor inicial do Timer 0
           OUTPUT_B(led=~led);      //Inverte a situa��o dos Leds
           }
       }
}
