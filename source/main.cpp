#include <vector>
#include <fstream>
#include <iostream>
#include <list>

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <malloc.h>
#include <cacert_pem.h>

#include <sys/stat.h>

#include <3ds.h>
#include <citro2d.h>

#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#pragma GCC diagnostic pop

#include <tinyxml2.h>
#include "rapidjson/stringbuffer.h"
#include <curl/curl.h>


using namespace rapidjson;

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240


C3D_RenderTarget* top;

u32 clrWhite = C2D_Color32(0xFF, 0xFD, 0xD0, 0xFF);
// u32 clrGreen = C2D_Color32(0x00, 0xFF, 0x00, 0xFF);
u32 clrRed   = C2D_Color32(0xFF, 0x00, 0x00, 0xFF);
// u32 clrBlue  = C2D_Color32(0x00, 0x00, 0xFF, 0xFF);

std::string defaultJSON = "{\"savedPodcasts\":[{\"Name\":\"Offbeat Oregon\",\"URL\":\"http://feeds.feedburner.com/OffbeatOregonHistory\"}]}";

C2D_Text title;
C2D_TextBuf titleBuf  = C2D_TextBufNew(100);
C2D_Text menu;
C2D_TextBuf menuBuf = C2D_TextBufNew(4096);

int retCode = 0;
int cursor = 0;
int menuLength = 3;
int selectedPodcast = 0;
int selectedEpisode = 0;
enum menuType {
	InitialMenu,
	ViewSavedPodcasts,
	PodcastOptions,
	ListEpisodes,
	ViewCredits
};

menuType currentMenu = InitialMenu;

char* menuString;

std::string titleText = "JCatcher";

std::string initialMenuText = \
		"Add new podcast URL\n"\
		"View Saved Podcasts\n"\
		"Credits";

std::string podcastOptionsText = \
		"Check Episodes\n"\
		"Edit\n"\
		"Remove";

std::string creditsText = \
		"Application: James Smythe\n"\
		"Thanks to: TinyXML2, RapidJSON";

std::vector<std::string> Names;
std::vector<std::string> URLs;
std::vector<std::string> EpisodeNames;
std::vector<std::string> EpisodeURLs;

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
static u32 *SOC_buffer = NULL;
static const char *pCACertFilePath = "/3ds/JCatch/cacert.pem";

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	((std::string *)userp)->append((char*)contents, realsize);
	std::cout << "#";
	return realsize;
}

int setupCACERT() {
	std::cout << "Checking for cacert.pem" << std::endl;
	struct stat st = {0};
	if (stat(pCACertFilePath, &st) == -1) {
		std::cout << "Creating cacert.pem" << std::endl;
		FILE* file = fopen(pCACertFilePath, "w");
		if (file == NULL) {
			return -1;
		}
		fseek(file, 0, SEEK_SET);
		fwrite(cacert_pem, 1, cacert_pem_size, file);
		fclose(file);
	}
	return 0;
}

int download(std::string url, std::string &fileContents) {
	CURL *curl;
	CURLcode res;
	const char* urlc = url.c_str();


	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, urlc);
		curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
		curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFilePath);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
		curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L); // cache the CA cert bundle in memory for a week
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 	    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&fileContents);

		/* Perform the request, res gets the return code */
		res = curl_easy_perform(curl);

		/* Check for errors */
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		/* always cleanup */
		curl_easy_cleanup(curl);
	}
 
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

