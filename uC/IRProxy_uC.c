 /* IRProxy.c
 *

 Descripción del Proyecto
 ~~~~~~~~~~~~~~~~~~~~~~~~

 El proyecto surge de la necesidad de enviar señales de mando infrarrojas al decodificador
 de cable, de manera de controlar su operación en forma remota desde donde no es posible
 utilizar su propio control remoto infrarrojo.

 La solución desarrollada es utilizar el conjunto ESP8266 y un microcontrolador (*1) de
 manera obtener la capacidad de enviar los mandos desde una red wifi (y por ende LAN e
 Internet inclusive), los cuales serían recibidos por el módulo ESP8266 y enviados por
 medio del puerto SPI al microcontrolador, el cual se encargaría de generar la señal
 infrarroja.

 El microcontrolador se incluye porque en general el módulo ESP8266, no tiene la capacidad
 para generar un patrón infrarrojo arbitrario(*2). Otras de las funciones del microcontrolador
 es la capacidad de cebar el ESP8266 de ser necesario.

 El protocolo de comunicación elegido es el MQTT, en razón que su soporte es mas estable y
 simple (contrario del COAP o HTTP) en NodeMCU (*3). Por otro lado como frente al usuario se
 utiliza Python con soporte de la librería Kivy, con la cual también se tiene mejor control
 sobre el tamaño de la ventana de la aplicación y es menos exigente en cuanto recursos con
 respecto a un navegador, con el agregado que facilita la compilación tanto para Windows como
 para Android de manera de obtener aplicaciones para ambos SO.

 La desventaja al utilizar el protocolo MQTT, es que debe utilizarse un servidor intermediario
 (Broker), al contrario que en Android, por el lado de la PC es un problema menor, para la
 cual es fácil montar un servidor, sin embargo dado que el servidor MQTT puede montarse en el
 servidor NAS como un servicio auxiliar, puede aprovecharse para otras aplicaciones IoT.

 Con el fin de no restringir el proyecto a su objetivo inicial y también poder usarlo aún
 es sus primeras etapas de desarrollo (con solo la captura de los patrones correspondientes a
 las teclas mas frecuentes) sin realizar mayores cambios al adicionar nuevos patrones para
 el mismo decodificador u otros equipos, se propone implementar el código del generador en
 forma genérica, de manera que la estructura de tiempos del patrón es recibida cada vez
 que deba emitirse.

 El libreto de IPython "Decodificacion de las Señales Infrarrojas del Control Remoto del
 Decodificador de Movistar.ipynb" se encarga de procesar la captura de las señales
 infrarrojas realizadas con el osciloscopio Rigol 1052E, verifica la rectitud de la captura
 y genera los archivos XML de especificación de los patrones correspondientes, los
 cuales pueden ser usados por la aplicación en Python

 Etapa Final 
 ~~~~~~~~~~~
 
 El diseño del equipo final incluye una fuente DC/DC para compartir la alimentación del 
 decodificador, la cual proviene de un adaptador de 12VDC, por lo que se incluye un 
 convertidor DC/DC híbrido el cual consta de pre-regulador conmutado step-down, operando en
 lazo abierto seguido de un regulador líneal de 3.3V. Por lo que se incluye en el codigo 
 la generación de la señal PWM con el soporte del NCO.
 
 También se incluye el código el supervisor del módulo ESP8266, para re-inicializar el 
 sistema si la comunicacíon (inter microcontrolador-módulo, router y/o servidor MQTT) se
 corta.
 
 Se pone en operación el 27 de Junio del 2017
 
 
 Notas
 ~~~~~

 (*1) : Para la pruebas de concepto se utilizo el PIC16F1619, un sub-siguiente periodo de
        prueba se realizo con el PIC16F18313. En etapa final para trabajo constante se
        utiliza también el PIC16F18313.

 (*2) : La versión actual del SDK del ESP8266 tiene soporte para generación de señales
        infrarojas aunque para un conjunto de formatos restringido, al momento no incluye
        el de Samsung.

 (3*) : El módulo NodeMCU fue utilizado para las pruebas iniciales, programado en Lua
        resulto bastante inestable, en su lugar se utilizo para el periodo de prueba
        la versión de Arduino para el ESP8266, el cual fue bastante estable y requirio
        su reinicio pocas veces.
*/


/* Patrón de Señales Infrarojas
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 
 * El patrón se recibe como una secuencia de números de longitud variable, de la cual la
 * interpretación de los 3 primeros números es la siguiente :
 *  [Versión de Protocolo]
 *  [Número de Pulsos (de la envolvente) del patrón].
 *  [Periodo de la Portadora, en ciclos del Oscilador del microcontrolador (32 MHz)] :
 *  [Periodo Activo de los Ciclos de la Portadora, en las mismas unidades]
 *
 * Los siguientes números definen en forma secuencial, para cada pulso del patrón la
 * duración en ciclos de la portadora :
 *  [Duración de la parte activa de la portadora]
 *  [Duración de la ausencia de portadora]
 *
 * La Versión del Protocolo, es 0x01 para el mando de control infrarrojo.
 *
 * Los números son codificados de la siguiente manera, se N el valor numérico :
 *    N <= 127           : 1 Byte, con el valor del Número N
 *    0x3FFF >= N >= 128 : 2 Bytes, byte LSB = 0x80 + (N % 128)
 *                                  byte MSB = int(N/128)
 *
 * Después del inicio, se espera por la recepción de la temporización (completa) del
 * patrón a generar y luego se procede a generarlo.
 *
 * La recepción de la temporización de un patrón es ignorada, mientras su emisión esta
 * en curso.
 *
 * La recepción de la temporización con un formato incorrecto, incompleto o sin respetar
 * el límite de tiempo intercaracteres también es ignorada y además en este caso se espera 
 * hasta que la transmisión cese por al menos el tiempo CLEARANCE_TIME antes de ser 
 * aceptada nuevamente.
 *
 * 
 * Otros Protocolos
 * ~~~~~~~~~~~~~~~~
 * 
 * La versión del protocolo acepta los siguientes valores para la identificación del 
 * protocolo :
 *    0x7F : Mensaje de verificación de la conexión microconrolador-módulo (KEEPALIVE_ID).
 *    0x7E : Mensaje de solicitud de cebado (RESETREQ_ID).
 * 
 * En ambos casos deben ser eguidos por un byte con el valor 0x00 (i.e. sin carga/payload).
*/

