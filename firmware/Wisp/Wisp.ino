#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <ESP_I2S.h>
#include <sys/time.h>
#include <DNSServer.h>
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include "certs.h"
#include "fflate.h"
#include "EPD_3in97.h"
#include "GUI_Paint.h"
#include "es8311.h"
#include "core.h"
#include "web.h"

Preferences prefs;
WebServer web(80);
JsonDocument state, feed;
FocusTimer focus;
I2SClass audio;
uint8_t *frame=nullptr;
String pinCode, apName, ssid, wifiPass, bridgeURL, bridgeToken, timezone;
bool dirty=true, sdOK=false, audioOK=false, apActive=false, recordPending=false;
int page=8, selected=0, readerPage=0, homeSelected=0, sessions=0;
const char* pages[]={"Today","Focus","Notes","Read","Briefing","Home","Pet","Settings","Apps","Tasks"};
float temperature=NAN, humidity=NAN;
int appSelection=0,noteSelection=0,noteOffset=0,bookSelection=0; bool noteOpen=false,bookOpen=false;
String books[64],activeBook="",lastVoice="",geminiKey="",geminiModel="gemini-3.8-flash",voiceProvider="off"; int bookCount=0;
DNSServer dns; uint32_t connectedAt=0; bool wasConnected=false;
String notice="", voiceStatus="Hold centre to record a 15-second note";
uint32_t lastDraw=0,lastSensor=0,lastFeed=0,lastMinute=0,lastWiFiTry=0;
float history[48]; int historyCount=0;
uint8_t imuAddr=0; float previousMotion=0; uint32_t petAwake=0;

