#include "DatabaseConnector.h"

DatabaseConnector::DatabaseConnector()
{

}

DatabaseConnector::~DatabaseConnector()
{
}

int DatabaseConnector::initialize(endpoint<CarData*>* _dataQ, endpoint<CarData*>* _boxDataQ, endpoint<CarData*>* _analysisQ, CarPool * _carSource, CacheBank * _outputCache)
{
	dataQ = _dataQ;
	boxDataQ = _boxDataQ;
	analysisQ = _analysisQ;
	carSource = _carSource;
	outputCache = _outputCache;
	InitializeDatabase();
	return SUCCESS;
}

int DatabaseConnector::start()
{
	isRunning.store(true, std::memory_order_relaxed);
	databaseThread = std::thread(&DatabaseConnector::runDatabaseThread, this);



	if (databaseThread.joinable()) {
		return SUCCESS;
	}
	else {
		return ERR_INITERR;
	}
}

int DatabaseConnector::stop()
{
	isRunning.store(false, std::memory_order_relaxed);
	for (unsigned int ii = 0; ii < playbackThreadStatus.size(); ++ii) {
		playbackThreadStatus.at(ii)->store(true, std::memory_order_relaxed);
	}

	for (unsigned int ii = 0; ii < playbackThreads.size(); ++ii) {
		if (playbackThreads.at(ii).joinable()) {
			playbackThreads.at(ii).join();
		}
	}

	for (unsigned int ii = 0; ii < playbackThreadStatus.size(); ++ii) {
		delete playbackThreadStatus.at(ii);
	}

	if (databaseThread.joinable()) {
		databaseThread.join();
	}

	closeConnection();

	return SUCCESS;
}

//Work Flow 
//1. initDB()
//n-1. DB manipulation
//n. shutdownDB();


//////////////////////////////////////////////////////////////////////////Private Functions///////////////////////////////////////////////////////////////////////////

int DatabaseConnector::initDB() {
	mysql_init(&mysql);
	int success = 0;
	if (!mysql_real_connect(&mysql, host, user, passwd, NULL, 0, NULL, 0)){ // if failed
		return mysql_errno(&mysql);//Failed
	}
	else {//Success
		return SUCCESS;//Pass
	}
}

int DatabaseConnector::createTable(CarData *receivedData) {
	int pass = 0;
	int errorNum = NULL;
	long carnum;
	std::string str1 = "CREATE TABLE IF NOT EXISTS car";
	std::string str2 = "";
	receivedData->get(ID_S, &carnum);
	if (carnum == std::numeric_limits<long>::min()) {
		carnum = 0;
	}
	else if (carnum < 0) {
		carnum = -carnum;
	}
	str2 = std::to_string(carnum);
	std::string str3 = "(UniqueID MEDIUMINT NOT NULL AUTO_INCREMENT,PRIMARY KEY (UniqueID));";// NOT NULL AUTO_INCREMENT PRIMARY KEY (UniqueID)  ADD Miliseconds Column 
	std::string NewCarTable = str1 + str2 + str3;
	pass = mysql_query(&mysql, NewCarTable.c_str());
	if (pass == 0) {//Success
		return SUCCESS;
	}
	else {//Failed
		return mysql_errno(&mysql);//Failed
	}
}

