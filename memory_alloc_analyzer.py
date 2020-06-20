import matplotlib.pyplot as plt

memory_log_name = input("Memory log to read: ")
memory_log = open(memory_log_name, "r")

memory_usage = 0
memory_usages = [] # (addr, size, type (+/-))
alloc_addresses = []
alloc_sizes = []

usage_over_time = []

for line in memory_log:
    if line[:4] == "+mem":
        data = line[5:].replace("\n", "").split(" ")
        # print("Allocation " + str(data))
        if int(data[0]) in alloc_addresses:
            print("Allocated already used address u.u")
            print("Address: " + hex(int(data[0])))
            print("Allocated from " + data[2])

        alloc_addresses.append(int(data[0]))
        alloc_sizes.append(int(data[1]))
        memory_usages.append((int(data[0]), int(data[1]), "+"))
        memory_usage += int(data[1])
        usage_over_time.append(memory_usage)
    elif line[:4] == "-mem":
        data = line[5:].replace("\n", "").split(" ")
        # print("Free " + str(data))
        if not int(data[0]) in alloc_addresses:
            print("Freed unused memory at address " + hex(int(data[0])) + " u.u")
            print("Freed from " + data[2])
        else:
            index = alloc_addresses.index(int(data[0]))
            if alloc_sizes[alloc_addresses.index(int(data[0]))] != int(data[1]):
                print("Sizes of free and allocation do not match! Size of allocation: " + str(alloc_sizes[index]) + " Size of free: " + data[1])
                print("Freeing from " + data[2])
            del alloc_addresses[index]
            del alloc_sizes[index]
        memory_usages.append((int(data[0]), int(data[1]), "-"))
        memory_usage -= int(data[1])
        usage_over_time.append(memory_usage)

print("\n\nFinal usage: " + str(memory_usage) + " bytes")

while True:
    see_graph = input("Would you like to see a graph of memory usage over time? (Y/N) ").lower()
    if see_graph == "y":
        plt.plot(usage_over_time)
        plt.show()
        break
    elif see_graph == "n":
        print("Ok, bye!")
        break
    else: 
        print("Please enter 'Y' or 'N'")
# print('"leaked" memory:')
# for alloc_address in alloc_addresses:
#     print(hex(alloc_address))