#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <list>

#include <3ds.h>
#include <citro2d.h>

#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#pragma GCC diagnostic pop

#include "rapidjson/stringbuffer.h"
#include <iostream>
#include <sys/stat.h>
#include <vector>
#include <fstream>

using namespace rapidjson;

struct fileBuf {
	u8 *buf = 0;
	u32 size = 0;
};

// struct episode {
// 	char *url;
// 	char *title;
// 	tm release;
// };

// struct podcast {
// 	char* title;
// 	char* description;
// 	SizeType episodesCount;
// 	episode* episodes;
// };

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240

C3D_RenderTarget* top;

u32 clrWhite = C2D_Color32(0xFF, 0xFD, 0xD0, 0xFF);
u32 clrGreen = C2D_Color32(0x00, 0xFF, 0x00, 0xFF);
u32 clrRed   = C2D_Color32(0xFF, 0x00, 0x00, 0xFF);
u32 clrBlue  = C2D_Color32(0x00, 0x00, 0xFF, 0xFF);

std::string defaultJSON = "{\"savedPodcasts\":[{\"Name\":\"Offbeat Oregon\",\"URL\":\"http://feeds.feedburner.com/OffbeatOregonHistory\"}]}";

C2D_Text title;
C2D_TextBuf titleBuf  = C2D_TextBufNew(100);
C2D_Text menu;
C2D_TextBuf menuBuf = C2D_TextBufNew(4096);

int cursor = 0;
int menuLength = 2;
int selectedPodcast = 0;
int selectedEpisode = 0;
enum menuType {
	InitialMenu,
	ViewSavedPodcasts,
	PodcastOptions,
	ListEpisodes
};

menuType currentMenu = InitialMenu;

char* menuString;

std::string titleText = "JCatcher";

std::string initialMenuText = \
		"Add new podcast URL\n"\
		"View Saved Podcasts";

std::string podcastOptionsText = \
		"Check Episodes\n"\
		"Edit\n"\
		"Remove";

int retCode = 0;
std::vector<std::string> Names;
std::vector<std::string> URLs;
std::vector<std::string> EpisodeNames;
std::vector<std::string> EpisodeURLs;

int http_download(std::string url, char* &fileBuf, int &finalSize) {
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
	u32 statuscode=0;
	u32 contentsize=0, readsize=0, size=0;
	u8 *buf, *lastbuf;

	// std::cout << __LINE__ << ": Downloading " << url << std::endl;

	do {
		ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url.c_str(), 1);
		if (ret != 0) std::cout << __LINE__ << ": return from httpcOpenContext: " << ret << std::endl;

		// This disables SSL cert verification, so https:// will be usable
		ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
		if (ret != 0) std::cout << __LINE__ << ": return from httpcSetSSLOpt: " << ret << std::endl;

		// Enable Keep-Alive connections
		ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
		if (ret != 0) std::cout << __LINE__ << ": return from httpcSetKeepAlive: " << ret << std::endl;

		// Set a User-Agent header so websites can identify your application
		ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
		if (ret != 0) std::cout << __LINE__ << ": return from httpcAddRequestHeaderField: " << ret << std::endl;

		// Tell the server we can support Keep-Alive connections.
		// This will delay connection teardown momentarily (typically 5s)
		// in case there is another request made to the same server.
		ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
		if (ret != 0) std::cout << __LINE__ << ": return from httpcAddRequestHeaderField: " << ret << std::endl;

		ret = httpcBeginRequest(&context);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			std::cout << __LINE__ << ": return from httpcBeginRequest: " << ret << std::endl;
			return ret;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			std::cout << __LINE__ << ": return from httpcGetResponseStatusCode: " << ret << std::endl;
			return ret;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = (char*)malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				return -1;
			}
			ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
			url = newurl; // Change pointer to the url that we just learned
			std::cout << __LINE__ << ": redirecting to url: " << url << std::endl;
			httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

	if(statuscode!=200){
		std::cout << __LINE__ << ": URL returned status: " << statuscode << std::endl;
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -2;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		std::cout << __LINE__ << ": return from httpcGetDownloadSizeState: " << ret << std::endl;
		return ret;
	}

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return -1;
	}

	do {
		// This download loop resizes the buffer as data is read.
		ret = httpcDownloadData(&context, buf+size, 0x1000, &readsize);
		size += readsize; 
		if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING){
				lastbuf = buf; // Save the old pointer, in case realloc() fails.
				buf = (u8*)realloc(buf, size + 0x1000);
				if(buf==NULL){ 
					httpcCloseContext(&context);
					free(lastbuf);
					if(newurl!=NULL) free(newurl);
					return -1;
				}
			}
			std::cout << "#";
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		return -1;
	}

	// Resize the buffer back down to our actual final size
	size++;
	lastbuf = buf;
	buf = (u8*)realloc(buf, size);
	if (buf == NULL) { // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if (newurl != NULL) free(newurl);
		return -1;
	}

	fileBuf = (char*) buf;
	finalSize = size;

	httpcCloseContext(&context);
	if (newurl!=NULL) free(newurl);
	return 0;
}

