#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>
#include <istream>
#include <windows.h>
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "FTDI.h"

#include "MouseInput.h"
#include "TrackBird.h"

#include "Circle.h"
#include "DataWriter.h"
#include "HandCursor.h"
#include "Object2D.h"
#include "Path2D.h"
#include "Region2D.h"
#include "Sound.h"
#include "Timer.h"
#include "Image.h"

#include "config.h"

#include <gl/GL.h>
#include <gl/GLU.h>

/*
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "SDL.lib")
#pragma comment(lib, "SDLmain.lib")
#pragma comment(lib, "SDL_mixer.lib")
#pragma comment(lib, "SDL_ttf.lib")
#pragma comment(lib, "SDL_image.lib")
#pragma comment(lib, "Bird.lib")
#pragma comment(lib, "ftd2xx.lib")
#pragma comment(lib, "ATC3DG.lib")
*/
#pragma push(1)

//state machine
enum GameState
{
	Idle = 0x01,       //00001
	Starting = 0x03,   //00011
	Active = 0x06,     //00110
	Ending = 0x08, //01000
	Finished = 0x10    //10000
};



SDL_Event event;
SDL_Window *screen = NULL;
SDL_GLContext glcontext = NULL;

HandCursor* curs[BIRDCOUNT + 1];
HandCursor* player = NULL;
Circle* startCircle = NULL;
Circle* targCirclel = NULL;
Circle* targCircler = NULL;
Image* text = NULL;
Image* trialnum = NULL;
Sound* startbeep = NULL;
Sound* scorebeep = NULL;
Sound* errorbeep = NULL;
SDL_Color textColor = {0, 0, 0, 1};
DataWriter* writer = NULL;
GameState state;
Timer* trialTimer;
Timer* hoverTimer;
Timer* movTimer;

//Uint32 gameTimer;
//Uint32 hoverTimer;

//FTDI variables
FT_HANDLE ftHandle;
bool sensorsActive;
int curtrstatus;
int pasttrstatus;
int curtentrstatus;
int pasttentrstatus;
int newtrial;
int probe1status, probe2status;

//tracker variables
int trackstatus;
TrackSYSCONFIG sysconfig;
TrackDATAFRAME dataframe[BIRDCOUNT+1];
Uint32 DataStartTime = 0;


//colors
float redColor[3] = {1.0f, 0.0f, 0.0f};
float greenColor[3] = {0.0f, 1.0f, 0.0f};
float blueColor[3] = {0.0f, 0.0f, 1.0f};
float cyanColor[3] = {0.0f, 0.5f, 1.0f};
float grayColor[3] = {0.6f, 0.6f, 0.6f};
float blkColor[3] = {0.0f, 0.0f, 0.0f};
float whiteColor[3] = {1.0f, 1.0f, 1.0f};
float orangeColor[3] = {1.0f, 0.5f, 0.0f};
float *startColor = greenColor;
float *targColorl = blueColor;
float *targColorr = cyanColor;
float *targHitColor = grayColor;
float *cursColor = redColor;



// Trial table structure, to keep track of all the parameters for each trial of the experiment
typedef struct {
	//int TrialType;		// Flag 1-for trial type
	float startx,starty;	// x/y pos of start target; also, trace will be centered here!
	float xposl,yposl;		// x/y pos of target.
	float xposr, yposr;
} TRTBL;

#define TRTBL_SIZE 2
TRTBL trtbl [TRTBL_SIZE];

#define curtr trtbl[0]


//target structure; keep track of the target and other parameters, for writing out to data stream
TargetFrame Target;

// Initializes everything and returns true if there were no errors
bool init();
// Sets up OpenGL
void setup_opengl();
// Performs closing operations
void clean_up();
// Draws objects on the screen
void draw_screen();
//file to load in trial table
int LoadTrFile(char *filename);
// Update loop (state machine)
void game_update();

bool quit = false;  //flag to cue exit of program