int saveFile(std::string fileContents, std::string fileName) {
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

int loadFile(std::string &fileContents, std::string fileName) {
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

int jsonParse(char* fileBuf, Document &document) {
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

// tm parse8601(const char* dateTimeString) {
// 	// https://stackoverflow.com/a/26896792
// 	int year,month,day,hour,minute;
// 	float second;
// 	sscanf(dateTimeString, "%d-%d-%dT%d:%d:%fZ", &year, &month, &day, &hour, &minute, &second);
// 	tm dateTime = { 0 };
// 	dateTime.tm_year = year - 1900;
// 	dateTime.tm_mon = month - 1;
// 	dateTime.tm_mday = day;
// 	dateTime.tm_hour = hour;
// 	dateTime.tm_min = minute;
// 	dateTime.tm_sec = (int)second;
// 	return dateTime;
// }

// int parsePodcast(Document &document) {
// 	if (!document.IsObject()) 									return -1;
// 	if (!document.HasMember("title"))							return -2;
// 	if (!document["title"].IsString())							return -3;
// 	if (!document.HasMember("description"))						return -4;
// 	if (!document["description"].IsString())					return -5;
// 	if (!document.HasMember("items"))							return -6;
// 	const Value& items = document["items"];
// 	if (!items.IsArray())										return -7;
// 	if (items.Empty())											return -8;
// 	SizeType itemCount = items.Size();
//
// 	for (SizeType i = 0; i < itemCount; i++) {
// 		const Value& curItem = items[i];
//		
// 		if (!curItem.HasMember("title"))						return (-i*10)-1;
// 		if (!curItem["title"].IsString())						return (-i*10)-2;
// 		EpisodeNames.push_back(curItem["title"].GetString());
//
// 		if (!curItem.HasMember("url"))							return (-i*10)-3;
// 		if (!curItem["url"].IsString())							return (-i*10)-4;
// 		EpisodeURLs.push_back(curItem["url"].GetString());
// 	}
//
// 	return 0;
// }

int stringToBuffer(std::string string, char* &buffer) {
	const char* cstr = string.c_str();
	int contentSize = strlen(cstr);
	buffer = (char*)malloc(contentSize+1);
	if (buffer == NULL) {
		return -1;
	}
	memcpy(buffer, cstr, contentSize);
	memset(buffer+contentSize, 0, 1);
	return 0;
}

void stringListToMenu(std::vector<std::string> stringList, int amount) {
	std::cout << stringList.size() << " items in sl" << std::endl;
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
	std::string downloadedFile;
	std::string targetURL = URLs[selectedPodcast];
	retCode = download(targetURL, downloadedFile);
	if (retCode != 0) {
		std::cout << "HTTP download failed: " << retCode << std::endl;
		return -1;
	}

	std::string fileDownload = "";

	char* DocBuffer;
	stringToBuffer(downloadedFile, DocBuffer);

	tinyxml2::XMLDocument RSSFeed;
	tinyxml2::XMLError xmlRetCode = RSSFeed.Parse(DocBuffer);
	if (xmlRetCode != tinyxml2::XML_SUCCESS) {
		std::cout << "Err, xml: " << RSSFeed.ErrorIDToName(xmlRetCode) << std::endl;
		std::cout << "Line: " << RSSFeed.ErrorLineNum() << std::endl;
		// holdForExit();
		return 0;
	}

	tinyxml2::XMLElement* Feed = RSSFeed.FirstChildElement("rss");
	tinyxml2::XMLElement* Chan = Feed->FirstChildElement("channel");
	// tinyxml2::XMLElement* Name = Chan->FirstChildElement("title");
	int i = 0;
	for (tinyxml2::XMLElement* node = Chan->FirstChildElement("item"); node != NULL; node = node->NextSiblingElement("item")) {
		EpisodeNames.push_back(node->FirstChildElement("title")->GetText());
		EpisodeURLs.push_back(node->FirstChildElement("enclosure")->Attribute("url"));
		i++;
	}
	std::cout << "Loaded " << i << " episodes" << std::endl;
	return 0;
}

void updateMenu() {
	// std::cout << "Current Menu: " << currentMenu << std::endl;
	if (currentMenu == InitialMenu) {
		free(menuString);
		stringToBuffer(initialMenuText, menuString);
		menuLength = 3;
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

static size_t WriteMP3Callback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	std::ofstream* myfile = (std::ofstream*)userp;
	

	myfile->write((const char *)contents, realsize);
	std::cout << "3";

	return realsize;
}

void downloadEpisode()  {
	std::cout << "Downloading: " << EpisodeURLs[selectedEpisode] << std::endl;
	std::string targetURL = EpisodeURLs[selectedEpisode];
	std::string filePath = "/3ds/JCatch/";
	filePath.append(EpisodeNames[selectedEpisode]);
	filePath.append(".mp3");

	std::ofstream myfile;
	myfile.open(filePath.c_str(), std::fstream::out | std::fstream::binary);
	if (!myfile.is_open()) {
		std::cout << "Failed to open " << filePath << " to download" << std::endl;
		return;
	}

	// std::string downloadedFile;
	
	CURL *curl;
	CURLcode res;
	const char* urlc = targetURL.c_str();


	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, urlc);
		curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
		curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFilePath);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
		curl_easy_setopt(curl, CURLOPT_CA_CACHE_TIMEOUT, 604800L); // cache the CA cert bundle in memory for a week
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMP3Callback);
 	    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&myfile);

		/* Perform the request, res gets the return code */
		res = curl_easy_perform(curl);

		/* Check for errors */
		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

		/* always cleanup */
		curl_easy_cleanup(curl);
	}
	myfile.close();
}

Document settingsDoc;
char* settingsBuf;

void writeSettings() {
	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	settingsDoc.Accept(writer);
	const char* output = buffer.GetString();
	saveFile(output, "settings.json");
}

