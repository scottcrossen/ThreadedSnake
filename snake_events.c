//******************************************************************************
//	snake_events.c  (07/13/2015)
//
//  Author:			Paul Roper, Brigham Young University
//  Revisions:		1.0		11/25/2012	RBX430-1
//
//******************************************************************************
//
#include "msp430.h"
#include <stdlib.h>
#include "RBX430-1.h"
#include "RBX430_lcd.h"
#include "snake.h"
#include "snakelib.h"
#include "RBX430_i2c.h"
#include "pthreads.h"
#include <time.h>

extern volatile uint16 sys_event;			// pending events
extern uint8 FRAM_write(uint16,uint8);
extern uint8 FRAM_read(uint16);
extern uint8 FRAM_init(void);

volatile enum MODE game_mode;				// 0=idle, 1=play, 2=next
volatile uint16 overall_score;				// overall score
volatile uint16 score;						// current score
volatile uint16 seconds;					// time
volatile uint16 time_limit;					// time limit
volatile uint16	snake_length;				// snake length
volatile uint16 food_count;					// food count limit
volatile uint16 rock_count;					// rock count limit
volatile uint16 hole_count;					// rock count limit
volatile uint16 bonus_count;					// rock count limit
volatile uint16 food_amount;				// food count limit
volatile uint16 start_count;				// food count limit
volatile uint16 move_cnt;					// snake speed
pthread_t move_cid;

volatile uint8 level;						// current level (1-4)
volatile uint8 direction;					// current move direction
volatile uint8 head;						// head index into snake array
volatile uint8 tail;						// tail index into snake array
SNAKE snake[MAX_SNAKE];						// snake segments


extern const uint16 snake_text_image[];		// snake text image
extern const uint16 snake1_image[];			// snake image
extern const uint16 king_snake_image[];		// snake image
extern sem_t WDT_move;						// sem signal
extern sem_t WDT_move_exit;					// sem signal
extern sem_t LCD_update;					// sem signal
extern pthread_mutex_t LCD_mutex;


typedef struct bonus_struct{				// BONUS struct
	POINT point;
	uint8 time;
	uint8 on;
	void (*draw)(struct bonus_struct* bonus);
} BONUS;

typedef struct object_struct{				// Object struct
	uint8 x;
	uint8 y;
	void (*draw)(struct object_struct* object);
} OBJECT;

void draw_food(OBJECT* food){
	int obj_type=rand() % 3;	// Randomize the type of food displayed.
	if (obj_type==0) lcd_triangle(COL(food->x), ROW(food->y), 2, 1);
	else if (obj_type==1) lcd_star(COL(food->x), ROW(food->y), 2, 1);
	else if (obj_type==2) lcd_circle(COL(food->x), ROW(food->y), 2, 1);
	else if (obj_type==3) lcd_diamond(COL(food->x), ROW(food->y), 2, 1);
	else lcd_square(COL(food->x), ROW(food->y), 2, 1);
}
void draw_rock(OBJECT* rock){
	lcd_square(COL(rock->x), ROW(rock->y), 2, 1 + FILL);
}
void draw_hole(OBJECT* hole){
	lcd_square(COL(hole->x), ROW(hole->y), 2, 1);
}
void draw_bonus(BONUS* bonus){
	lcd_diamond(COL(bonus->point.x), ROW(bonus->point.y), 2, 1);
}
OBJECT*	foods;
OBJECT*	rocks;
OBJECT*	holes;
BONUS*	bonus;
char*	initials;


