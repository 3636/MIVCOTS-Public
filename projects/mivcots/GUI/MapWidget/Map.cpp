#include "Map.h"

Map::Map(wxWindow *parent)
{
	this->parent = parent;
}

Map::Map()
{
}

Map::~Map()
{
}

bool Map::initMap(MIVCOTS* aMIVCOTS, std::vector<long>* activeCars, double* baseLat, double* baseLon)
{
	panel = new wxPanel(parent, wxID_ANY);
	this->activeCars = activeCars;
	this->baseLat = baseLat;
	this->baseLon = baseLon;

	mapName = "map1";
	wxImage::AddHandler(new wxPNGHandler);
	
	this->aMIVCOTS = aMIVCOTS;
	if (const char* env_p = std::getenv("MivcotsResources")) {
		wxLogMessage("Resources found at");
		wxLogMessage(_(env_p));
		std::string carPath = std::string(env_p) + std::string("maps\\car1.png");
		carimg1 = new wxImage(carPath, wxBITMAP_TYPE_ANY);
		carPath = std::string(env_p) + std::string("maps\\car2.png");
		carimg2 = new wxImage(carPath, wxBITMAP_TYPE_ANY);
		std::string filePath = std::string(env_p) + std::string("maps\\base.png");
		baseStationimg = new wxImage(filePath, wxBITMAP_TYPE_ANY);
		filePath = std::string(env_p) + std::string("maps\\alphacar2.png");
		alphaImg1 = new wxImage(filePath, wxBITMAP_TYPE_ANY);
		filePath = std::string(env_p) + std::string("maps\\alphacar2.png");
		alphaImg2 = new wxImage(filePath, wxBITMAP_TYPE_ANY);

		std::string mapPath = std::string(env_p) + std::string("maps/") + mapName + std::string(".png");
		//probably change to tmp
		//imgImg =  new wxImage(mapPath, wxBITMAP_TYPE_PNG);
		wxImage *tmpImg = new wxImage(mapPath, wxBITMAP_TYPE_PNG);
		int xSize = wxSystemSettings::GetMetric(wxSYS_SCREEN_X);
		int ySize = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y);
		wxLogMessage("Map Dimensions: x size = %d\ty size = %d", xSize,ySize);
		double MaxWidth = xSize * 0.66;
		double MaxHeight = ySize * 0.66;
		
		double X_Ratio = (double)MaxWidth / (double)tmpImg->GetWidth();
		double Y_Ratio = (double)MaxHeight / (double)tmpImg->GetHeight();
		double Ratio = X_Ratio < Y_Ratio ? X_Ratio : Y_Ratio;
		tmpImg->Rescale((int)(Ratio * tmpImg->GetWidth()), (int)(Ratio * tmpImg->GetHeight()), wxIMAGE_QUALITY_HIGH);
		imgImg = (const wxImage*)tmpImg;

		imgBitmap = new wxBitmap(*imgImg);
		picWindow = new PictureWindow(panel, *imgBitmap);

		getCoords(mapName);
		//printCoords();
		calcFactors();
		dc = new wxMemoryDC(*imgBitmap);
		buffDC = new wxBufferedDC(dc, *imgBitmap);

		angleTmp = 0;
		latTmp = 32.235744;
		lonTmp = -110.953771;

	}
	else {
		wxLogFatalError("NO ENVIRONEMENT VARIABLE FOR RESOURCES");
	}

	return true;
}

wxPanel * Map::getPanel()
{
	return panel;
}

//static bool isPrinted = false;
//#include <ctime>
bool Map::refresh()
{
	panel->Refresh();
	//if (!isPrinted) {
	//	auto time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	//	wxLogMessage("Displayed data for a playback request at %s", ctime(&time));
	//	isPrinted = true;
	//}
	
	return true;
}

