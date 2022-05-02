#include <stdint.h>
#include <LiquidCrystal.h>
#include <YetAnotherPcInt.h>
#include <EEPROM.h>

#define HIGH_SCORE_ADDR 0

#define BUZZER          6

#define LED_RED        A0
#define LED_BLUE       A1
#define LED_GREEN      A2
#define LED_YELLOW     A3

#define BUTTON_GREEN    2
#define BUTTON_BLUE     3
#define BUTTON_RED      4
#define BUTTON_YELLOW   5
#define BUTTON_ENTER    7

#define AMOUNT_OF_BUTTONS_AND_LEDS 4

#define MAX_SEQUENCE_LENGTH 12
#define MIN_SEQUENCE_LENGTH 4
#define MAX_AMOUNT_OF_STEPS_IN_SEQUENCE 200

#define START_DELAY 1000

typedef struct
{
  uint8_t button;
  uint8_t led;
} Led_Button;

static Led_Button button_led_map[AMOUNT_OF_BUTTONS_AND_LEDS] =
{
  {BUTTON_RED, LED_RED}, {BUTTON_BLUE, LED_BLUE}, {BUTTON_GREEN, LED_GREEN}, {BUTTON_YELLOW, LED_YELLOW}
};

typedef enum
{
  Game_GenerateGameSequenceState,
  Game_IndicateButtonState,
  Game_AwaitButtonPressState,
  Game_SuccessfullyPressedButtonState,
  Game_QuickTimeEventEnterState,
  Game_GameOverState,
  Game_CheckIfDoneState,
  Game_DoneState,
  Game_WaitForStartPressState,
  Game_LastState,
} Game_State;

typedef struct
{
  Game_State current_state;
  bool should_press_button;
  bool has_pressed_button;
  uint8_t button_number_pressed;
  uint8_t game_sequence[MAX_AMOUNT_OF_STEPS_IN_SEQUENCE];
  uint16_t sequence_index;
  uint16_t numbers_in_sequence;
  bool first_run;
  uint8_t successful_hits;
  uint8_t quicktime_score;
  uint8_t high_score;
  uint16_t timeout;
  uint16_t time_counter;
  uint32_t reaction_time;
  uint16_t loop_delay;
  uint8_t enter_quicktime_countdown;
} Game_InstanceData;

static Game_InstanceData instance_data;
static LiquidCrystal lcd(13, 12, 11, 10, 9 , 8 );
static bool sound_enabled = false;

typedef void (*Game_StateHdlFunc)(Game_InstanceData *inst_data);

static void Game_GenerateGameSequenceStateHandler(Game_InstanceData *inst_data)
{
  if (inst_data != NULL)
  {
    inst_data->numbers_in_sequence = MAX_AMOUNT_OF_STEPS_IN_SEQUENCE;

    for (uint8_t i = 0; i < inst_data->numbers_in_sequence; i++)
    {
      inst_data->game_sequence[i] = random(0, AMOUNT_OF_BUTTONS_AND_LEDS);
    }

    lcd.clear();
    lcd.home();
    lcd.print("     Ready");
    delay(500);

    lcd.clear();
    lcd.home();
    lcd.print("      Set");
    delay(500);

    lcd.clear();
    lcd.home();
    lcd.print("      Go!");
    delay(1000);

    inst_data->successful_hits = 0;
    inst_data->quicktime_score = 0;
    inst_data->reaction_time = 0;
    inst_data->current_state = Game_IndicateButtonState;
  }
}

static void Game_IndicateButtonStateHandler(Game_InstanceData *inst_data)
{
  if (inst_data != NULL)
  {
    const uint8_t current_sequence_number = inst_data->game_sequence[inst_data->sequence_index];
    const uint8_t current_led = button_led_map[current_sequence_number].led;

    if (sound_enabled)
    {
      analogWrite(BUZZER, 60);
    }
    digitalWrite(current_led, HIGH);
    inst_data->should_press_button = true;
    delay(250);
    digitalWrite(current_led, LOW);
    if (sound_enabled)
    {
      analogWrite(BUZZER, 0);
    }

    inst_data->current_state = Game_AwaitButtonPressState;
  }
}

static void Game_AwaitButtonPressStateHandler(Game_InstanceData *inst_data)
{
  if (inst_data != NULL)
  {
    inst_data->time_counter = 0;

    while (inst_data->time_counter < inst_data->timeout && inst_data->has_pressed_button == false)
    {
      inst_data->time_counter += 5;
      delay(5);
    }

    if (inst_data->has_pressed_button)
    {
      inst_data->current_state = Game_SuccessfullyPressedButtonState;
      inst_data->has_pressed_button = false;
    }
    else
    {
      inst_data->current_state = Game_GameOverState;
    }

    inst_data->should_press_button = false;
  }
}