void create_food(uint16 i1, uint16 iterate_to){
	int xr = rand() % 24;
	int yr = rand() % 23;
	int i2;
	for (i2=0; i2<iterate_to; i2++){	// Check to make sure that we're not writing over a screen location with food already on it.
		if ((xr==(foods+i2)->x) && (yr==(foods+i2)->y)){
			xr = rand() % 24;	// If already there re-randomize.
			yr = rand() % 23;
			i2=0;
		}
	}
	for (i2=0; i2<hole_count; i2++){	// Check to make sure that we're not writing over a screen location with a hole already on it.
		if ((xr==(holes+i2)->x) && (yr==(holes+i2)->y)){
			xr = rand() % (24+0);
			yr = rand() % 23;
			i2=0;
		}
	}
	for (i2=0; i2<bonus_count; i2++){	// Check to make sure that we're not writing over a screen location with a bonus already on it.
		if ((xr==(bonus+i2)->point.x) && (yr==(bonus+i2)->point.y)){
			xr = rand() % (24+0);
			yr = rand() % 23;
			i2=0;
		}
	}
	for (i2=0; i2<rock_count; i2++){	// Check to make sure that we're not writing over a screen location with a rock already on it.
		if ((xr==(rocks+i2)->x) && (yr==(rocks+i2)->y)){
			xr = rand() % (24+0);
			yr = rand() % 23;
			i2=0;
		}
	}
	(foods+i1)->x=xr;
	(foods+i1)->y=yr;
	(foods+i1)->draw=draw_food;
	((foods+i1)->draw)(foods+i1);



}
void create_bonus(uint16 i1, uint16 iterate_to){
	int xr = rand() % (24+0);
	int yr = rand() % 23;
	int i2;
	for (i2=0; i2<iterate_to; i2++){	// Check to make sure that we're not writing over a screen location with a bonus already on it.
		if ((xr==(bonus+i2)->point.x) && (yr==(bonus+i2)->point.y)){
			xr = rand() % (24+0);
			yr = rand() % 23;
			i2=0;
		}
	}
	for (i2=0; i2<hole_count; i2++){	// Check to make sure that we're not writing over a screen location with a hole already on it.
		if ((xr==(holes+i2)->x) && (yr==(holes+i2)->y)){
			xr = rand() % (24+0);
			yr = rand() % 23;
			i2=0;
		}
	}
	for (i2=0; i2<food_count; i2++){	// Check to make sure that we're not writing over a screen location with food already on it.
		if ((xr==(foods+i2)->x) && (yr==(foods+i2)->y)){
			xr = rand() % (24+0);
			yr = rand() % 23;
			i2=0;
		}
	}
	for (i2=0; i2<rock_count; i2++){	// Check to make sure that we're not writing over a screen location with a rock already on it.
		if ((xr==(rocks+i2)->x) && (yr==(rocks+i2)->y)){
			xr = rand() % (24+0);
			yr = rand() % 23;
			i2=0;
		}
	}
	(bonus+i1)->point.x=xr;
	(bonus+i1)->point.y=yr;
	(bonus+i1)->time=8;
	(bonus+i1)->on= rand() % 1;	// Write out the location and data of the bonus.
	(bonus+i1)->draw=draw_bonus;
}

//------------------------------------------------------------------------------
//-- move snake event ----------------------------------------------------------
//
//void MOVE_SNAKE_event(void){
void* MOVE_SNAKE_event(void* arg){
	while(1){
		sem_wait(&WDT_move);
		if (game_mode == PLAY){
			if (level > 0)
			{
				uint16 i1;
				pthread_mutex_lock(&LCD_mutex);
				for (i1=0; i1<food_count; i1++){	// Check to see if we're eating food.
					if ((snake[head].point.x == (foods+i1)->x) && (snake[head].point.y == (foods+i1)->y)) {
						beep();	// Do all the effects and increase score.
						blink();
						score+=level;
						overall_score+=level;
						snake_length++;
						lcd_cursor(ROW(5), COL(23)+2);
						lcd_printf("\t%d",overall_score);
						if ((score/level) >= 10){	// Check win condition.
							sys_event |= END_GAME;
							game_mode=NEXT;
						}
						else {
							if (level!=2)
								create_food(i1, food_count);	// Create new food.
						}
						i1=(food_count);
					}
				}
				if (start_count>0) {start_count--; snake_length++;}
				if ((i1 != (food_count+1)) && (start_count <= 0)){
					delete_tail();					// delete tail
				}

				for (i1=0; i1<rock_count; i1++){	// Check to see if we're hitting a rock.
					if ((snake[head].point.x == (rocks+i1)->x) && (snake[head].point.y == (rocks+i1)->y)) {
						lcd_cursor(COL(0), ROW(0));
						lcd_printf("Rock");
						sys_event |= END_GAME;
						i1=rock_count;
					}
				}

				for (i1=tail; i1<head; i1++){		// Check to see if we're hitting our tail.
					//if (snake[head].xy == snake[(head - i1)].xy) {
					if ((snake[head].point.x == snake[i1].point.x) && (snake[head].point.y == snake[i1].point.y)){
						lcd_cursor(COL(0), ROW(0));
						lcd_printf("Tail");
						sys_event |= END_GAME;
						i1=head;
					}
				}
				for (i1=0; i1<hole_count; i1++){	// Check to see if it's time to teleport.
					if ((snake[head].point.x == (holes+i1)->x) && (snake[head].point.y == (holes+i1)->y)) {
						int tel_to = (rand() % hole_count);	// Pick a random hole to teleport to.
						while (i1 == tel_to) tel_to = (rand() % hole_count);	// Make sure it's not this one.
						snake[head].point.x = ((holes+tel_to)->x);
						snake[head].point.y = ((holes+tel_to)->y);
						i1=hole_count;
					}
				}

				for (i1=0; i1<bonus_count; i1++){	// Check to see if we're eating the bonus.
					if ((snake[head].point.x == (bonus+i1)->point.x) && (snake[head].point.y == (bonus+i1)->point.y)) {
						beep();
						blink();
						score+=level;
						overall_score+=(level*2);
						lcd_cursor(ROW(5), COL(23)+2);
						lcd_printf("\t%d",overall_score);
						(bonus+i1)->time=10;	// Create a new bonus
						i1=(food_count);
					}
				}

				pthread_mutex_unlock(&LCD_mutex);
			}
			pthread_mutex_lock(&LCD_mutex);
			add_head();						// add head
			lcd_point(COL(snake[head].point.x), ROW(snake[head].point.y), PENTX);
			pthread_mutex_unlock(&LCD_mutex);
		}
		//if (sem_trywait(&WDT_move_exit) == 0){
		//	move_cid=pthread_self();
		//	pthread_exit(NULL);
		//}
	}
} // end MOVE_SNAKE_event


