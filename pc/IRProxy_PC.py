#!python
# -*- coding: UTF-8 -*-

from kivy.app import App
from kivy.uix.stacklayout import StackLayout
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.button import Button
from kivy.core.window import Window
import paho.mqtt.publish as publish
import sys
sys.path.insert(0,'..')
from secrets import *

# Versión del Protocolo de Mando Remoto por  Señales Infrarrojas :
INFRARED_REMOTE_PROXY_PROTOCOL = 1

# Tamaño inicial de la ventana de la aplicación :
Window.size = (200, 325)

def MQTTPublish(payload):
  """
  Publica el mensaje, aka. código de la tecla, en el tópico designado (topic), en el broker MQTT (host).
  """
  host = MQQT_BROKER
  port = MQTT_PORT

  topic = "ir_proxy/deco_tv"
  try :
    publish.single(topic, payload, hostname = host, port = port)
    print("Enviando : %s" % payload)
  except :
    print("Fallo la publicación del código")

def encode(file):
    u"""
    Devuelve el código del patrón definido en el archivo XML 'file'.
    """
    import xml.etree.ElementTree as etree

    # Lee e interpreta el archivo de definición ...
    xml_root = etree.parse(file).getroot()

    source = xml_root.find('SOURCE').text.strip()

    id = xml_root.find('ID').text.strip()

    xml_carrier = xml_root.find('CARRIER')
    unit_val, time_unit = xml_carrier.attrib['unit'].strip().split()
    carrier = {"unit" : {'value' : float(unit_val), 'time_unit' : time_unit},
               "period" : int(xml_carrier.find('PERIOD').text.strip()) ,
               "duty_cycle" : int(xml_carrier.find('DUTY_CYCLE').text.strip())}

    xml_pattern = xml_root.find('PATTERN')

    pattern = {"unit" : xml_pattern.attrib["unit"].strip(), "pulse" : [] }
    for p in xml_pattern :
      pattern["pulse"].append(dict(high=int(p.find('HIGH').text.strip()), low=int(p.find('LOW').text.strip())))

    # para generar el código del patron de la tecla :
    s = encode_num(INFRARED_REMOTE_PROXY_PROTOCOL)
    s += encode_num(len(pattern["pulse"]))
    s += encode_num(carrier['period'])
    s = s + encode_num(carrier['duty_cycle'])
    for p in pattern['pulse'] :
      s = s + encode_num(p['high']) + encode_num(p['low'])

    return s


class IRButton(Button):
  u"""
  Clase descendiente de Button, utilizada para representar las teclas del control remoto y asociar el método
  'on_press' con el envío del código asociado a la tecla que representa.
  """

  def on_press(self):
    print("Presionado : %s, " % self.text , end='')
    if self.text in buttons_code.keys() :
      MQTTPublish(buttons_code[self.text])
      print(self.pos)

    else :
      print("La tecla <%s> no tiene definición." % self.text)
      print(Window.size)

#class IRProxy(GridLayout):
class IRProxy(StackLayout):

    u"""
  Clase que define el objeto raíz de Kivy, descendiente de GridLayout, su contenido es especificado en el archivo de
  Kivy 'IRProxy.py'
  """

# Aplicación de Kivy.
class IRProxyApp(App):
  u"""
  Clase para la aplicación de Kivy.
  """

  def build(self):
    return IRProxy()


def encode_num(num) :
  """
  Devuelve el código correspondiente al número 'num'
  """
  if type(num) == int :
    s = ''
    while num > 0 :
      if num > 127 :
        s += '{:02X}'.format(int(128 + (num % 128)))
      else :
        s += '{:02X}'.format(int(num % 128))
      num //= 128
    return s

  raise TypeError('encode_num solo codifica números enteros.')


if __name__ == '__main__':
  # Correspondencia entre los nombres (texto) de las teclas y la definición de su patrón :
  # buttons_code = {'+CH' : encode('+CH.xml')}
  buttons_code = {'+CH'  : encode('CH_PLUS.xml') , '-CH' : encode('CH-Minus.xml'),
                  '+VOL' : encode('Vol-Plus.xml'), '-VOL' : encode('Vol-Minus.xml'),
                  '1'    : encode('K1.xml'), '2'   : encode('K2.xml'), '3'   : encode('K3.xml'),
                  '4'    : encode('K4.xml'), '5'   : encode('K5.xml'), '6'   : encode('K6.xml'),
                  '7'    : encode('K7.xml'), '8'   : encode('K8.xml'), '9'   : encode('K9.xml'),
                  '0'    : encode('K0.xml'),
                  'GUIDE': encode('Guide.xml') , 'INFO'   : encode('Info.xml'),
                  'BACK' : encode('Back.xml')  , 'AUDIO'  : encode('Audio.xml'),
                  'UP'   : encode('Up.xml')    , 'LEFT'   : encode('Left.xml'),
                  }
  print('+CH: ', buttons_code['+CH'])
  # Aplicación de Kivy :
  IRProxyApp().run()
