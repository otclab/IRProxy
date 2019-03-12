#!python
# -*- coding: utf-8 -*-

# ir_proxy_py.py
# Version 0.3.0

import utime
import network
from machine import Pin, SPI
from umqtt.simple import MQTTClient
from secrets import *
  

RESET_REQ_CODE   = b'\x7E\x00'
KEEPALIVE_CODE   = b'\x7F\x00'

# Se debe enviar el código guardián (KEEPALIVE_CODE), antes que transcurra el periodo especificado
# (KEEPALIVE_PERIOD), desde la última transmisión , se utiliza keepalive_cnt para medir este tiempo :
KEEPALIVE_PERIOD = 120 # en unidades de 0.1 seg.
keepalive_cnt = 0

# Intervalos de espera para reintentar la conexión con el router. Debe terminar en 0 pues no tiene
# caso esperar después del último (re-)intento, define implícitamente el número de reintentos (ie.
# (len(wifi_timeout)-1) : 
wifi_timeout = (2, 2, 5, 20, 30, 0)

# SSID del Router
ssid = WIFI_SSID
password = WIFI_PSW

# IP y puerto del broker MQTT (estándar para comunicación no cifrada}) :
broker_ip = MQTT_BROKER
port = MQTT_PORT

# ID del cliente y tópico al que se suscribe :
client_id_header = 'IR_PROXY_uPython_'
topic = b'ir_proxy/deco_tv'

# Se definen las líneas de control de los LEDs:
led_broker_OK = Pin(5, Pin.OUT)
led_wifi_OK = Pin(4, Pin.OUT)
# Se define el contador de señalización de mensajes del broker :
broker_cnt = 0

# inicialmente los LEDs se apagan, nótese que el estado de los LEDs es complementario a del
# los pines de control :
led_broker_OK.on()
led_wifi_OK.on()

# Arranque del interfaz SPI (hardware):
# CPOL = 0 ; CPHA = 0
hspi = SPI(1, baudrate=1000000, polarity=0, phase=0)
data = bytearray(50)

def show_APs() :
  for n, ap in enumerate(network.WLAN(network.STA_IF).scan()) :
    print(' {:d}: {:s} [{:d}dbm] CH{:2d} {:12s} {:s}'.format(n, ap[0], ap[3], ap[2],
     ('Abierta', 'WEP', 'WPA-PSK', 'WPA2-PSK', 'WPA/WPA2-PSK')[ap[4]], ('Visible', 'Oculta')[ap[5]]))
  print('\n')


def client_task(client) :
  global broker_cnt

  # Verifica si existen mensajes recibidos :
  client.check_msg()

  # Se apaga temporalmente el led de broker para indicar que se recibió un mensaje :
  if broker_cnt > 0 :
    led_broker_OK.on()
    broker_cnt -= 1
    
  else :
    # TO DO : Mecanismo de verificación que la conexión con el broker esta activa.
    led_broker_OK.off()


# Se conecta a la red seleccionada, espera hasta un máximo de 20 seg. por la confirmación,
# luego devuelve True/False según este conectada o no.
def wifi_connect() :
  # Se prepara para configurar la conexión como estación :
  sta_if = network.WLAN(network.STA_IF)
  sta_if.active(True)

  if (sta_if.config('essid') != ssid) or  (not sta_if.isconnected()) :
    print('Seleccionando la red {:s}'.format(ssid))
    sta_if.connect(ssid, password)
    led_wifi_OK.on()

  for n in range(40) :
    if sta_if.isconnected() : break
    utime.sleep_ms(500)
  else :
    print('No se pudo conectar a {:s}.'.format(ssid)) 
    print('Las redes reconocibles son :')
    show_APs()
    
    return False

  led_wifi_OK.off()
  print('Red Wifi         : {:s}'.format(sta_if.config('essid')))
  print('Dirección MAC    : {:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}'.format(*sta_if.config('mac')))
  print('Dirección IP     : {:s}\nMáscara          : {:s}\nPuerta de Enlace : {:s}\nDNS              : {:s}'.format(*sta_if.ifconfig()))

  return True

  