int DatabaseConnector::addNewColumn(CarData *receivedData) {
	int columnFound = 0;
	long carnum = 0;
	std::string KeyNameWithType = "";
	char typeCompChar = 'S';
	int length = 0;

	std::map<std::string, std::string> ::iterator iter;

	for (iter = keyMap.begin();iter !=keyMap.end(); iter++) {
		std::string str1 = "ALTER TABLE car";
		std::string str2 = "";
		receivedData->get(ID_S, &carnum);
		
		if (carnum == std::numeric_limits<long>::min()) {
			carnum = 0;
		}
		else if (carnum < 0) {
			carnum = -carnum;
		}
		str2 = std::to_string(carnum);
		std::string str3 = " ADD ";
		std::string str4 = iter->second;//columnName
		std::string str5 = " ";
		KeyNameWithType = iter->second;
		length = KeyNameWithType.length();
		typeCompChar = KeyNameWithType[length - 1];
		switch (typeCompChar) {
			case 'S': {//Long
				str5.append("INT;");
				break;
			}
			case 'U': {//Unsigned Long
				str5.append("INT UNSIGNED;");
				break;
			}
			case 'D': {//Double
				str5.append("DOUBLE;");
				break;
			}
			default: {
				//wxLogDebug("addNewColumn problem in switch statement");
				break;
			}
		}
		std::string NewCarTable = str1 + str2 + str3 + str4 + str5;
		mysql_query(&mysql, NewCarTable.c_str());
	}
	std::string str = "CREATE UNIQUE INDEX Uniquedatetime ON car";
	str.append(std::to_string(carnum));
	str.append("(TIME_S,DATE_S);");
	mysql_query(&mysql, str.c_str());
	str = "CREATE INDEX searchdatetime ON car";
	str.append(std::to_string(carnum));
	str.append("(TIME_S,DATE_S);");
	mysql_query(&mysql, str.c_str());
	return SUCCESS;
}

//Add Data to Table. EIther use Insert into or possibly update.
//int DatabaseConnector::addDataToTable(long carnum, long long datetime, std::string columnName, double storedata, endpoint <CarData*, CarData* > inputqadd) {
int DatabaseConnector::addDataToTable(CarData *receivedData) {
	int pass = 0;
	int getDataErrorCode = 0;
	long carnum = 0;
	long LongData = 0;//change to std::string when overload implemented
	unsigned long ULongData = 0;
	double DoubleData = 0;
	std::string KeyNameWithType = "";
	int length = 0;
	char typeCompChar = 'S';

	std::map<std::string, std::string> ::iterator iter;
	//create look to check that the KeyNames match the column order using for loop comparing it to the known string array 
	//does not need to be order dependent. For best perf only do this if the error 1054 is returned
	std::string str1 = "INSERT INTO car";
	std::string str2 = "";
	receivedData->get(ID_S, &carnum);
	if (carnum == std::numeric_limits<long>::min()) {
		carnum = 0;
	}
	else if (carnum < 0) {
		carnum = -carnum;
	}
	str2 = std::to_string(carnum);
	std::string str3 = " (";
	iter = keyMap.begin();
	str3.append(iter->second);
	iter++;
	for (iter; iter != keyMap.end(); iter++) {
		str3.append(",");
		str3.append(iter->second);//Paul is making map to get column names*****
	}
	std::string str7 = ") VALUES (";

	iter = keyMap.begin();
	KeyNameWithType = iter->second;
	length = KeyNameWithType.length();
	typeCompChar = KeyNameWithType[length - 1];
	switch (typeCompChar) {
		case 'S': {//Long
			getDataErrorCode = receivedData->get(iter->first, &LongData);//check return code for SUCCESS which means data is good. Otherwise write NULL
			if (getDataErrorCode == SUCCESS) {
				if (KeyNameWithType == "ID_S") {
					if (LongData == std::numeric_limits<long>::min()) {
						LongData = 0;
					}
					else if (LongData < 0) {
						LongData = -LongData;
					}	
				str7.append(std::to_string(LongData));
				}
				else {
					str7.append(std::to_string(LongData));
				}
			}
			else {
				str7.append("NULL");
			}
			break;
		}
		case 'U': {//Unsigned Long
			getDataErrorCode = receivedData->get(iter->first, &ULongData);
			if (getDataErrorCode == SUCCESS) {
				str7.append(std::to_string(ULongData));
			}
			else {
				str7.append("NULL");
			}
			break;
		}
		case 'D': {//Double
			getDataErrorCode = receivedData->get(iter->first, &DoubleData);
			if (getDataErrorCode == SUCCESS) {
				str7.append(std::to_string(DoubleData));
			}
			else {
				str7.append("NULL");
			}
			break;
		}
		default: {
			//wxLogDebug("addDataToTable problem in first switch statement ");
			break;
		}
	}
	iter++;
	for (iter; iter != keyMap.end(); iter++) {
		str7.append(",");
		//get all of the key values***** will also need map to have dataTypes
		//if (iter->second ) {
		KeyNameWithType = iter->second;
		length = KeyNameWithType.length();
		typeCompChar = KeyNameWithType[length - 1];
		switch (typeCompChar){
			case 'S': {//Long
				getDataErrorCode = receivedData->get(iter->first, &LongData);//check return code for SUCCESS which means data is good. Otherwise write NULL
				if (getDataErrorCode == SUCCESS) {
					str7.append(std::to_string(LongData));
				}
				else {
					str7.append("NULL");
				}
				break;
			}
			case 'U': {//Unsigned Long
				getDataErrorCode = receivedData->get(iter->first, &ULongData);
				if (getDataErrorCode == SUCCESS) {
					str7.append(std::to_string(ULongData));
				}
				else {
					str7.append("NULL");
				}
				break;
			}
			case 'D': {//Double
				getDataErrorCode = receivedData->get(iter->first, &DoubleData);
				if (getDataErrorCode == SUCCESS) {
					str7.append(std::to_string(DoubleData));
				}
				else {
					str7.append("NULL");
				}
				break;
			}
			default: {
				//wxLogDebug("addDataToTable problem in second switch statement");
				break;
			}
		}
	}
	std::string str8 = ");";
	std::string NewCarTable = str1 + str2 + str3  + str7 + str8;
	pass = mysql_query(&mysql, NewCarTable.c_str());
	if (pass == 0) {
		return SUCCESS;
	}
	else
		return mysql_errno(&mysql);//Failed
}

