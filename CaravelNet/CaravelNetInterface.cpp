// $Id$

/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is Deadly Rooms of Death.
*
* The Initial Developer of the Original Code is
* Caravel Software.
* Portions created by the Initial Developer are Copyright (C) 2002, 2005
* Caravel Software. All Rights Reserved.
*
* Contributor(s): Matt Schikore (Schik)
*
* ***** END LICENSE BLOCK ***** */

//CaravelNetInterface.cpp.
//Implementation of CCaravelNetInterface

#include "CaravelNetInterface.h"
#include "../DRODLib/Db.h"
#include "../DRODLib/DbPlayers.h"
#include "../DRODLib/DbXML.h"
#include "../DRODLib/GameConstants.h"
#include "../DRODLib/SettingsKeys.h"

#include <BackEndLib/Assert.h>
#include <BackEndLib/Base64.h>
#include <BackEndLib/Files.h>
#include <BackEndLib/Ports.h>
#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_mutex.h>
#include <json/json.h>

#include <stdio.h>

#ifdef WIN32 
#	define snprintf _snprintf
#endif

const UINT cMD5_HASH_SIZE = 32;
const UINT cBUF_SIZE = 256;

const string cNetStr = "http://forum.caravelgames.com/game50.php";

bool g_bCloseCloud = false;
std::deque<Json::Value> g_cloudQueue;
std::deque<Json::Value> g_cloudQueueOut;
SDL_mutex *g_cloudMutex = SDL_CreateMutex();
SDL_Thread *g_pCloudThread = NULL;


UINT CCaravelNet_Cloud_QueueSize()
{
	SDL_mutexP(g_cloudMutex);
	const UINT size = g_cloudQueue.size();
	SDL_mutexV(g_cloudMutex);

	return size;
}

//*****************************************************************************
//  This is the Cloud thread.  It should only upload prepared data, and when
//  a request comes back that requires more data to be prepared (i.e. download
//  and merge <this demo pack> and re-upload), it will simply put that request
//  into a queue that the main thread will fulfill.
//
//  This thread should do no database access.
int CCaravelNet_Cloud_Worker(void* pPtr)
{
	SDL_mutex* pMutex = (SDL_mutex*) pPtr;

	UINT dwHandle = 0;

	bool bDone = false;
	while (!bDone) {
		SDL_Delay(10);
		SDL_mutexP(pMutex);
		if (g_cloudQueue.size() == 0 && g_bCloseCloud) bDone = true;
		SDL_mutexV(pMutex);

		if (dwHandle && g_pTheNet) {
			// have a currently pending request.  Check to see if it's done.
			if (g_pTheNet->GetStatus(dwHandle) >= CURLE_OK) {
				CNetResult* pResult = g_pTheNet->GetResults(dwHandle);
				dwHandle = 0;
				if (pResult && pResult->pJson) {
					if (pResult->pJson->isMember("holdsNeeded") || pResult->pJson->isMember("version")) {
						Json::Value push = * pResult->pJson;
						SDL_mutexP(pMutex);
						g_cloudQueueOut.push_back(push);
						SDL_mutexV(pMutex);
					}
				}
				delete pResult;
			}
		}
		
		if (0 == dwHandle) {
			// No current request, fire off the next one.
			SDL_mutexP(pMutex);
			if (g_cloudQueue.size() > 0) {
				Json::Value json = g_cloudQueue.front();
				g_cloudQueue.pop_front();
				CCaravelNetInterface* pNet = (CCaravelNetInterface*)g_pTheNet;
				dwHandle = pNet->getRequest(cNetStr, json);
			}
			SDL_mutexV(pMutex);
		}
	}
	return 0;
}

//
//Public methods.
//

//*****************************************************************************
CCaravelNetInterface::CCaravelNetInterface()
	: CNetInterface()
	, wHoldHandle(0)
	, dwLastNotice(0)
	, dwDownloadHoldId(0)
{
	ASSERT(g_cloudMutex);
	g_pCloudThread = SDL_CreateThread(CCaravelNet_Cloud_Worker, "CaravelNet", (void *)g_cloudMutex);
}

//*****************************************************************************
CCaravelNetInterface::~CCaravelNetInterface()
{
	// Finish up any pending CaravelNet requests, so as to get a
	// valid Key for the next time the app is run.
	ClearActiveAction();

	// Tell the cloud thread to quit, and then wait until it does
	SDL_mutexP(g_cloudMutex);
	g_bCloseCloud = true;
	SDL_mutexV(g_cloudMutex);


	while (CCaravelNet_Cloud_QueueSize() > 0) {
		SDL_Delay(10);
	}

	if (g_pTheDB->IsOpen()) {
		this->ChatLogout();
	}

	ClearCNetHolds();
	ClearResults();
}

//*****************************************************************************
UINT CCaravelNetInterface::CloudQueueSize() const 
{
	return CCaravelNet_Cloud_QueueSize();
}

//*****************************************************************************
bool CCaravelNetInterface::Busy() const
//Returns: true if waiting for a transaction to finish
{
	for (CIDSet::iterator i = CCaravelNetInterface::currentHandles.begin(); i != CCaravelNetInterface::currentHandles.end(); i++) {
		if (CInternet::GetStatus(*i) < 0) {
			return true;
		}
	}
	if (this->wHoldHandle && CInternet::GetStatus(this->wHoldHandle) < 0)
		return true;

	return false; //not busy
}

//*****************************************************************************
void CCaravelNetInterface::ClearActiveAction()
//Waits for any outstanding query to complete so new key can be received
//before submitting another query.  Store results.
{
	//Process any special handles that are waiting.
	if (this->wHoldHandle)
	{
		WaitUntilHoldsReceived();
		return;
	}
	
	if (CCaravelNetInterface::currentHandles.size() > 0)
	{
		CIDSet copySet = CCaravelNetInterface::currentHandles;
		for (CIDSet::iterator i = copySet.begin(); i != copySet.end(); i++) {
		//Hold onto this value, because it gets reset by GetResults below.
			const UINT handle = *i;

			int status = CInternet::GetStatus(handle);
			while (status == -1) {
				SDL_Delay(20);	//wait until response
				status = CInternet::GetStatus(handle);
			}

			if (status >= 0) {
				//Save results for later lookup.
				CNetResult* pRes = GetResults(handle);
				this->results[handle] = pRes;
			}
		}
	}
	CCaravelNetInterface::currentHandles.clear();
}

