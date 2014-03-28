/*
 * File:   main.c
 * Author: roney
 *
 * Created on 21 de Mar�o de 2014, 16:53    -   Versao 0.1
 * Versao 0.2  - 23mar2014 - implementada comunicacao RS232 e I2C
 *               ainda desajustado com frequencia, SDA e SCL.
 * Versao 0.2a - 24mar2014 - I2C  nao funcionando, SLAVE nao respondendo.
 *               NAKs. troca endereco de 8B para 5C
 * Versao 0.2b - removidas rotinas experimentais desnecessarias.
 * Versao 0.2c - 25mar2014 - I2C/SCL nao funcionam de jeito nenhum.
 *               adicionado RTC com circuito DS1307, e tambem nao funciona.
 *               obs: usando um Arduino, tudo funciona normalmente, sem dor.
 * Versao 0.2d - cada vez pior
 * Versao 0.2e - constatados problemas na PIC18F4550, trocada pela PIC18F2525
 * Versao 0.2f - com a PIC18F2525 medindo niveis de ruido e oscilacao
 *               na alimentacao eletrica via USB, agora com capacitores
 * Versao 0.3  - FINALMENTE FUNCIONANDO, descoberta a falta de ACK na leitura
 *               dos bytes dos dispositivos SLAVEs I2C; Colocando o AckI2C() e
 *               o IdleI2C() resolveu-se o problema. Simples ?
 * Versao 0.3b - Publicando comentarios sobre os configuracoes de inicializacao
 *               e sobre os REGISTRADORES e seus VETORES de configuracao
 *
 */

#include <xc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "configbits2525.h"     // outro arquivo config_bits.h para PIC18F4550
#include <plib/delays.h>
#include <plib/usart.h>
#include <plib/i2c.h>

#define _XTAL_FREQ 8000000
#define FOSC       8000000UL
#define Baud 100000

#define LED_AMAR    PORTCbits.RC0
#define LED_VERD    PORTCbits.RC1
#define LED_VERM    PORTCbits.RC2



/*      ** funcoes e comandos do Protocolo I2C (plib) - Compilador XC8 **
        AckI2C      :   Generate I2C? bus Acknowledgecondition.
        CloseI2C    :   Disable the SSP module.
        DataRdyI2C  :   Is the data available in the I2C buffer?
        getcI2C     :   Read a single byte from the I2C bus.
        getsI2C     :   Read a string from the I2C bus operating in master I2C mode.
        IdleI2C     :   Loop until I2C bus is idle.
        NotAckI2C   :   Generate I2C bus Not Acknowledgecondition.
        OpenI2C     :   Configure the SSP module.
        putcI2C     :   Write a single byte to the I2C bus.
        putsI2C     :   Write a string to the I2C bus operating in either Master or Slave mode.
        ReadI2C     :   Read a single byte from the I2C bus.
        RestartI2C  :   Generate an I2C bus Restartcondition.
        StartI2C    :   Generate an I2C bus Startcondition.
        StopI2C     :   Generate an I2C bus Stopcondition.
        WriteI2C    :   Write a single byte to the I2C bus.
 */

void ciclo (void);
void getDS1307(void);


/*
 *
 */