//Create database Create a database on the sql server.		create database [databasename];
int DatabaseConnector::createDatabase() {
	int pass = 0;
	std::string str1 = "CREATE DATABASE ";
	std::string str2 = database;
	str2.append(";");
	std::string finalString = str1 + str2;
	pass = mysql_query(&mysql, finalString.c_str());
	if (pass == 0) {
		return SUCCESS;
	}
	else {
		return mysql_errno(&mysql);//Failed
	}
}

//Get Column names and associated types from database run on init and make function to append to this list when a new column is added
//column names will be unique key 
int DatabaseConnector::getColumnTypes(long carnum) {
	MYSQL_RES *result;
	MYSQL_ROW row;
	int pass = 0;
	int rowCnt = 0;
	std::string str1 = "SHOW COLUMNS FROM car";
	if(carnum == std::numeric_limits<long>::min()) {
		carnum = 0;
	}
	else if (carnum < 0) {
		carnum = -carnum;
	}
	std::string str2 = std::to_string(carnum);
	str2.append(";");
	std::string finalString = str1 + str2;
	pass = mysql_query(&mysql, finalString.c_str());
	if (pass == 0) {
		result = mysql_store_result(&mysql);
		while ((row = mysql_fetch_row(result))) {
			for (int i = 0; i < 2; i++) {
				if(columnDataTypes[rowCnt] != row[i]){
					columnDataTypes[endpointOfColumnTypeList] = row[i];
					endpointOfColumnTypeList++;
				}
				rowCnt++;
			}
		}
		mysql_free_result(result);
		return SUCCESS;
	}
	else {
		return mysql_errno(&mysql);//Failed
	}
}

