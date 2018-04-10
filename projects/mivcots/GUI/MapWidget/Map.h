#pragma once
#include <wx/wx.h>
#include <string.h>
#include "PictureWindow.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <wx/dcbuffer.h>



typedef struct coords_s {
	std::pair <double, double> southWest;
	std::pair <double, double> northEast;
}coords;

class Map
{
public:
	Map(wxWindow *parent);
	Map();
	~Map();

	bool initMap();
	wxPanel* getPanel();
	

	bool refresh();
	bool drawCar(double lat, double lon, double angle);
	coords getCoords();
	bool update();

	double angleTmp;
	double latTmp;
	double lonTmp;


private:
	
	wxPanel * panel;
	const wxImage *imgImg;
	wxBitmap *imgBitmap;
	PictureWindow * picWindow;
	std::string mapName;
	coords coordinates;
	wxBufferedDC* buffDC;
	wxMemoryDC* dc;

	double latFactor;
	double lonFactor;
	double latOffset;
	double lonOffset;

	bool createWidgets();
	bool getCoords(std::string mapName);
	void printCoords();
	bool calcFactors();
	
};