//*****************************************************************************
void CCaravelNetInterface::Enable(const bool bVal) //[default=true]
{
	CNetInterface::Enable(bVal);

	if (!bVal)
	{
		//Cancel any outgoing requests.
		CCaravelNetInterface::currentHandles.clear();
		this->wHoldHandle = 0;
		ASSERT(!Busy());
	}
}

//*****************************************************************************
int CCaravelNetInterface::GetStatus(
//Returns: Error code returned if handle response received (0 indicating success),
// else -1 if response not received
//
//Params:
	const UINT handle) // (in) handle of the download to query the status of
{
	//Check whether thread has already completed and results were stored.
	std::map<UINT, CNetResult*>::iterator iter = this->results.find(handle);
	if (iter != this->results.end())
		return CURLE_OK;	//assume result was received properly if it's in 'results'

	if (!IsEnabled())
		return CURLE_COULDNT_CONNECT; //don't cause indefinite waiting for response if disabled

	return CInternet::GetStatus(handle);
}

//*****************************************************************************
bool CCaravelNetInterface::ChatLogout()
//Sends a chat logout notification from this user.
//
//Returns: Whether notification was sent successfully.
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return false;

	post["action"] = "chatlogout";
	return CInternet::HttpGet(cNetStr, post); //ignore response from server (no handle returned)
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadDemo(
//Send a request to the server to download a specific demo
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const long demoID) // (in)  The id of the demo to download
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	post["action"] = "dldemo";
	post["id"] = (Json::Value::Int64)demoID;

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadHold(
//Send a request to CaravelNet to download a specific hold
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
   const long holdID) // (in)  The id of the hold to download
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	post["action"] = "dlhold";
	post["id"] = (Json::Value::Int64)holdID;

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadStyle(
//Send a request to CaravelNet to download a specific style
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
   const WCHAR* styleName) // (in)  The name of the style to download
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	post["action"] = "dlStyle";
	string b64 = Base64::encode((BYTE*)styleName, WCSlen(styleName)*sizeof(WCHAR));
	ASSERT(unsigned(b64.length()) == b64.length());
	post["StyleName"] = b64;

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadHoldList()
//Send a request to CaravelNet for a list of available holds
//
//Returns: A handle to use to get the response, or 0 on failure.
{
	ClearCNetHolds();

	//Only query the hold list for "real" players.
	if (!g_pTheDB->Players.IsLocal(g_pTheDB->GetPlayerID())) return 0;

	if (this->wHoldHandle) return 0; //still waiting on a pending hold list request

	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	post["action"] = "getholdlist";

	const UINT handle = this->wHoldHandle = getRequest(cNetStr, post);
	return handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::DownloadRecords(
//Send a request to CaravelNet for a list of records for the specified room(s)
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const CNetRoom& room,  //(in) XML data representing the room records are requested for
	const bool bSendIfConquered) //if true, tells server to only send records for
	                             //each room in question if it has been marked as
										  //conquered on the server end
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	post["action"] = "getrecords";
	Json::Value* roomData = room.ToJson();
	post["Room"] = *roomData;
	delete roomData;
	if (bSendIfConquered) {
		post["SendIfConquered"] = true;
	}

	return getRequest(cNetStr, post);
}

//*****************************************************************************
string CCaravelNetInterface::getCompressedBuffer(const string& text)
{
	CStretchyBuffer buffer(text);

	BYTE *pCompressed = NULL;
	ULONG compressedSize=0;
	if (!buffer.Compress(pCompressed, compressedSize)) {
		delete[] pCompressed;
		return string();
	}

	const string b64 = Base64::encode((BYTE*)pCompressed, compressedSize);
	delete[] pCompressed;

	return b64;
}

//*****************************************************************************
string CCaravelNetInterface::getCompressedEncodedBuffer(const string& text)
{
	CStretchyBuffer buffer(text);

	BYTE *pCompressed = NULL;
	ULONG compressedSize=0;
	if (!buffer.Compress(pCompressed, compressedSize)) {
		delete[] pCompressed;
		return string();
	}

	buffer.Set(pCompressed, compressedSize);
	delete[] pCompressed;
	buffer.Encode();

	const string b64 = Base64::encode((BYTE*)buffer, buffer.Size());
	return b64;
}

//*****************************************************************************
UINT CCaravelNetInterface::getRequest(const string& url, CPostData* pPost)
{
	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, &handle, pPost);
	if (bSuccess) CCaravelNetInterface::currentHandles += handle;
	return handle;
}
//*****************************************************************************
UINT CCaravelNetInterface::getRequest(const string& url, Json::Value& json)
{
	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(url, json, &handle);
	if (bSuccess) CCaravelNetInterface::currentHandles += handle;
	return handle;
}

//*****************************************************************************
UINT CCaravelNetInterface::HasRegisteredAccess()
//Query CaravelNet to see if the username and key are valid.
//This will receive a new key from the server for use in the next query.
//
//Returns: A handle to be used to retrieve the results
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	post["action"] = "login";

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::SendChatText(const SendData& text, const int lastID, const UINT roomID)
//Returns: A handle to use to get the response, or 0 on failure.
{
	vector<SendData> texts;
	texts.push_back(text);
	return SendChatText(texts, lastID, roomID);
}