void printParseError(ParseErrorCode parseError) {
	switch (parseError) {
	case kParseErrorDocumentEmpty:
		std::cout << "Parser error kParseErrorDocumentEmpty" << std::endl;
		break;
	case kParseErrorDocumentRootNotSingular:
		std::cout << "Parser error kParseErrorDocumentRootNotSingular" << std::endl;
		break;
	case kParseErrorValueInvalid:
		std::cout << "Parser error kParseErrorValueInvalid" << std::endl;
		break;
	case kParseErrorObjectMissName:
		std::cout << "Parser error kParseErrorObjectMissName" << std::endl;
		break;
	case kParseErrorObjectMissColon:
		std::cout << "Parser error kParseErrorObjectMissColon" << std::endl;
		break;
	case kParseErrorObjectMissCommaOrCurlyBracket:
		std::cout << "Parser error kParseErrorObjectMissCommaOrCurlyBracket" << std::endl;
		break;
	case kParseErrorArrayMissCommaOrSquareBracket:
		std::cout << "Parser error kParseErrorArrayMissCommaOrSquareBracket" << std::endl;
		break;
	case kParseErrorStringUnicodeEscapeInvalidHex:
		std::cout << "Parser error kParseErrorStringUnicodeEscapeInvalidHex " << std::endl;
		break;
	case kParseErrorStringUnicodeSurrogateInvalid:
		std::cout << "Parser error kParseErrorStringUnicodeSurrogateInvalid" << std::endl;
		break;
	case kParseErrorStringEscapeInvalid:
		std::cout << "Parser error kParseErrorStringEscapeInvalid" << std::endl;
		break;
	case kParseErrorStringMissQuotationMark:
		std::cout << "Parser error kParseErrorStringMissQuotationMark" << std::endl;
		break;
	case kParseErrorStringInvalidEncoding:
		std::cout << "Parser error kParseErrorStringInvalidEncoding" << std::endl;
		break;
	case kParseErrorNumberTooBig:
		std::cout << "Parser error kParseErrorNumberTooBig" << std::endl;
		break;
	case kParseErrorNumberMissFraction:
		std::cout << "Parser error kParseErrorNumberMissFraction" << std::endl;
		break;
	case kParseErrorNumberMissExponent:
		std::cout << "Parser error kParseErrorNumberMissExponent" << std::endl;
		break;
	case kParseErrorTermination:
		std::cout << "Parser error kParseErrorTermination" << std::endl;
		break;
	case kParseErrorUnspecificSyntaxError:
		std::cout << "Parser error kParseErrorUnspecificSyntaxError" << std::endl;
		break;
	default:
		std::cout << "Parser error fell throught to OTHER" << std::endl;
	}
}

void showFileBuf(fileBuf &fileBuf) {
// 	u32 size = fileBuf.size;
// 	if(size>(240*400*3*2))size = 240*400*3*2;

// 	u8* framebuf;
// 	framebuf = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
// 	memcpy(framebuf, fileBuf.buf, size);

// 	gfxFlushBuffers();
// 	gfxSwapBuffers();

// 	framebuf = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
// 	memcpy(framebuf, fileBuf.buf, size);

// 	gfxFlushBuffers();
// 	gfxSwapBuffers();
// 	gspWaitForVBlank();
}

