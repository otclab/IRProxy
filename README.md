# IRProxy

### Descripción del Proyecto

El proyecto surge de la necesidad de enviar señales de mando infrarrojas al decodificador  de cable, de manera de controlar su operación en forma remota desde donde no es posible  utilizar su propio control remoto infrarrojo.

La solución desarrollada es utilizar el conjunto _ESP8266_ y un microcontrolador <sup>(*1)</sup> de  manera obtener la capacidad de enviar los mandos desde una red inalámbrica (y por ende _LAN_ e  Internet inclusive), los cuales serían recibidos por el módulo _ESP8266_ y enviados por  medio del su puerto _SPI_ al microcontrolador, el cual se encargaría de generar la señal  infrarroja.

El microcontrolador se incluye porque en general el módulo _ESP8266_, no tiene la capacidad para generar un patrón infrarrojo arbitrario <sup>(*2)</sup> y porque la confiabilidad del microcontrolador es muy superior a la del módulo _ESP8266_ en cualquiera de los tres lenguajes que  se  usaron (_LUA_, _Arduino_/_++C_, y _(Micro)Python_), encargándose de cebar el sistema en caso  se pierda la comunicación por cualquier motivo.  

El protocolo de comunicación elegido es el **_MQTT_**, en razón que su soporte es mas estable y simple (contrario del _COAP_ o _HTTP_) en _NodeMCU_ <sup>(*3)</sup>. 

Por otro lado como frente _GUI_  en la _PC_ se  utiliza _Python_ con soporte de la librería _Kivy_, con la cual también se tiene mejor control  sobre el tamaño de la ventana de la aplicación y es menos exigente en cuanto recursos con  respecto a un navegador, con el agregado que facilita la compilación tanto para _Windows_ como  para _Android_ de manera de obtener aplicaciones para ambos SO.

La desventaja al utilizar el protocolo _MQTT_, es que debe utilizarse un servidor intermediario  (_Broker_), al contrario que en _Android_, por el lado de la _PC_ es un problema menor, para la  cual es fácil montar un servidor, sin embargo dado que el servidor _MQTT_ puede montarse en el  servidor _NAS_ como un servicio auxiliar que puede aprovecharse para otras aplicaciones _IoT_.

Con el fin de no restringir el proyecto a su objetivo inicial y también poder usarlo aún  es sus primeras etapas de desarrollo (con solo la captura de los patrones correspondientes a  las teclas mas frecuentes) sin realizar mayores cambios al adicionar nuevos patrones para  el mismo decodificador u otros equipos, se propone implementar el código del generador en  forma genérica, de manera que la estructura de tiempos del patrón es recibida cada vez  que deba emitirse.

 El libreto de _IPython_ _"Decodificacion de las Señales Infrarrojas del Control Remoto del  "Decodificador de Movistar.ipynb"_ se encarga de procesar la captura de las señales  infrarrojas realizadas con el osciloscopio _Rigol 1052E_, verifica la rectitud de la captura  y genera los archivos _XML_ de especificación de los patrones correspondientes, los  cuales pueden ser usados por la aplicación en _Python_ para la _PC_.

### Etapa Final 
 El diseño del equipo final incluye una fuente _DC/DC_ para compartir la alimentación del  decodificador, la cual proviene de un adaptador de _12VDC_, por lo que se incluye un  convertidor _DC/DC híbrido_ el cual consta de pre-regulador reductor conmutado, operando en lazo abierto seguido de un regulador lineal de 3.3V.  El microcontrolador auxiliar también se genera la señal de excitación del pre-regulador.
 
##### Notas
 
 >- <sup>(\*1)</sup>  Para la pruebas de concepto se utilizo el PIC16F1619, un sub-siguiente periodo de prueba se realizo con el PIC16F18313. En la etapa final para trabajo constante se utiliza también el PIC16F18313.
 >- <sup>(\*2)</sup>  La versión actual del SDK del ESP8266 tiene soporte para generación de señales infrarrojas aunque para un conjunto de formatos restringido, al momento no incluye el de Samsung.
 >- <sup>(\*3)</sup>  El módulo NodeMCU fue utilizado para las pruebas iniciales, programado en Lua resulto bastante inestable, en su lugar se utilizo para el periodo de prueba la versión de Arduino para el ESP8266, el cual fue bastante estable y requirió su reinicio pocas veces. Sin embargo después de la actualización del servidor NAS (a FreeNAS 11.0) perdió la capacidad de conectarse, por lo que se decidió utilizar Micropython como lenguaje de operación para el módulo.