//*****************************************************************************
UINT CCaravelNetInterface::SendChatText(const vector<SendData>& texts, const int lastID, const UINT roomID)
//Returns: A handle to use to get the response, or 0 on failure.
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	string url = cNetStr + "?action=";
	post["action"] = roomID ? "roomchat" : "chat"; //room vs global chat
	post["LastID"] = lastID;
	for (UINT i=0; i<texts.size(); ++i)
	{
		const SendData& data = texts[i];
		if (data.text.length()) //don't upload empty texts
		{
			const string b64 = Base64::encode(data.text);
			ASSERT(unsigned(b64.length()) == b64.length());
			post["Text"][i] = b64;

			UINT j = 0;
			for (CIDSet::const_iterator id=data.userIDs.begin(); id != data.userIDs.end(); ++id)
			{
				post["UserID"][i][j] = *id;
				j++;
			}
		}
	}
	if (roomID)
	{
		CIDSet roomIDs(roomID);
		CDbRefs dbRefs(V_Rooms, roomIDs);
		string buffer = CDbXML::getXMLheader();
		g_pTheDB->Rooms.ExportXML(roomID, dbRefs, buffer, true);
		buffer += CDbXML::getXMLfooter();
		post["RoomData"] = buffer;
	}

	return getRequest(url, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::UploadDemo(
//Send a buffer to CaravelNet of a recorded demo.
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const CNetRoom& room,
	const WSTRING& playerName,
	const string& demo,
	const UINT wMoveCount,
	const UINT dwTimeElapsed,	//in 10ms increments
	const UINT dwCreatedTime)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	const string b64 = getCompressedEncodedBuffer(demo);

	post["action"] = "UploadDemo";
	ASSERT(unsigned(b64.length()) == b64.length());
	post["Demo"] = b64;

	Json::Value* roomData = room.ToJson();
	post["Room"] = *roomData;
	delete roomData;

	post["PlayerName"] = Base64::encode(playerName);
	post["MoveCount"] = wMoveCount;
	post["RealTime"] = dwTimeElapsed;
	post["CreatedTime"] = dwCreatedTime;

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::UploadChallengeDemo(
//Send a buffer to CaravelNet of a demo of a challenge being completed
//Params:
	const CNetRoom& room,	// Reference for the hold
	const string& demo)	// Demo data
// Returns:
//	True if it was successfully added to the database (*not* validated)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	const string b64 = getCompressedEncodedBuffer(demo);

	post["action"] = "UploadChallengeDemo";
	ASSERT(unsigned(b64.length()) == b64.length());
	post["Demo"] = b64;

	Json::Value* roomData = room.ToJson();
	post["Room"] = *roomData;
	delete roomData;

	return getRequest(cNetStr, post);
}


//*****************************************************************************
UINT CCaravelNetInterface::UploadDemos(
//Send a buffer to CaravelNet of one or more recorded demos.
//
//Returns: Whether upload was sent successfully.
//
//Params:
	const string& text)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	const string b64 = getCompressedBuffer(text);
	if (b64.empty())
		return 0;

	post["action"] = "UploadDemos";
	ASSERT(unsigned(b64.length()) == b64.length());
	post["Demos"] = b64;
 
	UINT handle = 0;
	const bool bSuccess = CInternet::HttpGet(cNetStr, post, &handle);
	return bSuccess ? handle : 0;
}

//*****************************************************************************
bool CCaravelNetInterface::UploadExploredRooms(const string& buffer)
//Send a buffer to CaravelNet of a player's set of explored+conquered rooms (in a saved game).
//
//Returns: Whether upload was sent successfully.
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	const string b64 = getCompressedBuffer(buffer);
	if (b64.empty())
		return 0;

	post["action"] = "UploadSavedGames";
	ASSERT(unsigned(b64.length()) == b64.length());
	post["Player"] = b64;
	return CInternet::HttpGet(cNetStr, post);
}

