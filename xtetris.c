// XTetris -- lihtne X11 Tetris

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include "X11/Xlib.h"
#include "X11/Xutil.h"
#include "X11/keysym.h"

// konstandid

#define XTILES 10 // mänguvälja laius
#define YTILES 30 // kõrgus
#define TILESIZE 20 // ühe elemendi suurus, pix * pix
#define LEFT -1 // klotsi keerutamise suund
#define RIGHT 1 // klotsi keerutamise suund
#define TETRISMAX 7 // kui mitu erinevat tetrise kujundit
#define GAME_LEVEL_LINES_LIMIT 10 // mitu rida uue levelini
#define TIME_LEVEL_1 1000000 // level 1 ajasamm, mikrosekundit
#define TIME_F 0.91 // ajasammu vähendamise koefitsient per level
#define TIME_DROP 5000 // ajasamm klotsi kukutamise ajal
#define HIMAX 10 //mitu hiscore'i elementi
#define HISCORE_FILENAME "xtetris.score"

// display

Display* xd;
Window xw;
GC xgc;
int xblack, xwhite;
Pixmap xp;
XEvent xe;
int xwinx = XTILES*TILESIZE;
int xwiny = YTILES*TILESIZE;
Atom xwm_close;

Window xmenu;
Window xbutton_start, xbutton_quit;
int 	xmenu_width = (XTILES-2)*TILESIZE-2,
	xmenu_height = (YTILES-2)*TILESIZE-2, 
	xbutton_width = (XTILES-4)*TILESIZE-2,
	xbutton_height = TILESIZE-1;

// mäng

FILE* hiscore_file; // faili pointer
int b[YTILES][XTILES]; // mänguväli
_Bool game_redraw = 1; // joonista mänguväli
_Bool game_running = 1; // mäng käib
_Bool game_quit = 0; // programm quit
int game_score = 0;
int new_hiscore = -1; //uus hiscore, mitmes?!
int game_level = 1;
int game_level_lines = 0; //mitu rida on praeguses levelis skooritud?
int game_levelscore[4] = {10,25,40,60}; //skoori koefitsient, korraga ridu?

int tx, ty; // klotsi ülemise vasaku nurga asukoht
int tnr = 0; // mitmes klots võimalikest

struct timeval time_0, time_1; // kellaaja küsimise muutujad
float time_level = TIME_LEVEL_1,  // leveli alguse ajanihe
	time_delta = TIME_LEVEL_1; // hetke ajanihe

struct hiscore {
	int level,score;
};

struct hiscore game_hiscore[HIMAX]; // highscore

// klotsitüübid

typedef struct klots {
	int s; // klotsi suurus 4x4 bufris
	int e[4][4]; // klotsi elemendid 4x4 bufris
} klots;


klots tetris[TETRISMAX] = {
		    2,
		    1, 1, 0, 0, // "kast"
		    1, 1, 0, 0,
		    0, 0, 0, 0,
		    0, 0, 0, 0,

		    3,
		    0, 1, 1, 0, // "S"
		    1, 1, 0, 0,
		    0, 0, 0, 0,
		    0, 0, 0, 0,

		    4,
		    0, 1, 0, 0, // "|"
		    0, 1, 0, 0,
		    0, 1, 0, 0,
		    0, 1, 0, 0,

		    3,
		    1, 1, 0, 0, // "Z"
		    0, 1, 1, 0,
		    0, 0, 0, 0,
		    0, 0, 0, 0,

		    3,
		    0, 1, 0, 0, // "T"
		    0, 1, 1, 0,
		    0, 1, 0, 0,
		    0, 0, 0, 0,

		    3,
		    0, 1, 0, 0, // "L"
		    0, 1, 0, 0,
		    0, 1, 1, 0,
		    0, 0, 0, 0,

		    3,
		    0, 1, 0, 0, // "J"
		    0, 1, 0, 0,
		    1, 1, 0, 0,
		    0, 0, 0, 0};

klots kursor; // aktiivne klots

// funktsioonid

