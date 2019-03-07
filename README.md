# ir_pronxy-py

## Descripción del Proyecto

 El proyecto surge de la necesidad de enviar señales de mando infrarrojas al decodificador
 de cable, de manera de controlar su operación en forma remota desde donde no es posible 
 utilizar su propio control remoto infrarrojo.

 La solución desarrollada es utilizar el conjunto ESP8266 y un microcontrolador (\*1) de
 manera obtener la capacidad de enviar los mandos desde una red wifi (y por ende LAN e
 Internet inclusive), los cuales serían recibidos por el módulo ESP8266 y enviados por
 medio del supuerto SPI al microcontrolador, el cual se encargaría de generar la señal
 infrarroja.

 El microcontrolador se incluye porque en general el módulo ESP8266, no tiene la capacidad
 para generar un patrón infrarrojo arbitrario(\*2) y porque la confiabilidad del microcon-
 trolador es muy superior a la del módulo ESP8266 en cualquiera de los tres lenguajes que 
 se  usaron (LUA, Arduino/++C, y (Micro)Python), encargandose de cebar el sistema en caso 
 se pierda la comunicación por cualquier motivo.  

 El protocolo de comunicación elegido es el MQTT, en razón que su soporte es mas estable y
 simple (contrario del COAP o HTTP) en NodeMCU (\*3). Por otro lado como frente al usuario se
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
 cuales pueden ser usados por la aplicación en Python para la PC.

 Etapa Final 
 ~~~~~~~~~~~
 
 El diseño del equipo final incluye una fuente DC/DC para compartir la alimentación del 
 decodificador, la cual proviene de un adaptador de 12VDC, por lo que se incluye un 
 convertidor DC/DC híbrido el cual consta de pre-regulador conmutado step-down, operando en
 lazo abierto seguido de un regulador líneal de 3.3V. El microcontrolador auxiliar también
 se encarga de su control.
 
 Notas
 ~~~~~

 (\*1) : Para la pruebas de concepto se utilizo el PIC16F1619, un sub-siguiente periodo de
        prueba se realizo con el PIC16F18313. En la etapa final para trabajo constante se
        utiliza también el PIC16F18313.

 (\*2) : La versión actual del SDK del ESP8266 tiene soporte para generación de señales
        infrarojas aunque para un conjunto de formatos restringido, al momento no incluye
        el de Samsung.

 (3\*) : El módulo NodeMCU fue utilizado para las pruebas iniciales, programado en Lua
        resulto bastante inestable, en su lugar se utilizo para el periodo de prueba
        la versión de Arduino para el ESP8266, el cual fue bastante estable y requirio
        su reinicio pocas veces. Sin embargo después de la actualización del servidor 
        NAS (a FreeNAS 11.0) perdió la capacidad de conectarse, por lo que se decidió
        utilizar Micropython como lenguaje de operación para el módulo.


## Patrón de Señales
  
Para conservar la capacidad de generar un patrón cualquiera su especificación se trasmite
cada vez que deba generarse, ellas incluyen la frecuencia de la portadora, su ciclo de 
trabajo, seguido de la secuencia de tiempos de generación y pausa de las que consta.

El formato depende del medio donde se ejecuta el interprete PC, ESP8266 o Microcontrolador).

El formato XML se utiliza para la aplicación de escritorio (IRProxy.py), la especificación 
es embebida en la etiqueta IR_CODE, la cual contiene las etiquetas SOURCE, ID, CARRIER, y
PATTERN.

La etiquetas SOURCE e ID son usadas para propósitos administrativos, deberían definir 
el equipo cuyas señales de control remoto infrarrojas se emulan y la tecla/función que
se especifica.

La etiqueta CARRIER incluye dos sub-etiquetas PERIOD y DUTY_CYCLE, en las que se definen 
los valores del periodo y ciclo de trabajo de la portadora, la unidad utilizada para estos
se define por el atributo unit definido en la etiqueta CARRIER. Por conveniencia define 
la resolución del generador/temporizador utilizado para generarla (aka. 1/(32 MHZ) para 
el PIC16F18313).

Dentro la etiqueta PATTERN se define la secuencia (type="array")de pulsos, mediante la 
sub-etiquetas PULSE, las cuales a su vez constan de las etiquetas HIGH y LOW, que define los 
intervalos de activación y pausa de la portadora. Las unidades utilizadas se definen en el 
atributo unit, como opción su valor puede ser "CARRIER_PERIOD" implicando que las unidades 
utilizadas para definir estos intervalos es el periodo de la portadora.
Solo existe una versión (=1), y define la codificación antes descrita.

El formato para el envío del patrón hacia el módulo ESP8266, es una cadena de caracteres 
formada por la representación hexadecimal, de la secuencia de bytes que representan en forma 
consecutiva la versión, número de pulsos el periodo de la portadora, su ciclo de trabajo,
seguidos de los periodos de activación y pausa de cada pulso del patrón. 

La secuencia de bytes de cada valor es la correspondiente a la codificación VLQ (Variable 
Length Quantity), que utiliza el valor del mayor bit de cada byte para indicar si es el 
último, es decir si la representación en base 128 del valor es An ... A1 A0, la secuencia
utilizada es (A0+128), (A1+128), ... (An). 


Nótese que los tiempos deben estar especificados en la unidad de frecuencia utilizada 
por el micro-controlador.

El formato de envío al micro-controlador es la secuencia de bytes antes descrita.


Limitaciones del Patrón de Señales
Desde el punto de vista de la arquitectura de la especificación y su serialización, 
no existe limitación, sin embargo implementación de la generación en el microcontrolador 
impone algunas :
  - El campo del número de pulsos esta limitado a 255. En la practica el número de 
    pulsos esta limitado por el almacenamiento reservado al patrón, en la versión 
    actual 46 bytes
  - Los periodos de tiempo se limitan a 65535 de las unidades respectivas.
 