#if __16F18313
  // PIC16F18313 Configuration Bit Settings
  // 'C' source line config statements

  // CONFIG1
  #pragma config FEXTOSC = OFF    // FEXTOSC External Oscillator mode Selection bits (Oscillator not enabled)
  #pragma config RSTOSC = HFINT32 // Power-up default value for COSC bits (HFINTOSC with 2x PLL (32MHz))
  #pragma config CLKOUTEN = OFF   // Clock Out Enable bit (CLKOUT function is disabled; I/O or oscillator function on OSC2)
  #pragma config CSWEN = ON       // Clock Switch Enable bit (Writing to NOSC and NDIV is allowed)
  #pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)

  // CONFIG2
  #pragma config MCLRE = OFF      // Master Clear Enable bit (MCLR/VPP pin function is MCLR; Weak pull-up enabled )
  #pragma config PWRTE = ON       // Power-up Timer Enable bit (PWRT disabled)
  #pragma config WDTE = OFF       // Watchdog Timer Enable bits (WDT enabled, SWDTEN is ignored)
  #pragma config LPBOREN = OFF    // Low-power BOR enable bit (ULPBOR disabled)
  #pragma config BOREN = OFF      // Brown-out Reset Enable bits (Brown-out Reset enabled, SBOREN bit ignored)
  #pragma config BORV = LOW       // Brown-out Reset Voltage selection bit (Brown-out voltage (Vbor) set to 2.45V)
  #pragma config PPS1WAY = OFF    // PPSLOCK bit One-Way Set Enable bit (The PPSLOCK bit can be set and cleared repeatedly (subject to the unlock sequence))
  #pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable bit (Stack Overflow or Underflow will cause a Reset)
  #pragma config DEBUG = OFF      // Debugger enable bit (Background debugger disabled)

  // CONFIG3
  #pragma config WRT = OFF        // User NVM self-write protection bits (Write protection off)
  #pragma config LVP = OFF        // Low Voltage Programming Enable bit (HV on MCLR/VPP must be used for programming.)

  // CONFIG4
  #pragma config CP = OFF         // User NVM Program Memory Code Protection bit (User NVM code protection disabled)
  #pragma config CPD = OFF        // Data NVM Memory Code Protection bit (Data NVM code protection disabled)


#elif __16F1619
  // PIC16F1619 Configuration Bit Settings
  // 'C' source line config statements

  // CONFIG1
  #pragma config FOSC = INTOSC    // Oscillator Selection Bits (INTOSC oscillator: I/O function on CLKIN pin)
  #pragma config PWRTE = OFF      // Power-up Timer Enable (PWRT disabled)
  #pragma config MCLRE = OFF      // MCLR Pin Function Select (MCLR/VPP pin function is digital input)
  #pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
  #pragma config BOREN = OFF      // Brown-out Reset Enable (Brown-out Reset disabled)
  #pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
  #pragma config IESO = ON        // Internal/External Switch Over (Internal External Switch Over mode is enabled)
  #pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is enabled)

  // CONFIG2
  #pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
  #pragma config PPS1WAY = ON     // Peripheral Pin Select one-way control (The PPSLOCK bit cannot be cleared once it is set by software)
  #pragma config ZCD = OFF        // Zero Cross Detect Disable Bit (ZCD disable.  ZCD can be enabled by setting the ZCDSEN bit of ZCDCON)
  #pragma config PLLEN = ON       // PLL Enable Bit (4x PLL is always enabled)
  #pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
  #pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
  #pragma config LPBOR = OFF      // Low-Power Brown Out Reset (Low-Power BOR is disabled)
  #pragma config LVP = ON         // Low-Voltage Programming Enable (Low-voltage programming enabled)

  // CONFIG3
  #pragma config WDTCPS = WDTCPS1F// WDT Period Select (Software Control (WDTPS))
  #pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
  #pragma config WDTCWS = WDTCWSSW// WDT Window Select (Software WDT window size control (WDTWS bits))
  #pragma config WDTCCS = SWC     // WDT Input Clock Selector (Software control, controlled by WDTCS bits)

  // #pragma config statements should precede project file includes.
  // Use project enums instead of #define for ON and OFF.
#endif


/* Referencias Externas :
*/
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>


/* Oscilador(es) del microcontrolador :
*/
#define FINTRC                  (32E6)
#define FOSC                    FINTRC
#define FCY                     (FINTRC/4)
#define LFINTOSC                (31e3)


/* Asignación de E/S :
*/
// Excitación PWM del Pre-Regulador :
#define TRIS_SD_PWM             TRISAbits.TRISA5
#define ANSEL_SD_PWM            ANSELAbits.ANSA5
#define LAT_SD_PWM              LATAbits.LATA5
#define PPS_NCOOUT             (0b11101)
#define SD_PWM_PPS              RA5PPS

// Excitación PWM de LED Infrarojo :
#define TRIS_IR_PWM             TRISAbits.TRISA0
#define ANSEL_IR_PWM            ANSELAbits.ANSA0
#define LAT_IR_PWM              LATAbits.LATA0
#define PPS_CCP1OUT             (0b01100)
#define IR_PWM_PPS              RA0PPS

// Interfaz SPI (solo SCK y SDI) :
#define TRIS_SDI                TRISAbits.TRISA1
#define ANSEL_SDI               ANSELAbits.ANSA1
#define PPS_SDI                 (0b00001)           /* RA1 */

#define TRIS_SCK                TRISAbits.TRISA2
#define ANSEL_SCK               ANSELAbits.ANSA2
#define PPS_SCK                 (0b00010)           /* RA2 */

// Señal de cebado (RESET) del Módulo ESP8266 :
#define TRIS_ESP8266_RST        TRISAbits.TRISA4
#define ANSEL_ESP8266_RST       ANSELAbits.ANSA4
#define LAT_ESP8266_RST         LATAbits.LATA4

#if __16F1619 
  #undef ANSEL_SD_PWM
  uint8_t fake_ansel ;
  #define ANSEL_SD_PWM          fake_ansel
#endif

  
/* Complemento de Tipos estándar :
*/
//typedef unsigned short long   uint24_t ;



/* Prototipos :
*/


/** Base de Tiempos ********************************************************************/