//update table UPDATE [table name] SET Select_priv = 'Y',Insert_priv = 'Y',Update_priv = 'Y' where [field name] = 'user';
//int DatabaseConnector::tableUpdate(long carnum, int uniqueID,std::string columnName, double updatedValue, endpoint <CarData*, CarData* > inputqupdate) {//can also use timestamp instead of uniqueID. Also can add multiple column update.
int DatabaseConnector::tableUpdate(CarData *updateData) {
	int pass = 0;
	int i = 0;
	int dataFound = 0;
	long carnum, date, time;
	std::string KeyNameWithType;
	int length;
	char typeCompChar;
	long LongData;
	unsigned long ULongData;
	double DoubleData;
	std::map<std::string, std::string> ::iterator iter;
	
	std::string str1 = "UPDATE car";
	updateData->get(ID_S, &carnum);
	std::string str2 = std::to_string(carnum);
	std::string str3 = " SET ";
	iter = keyMap.begin();
	for (iter; iter != keyMap.end(); iter++) {
		KeyNameWithType = iter->second;
		if (KeyNameWithType == "DATE_S") {
			updateData->get(iter->first, &date);
		}
		if (KeyNameWithType == "TIME_S") {
			updateData->get(iter->first, &time);
		}
		length = KeyNameWithType.length();
		typeCompChar = KeyNameWithType[length - 1];
		switch (typeCompChar) {
			case 'S': {//Long
				dataFound = updateData->get(iter->first, &LongData);//check return code for SUCCESS which means data is good. Otherwise write NULL
				if (dataFound == SUCCESS) {
					str3.append(KeyNameWithType);
					str3.append("=");
					str3.append(std::to_string(LongData));
					str3.append(",");
				}
				break;
			}
			case 'U': {//Unsigned Long
				dataFound = updateData->get(iter->first, &ULongData);
				if (dataFound == SUCCESS) {
					str3.append(KeyNameWithType);
					str3.append("=");
					str3.append(std::to_string(ULongData));
					str3.append(",");
				}
				break;
			}
			case 'D': {//Double
				dataFound = updateData->get(iter->first, &DoubleData);
				if (dataFound == SUCCESS) {
					str3.append(KeyNameWithType);
					str3.append("=");
					str3.append(std::to_string(DoubleData));
					str3.append(",");
				}
				break;
			}
			default: {
				//wxLogDebug("addDataToTable problem in second switch statement");
				break;
			}
		}
	}
	str3.pop_back();
	str3.append(" WHERE DATE_S=");
	str3.append(std::to_string(date));
	str3.append(" AND TIME_S=");
	str3.append(std::to_string(time));
	std::string str4 = ";";
	std::string NewCarTable = str1 + str2 + str3 + str4;
	pass = mysql_query(&mysql, NewCarTable.c_str());
	if (pass == 0) {
		return SUCCESS;
	}
	else {
		return mysql_errno(&mysql);//Failed
	}
}

//Close connection		 mysql_close(&mysql)
int DatabaseConnector::closeConnection() {
	mysql_close(&mysql);
	return SUCCESS;
}


int DatabaseConnector::selectDatabase() {
	int pass = 0;
	pass = mysql_select_db(&mysql, database);
	if (pass == 0) {
		return SUCCESS;
	}
	else {
		return mysql_errno(&mysql);//Failed
	}
}

//Add Sanatize Function that cleans inputq and breaks it up into something usable for storing. AKA strings


////////////////////////////////////////////////////////Public Functions///////////////////////////////////////////////////////////////////////////////////////

