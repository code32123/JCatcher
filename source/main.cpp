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

using namespace rapidjson;

struct fileBuf {
	u8 *buf = 0;
	u32 size = 0;
	s32 error = 0;
};

fileBuf http_download(const char *url)
{
	fileBuf retBuf;
	Result ret=0;
	httpcContext context;
	char *newurl=NULL;
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
			retBuf.error = ret;
			return retBuf;
		}

		ret = httpcGetResponseStatusCode(&context, &statuscode);
		if(ret!=0){
			httpcCloseContext(&context);
			if(newurl!=NULL) free(newurl);
			printf("%d: err: 1" "\n", __LINE__);
			retBuf.error = ret;
			return retBuf;
		}

		if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
			if(newurl==NULL) newurl = (char*)malloc(0x1000); // One 4K page for new URL
			if (newurl==NULL){
				httpcCloseContext(&context);
				retBuf.error = -1;
				return retBuf;
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
		retBuf.error = -2;
		return retBuf;
	}

	// This relies on an optional Content-Length header and may be 0
	ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		printf("%d: err: 2" "\n", __LINE__);
		retBuf.error = ret;
		return retBuf;
	}

	//printf("%d: reported size: %" PRId32 "\n",__LINE__,contentsize);

	// Start with a single page buffer
	buf = (u8*)malloc(0x1000);
	if(buf==NULL){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		retBuf.error = -1;
		return retBuf;
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
					retBuf.error = -1;
					return retBuf;
				}
			}
	} while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);	

	if(ret!=0){
		httpcCloseContext(&context);
		if(newurl!=NULL) free(newurl);
		free(buf);
		retBuf.error = -1;
		return retBuf;
	}

	// Resize the buffer back down to our actual final size
	size++;
	lastbuf = buf;
	buf = (u8*)realloc(buf, size);
	if (buf == NULL) { // realloc() failed.
		httpcCloseContext(&context);
		free(lastbuf);
		if (newurl != NULL) free(newurl);
		retBuf.error = -1;
		return retBuf;
	}
	memset(buf + size, 0, 1); // Pad with 0

	printf("%d: downloaded size: %" PRId32 "\n",__LINE__,size);

	retBuf.buf = buf;
	retBuf.size = size;
	retBuf.error = 0;

	httpcCloseContext(&context);
	// free(buf);
	if (newurl!=NULL) free(newurl);
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

void showFileBuf(fileBuf fileBuf) {
	u32 size = fileBuf.size;
	if(size>(240*400*3*2))size = 240*400*3*2;

	u8* framebuf_bottom;
	framebuf_bottom = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	memcpy(framebuf_bottom, fileBuf.buf, size);

	gfxFlushBuffers();
	gfxSwapBuffers();

	framebuf_bottom = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
	memcpy(framebuf_bottom, fileBuf.buf, size);

	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
}

void saveFileBuf(fileBuf fileBuf, const char* fileName) {
	struct stat st = {0};

	if (stat("JCatch", &st) == -1) {
		mkdir("JCatch", 0777);
	}
	std::string filePath = "JCatch/";
	filePath.append(fileName);
	FILE* file = fopen(filePath.c_str(), "w");
	if (file == NULL) {
		return;
		// return -1;
	}
	fseek(file, 0, SEEK_SET);
	fwrite(fileBuf.buf, 1, fileBuf.size, file);
	fclose(file);
}

void parseFileBuf(fileBuf fileBuf) {
	Document document;
	document.ParseInsitu((char*)fileBuf.buf);
	if (document.HasParseError()) {
		printParseError(document.GetParseError());
		return;
		// return -2;
	}

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

int main()
{
	fileBuf downloadedFile;
	gfxInitDefault();
	httpcInit(0); // Buffer size when POST/PUT.

	consoleInit(GFX_TOP,NULL);

	//ret = http_download("https://www.toptal.com/developers/feed2json/convert?url=http://feeds.feedburner.com/OffbeatOregonHistory");
	downloadedFile = http_download("http://jimmytech.net/RSS2JSON/?url=http://feeds.feedburner.com/OffbeatOregonHistory");
	printf("%d: error from http_download: %" PRId32 "\n",__LINE__,downloadedFile.error);
	showFileBuf(downloadedFile);
	saveFileBuf(downloadedFile, "cast.json");
	parseFileBuf(downloadedFile);

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