void graph_init(); //graafika setup
void hiscore_read(); //loeb hiscore
void hiscore_calculate(); //arvutame hiscore
void hiscore_write(); //kirjutab hiscore
void game_init(); //uue mängu algseaded
void game_draw(); //mänguvälja joonistamine

void game_menu_map(); //menüü esiletoomine
void game_menu_draw(); //menüü joonistamine
int game_menu_run(); //menüü loop

void game_run(); //mängu üks tsükkel
void game_end_level(); //mängu lõpu "animatsioon"

int game_linetest(); //täisridade kontroll
void game_score_write(); //kirjutame skoori akna päisesse
void game_score_calculate(int); //arvutame skoori

void kursor_rotate(int); //kursori keerutamine
int klots_test(klots,int,int); //kas kursor sobib etteantud asukohta?
void kursor_drop(); //kursori jõudis alla
void kursor_new(); //uue klotsi loomine kursori alla


// --------------------------------------------------------------------


int main () {

	graph_init();
	hiscore_read();

	// TODO ilma levelite kiirenemiseta mäng

	while(!game_quit) {
		game_init();
		game_score_write();
		while (game_running) game_run();

		if (!game_quit) {
			game_end_level();
			game_menu_map();
			game_menu_draw();
			game_menu_run();
		}
	}

	return 0;
}


void graph_init() {

	xd = XOpenDisplay(0);
	xblack = BlackPixel(xd,DefaultScreen(xd));
	xwhite = WhitePixel(xd,DefaultScreen(xd));
	xw = XCreateSimpleWindow(xd,DefaultRootWindow(xd),0,0,
		xwinx,xwiny,0,xblack,xwhite);
	XMapWindow(xd,xw);
	xgc = XCreateGC(xd,xw,0,0);
	xp = XCreatePixmap(xd,xw,xwinx,xwiny,DefaultDepth(xd,0));
	XSelectInput(xd,xw,StructureNotifyMask | KeyPressMask | ExposureMask | ResizeRedirectMask);

	xmenu = XCreateSimpleWindow(xd,xw,TILESIZE+1,TILESIZE+1,xmenu_width,xmenu_height,0,xwhite,xblack);
	xbutton_start = XCreateSimpleWindow(xd,xmenu,TILESIZE+1,(YTILES-7)*TILESIZE+1,xbutton_width,xbutton_height,0,xblack,xwhite);
	xbutton_quit = XCreateSimpleWindow(xd,xmenu,TILESIZE+1,(YTILES-5)*TILESIZE+1,xbutton_width,xbutton_height,0,xblack,xwhite);

	XSelectInput(xd,xmenu,ExposureMask | KeyPressMask);
	XSelectInput(xd,xbutton_start,ButtonPressMask);
	XSelectInput(xd,xbutton_quit,ButtonPressMask);

	// häälestame evendi, kui window manager sulgeb akna
	xwm_close = XInternAtom(xd,"WM_DELETE_WINDOW",False);
	XSetWMProtocols(xd,xw,&xwm_close,1);

	// häälestame põhiakna mõõtmed WM jaoks
	XSizeHints* xsh; // Size Hints
	xsh = XAllocSizeHints();
	xsh->flags = PMinSize | PMaxSize;
	xsh->min_width = xsh->max_width = xwinx;
	xsh->min_height = xsh->max_height = xwiny;
	XSetNormalHints(xd,xw,xsh);

	// ootame põhiakna tekkimist
	while (xe.type != MapNotify) XNextEvent(xd,&xe);

}


void game_init() {
	for (int y=0;y<YTILES;y++)
		for (int x=0;x<XTILES;x++) b[y][x]=0;

	kursor_new();
	gettimeofday(&time_0,NULL);
	srand(time_0.tv_usec);

	time_level = time_delta = TIME_LEVEL_1;

	game_level_lines = 0;

	game_score = 0;
	game_level = 1;
	new_hiscore = -1;
	game_running = 1;
}