/* TICK_PERIOD es la unidad de tiempo utilizada para medir el tiempo en la secuencia de 
 * arranque y actividad del Módulo ESP8266 :
*/
#define TICK_CLK                    (LFINTOSC/16)     /* Hz.  */
#define TICK_PERIOD                 (0.1)             /* seg. */

/* Contador de tiempo (ticks) :
*/
int16_t tick_cnt ;

#if __16F18313
  void Tick_init(void) {
    T0CON1bits.T0CS    = 0b100  ; // Reloj derivado de LFINTOSC (32KHZ).
    T0CON1bits.T0CKPS  = 0b0100 ; // Taza del Pre-divisor = 1:16
    T0CON0bits.T0OUTPS = 0b0000 ; // Taza del Post-divisor = 1:1
    T0CON0bits.T016BIT = 0      ; // Modo del Temporizador en 8 bits.

    TMR0L = 0  ;
    TMR0H = (uint8_t)(TICK_PERIOD * TICK_CLK) ;
    PIR0bits.TMR0IF = 0 ;
    PIE0bits.TMR0IE = 0 ;
    T0CON0bits.T0EN = 1 ; // Enciende el temporizador TMR0

    tick_cnt = 0 ;
  }


  void Tick_task(void) {
    if (PIR0bits.TMR0IF) {
      PIR0bits.TMR0IF = 0 ;
      ++tick_cnt ;
      asm("CLRWDT") ;
    }
  }

#elif __16F1619
  void Tick_init(void) {
    T6HLTbits.MODE  = 0b000  ; // Modo estándar.    
    T6CLKCONbits.CS = 0b0011 ; // Reloj derivado de LFINTOSC (32KHZ).
    T6CONbits.CKPS  = 0b0100 ; // Taza del Pre-divisor = 1:16
    T6CONbits.OUTPS = 0b0000 ; // Taza del Post-divisor = 1:1
    
    TMR6 = 0  ;
    PR6  = (uint8_t)(TICK_PERIOD * TICK_CLK) ;
    PIR2bits.TMR6IF = 0 ;
    PIE2bits.TMR6IE = 0 ;
    
    T6CONbits.ON = 1 ; // Enciende el temporizador TMR6

    tick_cnt = 0 ;
  }


  void Tick_task(void) {
    if (PIR2bits.TMR6IF) {
      PIR2bits.TMR6IF = 0 ;
      ++tick_cnt ;
      asm("CLRWDT") ;
    }
  }
#endif
  
/** Pre-Regulador de Tensión ***********************************************************/

/* El pre-regulador es una fuente conmutada del tipo Step-Down, operando con un ciclo de
 * trabajo constante, es decir opera en lazo abierto. Sus parámetros de operación se 
 * fijan de manera que sea capaz de suministrar la corriente máxima de la carga (I_LOAD)
 * a la tensión mínima de operación del (post) regulador lineal. 
 * 
 * La entrada de tensión se asume constante de 12VDC, pues proviene de un adaptador de 
 * corriente. 
 * 
 * 
 * Secuencia de Arranque
 * 
 * Durante el arranque del sistema, cuando la fuente conmutada esta apagada, el sistema
 * se solo se alimenta por medio de la resistencia R1 (510 ohm), la cual sirve basicamente 
 * para alimentar al microcontrolador, mientras el módulo ESP8266 debe permanece en el
 * modo de bajo consumo (CHPD = RST = 0).
 * 
 * El microcontrolador debe esperar hasta que la tensión a la salida alcance su nivel 
 * estable para iniciar la generación de la señal PWM que sirve de excitación para el
 * pre-regulador (> 0.5 seg).
 * 
 * El ciclo de trabajo del pre-regulador se calcula para obtener la tensión mínima de 
 * regulación (4.8v) del regulador lineal, a la tensión de entrada del adaptador de 
 * corriente externo (12VDC). Asumiendo que la corriente sobre la inductancia L1 no 
 * es discontinua, el ciclo de trabajo se calcula como :
 * 
 * DUTY_CYCLE = (VREG_MIN + VSCHOTTKY) / (VDC_IN - VD) = 55.6 %
 * 
 * 
 * Generación de la Excitación PWM
 * 
 * Se utiliza el NCO para generar la señal PWM de excitación del pre-regulador conmutado
 * con un reloj independiente de TMR2, el cual es usado para generar la señal de excita-
 * ción para el led emisor infrarojo.
 * 
 * El NCO se opera en el modo "Pulse Frecuency" para liberar al código de su control
 * posterior despues del arranque. En este modo de operación la elección del ancho de 
 * pulso se restringe a pocos valores (N1PWS), como compromiso entre las perdidas de 
 * conmutación (que aumentan con la frecuencia) y las óhmicas (que aumentan con la 
 * corriente pico en el Mosfet, que a su vez aumenta al disminuir la frecuencia), 
 * se eligio el valor de ancho de pulso de 4 uS (128/FOSC), el periodo se calcula 
 * en función de este para obtener el ciclo de trabajo requerido.
 * 
 * Notas
 * - Se debio incluir el zener DZ1, en razón que el regulador lineal utilizado ASM1117
 *   no soporta la tensión de entrada (> 10V) que se genera cuando el pre-regulador 
 *   arranca o tiene poca carga. Dependiendo del nivel de entrada el regulador corta
 *   la salida e inclusive -leteralmente- se quema.
 * 
 * - El valor de la resistencia R1 se hallo empíricamente, de manera que la tensión de
 *   entrada salida del pre-regulador sea la operativa (i.e. la tensión del diodo Zener
 *   DZ1), de manera de reducir el sobreimpulso de corriente que se produce durante su
 *   arranque si la salida es menor.
 *  
*/
#define NCO_START_TIME                 ( 0.5)   /* seg      */
#define VLINR_MIM                      ( 4.3)   /* Voltios  */
#define VSCHOTTKY                      ( 2.0)   /* Voltios  */
#define VSTEPDOWN_MIN                  (VLINR_MIM + VSCHOTTKY)
#define VDC_IN                         (9)/*(12.0)   /* Voltios  */
#define VDIODE                         ( 0.6)   /* Voltios  */
#define PULSE_WIDTH                    (128)    /* TOSC     */
#define NCO_PERIOD                     (PULSE_WIDTH*(VDC_IN - VDIODE)/VSTEPDOWN_MIN)
#define NCO_TOTAL_COUNT                (1048576) 