int saveFileBuf(fileBuf &fileBuf, const char* fileName) {
	struct stat st = {0};

	if (stat("/3ds/JCatch/", &st) == -1) {
		mkdir("/3ds/JCatch/", 0777);
	}
	std::string filePath = "/3ds/JCatch/";
	filePath.append(fileName);
	FILE* file = fopen(filePath.c_str(), "w");
	if (file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	fwrite(fileBuf.buf, 1, fileBuf.size, file);
	fclose(file);
	return 0;
}

int saveFileBuf(std::string fileContents, const char* fileName) {
	struct stat st = {0};
	if (stat("/3ds/JCatch/", &st) == -1) {
		mkdir("/3ds/JCatch/", 0777);
	}
	std::string filePath = "/3ds/JCatch/";
	filePath.append(fileName);
	std::cout << "Writing: " << filePath << std::endl;
	std::ofstream myfile;
	myfile.open(filePath, std::fstream::out | std::fstream::trunc);
	if (!myfile.is_open()) {
		return -1;
	}
	myfile << fileContents << std::endl;
	myfile.close();
	return 0;
}

int loadFileBuf(std::string &fileContents, const char* fileName) {
	struct stat st = {0};

	if (stat("/3ds/JCatch/", &st) == -1) {
		mkdir("/3ds/JCatch/", 0777);
	}
	std::string filePath = "/3ds/JCatch/";
	filePath.append(fileName);
	FILE* file = fopen(filePath.c_str(), "r");
	if (file == NULL) {
		return -1;
	}
	int length = 0;
	while (!feof(file)) {
		fileContents.append(1, fgetc(file));
		length++;
	}
	length--;
	fileContents = fileContents.substr(0,length);
	return 0;
}

int parseFileBuf(char* fileBuf, Document &document) {
	document.ParseInsitu(fileBuf);
	if (document.HasParseError()) {
		printParseError(document.GetParseError());
		return -1;
	}
	return 0;
}

void holdForExit() {
	std::cout << "Press start to exit" << std::endl;
	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START) break; // break in order to return to hbmenu
	}
	
	gfxExit();
}

tm parse8601(const char* dateTimeString) {
	// https://stackoverflow.com/a/26896792
	int year,month,day,hour,minute;
	float second;
	sscanf(dateTimeString, "%d-%d-%dT%d:%d:%fZ", &year, &month, &day, &hour, &minute, &second);
	tm dateTime = { 0 };
	dateTime.tm_year = year - 1900;
	dateTime.tm_mon = month - 1;
	dateTime.tm_mday = day;
	dateTime.tm_hour = hour;
	dateTime.tm_min = minute;
	dateTime.tm_sec = (int)second;
	return dateTime;
}

int parsePodcast(Document &document) {
	if (!document.IsObject()) 									return -1;
	if (!document.HasMember("title"))							return -2;
	if (!document["title"].IsString())							return -3;
	if (!document.HasMember("description"))						return -4;
	if (!document["description"].IsString())					return -5;
	if (!document.HasMember("items"))							return -6;
	const Value& items = document["items"];
	if (!items.IsArray())										return -7;
	if (items.Empty())											return -8;
	SizeType itemCount = items.Size();

	for (SizeType i = 0; i < itemCount; i++) {
		const Value& curItem = items[i];
		
		if (!curItem.HasMember("title"))						return (-i*10)-1;
		if (!curItem["title"].IsString())						return (-i*10)-2;
		EpisodeNames.push_back(curItem["title"].GetString());

		if (!curItem.HasMember("url"))							return (-i*10)-3;
		if (!curItem["url"].IsString())							return (-i*10)-4;
		EpisodeURLs.push_back(curItem["url"].GetString());
	}

	return 0;
}

void stringToBuffer(std::string string, char* &buffer) {
	const char* cstr = string.c_str();
	int contentSize = strlen(cstr);
	buffer = (char*)malloc(contentSize+1);
	memcpy(buffer, cstr, contentSize);
	memset(buffer+contentSize, 0, 1);
}