int setupSettings() {
	std::string settingsContents;
	retCode = loadFile(settingsContents, "settings.json");
	if (retCode != 0) {
		std::cout << "settings.json not found, creating" << std::endl;
		// std::cout << defaultJSON << std::endl;
		settingsContents = defaultJSON;
		retCode = saveFile(defaultJSON, "settings.json");
		if (retCode != 0) {
			std::cout << "Failed to write file." << std::endl;
		}
	} else {
		std::cout << "Read settings" << std::endl;
	}
	stringToBuffer(settingsContents, settingsBuf);
	retCode = jsonParse(settingsBuf, settingsDoc);
	if (retCode != 0) {
		std::cout << "Corrupted settings JSON: " << retCode << std::endl;
		return -1;
	}
	if (!settingsDoc.HasMember("savedPodcasts")) {
		std::cout << "Corrupted settings JSON: missing key \"savedPodcasts\"" << std::endl;
		return -1;
	}
	for (SizeType i = 0; i < settingsDoc["savedPodcasts"].Size(); i++) {
		URLs.push_back(settingsDoc["savedPodcasts"][i]["URL"].GetString());
		Names.push_back(settingsDoc["savedPodcasts"][i]["Name"].GetString());
	}
	std::cout << "Loaded " << settingsDoc["savedPodcasts"].Size() << " saved podcast URLs" << std::endl;
	return 0;
}

void setupDirectory() {
	struct stat st = {0};
	if (stat("/3ds/JCatch/", &st) == -1) {
		mkdir("/3ds/JCatch/", 0777);
	}
}

int setupSoc() {
	std::cout << "Socket Setup" << std::endl;
	int ret;
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if(SOC_buffer == NULL) {
		std::cout << "memalign: failed to allocate" << std::endl;
		return -1;
	}

	// Now intialise soc:u service
	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    	printf("socInit: 0x%08X\n", (unsigned int)ret);
		return -1;
	}
	return 0;
}

int setupCurl() {
	std::cout << "Curl Setup" << std::endl;
	setupCACERT();
	setupSoc();
	return 0;
}

void cleanupCurl() {
	curl_global_cleanup();
	socExit();
}

int main() {
	setupGraphics();
	setupDirectory();
	setupCurl();
	setupSettings();

	// std::string testURL = "http://feeds.feedburner.com/OffbeatOregonHistory";
	// std::string testURL = "https://feeds.megaphone.fm/LMG3928170156";
	// std::string testURL = "https://omnycontent.com/d/playlist/e73c998e-6e60-432f-8610-ae210140c5b1/A91018A4-EA4F-4130-BF55-AE270180C327/44710ECC-10BB-48D1-93C7-AE270180C33E/podcast.rss";
	
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
		// if (kDown & KEY_L) {
		// 	std::cout << "Removing settings.json...";
		// 	const char* filePath = "/3ds/JCatch/settings.json";
		// 	retCode = std::remove(filePath);
		// 	if (retCode != 0) {
		// 		std::cout << "Err" << retCode << std::endl;
		// 		std::perror("Error deleting");
		// 		if (!std::ifstream{filePath}) {
		// 			std::cout << "And I can't read it either" << std::endl;
		// 		} else {
		// 			std::cout << "But I can read it just fine lol" << std::endl;
		// 		}
		// 	} else {
		// 		std::cout << "Done!" << std::endl;
		// 		break;
		// 	}
		// }
		if (kDown & KEY_A) {
			// std::cout << "m:" << currentMenu << ",c:" << cursor << std::endl;
			if (currentMenu == InitialMenu) {
				if (cursor == 0) {
					std::string newName = "Darknet Diaries";
					std::string newURL = "https://feeds.megaphone.fm/darknetdiaries";

					Document::AllocatorType& alloc = settingsDoc.GetAllocator();
					Names.push_back(newName);
					URLs.push_back(newURL);
					settingsDoc["savedPodcasts"].PushBack(Value().SetObject()
						.AddMember("Name", Value(newName.c_str(), alloc).Move(), alloc)
						.AddMember("URL", Value(newURL.c_str(), alloc).Move(), alloc)
					, alloc);
					writeSettings();

					std::cout << "Here's where I need to figure out the swkb lib lol." << std::endl;
				} else if (cursor == 1) {
					currentMenu = ViewSavedPodcasts;
					updateMenu();
				} else if (cursor == 2) {
					currentMenu = ViewCredits;
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

	// Exit services
	free(settingsBuf);
	cleanupCurl();
	cleanupGraphics();
	return 0;
}