#if __16F18313
  void SDPWM_init(void) {
    // Configura y asigna el puerto de salida del NCO : 
    LAT_SD_PWM   = 1 ; // Inicialmente mantiene apagado el pre-regulador.
    ANSEL_SD_PWM = 0 ;
    TRIS_SD_PWM  = 0 ;
    SD_PWM_PPS   = 0 ;
  }


  void SDPWM_start(void) {
    // Configura el NCO para generar la señal PWM de operación del pe-regulador :
    NCO1CLKbits.N1CKS = 0b01  ; // Reloj suministrado por FOSC 
    NCO1CLKbits.N1PWS = 0b111 ; // El ancho de pulso es de 128*TCLK
    NCO1CONbits.N1POL = 1     ; // La salida es invertida 
    NCO1CONbits.N1PFM = 1     ; // Opera en el Modo 'Pulse Frecuency'

    // Asigna la salida del NCO como excitación del pre-regulador ...
    SD_PWM_PPS = PPS_NCOOUT ;

    // para fijar el periodo e iniciar su operación :
    NCO1INC = ((uint24_t)(NCO_TOTAL_COUNT/NCO_PERIOD)) ;
    NCO1CONbits.N1EN = 1 ;  
  }
  
#elif __16F1619
  /* El control de la fuente no se implementa en la maqueta de pruebas : 
  */ 
  void SDPWM_init(void) { }
  void SDPWM_start(void) { }

#endif
  
/** Supervisor del Módulo ESP8266 ******************************************************/

/* La operación del módulo ESP8266 se superviza indirectamente, por medio de varios 
   mecanismos :
     - Por el cese de comunicación con el módulo durante el tiempo límite :
       KEEPALIVE_TIMEOUT (15 seg). 
     - Por el cese de recepción de códigos válidos en el tiempo límite :
       IR_INACTIVE_TIMEOUT (6 horas). 
     - Solicitud espresa del módulo ESP8266

   Si se cumple cualquiera de las condiciones se asegura de cebar el módulo (y a la vez
   colocarlo en el modo de bajo consumo) por al menos MINIMUN_TIME_RESET (1 seg), antes
   de auto-cebarse.
  
   Si el módulo envia en forma repetida la solicitud de cebado, el módulo se mantiene 
   inactivo durante el tiempo EXTENDED_DELAY_TIME. Esta condición intenta detectar 
   problemas serios de comunicación (router o servidor MQTT fuera de servicio) y evitar
   congestionar el espacio RF.
  
   La condición para el cebado extendido es que el módulo solicite al menos MAX_RETRIES_
   REQUEST (10) veces. El número de intentos se almacena en reset_reties.cnt, como la  
   validez de su valor debe ser reconocida, por si se trata de un arrabque inicial o si
   procede de un de una cebado.

   El sistema se arranca invocando a ESP8266Watchdog_init(), luego debe invocarse
   periódicamente a ESP8266Watchdog_task(), función que superviza el tiempo transcurrido
   desde el último mensaje. 
   
   Con cada recepción se debe rearmar la cuenta de tiempo, invocando a ESP8266Watchdog_rearm().
    
*/

/* Constantes Asociadas :
*/
#define NORMAL_DELAY_TIME             (     0.5) /* seg.  */
#define EXTENDED_DELAY_TIME           (   10*60) /* seg.  */

#define IR_INACTIVE_TIMEOUT           (     6.0) /* horas */
#define INIT_KEEPALIVE_TIMEOUT        (    90.0) /* seg.  */
#define KEEPALIVE_TIMEOUT             (    30.0) /* seg.  */

#define MIN_RESET_TIME                (     1.0) /* seg. */

/* Tipo/Validación del arranque :
*/
#define RETRIES_VALID_KEY              (0x91)
#define MAX_RETRIES_REQUEST            (10)

__persistent struct {
  uint8_t validation_key ;
  uint8_t cnt ;
} reset_retries ;

enum Esp8266Timer_t { KEEPALIVE_TIMER, IR_INACTIVITY_TIMER } ;
uint32_t irInactive_tmr ;
int16_t  keepalive_tmr  ;
int16_t  last_tick      ;


void ESP8266Watchdog_init(void) {
  // Permite el arranque del módulo :
  LAT_ESP8266_RST   = 1 ;
  ANSEL_ESP8266_RST = 0 ;
  TRIS_ESP8266_RST  = 0 ;
  irInactive_tmr    = 0 ;
  keepalive_tmr     = -(int16_t)(INIT_KEEPALIVE_TIMEOUT - KEEPALIVE_TIMEOUT) ;
  last_tick         = tick_cnt ;
}


void ESP8266Watchdog_restart(void) {
uint8_t  ;
  // Se procede a cebar el módulo ...
  LAT_ESP8266_RST = 0 ;

  //se asegura de cebar el módulo al menos 1 seg.:
  INTCONbits.GIE = 0 ; 
#if __16F18313
  TMR0L = 0 ; TMR0H = (uint8_t)(MIN_RESET_TIME * TICK_CLK) ;
  while (!PIR0bits.TMR0IF) continue ;
#elif __16F1619
  TMR6 = 0 ; PR6 = (uint8_t)(MIN_RESET_TIME * TICK_CLK) ;
  while (!PIR2bits.TMR6IF) continue ;
#endif
  // y finalmente el propio microcontrolador :
  asm("RESET") ;
}


void ESP8266Watchdog_reset(void) {
  reset_retries.cnt++ ;
  ESP8266Watchdog_restart() ;
}


void ESP8266Watchdog_rearm(enum Esp8266Timer_t tmr_type) {
  if (tmr_type == IR_INACTIVITY_TIMER) {
    irInactive_tmr = 0 ;
  }
  keepalive_tmr = 0 ;
}


void ESP8266Watchdog_task(void) {
  if ((tick_cnt - last_tick) >= ((int16_t)( 1 / TICK_PERIOD))) {
    irInactive_tmr++ ;
    keepalive_tmr ++ ;
    last_tick  = tick_cnt;
  }
  
  if (irInactive_tmr >= ((uint32_t)IR_INACTIVE_TIMEOUT * 3600)) {
      ESP8266Watchdog_restart() ;
  }
  
  if (keepalive_tmr >= ((int16_t)KEEPALIVE_TIMEOUT)) {
      ESP8266Watchdog_reset() ;
  }
  
}


/** Generación de Patrones *************************************************************/