int main(int argc, char* args[])
{
	int a = 0;
	UCHAR bit;

	//redirect stderr output to a file
	freopen( "./Debug/errorlog.txt", "w", stderr); 

	std::cerr << "Start main." << std::endl;

	SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
	//HIGH_PRIORITY_CLASS
	std::cerr << "Promote process priority to Above Normal." << std::endl;

	if (!init())
	{
		// There was an error during initialization
		std::cerr << "Initialization error." << std::endl;
		return 1;
	}
	
	DataStartTime = SDL_GetTicks();

	while (!quit)
	{
		int inputs_updated = 0;

		// Retrieve Flock of Birds data
		if (trackstatus>0)
		{
			// Update inputs from Flock of Birds
			inputs_updated = TrackBird::GetUpdatedSample(&sysconfig,dataframe);
		}

		// Handle SDL events
		while (SDL_PollEvent(&event))
		{
			// See http://www.libsdl.org/docs/html/sdlevent.html for list of event types
			if (event.type == SDL_MOUSEMOTION)
			{
				if (trackstatus <= 0)
				{
					MouseInput::ProcessEvent(event);
					inputs_updated = MouseInput::GetFrame(dataframe);

				}
			}
			else if (event.type == SDL_KEYDOWN)
			{
				// See http://www.libsdl.org/docs/html/sdlkey.html for Keysym definitions
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					quit = true;
				}
				else //if( event.key.keysym.unicode < 0x80 && event.key.keysym.unicode > 0 )
				{
					Target.key = *SDL_GetKeyName(event.key.keysym.sym);  //(char)event.key.keysym.unicode;
					//std::cerr << Target.flag << std::endl;
				}
			}
			else if (event.type == SDL_KEYUP)
			{
				Target.key = '0';
			}
			else if (event.type == SDL_QUIT)
			{
				quit = true;
			}
		}

		if ((state == Finished) && (trialTimer->Elapsed() >= 10000))
			quit = true;

		// Get data from input devices
		if (inputs_updated > 0) // if there is a new frame of data
		{

			if (sensorsActive) {
				//read the four lines
				bit = 1;
				curtrstatus = Ftdi::GetFtdiBitBang(ftHandle, bit);
				if (curtrstatus != pasttrstatus) {
					//Target.trcounter++;
					newtrial = 1;
				}

				bit = 2;
				curtentrstatus = Ftdi::GetFtdiBitBang(ftHandle, bit);
				if (curtentrstatus != pasttentrstatus) {
					Target.tentrcounter++;

				}

				bit = 3;
				probe1status = Ftdi::GetFtdiBitBang(ftHandle, bit);
				//Target.probe1 = probe1status;


				bit = 4;  //this is the CTS line
				probe2status = Ftdi::GetFtdiBitBang(ftHandle, bit);
				//Target.probe2 = probe2status;
			}

			//updatedisplay = true;
			for (int a = ((trackstatus>0) ? 1 : 0); a <= ((trackstatus>0) ? BIRDCOUNT : 0); a++)
			{
				if (dataframe[a].ValidInput)
				{
					curs[a]->UpdatePos(dataframe[a].x,dataframe[a].y);
					dataframe[a].vel = curs[a]->GetVel();
					writer->Record(a, dataframe[a], Target);
				}
			}

		}

		game_update(); // Run the game loop (state machine update)

		//if (updatedisplay)  //reduce number of calls to draw_screen -- does this speed up display/update?
		draw_screen();

	}

	clean_up();
	return 0;
}


//function to read in the name of the trial table file, and then load that trial table
int LoadTrFile(char* fname)
{

	//std::cerr << "LoadTrFile begin." << std::endl;

	char tmpline[100] = "";
	int ntrials = 0;

	//read in the trial file name
	std::ifstream trfile(fname);

	if (!trfile)
	{
		std::cerr << "Cannot open input file." << std::endl;
		return(-1);
	}
	else
		std::cerr << "Opened TrialFile " << TRIALFILE << std::endl;

	trfile.getline(tmpline, sizeof(tmpline), '\n');  //get the first line of the file, which is the name of the trial-table file

	while (!trfile.eof())
	{
		sscanf(tmpline, "%f %f %f %f %f %f",
			&trtbl[ntrials].startx, &trtbl[ntrials].starty,
			&trtbl[ntrials].xposl, &trtbl[ntrials].yposl,
			&trtbl[ntrials].xposr, &trtbl[ntrials].yposr);

		ntrials++;
		trfile.getline(tmpline, sizeof(tmpline), '\n');
	}

	trfile.close();
	if (ntrials == 0)
	{
		std::cerr << "Empty input file." << std::endl;
		//exit(1);
		return(-1);
	}
	return ntrials;
}

