#include <wx/wx.h>
#include <wx/image.h>

#include "PictureWindow.h"

wxBEGIN_EVENT_TABLE(PictureWindow, wxWindow)
EVT_ERASE_BACKGROUND(PictureWindow::onEraseBackground)
EVT_PAINT(PictureWindow::OnPaint)
wxEND_EVENT_TABLE()

PictureWindow::PictureWindow(wxWindow* parent, const wxImage& image) : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER)
{
	int width = image.GetWidth();
	int height = image.GetHeight();
	double X_Ratio = (double)MaxWidth / (double)width;
	double Y_Ratio = (double)MaxHeight / (double)height;
	double Ratio = X_Ratio < Y_Ratio ? X_Ratio : Y_Ratio;
	wxImage Image = image.Scale((int)(Ratio * width), (int)(Ratio * height));
	Bitmap = wxBitmap(Image);
	width = Image.GetWidth();
	height = Image.GetHeight();
	SetSize(width, height);
}

PictureWindow::PictureWindow(wxWindow* parent, const wxBitmap& image) : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSUNKEN_BORDER)
{
	int width = image.GetWidth();
	int height = image.GetHeight();
	Bitmap = image;
	width = image.GetWidth();
	height = image.GetHeight();
	SetSize(width, height);
}

void PictureWindow::OnPaint(wxPaintEvent& event)
{
	wxBufferedPaintDC dc(this);
	if (Bitmap.Ok())
	{
		dc.DrawBitmap(Bitmap, 0, 0);
	}
}

void PictureWindow::onEraseBackground(wxEraseEvent & event)
{
	//doing nothing on purpose
}

bool PictureWindow::setBitmap(wxBitmap img)
{
	Bitmap = img;
	return false;
}
