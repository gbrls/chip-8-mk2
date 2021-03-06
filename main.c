#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef unsigned short u16;
typedef unsigned char u8;

#define ADDR_MAX 0x1000

// TODO: implement a queue which holds the lastest states,
// highlight differences from the current state from the previous

//TODO:
//sound!

typedef struct {

	u16 pc;
	u16 stack[16];

	u8 ram[ADDR_MAX];
	u8 sp;
	u8 V[16]; // registers
	u16 I; // Address register

	u8 sound, delay;

}S; // S for state

S* s; // s is the holds the current vm state
u16 START = 0x200; // were the program is loaded
char code[8]; // a little helper string which holds the name of the latest processed instruction
u8 keyboard[16]={0};
int cpu_interval = 2;
int paused = 0;
int help = 0;

#define EMU_W 64
#define EMU_H 32
#define EMU_SCL 16

u8 screen[EMU_W/8][EMU_H] = {0};

int font_height = 5;
u8 font_sprite [] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // a
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // b
	0xF0, 0x80, 0x80, 0x80, 0xF0, // c
	0xE0, 0x90, 0x90, 0x90, 0xE0, // d
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // e
	0xF0, 0x80, 0xF0, 0x80, 0x80  // f
};

S* new_S() {
	S* s = calloc(1, sizeof(S));

	for(int i=0;i< 16*font_height;i++) {
		s->ram[i] = font_sprite[i];
	}

	s->sp = 0;

	return s;
}

void op_CLS() {
	memset(screen, 0, sizeof(screen));

	s->pc += 2;
}

void op_RET() {
	s->pc = s->stack[s->sp];
	s->sp -= 1;
}

void op_JMP(u16 addr) {
	s->pc = addr;
}

void op_CALL(u16 addr) {
	s->sp+=1;
	s->stack[s->sp] = s->pc+2;
	s->pc = addr;
}

void op_SE(u8 x, u8 byte) {

	x = s->V[x];

	if(x == byte) {
		s->pc += 2;
	}

	s->pc += 2;
}

void op_SNE(u8 x, u8 byte) {

	if(s->V[x] != byte) {
		s->pc += 2;
	}


	s->pc += 2;
}

void op_LD(u8* x, u8 byte) {
	*x = byte;
	s->pc += 2;
}

void op_OR(u8 x, u8 y) {
	s->V[x] |= y;
	s->pc += 2;
}

void op_AND(u8 x, u8 y) {
	s->V[x] &= y;
	s->pc += 2;
}

void op_XOR(u8 x, u8 y) {
	s->V[x] ^= y;
	s->pc += 2;
}

void op_ADD(u8 x, u8 y) {
	int a = s->V[x];
	int b = y;

	if(a+b>0xff) s->V[0xF]=1;

	s->V[x] = (a+b)&0xff;

	s->pc += 2;
}

void op_SUB(u8 x, u8 y) {
	s->V[0xF] = (s->V[x]>y);
	s->V[x] -= y;

	s->pc += 2;
}

void op_SHR(u8 x, u8 y) {
	s->V[0xF] = s->V[x]&1;
	s->V[x]/=2;

	s->pc += 2;
}

void op_SUBN(u8 x, u8 y) {
	s->V[0xF] = (s->V[x]<y);
	s->V[x] = y - s->V[x];

	s->pc += 2;
}

void op_SHL(u8 x, u8 y) {
	s->V[0xF] = s->V[x]&1;
	s->V[x]*=2;

	s->pc += 2;
}

void op_LDI(u16 x) {
	s->I = x;
	s->pc += 2;
}

void op_RND(u8 x, u8 kk) {
	s->V[x] = rand()&kk;
	s->pc += 2;
}

void op_DRW(u8 x, u8 y, u8 n) {

	x = s->V[x], y = s->V[y];

	int start = x/8;
	int off = x%8;

	int scnd = (5-(5+off)%8);

	int f = 0;


	for(int i=0;i<n;i++) {
		int byte = s->ram[s->I+i];

		//s->V[0xf] = ((screen[start][i+y]|(byte<<off))==(screen[start][i+y]^(byte<<off)));
		int or = screen[start%8][i+y] | (byte>>off);
		int xor = screen[start%8][i+y] ^ (byte>>off);

		if(or != xor) f=1;

		screen[start][(i+y)%EMU_H] ^= (byte>>off);

		if(off) {
			int or = screen[(start+1)%8][(i+y)%EMU_H] | (byte>>off);
			int xor = screen[(start+1)%8][(i+y)%EMU_H] ^ (byte>>off);

			if(or!=xor) f=1;

			screen[(start+1)%8][(i+y)%EMU_H] ^= (byte<<scnd);
		}
	}

	s->V[0xf]=f;

	s->pc += 2;
}