//initialization function - set up the experimental environment and load all relevant parameters/files
bool init()
{

	// Initialize Flock of Birds
	/* The program will run differently if the birds fail to initialize, so we
	 * store it in a bool.
	 */

	int a;
	char tmpstr[80];
	char fname[50] = TRIALFILE;
	//char dataPath[50] = DATA_OUTPUT_PATH;

	//std::cerr << "Start init." << std::endl;

	std::cerr << std::endl;

	trackstatus = TrackBird::InitializeBird(&sysconfig);
	if (trackstatus <= 0)
		std::cerr << "Tracker failed to initialize. Mouse Mode." << std::endl;

	std::cerr << std::endl;

	// Initialize SDL, OpenGL, SDL_mixer, and SDL_ttf
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		std::cerr << "SDL failed to intialize."  << std::endl;
		return false;
	}
	else
		std::cerr << "SDL initialized." << std::endl;


	screen = SDL_CreateWindow("Code Base SDL2",
		//SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,
		(WINDOWED ? SDL_WINDOWPOS_UNDEFINED : SDL_WINDOWPOS_CENTERED),(WINDOWED ? SDL_WINDOWPOS_UNDEFINED : SDL_WINDOWPOS_CENTERED),
		SCREEN_WIDTH, SCREEN_HEIGHT, 
		SDL_WINDOW_OPENGL | (WINDOWED ? 0 : (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_ALWAYS_ON_TOP) ) ); //SCREEN_BPP, //
	//note, the fullscreen option doesn't play nicely with the video window, so we will make a "fake" fullscreen which is simply a maximized window. 
	//Also, "borderless" doesn't play nicely either so we can't use that option, we just have to make the window large enough that the border falls outside the window.
	//To get full screen (overlaying the taskbar), we need to set the taskbar to auto-hide so that it stays offscreen. Otherwise it has priority and will be shown on top of the window.
	if (screen == NULL)
	{
		std::cerr << "Screen failed to build." << std::endl;
		return false;
	}
	else
	{
		//SDL_SetWindowBordered(screen, SDL_FALSE);
		SDL_Rect usable_bounds;
		int display_index = SDL_GetWindowDisplayIndex(screen);
		if (0 != SDL_GetDisplayUsableBounds(display_index, &usable_bounds)) 
			std::cerr << "SDL error getting usable screen bounds." << std::endl;
		
		SDL_SetWindowPosition(screen, usable_bounds.x, usable_bounds.y);
		SDL_SetWindowSize(screen, usable_bounds.w, usable_bounds.h);
		SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

		//create OpenGL context
		glcontext = SDL_GL_CreateContext(screen);
		std::cerr << "Screen built." << std::endl;
	}


	SDL_GL_SetSwapInterval(0); //ask for immediate updates rather than syncing to vertical retrace

	setup_opengl();

	a = Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 512);  //initialize SDL_mixer
	if (a != 0)
	{
		std::cerr << "Audio failed to initialize." << std::endl;
		return false;
	}
	else
		std::cerr << "Audio initialized." << std::endl;


	//initialize SDL_TTF (text handling)
	if (TTF_Init() == -1)
	{
		std::cerr << "SDL_TTF failed to initialize." << std::endl;
		return false;
	}
	else
		std::cerr << "SDL_TTF initialized." << std::endl;

	//turn off the computer cursor
	SDL_ShowCursor(0);

	std::cerr << std::endl;

	

	curtrstatus = 0;
	pasttrstatus = -1;
	Target.trcounter = 0;

	curtentrstatus = 0;
	pasttentrstatus = 0;
	Target.tentrcounter = -1;
	


	startCircle = new Circle(curtr.startx, curtr.starty, START_RADIUS*2, startColor);
	startCircle->SetBorderWidth(0.001f);
	startCircle->SetBorderColor(blkColor);
	startCircle->On();
	startCircle->BorderOn();

	targCirclel = new Circle(curtr.xposl, curtr.yposl, TARGET_RADIUS*2, targColorl);
	targCirclel->SetBorderColor(blkColor);
	targCirclel->SetBorderWidth(0.002f);
	targCirclel->BorderOn();
	targCirclel->Off();

	targCircler = new Circle(curtr.xposr, curtr.yposr, TARGET_RADIUS * 2, targColorr);
	targCircler->SetBorderColor(blkColor);
	targCircler->SetBorderWidth(0.002f);
	targCircler->BorderOn();
	targCircler->Off();

	
	//initialize the FTDI cable
	int status = -5;
	int devNum = 0;

	UCHAR Mask = 0x0f;  
	//the bits in the upper nibble should be set to 1 to be output lines and 0 to be input lines (only used 
	//  in SetSensorBitBang() ). The bits in the lower nibble should be set to 1 initially to be active lines.

	status = Ftdi::InitFtdi(devNum,&ftHandle,1,Mask);
	std::cerr << "FTDI: " << status << std::endl;

	Ftdi::SetFtdiBitBang(ftHandle,Mask,3,0);

	UCHAR dataBit;

	FT_GetBitMode(ftHandle, &dataBit);
	
	std::cerr << "DataByte: " << std::hex << dataBit << std::dec << std::endl;
	
	if (status==0)
	{
		printf("FTDI found and opened.\n");
		sensorsActive = true;
	}
	else
	{
		if (status == 1)
			std::cerr << "   Failed to create device list." << std::endl;
		else if (status == 2)
			std::cerr << "   Sensor ID=" << devNum << " not found." << std::endl;
		else if (status == 3)
			std::cerr << "   Sensor " << devNum << " failed to open." << std::endl;
		else if (status == 4)
			std::cerr << "   Sensor " << devNum << " failed to start in BitBang mode." << std::endl;
		else
			std::cerr << "UNDEFINED ERROR!" << std::endl;

		sensorsActive = false;
	}

	std::cerr << std::endl;



	//load trial table from file
	int NTrials = LoadTrFile(fname);
	//std::cerr << "Filename: " << fname << std::endl;

	if (NTrials == -1)
	{
		std::cerr << "Trial File did not load." << std::endl;
		return false;
	}
	else
		std::cerr << "Trial File loaded: " << NTrials << " trials found." << std::endl;


	//assign the data-output file name based on the trial-table name 
	//std::string savfile ("rhi_reach_data");
	std::string savfile;
	savfile.assign(fname);
	savfile.insert(savfile.rfind("."), "_data");

	std::strcpy(fname, savfile.c_str());

	std::cerr << "SavFileName: " << fname << std::endl;

	writer = new DataWriter(&sysconfig,fname);  //set up the data-output file


	// set up the cursors
	if (trackstatus > 0)
	{
		/* Assign birds to the same indices of controller and cursor that they use
		* for the Flock of Birds
		*/
		for (a = 1; a <= BIRDCOUNT; a++)
		{
			curs[a] = new HandCursor(curtr.startx, curtr.starty, CURSOR_RADIUS*2, cursColor);
			curs[a]->BorderOff();
			curs[a]->SetOrigin(curtr.startx, curtr.starty);
		}

		player = curs[HAND];  //this is the cursor that represents the hand

	}
	else
	{
		// Use mouse control
		curs[0] = new HandCursor(curtr.startx, curtr.starty, CURSOR_RADIUS*2, cursColor);
		curs[0]->SetOrigin(curtr.startx, curtr.starty);
		player = curs[0];
	}


	player->On();

	

	//load sound files
	char sndbeep[24] = "Resources/startbeep.wav";
	char sndcoin[19] = "Resources/coin.wav";
	char snderr[25] = "Resources/errorbeep1.wav";
	startbeep = new Sound(sndbeep);
	scorebeep = new Sound(sndcoin);
	errorbeep = new Sound(snderr);

	//set up placeholder text
	text = Image::ImageText(text, "DONE.","arial.ttf", 28, textColor);
	text->Off();

	//set up trial number text image
	trialnum = Image::ImageText(trialnum,"1","arial.ttf", 18,textColor);
	trialnum->On();

	hoverTimer = new Timer();
	trialTimer = new Timer();
	movTimer = new Timer();
	
	// Set the initial game state
	state = Idle; 

	std::cerr << "initialization complete." << std::endl;
	return true;
}