void stringListToMenu(std::vector<std::string> stringList, int amount) {
	std::string output = "";
	menuLength = std::fmin(stringList.size(), amount);
	for (int i = 0; i < menuLength; i++) {
		output.append(stringList[i].substr(0, 65));
		output.append("\n");
	}
	free(menuString);
	stringToBuffer(output, menuString);
}

int fetchPodcasts() {
	// Populates EpisodeNames and EpisodeURLs
	EpisodeNames = {};
	EpisodeURLs = {};
	std::cout << "Fetching episodes for " << Names[selectedPodcast] << std::endl;
	char* downloadedFile;
	std::string targetURL = "http://jimmytech.net/RSS2JSON/?url=";
	targetURL.append(URLs[selectedPodcast]);
	int size;
	retCode = http_download(targetURL, downloadedFile, size);
	if (retCode != 0) {
		std::cout << "HTTP download failed: " << retCode << std::endl;
		return -1;
	}
	Document RSSFeed;
	retCode = parseFileBuf(downloadedFile, RSSFeed);
	if (retCode != 0) {
		std::cout << "JSON Parser failed: " << retCode << std::endl;
		return -2;
	}

	retCode = parsePodcast(RSSFeed);
	if (retCode != 0) {
		std::cout << "Podcast Parser failed: " << retCode << std::endl;
		return -3;
	}
	free(downloadedFile);
	return 0;
}

void updateMenu() {
	// std::cout << "Current Menu: " << currentMenu << std::endl;
	if (currentMenu == InitialMenu) {
		free(menuString);
		stringToBuffer(initialMenuText, menuString);
		menuLength = 2;
		titleText = "JCatcher";
	} else if (currentMenu == ViewSavedPodcasts) {
		stringListToMenu(Names, 15);
		titleText = "Podcasts";
	} else if (currentMenu == PodcastOptions) {
		free(menuString);
		stringToBuffer(podcastOptionsText, menuString);
		menuLength = 3;
		titleText = Names[selectedPodcast];
	} else if (currentMenu == ListEpisodes) {
		stringListToMenu(EpisodeNames, 15);
		titleText = Names[selectedPodcast];
	}
	cursor = 0;
}

void setupGraphics() {
	gfxInitDefault();
	httpcInit(0); // Buffer size when POST/PUT.

	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	
	consoleInit(GFX_BOTTOM,NULL);
	top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

	currentMenu = InitialMenu;
	updateMenu();
}

void cleanupGraphics() {
	C2D_TextBufDelete(titleBuf);
	C2D_TextBufDelete(menuBuf);
	C2D_Fini();
	C3D_Fini();
	gfxExit();
}

void drawUI() {
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(top, clrWhite);
	C2D_SceneBegin(top);
	C2D_TextBufClear(titleBuf);
	C2D_TextBufClear(menuBuf);

	// Draw the title
	C2D_TextParse(&title, titleBuf, titleText.c_str());
	C2D_TextOptimize(&title);
	C2D_DrawText(&title, 0, 10, 10, 0, 0.7, 0.7);

	// Draw the menu
	C2D_TextParse(&menu, menuBuf, menuString);
	C2D_TextOptimize(&menu);
	C2D_DrawText(&menu, 0, 30, 40, 0, 0.4, 0.4);

	// Draw the cursor
	C2D_DrawTriangle(10, cursor*12+41, clrRed, 20, cursor*12+46, clrRed, 10, cursor*12+51, clrRed, 0);

	C3D_FrameEnd(0);
}

int saveMP3(const char* fileContents, int fileSize, std::string fileName) {
	fileName.append(".mp3");

	struct stat st = {0};
	if (stat("/3ds/JCatch/", &st) == -1) {
		mkdir("/3ds/JCatch/", 0777);
	}
	std::string filePath = "/3ds/JCatch/";
	filePath.append(fileName);
	FILE* file = fopen(filePath.c_str(), "w");
	if (file == NULL) {
		return -1;
	}
	fseek(file, 0, SEEK_SET);
	fwrite(fileContents, 1, fileSize, file);
	fclose(file);
	return 0;
}