# Función de callback para el proceso de los mensajes al tópico suscrito. Decodifica el mensaje
# para convertirlo en la secuencia de bytes que representa.
def relay_code(topic, code_str) :
  global keepalive_cnt, broker_cnt

  def print_msg(msg) : print('Mensaje recibido : {:s}'.format(code_str))

  if len(code_str) % 2 == 0 :
    try :
      print('decoding ...')
      data = bytearray(int(chr(h1)+chr(h2), 16) for h1,h2 in zip(*[iter(code_str)]*2))
    except Exception as e :
      print('El mensaje recibido contiene caracteres diferentes de las cifras hexadecimales.')
      print_msg(code_str)
      print('Excepción : {!r}'.format(e))
      return

    print('Re-dirigiendo el mensaje al puerto SPI.')
    print_msg(code_str)
    print('packed_data : {!r}'.format(data))
    hspi.write(data)
    print('Done\n\n')
    
    # Se señaliza la recepción (como consecuencia se apaga el LED del broker brevemente) :
    broker_cnt = 1
    
    # Finalmente se reinicia el periodo de espera del guardián :
    keepalive_cnt = 0

  else :
    # El mensaje no tiene la longitud correcta :
    print('El mensaje recibido no tiene una longitud par.\n')
    print_msg(code_str)

    
# task
def task() :
  global keepalive_cnt, wifi_timeout, broker_ip, topic

  while 1 :
    num_retries = 5
    # Intenta recuperar la conexión Wifi :
    for timeout in wifi_timeout:
      if wifi_connect() : break 
      # Se aumenta progresivamente el tiempo de espera para el siguiente intento :
      utime.sleep(timeout)
    else :
      # No se pudo conectar a la red DELFOS. Se termina el proceso, para que a continuación
      # se solicite el cebado del sistema ...
      break

    # Intenta conectarse con el servidor MQTT :
    num_retries = 5
    for n in range(num_retries) :
      try :
        print('[{:d}/{:d}] Conectándose al Servidor MQTT : {:s} ... '.format(n+1, num_retries, broker_ip), end='')
        
        # Nótese que el ID del cliente debe diferente a otros, al menos en redes locales una forma
        # de asegurar su singularidad es agregar el IP :
        client = MQTTClient(client_id_header + network.WLAN(network.STA_IF).ifconfig()[0], broker_ip)
        client.set_callback(relay_code)

        # Inicia la conexión con el broker :
        client.connect()

        print('conectado!')
        break
      except Exception as e:
        print('no resulto!')
        print('Razón : ', e)
        
        # Se re-intenta ... 
        utime.sleep(2)
        continue
    else :
      # No se pudo conectar a con el servidor MQTT. Se termina el proceso, para que a continuación
      # se solicite el cebado del sistema ...
      break

    # Se suscribe al tópico :
    num_retries = 5
    for n in range(num_retries) :
      try :
        print('[{:d}/{:d}] Suscribiéndose al Tópico <<{:s}>> ... '.format(n+1, num_retries, topic), end='')
        print 
        client.subscribe(topic)
        print('suscrito!')
        break 
        
      except Exception as e:
        print('no resulto!')
        print('Razón : ', e)
                
        # Se re-intenta :
        utime.sleep(2)
        continue
    else :
      # No se pudo suscribir en el tópico. Se termina el proceso, para que a continuación
      # se solicite el cebado del sistema ...
      break

    # Se continúa con la re-trasmisión de los códigos/patrones recibidos :
    keepalive_cnt = 0
    try :
      while network.WLAN(network.STA_IF).isconnected() :
        client_task(client)

        keepalive_cnt += 1
        if keepalive_cnt >= KEEPALIVE_PERIOD :
          # Transcurrió el periodo de tiempo límite, se envía el código guardián para indicar
          # al microcontrolador que sique operando correctamente :
          hspi.write(KEEPALIVE_CODE)
          
          # Se reinicia el periodo de espera :
          keepalive_cnt = 0

        utime.sleep_ms(100)

    except Exception as e:
      # Ha ocurrido un error inesperado, se abandona la ejecución normal, lo que implica
      # el cebado del sistema :
      print('Se produjo un error, :')
      print(e)
      raise e
      break

  # Este punto se alcanza si programa ha cesado de operar normalmente, se ordena al microcontrolador
  # cebar el ESP8266 :
  print('Se procede a cebar el sistema.')
  utime.sleep(2)
  hspi.write(RESET_REQ_CODE)


if __name__ == '__main__' :
  task()
  