static void setup_opengl()
{
	glClearColor(1, 1, 1, 0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	/* The default coordinate system has (0, 0) at the bottom left. Width and
	 * height are in meters, defined by PHYSICAL_WIDTH and PHYSICAL_HEIGHT
	 * (config.h). If MIRRORED (config.h) is set to true, everything is flipped
	 * horizontally.
	 */
	glOrtho(MIRRORED ? PHYSICAL_WIDTH : 0, MIRRORED ? 0 : PHYSICAL_WIDTH,
		0, PHYSICAL_HEIGHT, -1.0f, 1.0f);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);

}


//end the program; clean up everything neatly.
void clean_up()
{
	delete startCircle;
	delete targCirclel;
	delete targCircler;
	delete scorebeep;
	delete errorbeep;
	

	int status = Ftdi::CloseFtdi(ftHandle,1);

	delete text;
	delete trialnum;

	delete writer;


	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(screen);

	Mix_CloseAudio();
	TTF_Quit();
	SDL_Quit();
	if (trackstatus > 0)
		TrackBird::ShutDownBird(&sysconfig);

	freopen( "CON", "w", stderr );

}

//control what is drawn to the screen
static void draw_screen()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	
	// Draw the start marker, if true
	startCircle->Draw();
	if (startCircle->drawState())
	{
		Target.startx = startCircle->GetX();
		Target.starty = startCircle->GetY();
	}
	else
	{
		Target.startx = -100;
		Target.starty = -100;
	}


	// Draw the target marker for the current trial, if true
	targCirclel->Draw();
	if (targCirclel->drawState())
	{
		// Marker is stretched to the activation radius
		Target.tgtxl = targCirclel->GetX();
		Target.tgtyl = targCirclel->GetY();
	}
	else
	{
		Target.tgtxl = -100;
		Target.tgtyl = -100;
	}

	targCircler->Draw();
	if (targCircler->drawState())
	{
		// Marker is stretched to the activation radius
		Target.tgtxr = targCircler->GetX();
		Target.tgtyr = targCircler->GetY();
	}
	else
	{
		Target.tgtxr = -100;
		Target.tgtyr = -100;
	}



	player->Draw();


	// Draw text - provide feedback at the end of the block
	text->Draw(0.6f, 0.5f);

	//write the trial number
	trialnum->Draw(PHYSICAL_WIDTH*23/24, PHYSICAL_HEIGHT*23/24);

	SDL_GL_SwapWindow(screen);
	glFlush();

}