//*****************************************************************************
bool CCaravelNetInterface::UploadExploredRoom(const CNetRoom& room)
//Send a buffer to CaravelNet of a newly explored room.
//
//Returns: Whether upload was sent successfully.
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	post["action"] = "UploadExploredRoom";
	Json::Value* pRoom = room.ToJson();
	post["Room"] = *pRoom;
	delete pRoom;
	return CInternet::HttpGet(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::RateHold(
//Rates the hold with specified ID.
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const WCHAR* holdName, const float fDifficulty, const float fOverall)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	const string b64 = Base64::encode((BYTE*)holdName, WCSlen(holdName)*sizeof(WCHAR));

	post["action"] = "RateHold";
	ASSERT(unsigned(b64.length()) == b64.length());
	post["HoldName"] = b64;
	post["Difficulty"] = fDifficulty;
	post["Overall"] = fOverall;

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::RateMod(
//Rates the mod with specified name.
//
//Returns: A handle to use to get the response, or 0 on failure.
//
//Params:
	const WCHAR* name, const float fRating)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	string b64 = Base64::encode((BYTE*)name, WCSlen(name)*sizeof(WCHAR));

	post["action"] = "RateMod";
	post["Name"] = b64;
	post["Rating"] = fRating;

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::RequestNewKey(const string& strUser)
//Send a request to CaravelNet to send the user a new key
//
//Returns: A handle to use to get the response, or 0 on failure.
{
	ClearActiveAction();
	if (!IsEnabled())
		return 0; //don't request a key when disabled

	Json::Value post;
	CCaravelNetInterface::SetCredentials(post, strUser, "", false);

	post["action"] = "reqnewkey";

	return getRequest(cNetStr, post);
}

//*****************************************************************************
CNetResult* CCaravelNetInterface::GetResults(
//Retrieve the buffer that corresponds to the given handle
//
//Returns: A buffer sent as response to the specified request, or NULL on failure.
//
//Params:
	const UINT handle) // (in) handle of the download to get the results of
{
	if (!handle)
		return NULL;

	//Check for cached result first.
	std::map<UINT, CNetResult*>::iterator iter = this->results.find(handle);
	if (iter != this->results.end())
	{
		CNetResult* pResult = iter->second;
		this->results.erase(iter);
		return pResult;
	}

	//Get query results.
	const int status = GetStatus(handle);
	CStretchyBuffer* pBuffer = CInternet::GetResults(handle);

	//If there was a receiving error, mention it,
	//but retrieve new key if possible from received buffer before failing.
	if (status > 0)
		CInternet::OutputError(handle);

	if (!pBuffer)
		return NULL;

	CNetResult* pResult = new CNetResult;
	pResult->pBuffer = pBuffer;

	Json::Reader reader;
	Json::Value* pRoot = new Json::Value;
	const char* pBytes = (const char*)(const BYTE*)*pBuffer;
	bool bSuccess = reader.parse(pBytes, pBytes + pBuffer->Size(), *pRoot);
	if (!bSuccess) {
		// Indicates a non-JSON result.
		delete pRoot;
	} else {
		//Save the new password into the current user's profile.
		CDbPlayer *pPlayer = g_pTheDB->GetCurrentPlayer();
		if (!pPlayer)
		{
			ASSERT(!"ERROR - Key lost?  GetResults was called with no current player.");
		} else {
			WSTRING wstrKey;

			const string newKey = pRoot->get("NewKey", "default").asString();

			if (VerifyKeyFormat(newKey))
			{
				AsciiToUnicode(newKey.c_str(), wstrKey);
				pPlayer->CNetPasswordText = wstrKey.c_str();
				pPlayer->Update();
			}
			delete pPlayer;
		}
		pResult->pJson = pRoot;
		delete pBuffer;
		pResult->pBuffer = NULL;

		ParseNotices(*pResult->pJson);
		ParseVersion(*pResult->pJson);
	}

	//Now that the new key has been retrieved, we can fail if there was an error.
	if (status > 0)
	{
		delete pResult;
		return NULL;
	}
	CCaravelNetInterface::currentHandles -= handle;
	return pResult;
}

//*****************************************************************************
void CCaravelNetInterface::ParseNotices(
// Parse notices out of the JSON response from any request
//
// Params:
	const Json::Value& root) // The root of the JSON object
{
	if (root.isMember("notices")) {
		const Json::Value& notices = root["notices"];
		for (UINT x = 0; x < notices.size(); x++) {
			CNetNotice n;
			n.id = notices[x].get("id", 0).asInt();
			n.type = notices[x].get("type", -1).asUInt();
			n.dwServerHoldId = notices[x].get("holdId", 0).asInt();
			AsciiToUnicode(notices[x].get("title", "").asString(), n.title);
			AsciiToUnicode(notices[x].get("text", "").asString(), n.text);
			if (notices[x].isMember("url")) {
				n.url = notices[x]["url"].asString();
			}
			n.dwCloudId = notices[x].get("cloudId", 0).asUInt();
			if (notices[x].isMember("hold")) {
				Json::Value room = notices[x]["hold"];
			}

			this->notices[n.id] = n;
			if (n.id > this->dwLastNotice) this->dwLastNotice = n.id;
		}
	}
}

//*****************************************************************************
void CCaravelNetInterface::SetDownloadHold(
// When a notice is received that a new/updated hold is available, this is set to allow the
// manage holds screen to find the appropriate hold
//
// Params:
	const UINT dwServerHoldId)
{
	this->dwDownloadHoldId = dwServerHoldId;
}

//*****************************************************************************
UINT CCaravelNetInterface::GetDownloadHold() const
// Return the ID of the hold previously selected from a Notice
{
	std::map<UINT, UINT>::const_iterator it = this->serverHoldIdToListboxId.find(this->dwDownloadHoldId);
	return it == this->serverHoldIdToListboxId.end() ? 0 : it->second;
}


//*****************************************************************************
void CCaravelNetInterface::ParseVersion(
// Parse version updates out of the JSON response from any request
//
// Params:
	const Json::Value& root) // The root of the JSON object
{
	if (root.isMember("version")) {
		// The idle queue handles these.
		g_cloudQueueOut.push_back(root);
	}
}

//*****************************************************************************
void CCaravelNetInterface::MatchCNetHolds()
//Mark which CNet holds are installed locally and also
//those that have a newer version online.
{
	this->betaHolds.clear();
	this->localHolds.clear();
	this->updatedHolds.clear();
	serverHoldIdToListboxId.clear();

	CDbPlayer *pPlayer = g_pTheDB->GetCurrentPlayer();

	for (UINT wIndex=this->cNetMedia.size(); wIndex--; )
	{
		CNetMedia& holdData = *(this->cNetMedia[wIndex]);

		//Is the current player the CaravelNet author of this item?
		if (pPlayer && !WCScmp((const WCHAR*)pPlayer->CNetNameText, (const WCHAR*)holdData.CNetNameText))
			holdData.bPlayerIsAuthor = true;

		//Only consider holds.
		if (holdData.mediaType != MT_Hold)
			continue;

		const UINT dwHoldID = g_pTheDB->Holds.GetHoldID(holdData.Created,
				holdData.HoldNameText, holdData.OrigAuthorText);
		holdData.localHoldID = dwHoldID;

		if (!dwHoldID) {
			serverHoldIdToListboxId[(UINT)holdData.lHoldID] = (UINT)holdData.lHoldID;
			continue;
		}

		CDbHold *pLocalHold = g_pTheDB->Holds.GetByID(dwHoldID, true);
		ASSERT(pLocalHold);

		serverHoldIdToListboxId[(UINT)holdData.lHoldID] = pLocalHold->dwHoldID;

		//Holds only match if they are the exact same version,
		//or if local hold is a newer version of a CaravelNet hold in beta.
		if (holdData.LastModified == pLocalHold->LastUpdated ||
				(holdData.LastModified < pLocalHold->LastUpdated && holdData.bBeta))
			this->localHolds += dwHoldID;

		//Compare timestamps to decide which hold is newer.
		//If forum hold is newer, mark it as updated.
		if (holdData.LastModified > pLocalHold->LastUpdated)
			this->updatedHolds += dwHoldID;

		//Track holds in beta.
		if (holdData.bBeta)
			this->betaHolds += dwHoldID;

		//Check Cloud status, see if we need to download progress.
		const UINT localCloudVersion = GetCloudVersionID(Settings::CloudHoldDemosVersion, dwHoldID);
		if (localCloudVersion < holdData.wCloudDemosVer) {
			// Download and merge
			this->updatedCloudDemosHolds += dwHoldID;
		}

		delete pLocalHold;
	}

	delete pPlayer;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
//  Cloud data
//
//  A chunk of cloud data has a type - Types 'Hold' and 'Demos' are associated with holds,
//   any other types will be identified by only the type - i.e.  one chunk of cloud data
//   for any non-hold, non-demo type.
//
//////////////////////////////////////////////////////////////////////////////////////////

//*****************************************************************************
UINT CCaravelNetInterface::CloudUploadMyHold(const UINT dwHoldID)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post))
		return 0;

	CDbHold* pHold = g_pTheDB->Holds.GetByID(dwHoldID);
	if (!pHold)
		return 0;

	string holdXML;
	if (!CDbXML::ExportXML(V_Holds, dwHoldID, holdXML)) {
		delete pHold;
		return 0;
	}

	CNetRoom room;
	room.FillFromHold(pHold);
	delete pHold;

	const UINT version = GetCloudVersionID(Settings::CloudHoldVersion, dwHoldID);

	return CloudUploadData(CLOUD_HOLD, room, version, holdXML);
}