void op_SKP(u8 x) {
	if(keyboard[s->V[x]]) s->pc += 2;
	s->pc += 2;
}

void op_SKNP(u8 x) {
	if(!keyboard[s->V[x]]) s->pc += 2;
	s->pc += 2;
}

void op_LDK(u8 x) {
	int i=0;
	for(int i=0;i<16;i++) {
		if(keyboard[i]) {
			s->V[x] = i;
			s->pc += 2;
			return;
		}
	}
}

void op_LDS(u8 x) {
	s->I = (font_height * s->V[x]);
	s->pc += 2;
}

void op_LDB(u8 x) {
	int number = s->V[x];

	s->ram[s->I] = number/100;
	s->ram[s->I+1] = (number/10)%10;
	s->ram[s->I+2] = number%10;

	s->pc += 2;
}

void op_LDIX(u8 x) {
	for(int i=0;i<=x;i++) {
		s->ram[s->I+i] = s->V[i];
	}

	s->pc += 2;
}

void op_LDXI(u8 x) {

	for(int i=0;i<=x;i++) {
		s->V[i] = s->ram[s->I+i];
	}

	s->pc += 2;
}

// Syntatic sugar for defining instructions
// Why wouldn't we modify C a little bit, just
// for convinience?
#define OP(name, ...)							\
	do {										\
		strcpy(code, #name);					\
		if(f) op_ ## name (__VA_ARGS__);		\
	} while(0)


int instr(u16 start, int f) {

	u16 in = (s->ram[start]<<(2*4)) | s->ram[start+1];
	//if(f) printf("%04X, %02X %02X\n", in, s->ram[start], s->ram[start+1]);

	u16 nnn, n, x, y, kk, h, ht, hl;

	nnn = 0x0fff & in;
	n = 0x000f & in;
	x = (0x0f00 & in)>>(2*4);
	y = (0x00f0 & in)>>(4);
	kk = 0x00ff & in;
	h = (0xff00 & in)>>(2*4);
	ht = (0xf000 & in)>>(3*4);
	hl = (0x0f00 & in)>>(2*4);

	//if(f) printf("nnn: %04X, n: %01X, x: %01X, y %01X, kk %02X\n", nnn, n, x,y,kk);

	switch (ht) {
		case 0x0:
			if(n == 0x0) {
				if(in != 0x0000) {
					OP(CLS);
				}
			} else if(n == 0xE) {
				OP(RET);
			}
			break;
		case 0x1:
			OP(JMP, nnn);
			break;
		case 0x2:
			OP(CALL, nnn);
			break;
		case 0x3:
			OP(SE, x, kk);
			break;
		case 0x4:
			OP(SNE, x, kk);
			break;
		case 0x5:
			OP(SE, x, y);
			break;
		case 0x6:
			OP(LD, &s->V[x], kk);
			break;
		case 0x7:
			OP(ADD, x, kk);
			break;
		case 0x8:
			switch(n) {
				case 0x0:
					OP(LD, &s->V[x], s->V[y]);
					break;
				case 0x1:
					OP(OR, x, s->V[y]);
					break;
				case 0x2:
					OP(AND, x, s->V[y]);
					break;
				case 0x3:
					OP(XOR, x, s->V[y]);
					break;
				case 0x4:
					OP(ADD, x, s->V[y]);
					break;
				case 0x5:
					OP(SUB, x, s->V[y]);
					break;
				case 0x6:
					OP(SHR, x, s->V[y]);
					break;
				case 0x7:
					OP(SUBN, x, s->V[y]);
					break;
				case 0xE:
					OP(SHL, x, s->V[y]);
					break;
				default:
					break;
			}
			break;
		case 0x9:
			OP(SNE, x, s->V[y]);
			break;
		case 0xA:
			OP(LDI, nnn);
			break;
		case 0xB:
			OP(JMP, s->V[0]+nnn);
			break;
		case 0xC:
			OP(RND,x,kk);
			break;
		case 0xD:
			OP(DRW,x,y,n);
			break;
		case 0xE:
			if(n == 0xE) {
				OP(SKP,x);
			} else if(n == 0x1) {
				OP(SKNP, x);
			}
			break;
		case 0xF:
			switch(kk) {
				case 0x07:
					OP(LD, &s->V[x], s->delay);
					break;
				case 0x0A:
					OP(LDK, x);
					break;
				case 0x15:
					OP(LD, &s->delay, s->V[x]);
					break;
				case 0x18:
					OP(LD, &s->sound, s->V[x]);
					break;
				case 0x1E:
					OP(LDI, s->V[x]+s->I);
					break;
				case 0x29:
					OP(LDS, x);
					break;
				case 0x33:
					OP(LDB, x);
					break;
				case 0x55:
					OP(LDIX, x);
					break;
				case 0x65:
					OP(LDXI, x);
					break;
				default:
					break;
			}
			break;

		default:
			printf("NOT IMPLEMENTED (%04X)\n", in);
			break;
	}
	return in;
}

void load(u8* tape, u16 start, int n) {
	for(int i=start;i<start+n;i++) {
		s->ram[i] = tape[i-start];
	}

	s->pc = start;
}

void print(u16 start) {

	putchar('\n');

	for(int i=start;i<5+start;i++) {
		instr(i, 0);

		if(i==s->pc) printf("[%s] ", code);
		else printf("%s ", code);
	}

	putchar('\n');

	for(int i=0;i</*16*/2;i++) {
		printf("[%04X] ", s->V[i]);
	}

	putchar('\n');
}

void update() {
	u16 in = instr(s->pc, 1);
	if(in == 0x0000) paused = 1;
}

SDL_Window* window=NULL;
SDL_Renderer* renderer=NULL;
TTF_Font* font=NULL;
int should_quit=0;
unsigned int past=0;
unsigned int last_draw=0;
unsigned int last_update=0;

const char* fpath = "./share.ttf";

#define W 1200
#define H 800

void init_video() {
	if(SDL_Init(SDL_INIT_VIDEO)<0) {
		printf("SDL_Error: (%s)\n", SDL_GetError());
	}
	window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
							  W, H, SDL_WINDOW_SHOWN);
	renderer=SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	TTF_Init();
	font = TTF_OpenFont(fpath, 18);
	assert(font!=NULL);

}