//game update loop - state machine controlling the status of the experiment
bool hitTargetl = false;
bool hitTargetr = false;



void game_update()
{

	switch (state)
	{
		case Idle:
			/* If newtrial flag is set for first time - to avoid double-counting trials upon initialization
			 */

			Target.trcounter = 0;
			Target.trial = 0;

			startCircle->SetPos(curtr.startx, curtr.starty);
			startCircle->On();
			targCirclel->Off();
			targCircler->Off();


			if( newtrial == 1)
			{
				hoverTimer->Reset();
				trialTimer->Reset();
				
				newtrial = 0;

				std::cerr << "Leaving IDLE state." << std::endl;
				
				state = Starting;
			}
			break;
		case Starting: 
			/* Wait for probe flags to change
			 */

			//make sure newtrial flag stays off until some minimum time has elapsed to avoid mis-counts
			if (trialTimer->Elapsed() < 500)
				newtrial = 0;

			startCircle->On();
			startCircle->SetColor(startColor);
			targCirclel->Off();
			targCircler->Off();

			// check if probe flags are set, or if newtrial flag is set
			if (probe1status > 0) {
				Target.probe1 = probe1status;
				Target.probe2 = probe2status;

				if (probe2status == 0) {
					targCirclel->SetPos(curtr.xposl, curtr.yposl);
					targCirclel->SetColor(targColorl);
					targCirclel->On();
				}

				if (probe2status == 1) {
					targCirclel->SetPos(curtr.xposl, curtr.yposl);
					targCirclel->SetColor(targColorl);
					targCirclel->On();

					targCircler->SetPos(curtr.xposr, curtr.yposr);
					targCircler->SetColor(targColorr);
					targCircler->On();

				}

				hitTargetl = false;
				hitTargetr = false;

				startbeep->Play();

				newtrial = 0;

				std::cerr << "Leaving STARTING state to Active probe state." << std::endl;
				state = Active;

			}
			else if (newtrial == 1) {
				//Target.trcounter++;
				//Target.trial++;

				scorebeep->Play();

				trialTimer->Reset();

				std::cerr << "Leaving STARTING state to Ending state." << std::endl;
				state = Ending;
			}


			break;

		case Active:
			

			
			//check if the player hit the target
			if (player->HitTarget(targCirclel))
			{
				hitTargetl = true;
				targCirclel->SetColor(targHitColor);
			}

			if (player->HitTarget(targCircler))
			{
				hitTargetr = true;
				targCircler->SetColor(targHitColor);
			}

			
			if (newtrial == 1) {
				//Target.trcounter++;
				//Target.trial++;

				scorebeep->Play();

				trialTimer->Reset();

				std::cerr << "Leaving ACTIVE state to Ending state." << std::endl;
				state = Ending;
			}

			
			break;

		case Ending:


			targCirclel->Off();
			targCircler->Off();


			if (trialTimer->Elapsed() > 500) {

				newtrial = 0;
				Target.trcounter++;
				Target.trial++;

				//if we have exceeded NTRIALS, quit
				if (Target.trial >= NTRIALS)
				{
					std::cerr << "Leaving ENDING state to Finished state." << std::endl;
					trialTimer->Reset();
					state = Finished;
				}
				else
				{
					hoverTimer->Reset();
					std::cerr << "Leaving ACTIVE state to Starting state." << std::endl;
					state = Starting;
				}
			}

			break;

		case Finished:
			// Trial table ended, wait for program to quit

			startCircle->Off();
			targCirclel->Off();
			targCircler->Off();

			//provide the score at the end of the block.
			text->On();

			if (trialTimer->Elapsed() > 5000)
				quit = true;


			break;

	}
}