//*****************************************************************************
UINT CCaravelNetInterface::CloudUploadProgress(const UINT dwHoldId)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post))
		return 0;

	CDbHold* pHold = g_pTheDB->Holds.GetByID(dwHoldId);
	if (!pHold)
		return 0; // Invalid hold!

	const CIDSet savesInHold = CDb::getSavedGamesInHold(dwHoldId);
	const UINT dwCurrentPlayerID = g_pTheDB->GetPlayerID();

	CIDSet playerSaveIDs;
	for (CIDSet::const_iterator it=savesInHold.begin(); it!=savesInHold.end(); ++it)
	{
		if (CDbSavedGames::GetPlayerIDofSavedGame(*it) == dwCurrentPlayerID)
			playerSaveIDs += *it;
	}

	CDb db;
	db.Demos.FilterByPlayer(dwCurrentPlayerID);
	db.Demos.FilterByHold(dwHoldId);
	db.Demos.FindHiddens(true);
	const CIDSet playerDemoIDs = db.Demos.GetIDs();

	if (playerSaveIDs.empty() && playerDemoIDs.empty()) {
		delete pHold;
		return 1; //success -- nothing to do
	}

	CDbXML::ViewIDMap viewIDs;
	viewIDs[V_SavedGames] = playerSaveIDs;
	viewIDs[V_Demos] = playerDemoIDs;

	string savesXML;
	if (!CDbXML::ExportXML(viewIDs, savesXML)) {
		delete pHold;
		return 0;
	}

	savesXML = CCaravelNetInterface::getCompressedBuffer(savesXML);
	if (savesXML.empty()) {
		delete pHold;
		return 0;
	}

	const WCHAR *pHoldAuthor = pHold->GetAuthorText();
	const string b64Author = Base64::encode((BYTE*)pHoldAuthor, WCSlen(pHoldAuthor)*sizeof(WCHAR));
	const WCHAR *pHoldName = (const WCHAR*)pHold->NameText;
	const string b64HoldName = Base64::encode((BYTE*)pHoldName, WCSlen(pHoldName)*sizeof(WCHAR));

	post["action"] = "CloudUploadHoldDemos";
	Json::Value* obj = new Json::Value(Json::objectValue);
	Json::Value& hold = post["hold"] = *obj;
	hold["localID"] = dwHoldId;
	hold["created"] = (Json::Value::Int64)pHold->GetCreated();
	hold["lastUpdated"] = (Json::Value::Int64)pHold->LastUpdated;
	hold["architect"] = b64Author;
	hold["name"] = b64HoldName;
	hold["data"] = savesXML;

	const UINT version = GetCloudVersionID(Settings::CloudHoldDemosVersion, dwHoldId);
	hold["version"] = version;

	delete pHold;

	AddToCloudQueue(post);
	return 1;
}


//*****************************************************************************
UINT CCaravelNetInterface::CloudDownloadProgress(const UINT dwHoldID)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	CDbHold* pHold = g_pTheDB->Holds.GetByID(dwHoldID);
	if (pHold) {
		const CNetRoom room(pHold);
		const WCHAR *pHoldName = (const WCHAR*)pHold->NameText;
		const string b64HoldName = Base64::encode((BYTE*)pHoldName, WCSlen(pHoldName)*sizeof(WCHAR));

		delete pHold;

		post["action"] = "CloudDownloadHoldDemos";
		Json::Value* holdData = room.ToJson();
		(*holdData)["name"] = b64HoldName;
		post["hold"] = *holdData;
		delete holdData;

		return getRequest(cNetStr, post);
	}
	return 0;
}


//*****************************************************************************
UINT CCaravelNetInterface::CloudUploadData(CloudDataType type, const CNetRoom& room, const int version, const string& data)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post)) return 0;

	post["action"] = "CloudUpload";
	post["Type"] = static_cast<int>(type);
	Json::Value* holdData = room.ToJson();
	post["Room"] = *holdData;
	post["Data"] = data;
	post["Version"] = version;
	delete holdData;

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::CloudInitialize()
// Create a minimal player, a list of all holds, and (optional?) a list of demos per hold.
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post))
		return 0;

	post["action"] = "CloudInitialize";
	const UINT playerID = g_pTheDB->GetPlayerID();
	if (!playerID)
		return 0;

	string playerXML;
	if (!CDbXML::ExportXML(V_Players, playerID, playerXML, ST_Cloud))
		return 0;

	post["player"] = playerXML;
	Json::Value* holds = new Json::Value(Json::arrayValue);
	post["holds"] = *holds;
	const UINT dwHoldCount = g_pTheDB->Holds.GetViewSize();

	//Each iteration checks a hold's GIDs.
	for (UINT dwHoldI = 0; dwHoldI < dwHoldCount; ++dwHoldI) {
		c4_RowRef row = g_pTheDB->Holds.GetRowRef(V_Holds, dwHoldI);
		const UINT dwHoldID = UINT(p_HoldID(row));
		CDbHold* pHold = g_pTheDB->Holds.GetByID(dwHoldID);
		if (!pHold)
			continue;

		const WCHAR *pHoldAuthor = pHold->GetAuthorText();
		const string b64Author = Base64::encode((BYTE*)pHoldAuthor, WCSlen(pHoldAuthor)*sizeof(WCHAR));
		const WCHAR *pHoldName = (const WCHAR*)pHold->NameText;
		const string b64HoldName = Base64::encode((BYTE*)pHoldName, WCSlen(pHoldName)*sizeof(WCHAR));

		Json::Value* obj = new Json::Value(Json::objectValue);
		Json::Value& hold = post["holds"][dwHoldI] = *obj;
		hold["created"] = (Json::Value::Int64)pHold->GetCreated();
		hold["lastUpdated"] = (Json::Value::Int64)pHold->LastUpdated;
		hold["architect"] = b64Author;
		hold["name"] = b64HoldName;

		delete pHold;
	}

	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::CloudGetPlayer(const WCHAR* pUsername, const WCHAR* pKey)
{
	Json::Value post;
	if (pUsername == NULL) {
		if (!CCaravelNetInterface::SetCredentials(post))
			return 0;
	} else {
		const string username = UnicodeToUTF8(pUsername);
		const string key = UnicodeToUTF8(pKey);
		if (!SetCredentials(post, username, key)) {
			return 0;
		}
	}

	post["action"] = "CloudGetPlayer";
	return getRequest(cNetStr, post);
}