bool Map::drawCar(double lat, double lon, double angle, int carID)
{
	wxImage tmpimg;
	if (carID == 0) {
		//tmpimg = baseStationimg->Copy();
		wxPoint center = wxPoint(baseStationimg->GetWidth() / 2, baseStationimg->GetHeight() / 2);
		//tmpimg = baseStationimg->Rotate(angle, center);
	}
	else if (carID == -1) {
		//tmpimg = alphaImg->Copy();
		wxPoint center = wxPoint(alphaImg1->GetWidth() / 2, alphaImg1->GetHeight() / 2);
		tmpimg = alphaImg1->Rotate(angle, center);
	}
	else if (carID == -2) {
		//tmpimg = alphaImg->Copy();
		wxPoint center = wxPoint(alphaImg2->GetWidth() / 2, alphaImg2->GetHeight() / 2);
		tmpimg = alphaImg2->Rotate(angle, center);
	}
	else if (carID == 1) {
		//tmpimg = carimg->Copy();
		wxPoint center = wxPoint(carimg1->GetWidth() / 2, carimg1->GetHeight() / 2);
		tmpimg = carimg1->Rotate(angle, center);
	}
	else if (carID == 2) {
		//tmpimg = carimg->Copy();
		wxPoint center = wxPoint(carimg2->GetWidth() / 2, carimg2->GetHeight() / 2);
		tmpimg = carimg2->Rotate(angle, center);
	}
	else {
		return false;
	}
	//tmpimg = tmpimg.Rotate(angle, center);

	double x = (lon - lonOffset)*lonFactor;
	double y = -(lat - latOffset)*latFactor;

	//wxLogMessage("x=%lf\ty=%lf", x, y);
	wxBitmap tmp = wxBitmap(tmpimg);

		
	picWindow->setBitmap(*imgBitmap);
		
	//translating to the center of the image
	x -= tmp.GetHeight() / 2;
	y -= tmp.GetWidth() / 2;
	buffDC->SelectObject(*imgBitmap);
	buffDC->DrawBitmap(tmp, x, y, true);

	buffDC->SelectObject(wxNullBitmap);

	picWindow->setBitmap(*imgBitmap);
	//free(buffDC);
	//}
	return false;
}

int Map::mapRefresh() {
	picWindow->setBitmap(*imgBitmap);
	refresh();
	return SUCCESS;
}

coords Map::getCoords()
{
	return coordinates;
}

bool Map::update()
{
	*imgBitmap = wxBitmap(*imgImg);
	
	sharedCache<CarData*>::cacheIter iter; 
	if (activeCars->size() == 0) {
		mapRefresh();
	}
	for (int i : *activeCars) {
		double lat = -1;
		double lon = -1;
		double course = -1;
		int rc = SUCCESS;
		std::shared_lock<std::shared_mutex> toLock;
		//wxLogDebug("Map is trying to read cache");
		rc = aMIVCOTS->acquireReadLock(i, &toLock);

		if (rc != SUCCESS) {
			//wxLogDebug("Couldn't read cache for car %d", i);
			aMIVCOTS->endCacheRead(i, &toLock);
			continue;
		}

		rc = aMIVCOTS->readLatestUpdateGreaterThan(i, &iter, 1); // TODO: don't hardcode the update count
		if (rc == SUCCESS) {
			if (((*iter)->get(LON_D, &lon) | (*iter)->get(LAT_D, &lat) | (*iter)->get(HEADING_D, &course)) != SUCCESS) {
			}
			else {
				if (i == 0) {
					*baseLat = lat;
					*baseLon = lon;
				}
				drawCar(lat, lon, course * 0.01745329252, i);

				refresh();
			}
		}
		else {
			//wxLogDebug("Couldn't read updates from cache for car %d", i);
		}

		aMIVCOTS->endCacheRead(i, &toLock);
		//wxLogDebug("Map released cache read");
	}
	return true;
}

bool Map::createWidgets()
{


	return true;
}

bool Map::getCoords(std::string mapName)
{
	if (const char* env_p = std::getenv("MivcotsResources")) {
		std::string coordsPath = std::string(env_p) + std::string("maps/") + mapName + std::string("_sw_ne_coords.txt");

		std::ifstream inFile(coordsPath);
		std::string value;
		int i = 0;
		while (inFile.good()) {
			getline(inFile, value, ',');
			switch (i) {
			case 0:
				coordinates.southWest.first = std::stod(value);

				break;
			case 1:
				coordinates.southWest.second = std::stod(value);
				break;
			case 2:
				coordinates.northEast.first = std::stod(value);
				break;
			case 3:
				coordinates.northEast.second = std::stod(value);
				break;

			}
			i++;
		}
	}
	return true;
}

void Map::printCoords()
{
	std::stringstream wss;
	wss << std::setprecision(std::numeric_limits<double>::digits10 + 1);

	wss << coordinates.southWest.first;
	wxLogMessage(_(wss.str()));
	wss.str("");

	wss << coordinates.southWest.second;
	wxLogMessage(_(wss.str()));
	wss.str("");

	wss << coordinates.northEast.first;
	wxLogMessage(_(wss.str()));
	wss.str("");

	wss << coordinates.northEast.second;
	wxLogMessage(_(wss.str()));
	wss.str("");
}

bool Map::calcFactors()
{
	latFactor = (imgImg->GetHeight()) / (coordinates.northEast.first - coordinates.southWest.first);
	lonFactor = (imgImg->GetWidth()) / (coordinates.northEast.second - coordinates.southWest.second);

	latOffset = coordinates.northEast.first;
	lonOffset = coordinates.southWest.second;

	return true;
}