### Patrón de Señales
  
Para conservar la capacidad de generar un patrón cualquiera su especificación se trasmite cada vez que deba generarse, ellas incluyen la frecuencia de la portadora, su ciclo de trabajo, seguido de la secuencia de tiempos de generación y pausa de los que consta.

El formato depende del medio donde se ejecuta el interprete _PC_, _ESP8266_ o _Microcontrolador_, para la aplicación de escritorio _IRProxy_PC.py_ se utiliza el formato _XML_, la especificación es embebida en la etiqueta **_IR_CODE_**, la cual contiene las etiquetas **_SOURCE_**, **_ID_**, **_CARRIER_**, y **_PATTERN_**.

La etiquetas **_SOURCE_** e **_ID_** sirven para identificar el equipo cuyas señales de control remoto infrarrojas y la tecla o función que emulan respectivamente.

<La etiqueta **_CARRIER_** incluye dos sub-etiquetas **_PERIOD_** y **_DUTY_CYCLE_**, en las que se definen los valores del periodo y ciclo de trabajo de la portadora, la unidad utilizada para estos se define por el atributo **_unit_** definido en la etiqueta **_CARRIER_**. Por conveniencia define la resolución del generador / temporizador utilizado para generarla (
<a href="https://www.codecogs.com/eqnedit.php?latex=\dpi{100}&space;\tiny&space;\frac{1}{32\,&space;MHZ}" target="_blank"><img src="https://latex.codecogs.com/gif.latex?\dpi{100}&space;\tiny&space;\frac{1}{32\,&space;MHZ}" title="\tiny \frac{1}{32\, MHZ}" /></a>
 para el PIC16F18313).

Dentro la etiqueta **_PATTERN_** se define la secuencia (_type="array"_) de pulsos, mediante la sub-etiquetas **_PULSE_**, las cuales a su vez constan de las etiquetas **_HIGH_** y **_LOW_**, que define los intervalos de activación y pausa de la portadora. Las unidades utilizadas se definen en el  atributo **_unit_**, como opción su valor puede ser _"CARRIER_PERIOD"_ implicando que las unidades  utilizadas para definir estos intervalos es el periodo de la portadora.

El formato para el envío del patrón hacia el módulo _ESP8266_, es una cadena de caracteres formada por la representación hexadecimal, de la secuencia de bytes que representan en forma consecutiva la _versión_, _número de pulsos_ el _periodo de la portadora_, su _ciclo de trabajo_, seguidos de los periodos de _activación_ y _pausa_ de cada pulso del patrón. 
En este caso no se trasmite la identificación del equipo y función a la que pertenece el patrón, pero se incluye la _versión del protocolo_, al momento solo existe la versión _1_ que es la descrita. Los valores de _127_  y _126_ se reservan para comunicación particular entre el módulo _ESP8266_ y el _microcontrolador_.

La secuencia de bytes de cada valor es la correspondiente a la codificación _VLQ_ (_Variable Length Quantity_), que utiliza el valor del bit de mayor peso de cada byte para indicar si es el último, es decir si la representación en base $128$ del valor es $A_n ...  A_1 A_0$, la secuencia utilizada es <span lang="latex">(A_0+128), (A_1+128), ... (A_n + 0)</span>. 

Nótese que los valores de los tiempos deben estar especificados en la unidad de tiempo utilizada por el microcontrolador.

Finalmente, el formato de envío al microcontrolador es la secuencia de bytes antes descrita ( no su representación hexadecimal).  Los valores reservados para la versión son utilizados para :
>- _0xFF_ : Informar que la comunicación es correcta (_keepalive_).
>- _0xFE_ : Solicitud de cebado del sistema (_reset_).

####  Limitaciones del Patrón de Señales
Desde el punto de vista de la arquitectura de la especificación y su serialización, no existe limitación, sin embargo la implementación de la generación en el _microcontrolador_ impone algunas :
  >- El campo del número de pulsos esta limitado a 255. En la practica el número de pulsos esta limitado por el almacenamiento reservado al patrón, en la versión actual 46 bytes.
  >- Los periodos de tiempo se limitan a 65535 de las unidades respectivas.



