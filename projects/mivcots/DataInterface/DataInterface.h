#pragma once

#include <atomic>
#include <iostream>
#include <thread>

#include "CarData.h"
#include "endpoint.h"
#include "serial/serial.h"

class DataInterface
{
public:
	DataInterface();
	~DataInterface();

	int initialize(std::string portName, long int baud, endpoint<CarData*>* _outputQ);
	void start();
	void stop();

protected:
	std::atomic<bool> isRunning;
	std::thread serialThread;

	endpoint<CarData*>* outputQ;

	void runSerialThread();

	CarData* parseString(std::string toParse);

	serial::Serial* serialPort;
};

