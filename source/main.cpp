#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include <3ds.h>

#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#pragma GCC diagnostic pop

#include "rapidjson/stringbuffer.h"
#include <iostream>
#include <sys/stat.h>

#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240

using namespace rapidjson;
struct memBufObj {
	u8 *buf = 0;
	u32 size = 0;
};
memBufObj http_download(const char *url)
{
	memBufObj retBuf;
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
	//u8* framebuf_top;
	u32 statuscode=0;
	u32 contentsize=0, readsize=0, size=0;
	u8 *buf, *lastbuf;

	printf("%d: Downloading %s\n", __LINE__, url);

	do {
		ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
		if (ret != 0) printf("%d: return from httpcOpenContext: %" PRId32 "\n",__LINE__,ret);

		// This disables SSL cert verification, so https:// will be usable
		ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
		if (ret != 0) printf("%d: return from httpcSetSSLOpt: %" PRId32 "\n",__LINE__,ret);

		// Enable Keep-Alive connections
		ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
		if (ret != 0) printf("%d: return from httpcSetKeepAlive: %" PRId32 "\n",__LINE__,ret);

		// Set a User-Agent header so websites can identify your application
		ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
		if (ret != 0) printf("%d: return from httpcAddRequestHeaderField: %" PRId32 "\n",__LINE__,ret);

		// Tell the server we can support Keep-Alive connections.
		// This will delay connection teardown momentarily (typically 5s)
		// in case there is another request made to the same server.
		ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
		if (ret != 0) printf("%d: return from httpcAddRequestHeaderField: %" PRId32 "\n",__LINE__,ret);

		ret = httpcBeginRequest(&context);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			printf("%d: err: 0" "\n", __LINE__);
			return retBuf;
			//return ret;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			printf("%d: err: 1" "\n", __LINE__);
			return retBuf;
			//return ret;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = (char*)malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				return retBuf;
				//return -1;
			}
			ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
			url = newurl; // Change pointer to the url that we just learned
			printf("%d: redirecting to url: %s\n",__LINE__,url);
			httpcCloseContext(&context); // Close this context before we try the next
		}
	} while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

	if(statuscode!=200){
		printf("%d: URL returned status: %" PRId32 "\n", __LINE__, statuscode);
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return retBuf;
		//return -2;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		printf("%d: err: 2" "\n", __LINE__);
		return retBuf;
		//return ret;
	}

	//printf("%d: reported size: %" PRId32 "\n",__LINE__,contentsize);

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		return retBuf;
		//return -1;
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
					return retBuf;
					//return -1;
				}
			}
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		return retBuf;
		//return -1;
	}

	//// Resize the buffer back down to our actual final size
	size++;
	lastbuf = buf;
	buf = (u8*)realloc(buf, size);
	if (buf == NULL) { // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if (newurl != NULL) free(newurl);
		return retBuf;
		//return -1;
	}
	memset(buf + size, 0, 1); // Pad with 0

	printf("%d: downloaded size: %" PRId32 "\n",__LINE__,size);

	//if(size>(240*400*3*2))size = 240*400*3*2;

	//framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	//memcpy(framebuf_top, buf, size);

	//gfxFlushBuffers();
	//gfxSwapBuffers();

	//framebuf_top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	//memcpy(framebuf_top, buf, size);

	//gfxFlushBuffers();
	//gfxSwapBuffers();
	//gspWaitForVBlank();

	retBuf.buf = buf;
	retBuf.size = size;

	httpcCloseContext(&context);
	if (newurl!=NULL) free(newurl);
	//free(buf);
	return retBuf;
}

int saveFile(memBufObj fileData, std::string fileName) {
	if (mkdir("JCatch", 0777) && errno != EEXIST) {
		return -2;
	}
	std::string filePath = "JCatch/" + fileName;
	FILE* file = fopen(filePath.c_str(), "w");
	if (file == NULL) return -1;
	fseek(file, 0, SEEK_SET);
	fwrite(fileData.buf, 1, fileData.size, file);
	fclose(file);
	return 0;
}