/* Constantes asociadas al patrón de los mensajes :
*/
#define KEEPALIVE_ID                        (0x7F)
#define RESETREQ_ID                         (0x7E)
#define INFRARED_REMOTE_PROXY_PROTOCOL      (01)
#define MAX_NUMBER_OF_PULSES    (17)

/* Alias de los SFR (CCP1 y TMR2) utilizados para la generción de patrones :
*/
#define CCP1CONbits_FMT          CCP1CONbits.CCP1FMT
#define CCP1CONbits_EN           CCP1CONbits.CCP1EN
#define CCP1CONbits_MODE         CCP1CONbits.CCP1MODE
#define T2CONbits_CKPS           T2CONbits.T2CKPS
#define T2CONbits_OUTPS          T2CONbits.T2OUTPS

/* Memoria de Contención de la definición del patrón :
*/
typedef union {
  struct {
    uint8_t num_pulses ;

    struct {
      uint16_t period, duty_cycle ;
    } carrier ;

    /*
    struct {
        uint8_t idx ;
        uint8_t buffer[40] ;
    } pulse ;
    */
  } ;

  uint8_t byte[46] ;
} ir_code_t ;

/* TO DO : IRCodeTX no tiene que ser del tamaño de ir_code_t */
ir_code_t irCodeTX ;

uint8_t irCodeRX[sizeof(ir_code_t)] ;
uint16_t ReadNumber(void) ;
uint16_t ReadNumber_copy(void) ;


uint8_t pattern_pulseCnt, carrier_cycleCnt ;


bool IRCodeHasEnded(void) {
  // El par CCP1/TMR2 es utilizado para generar la señal PWM del patrón de pulsos
  // infrarrojo, para lo cual se utilizan las interrupciones del TMR2 solo cuando
  // se esta generando un patrón, precisamente las interrupciones se habilitan
  // cuando se esta generando un patrón y des-habilitan cuando el generador esta
  // en reposo, por lo cual se puede utilizar como indicador para determinar que 
  // la generación termino y/o esta en reposo :
  return (unsigned)(PIE1bits.TMR2IE == 1) ;
}


/* Generación del Patrón de Pulsos del LED Infrarojo :
*/
void IRCodeTask(void) {
  if (PIR1bits.TMR2IF) {
    PIR1bits.TMR2IF = 0 ;

    if (--carrier_cycleCnt == 0) {
      carrier_cycleCnt = 0 ;

      if (++pattern_pulseCnt >= (uint8_t)(irCodeTX.num_pulses << 1)) {
        // Apaga el generador PWM (aka. el móduo CCP1)  y la salida :
        T2CONbits.TMR2ON     = 0      ;
        CCP1CONbits.CCP1MODE = 0b0000 ;
        LAT_IR_PWM = 1 ;

        // Señaliza que la generación del patrón termino :
        PIE1bits.TMR2IE = 0 ;
        return ;
      }

      carrier_cycleCnt = ReadNumber_copy() ;

      if (pattern_pulseCnt & 0x01) {
        // Periodo de reposo (no portadora) :
        LAT_IR_PWM = 1       ; // Se necesita corregir el estado de la salida PWM
                             ; // pues el módulo la deja en 0, que es el estado 
                             ; // activo del LED infrarojo.
        IR_PWM_PPS = 0b00000 ; // Se 'desconecta' la portadora de la salida.
      }
      else {
        // Periodo activo de la portadora :
        IR_PWM_PPS  = PPS_CCP1OUT ;
      }
    }
  }
}

void interrupt ServInt(void) {
  IRCodeTask() ;
}


void IRCodeInit(void) {
  // Prepara la salida de control del LED Infrarrojo a su estado en reposo :
  LAT_IR_PWM = 1 ; ANSEL_IR_PWM = 0 ; TRIS_IR_PWM = 0 ;

  // Se prepara el módulo CCP1 para actuar en el modo PWM, y se mantiene
  // apagado...
  CCP1CONbits_FMT  = 0  ; // Ciclo de trabajo = CCPR1L + 256*CCPR1H
  CCP1CONbits_EN   = 0  ;
  IR_PWM_PPS       = 0b00000 ;
  CCP1CONbits_MODE = 0b0000  ;
  
  // de la misma manera el temporizador TMR2 :
  T2CONbits_CKPS   = 0b000  ; // Pre-divisor 1:1
  T2CONbits_OUTPS  = 0b0000 ; // Post-divisor 1:1
  
  T2CONbits.TMR2ON = 0 ;
}


/* Prepara para la generación del patrón de pulsos del LED infrarrojo,
   la generación en sí se realiza en el servicio de interrupciones, 
   aunque mantiene el control hasta que finalice.
*/
void IRCodeXmit(void) {
  // Asigna los parámetros de generación del patrón :
  irCodeTX.num_pulses = ReadNumber() ;
  irCodeTX.carrier.period = ReadNumber() ;
  
  // El ciclo de trabajo del LED es el complementario de la excitación a su 
  // transistor de ataque (Q3), y por consiguiente la de la portadora  generada
  // por el módulo PWM :
  irCodeTX.carrier.duty_cycle = irCodeTX.carrier.period - ReadNumber() ;

  // Prepara para temporizar el (estado activo del) primer pulso :
  pattern_pulseCnt = 0 ;
  carrier_cycleCnt = ReadNumber() ;

  // Prepara los módulos CCP1 y TMR2 para generarla señal PWM con la frecuencia
  // de portadora y ciclo de trabajo solicitados  :
  // portadora :
  CCPR1 = irCodeTX.carrier.duty_cycle ;
  TMR2  = 0 ;
  PR2   = (irCodeTX.carrier.period >> 2)  - 1 ;

  // Prepara el sistema de interrupciones :
  PIR1bits.TMR2IF  = 0 ;
  PIE1bits.TMR2IE  = 1 ;

  // Activa la generación PWM, iniciando la generación del patrón ...
  CCP1CONbits.CCP1MODE = 0b1111      ; // Modo PWM
  CCP1CONbits.CCP1EN   = 1           ;
  LAT_IR_PWM           = 0           ;
  IR_PWM_PPS           = PPS_CCP1OUT ;
  T2CONbits.TMR2ON     = 1           ;

  // y espera a que termine :
  while (!IRCodeHasEnded()) continue ;
}


/** Recepción del Formato del Patrón a Generar *****************************************/