static void Game_SuccessfullyPressedButtonStateHandler(Game_InstanceData *inst_data)
{
  if (inst_data != NULL)
  {
    const uint8_t current_sequence_number = inst_data->game_sequence[inst_data->sequence_index];
    if (inst_data->button_number_pressed == button_led_map[current_sequence_number].button)
    {
      lcd.clear();
      lcd.home();

      if (inst_data->time_counter < 200)
      {
        lcd.print("     FAST!");
      }
      else if (inst_data->time_counter < 500)
      {
        lcd.print("     Cool!");
      }
      else
      {
        lcd.print("Barely made it");
      }

      inst_data->reaction_time += inst_data->time_counter;
      ++inst_data->successful_hits;


      if (instance_data.loop_delay >= 25)
      {
        instance_data.loop_delay -= 25;
      }
      if (inst_data->timeout >= 25)
      {
        inst_data->timeout -= 5;
      }
      else
      {
        inst_data->timeout = 5;
      }
      delay(inst_data->loop_delay);
      if(random(0, 10) == 0) {
        inst_data->enter_quicktime_countdown = 8;
        inst_data->current_state = Game_QuickTimeEventEnterState;
      }
      else 
      {
        inst_data->current_state = Game_CheckIfDoneState;
      }
    }
    else
    {
      /* Hit wrong button - game over! */
      inst_data->current_state = Game_GameOverState;
    }
  }
}


static void Game_QuickTimeEventEnterHandler(Game_InstanceData *inst_data)
{
  if (inst_data != NULL)
  {
    int button_value = 0;
    int wait_loop = 10;
    lcd.home();
    lcd.clear();
    lcd.print("  Smash ENTER!");
    lcd.setCursor(0, 1);
    switch(inst_data->enter_quicktime_countdown) {
      case 8:
        lcd.print("****************");
        break;
      case 7:
        lcd.print(" ************** ");
        break;
      case 6:
        lcd.print("  ************  ");
        break;
      case 5:
        lcd.print("   **********   ");
        break;
      case 4:
        lcd.print("    ********    ");
        break;
      case 3:
        lcd.print("     ******     ");
        break;
      case 2:
        lcd.print("      ****      ");
        break;
      case 1:
        lcd.print("       **       ");
        break;
      case 0:
        lcd.print("                ");
        break;
    }
    lcd.setCursor(0, 0);
    
    while(wait_loop > 0) {
      delay(8);
      button_value = digitalRead(BUTTON_ENTER);
      wait_loop--;
      if(button_value == LOW) 
      {
        break;
      }
    }
    
    if(button_value == LOW)
    {
      inst_data->current_state = Game_CheckIfDoneState;
      inst_data->quicktime_score += inst_data->enter_quicktime_countdown;
      lcd.home();
      lcd.clear();
      lcd.print("    Awesome!");
      delay(500);
    }
    else if(inst_data->enter_quicktime_countdown == 0)
    {
      inst_data->current_state = Game_GameOverState;
    }
    inst_data->enter_quicktime_countdown--;
  }
}

static void Game_GameOverStateHandler(Game_InstanceData *inst_data)
{
  if (inst_data != NULL)
  {
    lcd.home();
    lcd.clear();
    lcd.print("   Game over!");

    int melody[] = { 262, 196, 196, 220, 196, 0, 247, 262 };

    // note durations: 4 = quarter note, 8 = eighth note, etc.:
    int noteDurations[] = { 4, 8, 8, 4, 4, 4, 4, 4 };

    for (int thisNote = 0; thisNote < 8; thisNote++) {
      int noteDuration = 1000 / noteDurations[thisNote];
      tone(BUZZER, melody[thisNote], noteDuration);
      int pauseBetweenNotes = noteDuration * 1.30;
      delay(pauseBetweenNotes);
      noTone(12);
    }

    delay(1000);

    inst_data->current_state = Game_DoneState;
  }
}

static void Game_CheckIfDoneStateHandler(Game_InstanceData *inst_data)
{
  if (inst_data != NULL)
  {
    if (inst_data->sequence_index >= inst_data->numbers_in_sequence)
    {
      /* We're done with this sequence */
      inst_data->current_state = Game_DoneState;
      instance_data.sequence_index = 0;
    }
    else
    {
      /* Go to next button/LED in sequence */
      inst_data->current_state = Game_IndicateButtonState;
      ++inst_data->sequence_index;
    }

    instance_data.should_press_button = false;
    instance_data.has_pressed_button = false;
  }
}

static void Game_DoneStateHandler(Game_InstanceData *inst_data)
{
  if (inst_data != NULL)
  {
    char score_buffer[50] = "";
    char highscore_buffer[50] = "";
    char reaction_buffer[50] = "";
    uint16_t average_reaction_time = 0;

    lcd.home();
    lcd.clear();

    if ((inst_data->successful_hits + inst_data->quicktime_score) > inst_data->high_score)
    {
      inst_data->high_score = inst_data->successful_hits + inst_data->quicktime_score;

      EEPROM.write(HIGH_SCORE_ADDR, inst_data->high_score);

      lcd.home();
      lcd.clear();

      for (uint8_t i = 0; i < 8; i++)
      {
        lcd.print("New high score!");
        delay(200);
        lcd.clear();
        delay(200);
      }
    }

    sprintf(score_buffer, "Your score: %d", inst_data->successful_hits +  + inst_data->quicktime_score);

    lcd.home();
    lcd.clear();
    lcd.print(score_buffer);
    lcd.setCursor(0, 1);
    sprintf(highscore_buffer, "High score: %d", inst_data->high_score);
    lcd.print(highscore_buffer);
    lcd.setCursor(0, 0);

    delay(3000);

    if (inst_data->successful_hits > 0)
    {
      average_reaction_time = (inst_data->reaction_time) / (inst_data->successful_hits);

      lcd.home();
      lcd.clear();
      lcd.print("Average reaction:");
      sprintf(reaction_buffer, "     %d ms", average_reaction_time);
      lcd.setCursor(0, 1);
      lcd.print(reaction_buffer);
      lcd.setCursor(0, 0);

      delay(3000);
    }

    inst_data->current_state = Game_WaitForStartPressState;
  }
}