//*****************************************************************************
UINT CCaravelNetInterface::CloudUploadGeneralHold(const UINT dwHoldId)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post))
		return 0;

	CDbHold* pHold = g_pTheDB->Holds.GetByID(dwHoldId);
	if (!pHold)
		return 0; // Invalid hold!

	//Don't attempt to upload official holds as user holds for performance and stability reasons
	if (pHold->status != CDbHold::Homemade) {
		delete pHold;
		return 0;
	}

	string holdXML;
	if (!CDbXML::ExportXML(V_Holds, dwHoldId, holdXML)) {
		delete pHold;
		return 0;
	}

	holdXML = CCaravelNetInterface::getCompressedBuffer(holdXML);
	if (holdXML.empty()) {
		delete pHold;
		return 0;
	}

	CDbPlayer* pPlayer = g_pTheDB->Players.GetByID(pHold->dwPlayerID);
	const WCHAR *pOrigAuthor = pPlayer->OriginalNameText;
	const string b64OrigAuthor = Base64::encode((BYTE*)pOrigAuthor, WCSlen(pOrigAuthor)*sizeof(WCHAR));
	delete pPlayer;
	const WCHAR *pHoldName = (const WCHAR*)pHold->NameText;
	const string b64HoldName = Base64::encode((BYTE*)pHoldName, WCSlen(pHoldName)*sizeof(WCHAR));

	post["action"] = "CloudUploadGeneralHold";
	Json::Value* obj = new Json::Value(Json::objectValue);
	Json::Value& hold = post["hold"] = *obj;
	hold["localID"] = dwHoldId;
	hold["created"] = (Json::Value::Int64)pHold->GetCreated();
	hold["lastUpdated"] = (Json::Value::Int64)pHold->LastUpdated;
	hold["architect"] = b64OrigAuthor;
	hold["name"] = b64HoldName;
	hold["data"] = holdXML;

	delete pHold;

	AddToCloudQueue(post);
	return 0;
}
//*****************************************************************************
UINT CCaravelNetInterface::CloudDownloadGeneralHold(const Json::Value& hold)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post))
		return 0;

	post["action"] = "CloudDownloadGeneralHold";
	post["hold"] = hold;

	return getRequest(cNetStr, post);
}

void CCaravelNetInterface::AddToCloudQueue(const Json::Value& json)
{
	SDL_mutexP(g_cloudMutex);
	g_cloudQueue.push_back(json);
	SDL_mutexV(g_cloudMutex);
}

//*****************************************************************************
UINT CCaravelNetInterface::CloudSetHoldInstalled(const UINT dwHoldId, bool bInstalled)
{
	Json::Value post;
	if (!CCaravelNetInterface::SetCredentials(post))
		return 0;

	CDbHold* pHold = g_pTheDB->Holds.GetByID(dwHoldId);
	if (!pHold)
		return 0; // Invalid hold!

	const WCHAR *pHoldAuthor = pHold->GetAuthorText();
	const string b64Author = Base64::encode((BYTE*)pHoldAuthor, WCSlen(pHoldAuthor)*sizeof(WCHAR));
	const WCHAR *pHoldName = (const WCHAR*)pHold->NameText;
	const string b64HoldName = Base64::encode((BYTE*)pHoldName, WCSlen(pHoldName)*sizeof(WCHAR));

	post["action"] = "CloudSetHoldInstalled";
	Json::Value* obj = new Json::Value(Json::objectValue);
	Json::Value& hold = post["hold"] = *obj;
	hold["localID"] = dwHoldId;
	hold["created"] = (Json::Value::Int64)pHold->GetCreated();
	hold["lastUpdated"] = (Json::Value::Int64)pHold->LastUpdated;
	hold["architect"] = b64Author;
	hold["name"] = b64HoldName;
	hold["installed"] = bInstalled;

	delete pHold;

	AddToCloudQueue(post);

	return 0;
}

//*****************************************************************************
// This is called in the main thread any time a CaravelNet connection is made.
// It checks for responses from the Cloud thread that require further processing.
// All db access is made in the main thread - the Cloud thread only monitors
// sending of queued cloud requests and has no interaction or knowledge of game
// data.  This function will block the Cloud thread while copying the queue to
// a local copy and then clearing it.
void CCaravelNetInterface::CloudIdle()
{
	SDL_mutexP(g_cloudMutex);
	std::deque<Json::Value> queue;
	while (g_cloudQueueOut.size() > 0) {
		queue.push_back(g_cloudQueueOut.front());
		g_cloudQueueOut.pop_front();
	}
	g_cloudQueueOut.clear();
	SDL_mutexV(g_cloudMutex);

	for (std::deque<Json::Value>::iterator c = queue.begin(); c != queue.end(); c++) {
		Json::Value& json = *c;

		if (json.isMember("holdsNeeded")) {
			// Need to upload these holds.  Add them to the queue!
			const Json::Value& holds = json["holdsNeeded"];
			for (UINT x = 0; x < holds.size(); ++x) {
				CDbMessageText author, holdName;
				const CDate date(holds[x].get("created", 0).asInt());
				WSTRING wstr;
				Base64::decode(holds[x].get("architect", "").asString(), wstr);
				author = wstr.c_str();

				Base64::decode(holds[x].get("name", "").asString(), wstr);
				holdName = wstr.c_str();

				UINT dwHoldId = g_pTheDB->Holds.GetHoldID(date, holdName, author);
				if (dwHoldId > 0) {
					g_pTheNet->CloudUploadGeneralHold(dwHoldId);
				}
			}
		}

		if (json.isMember("version") && json["version"].isMember("hold")) {
			// {"status":0,"version":{"version":1,"hold":{"architect":"TQBvAHIAaQBvAHIA","created":1176930130,"lastUpdated":1182995816,"localID":10610,"name":"QQBsAGIAdQBtAA=="}}}
			// find hold json["version->hold...."].  set its version to json["version->version"]
			Json::Value hold = json["version"]["hold"];

			CDbMessageText author, holdName;
			const CDate date(hold.get("created", 0).asInt());
			WSTRING wstr, auth;
			Base64::decode(hold.get("architect", "").asString(), wstr);
			author = wstr.c_str();

			Base64::decode(hold.get("name", "").asString(), wstr);
			holdName = wstr.c_str();

			UINT version = json["version"].get("version", 0).asUInt();

			const UINT holdID = g_pTheDB->Holds.GetHoldID(date, holdName, author);
			if (holdID > 0) {
				SetCloudVersionID(Settings::CloudHoldDemosVersion, holdID, version);
				g_pTheNet->SetCloudDemosCurrent(holdID);
			}
		}
	}
}

