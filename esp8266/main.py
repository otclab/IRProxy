# main.py for ir_proxy-py

""" Libreto de Arranque.
"""
import ir_proxy_py

while 1 :
  try :
    ir_proxy_py.task()
  except Exception as e :
    print("IR_proxy task terminated \n\n", e)
    print(25*"-")