void game_run() {
	gettimeofday(&time_1,NULL);

	if (  ( (time_1.tv_sec-time_0.tv_sec)*1000000 + time_1.tv_usec - time_0.tv_usec) >= time_delta) {

		if (klots_test(kursor,tx,ty+1)==0) ++ty;
		else { // jõudis alla
			kursor_drop();
			game_score_calculate(game_linetest());
			game_score_write();
			time_delta = time_level;
			kursor_new();
			if (klots_test(kursor,tx,ty)) game_running = 0;
		}
		game_redraw = 1;
		gettimeofday(&time_0,NULL);
	}

	if (XPending(xd)) XNextEvent(xd,&xe); // mingi sündmus

	if (xe.type == KeyPress) {
		switch (XLookupKeysym(&xe.xkey,0)) {
			case XK_Left:
				if (klots_test(kursor,tx-1,ty)==0) --tx;
				break;
			case XK_Right:
				if (klots_test(kursor,tx+1,ty)==0) ++tx;
				break;
			case XK_Up:
				kursor_rotate(LEFT);
				break;
			case XK_Down:
				kursor_rotate(RIGHT);
				break;
//			cheat, tab'iga saab klotse vahetada
//			case XK_Tab:
//				if (--tnr<0) tnr=TETRISMAX-1;
//				kursor = tetris[tnr];
//				break;
			case XK_space:
				time_delta = TIME_DROP; // kiirendatud liikumine
				break;
	}
		game_redraw = 1;
		xe.type = 0;
	}
	else if (xe.type == Expose) game_redraw = 1;
	else if (xe.type == ClientMessage) if (xe.xclient.data.l[0]=xwm_close) {
		game_running = 0;
		game_quit = 1; // püüame kinni akna sulgemise jõuga
	}
	if (game_redraw) game_draw();
}


void game_draw() {
	int x,y;

	// tühjendame ekraanibufri
	XSetForeground(xd,xgc,xwhite);
	XFillRectangle(xd,xp,xgc,0,0,xwinx,xwiny);

	// joonistame paigalolevad klotsid
	XSetForeground(xd,xgc,xblack);
	for (y=0;y<YTILES;y++)
		for (x=0;x<XTILES;x++) {
			if (b[y][x]) XFillRectangle(xd,xp,xgc,x*TILESIZE+1,y*TILESIZE+1,TILESIZE-2,TILESIZE-2);
			else XFillRectangle(xd,xp,xgc,x*TILESIZE+(TILESIZE/2)-1,y*TILESIZE+(TILESIZE/2)-1,2,2);
		}

	// joonistame liikuva klotsi
	for (y=0;y<kursor.s;y++)
		for (x=0;x<kursor.s;x++)
			if (kursor.e[y][x]!=0) XFillRectangle(xd,xp,xgc,(tx+x)*TILESIZE+1,(ty+y)*TILESIZE+1,TILESIZE-2,TILESIZE-2);

	// buffer aknasse
	XCopyArea(xd,xp,xw,xgc,0,0,xwinx,xwiny,0,0);
	game_redraw = 0;
}


void kursor_rotate(int suund) {
	int x,y;
	klots temp;

	temp.s = kursor.s;

	if (suund==LEFT) {
		for (x=0;x<kursor.s;x++)
			for (y=kursor.s-1;y>=0;y--)
				temp.e[kursor.s-1-y][x] = kursor.e[x][y];
	}
	else if (suund==RIGHT) {
		for (x=kursor.s-1;x>=0;x--)
			for (y=0;y<kursor.s;y++)
				temp.e[y][kursor.s-1-x] = kursor.e[x][y];
	}
	// TODO: klotsi nihutamine, kui rotatides ei mahu?

	if (klots_test(temp,tx,ty)==0) kursor = temp;

}


int klots_test(klots temp, int x, int y) {

	for (int _y=0;_y<temp.s;_y++)
		for (int _x=0;_x<temp.s;_x++) {
			//kontroll paremal ja vasakul
			if (temp.e[_y][_x]==1 && ((x+_x)<0 || (x+_x)>=XTILES)) return 1;

			//kontroll alt
			if (temp.e[_y][_x]==1 && y+_y>=YTILES) return 1;

			// kontroll teiste klotsidega
			if (temp.e[_y][_x]==1 && b[_y+y][_x+x]==1) return 1;
		}

	return 0;
}