//*****************************************************************************
vector<string> Tokenize(const string &str, const char delim)
{
    vector<string> tokens;

    size_t p0 = 0, p1 = string::npos;
    while (p0 != string::npos)
    {
    	p1 = str.find_first_of(delim, p0);
    	if (p1 != p0)
    	{
    		const string token = str.substr(p0, p1 - p0);
    		tokens.push_back(token);
    	}
    	p0 = str.find_first_not_of(delim, p1);
    }

    return tokens;
}

typedef map<UINT,UINT> CloudVersionMap;
CloudVersionMap getKVpairs(const string& str)
{
	CloudVersionMap pairs;

	const vector<string> kvPairs = Tokenize(str, ',');
	for (vector<string>::const_iterator it=kvPairs.begin(); it!=kvPairs.end(); ++it)
	{
		const string& pair = *it;
		const size_t equal = pair.find_first_of('=');
		if (equal != string::npos) {
			const UINT key = atoi(pair.c_str());
			const UINT val = atoi(pair.c_str() + equal+1);
			pairs[key] = val;
		}
	}

	return pairs;
}

//*****************************************************************************
UINT getVersionForID(const string& str, const UINT id)
{
	const CloudVersionMap pairs = getKVpairs(str);
	CloudVersionMap::const_iterator it = pairs.find(id);
	return it!=pairs.end() ? it->second : 0;
}

//*****************************************************************************
string setVersionForID(const string& str, const UINT id, const UINT version) {
	CloudVersionMap pairs = getKVpairs(str);
	CloudVersionMap::iterator it;
	const int BUFSIZE = 32;
	char buffer[BUFSIZE];
	pairs[id] = version;

	string final;
	for (it = pairs.begin(); it != pairs.end(); it++) {
		if (it != pairs.begin()) final += ",";
		snprintf(buffer, BUFSIZE, "%u", unsigned(it->first));
		final += buffer;
		final += "=";
		snprintf(buffer, BUFSIZE, "%u", unsigned(it->second));
		final += buffer;
	}
	return final;
}

//*****************************************************************************
UINT CCaravelNetInterface::GetCloudVersionID(const char* settingsKey, const UINT id) const
{
	const CDbPackedVars settings = g_pTheDB->GetCurrentPlayerSettings();
	const char* versionIDs = settings.GetVar(settingsKey, (char *)NULL);
	if (!versionIDs)
		return 0;

	return getVersionForID(string(versionIDs), id);
}

//*****************************************************************************
bool CCaravelNetInterface::SetCloudVersionID(const char* settingsKey, const UINT id, const UINT version) const
{
	CDbPlayer *pCurrentPlayer = g_pTheDB->GetCurrentPlayer();

	if (pCurrentPlayer)
	{
		CDbPackedVars& vars = pCurrentPlayer->Settings;
		const char* versionIDs = vars.GetVar(settingsKey, (char*)NULL);
		if (!versionIDs) {
			vars.SetVar(settingsKey, "");
			versionIDs = "";
		}

		string newVersions = setVersionForID(string(versionIDs), id, version);
		vars.SetVar(settingsKey, newVersions.c_str());
		pCurrentPlayer->Update();
		delete pCurrentPlayer;
	}

	return true;
}

//*****************************************************************************
//Ensure data received are current before returning anything.
vector<CNetMedia*>& CCaravelNetInterface::GetCNetMedia()
{
	WaitUntilHoldsReceived();
	return CNetInterface::GetCNetMedia();
}

CIDSet& CCaravelNetInterface::GetBetaHolds()
{
	WaitUntilHoldsReceived();
	return CNetInterface::GetBetaHolds();
}

CIDSet& CCaravelNetInterface::GetLocalHolds()
{
	WaitUntilHoldsReceived();
	return CNetInterface::GetLocalHolds();
}

CIDSet& CCaravelNetInterface::GetUpdatedHolds()
{
	WaitUntilHoldsReceived();
	return CNetInterface::GetUpdatedHolds();
}

//*****************************************************************************
bool CCaravelNetInterface::IsLoggedIn()
//Returns: Whether user is logged in to CaravelNet
{
	const vector<CNetMedia*>& media = GetCNetMedia();
	return !media.empty();
}

//
// Protected methods.
//