int saveFile(std::string fileData, std::string fileName) {
	if (mkdir("JCatch", 0777) && errno != EEXIST) {
		return -2;
	}
	std::string filePath = "JCatch/" + fileName;
	FILE* file = fopen(filePath.c_str(), "w");
	if (file == NULL) return -1;
	fseek(file, 0, SEEK_SET);
	fwrite(fileData.c_str(), 1, fileData.length(), file);
	fclose(file);
	return 0;
}

memBufObj loadFile(std::string fileName) {
	memBufObj retBuf;
	FILE* file = fopen(fileName.c_str(), "rb");
	if (file == NULL) return retBuf;

	fseek(file, 0, SEEK_END);
	off_t size = ftell(file);
	fseek(file, 0, SEEK_SET);

	u8* buffer = (unsigned char*)malloc(size);
	if (!buffer) return retBuf;

	off_t bytesRead = fread(buffer, 1, size, file);

	fclose(file);

	if (size != bytesRead) return retBuf;
	retBuf.size = size;
	retBuf.buf = buffer;
	return retBuf;
}

void printParseError(ParseErrorCode parseError) {
	switch (parseError) {
	case kParseErrorDocumentEmpty:
		printf("%d: Parser error kParseErrorDocumentEmpty\n", __LINE__);
		break;
	case kParseErrorDocumentRootNotSingular:
		printf("%d: Parser error kParseErrorDocumentRootNotSingular\n", __LINE__);
		break;
	case kParseErrorValueInvalid:
		printf("%d: Parser error kParseErrorValueInvalid\n", __LINE__);
		break;
	case kParseErrorObjectMissName:
		printf("%d: Parser error kParseErrorObjectMissName\n", __LINE__);
		break;
	case kParseErrorObjectMissColon:
		printf("%d: Parser error kParseErrorObjectMissColon\n", __LINE__);
		break;
	case kParseErrorObjectMissCommaOrCurlyBracket:
		printf("%d: Parser error kParseErrorObjectMissCommaOrCurlyBracket\n", __LINE__);
		break;
	case kParseErrorArrayMissCommaOrSquareBracket:
		printf("%d: Parser error kParseErrorArrayMissCommaOrSquareBracket\n", __LINE__);
		break;
	case kParseErrorStringUnicodeEscapeInvalidHex:
		printf("%d: Parser error kParseErrorStringUnicodeEscapeInvalidHex \n", __LINE__);
		break;
	case kParseErrorStringUnicodeSurrogateInvalid:
		printf("%d: Parser error kParseErrorStringUnicodeSurrogateInvalid \n", __LINE__);
		break;
	case kParseErrorStringEscapeInvalid:
		printf("%d: Parser error kParseErrorStringEscapeInvalid  \n", __LINE__);
		break;
	case kParseErrorStringMissQuotationMark:
		printf("%d: Parser error kParseErrorStringMissQuotationMark  \n", __LINE__);
		break;
	case kParseErrorStringInvalidEncoding:
		printf("%d: Parser error kParseErrorStringInvalidEncoding  \n", __LINE__);
		break;
	case kParseErrorNumberTooBig:
		printf("%d: Parser error kParseErrorNumberTooBig  \n", __LINE__);
		break;
	case kParseErrorNumberMissFraction:
		printf("%d: Parser error kParseErrorNumberMissFraction  \n", __LINE__);
		break;
	case kParseErrorNumberMissExponent:
		printf("%d: Parser error kParseErrorNumberMissExponent  \n", __LINE__);
		break;
	case kParseErrorTermination:
		printf("%d: Parser error kParseErrorTermination  \n", __LINE__);
		break;
	case kParseErrorUnspecificSyntaxError:
		printf("%d: Parser error kParseErrorUnspecificSyntaxError  \n", __LINE__);
		break;
	default:
		printf("%d: Parser error fell throught to OTHER\n", __LINE__);
	}
}

static SwkbdCallbackResult MyCallback(void* user, const char** ppMessage, const char* text, size_t textlen)
{
	if (strstr(text, "lenny")) // Contains
	{
		*ppMessage = "Nice try but I'm not letting you use that meme right now";
		return SWKBD_CALLBACK_CONTINUE; // Return to keyboard, use this for validation
	}

	//if (strstr(text, "brick"))
	//{
	//	*ppMessage = "~Time to visit Brick City~";
	//	return SWKBD_CALLBACK_CLOSE; // Weird continuation, not a confirmation
	//}

	return SWKBD_CALLBACK_OK;
}