int DatabaseConnector::AvailablePlaybackData(std::vector<databaseInfo>*availableInfo) {
	if (databaseConnected == true) {
		int pass = 0;
		MYSQL_RES *result;
		MYSQL_ROW row;
		long tempResultL = 0;
		long numberOfTables = 0;
		std::string tempResultS = "";
		int carnums[200];
		availableInfo->clear();
		int i = 0;

		//Gets the Number of tables in the DB
		std::string str = "SELECT COUNT(*) FROM information_schema.tables WHERE table_schema='mivcots';";
		pass = mysql_query(&mysql, str.c_str());
		result = mysql_store_result(&mysql);
		if (result == NULL) {
			return ERR_NODATAINDB;
		}
		row = mysql_fetch_row(result);
		if (row[0] == NULL) {
			return ERR_NODATAINDB;
		}
		tempResultS = row[0];
		numberOfTables = std::stol(tempResultS, NULL, 10);
		mysql_free_result(result);
		//Gets the actual carnums in the DB
		str = "SHOW TABLES;";
		pass = mysql_query(&mysql, str.c_str());
		result = mysql_store_result(&mysql);
		while (row = mysql_fetch_row(result)) {
			tempResultS = row[0];
			tempResultS.erase(0, 3);
			carnums[i] = std::stoi(tempResultS);	// HACK: this might explode
													// TODO: try catch

			i++;
		}

		for (long i = 0; i < numberOfTables; i++) {
			//Gets the endDate which is the Max DATE_S from the DB
			databaseInfo temp;
			temp.carID = carnums[i];
			str = "SELECT MAX(DATE_S) AS DATE_S FROM car";
			str.append(std::to_string(carnums[i]));
			str.append(";");
			pass = mysql_query(&mysql, str.c_str());
			result = mysql_store_result(&mysql);
			if (result == NULL) {
				return ERR_NODATAINDB;
			}
			row = mysql_fetch_row(result);
			if (row[0] == NULL) {
				return ERR_NODATAINDB;
			}
			tempResultS = row[0];
			tempResultL = std::stol(tempResultS, NULL, 10);
			temp.endDate = tempResultL;

			//Gets the startDate which is the Min DATE_S from the DB
			str = "SELECT MIN(DATE_S) AS DATE_S FROM car";
			str.append(std::to_string(carnums[i]));
			str.append(";");
			pass = mysql_query(&mysql, str.c_str());
			result = mysql_store_result(&mysql);
			if (result == NULL) {
				return ERR_NODATAINDB;
			}
			row = mysql_fetch_row(result);
			if (row[0] == NULL) {
				return ERR_NODATAINDB;
			}
			tempResultS = row[0];
			tempResultL = std::stol(tempResultS, NULL, 10);
			temp.startDate = tempResultL;

			//Gets the endTime which is the Max TIME_S from the DB
			str = "SELECT MAX(TIME_S) AS TIME_S FROM car";
			str.append(std::to_string(carnums[i]));
			str.append(";");
			pass = mysql_query(&mysql, str.c_str());
			result = mysql_store_result(&mysql);
			if (result == NULL) {
				return ERR_NODATAINDB;
			}
			row = mysql_fetch_row(result);
			if (row[0] == NULL) {
				return ERR_NODATAINDB;
			}
			tempResultS = row[0];
			tempResultL = std::stol(tempResultS, NULL, 10);
			temp.endTime = tempResultL;

			//Gets the startTime which is the Min TIME_S from the DB
			str = "SELECT MIN(TIME_S) AS TIME_S FROM car";
			str.append(std::to_string(carnums[i]));
			str.append(";");
			pass = mysql_query(&mysql, str.c_str());
			result = mysql_store_result(&mysql);
			if (result == NULL) {
				return ERR_NODATAINDB;
			}
			row = mysql_fetch_row(result);
			if (row[0] == NULL) {
				return ERR_NODATAINDB;
			}
			tempResultS = row[0];
			tempResultL = std::stol(tempResultS, NULL, 10);
			temp.startTime = tempResultL;
			availableInfo->push_back(temp);
		}
		mysql_free_result(result);
		return SUCCESS;
	}
	else
		return ERR_DATABASE;
}

int DatabaseConnector::InitializeDatabase() {
	initDB();
	endpointOfColumnTypeList = 0;
	int ErrorNum = createDatabase();

	if (ErrorNum != 1007 && ErrorNum != 0) {
		//wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
		databaseConnected = false;
		return ERR_DATABASE;
	}

	int ErrorNum2 = selectDatabase();

	if (ErrorNum2 != 0) {
		//wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
		databaseConnected = false;
		return ERR_DATABASE;
	}
	/*
	int ErrorNum3 = getColumnTypes(1);

	if (ErrorNum3 != 0 || ErrorNum3 != 1146) {
		wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
		return ERR_DATABASE;
	}
	*/
	for (int i = 0; i < 128; i++) {
		knownCarTables[i] = 0;
		columnDataTypes[i] = "";
	}
	databaseConnected = true;
	return SUCCESS;
}