String clean(String s, size_t maxlen=1000) { String out; out.reserve(min(s.length(),maxlen)); for(size_t i=0;i<s.length()&&out.length()<maxlen;i++){unsigned char c=s[i];if(c>=32&&c<127)out+=(char)c;else if(c=='\n')out+='\n';}return out; }
void saveState(){ File f=LittleFS.open("/state.tmp","w"); if(!f)return; serializeJson(state,f);f.close();LittleFS.rename("/state.tmp","/state.json"); }
void text(int x,int y,String s,sFONT* font=&Font20,int width=740){s=clean(s,width/font->Width);s.replace("\n"," ");Paint_DrawString_EN(x,y,s.c_str(),font,WHITE,BLACK);}
void line(int x,int y,int xx,int yy){Paint_DrawLine(x,y,xx,yy,BLACK,DOT_PIXEL_1X1,LINE_STYLE_SOLID);}
void box(int x,int y,int w,int h,bool fill=false){Paint_DrawRectangle(x,y,x+w,y+h,BLACK,DOT_PIXEL_2X2,fill?DRAW_FILL_FULL:DRAW_FILL_EMPTY);}
void wrap(int x,int y,String s,int cols,int rows,sFONT* f=&Font20){s=clean(s,30000);for(int r=0;r<rows&&s.length();r++){int n=min((int)s.length(),cols);int nl=s.indexOf('\n');if(nl>=0&&nl<n)n=nl;else if(n<(int)s.length()){int sp=s.lastIndexOf(' ',n);if(sp>cols/2)n=sp;}text(x,y+r*(f->Height+8),s.substring(0,n),f,cols*f->Width);s=s.substring(n);if(s.startsWith(" ")||s.startsWith("\n"))s.remove(0,1);}}
String clockText(bool date=false){time_t t=time(nullptr); if(t<1700000000)return date?"Set Wi-Fi or sync phone time":"--:--";struct tm tm;localtime_r(&t,&tm);char buf[50];strftime(buf,sizeof(buf),date?"%a, %b %d, %Y":"%H:%M",&tm);return buf;}
String petMood(){if(focus.running)return focus.rest?"Taking a breather":"Quietly concentrating";if(millis()-petAwake<15000)return "Hello! I felt that.";if(isfinite(humidity)&&humidity<30)return "A little dry in here";return sessions?"Proud of your progress":"One small task at a time";}
// Pocket Ghost: two crisp 24x24, one-bit poses.
static const char* GHOST_IDLE[]={
"........................",
"........................",
"........########........",
"........#.......##......",
".....###.........#......",
"....#.............##....",
"....#..............#....",
"....#..............#....",
"....#..............#....",
"....#..............#....",
"....#...##.....##...###.",
"....#...##.....##.....#.",
".##.#...##.....##.....#.",
"#..#..................#.",
"#..........#.#.......#..",
"#...........#.......#...",
".##.................#...",
"...#................#...",
"...#................#...",
"...#................#...",
"...#....##....#.....#...",
"...#...#..#..#.#...#....",
"....###....##...####....",
"........................"
};
static const char* GHOST_REST[]={
"........................",
"........................",
"........................",
"........................",
"........................",
"........................",
"........................",
"........................",
"........................",
"........................",
".........######.........",
".........#.....###......",
"......###........#......",
".....#............##....",
".....#.............#....",
"....#..............#....",
"....#...............#...",
"...#...#.#.....#.#...#..",
"..#.....#.......#....#..",
".#.........##.........#.",
".#....................#.",
".#....................#.",
"..####################..",
"........................"
};
void pet(int x,int y,int scale=1){
 const char** pose=focus.running?GHOST_REST:GHOST_IDLE;int unit=2*scale;
 for(int row=0;row<24;row++)for(int col=0;col<24;col++)if(pose[row][col]=='#')Paint_DrawRectangle(x+col*unit,y+row*unit,x+(col+1)*unit-1,y+(row+1)*unit,BLACK,DOT_PIXEL_1X1,DRAW_FILL_FULL);
}
#include "services.h"
#include "launcher.h"
void render(){
 if(!frame)return; Paint_Clear(WHITE); if(page==8){drawLauncher();EPD_3IN97_Init();EPD_3IN97_Display(frame);EPD_3IN97_Sleep();lastDraw=millis();dirty=false;return;} text(24,18,"WISP / "+String(pages[page]),&Font24);text(670,24,clockText(),&Font20,120);line(24,58,776,58);
 JsonArray tasks=state["tasks"].as<JsonArray>();
 if(page==0){text(24,80,clockText(true),&Font24);text(24,120,feed["weather"].as<String>().length()?feed["weather"].as<String>():"Weather: connect the optional bridge",&Font16);
 text(24,160,"NEXT UP",&Font16);wrap(24,190,feed["calendar"] | "Add a calendar in the bridge, or a task below.",48,2);
 text(24,260,"YOUR THREE PRIORITIES",&Font16);int shown=0;for(JsonObject t:tasks){if(!(t["done"]|false)&&shown<3){text(24,295+shown*34,"[ ] "+t["text"].as<String>(),&Font20,620);shown++;}}if(!shown)text(24,300,"A clear page. Add a task from your phone.",&Font16);
 pet(690,325);text(24,407,isfinite(temperature)?String(temperature,1)+" C  /  "+String(humidity,0)+"% humidity":"Room sensor unavailable",&Font16);
 }else if(page==1){text(24,85,focus.rest?"REST & RECHARGE":"ONE THING AT A TIME",&Font24);String task=tasks.size()?tasks[min(selected,(int)tasks.size()-1)]["text"].as<String>():"Add your first task on the Notes page";wrap(24,130,task,48,2);text(24,225,String((focus.remaining+59)/60)+" minutes",&Font24);text(24,270,focus.running?"Focusing... centre to pause":"Centre to start / resume",&Font20);text(24,315,"Completed sessions: "+String(sessions),&Font20);text(24,363,"Hold centre: reset to 25 minutes",&Font16);pet(630,230,2);
 }else if(page==9){text(24,80,"YOUR TASKS",&Font24);if(!tasks.size())wrap(24,135,"Add tasks in the browser, or turn a note into a task.",50,3);int start=(selected/6)*6;for(int i=start;i<min(start+6,(int)tasks.size());i++)text(24,125+(i-start)*38,String(i==selected?"> ":"  ")+(tasks[i]["done"].as<bool>()?"[x] ":"[ ] ")+tasks[i]["text"].as<String>(),&Font20);text(24,365,"Select a task in the browser to focus on it.",&Font16);text(24,405,"Up/down select | centre complete | BOOT app grid",&Font16);
 }else if(page==2){drawNotes();
 }else if(page==3){drawLibrary();
 }else if(page==4){text(24,80,"YOUR DAILY BRIEFING",&Font24);wrap(24,125,feed["briefing"] | "Connect the bridge for RSS headlines and weather. No subscriptions are required for the offline pages.",60,10,&Font16);
 }else if(page==5){text(24,80,"HOME & ROOM",&Font24);text(24,125,isfinite(temperature)?String(temperature,1)+" C   "+String(humidity,0)+"% RH":"Room sensor unavailable",&Font24);JsonArray entities=feed["home"].as<JsonArray>();for(int i=0;i<min(5,(int)entities.size());i++)text(24,175+i*35,String(i==homeSelected?"> ":"  ")+entities[i]["name"].as<String>()+": "+entities[i]["state"].as<String>(),&Font16);
 if(!entities.size())text(24,180,"Optional: connect Home Assistant in the bridge.",&Font16);if(historyCount>1){float lo=100,hi=-100;for(int i=0;i<historyCount;i++){lo=min(lo,history[i]);hi=max(hi,history[i]);}for(int i=1;i<historyCount;i++)line(24+(i-1)*15,390-(history[i-1]-lo)*30/max(1.f,hi-lo),24+i*15,390-(history[i]-lo)*30/max(1.f,hi-lo));}text(24,415,"Room trend: last 48 minutes | centre toggles selected light",&Font12);
 }else if(page==6){pet(355,165,2);text(24,365,petMood(),&Font24);text(24,410,"Focus sessions: "+String(sessions)+" | Centre says hello",&Font16);
 }else {text(24,80,"MAKE YOURSELF AT HOME",&Font24);text(24,125,"Wi-Fi: "+String(WiFi.status()==WL_CONNECTED?WiFi.SSID():"not connected"),&Font20);text(24,165,"Open: http://"+(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():WiFi.softAPIP().toString()),&Font20);text(24,205,"Login: admin    Password: "+pinCode,&Font20);text(24,245,"Setup Wi-Fi: "+apName,&Font20);text(24,285,"Setup Wi-Fi password: "+pinCode,&Font20);text(24,330,"Centre: enable setup Wi-Fi",&Font16);text(24,360,"microSD: "+String(sdOK?"ready":"not inserted")+" | audio: "+String(audioOK?"ready":"unavailable"),&Font16);text(24,395,"Firmware 0.2.2 | USB desk mode",&Font16);}
 line(24,445,776,445);text(24,457,notice.length()?notice:String(page+1)+"/8   BOOT: App grid   Hold centre: record note",&Font12);
 EPD_3IN97_Init();EPD_3IN97_Display(frame);EPD_3IN97_Sleep();lastDraw=millis();dirty=false;
}
void i2cWrite(uint8_t addr,uint8_t reg,uint8_t val){Wire.beginTransmission(addr);Wire.write(reg);Wire.write(val);Wire.endTransmission();}
bool i2cRead(uint8_t addr,uint8_t reg,uint8_t* b,int n){Wire.beginTransmission(addr);Wire.write(reg);if(Wire.endTransmission(false))return false;if(Wire.requestFrom(addr,(uint8_t)n)!=n)return false;for(int i=0;i<n;i++)b[i]=Wire.read();return true;}
void shtCommand(uint16_t cmd){Wire.beginTransmission(0x70);Wire.write(cmd>>8);Wire.write(cmd&255);Wire.endTransmission();}
void readSensor(){shtCommand(0x3517);delay(1);shtCommand(0x7866);delay(15);uint8_t b[6];if(Wire.requestFrom((uint8_t)0x70,(uint8_t)6)==6){for(int i=0;i<6;i++)b[i]=Wire.read();if(sensorCRC(b,2)==b[2]&&sensorCRC(b+3,2)==b[5]){temperature=-45+175*((b[0]<<8)|b[1])/65536.f;humidity=100*((b[3]<<8)|b[4])/65536.f;if(historyCount==48){memmove(history,history+1,47*sizeof(float));historyCount--;}history[historyCount++]=temperature;if(sdOK){File f=SD_MMC.open("/room.csv",FILE_APPEND);if(f){f.printf("%ld,%.2f,%.2f\n",(long)time(nullptr),temperature,humidity);f.close();}}}else temperature=humidity=NAN;}else temperature=humidity=NAN;shtCommand(0xB098);}
void rtcLoad(){uint8_t b[7];if(!i2cRead(0x51,4,b,7)||(b[0]&128))return;auto dec=[](int v){return (v>>4)*10+(v&15);};struct tm t={};t.tm_sec=dec(b[0]&127);t.tm_min=dec(b[1]&127);t.tm_hour=dec(b[2]&63);t.tm_mday=dec(b[3]&63);t.tm_mon=dec(b[5]&31)-1;t.tm_year=dec(b[6])+100;if(t.tm_year<124||t.tm_mon<0||t.tm_mon>11)return;setenv("TZ","UTC0",1);tzset();time_t epoch=mktime(&t);setenv("TZ",timezone.c_str(),1);tzset();struct timeval tv={epoch,0};settimeofday(&tv,nullptr);}
void rtcSave(){time_t now=time(nullptr);if(now<1700000000)return;struct tm t;gmtime_r(&now,&t);auto bcd=[](int v){return (v/10)*16+v%10;};Wire.beginTransmission(0x51);Wire.write(4);for(int v:{t.tm_sec,t.tm_min,t.tm_hour,t.tm_mday,t.tm_wday,t.tm_mon+1,t.tm_year-100})Wire.write(bcd(v));Wire.endTransmission();}
void setupAP(){WiFi.mode(WIFI_AP_STA);WiFi.softAP(apName.c_str(),pinCode.c_str());dns.start(53,"*",WiFi.softAPIP());connectedAt=millis();apActive=true;dirty=true;}
bool authorized(){if(web.authenticate("admin",pinCode.c_str()))return true;web.requestAuthentication();return false;}
void jsonReply(JsonDocument& d){String s;serializeJson(d,s);web.send(200,"application/json",s);}
bool parseBody(JsonDocument& d,size_t limit=32000){if(web.arg("plain").length()>limit){web.send(413,"text/plain","Too large");return false;}if(deserializeJson(d,web.arg("plain"))){web.send(400,"text/plain","Invalid JSON");return false;}return true;}
void addTask(String s){s=clean(s,100);s.trim();if(!s.length())return;JsonArray tasks=state["tasks"].as<JsonArray>();if(tasks.size()>=30){notice="Task list full (30)";return;}JsonObject t=tasks.add<JsonObject>();t["text"]=s;t["done"]=false;saveState();dirty=true;}
void fetchFeed(){lastFeed=millis();if(WiFi.status()!=WL_CONNECTED||!bridgeURL.length())return;HTTPClient http;http.setTimeout(5000);http.begin(bridgeURL+"/feed");http.addHeader("Authorization","Bearer "+bridgeToken);int code=http.GET();if(code==200&&http.getSize()<=20000){String body=http.getString();JsonDocument incoming;if(body.length()<=20000&&!deserializeJson(incoming,body)){feed= incoming;if(feed["refreshing"]|false)lastFeed=millis()-840000;File f=LittleFS.open("/feed.json","w");serializeJson(feed,f);f.close();notice="";dirty=true;}}else notice="Bridge unavailable; showing saved data";http.end();}
void homeToggle(){JsonArray a=feed["home"].as<JsonArray>();if(homeSelected>=(int)a.size()||!bridgeURL.length()||WiFi.status()!=WL_CONNECTED)return;if(!(a[homeSelected]["controllable"]|false)){notice="This item is read-only";dirty=true;return;}HTTPClient h;h.setTimeout(5000);h.begin(bridgeURL+"/toggle");h.addHeader("Authorization","Bearer "+bridgeToken);h.addHeader("Content-Type","application/json");JsonDocument d;d["entity_id"]=a[homeSelected]["id"];String s;serializeJson(d,s);notice=h.POST(s)==200?"Home action sent":"Home action failed";h.end();fetchFeed();dirty=true;}
void recordNote(){recordPending=false;captureNote();}
void setupWeb(){
 setupServicesWeb();
 web.on("/",HTTP_GET,[]{if(authorized())web.send_P(200,"text/html",WEB_PAGE);});
 web.on("/api/state",HTTP_GET,[]{if(!authorized())return;JsonDocument d;d["tasks"]=state["tasks"];d["notes"]=state["notes"];d["sd"]=sdOK;d["ip"]=WiFi.localIP().toString();d["connected"]=WiFi.status()==WL_CONNECTED;d["provider"]=voiceProvider;d["model"]=geminiModel;d["key_set"]=geminiKey.length()>0;d["reading"]=state["reading"];d["page"]=page;d["sessions"]=sessions;d["ssid"]=ssid;d["bridge"]=bridgeURL;d["timezone"]=timezone;d["temperature"]=isfinite(temperature)?temperature:0;d["sensor_ok"]=isfinite(temperature);d["voice"]=voiceStatus;d["feed"]=feed;d["focus_minutes"]=(focus.remaining+59)/60;d["focus_running"]=focus.running;jsonReply(d);});
 web.on("/api/config",HTTP_POST,[]{if(!authorized())return;JsonDocument d;if(!parseBody(d,2000))return;String u=d["bridge"]|"";if(u.length()&&!u.startsWith("http://")){web.send(400,"text/plain","Bridge must be http://host:8787 on your trusted LAN");return;}voiceProvider=d["provider"]|"off";if(voiceProvider!="off"&&voiceProvider!="gemini"&&voiceProvider!="bridge")voiceProvider="off";if(d["gemini_key"].as<String>().length())geminiKey=d["gemini_key"].as<String>();geminiModel=d["model"]|"gemini-3.8-flash";prefs.putString("provider",voiceProvider);prefs.putString("gemini",geminiKey);prefs.putString("model",geminiModel);ssid=String(d["ssid"]|"").substring(0,32);if(d["password"].as<String>().length())wifiPass=d["password"].as<String>().substring(0,63);bridgeURL=u;while(bridgeURL.endsWith("/"))bridgeURL.remove(bridgeURL.length()-1);if(d["token"].as<String>().length())bridgeToken=d["token"].as<String>();timezone=String(d["timezone"]|"MST7MDT,M3.2.0,M11.1.0");prefs.putString("ssid",ssid);prefs.putString("wifi",wifiPass);prefs.putString("bridge",bridgeURL);prefs.putString("token",bridgeToken);prefs.putString("tz",timezone);configTzTime(timezone.c_str(),"pool.ntp.org","time.nist.gov");web.send(200,"text/plain","Saved. Join your home Wi-Fi and open the address on the board.");if(ssid.length())WiFi.begin(ssid.c_str(),wifiPass.c_str());lastFeed=millis()-900000;dirty=true;});
 web.on("/api/task",HTTP_POST,[]{if(!authorized())return;JsonDocument d;if(!parseBody(d,500))return;String action=d["action"]|"add";int i=d["index"]|-1;JsonArray a=state["tasks"].as<JsonArray>();if(action=="add"){if(a.size()>=30){web.send(409,"text/plain","Task limit is 30");return;}addTask(d["text"]|"");}else if(i>=0&&i<(int)a.size()){if(action=="toggle")a[i]["done"]=!a[i]["done"].as<bool>();else if(action=="delete")a.remove(i);else if(action=="select")selected=i;saveState();}else{web.send(400,"text/plain","Invalid task");return;}dirty=true;web.send(200,"text/plain","Saved");});
 web.on("/api/reading",HTTP_POST,[]{if(!authorized())return;JsonDocument d;if(!parseBody(d))return;String s=clean(d["text"]|"",24000);s.replace("\n"," ");state["reading"]=s;readerPage=0;saveState();dirty=true;web.send(200,"text/plain","Reading saved");});
 web.on("/api/action",HTTP_POST,[]{if(!authorized())return;JsonDocument d;if(!parseBody(d,500))return;String a=d["action"]|"";if(a=="page")page=constrain(d["page"]|8,0,9);else if(a=="focus")focus.toggle(millis());else if(a=="reset")focus.reset(millis());else if(a=="record")recordPending=true;else if(a=="refresh")lastFeed=millis()-900000;else if(a=="time"){time_t epoch=d["epoch"]|0L;if(epoch>1700000000){struct timeval tv={epoch,0};settimeofday(&tv,nullptr);rtcSave();}}dirty=true;web.send(200,"text/plain","OK");});
 web.on("/voice.wav",HTTP_GET,[]{if(!authorized())return;File f=LittleFS.open("/voice.wav","r");if(!f){web.send(404,"text/plain","No recording yet");return;}web.streamFile(f,"audio/wav");f.close();});
 web.onNotFound([]{if(apActive){web.sendHeader("Location","http://192.168.4.1/");web.send(302,"text/plain","");}else web.send(404,"text/plain","Not found");});web.begin();
}
void action(int button,bool held){
 if(button==0){page=8;noteOpen=false;dirty=true;return;}
 if(page==8){if(button==5)page=APP_PAGES[appSelection];else appSelection=(appSelection+(button==4?-1:1)+9)%9;dirty=true;return;}
 if(button==5&&held&&page==2){recordPending=true;return;}
 if(page==2){JsonArray n=state["notes"].as<JsonArray>();if(button==5){noteOpen=!noteOpen;noteOffset=0;}else if(noteOpen)noteOffset=max(0,noteOffset+(button==4?-1:1));else noteSelection=constrain(noteSelection+(button==4?-1:1),0,max(0,(int)n.size()-1));}
 else if(page==3){if(button==5){if(held){bookOpen=false;}else if(!bookOpen&&bookCount){activeBook=books[bookSelection];readerPage=prefs.getInt(bookKey(activeBook).c_str(),0);bookOpen=true;}}else if(bookOpen){readerPage=max(0,readerPage+(button==4?-1:1));prefs.putInt(bookKey(activeBook).c_str(),readerPage);}else bookSelection=constrain(bookSelection+(button==4?-1:1),0,max(0,bookCount-1));}
 else if(page==9){JsonArray a=state["tasks"].as<JsonArray>();if(button==5&&a.size()){a[selected]["done"]=!a[selected]["done"].as<bool>();saveState();}else selected=constrain(selected+(button==4?-1:1),0,max(0,(int)a.size()-1));}
 else if(page==1&&button==5){if(held)focus.reset(millis());else focus.toggle(millis());}
 else if(page==5){if(button==5)homeToggle();else homeSelected=constrain(homeSelected+(button==4?-1:1),0,max(0,min(5,(int)feed["home"].size())-1));}
 else if(page==6&&button==5)petAwake=millis();
 else if(page==7&&button==5)setupAP();
 dirty=true;
}
void buttons(){static const int pins[]={0,4,5,6};static bool stable[4]={1,1,1,1},rawOld[4]={1,1,1,1};static uint32_t changed[4]={},down[4]={};for(int i=0;i<4;i++){bool raw=digitalRead(pins[i]);if(raw!=rawOld[i]){rawOld[i]=raw;changed[i]=millis();}if(raw!=stable[i]&&millis()-changed[i]>35){stable[i]=raw;if(!raw)down[i]=millis();else action(pins[i],millis()-down[i]>800);}}}
void setup(){
 Serial.begin(115200);for(int p:{0,4,5,6})pinMode(p,INPUT_PULLUP);Wire.begin(41,42);Wire.setTimeOut(100);pinMode(39,OUTPUT);digitalWrite(39,LOW);
 // Keep the original NVS namespace so existing installations retain settings.
 prefs.begin("deskling",false);voiceProvider=prefs.getString("provider","off");geminiKey=prefs.getString("gemini","");geminiModel=prefs.getString("model","gemini-3.8-flash");pinCode=prefs.getString("pin","");if(!pinCode.length()){char b[16];snprintf(b,sizeof(b),"%08lu",(unsigned long)(esp_random()%90000000+10000000));pinCode=b;prefs.putString("pin",pinCode);}apName="Wisp-"+String((uint32_t)ESP.getEfuseMac(),HEX).substring(0,4);ssid=prefs.getString("ssid","");wifiPass=prefs.getString("wifi","");bridgeURL=prefs.getString("bridge","");bridgeToken=prefs.getString("token","");timezone=prefs.getString("tz","MST7MDT,M3.2.0,M11.1.0");sessions=prefs.getInt("sessions",0);
 if(!LittleFS.begin(true))notice="Storage failed";File f=LittleFS.open("/state.json","r");if(f){deserializeJson(state,f);f.close();}if(!state["notes"].is<JsonArray>())state["notes"].to<JsonArray>();if(!state["tasks"].is<JsonArray>())state["tasks"].to<JsonArray>();f=LittleFS.open("/feed.json","r");if(f){deserializeJson(feed,f);f.close();}
 setenv("TZ",timezone.c_str(),1);tzset();rtcLoad();WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(true);if(ssid.length()){WiFi.begin(ssid.c_str(),wifiPass.c_str());}else{setupAP();page=7;}configTzTime(timezone.c_str(),"pool.ntp.org","time.nist.gov");
 SD_MMC.setPins(16,17,15,7,8,18);sdOK=SD_MMC.begin("/sdcard",true);if(sdOK){File r=SD_MMC.open("/reading.txt");if(r&&r.size()<=24000){String s=r.readString();s=clean(s,24000);s.replace("\n"," ");state["reading"]=s;}r.close();}
 if(sdOK){SD_MMC.mkdir("/books");SD_MMC.mkdir("/notes");}scanBooks();restoreNoteArchive();readSensor();uint8_t id;for(uint8_t addr:{0x6A,0x6B})if(i2cRead(addr,0,&id,1)&&id==5){imuAddr=addr;i2cWrite(addr,2,0x60);i2cWrite(addr,3,0x06);i2cWrite(addr,8,0x01);break;}
 audio.setPins(14,47,48,21,13);if(audio.begin(I2S_MODE_STD,16000,I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO,I2S_STD_SLOT_LEFT)){es8311_handle_t codec=es8311_create(I2C_NUM_0,ES8311_ADDRRES_0);es8311_clock_config_t c={false,false,true,4096000,16000};if(codec&&es8311_init(codec,&c,ES8311_RESOLUTION_16,ES8311_RESOLUTION_16)==ESP_OK){es8311_microphone_config(codec,false);es8311_microphone_gain_set(codec,ES8311_MIC_GAIN_24DB);es8311_voice_volume_set(codec,50,nullptr);audioOK=true;}}
 setupWeb();MDNS.begin("wisp");DEV_Module_Init();frame=(uint8_t*)ps_malloc(48000);if(!frame)frame=(uint8_t*)malloc(48000);if(!frame){Serial.println("No framebuffer memory");return;}Paint_NewImage(frame,800,480,0,WHITE);Paint_SetScale(2);render();lastSensor=millis();lastFeed=millis()-900000;petAwake=millis()-20000;
}
void loop(){if(apActive)dns.processNextRequest();web.handleClient();serviceNetwork();collectVoiceResult();buttons();if(recordPending)recordNote();if(focus.tick(millis())){if(focus.rest){sessions++;prefs.putInt("sessions",sessions);}notice=focus.rest?"Focus complete! Centre starts your break":"Break complete! Ready for another task";dirty=true;}
 if(millis()-lastSensor>=60000){lastSensor=millis();readSensor();rtcSave();dirty=true;}
 if(millis()-lastMinute>=60000){lastMinute=millis();dirty=true;}
 if(millis()-lastFeed>=900000&&WiFi.status()==WL_CONNECTED)fetchFeed();
 if(WiFi.status()!=WL_CONNECTED&&millis()-lastWiFiTry>30000){lastWiFiTry=millis();if(!apActive){setupAP();notice="Wi-Fi unavailable. Setup network enabled.";}if(ssid.length())WiFi.begin(ssid.c_str(),wifiPass.c_str());}
 static uint32_t motionAt=0;if(imuAddr&&millis()-motionAt>500){motionAt=millis();uint8_t b[6];if(i2cRead(imuAddr,0x35,b,6)){float magnitude=0;for(int i=0;i<3;i++){float v=(int16_t)((b[i*2+1]<<8)|b[i*2])/16384.f;magnitude+=v*v;}if(fabsf(magnitude-previousMotion)>0.8){petAwake=millis();if(page==6&&millis()-lastDraw>15000)dirty=true;}previousMotion=magnitude;}}
 if(frame&&dirty&&millis()-lastDraw>3000)render();delay(5);
}