void draw_text(int x, int y, char* text, SDL_Color color) {
	SDL_Surface *surface;

	surface = TTF_RenderText_Solid(font, text, color);
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

	SDL_Rect r;
	r.w=surface->w, r.h=surface->h;
	r.x=x,r.y=y;

	SDL_RenderCopy(renderer, texture, NULL, &r);

	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);
}

void draw_program(int x, int y, int start) {
	int off = 18;

	SDL_Rect r = {.x = x, .y = 0, .w=(W-x), .h=H};
	SDL_SetRenderDrawColor(renderer, 50, 50, 120, 0xff);
	SDL_RenderFillRect(renderer, &r);


	char buf[10]; sprintf(buf, "RAM");
	draw_text(x+5, 5, buf, (SDL_Color){0xff,0xff,0x8f,0xff});

	int poff=0;
	if(s->pc > 80+start) {
		poff = (s->pc - start)/2;
	}

	for(int i=0;i<40;i++) {
		int in = instr(start+2*(i+poff), 0);

		char buf[20];
		sprintf(buf, "%*s     %02X %02X", 4, code, in>>(2*4),in&0x00ff);

		if(s->pc!=start+2*(i+poff)) draw_text(x, y+i*off, buf,(SDL_Color){0xff,0xff,0xff,0xff});
		else draw_text(x, y+i*off, buf,(SDL_Color){200,20,20,255});

		if(in == 0x0000) break;
	}
}

void draw_state(int x, int y) {
	int off = 55;
	int line_off = 30;

	SDL_Rect r = {.x=0,.y=y,.w=1030,.h=H-y};
	SDL_SetRenderDrawColor(renderer, 50, 50, 120, 0xff);
	SDL_RenderFillRect(renderer, &r);

	draw_text(x,y+2,"CPU", (SDL_Color){0xff,0xff,0x8f,0xff});

	for(int i=0;i<16;i++) {
		char buf[10]; sprintf(buf, "%04X", s->V[i]);
		draw_text(150+i*off+x, y+2, buf, (SDL_Color){0xff,0xff,0xff,0xff});
	}

	for(int i=0;i<16;i++) {
		char buf[3]; sprintf(buf, "%X", i);
		if(keyboard[i]) draw_text(400+i*15+x, y+line_off, buf, (SDL_Color){0xff,0xff,0xff,0xff});
		else draw_text(400+i*15+x, y+line_off, buf, (SDL_Color){0x50,0x50,0x80,0xff});
	}

	char buf[20]; sprintf(buf, "PC  %04X", s->pc);
	draw_text(x,y+line_off,buf, (SDL_Color){0xff,0xff,0xff,0xff});

	sprintf(buf, "SP  %04X", s->sp);
	draw_text(x+150,y+line_off,buf, (SDL_Color){0xff,0xff,0xff,0xff});


	sprintf(buf, "I  %04X", s->I);
	draw_text(x+300,y+line_off,buf, (SDL_Color){0xff,0xff,0xff,0xff});

}