int DatabaseConnector::AddData(CarData *receivedData) {
	if (databaseConnected == true) {
		int ErrorNum = 0;
		int ErrorNum2 = 0;
		long carnum;
		int rc = receivedData->get(ID_S, &carnum);
		if (rc != SUCCESS) {
			return rc;
		}
		if (knownCarTables[carnum] == 0) {
			knownCarTables[carnum] = 1;
			ErrorNum = createTable(receivedData);
			ErrorNum2 = addNewColumn(receivedData);
		}
		if (ErrorNum != 0) {
			//wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
			return ERR_DATABASE;
		}
		else if (ErrorNum2 != 0) {
			//wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
			return ERR_DATABASE;
		}

		int ErrorNum3 = addDataToTable(receivedData);
		if (ErrorNum3 != 0) {
			//wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
			return ERR_DATABASE;
		}
		return SUCCESS;
	}
	else
		return ERR_DATABASE;
}


int DatabaseConnector::UpdateData(CarData *updateData) {
	
	int ErrorNum = tableUpdate(updateData);
	if (ErrorNum != 0) {
		//wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
		return 1;
	}

	return SUCCESS;
}

//shutdown mysql_shutdown(&mysql)
int DatabaseConnector::shutdown() {
	int pass = 0;
	pass = mysql_query(&mysql, "SHUTDOWN");
	closeConnection();//calls the close connection function
	if (pass == 0) {
		std::cout << "Shutdown Successful" << std::endl;
		return SUCCESS;
	}
	else {
		int ErrorNum = mysql_errno(&mysql);
		//wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
		return ERR_DATABASE;//Failed
	}
}

int DatabaseConnector::dropTable(long carnum) {
	int pass = 0;
	std::string str1 = "DROP TABLE car";
	std::string str2 = "";
	str2 = std::to_string(carnum);
	std::string finalString = str1 + str2;
	pass = mysql_query(&mysql, finalString.c_str());
	if (pass == 0) {
		return SUCCESS;
	}
	else {
		int ErrorNum = mysql_errno(&mysql);
		//wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
		return ERR_DATABASE;//Failed
	}
}

//delete row of data from table   Delete a row(s) from a table.		DELETE from [table name] where [field name] = 'whatever';
int DatabaseConnector::dropRowFromTable(long carnum, long long timestamp) {
	int pass = 0;
	std::string str1 = "DELETE FROM car";
	std::string str2 = std::to_string(carnum);
	std::string str3 = " WHERE timestamp = ";
	std::string str4 = std::to_string(timestamp);
	std::string str5 = ";";
	std::string finalString = str1 + str2 + str3 + str4 + str5;
	pass = mysql_query(&mysql, finalString.c_str());
	if (pass == 0) {
		return SUCCESS;
	}
	else {
		int ErrorNum = mysql_errno(&mysql);
		wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
		return ERR_DATABASE;//Failed
	}
}

//delete column from table  Delete a column.	alter table [table name] drop column [column name];
int DatabaseConnector::dropColumn(long carnum, std::string columnName) {
	int pass = 0;
	std::string str1 = "ALTER TABLE car";
	std::string str2 = std::to_string(carnum);
	std::string str3 = " DROP COLUMN ";
	std::string str4 = columnName;
	std::string str5 = ";";
	std::string finalString = str1 + str2 + str3 + str4 + str5;
	pass = mysql_query(&mysql, finalString.c_str());
	if (pass == 0) {
		return SUCCESS;
	}
	else {
		int ErrorNum = mysql_errno(&mysql);
		wxLogError(_(std::to_string(ErrorNum) + mysql_error(&mysql)));
		return ERR_DATABASE;//Failed
	}
}