void main(void) {

    //OSCCONbits.IRCF = 0b111; // definir clock interno para 8 mhz
    //O Oscilador Interno (INTRC) nao tem precisao para gerar Clock I2C
    //Recomenda-se usar um Oscilador Externo (cristal) para maior precisao
    //Para se usar o INTRC (Oscilador Interno) deve-se mudar o arquivo
    // "configbits.h" com #Pragma config OSC INTRC (ou semelhante)

    TRISCbits.RC0=0;    // LED Amarelo para simples sinalizacao
    TRISCbits.RC1=0;    // LED Verde para simples sinalizacao
    TRISCbits.RC2=0;    // LED Vermelho para simples sinalizacao
    
    TRISCbits.RC6=1;    // TX da EUSART
                        // O programa ira informar na porta serial o status
                        // e logs de funcionamento da coleta de dados I2C

    /* 17. Module: MSSP (ERRATA for PIC18F4550 and PIC18F2525, etc)
     * ================
     *
     *  It has been observed that following a Power-on Reset, I2C mode may not
     *  initialize properly by just configuring the SCL and SDA pins as either
     *  inputs or outputs. This has only been seen in a few unique system
     *  environments. A test of a statistically significant sample of pre-
     *  production systems, across the voltage and current range of the
     *  application's power supply, should indicate if a system is
     *  susceptible to this issue.
     *
     * Work around = Before configuring the module for I2C operation:
     * 1. Configure the SCL and SDA pins as outputs by clearing
     *  their corresponding TRIS bits.
     * 2. Force SCL and SDA low by clearing the corresponding LAT bits.
     * 3. While keeping the LAT bits clear, configure SCL and SDA as
     *  inputs by setting their TRIS bits.
     *
     * Once this is done, use the SSPCON1 and SSPCON2 registers to
     *  configure the proper I2C mode as before.
     */

    TRISCbits.RC3=0;    // SCL do I2C colocado como saida por causa de bug*
    TRISCbits.RC4=0;    // SDA do I2C colocado como saida por causa de bug*
    LATC3=0;            // bug* pede que zere-se o LAT das portas SCL e SDA
    LATC4=0;            // durante inicializacao do I2C para evitar flutuacoes
                        // eletricas que ficariam nas portas antes de liga-las

    Delay10KTCYx(10);   // simples pausa para troca de estado na SDA e SCL

    TRISCbits.RC3=1;    // SCL do I2C, agora corretamente como saida
    TRISCbits.RC4=1;    // SDA do I2C, agora corretamente como saida
    // here ends "errata workaround"

    LED_AMAR=0; LED_VERM=1; LED_VERD=0;

    CMCON = 0x00;   // aplica configuracao '0' aos Comparators;
                    // teoricamente isso nada influi no funcionamento do I2C
                    // e essa configuracao poderia ser descartada

    INTCON = 0x00;  // Interrupt Control Register (desnecessario neste ponto)
                    // Esse registrador quando colocado '0', desabilita todas
                    // as configuracoes de interrupcao nos vetores abaixo:
                    // GIE/GIEH PEIE/GIEL TMR0IE INT0IE RBIE TMR0IF INT0IF RBIF
    PEIE=0;         // como o INTCON ja tinha sido zerado anteriormente
    GIE=0;          // esses dois vetores nao precisariam estar sendo zerados

     /*  O protocolo I2C utiliza os seguintes REGISTRADORES:
     *
     *  MSSP Control Register 1 (SSPCON1)
     *  =================================
     *
     *      WCOL:  Write Collision Detect bit (sem colisao = 0)
     *      SSPOV: Receive Overflow Indicator bit (sem overflow = 0)
     *      SSPEN: Master Synchronous Serial Port Enable bit (habilita pinos)
     *             Os pinos fisicos devem ser setados (TRIS) previamente com 1
     *
     *      CKP: SCK Release Control bit (segura o clock em LOW = 0, e libera o
     *           clock = 1 elevando a voltagem pull-up no pino)
      *
     *      SPM3:SSPM0: Master Synchronous Serial Port Mode Select bit
     *                  Define o TIPO MASTER (2 tipos) ou SLAVE (4 tipos)
     */


    SSPCON2=0x00;   // nao precisaria teoricamente ser zerado, pois eh
                    // controlado pelos comandos da PLIB

     /*  MSSP Control Register 2 (SSPCON2) - Detalhes no Modo Master
     *   =================================
     *
     *      GCEN: General Call Enable bit (Slave mode only) (not used in MASTER)
     *      ACKSTAT: Acknowledge Status bit, Master Transmit Only
     *              (ACK foi recebido do Slave =0; =1 nao recebeu ACK)
     *
     *      ACKDT: Acknowledge Data bit (Recepcao de ACK pelo MASTER)
     *              (ACK foi confirmado, gerou ACK)
     *
     *      ACKEN: Acknowledge Sequence Enable bit (Controla a transmissao de
     *             ACK pelo ACKDT, zerado automaticamente pelo hardware apos)
     *
     *      RCEN: Receive Enable bit (Master Receive mode only)
     *            (1 habilita a recepcao; 0 esta em modo IDLE)
     *
     *      PEN: Stop Condition Enable bit (inicia condicao de STOP = 1, e eh
     *           zerado automaticamente pelo hardware apos)
     *
     *      RSEN: Repeated Start Condition Enable bit (inicia repeticao de
     *            START e eh zerado automaticamente pelo hardware apos)
     * 
     *      SEN: Start Condition Enable/Stretch Enable bit
     *      obs: esses vetores (ACKEN, RCEN, PEN, RSEN e SEN) nao podem ser
     *           escritos no momento que o I2C estiver ativo (Startado)
     */

     /*  MSSP Control Register 2 (SSPCON2) - Detalhes no Modo Slave
     *   =================================
     *
     *      GCEN: General Call Enable bit (1= gera interrupcao quando o endereco
     *            0x0000 for recebido no SSPSR)
     *      ACKSTAT: Acknowledge Status bit (nao utilizado no modo slave)
     *      ADMSK5:ADMSK2: Slave Address Mask Select bits (1= mascaramento do
     *                     endereco Slave do SSPADD)
     *      ADMSK1: Slave Address Mask Select bit (=1 habilita mascara SSPADD)
     *      SEN: Stretch Enable bit (1= habilitado o Stretch)
     */

    SMP = 1;        // desabilita o SLEW RATE CONTROL, para 100 khz
                    // comando nao necessario, pois sera controlado pelo
                    // OpenI2C da PLIB. Vetor dentro do Registrador SSPSTAT.
     /*
     *  MSSP Status Register (SSPSTAT)
     *  ==============================
     *
     *      obs: Apenas o SMP e CKE setados para 100/400/1000khz e MASTER/SLAVE
     *      DA, P, S, RW, UA e BF sao status durante a transmissao ou recepcao
     *
     *  SMP: Slew Rate Control bit
      *      0 = 100 khz ou 1 mhz
      *      1 = 400 khz
      *
     *  CKE: SMBus Select bit
      *      Habilita o SMBus Specific Inputs
      *
     *  SSPBUF: Serial Receive/Transmit Buffer Register
     *          receive the byte from SSPSR and generate interrupt SSPIF
     *          SSPBUF/SSPRR are used also so send a byte
     *
     *  SSPSR:  Shift Register ? Not directly accessible, used for
     *                           shifting data in or out
     *  SSPADD: Address Register, address when in I2C Slave mode.
     *
     *  When the in Master mode, the lower seven bits of SSPADD act as the
     *  Baud Rate Generator (BRG) reload value
     */

    SSPIF=0;        // limpa o flag de interrupcao SSPIF
                    // nao necessario neste ponto do programa
    /*  PIR1: PERIPHERAL INTERRUPT REQUEST (FLAG) REGISTER 1
     *  ====================================================
     *          obs: existe outro PIR2 para mais perifericos
     * 
     *      SPPIF: Streaming Parallel Port Read/Write Interrupt Flag bit
     *      ADIF: A/D Converter Interrupt Flag bit
     *      RCIF: EUSART Receive Interrupt Flag bit
     *      TXIF: EUSART Transmit Interrupt Flag bit
     *      SSPIF: Master Synchronous Serial Port Interrupt Flag bit
     *             0 = esperando para receber ou transmitir
     *             1 = a recepcao ou transmissao esta completada
     *      CCP1IF: CCP1 Interrupt Flag bit
     *      TMR2IF: TMR2 to PR2 Match Interrupt Flag bit
     *      TMR1IF: TMR1 Overflow Interrupt Flag bit
     */

    BCLIF=0;        // limpa o flag de COLISAO do barramento
                    // nao necessario na inicializacao do programa
    /*  PIR2: PERIPHERAL INTERRUPT REQUEST (FLAG) REGISTER 2
     *  ====================================================
     *
     *      OSCFIF: Oscillator Fail Interrupt Flag bit
     *      CMIF: Comparator Interrupt Flag bit
     *      USBIF: USB Interrupt Flag bit
     *      EEIF: Data EEPROM/Flash Write Operation Interrupt Flag bit
     *      BCLIF: Bus Collision Interrupt Flag bit
     *             1 = houve COLISAO no barramento
     *             obs: tem que ser ZERADO em software
     *      HLVDIF: High/Low-Voltage Detect Interrupt Flag bit
     *      TMR3IF: TMR3 Overflow Interrupt Flag bit
     *      CCP2IF: CCP2 Interrupt Flag bit
     */


    //INTCON2bits.RBPU=1;   // pull up interno do PORTB desabilitado
                            // nao necessario para o I2C, default nao interfere
    /*  obs:    The weak pull-up is automatically turned off when the port
     *          pin is configured as an output. The pull-ups are disabled on
     *          a Power-on Reset.
     */

    //TRISCbits.RC6=0;    // Saida TX EUSART
    ADCON1=0x0F;              // desabilita todas as portas analogicas e
                              // as torna digitais
    //ADCON1=0x05;            //disable analog inputs on pins 33(RB0/AN12), 34(RB1/AN10) and 37(RB4/AN11)
    //PORTD=0x00; //PORTCbits.RC2=0;

    

    //SSPCON1bits.SSPEN=1;
    //SSPCON1=0x00;
    //SSPCON2=0x00;
    //SSPSTATbits.SMP = 1;        // desabilita o SLEW RATE CONTROL para 100 khz
    //PIR1bits.PSPIF=0;  // clear SSPIF interrupt flag
    //PIR2bits.BCLIF=0;  // clear bus collision flag
    //SSPSTAT=0x00;
    //CMCON=0x00;


    /*
 

    SSPSTATbits.CKE=0;  // use I2C levels worked also with '0'

    SSPSTATbits.SMP=1;  // disable slew rate control worked also with '0'
    PIR1bits.PSPIF=0;  // clear SSPIF interrupt flag
    PIR2bits.BCLIF=0;  // clear bus collision flag

    PIR1bits.SSPIF = 0;                  // Clear the serial port interrupt flag
    PIR2bits.BCLIF = 0;                  // Clear the bus collision interrupt flag
                                         // limpa o flag de COLISAO do barramento
    PIE2bits.BCLIE = 1;                  // Enable bus collision interrupts
    PIE1bits.SSPIE = 1;                  // Enable serial port interrupts

    INTCONbits.PEIE = 1;                   // Enable peripheral interrupts
    INTCONbits.GIE = 1;                     // Enable global interrupts
    INTCONbits.GIE_GIEH  = 1;       // GIE/GIEH: Global Interrupt Enable bit
    INTCONbits.PEIE_GIEL = 1;       // PEIE/GIEL: Peripheral Interrupt Enable bi

    PIR1bits.SSPIF = 0;             // Clear MSSP interrupt request flag
                                    // limpa o flag de interrupcao SSPIF
    PIE1bits.SSPIE = 1;             // Enable MSSP interrupt enable bit

    SSPCON2bits.SEN = 1;        // Clock stretching is enabled
    //SSPCON1bits.CKP = 1;            // release CLK

    SSPCON1        = 0x36;          // SSPEN: Synchronous Serial Port Enable bit - Enables the serial port and configures the SDA and SCL pins as the serial port pins
                                    // CKP: SCK Release Control bit              - Release clock
                                    // SSPM3:SSPM0: SSP Mode Select bits         - 0110 = I2C Slave mode, 7-bit address
    SSPSTAT        = 0x00;
    //SSPCON2        = 0x01;          // GCEN: General Call address (00h) (Slave mode only) 0 = General call address disabled
                                    // SEN: Start Condition Enable/Stretch Enable bit(1) ()Slave mode) 1 = Clock stretching is enabled
    
    SSPSTATbits.SMP = 1;        // desabilita o SLEW RATE CONTROL para 100 khz

     */

    //SSPADD = 0x27; // para 100 khz e clock de 8 mhz

    //IdleI2C();                    // Wait for the bus get idle

    // SSPADD = (( FOSC/ Bit Rate) /4 ) - 1;
    // SSPADD = ( 4.000.000/100.000 )/4 -1
    // SSPADD = ( 40 ) / 4 -1 = 10-1 = 9

    SSPADD = ((FOSC/4)/Baud)-1;


    //SSPADD =  ( 4000000/4 / 100000) -1;
    //SSPADD =    1.000.000 / 100.000  - 1
    //SSPADD = 10 - 1 = 9

    // (8.000.000 / 4) / 100.000   - 1;
    //  2.000.000
    //  --------- = 20 - 1 = 19
    //    100.000

    //SSPADD = 0x27;
    //SSPADD = 19;


    LED_VERM=0;

    CloseUSART();   // fecha qualquer USART que estaria supostamente aberta antes
                    // just closes any previous USART open port

    OpenUSART(  USART_TX_INT_OFF &
                USART_RX_INT_OFF &
                USART_ASYNCH_MODE &
                USART_EIGHT_BIT &
                USART_CONT_RX &
                USART_BRGH_LOW,
                12
                );
                // 51 = 1200bps@4mhz;  6=9600bps@4mhz (Err);  25=2400bps@4mhz
                // 103=1200bps@8mhz; 25=4800bps@8mhz; 51=2400bps@8mhz; 12=9600bps@8mhz

                while(BusyUSART());
                putrsUSART("\n\rINIT SERIAL; SSPAD=");

                while(BusyUSART());
                putsUSART( itoa(NULL,SSPADD,10) );

                while(BusyUSART());
                putrsUSART(" (hex=0x");
                putrsUSART( itoa(NULL,SSPADD,16) );
                putrsUSART(")\n\r");


    CloseI2C();


    OpenI2C(MASTER,SLEW_OFF);
    //SSPADD = ((FOSC/4)/Baud)-1;
    //SSPCON2bits.SEN = 1;        // Clock stretching is enabled


   

    Delay10KTCYx(50);
    //#define StartI2C()  SSPCON2bits.SEN=1;while(SSPCON2bits.SEN)

    /*
    	do
	{
	status = WriteI2C( 0xB8 | 0x00 );	//write the address of slave
		if(status == -1)		//check if bus collision happened
		{
                    while(BusyUSART());
                        putrsUSART("\n\rColisao.\n\r");

			data = SSPBUF;		//upon bus collision detection clear the buffer,
			SSPCON1bits.WCOL=0;	// clear the bus collision status bit
		}
	}
	while(status!=0);		//write untill successful communication
     */

        LED_VERM=0; LED_AMAR=0; LED_VERD=1;
        Delay10KTCYx(50);
        LED_VERD=0;

    // ---//---

    //while ( SSPCON2bits.SEN );


    // ---//---

    /*
    if (DataRdyI2C())
    {
        unsigned char data = getcI2C();
        LED_VERD1=1;
        Delay10KTCYx(100);
        LED_VERD1=0;
        while(BusyUSART());
            putcUSART(data);
    }*/

        // reading humidity
        //rx_byte =  i2c_read(0x00);
        //rx_byte1 = i2c_read(0x01);

        //read temp
        //rx_byte2 = i2c_read(0x02);
        //rx_byte3 = i2c_read(0x03);

        //printf ("rx_byte1 = 0x%2.2X\r   0x%2.2X\r \n", rx_byte, rx_byte1);
        //printf ("rx_byte1 = 0x%2.2X\r   0x%2.2X\r \n", rx_byte2, rx_byte3);


    //IdleI2C();

    //getcI2C();

    //WriteI2C();

    //LED_AMAR=1;
    //CloseI2C();

    //Delay10KTCYx(100);
    //LED_AMAR=0; LED_VERD1=1;
    //Delay10KTCYx(100); LED_VERD1=0;

    

    GIE=0;  // Desabilitando Interrupcoes Globais

    ciclo();

    while (1);


}