void kursor_drop() { //kirjutame klotsi mänguväljale
	for (int y=0;y<kursor.s;y++)
		for (int x=0;x<kursor.s;x++)
			if (kursor.e[y][x]==1) b[ty+y][tx+x] = 1;
}


void kursor_new() {
	tx = XTILES/2-1;
	ty = 0;

	tnr = rand()%TETRISMAX;

	kursor = tetris[tnr];
}


int game_linetest() { // kas on täisridu? return ridade arv
	int x,y;
	int _x,_y;
	int lines = 0;

	for (y=0;y<YTILES;y++) {
		for (x=0;x<XTILES;x++) if (b[y][x]==0) break;
		if (x==XTILES) { // jah, on täisridu, ülemised alla
			lines++;
			for (_y=y-1;_y>=0;_y--)
				for (_x=0;_x<XTILES;_x++) b[_y+1][_x] = b[_y][_x];
		}
	}

	return lines;}


void game_score_write() { // kirjutame skoori akna nimesse
	char c[15];
	sprintf(c,"L%d S%d",game_level,game_score);
	XStoreName(xd,xw,c);
}


void game_score_calculate(int lines){
	if (lines>0) { // arvutab skoori leveli ja ridade järgi
		// mida kõrgem level, seda suurem kordaja = (1/TIME_F)^(level-1)
		// see on õiglane kasv, sest TIME_F=0.91 korral on
		// levelil 18 koefitsient u 5, mitte 18...
		float abi = pow((float)(1/TIME_F),(game_level-1));
		game_score += round((abi*game_levelscore[lines-1]));
		game_level_lines+=lines;

		// läheb järgmisele levelile, kui piisavalt ridu kaotatud
		if (game_level_lines>=GAME_LEVEL_LINES_LIMIT) {
			game_level++;
			game_level_lines = 0;
			time_level *= TIME_F; // järgmine level on kiirem
			time_delta = time_level;
		}
	}
}


void game_end_level() {
	int vilk = 8; // mitu korda vilgub?

	hiscore_calculate();
	if (new_hiscore>=0) hiscore_write();

	gettimeofday(&time_0,NULL);

	while (vilk > 0) {
		gettimeofday(&time_1,NULL);
		if (  ( (time_1.tv_sec-time_0.tv_sec)*1000000 + time_1.tv_usec - time_0.tv_usec) >= 200000) {
			xblack = xblack + xwhite - (xwhite = xblack); // vahetame ühe rea peal xblack ja xwhite väärtused :D
			game_draw();
			XFlush(xd); // põhiluubis flushib mingi event
			time_0 = time_1;
			vilk--;
		}
	}
}


void game_menu_map() {
	XMapWindow(xd,xmenu);
	XMapSubwindows(xd,xmenu);

	while (xe.type!=Expose) XNextEvent(xd,&xe);
}