void draw_controls(int start, int end) {

	SDL_Rect r = {.x=0,.y=start,.w=1030,.h=end-start};
	SDL_SetRenderDrawColor(renderer, 50, 50, 120, 0xff);
	SDL_RenderFillRect(renderer, &r);


	draw_text(5,start+5,"CONTROLS", (SDL_Color){0xff,0xff,0x8f,0xff});
	draw_text(5,end-25,"Press H for help.", (SDL_Color){0xff,0xff,0xff,0xff});

	float clock = -1;
	if(cpu_interval > 0) clock = 1000.0f/(float)cpu_interval;

	char buf[20]; sprintf(buf, "Clock: %0.2fHz", clock);
	draw_text(200,start+5,buf, (SDL_Color){0xff,0xff,0xff,0xff});


	sprintf(buf, "Paused");
	if(paused) draw_text(500,start+5,buf, (SDL_Color){0xff,0xff,0xff,0xff});
	else draw_text(500,start+5,buf, (SDL_Color){0x50,0x50,0x80,0xff});

}

void draw_help(int x, int y) {

	int border = 5;

	SDL_Rect r2 = {.x=x-border,.y=y-border,.w=500+2*border,.h=500+2*border};
	SDL_SetRenderDrawColor(renderer, 50, 50, 50, 0xff);
	SDL_RenderFillRect(renderer, &r2);
	SDL_Rect r = {.x=x,.y=y,.w=500,.h=500};
	SDL_SetRenderDrawColor(renderer, 10, 10, 10, 0xff);
	SDL_RenderFillRect(renderer, &r);


	draw_text(x+5,y+5,"ESCAPE: Exit, SPACE: Pause, H: Toggle Help", (SDL_Color){0xff,0xff,0xff,0xff});
	draw_text(x+5,y+25,"UP/DOWN: Control Clock", (SDL_Color){0xff,0xff,0xff,0xff});
}


void draw_screen() {

	SDL_SetRenderDrawColor(renderer, 50, 50, 150, 0xff);

	for(int i=0;i<=EMU_W;i++) {
		SDL_RenderDrawLine(renderer, i*EMU_SCL, 0, i*EMU_SCL, EMU_H*EMU_SCL);
	}

	for(int i=0;i<=EMU_H;i++) {
		SDL_RenderDrawLine(renderer, 0, i*EMU_SCL,EMU_W*EMU_SCL,i*EMU_SCL);
	}

	for(int i=0; i<EMU_W/8; i++) {
		for(int j=0;j<EMU_H;j++) {

			for(int k=0;k<8;k++) {
				if(screen[i][j]&(1<<k)) {
					SDL_Rect r = {.x=(8*i+(7-k))*EMU_SCL, .y=j*EMU_SCL, .w=EMU_SCL, .h=EMU_SCL};
					SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
					SDL_RenderFillRect(renderer, &r);
				}
			}
		}
	}
}

void load_program(char* name, int start) {
	FILE* f;
	f = fopen(name, "rb");

	u8 buf[ADDR_MAX];
	fread(buf,ADDR_MAX-start, 1, f);

	puts("File read.");

	load(buf, start, ADDR_MAX-start);

	fclose(f);

	puts("Program loaded.");
}