void DatabaseConnector::runDatabaseThread()
{
	mysql_thread_init();
	CarData* receivedData, * receivedBoxData, * receivedAnalysisData;

	while (isRunning) {
		while (dataQ->receiveQsize() > 0) {
			dataQ->receive(&receivedData);
			AddData(receivedData);
			wxLogDebug("Database connector received a cardata object");
			outputCache->feed(receivedData);	// TODO: try feed function that doesn't block
			wxLogDebug("Database connector sent a cardata object");
		}

		while (analysisQ->receiveQsize() > 0) {
			analysisQ->receive(&receivedAnalysisData);
			UpdateData(receivedAnalysisData);	// TODO: is this the right function?
			carSource->releaseCar(receivedAnalysisData);
		}

		while (boxDataQ->receiveQsize() > 0) {
			boxDataQ->receive(&receivedBoxData);
			outputCache->feed(receivedBoxData);
		}

		Sleep(1);

	}
	mysql_thread_end();
}

int DatabaseConnector::startPlayback(databaseInfo playbackRequest, double timeFactor)
{
	if (databaseConnected = true) {
		if (playbackThreads.empty()) {
			// Start a new one
			playbackThreads.emplace_back();

			playbackThreadStatus.push_back(new std::atomic<bool>);
			playbackThreadStatus.at(0)->store(true, std::memory_order_relaxed);
		}

		for (unsigned int ii = 0; ii < playbackThreads.size(); ++ii) {
			if (playbackThreadStatus.at(ii)->load(std::memory_order_relaxed)) {
				// We can reuse a thread!
				if (playbackThreads.at(ii).joinable()) {
					playbackThreads.at(ii).join();
				}

				playbackThreads.at(ii) = std::thread(&DatabaseConnector::getDataTimestamp, this, playbackThreadStatus.at(ii),
					playbackRequest.carID, playbackRequest.startDate, playbackRequest.endDate,
					playbackRequest.startTime, playbackRequest.endTime, timeFactor);
				playbackThreadStatus.at(ii)->store(false, std::memory_order_relaxed);
				return SUCCESS;
			}
		}

		// All threads are busy. Start a new one
		playbackThreads.emplace_back();
		playbackThreadStatus.push_back(new std::atomic<bool>);
		playbackThreads.at(playbackThreads.size() - 1) = std::thread(&DatabaseConnector::getDataTimestamp, this, playbackThreadStatus.at(playbackThreadStatus.size() - 1),
			playbackRequest.carID, playbackRequest.startDate, playbackRequest.endDate,
			playbackRequest.startTime, playbackRequest.endTime, timeFactor);
		playbackThreadStatus.at(playbackThreadStatus.size() - 1)->store(false, std::memory_order_relaxed);


		return SUCCESS;
	}
	else
		return ERR_DATABASE;
}

