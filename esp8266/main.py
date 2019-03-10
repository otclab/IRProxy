# main.py for ir_proxy-py

""" Libreto de Arranque.
"""
import IRProxy_uPy

while 1 :
  try :
    IRProxy_uPy.task()
  except Exception as e :
    print("IRProxy task terminated \n\n", e)
    print(25*"-")


