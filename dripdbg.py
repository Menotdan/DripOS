import time
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(("0.0.0.0", 12345))
s.listen(1)

eom_sig = "#$^&e"

def remote_print(conn, str):
    try:
        conn.send(("p" + str + eom_sig).encode("ASCII")) # Remote debugger command to print stuff
    except:
        conn.close()

while True:
    conn,addr = s.accept()
    time.sleep(10)
    remote_print(conn, "\nHello from debug server!")
    while True:
        input_dat = input("Print something: ")
        remote_print(conn, input_dat)