void game_menu_draw() {
	int i,suund,ylevalt,alt;
	XCharStruct xchar;
	XFontStruct* xf = XQueryFont(xd,XGContextFromGC(xgc));
	char txt[25];

	XSetForeground(xd,xgc,xwhite);

	XDrawString(xd,xmenu,xgc,TILESIZE,TILESIZE*2,"XTetris",7);
	XDrawString(xd,xmenu,xgc,TILESIZE,TILESIZE*3,"2020",4);
	XDrawString(xd,xmenu,xgc,TILESIZE,TILESIZE*4,"noonius@gmail.com",17);
	XDrawLine(xd,xmenu,xgc,TILESIZE,TILESIZE*4.5,xmenu_width-TILESIZE,TILESIZE*4.5);
	XDrawString(xd,xmenu,xgc,TILESIZE,TILESIZE*6,"Hall of Fame",12);
	XDrawString(xd,xmenu,xgc,TILESIZE,TILESIZE*7,"LVL    SCORE",12);


	for (i=0;i<HIMAX;i++) {
		sprintf(txt,"%3d %8d",game_hiscore[i].level,game_hiscore[i].score);
		if (i==new_hiscore) sprintf(txt,"%s <<<<<",txt);
		XDrawString(xd,xmenu,xgc,TILESIZE,TILESIZE*(i+8),txt,strlen(txt));
	}

	//nuppude tekstid paneme ilusasti nuppude keskele
	XSetForeground(xd,xgc,xblack);
	XTextExtents(xf,"Start",5,&suund,&ylevalt,&alt,&xchar);
	XDrawString(xd,xbutton_start,xgc,
		(xbutton_width - xchar.width)/2,(xbutton_height-alt+ylevalt)/2,"Start",5);
	XTextExtents(xf,"Quit",4,&suund,&ylevalt,&alt,&xchar);
	XDrawString(xd,xbutton_quit,xgc,
		(xbutton_width - xchar.width)/2,(xbutton_height-alt+ylevalt)/2,"Quit",4);
	XFlush(xd);
}


int game_menu_run() {
	_Bool start = 0;

	while(!start && !game_quit) {
		XNextEvent(xd,&xe);
		//nupp, s ja S alustavad uue mängu
		if ((xe.type == ButtonPress && xe.xbutton.window == xbutton_start) ||
			 (xe.type == KeyPress &&
				( XLookupKeysym(&xe.xkey,0) == XK_s || XLookupKeysym(&xe.xkey,0) == XK_S))) start=1;
		//nupp, q ja Q lõpetavad proge
		else if ((xe.type == ButtonPress && xe.xbutton.window == xbutton_quit) ||
			 (xe.type == KeyPress &&
				( XLookupKeysym(&xe.xkey,0) == XK_q || XLookupKeysym(&xe.xkey,0) == XK_Q))) game_quit = 1;
		else if (xe.type == ClientMessage) if (xe.xclient.data.l[0]=xwm_close) exit(0); // püüame kinni akna sulgemise jõuga

		//keegi peitis vahepeal akent
		if (xe.type == Expose) {
			game_draw();
			game_menu_draw();
		}
	}
	XUnmapWindow(xd,xmenu); //menüüd peitu
}


void hiscore_read() { // loeme failist
	char line[10];
	int maxline = 10, i = 0;

	if ((hiscore_file = fopen(HISCORE_FILENAME,"r")))	{
		for (int i=0;i<HIMAX;i++) {
			fscanf(hiscore_file,"%d",&game_hiscore[i].score);
			fscanf(hiscore_file,"%d",&game_hiscore[i].level);
		}
	}
	else { // ei ole hiscore faili
		// loome hiscore näidissisu
		for (int i=0;i<HIMAX;i++) {
			game_hiscore[i].level = HIMAX-i;
			game_hiscore[i].score = (HIMAX-i)*game_levelscore[0]*GAME_LEVEL_LINES_LIMIT;
		}
		hiscore_write(); // kirjutame faili ka
	}

	fclose(hiscore_file);

}


void hiscore_calculate(){ // kas läks hiscore-i?
	for (int i=HIMAX-1;i>=0;i--) if (game_score >= game_hiscore[i].score) {
		if (i!=HIMAX-1) {
			game_hiscore[i+1].score = game_hiscore[i].score;
			game_hiscore[i+1].level = game_hiscore[i].level;
		}
		game_hiscore[i].score=game_score;
		game_hiscore[i].level=game_level;
		new_hiscore = i;
	}
}


void hiscore_write() { // kirjutame faili
	char line[10];

	hiscore_file = fopen(HISCORE_FILENAME,"w");
	for (int i=0;i<HIMAX;i++) {
		sprintf(line,"%d\n",game_hiscore[i].score);
		fputs(line,hiscore_file);
		sprintf(line,"%d\n",game_hiscore[i].level);
		fputs(line,hiscore_file);
	}
	fclose(hiscore_file);
}