int DatabaseConnector::getDataTimestamp(std::atomic<bool>* status, long carnum, long minDateValue, long maxDateValue, long minTimeValue, long maxTimeValue, double timeFactor) {
	//SELECT * FROM car# WHERE timestamp > # AND timestamp < #;  
	//Can add sorting of results with "ORDER BY timestamp;"
	if (databaseConnected == true) {
		mysql_thread_init();
		MYSQL_RES *result;
		MYSQL_ROW row;
		std::string carRowData[numKeys + 1];
		CarData* carDP;
		std::map<std::string, std::string> ::iterator iter;
		unsigned int num_fields;
		unsigned int i;
		int pass = 0;
		std::string KeyNameWithType = "";
		char typeCompChar;
		int length;
		long LData;
		unsigned long ULData;
		double DData;

		long prevCarTime(0), curCarTime(0), fetchTime;
		long long curTime(0), prevTime(0);

		std::string str1 = "SELECT * FROM car";
		if (carnum == std::numeric_limits<long>::min()) {
			carnum = 0;
		}
		else if (carnum < 0) {
			carnum = -carnum;
		}
		std::string str2 = std::to_string(carnum);
		std::string str3 = " WHERE DATE_S >= ";
		std::string str4 = std::to_string(minDateValue);
		std::string str5 = " AND DATE_S <= ";
		std::string str6 = std::to_string(maxDateValue);
		str6.append(" AND TIME_S >= ");
		str6.append(std::to_string(minTimeValue));
		str6.append(" AND TIME_S <= ");
		str6.append(std::to_string(maxTimeValue));
		std::string str7 = " ORDER BY TIME_S ASC;";
		std::string finalString = str1 + str2 + str3 + str4 + str5 + str6 + str7;
		pass = mysql_query(&mysql, finalString.c_str());
		if (pass == 0) {
			result = mysql_store_result(&mysql);
			num_fields = mysql_num_fields(result);//returns the number of valeus in a row
			while (row = mysql_fetch_row(result)) {//Retrieves the row of a result set. Returns NULL when no more rows
				unsigned long *lengths;
				lengths = mysql_fetch_lengths(result);
				for (i = 0; i < num_fields; i++) {
					carRowData[i] = row[i] ? row[i] : "NULL";
					//get data into multiple cardata objects and then use cashebank->feed to return car data objects
				}
				carSource->getCar(&carDP);
				//indert data that is not NULL and no need for uniqueID
				iter = keyMap.begin();
				for (i = 1; i < num_fields; i++) {
					if (carRowData[i] != "NULL" && carRowData[i] != "") {
						KeyNameWithType = iter->second;
						length = KeyNameWithType.length();
						typeCompChar = KeyNameWithType[length - 1];
						switch (typeCompChar) {
						case 'S': {//Long
							if (iter->second == "ID_S") {
								carDP->addKey(iter->first);
								LData = std::stol(carRowData[i], NULL, 10);
								LData = -LData;
								if (LData == 0) {
									LData = std::numeric_limits<long>::min();
								}
								carDP->set(iter->first, LData);
							}
							else {
								carDP->addKey(iter->first);
								LData = std::stol(carRowData[i], NULL, 10);
								carDP->set(iter->first, LData);
							}
							break;
						}
						case 'U': {//Unsigned Long
							carDP->addKey(iter->first);
							ULData = std::stoul(carRowData[i], NULL, 10);
							carDP->set(iter->first, ULData);
							break;
						}
						case 'D': {//Double
							carDP->addKey(iter->first);
							DData = std::stod(carRowData[i], NULL);
							carDP->set(iter->first, DData);
							break;
						}
						default: {
							wxLogDebug("GetData problem in the switch statement");
							break;
						}
						}
					}
					iter++;
				}
				prevTime = curTime;
				prevCarTime = curCarTime;
				carDP->get(TIME_S, &fetchTime);
				curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				wxLogDebug("Cur Time: %s", std::to_string(curTime));
				curCarTime = convertTimestamp(fetchTime);
				wxLogDebug("Left side %s", std::to_string(((double)(curTime - prevTime) * timeFactor)));
				wxLogDebug("Right side %s", std::to_string((double)(curCarTime - prevCarTime)));
				while (((double)(curTime - prevTime) * timeFactor) < (double)(curCarTime - prevCarTime)) {
					Sleep(1);
					curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				}
				wxLogDebug("Time diff: %s", std::to_string(curTime - prevTime));
				outputCache->feed(carDP);

				wxLogDebug("Logging at time %s", std::to_string(curTime));
				if (status->load(std::memory_order_relaxed)) {
					return SUCCESS;
				}
			}
			//mysql_free_result(tbl_cols);
			mysql_free_result(result);
			mysql_thread_end();
			status->store(true, std::memory_order_relaxed);

			long invertedCarnum = -carnum;
			if (invertedCarnum == 0) {
				invertedCarnum = std::numeric_limits<long>::min();
			}
			outputCache->endPlayback(invertedCarnum);

			return SUCCESS;
		}
		mysql_thread_end();
		status->store(true, std::memory_order_relaxed);
		return mysql_errno(&mysql);//Failed
	}
	else
		return ERR_DATABASE;
}