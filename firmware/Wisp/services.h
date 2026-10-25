#pragma once
// Storage, provisioning, and voice operations. Only the main loop mutates UI/state.
void addTask(String); void render(); bool authorized(); bool parseBody(JsonDocument&,size_t); void jsonReply(JsonDocument&); void setupAP(); void rtcSave();
bool safeFilename(const String& name){if(!name.length()||name.length()>100||name.indexOf("..")>=0)return false;for(unsigned i=0;i<name.length();i++){char c=name[i];if(!isalnum((unsigned char)c)&&c!='.'&&c!='_'&&c!='-'&&c!=' ')return false;}return true;}
String bookKey(const String& name){uint32_t h=2166136261u;for(unsigned i=0;i<name.length();i++){h^=(uint8_t)name[i];h*=16777619u;}return "b"+String(h,HEX);}
void scanBooks(){bookCount=0;if(!sdOK)return;File dir=SD_MMC.open("/books");if(!dir)return;File f;while((f=dir.openNextFile())&&bookCount<64){String name=f.name();name=name.substring(name.lastIndexOf('/')+1);if(!f.isDirectory()&&name.endsWith(".txt"))books[bookCount++]=name;f.close();}for(int i=0;i<bookCount;i++)for(int j=i+1;j<bookCount;j++)if(books[j]<books[i]){String t=books[i];books[i]=books[j];books[j]=t;}bookSelection=constrain(bookSelection,0,max(0,bookCount-1));}
void restoreNoteArchive(){
 if(!sdOK)return;File dir=SD_MMC.open("/notes");File f;JsonArray notes=state["notes"].as<JsonArray>();while((f=dir.openNextFile())&&notes.size()<40){String name=f.name();if(!f.isDirectory()&&name.endsWith(".json")&&f.size()<12000){JsonDocument d;if(!deserializeJson(d,f)){String id=d["id"]|"";bool exists=false;for(JsonObject n:notes)if(n["id"].as<String>()==id)exists=true;if(id.length()&&!exists)notes.add(d.as<JsonObject>());}}f.close();}}
