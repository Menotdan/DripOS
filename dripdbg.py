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
        s.close()
        quit()

def dripdbg_get_msg(conn):
    try:
        cur_msg = ""
        while True:
            cur_msg += (conn.recv(1024).decode("ASCII"))
            if eom_sig in cur_msg:
                return cur_msg.split(eom_sig)[0]
    except:
        print("ERROR")
        conn.close()
        s.close()
        quit()

def dripdbg_get_threads(conn):
    conn.send(("t" + eom_sig).encode("ASCII"))
    return dripdbg_get_msg(conn).split("+=")[1:]

def dripdbg_get_tids(conn):
    conn.send(("i" + eom_sig).encode("ASCII"))
    elements = dripdbg_get_msg(conn).split("+=")[1:]
    ret = []
    for e in elements:
        ret.append(int(e))
    return ret

def dripdbg_get_thread_info(conn, tid):
    data = str(tid)
    conn.send(("I" + data + eom_sig).encode("ASCII"))
    elements = dripdbg_get_msg(conn).split("+=")[1:]
    ret = []
    for e in elements:
        ret.append(int(e))

    return ret

def get_hello_world(conn):
    try:
        conn.send(("h" + eom_sig).encode("ASCII"))
        return dripdbg_get_msg(conn)
    except:
        conn.close()
        s.close()
        quit()
        return "bad"

def dripdbg_parse_cmd(conn, cmd, recursive, last_command):
    params_etc = cmd.split(" ")

    # Commands
    if params_etc[0] == "threads":
        print("Thread list: ", dripdbg_get_threads(conn))
        print("Thread ids: ", dripdbg_get_tids(conn))
    elif params_etc[0] == "registers":
        # Sanity check
        if not len(params_etc) == 2:
            print("Expected 1 param, got ", str(len(params_etc) - 1))
            return cmd
        if not params_etc[1].isnumeric():
            print("expected: registers <tid>")
            return cmd

        # Actually run command
        thread_info = dripdbg_get_thread_info(conn, int(params_etc[1]))

        print("rax: " + hex(thread_info[0]))
        print("rbx: " + hex(thread_info[1]))
        print("rcx: " + hex(thread_info[2]))
        print("rdx: " + hex(thread_info[3]))
        print("rdi: " + hex(thread_info[4]))
        print("rsi: " + hex(thread_info[5]))
        print("rbp: " + hex(thread_info[6]))
        print("r8: " + hex(thread_info[7]))
        print("r9: " + hex(thread_info[8]))
        print("r10: " + hex(thread_info[9]))
        print("r11: " + hex(thread_info[10]))
        print("r12: " + hex(thread_info[11]))
        print("r13: " + hex(thread_info[12]))
        print("r14: " + hex(thread_info[13]))
        print("r15: " + hex(thread_info[14]))
        print("rsp: " + hex(thread_info[15]))
        print("rip: " + hex(thread_info[16]))
        print("rflags: " + hex(thread_info[17]))
        print("cs: " + hex(thread_info[18]))
        print("ss: " + hex(thread_info[19]))
        print("cr3: " + hex(thread_info[20]))

    elif params_etc[0] == "print":
        if not len(params_etc) == 2:
            print("Expected 1 param, got ", str(len(params_etc) - 1))
            return cmd
        remote_print(conn, params_etc[1])
    elif params_etc[0] == "quit":
        conn.close()
        s.close()
        quit()
    else:
        if params_etc[0] == "" and not recursive == True:
            return dripdbg_parse_cmd(conn, last_command, True, "")

        print("Unknown command: " + params_etc[0])
        return cmd
    
    return cmd

while True:
    conn,addr = s.accept()
    time.sleep(10)
    remote_print(conn, "\nHello from debugger!")
    print("Message: " + get_hello_world(conn))
    print("Thread list: ", dripdbg_get_threads(conn))
    print("Thread ids: ", dripdbg_get_tids(conn))

    last_command = ""

    while True:
        # Command parser
        command = input("(DripDBG) ")
        last_command = dripdbg_parse_cmd(conn, command, False, last_command)