/* El formato del Patrón a generar, se recibe desde el interfaz (SPI), como un paquete
   de bytes (de longitud variable), utiliza el temporizador TMR1 como vigilante de tiempo
   para abortar la recepción del paquete es caso no se reciba correctamente.

   El formato del patrón es :
   [PATTERN_LENGTH] [CARRIER_TOTAL_PERIOD] [CARRIER_HIGH_PERIOD]
     [PULSE_1_HIGH] [PULSE_2_LOW] ... [PULSE_N_HIGH] [PULSE_N_LOW]

    CARRIER_TOTAL_PERIOD (2 bytes), CARRIER_HIGH_PERIOD (2 bytes) : Definen el periodo de
    la portadora y el periodo en alto de la portadora, medidos en Tcy (1/FCY = 1/8 uS).

    PULSE_LENGTH (1 byte) : Número de Pulsos del patrón, cada pulso implica un periodo en
     alto (aka. trasmisión de la portadora) y un periodo en bajo (ausencia de trasmisión)

    PULSE_x_HIGH, PULSE_x_LOW : Definen los periodos en alto y bajo del x-ésimo pulso,
    x va de 1 a PULSE_LENGTH.

    Para validar la recepción de un paquete debe cumplirse :
      - El tiempo entre la recepción de dos bytes consecutivos no debe sobrepasar
        definido por la constante RCVE_TIMEOUT.
      - El patrón de pulsos no debe sobrepasar el almacenamiento.

    Si el paquete no es validado se descarta la subsiguiente recepción hasta que
    cese el tiempo definido por CLEARANCE_TIME.

*/

#define FTMR1                    (LFINTOSC)
#define RCVE_TIMEOUT             ( 10e-3) /* seg. */
#define CLEARANCE_TIME           (100e-3) /* seg. */

/*
*/
struct {
  uint8_t rd, wr ;
} pattern_idx ;

/* NOTA : Debido a que durante el arranque del módulo ESP8266, la línea SCK, tiene
          una transición positiva, sería mejor utilizar el modo CPOL/CKP = 1, con
          fin de evitar que esta des-sincronice la comunicación, sin embargo este
          modo de operación no es soportado por el firmaware de NODEMCU (Lua 5.1.4).
          La solución adoptada es utilizar una resistencia de arrastre a tierra
          (pull-down) en la línea SCK, aunque el código también implementa la
          resincronía ante cualquier error.
*/
void SPI_Init(void) {
volatile uint8_t dummy ;
  SSP1CON1bits.SSPEN  = 0     ; // Se asegura de empezar la inicialización con el
                                // interfaz apagado/deshabilitado.

  // Configura la asignación de las E/S al interfaz SPI (SDO no se utiliza) :
  #if defined(__16F1619)
    SSPCLKPPS = PPS_SCK ; //SPI CLK asignado a RA2
    ANSEL_SCK = 0       ;

    SSPDATPPS = PPS_SDI ; //SPI SDI asignado a RA1
    ANSEL_SDI  = 0      ;
    
  #elif defined(__16F18313)
    SSP1CLKPPS = PPS_SCK ; //SPI CLK asignado a RC6
    ANSEL_SCK  = 0       ;

    SSP1DATPPS = PPS_SDI ; //SPI SDI asignado a RC4
    ANSEL_SDI  = 0       ;
  #endif
  TRIS_SDI = 1 ;  TRIS_SCK = 1 ;

  // Inicialización del SPI en el modo esclavo/muestreo en el flanco positivo del
  // reloj :
  SSP1STATbits.SMP   = 0      ; // SMP = 0 en el modo esclavo.
  SSP1STATbits.CKE   = 1      ; // El muestreo en el flanco del reloj reposo -> activo
                                // (= !CPHA )

  SSP1CON1bits.CKP   = 0      ; // El reloj esta en reposo en el estado lógico bajo.
                              ; // (= CPOPL)
  SSP1CON1bits.SSPOV = 0      ; // Borra cualquier condición de solapamiento.
  SSP1CON3bits.BOEN  = 1      ; // Actualiza incondicionalmente el buffer.
  SSP1CON1bits.SSPM  = 0b0101 ; // MSPP en el modo SPI-Esclavo sin uso de la entrada SS.

  SSP1CON1bits.SSPEN = 1      ; // Activa el interfaz SPI.

  dummy = SSP1BUF ;
}


/* Configura el temporizador de vigilancia TMR1 y el módulo de Comparación/Captura
   CCP1.
*/
void PatternRcveInit(void) {
  // Configura TMR1 para utilizarse como tiempo de guarda de la cominicación :
  T1GCONbits.TMR1GE = 0    ; // Conteo libre.
  T1CONbits.T1CKPS  = 0b00 ; // Taza del Pre-divizor = 1:1
  T1CONbits.TMR1CS  = 0b11 ; // Reloj derivado del principal (LFINOSC).
  PIR1bits.TMR1IF   = 0    ;
  PIE1bits.TMR1IE   = 0    ;
  TMR1 = -(uint16_t)(RCVE_TIMEOUT * FTMR1) ;
  T1CONbits.TMR1ON  = 1    ; // Enciende el temporizador TMR1.

  // Inicialización del SPI  :
  SPI_Init() ;

  // Prepara lo permiso de las interrupciones de TMR1 y SPI :
  PIE1bits.TMR1IE    = 0 ;
  PIE1bits.SSP1IE    = 0 ;
}


char Background_task(void) {
  // Mientras espera por la recepción del siguiente carácter, verifica si el tiempo
  // de espera no supera el máximo establecido :
  if (PIR1bits.TMR1IF) {
    PIR1bits.TMR1IF = 0 ;
    return true ;
  }

  // Como la recepción de mensajes mantiene el control en forma exclusiva, se deben 
  // ejecutar las tareas de los módulos restantes :
  Tick_task() ;
  ESP8266Watchdog_task() ;

  return false ;
}

