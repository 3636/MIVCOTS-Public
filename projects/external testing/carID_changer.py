with open("new_test_data.csv") as f:
    data = f.readlines()
	
f.close()

fnew = open('newer_test_data.csv', 'w')

for line in data:
	line = line.split(",")
	line[3] = str(int(line[3]) + 1)
	line.insert(2, "AU")
	line.insert(3, "10")
	print(line)
	fnew.write(",".join(line))
	
	
fnew.close()