//------------------------------------------------------------------------------
//-- new game event ------------------------------------------------------------
//
void NEW_GAME_event(void)
{
	lcd_backlight(1);					// turn on backlight
	switch (game_mode){
	case SETUP:
		pthread_create(NULL, NULL, MOVE_SNAKE_event, NULL);
		pthread_mutex_lock(&LCD_mutex);
		lcd_clear();						// clear lcd
		lcd_wordImage(snake1_image, (159-60)/2, 60, 1);
		lcd_wordImage(snake_text_image, (159-111)/2, 20, 1);
		lcd_diamond(COL(16), ROW(20), 2, 1);
		lcd_star(COL(17), ROW(20), 2, 1);
		lcd_circle(COL(18), ROW(20), 2, 1);
		lcd_square(COL(19), ROW(20), 2, 1);
		lcd_triangle(COL(20), ROW(20), 2, 1);
		initials = (char*)malloc(sizeof(char)*4);
		initials[0]='A';
		initials[1]='A';
		initials[2]='A';
		initials[3]=0x88;
		FRAM_init();
		lcd_cursor((score < 100) ? 75 : 66, 65);
		lcd_putchar(initials[0]);	// Create the initials to modify.
		lcd_putchar(initials[1]);
		lcd_putchar(initials[2]);
		score=0;
		lcd_cursor(COL(0), ROW(23));
		lcd_printf("Hi");
		lcd_cursor(COL(2)-3, ROW(23));
		lcd_printf("gh score: ");
		lcd_putchar(FRAM_read(0));	// Place the high score to be seen.
		lcd_putchar(FRAM_read(2));
		lcd_putchar(FRAM_read(4));
		lcd_printf("  %d",FRAM_read(6));
		pthread_mutex_unlock(&LCD_mutex);
		break;
	case IDLE:
		break;
	case NEXT:
		sys_event |= NEXT_LEVEL;
		level=0;
		overall_score=0;
		break;
	default:
		break;
	}
	return;
} // end NEW_GAME_event