/* Recibe un número en formato :
 * El numero es enviado en little-endian (LSB primero), en grupos de 7 bits,
 * excepto para el último byte (MSB) el 8vo bit de sus predecesores es puesto a 1.
 *
 * El número máximo es almacenado en num_bytes es : [2^7]^(num_bytes) - 1
*/
bool RcveNumber(uint8_t len) {
uint8_t i ;
  for (i = 0 ; i < len; i++) {
    if (pattern_idx.wr >= sizeof(irCodeRX)) {
      // La capacidad de almacenamiento fue desbordada :
      return false ;
    }
    
    while (!SSP1STATbits.BF) {
      if (Background_task()) {
        return false ;
      } ;
    }

    irCodeRX[pattern_idx.wr] = SSP1BUF ;

    // Rearma el tiempo de vigilancia :
    TMR1 = -(uint16_t)(RCVE_TIMEOUT * FTMR1) ;
    PIR1bits.TMR1IF = 0 ;

    // Verifica si la recepción del número terminó :
    if ((irCodeRX[pattern_idx.wr++] & 0x80) == 0x00) { return true ;}
  }

  // El número de bytes del número supera la esperada :
  return false ;
}


/* Devuelve el siguiente número almacenado.
 * Solo es válido para números en empaquetados en 1 o 2 bytes.
*/
uint16_t ReadNumber(void) {
uint8_t b ;
uint16_t num ;

  b = irCodeRX[pattern_idx.rd++] ;
  num = (uint16_t)b & 0x7F ;
  if ((b & 0x080) != 0) {
    b = irCodeRX[pattern_idx.rd++] ;
    num += (((uint16_t)(b & 0x7F)) << 7)  ;
  }

  return num ;
}

/* Se incluye una copia de ReadNumber() para evitar que el compilador muestre la
 * advertencia que esta creando una copia, en vista que es invocada tanto por 
 * la tarea de fondo como por la de interrupciones.
*/
uint16_t ReadNumber_copy(void) {
uint8_t b ;
uint16_t num ;

  b = irCodeRX[pattern_idx.rd++] ;
  num = (uint16_t)b & 0x7F ;
  if ((b & 0x080) != 0) {
    b = irCodeRX[pattern_idx.rd++] ;
    num += (((uint16_t)(b & 0x7F)) << 7)  ;
  }

  return num ;
}

/* PatternRcveTask() :
   Recibe el patron de pulsos desde el interfaz SPI, devuelve true si se recibio un 
   patrón corecto y false si el patrón es incorrecto o incompleto.
   
   Nota :
   PatternRcveTask() mantiene el control exclusivo del microcontrolador, hasta recibir 
   un mensaje, por lo tanto debe asegurar invocar con sufuciente periodicidad las tareas
   de control de tiempo (Tick_Task()) y de supervición del módulo (ESP8266Watchdog_task()).
*/
bool PatternRcveTask(void) {
uint8_t i ;
  // Inicializa el índice de escritura :
  pattern_idx.wr = 0 ;
  
  // Espera por la recepción del primer byte :
   while (!SSP1STATbits.BF) {
    // No se evalua el valor de retorno, pues en este caso el temporizador guardián de
    // la recepción no es usado, pues no hay recepción en progreso :
    Background_task() ;
  }

  // Arma el tiempo de vigilancia :
  TMR1 = -(uint16_t)(RCVE_TIMEOUT * FTMR1) ;
  PIR1bits.TMR1IF = 0 ;

  // Completa la recepción de la identificación del protocolo :
  if (!RcveNumber(1)) {
    return false ;
  }

  // Verifica si se trata de los protocolos/identificadores de mensaje soportados :
  if ((irCodeRX[0] == KEEPALIVE_ID) || (irCodeRX[0] == RESETREQ_ID)) {
    // Espera por recibir el tamaño de la carga, aka. 0, para los mensajes de
    // confirmación de la operatividad y /o solicitud de cebado del módulo ESP8266 :
    if (!RcveNumber(sizeof(irCodeTX.num_pulses)) && (irCodeRX[1] != 0x00)) {
      // La recepción fue incorrecta, incompleta o el tamaño esperado para la carga 
      // no es la correcta :
      return false ;
    }

    // La recepción del mensaje de confirmación de operatividad concluyo,
    // con éxito :
    return true ;
  }

  else if (irCodeRX[0] != INFRARED_REMOTE_PROXY_PROTOCOL) {
    // No se puede reconocer el protocolo :
    return false ;
  }

  /* Se continua con la recepción del mensaje con el patrón de la señal
     infraroja a trasmitir.
  */
  if (!RcveNumber(sizeof(irCodeTX.num_pulses))) {
    // Se produjo un error en la comunicación :
    return false ;
  }

  if (!RcveNumber(sizeof(irCodeTX.carrier.period))) {
    // Se produjo un error en la comunicación :
    return false ;
  }

  if (!RcveNumber(sizeof(irCodeTX.carrier.duty_cycle))) {
    // Se produjo un error en la comunicación :
    return false ;
  }

  // Se reciben y verifican el formato de los periodos de los pulsos del patrón
  // mientras se almacenan sin decodificarse :
  for (i = 0 ; i < irCodeRX[1] ; i++) {
    if ( RcveNumber(2) && // Tiempo de emisión de la portadora.
         RcveNumber(2) ) { // Tiempo de reposo.
      continue ;
    }

    // Se produjo un error en la comunicación y/o el la capacidad de almacenamiento fue
    // desbordada :
    return false ;
  }

  // Se prepara el índice de lectura (nótese que se evita la lectura del
  // primer byte, aquel que contiene el protocolo) :
  pattern_idx.rd = 1 ;

  return true ;
}


void PatternRcveClearance(void) {
volatile char dummy ;
  while (true) {
    TMR1 = (uint16_t)(-CLEARANCE_TIME * FTMR1) ;
    PIR1bits.TMR1IF = 0 ; PIR1bits.SSP1IF = 0 ;

    while (!SSP1STATbits.BF) {
      if (Background_task()) {
        // Es probable que el error de comunicación se deba a una falla de
        // sincronización, por eso se reinicializa el interfaz SPI :
        SPI_Init() ;

        return  ;
      }
    }

    dummy = SSP1BUF ;
  }
}



/** Programa Principal *****************************************************************/


