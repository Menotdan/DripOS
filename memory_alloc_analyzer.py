memory_log_name = input("Memory log to read: ")
memory_log = open(memory_log_name, "r")

memory_usage = 0
memory_usages = [] # (addr, size, type (+/-))
alloc_addresses = []

for line in memory_log:
    if line[:4] == "+mem":
        data = line[5:].replace("\n", "").split(" ")
        # print("Allocation " + str(data))
        if int(data[0]) in alloc_addresses:
            print("Allocated already used address u.u")

        alloc_addresses.append(int(data[0]))
        memory_usages.append((int(data[0]), int(data[1]), "+"))
        memory_usage += int(data[1])
    elif line[:4] == "-mem":
        data = line[5:].replace("\n", "").split(" ")
        # print("Free " + str(data))
        if not int(data[0]) in alloc_addresses:
            print("Freed unused memory at address " + hex(int(data[0])) + " u.u")
        else:
            del alloc_addresses[alloc_addresses.index(int(data[0]))]
        memory_usages.append((int(data[0]), int(data[1]), "-"))
        memory_usage -= int(data[1])

print("\n\nFinal usage: " + str(memory_usage) + " bytes")
print('"leaked" memory:')
for alloc_address in alloc_addresses:
    print(hex(alloc_address))