//------------------------------------------------------------------------------
//-- start level event -----------------------------------------------------------
//
void START_LEVEL_event(void)
{
	//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	//	Add code here to setup playing board for next level
	//	Draw snake, foods, reset timer, set level, move_cnt etc
	//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	game_mode = PLAY;					// start level
	seconds = 0;						// restart timer
	foods = (OBJECT*)malloc(sizeof(OBJECT)*food_amount);	// Allocate memory
	rocks = (OBJECT*)malloc(sizeof(OBJECT)*rock_count);
	holes = (OBJECT*)malloc(sizeof(OBJECT)*hole_count);
	bonus = (BONUS*)malloc(sizeof(BONUS)*bonus_count);
	if (((foods == 0) && (food_amount !=0)) || ((rocks == 0) && (rock_count !=0)) || ((holes == 0) && (hole_count !=0)) || ((bonus == 0) && (bonus_count !=0))){ //Check if memory full.
	  if (bonus == 0) bonus_count=0;
	  if (holes == 0) hole_count=0;
	  if (rocks == 0) rock_count=0;
	  if (foods == 0) food_amount=0;
	}
	uint16 i1;


	pthread_mutex_lock(&LCD_mutex);
	for(i1=0; i1<rock_count; i1++){	// Place the desired amount of rocks.
		int xr = rand() % 24;
		int yr = rand() % 23;
		int i2;
		for (i2=0; i2<i1; i2++){
			if ((xr==(rocks+i2)->x) && (yr==(rocks+i2)->y)){	// Check to make sure that we're not writing over a screen location with a rock already on it.
				xr = rand() % 24;	// Re-randomize if true;
				yr = rand() % 23;
				i2=0;
			}
		}
		(rocks+i1)->x=xr;
		(rocks+i1)->y=yr;
		//lcd_square(COL((rocks+i1)->x), ROW((rocks+i1)->y), 2, 1 + FILL);
		(rocks+i1)->draw=draw_rock;
		((rocks+i1)->draw)(rocks+i1);
	}


	for(i1=0; i1<hole_count; i1++){	// Create the desired amount of holes
		int xr = rand() % 24;
		int yr = rand() % 23;
		int i2;
		for (i2=0; i2<i1; i2++){	// Check to make sure that we're not writing over a screen location with a hole already on it.
			if ((xr==(holes+i2)->x) && (yr==(holes+i2)->y)){
				xr = rand() % 24;
				yr = rand() % 23;
				i2=0;
			}
		}
		for (i2=0; i2<rock_count; i2++){	// Check to make sure that we're not writing over a screen location with a rock already on it.
			if ((xr==(rocks+i2)->x) && (yr==(rocks+i2)->y)){
				xr = rand() % 24;
				yr = rand() % 23;
				i2=0;
			}
		}
		(holes+i1)->x=xr;
		(holes+i1)->y=yr;
		//lcd_square(COL((holes+i1)->x), ROW((holes+i1)->y), 2, 1);
		(holes+i1)->draw=draw_hole;
		((holes+i1)->draw)(holes+i1);
	}


	for(i1=0; i1<food_amount; i1++){	// Create the desired amount of food.
		create_food(i1, i1);
	}
	for(i1=0; i1<bonus_count; i1++){	// Create the desired amount of bonus.
		create_bonus(i1, i1);
	}
	pthread_mutex_unlock(&LCD_mutex);
	return;
} // end START_LEVEL_event


//------------------------------------------------------------------------------
//-- next level event -----------------------------------------------------------
//
void NEXT_LEVEL_event(void)
{
	lcd_clear();						// clear lcd
	if (level==4){
		sys_event |= END_GAME;	// You won. Go to king snake.
		game_mode = IDLE;
		return;
	}
	else if (level==3){			// Initialize level four.
		hole_count=5;
		bonus_count=2;
		rock_count=rand() % 10;
		free(foods);
		free(rocks);
		free(holes);
		free(bonus);
		level = 4;
		time_limit=60;
		food_count=10;
		move_cnt = WDT_MOVE4;				// level 2, speed 2
		food_amount=1;
		start_count=30;
	}
	else if (level==2){			// Initialize level three.
		hole_count=4;
		bonus_count=2;
		rock_count=rand() % 10;
		free(foods);
		free(rocks);
		free(holes);
		free(bonus);
		level = 3;
		time_limit=45;
		food_count=10;
		move_cnt = WDT_MOVE3;				// level 2, speed 2
		food_amount=1;
		start_count=20;
	}
	else if (level==1){			// Initialize level two.
		hole_count=3;
		bonus_count=2;
		rock_count=rand() % 10;
		free(foods);
		free(rocks);
		free(holes);
		free(bonus);
		level = 2;
		time_limit=30;
		food_count=10;
		move_cnt = WDT_MOVE2;				// level 2, speed 2
		food_amount=10;
		start_count=10;
	}
	else if (level==0){			// Initialize level one.
		hole_count=2;
		rock_count=0;
		bonus_count=2;
		level = 1;
		time_limit=30;
		food_count=10;
		move_cnt = WDT_MOVE1;				// level 2, speed 2
		food_amount=10;
		start_count=0;
	}
	snake_length=0;
	score = 0;
	direction=RIGHT;
	//new_snake(score, RIGHT);				// Create new snake.
	if ((direction== UP) || (direction==DOWN)) new_snake(0, UP);				// Create new snake.
	if ((direction== LEFT) || (direction==RIGHT)) new_snake(0, RIGHT);				// Create new snake.
	sys_event |= START_LEVEL;
	pthread_mutex_lock(&LCD_mutex);
	lcd_cursor(ROW(0)-7, COL(23)+2);
	lcd_printf("SCORE:     LEVEL:        ");	// Display new level.
	lcd_cursor(ROW(5), COL(23)+2);
	lcd_printf("\t%d",overall_score);
	lcd_cursor(ROW(16)+1, COL(23)+2);
	lcd_printf("\t%d",level);
	pthread_mutex_unlock(&LCD_mutex);
} // end NEXT_LEVEL_event