void main(void) {
enum {
  STARTUP_EXTENDED_DELAY_STAGE ,
  PS_STARTUP_DELAY_STAGE       ,
  PS_STARTUP_STAGE             ,
  EPS_STARTUP_DELAY_STAGE       ,
  EPS_STARTUP_STAGE             ,  
  PROXY_STAGE
} stage ;
uint8_t n, b ;

  /* Inicialización del sistema interno del microcontrolador :
  */
  #if __16F18313
    // Oscilador : FOSC = 32 MHz, FCY = 8 MHz, TCY = 0.125 uS.
    OSCCON1bits.NOSC   = 0b000 ; OSCCON1bits.NDIV = 0b0000 ;
    OSCCON3bits.SOSCBE = 0 ;

    // Prepara las E/S como salidas a su estado inactivo por defecto :
    // (particularmente mantiene cebado al módulo ESP8266 y en el modo
    // de bajo consumo, indispensable para el arranque del sistema en
    // cuanto el pre-regulador de tensión esta todavía apagado.)
    LATA = 0x21 ; TRISA = 0xCE ; ANSELA = 0 ; INLVLA = 0 ;

    // Define el periodo del guardián del microcontrolador :
    WDTCONbits.WDTPS   = 0b01110 ; // 16 seg.
    WDTCONbits.SWDTEN  = 0       ; // Guardián encendido.
  #elif __16F1619
    OSCCONbits.SCS    = 0b00   ; // Oscilador Primario (i.e. HFINTOSC).
    OSCCONbits.IRCF   = 0b1110 ; // Oscilador interno (HFINTOSC) a 8 MHz.
    OSCCONbits.SPLLEN = 1      ; // Multiplidcador PLL activo.      

    // Define el periodo del guardián del microcontrolador :
    OPTION_REG = 0x8F ;
    WDTCON0bits.WDTPS  = 0b01110 ; // 16 seg.
    WDTCON1bits.WDTCS  = 0b000   ; // Reloj derivado del LFINTOSC
    WDTCON1bits.WINDOW = 0b111   ; // Sin Ventana.

    WDTCON0bits.SEN   = 1        ; // Guardián Apagado.
#endif

  // Como una medida para no saturar el medio con solicitudes de conexión, en forma
  // continua, se evalua el número de re-intentos de cebado, para decidir si permite 
  // el arranque del móculo ESP8266 en forma inmediata (NORMAL_DELAY_TIME seg.) o se
  // difiere por un tiempo prudente (EXTENDED_DELAY_TIME) :
  if ((reset_retries.validation_key == RETRIES_VALID_KEY) 
                                         && (reset_retries.cnt > MAX_RETRIES_REQUEST)) {
    stage = STARTUP_EXTENDED_DELAY_STAGE ;
    stage = PS_STARTUP_DELAY_STAGE ;
  }
  else {
    stage = PS_STARTUP_DELAY_STAGE ;
  }

  // Se asegura que la clave de validación del arranque sea válida en los subsiguientes
  // arranques en caliente :
  if (reset_retries.validation_key != RETRIES_VALID_KEY) {
    reset_retries.validation_key = RETRIES_VALID_KEY ;
    reset_retries.cnt = 0 ;
  }

  // Configura el pre-regulador :
  SDPWM_init() ;

  // Arranca la base de tiempos :
  Tick_init() ;

  // Finalmente, activa las interrupciones y arranca el sistema :
  INTCONbits.GIE  = 1 ; INTCONbits.PEIE = 1 ;

  // e inicia las tareas ...
  while (1) {
    switch (stage) {
      case STARTUP_EXTENDED_DELAY_STAGE :
        if (tick_cnt > (int16_t)(EXTENDED_DELAY_TIME/TICK_PERIOD)) {
          stage = PS_STARTUP_STAGE ;
          tick_cnt = 0 ;
        }

        // Durante esta etapa solo la tarea de control de tiempo esta activa :
        Tick_task() ;
      break ;
      
      case PS_STARTUP_DELAY_STAGE :
        //if (tick_cnt > (uint16_t)(NORMAL_DELAY_TIME/TICK_PERIOD)) stage = STARTUP_STAGE ;
        if (tick_cnt > 10) {
          stage = PS_STARTUP_STAGE ;
          tick_cnt = 0 ;
        }
        
        // Durante esta etapa solo la tarea de control de tiempo esta activa :
        Tick_task() ;
      break ;

      case PS_STARTUP_STAGE :
        // Arranca el Pre-regulador :
        SDPWM_start() ;

        stage = EPS_STARTUP_DELAY_STAGE ;
        tick_cnt = 0 ;

        Tick_task() ;
      break ;

      case EPS_STARTUP_DELAY_STAGE :
        //if (tick_cnt > (uint16_t)(NORMAL_DELAY_TIME/TICK_PERIOD)) stage = STARTUP_STAGE ;
        if (tick_cnt > 10) {
          stage = EPS_STARTUP_STAGE ;
          tick_cnt = 0 ;
        }
        
        // Durante esta etapa solo la tarea de control de tiempo esta activa :
        Tick_task() ;
      break ;

      case EPS_STARTUP_STAGE :
        // Configura el temporizador del guardián del módulo ESP8266 :
        ESP8266Watchdog_init() ;

        // Configura el generador IR ...
        IRCodeInit() ;

        // y el receptor de códigos :
        PatternRcveInit() ;

        stage = PROXY_STAGE ;

        Tick_task() ;
      break ;

      case PROXY_STAGE :
        // Se recibe el patrón :
        if (PatternRcveTask()) {
          // Se recibió un mensaje y se procesa de acuerdo a su tipo :
          switch (irCodeRX[0]) {
            case INFRARED_REMOTE_PROXY_PROTOCOL :
              // Trasmite la señal respectiva al código recibido :
              IRCodeXmit() ;
              
              // Puesta a cero del Guardián del módulo ESP8266 :
              ESP8266Watchdog_rearm(IR_INACTIVITY_TIMER) ;
              
              // Cancela el cebado largo, si es necesario :
              reset_retries.cnt = 0 ;
            break ;

            case KEEPALIVE_ID :
              // Se recibó el mensaje de confirmación que comunicacíon esta operativa,
              // se realiza la puesta a cero del guardián del módulo ESP8266 :
              ESP8266Watchdog_rearm(KEEPALIVE_TIMER) ;
            break ;

            case RESETREQ_ID :
              // El módulo solicita su cebado (no pudo establecer comunicación con el router
              // o el servidor MQQT), en consecuencia se ceba el sistema :
              ESP8266Watchdog_reset() ;
          }

          // Se continua con la recepción del siguiente mensaje :
        }
        else {
          // Se recibio un mensaje con un formato incorrecto :
          PatternRcveClearance() ;
        }
      break ;

      default :
        asm("RESET") ;
    }
  }
}