static void Game_WaitForStartPressStateHandler(Game_InstanceData *inst_data)
{
  static bool once = false;

  if (inst_data != NULL)
  {
    int button_value = 0;
    inst_data->loop_delay = START_DELAY;
    inst_data->timeout = 800;

    button_value = digitalRead(BUTTON_ENTER);

    if (button_value == LOW)
    {
      inst_data->current_state = Game_GenerateGameSequenceState;
      once = false;
    }
    else if (once == false)
    {
      once = true;

      lcd.clear();
      lcd.home();
      lcd.print("  Smash ENTER!");
    }
    else if (inst_data->button_number_pressed == BUTTON_GREEN && inst_data->has_pressed_button == true) {

      once = false;
      if (sound_enabled) {
        sound_enabled = false;
        lcd.clear();
        lcd.home();
        lcd.print("  Smash ENTER!");
        lcd.setCursor(0, 1);
        lcd.print(" Sound disabled");
        lcd.setCursor(0, 0);
        delay(1000);
      }
      else
      {
        sound_enabled = true;
        lcd.clear();
        lcd.home();
        lcd.print("  Smash ENTER!");
        lcd.setCursor(0, 1);
        lcd.print(" Sound enabled");
        lcd.setCursor(0, 0);
        delay(1000);
      }
      inst_data->has_pressed_button = false;
    }
  }
}

static const Game_StateHdlFunc Game_StateHandlers[Game_LastState] = {
  Game_GenerateGameSequenceStateHandler,
  Game_IndicateButtonStateHandler,
  Game_AwaitButtonPressStateHandler,
  Game_SuccessfullyPressedButtonStateHandler,
  Game_QuickTimeEventEnterHandler,
  Game_GameOverStateHandler,
  Game_CheckIfDoneStateHandler,
  Game_DoneStateHandler,
  Game_WaitForStartPressStateHandler,
};

void Game_Task(const uint16_t call_rate)
{
  if (instance_data.current_state < Game_LastState &&
      Game_StateHandlers[instance_data.current_state] != NULL)
  {
    Game_StateHandlers[instance_data.current_state](&instance_data);
  }
}

void button_red_interrupt_handler()
{
  instance_data.button_number_pressed = BUTTON_RED;
  instance_data.has_pressed_button = true;
}

void button_blue_interrupt_handler()
{
  instance_data.button_number_pressed = BUTTON_BLUE;
  instance_data.has_pressed_button = true;
}

void button_green_interrupt_handler()
{
  instance_data.button_number_pressed = BUTTON_GREEN;
  instance_data.has_pressed_button = true;
}

void button_yellow_interrupt_handler()
{
  instance_data.button_number_pressed = BUTTON_YELLOW;
  instance_data.has_pressed_button = true;
}

void setup() {
  randomSeed(analogRead(0));

  pinMode(BUZZER,     OUTPUT);

  pinMode(LED_RED,    OUTPUT);
  pinMode(LED_BLUE,   OUTPUT);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);

  pinMode(BUTTON_RED,    INPUT);
  pinMode(BUTTON_BLUE,   INPUT);
  pinMode(BUTTON_GREEN,  INPUT);
  pinMode(BUTTON_YELLOW, INPUT);
  pinMode(BUTTON_ENTER,  INPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON_BLUE),   button_blue_interrupt_handler, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_GREEN),  button_green_interrupt_handler, FALLING);
  PcInt::attachInterrupt(BUTTON_RED,    button_red_interrupt_handler, FALLING);
  PcInt::attachInterrupt(BUTTON_YELLOW, button_yellow_interrupt_handler, FALLING);

  memset(&instance_data, 0x00, sizeof(instance_data));

  //Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.clear();

  instance_data.current_state = Game_WaitForStartPressState;
  instance_data.should_press_button = false;
  instance_data.has_pressed_button = false;
  instance_data.button_number_pressed = 0;
  instance_data.first_run = true;
  instance_data.sequence_index = 0;
  instance_data.timeout = 800;
  instance_data.loop_delay = START_DELAY;

  instance_data.high_score = EEPROM.read(HIGH_SCORE_ADDR);
  if (instance_data.high_score == 0xFF)
  {
    instance_data.high_score = 0;
  }

  memset(&instance_data.game_sequence, 0x00, MAX_AMOUNT_OF_STEPS_IN_SEQUENCE);
}

void loop() {
  Game_Task(instance_data.loop_delay);
}