std::string inputSWKB() {
	std::string outString = "";
	static SwkbdState swkbd;
	static char mybuf[60];
	//static SwkbdStatusData swkbdStatus;
	//static SwkbdLearningData swkbdLearning;
	SwkbdButton button = SWKBD_BUTTON_NONE;

	swkbdInit(&swkbd, SWKBD_TYPE_WESTERN, 2, -1);
	swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	swkbdSetFeatures(&swkbd, SWKBD_DARKEN_TOP_SCREEN | SWKBD_ALLOW_HOME | SWKBD_ALLOW_RESET | SWKBD_ALLOW_POWER);
	swkbdSetFilterCallback(&swkbd, MyCallback, NULL);

	bool shouldQuit = false;
	mybuf[0] = 0;
	do
	{
		swkbdSetInitialText(&swkbd, mybuf);
		button = swkbdInputText(&swkbd, mybuf, sizeof(mybuf));
		if (button != SWKBD_BUTTON_NONE)
			return outString;

		SwkbdResult res = swkbdGetResult(&swkbd);
		if (res == SWKBD_RESETPRESSED)
		{
			shouldQuit = true;
			aptSetChainloaderToSelf();
			return outString;
		}
		else if (res != SWKBD_HOMEPRESSED && res != SWKBD_POWERPRESSED) {
			return outString;
			break; // An actual error happened, display it
		}

		shouldQuit = !aptMainLoop();
	} while (!shouldQuit);

	outString = mybuf;
	return outString;
	//if (shouldQuit)
	//	break;
}

int main()
{
	gfxInitDefault();
	httpcInit(0); // Buffer size when POST/PUT.

	consoleInit(GFX_TOP, NULL);

	Document UserSettings;
	memBufObj userSettingsBuffer = loadFile("Casts.json");
	if (userSettingsBuffer.size != 0) {
		UserSettings.ParseInsitu((char*)userSettingsBuffer.buf);
	}
	else {
		char* jsonDefault = (char*)" { \"Casts\": [] } ";
		saveFile(jsonDefault, "Cast.json");
		UserSettings.ParseInsitu(jsonDefault);
	}
	printf("Validating UserSettings JSON");
	assert(UserSettings.IsObject());
	assert(UserSettings.HasMember("Casts"));
	// Using a reference for consecutive access is handy and faster.
	const Value& Casts = UserSettings["Casts"];
	assert(Casts.IsArray());
	printf("Casts = \n");
	for (SizeType i = 0; i < Casts.Size(); i++) // Uses SizeType instead of size_t
		printf("  %d:%d\n", i, Casts[i].GetInt());

	printf("Casts Validated");

	std::string newPodcastURL = inputSWKB();



	memBufObj ret;
	ret = http_download("http://jimmytech.net/RSS2JSON/?url=http://feeds.feedburner.com/OffbeatOregonHistory");

	if (ret.size == 0) {
		printf("%d: Download size 0!\n", __LINE__);
	} else if (saveFile(ret, "downloadOut2.json") != 0) {
		printf("%d: Failed to write file!\n", __LINE__);
	} else {
		printf("%d: Parsing Document!\n", __LINE__);
		printf("%d: Document size: %" PRId32 "\n", __LINE__, ret.size);
		Document document;
		document.ParseInsitu((char*)ret.buf);
		if (document.HasParseError()) printParseError(document.GetParseError());
		else {
			printf("\nParsing to document succeeded.\n");
			printf("\nAccess values in document:\n");
			assert(document.IsObject());    // Document is a JSON value represents the root of DOM. Root can be either an object or array.

			assert(document.HasMember("title"));
			assert(document["title"].IsString());
			printf("title = %s\n", document["title"].GetString());
			assert(document.HasMember("description"));
			assert(document["description"].IsString());
			printf("description = %s\n", document["description"].GetString());
		}
	}

	// Main loop
	while (aptMainLoop())
	{
		gspWaitForVBlank();
		hidScanInput();

		// Your code goes here

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu

	}

	// Exit services
	httpcExit();
	gfxExit();
	return 0;
}