//------------------------------------------------------------------------------
//-- end game event -------------------------------------------------------------
//
void END_GAME_event(void)
{
	if (game_mode == NEXT){						// this means we must have a new level.
		pthread_mutex_lock(&LCD_mutex);
		lcd_cursor(COL(3), 75);
		lcd_printf("\bNEW LEVEL");
		lcd_cursor(COL(3), 53);
		lcd_printf("\bSCORE:%d", overall_score);
		pthread_mutex_unlock(&LCD_mutex);
		charge();
		uint8 i1;
		for (i1=0; i1<(snake_length); i1++){		// Check to see if we're hitting our tail.
			delete_tail();
		}
	}
	if (game_mode == IDLE){						// Give me the victory logo.
		free(foods);
		free(rocks);
		free(holes);
		free(bonus);
		pthread_mutex_lock(&LCD_mutex);
		lcd_clear();							// clear lcd
		lcd_wordImage(king_snake_image, (159-60)/2-10, 45, 1);
		//lcd_wordImage(snake_text_image, (159-111)/2, 20, 1);
		lcd_cursor(COL(5)+4, 25);
		lcd_printf("\bWINNER!");
		lcd_cursor(COL(4)+4, 3);
		lcd_printf("\bSCORE:%d", overall_score);
		pthread_mutex_unlock(&LCD_mutex);
		if (overall_score >= FRAM_read(6)){
			FRAM_write(0,initials[0]);
			FRAM_write(2,initials[1]);
			FRAM_write(4,initials[2]);
			FRAM_write(6,overall_score);
		}
		free(initials);
		imperial_march();
		sys_event=0;
		//sem_signal(&WDT_move_exit);
		//pthread_join(move_cid, NULL);
	}
	if ((game_mode == PLAY) || (game_mode == EOG)){	// You're a loser.
		free(foods);
		free(rocks);
		free(holes);
		free(bonus);
		pthread_mutex_lock(&LCD_mutex);
		lcd_cursor(COL(3), 75);
		lcd_printf("\bGAME OVER");
		lcd_cursor(COL(3), 53);
		lcd_printf("\bSCORE:%d", overall_score);
		pthread_mutex_unlock(&LCD_mutex);
		if (overall_score >= FRAM_read(6)){
			FRAM_write(0,initials[0]);
			FRAM_write(2,initials[1]);
			FRAM_write(4,initials[2]);
			FRAM_write(6,overall_score);
		}
		game_mode = EOG;
		free(initials);
		rasberry();
		sys_event=NEW_GAME;
		//sem_signal(&WDT_move_exit);
		//pthread_join(move_cid, NULL);
	}
} // end END_GAME_event


//------------------------------------------------------------------------------
//-- switch #1 event -----------------------------------------------------------
//
void SWITCH_1_event(void)
{
	switch (game_mode)
	{
		case IDLE:						// NEW_GAME will know what to do.
			sys_event |= NEW_GAME;
			break;
		case SETUP:						// finished writing initials.
			game_mode=NEXT;
			sys_event |= NEW_GAME;
			break;
		case PLAY:						// Move in direction.
			if (direction != LEFT)
			{
				if (snake[head].point.x < X_MAX)
				{
					direction = RIGHT;
					sys_event |= MOVE_SNAKE;
				}
			}
			break;
		case NEXT:						// start next level.
			sys_event |= NEXT_LEVEL;
			break;
		case EOG:						// You Won. goto king snake.
			sys_event |= NEW_GAME;
			game_mode=SETUP;
			break;
	}
	return;
} // end SWITCH_1_event


//------------------------------------------------------------------------------
//-- switch #2 event -----------------------------------------------------------
//
void SWITCH_2_event(void)
{
	switch (game_mode)
	{
	case SETUP:						// Move cursor left.
		if (score>0)
			score--;
		break;
	case NEXT:						// start next level.
		sys_event |= NEXT_LEVEL;
		break;
	case PLAY:						// Move in direction.
		if (direction != RIGHT)
		{
			if (snake[head].point.x > X_MIN)
			{
				direction = LEFT;
				sys_event |= MOVE_SNAKE;
			}
		}
	default:
		break;

	}

} // end SWITCH_2_event