int main() {

	s = new_S();
	//load_program("./test_opcode.ch8", 0x200);
	//load_program("./roms/demos/maze (alt) [david winter, 199x].ch8", 0x200);
	//load_program("./roms/demos/Zero Demo [zeroZshadow, 2007].ch8", 0x200);
	//load_program("./roms/games/Pong [Paul Vervalin, 1990].ch8", 0x200);
	//load_program("./roms/games/Breakout (Brix hack) [David Winter, 1997].ch8", 0x200);
	//load_program("./roms/games/Brick (Brix hack, 1990).ch8", 0x200);
	//load_program("./roms/games/Space Invaders [David Winter].ch8", 0x200);
	//load_program("./roms/games/Tetris [Fran Dachille, 1991].ch8", 0x200);
	load_program("./roms/games/Tank.ch8", 0x200);



	u8 prog[] = {0x63,0x08,
				 0xF3,0x29, // I = spr Vx
				 //0x00,0xE0,
				 0xD0,0x05, // draw 5 lines at (1,1)
				 0x70,0x01, // Add 1 to V0
				 //0x30,0xFF, // skip if Vx = kk
				 0x12,0x00, // jump to the begginnig,

				 0xF1,0x0A, // way for key press

				 0xA2,0x00, // I = 200
				 0xF0,0x33, // store bcd representation on I
				 0x00,0x00};

	//load(prog, START,sizeof(prog)/sizeof(u8));

	init_video();
	SDL_Event e;
	char buf[10];

	while(!should_quit) {

		unsigned int ticks = SDL_GetTicks();


		while(SDL_PollEvent(&e)) {
			if(e.type == SDL_QUIT) should_quit=1;
			else if(e.type == SDL_KEYDOWN) {
				SDL_KeyCode sym = e.key.keysym.sym;
				if(sym == SDLK_ESCAPE) {
					if(!help) should_quit=1;
					else help = 0;
				}


				if(sym == SDLK_SPACE && !help) paused ^= 1;
				if(sym == SDLK_h) help ^= 1, paused = 1;


				if(sym == SDLK_UP) {
					cpu_interval>>=1;
					if(cpu_interval < 2) cpu_interval = 2;
				}
				if(sym == SDLK_DOWN) cpu_interval=((cpu_interval|1))<<1;


				if(sym == SDLK_0) keyboard[0] = 1;
				if(sym == SDLK_1) keyboard[1] = 1;
				if(sym == SDLK_2) keyboard[2] = 1;
				if(sym == SDLK_3) keyboard[3] = 1;
				if(sym == SDLK_4) keyboard[4] = 1;
				if(sym == SDLK_5) keyboard[5] = 1;
				if(sym == SDLK_6) keyboard[6] = 1;
				if(sym == SDLK_7) keyboard[7] = 1;
				if(sym == SDLK_8) keyboard[8] = 1;
				if(sym == SDLK_9) keyboard[9] = 1;

				if(sym == SDLK_a) keyboard[0xa] = 1;
				if(sym == SDLK_b) keyboard[0xb] = 1;
				if(sym == SDLK_c) keyboard[0xc] = 1;
				if(sym == SDLK_d) keyboard[0xd] = 1;
				if(sym == SDLK_e) keyboard[0xe] = 1;
				if(sym == SDLK_f) keyboard[0xf] = 1;

			} else if(e.type == SDL_KEYUP) {

				SDL_KeyCode sym = e.key.keysym.sym;


				if(sym == SDLK_0) keyboard[0] = 0;
				if(sym == SDLK_1) keyboard[1] = 0;
				if(sym == SDLK_2) keyboard[2] = 0;
				if(sym == SDLK_3) keyboard[3] = 0;
				if(sym == SDLK_4) keyboard[4] = 0;
				if(sym == SDLK_5) keyboard[5] = 0;
				if(sym == SDLK_6) keyboard[6] = 0;
				if(sym == SDLK_7) keyboard[7] = 0;
				if(sym == SDLK_8) keyboard[8] = 0;
				if(sym == SDLK_9) keyboard[9] = 0;

				if(sym == SDLK_a) keyboard[0xa] = 0;
				if(sym == SDLK_b) keyboard[0xb] = 0;
				if(sym == SDLK_c) keyboard[0xc] = 0;
				if(sym == SDLK_d) keyboard[0xd] = 0;
				if(sym == SDLK_e) keyboard[0xe] = 0;
				if(sym == SDLK_f) keyboard[0xf] = 0;
			}
		}

		if(ticks-last_update>cpu_interval && !paused) {
			last_update=ticks;
			update();
		}

		if(ticks-last_draw>16) {

			if(s->delay) s->delay -= 1;
			if(s->sound) s->sound -= 1;

			sprintf(buf, "%04X", ticks);

			SDL_SetRenderDrawColor(renderer, 20, 30, 100, 0xff);
			SDL_RenderClear(renderer);

			draw_program(1040, 40, START);
			draw_state(10,740);
			draw_controls(600, 730);
			draw_screen();

			if(help) draw_help(25, 25);

			SDL_RenderPresent(renderer);

			last_draw = ticks;
		}
	}

	free(s);
	return 0;
}