void downloadEpisode()  {
	std::cout << "Downloading: " << EpisodeURLs[selectedEpisode] << std::endl;

	std::string targetURL = "http://jimmytech.net/HTTPRSS/?url=";
	targetURL.append(EpisodeURLs[selectedEpisode]);

	char* downloadedFile;
	int fileSize;
	retCode = http_download(targetURL, downloadedFile, fileSize);

	saveMP3(downloadedFile, fileSize, EpisodeNames[selectedEpisode]);

	free(downloadedFile);
}

int main() {
	setupGraphics();

	std::string settingsContents;
	retCode = loadFileBuf(settingsContents, "settings.json");
	if (retCode != 0) {
		std::cout << "Failed to read file, setting & saving defaults. Enjoy!" << std::endl;
		std::cout << defaultJSON << std::endl;
		retCode = saveFileBuf(defaultJSON, "settings.json");
		if (retCode != 0) {
			std::cout << "Failed to write file." << std::endl;
		}
	} else {
		std::cout << "Read settings" << std::endl;
	}
	Document settingsDoc;
	char* settingsBuf;
	stringToBuffer(settingsContents, settingsBuf);
	retCode = parseFileBuf(settingsBuf, settingsDoc);
	if (retCode != 0) {
		std::cout << "Corrupted settings JSON: " << retCode << std::endl;
		holdForExit();
		return 0;
	}
	if (!settingsDoc.HasMember("savedPodcasts")) {
		std::cout << "Corrupted settings JSON: missing key \"savedPodcasts\"" << std::endl;
		holdForExit();
		return 0;
	}
	for (SizeType i = 0; i < settingsDoc["savedPodcasts"].Size(); i++) {
		URLs.push_back(settingsDoc["savedPodcasts"][i]["URL"].GetString());
		Names.push_back(settingsDoc["savedPodcasts"][i]["Name"].GetString());
	}
	std::cout << "Loaded " << settingsDoc["savedPodcasts"].Size() << " saved podcast URLs" << std::endl;
	free(settingsBuf);

	// Main loop
	while (aptMainLoop())
	{
		hidScanInput();
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START || kDown & KEY_X)
			break; // break in order to return to hbmenu
		if (kDown & KEY_CPAD_DOWN || kDown & KEY_DDOWN)
			cursor = (cursor < menuLength-1) ? cursor+1 : cursor;
		if (kDown & KEY_CPAD_UP || kDown & KEY_DUP)
			cursor = (cursor > 0) ? cursor-1 : cursor;
		if (kDown & KEY_B) {
			if (currentMenu == ViewSavedPodcasts) {
				currentMenu = InitialMenu;
				updateMenu();
			} else if (currentMenu == PodcastOptions) {
				currentMenu = ViewSavedPodcasts;
				updateMenu();
			} else if (currentMenu == ListEpisodes) {
				currentMenu = PodcastOptions;
				updateMenu();
			}
		}
		if (kDown & KEY_A) {
			// std::cout << "m:" << currentMenu << ",c:" << cursor << std::endl;
			if (currentMenu == InitialMenu) {
				if (cursor == 0) {
					std::cout << "Here's where I need to figure out the swkb lib lol." << std::endl;
				} else if (cursor == 1) {
					currentMenu = ViewSavedPodcasts;
					updateMenu();
				}
			} else if (currentMenu == ViewSavedPodcasts) {
				selectedPodcast = cursor;
				// std::cout << "Selected Podcast: " << Names[selectedPodcast] << std::endl;
				currentMenu = PodcastOptions;
				updateMenu();
			} else if (currentMenu == PodcastOptions) {
				if (cursor == 0) {
					currentMenu = ListEpisodes;
					retCode = fetchPodcasts();
					if (retCode != 0) {
						currentMenu = PodcastOptions;
					}
					updateMenu();
				}
			} else if (currentMenu == ListEpisodes) {
				selectedEpisode = cursor;
				downloadEpisode();
			}
		}

		drawUI();
	}

	// free(newPodcast.episodes);
	// free(downloadedFile.buf);

	// Exit services
	cleanupGraphics();
	httpcExit();
	return 0;
}