//------------------------------------------------------------------------------
//-- switch #3 event -----------------------------------------------------------
//
void SWITCH_3_event(void)
{
	switch (game_mode)
	{
	case SETUP:						// Move cursor right.
		if (score<2)
			score++;
		break;
	case NEXT:						// start next level.
		sys_event |= NEXT_LEVEL;
		break;
	case PLAY:						// Move in direction.
		if (direction != UP)
		{
			if (snake[head].point.y > Y_MIN)
			{
				direction = DOWN;
				sys_event |= MOVE_SNAKE;
			}
		}
	default:
		break;

	}
} // end SWITCH_3_event


//------------------------------------------------------------------------------
//-- switch #4 event -----------------------------------------------------------
//
void SWITCH_4_event(void)
{
	switch (game_mode)
	{
	case SETUP:						// increase the initials.
		if (initials[score]<'Z')
			(initials[score])+=1;
		else (initials[score])='A';
		lcd_cursor((score < 100) ? 75 : 66, 65);
		lcd_putchar(initials[0]);
		lcd_putchar(initials[1]);
		lcd_putchar(initials[2]);
		break;
	case NEXT:						// start next level.
		sys_event |= NEXT_LEVEL;
		break;
	case PLAY:						// Move in direction.
		if (direction != DOWN)
		{
			if (snake[head].point.y < Y_MAX)
			{
				direction = UP;
				sys_event |= MOVE_SNAKE;
			}
		}
	default:
		break;

}
} // end SWITCH_4_event


//------------------------------------------------------------------------------
//-- update LCD event -----------------------------------------------------------
//
void* LCD_UPDATE_event(void* arg){
//void LCD_UPDATE_event(void){
	//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
	//	Add code here to handle LCD_UPDATE event
	//lcd_cursor((score < 100) ? 75 : 66, 65);
	//lcd_printf("\b\t%d", seconds);


	while(1){
		sem_wait(&LCD_update);
		if(game_mode==PLAY){
			pthread_mutex_lock(&LCD_mutex);
			lcd_cursor(ROW(20), COL(23)+2);					// Write current time and score.
			if ((time_limit-seconds) <10)
				lcd_printf("\t%d:0%d",(time_limit-seconds)/60,(time_limit-seconds)%60);//,score,level, seconds);
			else
				lcd_printf("\t%d:%d",(time_limit-seconds)/60,(time_limit-seconds)%60);//,score,level, seconds);
			if ((time_limit-seconds) == 0){
				sys_event |= END_GAME;
				lcd_cursor(COL(0), ROW(0));
				lcd_printf("Time");
			}
			uint8 i1;
			for (i1=0; i1<hole_count; i1++){				// make sure our worm holes still look like worm holes by deleting and re=creating them.
				lcd_cursor(COL((holes+i1)->x)-3, ROW((holes+i1)->y)-3);
				lcd_putchar(' ');
				//lcd_rectangle(COL((holes+i1)->x)-2, ROW((holes+i1)->y)-2, 3, 3, 0xf0);
				lcd_square(COL((holes+i1)->x), ROW((holes+i1)->y), 2, 1);
			}

			for (i1=0; i1<bonus_count; i1++){				// Test to see if the bonuses need to disappear.
				if (((bonus+i1)->time) >= 9){
					create_bonus(i1,bonus_count);
				}
				else if ((((bonus+i1)->time)--) <= 0){
					lcd_cursor(COL((bonus+i1)->point.x)-3, ROW((bonus+i1)->point.y)-3);
					lcd_putchar(' ');
					create_bonus(i1,bonus_count);
				}
				if((bonus+i1)->on==1){						// flash the bonuses.
					((bonus+i1)->draw)(bonus+i1);
					//lcd_diamond(COL((bonus+i1)->point.x), ROW((bonus+i1)->point.y), 2, 1);
					(bonus+i1)->on=0;
				}
				else {										// flash the bonuses.
					lcd_cursor(COL((bonus+i1)->point.x)-3, ROW((bonus+i1)->point.y)-3);
					lcd_putchar(' ');
					//lcd_rectangle(COL((bonus+i1)->point.x)-2, ROW((bonus+i1)->point.y)-2, 5, 5, 0xf0);
					(bonus+i1)->on=1;
				}
				pthread_mutex_unlock(&LCD_mutex);
			}
		}
	}
	//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
} // end LCD_UPDATE_event
