#include <msp430.h>
#include <stdint.h>
#define P1_VECTOR 47
#define TA1_CCR0 49
#define TA0_CCR0 53
#define P1_4 0x0A
#define DELAY1 10000
#define P1_1  4
#define P2_1  4
uint8_t volatile rxCount, RX_STATE, TX_STATE, RX_BIT, RX_BUFFER, TX_BUFFER;
int8_t volatile txCount;
uint8_t P1_ON = 0xA0, P2_ON = 0x0A;
int volatile x = 0, y = 0;
enum {
    start, erro, rx, stop, ended
};

void config(void);
void sendByte(uint8_t byte);
/**
 * main.c
 */
int main(void)
{
    WDTCTL = WDTPW | WDTHOLD; // stop watchdog timer

    config();
    __bis_SR_register(GIE);

    //sendByte(0x01);
    //sendByte(0x0A);
    //sendByte(0x10);
    //sendByte(0xA0);
    //while(1);
while(1)

{
    if(P1IN & BIT1)
       {
        if(x == 0)
                   {
                       x = 1;
                   __delay_cycles(DELAY1);

                     if(P1_ON == 0x10)
                     {
                         P1_ON = 0xA0;
                     }
                     else if(P1_ON == 0xA0)
                     {
                         P1_ON = 0x10;
                     }

                 }
       }
       else if(!(P1IN & BIT1))
       {

                        x = 0;
                        __delay_cycles(DELAY1);
                        sendByte(P1_ON);
       }

       if(P2IN & BIT1)
       {
           if(y == 0)
                  {
                     y = 1;
                   __delay_cycles(DELAY1);
                   if(P2_ON == 0x01)
                   {
                      P2_ON = 0x0A;
                   }
                   else if(P2_ON == 0x0A)
                   {
                       P2_ON = 0x01;
                   }
               }
       }
       else if(!(P2IN & BIT1))
       {
           y = 0;
                 __delay_cycles(DELAY1);
                  sendByte(P2_ON);
       }
    }
return 0;
}
// Configura��o dos pinos necess�rios para realizar a comunica��o
void config(void){
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;
    // Configura��o do segundo led
    P4DIR |= BIT7;
    P4OUT &= ~BIT7;

   // Configura��o da porta P1.1 ao lado do led em P4.7
    P1DIR &= ~BIT1; // Como entrada de dados
    P1REN |= BIT1; // Habilita resistor de pull/up ou pull/down
    P1SEL &= ~BIT1; // Define como dispositivo de I/O
   // P1IE |= BIT1; // Habilita a interrup��o da porta
   // P1IES |= BIT1; // Aciona em high->low
    P1OUT |= BIT1; // Seta resistor de pull/up
    // Configura��o da porta P2.1 ao lado do led P1.0
    P2DIR &= ~ BIT1; // Como entrada de dados
    P2REN |= BIT1; // Habilita resistor de pull/up ou pull/down
    P2SEL &= ~BIT1; // Define como dispostivo de I/O
   // P2IE |= BIT1; // Habilita a interrup��o da porta
   // P1IES |= BIT1; // Acina em High->low
    P1OUT |= BIT1; //Seta resistor de pull/up

    // Configura��o pino P1.3 -> TRANSMISSOR
    P1OUT |= BIT3;              // Seta TX = 1 -> Simboliza comunica��o no canal como IDLE
    P1DIR |= BIT3;              // P1.3 como OUTPUT
    P1SEL &= ~BIT3;             // P1.3 como GPIO
    P1IFG &= ~BIT3;
    // Configura��o pino P1.4 -> RECEPTOR
    P1DIR &= ~BIT4;             // P1.4 como INPUT
    P1SEL &= ~BIT4;             // P1.4 como GPIO
    P1IES |= BIT4;              // Seleciona o flanco de descida para ativar interrup��o
    P1IE  |= BIT4;              // Habilida interrup��o
    P1IFG &= ~BIT4;
    // Configura��o do timer A canal 1
    TA1CTL = TASSEL__ACLK | MC_0;       // Timer fica parado at� que comece a recep�ao
    TA1CCR0 = 14;                       // Necess�rio para pegar o bit na metade do start
    TA1CCTL0 = CCIE;                    // Habilita interrup��o do modo de captura
    // Configura��o do timer A canal 0
    TA0CTL = TASSEL__ACLK | MC_0;        // Timer fica parado at� que comece a transmissao
    TA0CCR0 = 27;                        // Baud Rate
    TA0CCTL0 = CCIE;                    // Habilita interrup��o do modo de captura

}
// Configura��o da interrup��o do pino P1.4 -> RECEPTOR (Sinal que inicia a recep��o)
#pragma vector = P1_VECTOR;
__interrupt void P1_ISR () {
    switch(_even_in_range(P1IV, 16)){
        case P1_4:                      // 0x06 -> Start foi recebido
            TA1CTL |= MC__UP;           // Ativa o clock
            P1IE &= ~BIT4;              // Desabilita a interrup��o na receptora
            RX_STATE = start;
            rxCount = 0;
            break;
    }
}