void ciclo (void)
{
    unsigned char TEMPL=0, TEMPH=0, HUMIDL=0, HUMIDH=0;
    unsigned char DUMMY=0, OP=0, BT=0;
    float humidade, temperatura;

    char msg[55];

    Delay10KTCYx(100); //Passing 0 (zero) results in a delay of 2,560,000 cycles

    while(BusyUSART());
    putrsUSART("\n\rINIT SERIAL.\n\r");

    while (1)
    {
        //putrsUSART("\r\n____OPEN I2C; ");
        //OpenI2C(MASTER,SLEW_OFF);
        LED_VERM=1;

        //while(BusyUSART());
        //putrsUSART("____START I2C.\n\r");

        getDS1307();

        StartI2C();             // ACORDAR DEVICE
        __delay_us(16);
            WriteI2C(0xB8);     // endereco Slave do AM2315
            //WriteI2C(0x03);     // byte que simboliza a temperatura
            //WriteI2C(0x00);     // start byte para leitura
            //WriteI2C(0x00);     // quantidades de bytes a serem lidos;
            //Delay1KTCYx(2);    // 2 milisegundos (800 us a 3 ms)
            __delay_us(135);
        StopI2C();

        // 10K (100) = 1000 ms
        // 1K  (100) = 100 ms
        // 1K  (10)  = 10 ms
        // 1K  (2)   = 2 ms
        // Delay100TCYx();
        __delay_us(25);


        RestartI2C();             // REQUISITAR PEDIDO DE BYTES
        __delay_us(16);
            WriteI2C(0xB8);     // endereco Slave do AM2315
            __delay_us(60);


            WriteI2C(0x03);     // byte que simboliza a temperatura
            __delay_us(60);


            WriteI2C(0x00);     // start byte para leitura
            __delay_us(60);


            WriteI2C(0x04);     // quantidades de bytes a serem lidos;
            //AckI2C();
            __delay_us(16);
        StopI2C();

        //AckI2C();
        __delay_ms(10);
        //LED_VERD2=1;
        // Delay10KTCYx(1);

        RestartI2C();
        WriteI2C(0xB9);     // endereco Slave do AM2315
        AckI2C();
        IdleI2C();

        OP          = ReadI2C();        // 1 byte
        AckI2C();
        IdleI2C();

        BT          = ReadI2C();        // 2 byte
        AckI2C();
        IdleI2C();

            HUMIDL       = ReadI2C();    // 3 byte
            AckI2C();
            IdleI2C();

            HUMIDH       = ReadI2C();    // 4 byte
            AckI2C();
        IdleI2C();

            TEMPL    = ReadI2C();    // 5 byte
            AckI2C();
        IdleI2C();

            TEMPH    = ReadI2C();    // 6 byte
            AckI2C();
        IdleI2C();

        DUMMY          = ReadI2C();     // 7 byte
        AckI2C();
        IdleI2C();

        DUMMY          = ReadI2C();     // 8 byte

        //__delay_us(16);
        StopI2C();

        //AckI2C();


        LED_VERM=0; LED_AMAR=0;LED_VERD=0;
        //LED_VERD2=0;
        //LED_AZUL=1;

        humidade  = HUMIDL;
        humidade *= 256;
        humidade += HUMIDH;
        humidade /= 10;
        
        temperatura  = TEMPL;
        temperatura *= 256;
        temperatura += TEMPH;
        temperatura /= 10;

         /* ou ainda
         RH = RHH << 8;
         RH |= RHL;

         TEMP = TEMPH << 8;
         TEMP |= TEMPL;
         */


        //sprintf (msg, "OP=%2.2X BT=%2.2X PL=%2.2X %2.2X TL=%2.2X %2.4X Dy=%2.4X \r\n",OP,BT,PRESSAOH,PRESSAOL,TEMPL,TEMPH,DUMMY);
        //printf ("TEMPL = 0x%2.2X TEMPH= 0x%2.2X \r ",TEMPL,TEMPH);
        sprintf (msg, "Temperatura= %f, Humidade= %f .", temperatura, humidade);

        while(BusyUSART());
        //putrsUSART(msg);
        putsUSART(msg);

        //,

        Delay10KTCYx(100); //LED_AZUL=0;
        while(BusyUSART());
        putrsUSART("\n\r");
        
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);

        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);

        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);

        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);


        /*Delay10KTCYx(100);Delay10KTCYx(100);Delay10KTCYx(100);
        Delay10KTCYx(100);Delay10KTCYx(100);*/

    }



}

