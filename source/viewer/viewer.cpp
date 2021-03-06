#include "viewer.hpp"

#include <sf2d.h>

/*----------------------------------------------------------*\
 |                       Viewer Class                       |
\*----------------------------------------------------------*/


	/*--------------------------------------------------*\
	 |             Constructor / Destructor             |
	\*--------------------------------------------------*/


// --------------------------------------------------
Viewer::Viewer(Viewer* parent)
// --------------------------------------------------
{
	this->parent = parent;
	this->child = NULL;

	if (this->parent)
	{
		this->parent->child = this;
	}
}


// --------------------------------------------------
Viewer::Viewer(ViewType vType, Viewer* parent) : Viewer(parent)
// --------------------------------------------------
{
	this->vType = vType;
}


// --------------------------------------------------
Viewer::~Viewer()
// --------------------------------------------------
{
	if (this->parent)
	{
		this->parent->child = NULL;
		this->parent = NULL;
	}

	delete child;
}


	/*--------------------------------------------------*\
	 |                  Viewer Methods                  |
	\*--------------------------------------------------*/


		/*------------------------------------------*\
		 |              Public Methods              |
		\*------------------------------------------*/


			/*----------------------------------*\
			 |          Viewer Methods          |
			\*----------------------------------*/


// --------------------------------------------------
Result Viewer::initialize()
// --------------------------------------------------
{
	return SUCCESS_STEP;
}


// --------------------------------------------------
Result Viewer::drawTopScreen()
// --------------------------------------------------
{
	return SUCCESS_STEP;
}


// --------------------------------------------------
Result Viewer::drawBotScreen()
// --------------------------------------------------
{
	return SUCCESS_STEP;
}


// --------------------------------------------------
Result Viewer::updateControls(const u32& kDown, const u32& kHeld, const u32& kUp, const touchPosition* touch)
// --------------------------------------------------
{
	return SUCCESS_STEP;
}


			/*----------------------------------*\
			 |        Hierarchy  Section        |
			\*----------------------------------*/


// --------------------------------------------------
bool Viewer::hasParent() { return this->parent; }
// --------------------------------------------------


// --------------------------------------------------
bool Viewer::hasChild() { return this->child; }
// --------------------------------------------------


// --------------------------------------------------
bool Viewer::isParent() { return this->child; }
// --------------------------------------------------


// --------------------------------------------------
bool Viewer::isChild() { return this->parent; }
// --------------------------------------------------


// --------------------------------------------------
bool Viewer::hasRegularChild()
// --------------------------------------------------
{
	return this->hasChild() && child->isRegular();
}


// --------------------------------------------------
bool Viewer::hasOverlayChild()
// --------------------------------------------------
{
	return this->hasChild() && child->isOverlay();
}


			/*----------------------------------*\
			 |          State  Methods          |
			\*----------------------------------*/


// --------------------------------------------------
bool Viewer::isRegular()
// --------------------------------------------------
{
	return vType == ViewType::Regular;
}


// --------------------------------------------------
bool Viewer::isOverlay()
// --------------------------------------------------
{
	return vType == ViewType::Overlay;
}


// --------------------------------------------------
void Viewer::setType(ViewType vType)
// --------------------------------------------------
{
	this->vType = vType;
}


			/*----------------------------------*\
			 |          State  Methods          |
			\*----------------------------------*/


// --------------------------------------------------
bool Viewer::isRunning()
// --------------------------------------------------
{
	return vState == ViewState::Running || vState == ViewState::Continuing;
}


// --------------------------------------------------
ViewState Viewer::close()
// --------------------------------------------------
{
	ViewState state = this->state();
	if (this->isChild()) delete this;
	return state;
}


// --------------------------------------------------
ViewState Viewer::state()
// --------------------------------------------------
{
	return this->vState;
}


// --------------------------------------------------
void Viewer::setState(ViewState vState)
// --------------------------------------------------
{
	this->vState = vState;
}


			/*----------------------------------*\
			 |          Utils  Methods          |
			\*----------------------------------*/


// --------------------------------------------------
bool Viewer::touchWithin(s16 px, s16 py, s16 x, s16 y, s16 w, s16 h)
// --------------------------------------------------
{
	return (px >= x && py >= y) && (px <= x + w && py <= y + h);
}


		/*------------------------------------------*\
		 |             Protected Methods            |
		\*------------------------------------------*/


		/*------------------------------------------*\
		 |              Private Methods             |
		\*------------------------------------------*/


/*----------------------------------------------------------*\
 |                      Static Section                      |
\*----------------------------------------------------------*/

// --------------------------------------------------
ViewState Viewer::startMainLoop(Viewer* viewer)
// --------------------------------------------------
{
	viewer->initialize();

	u32 kDown, kHeld, kUp;
	touchPosition touch;
	while(viewer->isRunning() && aptMainLoop())
	{
		hidScanInput();
		kDown = hidKeysDown();
		kHeld = hidKeysHeld();
		kUp = hidKeysUp();
		if ((kDown | kHeld) & KEY_TOUCH)
			hidTouchRead(&touch);

		sf2d_start_frame(GFX_TOP, GFX_LEFT);
			viewer->drawTopScreen();
		sf2d_end_frame();
		sf2d_start_frame(GFX_BOTTOM, GFX_LEFT);
			viewer->drawBotScreen();
		sf2d_end_frame();
		sf2d_swapbuffers();

		viewer->updateControls(kDown, kHeld, kUp, &touch);
	}

	return viewer->state();
}