void serviceNetwork(){
 bool connected=WiFi.status()==WL_CONNECTED;if(connected&&!wasConnected){connectedAt=millis();MDNS.end();MDNS.begin("wisp");MDNS.addService("http","tcp",80);notice="Online: http://wisp.local";dirty=true;}
 if(!connected&&wasConnected){notice="Reconnecting to saved Wi-Fi...";dirty=true;}
 wasConnected=connected;
 if(connected&&apActive&&millis()-connectedAt>120000){dns.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_STA);apActive=false;dirty=true;}
}
struct VoiceJob {uint8_t* wav;size_t size;String id,key,model,provider,url,token,text,error;};
QueueHandle_t voiceResults=nullptr;bool voiceBusy=false;
void transcribeWorker(void* arg){
 VoiceJob* job=(VoiceJob*)arg;HTTPClient h;h.setTimeout(60000);
 if(job->provider=="bridge"){
 h.begin(job->url+"/voice");h.addHeader("Authorization","Bearer "+job->token);h.addHeader("Content-Type","audio/wav");int code=h.POST(job->wav,job->size);if(code==200){JsonDocument d;if(!deserializeJson(d,h.getString()))job->text=d["text"]|"";}else job->error="Bridge transcription failed ("+String(code)+")";h.end();
 }else if(job->provider=="gemini"){
 WiFiClientSecure client;client.setCACert(GOOGLE_ROOTS);size_t encodedLen=4*((job->size+2)/3);char* encoded=(char*)ps_malloc(encodedLen+1);if(encoded){size_t actual;mbedtls_base64_encode((unsigned char*)encoded,encodedLen+1,&actual,job->wav,job->size);encoded[actual]=0;
 const char* prefix="{\"contents\":[{\"parts\":[{\"text\":\"Transcribe this voice note faithfully. Return only the transcript, without commentary. If no intelligible speech, return an empty string.\"},{\"inline_data\":{\"mime_type\":\"audio/wav\",\"data\":\"";const char* suffix="\"}}]}],\"generationConfig\":{\"maxOutputTokens\":2048}}";
 size_t size=strlen(prefix)+actual+strlen(suffix);char* body=(char*)ps_malloc(size+1);if(body){strcpy(body,prefix);memcpy(body+strlen(prefix),encoded,actual);strcpy(body+strlen(prefix)+actual,suffix);
 h.begin(client,"https://generativelanguage.googleapis.com/v1beta/models/"+job->model+":generateContent");h.addHeader("x-goog-api-key",job->key);h.addHeader("Content-Type","application/json");int code=h.POST((uint8_t*)body,size);if(code==200){JsonDocument d;if(!deserializeJson(d,h.getString()))for(JsonObject part:d["candidates"][0]["content"]["parts"].as<JsonArray>())if(!(part["thought"]|false))job->text+=part["text"].as<String>();}else job->error="Gemini request failed ("+String(code)+"). Check key, model and clock.";h.end();free(body);}else job->error="Not enough memory for transcription";free(encoded);}else job->error="Not enough memory for audio encoding";
 }
 free(job->wav);job->wav=nullptr;if(!job->text.length()&&!job->error.length())job->error="No transcript returned; recording kept";xQueueSend(voiceResults,&job,portMAX_DELAY);vTaskDelete(nullptr);
}
void saveNoteArchive(JsonObject note){if(!sdOK)return;String id=note["id"]|"";File f=SD_MMC.open("/notes/"+id+".json","w");if(f){serializeJson(note,f);f.close();}}
void collectVoiceResult(){VoiceJob* job=nullptr;if(!voiceResults||xQueueReceive(voiceResults,&job,0)!=pdTRUE)return;for(JsonObject n:state["notes"].as<JsonArray>())if(n["id"].as<String>()==job->id){if(job->text.length()){n["text"]=job->text.substring(0,4000);n["status"]="transcribed";}else n["status"]=job->error;saveNoteArchive(n);break;}saveState();voiceStatus=job->text.length()?"Transcript saved to Notes":job->error;voiceBusy=false;dirty=true;delete job;}
void captureNote(){
 if(voiceBusy){voiceStatus="A transcription is still running";dirty=true;return;}if(!audioOK){voiceStatus="Microphone unavailable";dirty=true;return;}if(state["notes"].size()>=40){voiceStatus="Notes full. Delete a note in the browser.";dirty=true;return;}
 const size_t limit=16000*2*30;uint8_t* wav=(uint8_t*)ps_malloc(limit+44);if(!wav){voiceStatus="Not enough PSRAM for recording";dirty=true;return;}page=2;voiceStatus="RECORDING: speak now. Centre stops. 30 sec max.";render();size_t used=0;uint32_t started=millis();while(used<limit){size_t n=audio.readBytes((char*)wav+44+used,min((size_t)1024,limit-used));if(!n)break;used+=n;if(millis()-started>600&&digitalRead(5)==LOW)break;if(millis()-started>31000)break;}
 while(digitalRead(5)==LOW&&millis()-started<33000)delay(10);
 if(used<1600){free(wav);voiceStatus="No audio captured";dirty=true;return;}
 memset(wav,0,44);memcpy(wav,"RIFF",4);memcpy(wav+8,"WAVEfmt ",8);memcpy(wav+36,"data",4);auto put32=[&](int p,uint32_t v){memcpy(wav+p,&v,4);};put32(4,used+36);put32(16,16);wav[20]=1;wav[22]=1;put32(24,16000);put32(28,32000);wav[32]=2;wav[34]=16;put32(40,used);
 String id=String((uint32_t)time(nullptr))+"-"+String(esp_random(),HEX);String path="/notes/"+id+".wav";File f=sdOK?SD_MMC.open(path,"w"):LittleFS.open("/voice.wav","w");bool saved=f&&f.write(wav,used+44)==used+44;f.close();if(!saved){free(wav);voiceStatus="Storage write failed; note not saved";dirty=true;return;}
 if(!sdOK)for(JsonObject old:state["notes"].as<JsonArray>())if(old["audio"].as<String>()=="/voice.wav")old.remove("audio");
 JsonObject note=state["notes"].as<JsonArray>().add<JsonObject>();note["id"]=id;note["text"]="Voice note - awaiting transcription";note["audio"]=sdOK?path:"/voice.wav";note["status"]="recorded";saveNoteArchive(note);saveState();noteSelection=state["notes"].size()-1;lastVoice=path;
 if(voiceProvider=="off"||WiFi.status()!=WL_CONNECTED||(voiceProvider=="gemini"&&!geminiKey.length())||(voiceProvider=="bridge"&&!bridgeURL.length())){free(wav);voiceStatus="Audio saved. Configure transcription in Settings.";dirty=true;return;}
 VoiceJob* job=new VoiceJob{wav,used+44,id,geminiKey,geminiModel,voiceProvider,bridgeURL,bridgeToken,"",""};voiceBusy=true;voiceStatus="Saved. Transcribing in background...";if(xTaskCreate(transcribeWorker,"transcribe",12288,job,1,nullptr)!=pdPASS){voiceBusy=false;free(wav);delete job;voiceStatus="Could not start transcription";}dirty=true;
}
bool retryNote(JsonObject n){
 if(voiceBusy||voiceProvider=="off"||WiFi.status()!=WL_CONNECTED)return false;
 if((voiceProvider=="gemini"&&!geminiKey.length())||(voiceProvider=="bridge"&&!bridgeURL.length()))return false;
 String path=n["audio"]|"";if(!path.length())return false;File f=path.startsWith("/notes/")?(sdOK?SD_MMC.open(path):File()):LittleFS.open(path);if(!f||f.size()<44||f.size()>960044){f.close();return false;}
 size_t size=f.size();uint8_t* wav=(uint8_t*)ps_malloc(size);if(!wav){f.close();return false;}size_t got=f.read(wav,size);f.close();if(got!=size){free(wav);return false;}
 VoiceJob* job=new VoiceJob{wav,size,n["id"].as<String>(),geminiKey,geminiModel,voiceProvider,bridgeURL,bridgeToken,"",""};voiceBusy=true;
 if(xTaskCreate(transcribeWorker,"transcribe",12288,job,1,nullptr)!=pdPASS){voiceBusy=false;free(wav);delete job;return false;}voiceStatus="Transcribing saved recording...";dirty=true;return true;
}
File incoming;String incomingName;bool uploadOK=false;size_t uploadBytes=0;
void setupServicesWeb(){
 voiceResults=xQueueCreate(1,sizeof(VoiceJob*));
 web.on("/portal.js",HTTP_GET,[]{if(authorized())web.send_P(200,"application/javascript",PORTAL_JS);});
 web.on("/fflate.js",HTTP_GET,[]{web.sendHeader("Content-Encoding","gzip");web.send_P(200,"application/javascript",(const char*)FFLATE_GZ,sizeof(FFLATE_GZ));});
 for(const char* route:{"/generate_204","/hotspot-detect.html","/connecttest.txt","/ncsi.txt"})web.on(route,HTTP_GET,[]{web.sendHeader("Location","http://192.168.4.1/");web.send(302,"text/plain","");});
 web.on("/api/wifi",HTTP_GET,[]{if(!authorized())return;JsonDocument d;JsonArray a=d["networks"].to<JsonArray>();int n=WiFi.scanComplete();if(n>=0){for(int i=0;i<min(n,25);i++){JsonObject x=a.add<JsonObject>();x["ssid"]=WiFi.SSID(i);x["rssi"]=WiFi.RSSI(i);}WiFi.scanDelete();}else if(n==-2)WiFi.scanNetworks(true);d["scanning"]=n<0;jsonReply(d);});
 web.on("/api/files",HTTP_GET,[]{if(!authorized())return;JsonDocument d;d["sd"]=sdOK;JsonArray a=d["files"].to<JsonArray>();if(sdOK){d["free"]=SD_MMC.totalBytes()-SD_MMC.usedBytes();File dir=SD_MMC.open("/books");File f;int count=0;while((f=dir.openNextFile())&&count<128){if(!f.isDirectory()){JsonObject x=a.add<JsonObject>();String n=f.name();x["name"]=n.substring(n.lastIndexOf('/')+1);x["size"]=f.size();count++;}f.close();}}jsonReply(d);});
 web.on("/api/file",HTTP_GET,[]{if(!authorized())return;String n=web.arg("name");if(!sdOK||!safeFilename(n)){web.send(400,"text/plain","Invalid filename or no SD");return;}File f=SD_MMC.open("/books/"+n);if(!f){web.send(404,"text/plain","File not found");return;}web.sendHeader("Content-Disposition","attachment; filename=\""+n+"\"");web.streamFile(f,"application/octet-stream");f.close();});
 web.on("/api/file",HTTP_POST,[]{if(!authorized())return;JsonDocument d;if(!parseBody(d,500))return;String n=d["name"]|"";if(!sdOK||!safeFilename(n)){web.send(400,"text/plain","Invalid filename or no SD");return;}String op=d["action"]|"";if(op=="delete"){SD_MMC.remove("/books/"+n);if(n==activeBook)bookOpen=false;scanBooks();}else if(op=="open"&&n.endsWith(".txt")){activeBook=n;readerPage=prefs.getInt(bookKey(n).c_str(),0);bookOpen=true;page=3;dirty=true;}else {web.send(400,"text/plain","Unsupported action");return;}web.send(200,"text/plain","OK");});
 web.on("/api/upload",HTTP_POST,[]{if(!authorized())return;web.send(uploadOK?200:400,"text/plain",uploadOK?"Saved to SD":"Upload failed: check SD, name, size, or duplicate filename");},[]{
 HTTPUpload& u=web.upload();if(u.status==UPLOAD_FILE_START){uploadOK=false;uploadBytes=0;incomingName=u.filename;if(!web.authenticate("admin",pinCode.c_str())||!sdOK||!safeFilename(incomingName)||(!incomingName.endsWith(".txt")&&!incomingName.endsWith(".epub"))||SD_MMC.exists("/books/"+incomingName))return;incoming=SD_MMC.open("/books/"+incomingName+".part","w");uploadOK=(bool)incoming;}
 else if(u.status==UPLOAD_FILE_WRITE){uploadBytes+=u.currentSize;if(uploadBytes>20*1024*1024)uploadOK=false;if(uploadOK&&incoming.write(u.buf,u.currentSize)!=u.currentSize)uploadOK=false;}
 else if(u.status==UPLOAD_FILE_END||u.status==UPLOAD_FILE_ABORTED){incoming.close();if(u.status==UPLOAD_FILE_ABORTED)uploadOK=false;if(uploadOK)uploadOK=SD_MMC.rename("/books/"+incomingName+".part","/books/"+incomingName);if(!uploadOK&&safeFilename(incomingName))SD_MMC.remove("/books/"+incomingName+".part");scanBooks();dirty=true;}
 });
 web.on("/api/note",HTTP_POST,[]{if(!authorized())return;JsonDocument d;if(!parseBody(d,10000))return;JsonArray a=state["notes"].as<JsonArray>();String op=d["action"]|"save";int i=d["index"]|-1;if(op=="add"){if(a.size()>=40){web.send(409,"text/plain","Notes full");return;}JsonObject n=a.add<JsonObject>();n["id"]=String((uint32_t)time(nullptr))+"-"+String(esp_random(),HEX);n["text"]=String(d["text"]|"").substring(0,4000);n["status"]="typed";saveNoteArchive(n);}else if(i>=0&&i<(int)a.size()){if(op=="transcribe"){if(!retryNote(a[i].as<JsonObject>())){web.send(409,"text/plain","Check Wi-Fi, provider configuration, recording availability, or an active transcription");return;}}else if(op=="delete"){if(voiceBusy){web.send(409,"text/plain","Wait for transcription to finish");return;}String id=a[i]["id"]|"";if(sdOK){SD_MMC.remove("/notes/"+id+".json");SD_MMC.remove("/notes/"+id+".wav");}a.remove(i);}else if(op=="task"){addTask(a[i]["text"].as<String>());}else{a[i]["text"]=String(d["text"]|"").substring(0,4000);saveNoteArchive(a[i]);}}else{web.send(400,"text/plain","Invalid note");return;}saveState();dirty=true;web.send(200,"text/plain","Saved");});
 web.on("/api/audio",HTTP_GET,[]{if(!authorized())return;String id=web.arg("id");if(!safeFilename(id)){web.send(400,"text/plain","Invalid ID");return;}File f=sdOK?SD_MMC.open("/notes/"+id+".wav"):LittleFS.open("/voice.wav");if(!f){web.send(404,"text/plain","Recording not available");return;}web.streamFile(f,"audio/wav");f.close();});
}