void getDS1307(void)
{
    int hora=0, minuto=0, segundo=0, diasemana=0, dia=0, mes=0, ano=0, dummy=0;
    char msg[40];


    IdleI2C();
    RestartI2C();
        //IdleI2C();
        __delay_us(16);
        WriteI2C( 0xD0 );
        //IdleI2C();
        __delay_us(60);
        WriteI2C( 0x00 );
        IdleI2C();
        __delay_us(16);
        //AckI2C();AckI2C();AckI2C();AckI2C();AckI2C();AckI2C();AckI2C();AckI2C();
    StopI2C();

    //IdleI2C();
    __delay_us(26);

    RestartI2C();
        __delay_us(16);
        WriteI2C( 0xD1 );
        //LED_AMAR = 0;
        //while(!DataRdyI2C()) LED_VERM=~LED_VERM;
        __delay_us(1);
        IdleI2C();

        segundo    =ReadI2C();
        AckI2C();
        IdleI2C();

        minuto  =ReadI2C();
        AckI2C();
        IdleI2C();

        hora =ReadI2C();
        AckI2C();
        IdleI2C();

        diasemana=ReadI2C();
        AckI2C();
        IdleI2C();

        dia     =ReadI2C();
        AckI2C();
        IdleI2C();

        mes     =ReadI2C();
        AckI2C();
        IdleI2C();

        ano     =ReadI2C();
        AckI2C();
        IdleI2C();

        dummy   =ReadI2C();
        //AckI2C();
        //__delay_us(16);
        //IdleI2C();
        //NotAckI2C();
        //IdleI2C();
    StopI2C();


    LED_VERM = 0;

    while(BusyUSART());
                putrsUSART("SSPAD=0x");
                while(BusyUSART());
                putsUSART( itoa(NULL,SSPADD,16) );


    sprintf(msg,"(%d) __ %xh:%xm:,%xs _ dia %x/%x/%x\r\n", SSPADD,hora,minuto,segundo,
            dia,mes,ano);

    while(BusyUSART());
                putsUSART( msg );
}