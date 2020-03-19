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

def dripdbg_get_msg(conn):
    try:
        cur_msg = ""
        while True:
            cur_msg += (conn.recv(1024).decode("ASCII"))
            if eom_sig in cur_msg:
                return cur_msg.split(eom_sig)[0]
    except:
        print("ERROR")

def get_hello_world(conn):
    try:
        conn.send(("h" + eom_sig).encode("ASCII"))
        return dripdbg_get_msg(conn)
    except:
        conn.close()
        return "bad"

while True:
    conn,addr = s.accept()
    time.sleep(10)
    remote_print(conn, "Hello from debugger!")
    print("Message: " + get_hello_world(conn))