//*****************************************************************************
void CCaravelNetInterface::AddHoldsOnCNet()
//Adds holds published on CaravelNet to holds list box.
{
	ClearCNetHolds();

	const Json::Value holds = this->CNetHoldBuffer["holds"];
	
	for (unsigned int i = 0; i < holds.size(); i++) {
		WSTRING wStr;
		string str;
		CNetMedia *pDatum = new CNetMedia;

		const Json::Value& hold = holds[i];
		
		AsciiToUnicode(hold.get("Author", "Unknown").asString(), wStr);
		pDatum->AuthorText = wStr.c_str();

		Base64::decode(hold.get("OriginalAuthorB64", "").asString(), wStr);
		pDatum->OrigAuthorText = wStr.c_str();
		
		pDatum->wVersion = hold.get("Version", 401).asInt();
		pDatum->Created = (time_t)hold.get("GID_Created", 0).asLargestInt();
		pDatum->LastModified = (time_t)hold.get("LastUpdated", 0).asLargestInt();
		pDatum->thumbnailURL = hold.get("ThumbnailURL", "").asString();

		AsciiToUnicode(hold.get("ForumName", "Unknown").asString(), wStr);
		pDatum->CNetNameText = wStr.c_str();

		Base64::decode(hold.get("NameMessageB64", "").asString(), wStr);
		pDatum->HoldNameText = wStr.c_str();
		pDatum->lHoldID = hold.get("HoldId", 0).asInt();
		pDatum->status = hold.get("Status", 0).asInt();
		pDatum->difficulty = hold.get("Difficulty", "").asString();
		pDatum->rating = hold.get("Rating", "").asString();
		pDatum->numVotes = hold.get("NumVotes", "").asString();
		pDatum->filesize = hold.get("FileSize", "0").asString();
		pDatum->filesizebytes = hold.get("FileSizeBytes", "0").asString();
		pDatum->myDifficulty = hold.get("MyDifficulty","0").asString();
		pDatum->myRating = hold.get("MyRating", "0").asString();
		pDatum->bBeta = hold.get("Beta", false).asBool();
		pDatum->mediaType = (MediaType)hold.get("Media", 0).asInt();

		pDatum->wCloudDemosVer = hold.get("cloudDemoId", 0).asUInt();
		pDatum->bCloudInstalled = hold.get("cloudInstalled", false).asBool();

		this->cNetMedia.push_back(pDatum);
	}

	MatchCNetHolds();
}

//*****************************************************************************
void CCaravelNetInterface::QueryNotices(vector<CNetNotice> &notices, UINT typeMask, UINT lastId) const
{
	for (std::map<UINT, CNetNotice>::const_iterator i = this->notices.begin(); i != this->notices.end(); i++) {
		if (lastId < i->first && ((i->second.type & typeMask) != 0)) {
			notices.push_back(i->second);
		}
	}
}


//*****************************************************************************
void CCaravelNetInterface::ClearResults()
{
//	for (std::map<UINT, CNetResult>::iterator iter = results.begin();
//		iter != results.end(); ++iter)
//	{
//		delete iter->second;
//	}
	results.clear();
}

//*****************************************************************************
void CCaravelNetInterface::WaitUntilHoldsReceived()
//When requested, wait until hold list is received.
{
	if (!this->wHoldHandle) return;

	while (CInternet::GetStatus(this->wHoldHandle) < 0)
		SDL_Delay(20);

	CNetResult* pResult = GetResults(this->wHoldHandle);
	this->wHoldHandle = 0;
	if (pResult)
	{
		if (pResult->pJson)
		{
			this->CNetHoldBuffer = *pResult->pJson;
			AddHoldsOnCNet();
		}
		delete pResult;
	}
	// else the call failed.  Don't try again.
}

//*****************************************************************************
bool CCaravelNetInterface::SetCredentials(Json::Value& json, bool bVerify)
//Set the cookies to be the current user's name and key
//
//Returns: false if there is no current player, if the player's key is not
// in a valid format, of if the player turned off CaravelNet connectivity
{
	if (!IsEnabled())
		return false; //don't allow initing any requests when disabled

	CDbPlayer *pPlayer = g_pTheDB->GetCurrentPlayer();
	if (!pPlayer)
		return false;

	//Only allow if player has Internet connectivity set.
	if (pPlayer->Settings.GetVar(Settings::ConnectToInternet, false) == false)
	{
		delete pPlayer;
		return false;
	}

	//Provide player GUID as player cloud id.
	const string originalName = UnicodeToUTF8((const WCHAR*)pPlayer->CNetNameText);
	json["OriginalName"] = originalName;
	json["Created"] = (Json::Value::Int64)pPlayer->Created;

	const string strName = UnicodeToUTF8((const WCHAR*)pPlayer->CNetNameText);
	const string strKey = UnicodeToUTF8((const WCHAR*)pPlayer->CNetPasswordText);
	delete pPlayer;

	return SetCredentials(json, strName, strKey, bVerify);
}

//*****************************************************************************
bool CCaravelNetInterface::SetCredentials(Json::Value& json, const string& strUser, const string& strKey, bool bVerify)
{
	CloudIdle();

	if (bVerify && !VerifyKeyFormat(strKey)) return false;

	json["UserName"] = strUser;
	json["Key"] = strKey;
	json["LastNotice"] = this->dwLastNotice;

	// Also add the version, as every single request should do this
	string str;
	UnicodeToUTF8(wszVersionReleaseNumber, str);
	json["Version"] = str;

	// Also add the specific hold data version
	json["HoldVersion"] = std::to_string(VERSION_NUMBER);

#ifdef STEAMBUILD
	json["steam"] = true;
#endif

	return true;
}

//*****************************************************************************
bool CCaravelNetInterface::VerifyKeyFormat(
//Checks a key to make sure it's in the correct format.
//
//Returns: true if it's valid, false otherwise
//
//Params:
	const string& strKey) // (in)  The key to check
{
	// If the key isn't the right size, it's not valid.
	if (strKey.length() != cMD5_HASH_SIZE)
		return false;
	for (string::const_iterator iter = strKey.begin(); iter != strKey.end(); iter++) {
		// can only contain alphanumeric characters.
		if (!isalnum(*iter))
			return false;
	}
	return true;
}

bool CCaravelNetInterface::IsHoldInCloudQueue(const UINT holdID) const
{
	bool ret = false;

	SDL_mutexP(g_cloudMutex);
	ASSERT(!g_bCloseCloud);
	for (std::deque<Json::Value>::const_iterator it=g_cloudQueue.begin();
			it!=g_cloudQueue.end(); ++it)
	{
		const Json::Value& post = *it;
		if (post.isMember("hold")) {
			const Json::Value& hold = post.get("hold", 0);
			if (hold.get("localID", 0).asUInt() == holdID) {
				ret = true;
				break;
			}
		}
	}
	SDL_mutexV(g_cloudMutex);

	return ret;
}