// Configura��o da interrup��o do receptor ( Sinal enquanto a recep��o est� ocorrendo)
#pragma vector = TA1_CCR0;
__interrupt void TA1_CCR0_ISR () {
    switch(RX_STATE) {
        case start:
            RX_BIT = ((P1IN & BIT4) >> 3);
            if(RX_BIT == 0) {
                RX_STATE = rx;
                TA1CCR0 = 27;
            }
            else {
                RX_STATE = start;                   // Ocorreu um erro na transmiss�o
                P1IE |= BIT4;                       // Para o timer
            }
            break;

        case rx:
            rxCount++;
            RX_BIT = ((P1IN & BIT4) << 3);
            RX_BUFFER = (RX_BUFFER >> 1) | RX_BIT;
            if(rxCount == 8) RX_STATE = stop;    // Byte j� foi transmitido -> Fim da comuni��o
            break;

        case stop:
            if((P1IN & BIT4) == 0) {
                RX_STATE = ended;       // Fim da recep��o
                switch(RX_BUFFER){
                case 0x01:
                    P1OUT |= BIT0;
                break;

                case 0x0A:
                    P1OUT &= ~BIT0;
                break;

                case 0x10:
                    P4OUT |= BIT7;
                break;

                case 0xA0:
                     P4OUT &= ~BIT7;
                 break;
                }
                TA1CCR0 = 14;
                TA1CTL = TASSEL__ACLK | MC_0;
            }
            break;
    }
}
// Configura��o do pino P1.3 -> Tranmissor ( Sinal de envio dos bits at� transmitir um byte)
void sendByte(uint8_t byte){

    P1IE |= BIT4;
    TX_STATE = start;
    // Inicializa comunica��o
    txCount = 9;                // Ainda n�o transmitiu nenhum bit
    TX_BUFFER = byte;           // Armazea o byte a ser transmitido
    P1OUT &= ~BIT3;             // Puxa a linha para 0 -> Simboliza inicio de comunica��o
    TA0CTL |= MC__UP |          // Inicializa o timer da transmiss�o
              TACLR;            // Garante que timer inicie de 0
    while(TX_STATE != ended);

}
// Configura��o da interrup��o do timer A0 -> Respons�vel por transmitir os bits
#pragma vector = TA0_CCR0
__interrupt void TA0_CCR0_ISR () {

    if((TX_BUFFER & BIT0) == 1) P1OUT |= BIT3;     // Transmite bit 1
    else P1OUT &= ~BIT3;                    // Transmite bit 0

    TX_BUFFER = TX_BUFFER >> 1;
    txCount--;
    if(txCount==0){                         // Verifica se todos os bits j� foram transmitidos
        TX_STATE = ended;                   // Fim da comunica��o
        TA0CTL = TASSEL__ACLK | MC_0;                    // Desabilita o contador da transmiss�o
        P1OUT &= ~BIT3;
        __delay_cycles(10000);
    }
}


/*
#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR(){
    switch(__even_in_range(P1IV,0x10))
    {
        case P1_1:
            __delay_cycles(DELAY);
            if(P1_ON == 0xA0) P1_ON = 0x10;
            else if(P1_ON == 0x10) P1_ON = 0xA0;
            sendByte(P1_ON);
            break;
        default:
            break;
   }

}
#pragma vector = PORT2_VECTOR
__interrupt void PORT2_ISR()
{
    switch(__even_in_range(P2IV,0x10))
    {

        case P2_1:
            __delay_cycles(DELAY);
            if(P2_ON == 0x0A) P2_ON = 0x01;
            else if(P2_ON == 0x01) P2_ON = 0x0A;
            sendByte(P2_ON);
            break;
        default:
            break;

    }

}
*/
