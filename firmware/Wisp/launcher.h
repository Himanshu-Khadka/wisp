#pragma once
const int APP_PAGES[9]={0,2,9,1,3,4,5,6,7};
const char* APP_NAMES[9]={"Today","Notes","Tasks","Focus","Library","Briefing","Home","Companion","Settings"};
const char* APP_HINTS[9]={"Your day at a glance","Speak. Save. Remember.","One thing at a time","Make space to focus","Your shelf, on SD","A quieter news feed","Room and devices","Meet Miso","Wi-Fi and preferences"};
void appIcon(int id,int x,int y){
 if(id==7){pet(x,y,1);return;}
 if(id==3){Paint_DrawCircle(x+27,y+25,23,BLACK,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);line(x+27,y+25,x+27,y+10);line(x+27,y+25,x+40,y+30);return;}
 if(id==6){line(x,y+22,x+28,y);line(x+28,y,x+56,y+22);box(x+7,y+22,42,32);box(x+25,y+36,10,18);return;}
 if(id==8){Paint_DrawCircle(x+27,y+27,23,BLACK,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);Paint_DrawCircle(x+27,y+27,9,BLACK,DOT_PIXEL_2X2,DRAW_FILL_EMPTY);for(int i=0;i<8;i++){float a=i*PI/4;line(x+27+cos(a)*24,y+27+sin(a)*24,x+27+cos(a)*31,y+27+sin(a)*31);}return;}
 box(x+5,y,47,52);if(id==4){line(x+28,y,x+28,y+52);for(int i=0;i<3;i++){line(x+11,y+12+i*10,x+22,y+12+i*10);line(x+34,y+12+i*10,x+46,y+12+i*10);}}else if(id==0){line(x+5,y+14,x+52,y+14);text(x+13,y+23,"24",&Font20,40);}else{for(int i=0;i<3;i++){if(id==2)box(x+12,y+10+i*13,5,5);line(x+(id==2?23:12),y+12+i*13,x+45,y+12+i*13);}}
}
void drawLauncher(){
 text(24,18,"WISP",&Font24);text(24,48,"A little space for your day",&Font12);text(665,25,clockText(),&Font20,120);
 for(int i=0;i<9;i++){int x=24+(i%3)*254,y=84+(i/3)*107;box(x,y,240,94);if(i==appSelection){Paint_DrawRectangle(x+2,y+2,x+238,y+6,BLACK,DOT_PIXEL_1X1,DRAW_FILL_FULL);box(x+4,y+4,232,86);}appIcon(i,x+15,y+20);text(x+89,y+29,APP_NAMES[i],&Font16,145);text(x+89,y+56,String(i+1)+" / 9",&Font12,140);}
 text(24,418,APP_HINTS[appSelection],&Font16);line(24,445,776,445);text(24,456,"UP / DOWN  Choose app     CENTRE  Open     BOOT  Home",&Font12);
}
void drawNotes(){
 JsonArray n=state["notes"].as<JsonArray>();text(24,82,"YOUR THOUGHTS, KEPT",&Font24);if(!n.size()){wrap(24,145,"Hold centre, speak, then press centre to finish. Your audio is saved first. Enable transcription in the browser to turn it into a note.",48,5);}
 else{noteSelection=constrain(noteSelection,0,(int)n.size()-1);if(noteOpen){String body=n[noteSelection]["text"]|"Audio saved. Transcription is not available yet.";int maxPage=max(0,((int)body.length()-1)/600);noteOffset=constrain(noteOffset,0,maxPage);String chunk=clean(body.substring(noteOffset*600,(noteOffset+1)*600),600);chunk.replace("\n"," ");for(int row=0;row<10;row++)text(24,128+row*25,chunk.substring(row*60,(row+1)*60),&Font16,750);text(24,395,"Page "+String(noteOffset+1)+" | centre returns to notes",&Font12);}else {int start=(noteSelection/6)*6;for(int i=start;i<min(start+6,(int)n.size());i++){String title=n[i]["text"]|"Voice recording";title.replace("\n"," ");text(24,130+(i-start)*40,String(i==noteSelection?"> ":"  ")+title,&Font16,730);}text(24,390,"Centre: read note | hold centre: new recording",&Font12);}}
 text(24,419,voiceStatus,&Font12);
}
void drawLibrary(){
 text(24,82,bookOpen?activeBook:"YOUR LIBRARY",&Font24);if(!sdOK){wrap(24,145,"Insert a FAT32 microSD card, then restart. Open wisp.local on your phone or computer to upload TXT or EPUB books.",48,5);return;}
 if(!bookOpen){if(!bookCount)wrap(24,145,"Your shelf is empty. Open the Library in your browser and upload a book. EPUB files are converted locally in your browser for this screen.",48,5);for(int i=(bookSelection/6)*6;i<min((bookSelection/6)*6+6,bookCount);i++)text(24,135+(i%6)*41,String(i==bookSelection?"> ":"  ")+books[i],&Font16);text(24,418,"Centre: open book | BOOT: app grid",&Font12);return;}
 File f=SD_MMC.open("/books/"+activeBook);if(!f){bookOpen=false;scanBooks();return;}int count=max(1,(int)((f.size()+719)/720));readerPage=constrain(readerPage,0,count-1);f.seek(readerPage*720);char b[721];size_t got=f.readBytes(b,720);b[got]=0;f.close();String chunk=clean(b,720);chunk.replace("\n"," ");for(int r=0;r<12;r++)text(24,120+r*23,chunk.substring(r*60,(r+1)*60),&Font16,750);text(24,414,"Page "+String(readerPage+1)+" / "+String(count)+"   Hold centre: shelf",&Font12);
}
