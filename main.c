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

typedef struct {

	u16 pc;
	u16 stack[16];

	u16 ram[ADDR_MAX]; // FIXME: u8 or u16?
	u8 sp;
	u8 V[16]; // registers
	u16 I; // Address register

	u8 sound, delay;

}S; // S for state

S* s; // s is the holds the current vm state
u16 START = 0x200; // were the program is loaded
char code[8]; // a little helper string which holds the name of the latest processed instruction

S* new_S() {
	S* s = calloc(1, sizeof(S));

	return s;
}

void op_CLS() {

}

void op_RET() {

}

void op_JMP(u16 addr) {
	s->pc = addr;
}

void op_CALL(u16 addr) {
	s->sp++;
	s->stack[s->sp] = s->pc;
	s->pc = addr;
}

void op_SE(u8 x, u8 byte) {

	x = s->V[x];

	if(x == byte) {
		s->pc += 1;
	}

	s->pc += 1;
}

void op_SNE(u8 x, u8 byte) {

	if(x != byte) {
		s->pc += 1;
	}

	s->pc += 1;
}

void op_LD(u8* x, u8 byte) {
	*x = byte;
	s->pc += 1;
}

void op_OR(u8 x, u8 y) {
	s->V[x] |= y;
	s->pc += 1;
}

void op_AND(u8 x, u8 y) {
	s->V[x] &= y;
	s->pc += 1;
}

void op_XOR(u8 x, u8 y) {
	s->V[x] ^= y;
	s->pc += 1;
}

void op_ADD(u8 x, u8 y) {
	int a = s->V[x];
	int b = y;

	if(a+b>0xff) s->V[0xF]=1;

	s->V[x] = (a+b)&0xff;

	s->pc += 1;
}

void op_SUB(u8 x, u8 y) {
	s->V[0xF] = (s->V[x]>y);
	s->V[x] -= y;

	s->pc += 1;
}

void op_SHR(u8 x, u8 y) {
	s->V[0xF] = s->V[x]&1;
	s->V[x]/=2;

	s->pc += 1;
}

void op_SUBN(u8 x, u8 y) {
	s->V[0xF] = (s->V[x]<y);
	s->V[x] = y - s->V[x];

	s->pc += 1;
}

void op_SHL(u8 x, u8 y) {
	s->V[0xF] = s->V[x]&1;
	s->V[x]*=2;

	s->pc += 1;
}

void op_LDI(u16 x) {
	s->I = x;
	s->pc += 1;
}

void op_RND(u8 x, u8 kk) {

}

void op_DRW(u8 x, u8 y, u8 n) {

}

void op_SKP(u8 x) {

}

void op_SKNP(u8 x) {

}

void op_LDK(u8 x) {

}

void op_LDSPR(u8 x) {

}

void op_LDB(u8 x) {

}

void op_LDIX(u8 x) {

}

void op_LDXI(u8 x) {

}

// Syntatic sugar for defining instructions
// Why wouldn't we modify C a little bit, just
// for convinience?
#define OP(name, ...)							\
	do {										\
		strcpy(code, #name);					\
		if(f) op_ ## name (__VA_ARGS__);		\
	} while(0)


void instr(u16 in, int f) {
	u16 nnn, n, x, y, kk, h, ht, hl;

	nnn = 0x0fff & in;
	n = 0x000f & in;
	x = (0x0f00 & in)>>(2*4);
	y = (0x00f0 & in)>>(4);
	kk = 0x00ff & in;
	h = (0xff00 & in)>>(2*4);
	ht = (0xf000 & in)>>(3*4);
	hl = (0x0f00 & in)>>(2*4);

	switch (ht) {
		case 0x0:
			if(n == 0x0) {
				OP(CLS);
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
					OP(LDSPR, x);
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
			break;
	}
}

void load(u16* tape, u16 start, int n) {
	for(int i=start;i<start+n;i++) {
		s->ram[i] = tape[i-start];
	}

	s->pc = start;
}

void print(u16 start) {

	putchar('\n');

	for(int i=start;i<5+start;i++) {
		instr(s->ram[i], 0);

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
	if(s->ram[s->pc] != 0xFFFF && s->ram[s->pc] != 0x0000)
		instr(s->ram[s->pc], 1);
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

	for(int i=0;i<44;i++) {
		instr(s->ram[start+i], 0);

		if(i>0 && s->ram[start+i]==0 && s->ram[start+(i-1)]==0) break;

		char buf[20];
		sprintf(buf, "%*s    0x%04X", 4, code, s->ram[start+i]);

		if(s->pc!=start+i) draw_text(x, y+i*off, buf,(SDL_Color){0xff,0xff,0xff,0xff});
		else draw_text(x, y+i*off, buf,(SDL_Color){200,20,20,255});
	}
}

void draw_state(int x, int y) {
	int off = 45;
	int line_off = 30;

	SDL_Rect r = {.x=0,.y=y,.w=1030,.h=H-y};
	SDL_SetRenderDrawColor(renderer, 50, 50, 120, 0xff);
	SDL_RenderFillRect(renderer, &r);

	draw_text(x,y+2,"Registers", (SDL_Color){0xff,0xff,0xff,0xff});

	for(int i=0;i<16;i++) {
		char buf[10]; sprintf(buf, "%04X", s->V[i]);
		draw_text(150+i*off+x, y+2, buf, (SDL_Color){0xff,0xff,0xff,0xff});
	}

	char buf[20]; sprintf(buf, "PC  %04X", s->pc);
	draw_text(x,y+line_off,buf, (SDL_Color){0xff,0xff,0xff,0xff});

	sprintf(buf, "SP  %04X", s->sp);
	draw_text(x+150,y+line_off,buf, (SDL_Color){0xff,0xff,0xff,0xff});


	sprintf(buf, "I  %04X", s->I);
	draw_text(x+300,y+line_off,buf, (SDL_Color){0xff,0xff,0xff,0xff});
}

int main() {

	s = new_S();

	u16 prog[] = { 0x7001, // Add 1 to V0
				   0x8100, // Vx = Vy
				   0x30FF, // skip if Vx = kk
				   0x1200, // jump to the begginnig,
				   0x0000, /*end*/};

	load(prog, START,5);

	init_video();
	SDL_Event e;
	char buf[10];
	while(!should_quit) {

		unsigned int ticks = SDL_GetTicks();


		while(SDL_PollEvent(&e)) {
			if(e.type == SDL_QUIT) should_quit=1;
			else if(e.type == SDL_KEYDOWN) {
				SDL_KeyCode sym = e.key.keysym.sym;
				if(sym == SDLK_ESCAPE) should_quit=1;
			}
		}

		if(ticks-last_update>200) {
			last_update=ticks;
			update();
		}

		if(ticks-last_draw>20) {
			sprintf(buf, "%04X", ticks);

			SDL_SetRenderDrawColor(renderer, 20, 30, 100, 0xff);
			SDL_RenderClear(renderer);

			draw_program(1040, 3, START);
			draw_state(10,740);

			SDL_RenderPresent(renderer);

			last_draw = ticks;
		}

	}

	free(s);
	return 0;
}
