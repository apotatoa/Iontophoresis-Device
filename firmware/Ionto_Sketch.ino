#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <MUIU8g2.h>

// Pin Definitions

// OLED Display (I2C)
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, /*scl=*/9, /*sda=*/8);

// Current Control (PWM to RC Filter -> Op-Amp)
const int CURRENT_CONTROL_PIN = 17; // GPIO 17 will output PWM for current magnitude

// H-Bridge Direction Control (Binary HIGH/LOW)
const int DRV_IN1_PIN = 39;
const int DRV_IN2_PIN = 45;

// Button Inputs
const int BUTTON_ENTER_PIN = 40;
const int BUTTON_INCREASE_PIN = 42;
const int BUTTON_DECREASE_PIN = 41;

// Buzzer Output
const int BUZZER_PIN = 5;

// PWM Configuration for the Current Control Pin
const int PWM_FREQUENCY = 25000;    // 25 kHz is great for smooth RC filtering
const int PWM_RESOLUTION_BITS = 8;  // 0-255 duty cycle range

// Global State Variables

// User Input Values 
uint8_t input_minutes = 0;
uint8_t input_seconds = 0;
uint8_t current_mA = 0; // The target current in milliamps (0-30mA)

// Treatment State 
bool treatment_active = false;
uint8_t direction = 0; // 0 for <-- (IN2 HIGH), 1 for --> (IN1 HIGH)
const char* direction_labels[] = { "<--", "-->" };

// Timer State 
uint32_t total_seconds_set = 0;
uint32_t elapsed_seconds = 0;
uint32_t last_tick_ms = 0;
uint8_t display_minutes = 0;
uint8_t display_seconds = 0;

// UI & Menu State 
MUIU8G2 mui;
uint8_t mui_exit_button_var = 0;
bool trigger_treatment_screen = false;
bool auto_return_to_menu = false;
bool countdown_finished_logged = false;
uint8_t redraw_menu = 1;
uint8_t current_field_index = 0; // Tracks which field is active: 0=MM, 1=SS, 2=CU, 3=OK

// MUI Setup

muif_t muif_list[] = {
  MUIF_U8G2_FONT_STYLE(0, u8g2_font_helvR08_tr),
  MUIF_U8G2_U8_MIN_MAX("MM", &input_minutes, 0, 20, mui_u8g2_u8_min_max_wm_mse_pi),
  MUIF_U8G2_U8_MIN_MAX("SS", &input_seconds, 0, 59, mui_u8g2_u8_min_max_wm_mse_pi),
  MUIF_U8G2_U8_MIN_MAX("CU", &current_mA, 0, 30, mui_u8g2_u8_min_max_wm_mse_pi),
  MUIF_VARIABLE("OK", &mui_exit_button_var, mui_u8g2_btn_exit_wm_fi),
  MUIF_LABEL(mui_u8g2_draw_text)
};

fds_t fds_data[] MUI_PROGMEM =
  MUI_FORM(1)
  MUI_STYLE(0)
  MUI_LABEL(5, 0, "------")
  MUI_LABEL(5, 8, "Minutes:")
  MUI_XY("MM", 64, 8)
  MUI_LABEL(96, 8, ":")
  MUI_XY("SS", 102, 8)
  MUI_LABEL(5, 24, "Current:")
  MUI_XY("CU", 80, 24)
  MUI_LABEL(5, 56, "------")
  MUI_XYT("OK", 110, 56, " OK ");

// Hardware Control 

/**
 * @brief Sets the current magnitude by adjusting the PWM duty cycle on the control pin.
 * This PWM signal is intended to be smoothed by an external RC filter.
 */
void set_current_level() {
  if (!treatment_active) {
    ledcWrite(CURRENT_CONTROL_PIN, 0); // Safety: ensure PWM is off if treatment is not active
    return;
  }
  // Map the desired current (0-30mA) to a PWM duty cycle (0-255)
  uint8_t duty_cycle = map(current_mA, 0, 30, 0, 255);
  ledcWrite(CURRENT_CONTROL_PIN, duty_cycle);
}

/**
 * @brief Sets the H-Bridge direction using simple binary logic.
 * IN1 HIGH, IN2 LOW = Direction 1 (-->)
 * IN1 LOW, IN2 HIGH = Direction 0 (<--)
 */
void set_h_bridge_direction() {
  if (direction == 1) { // Forward -->
    digitalWrite(DRV_IN2_PIN, LOW);
    digitalWrite(DRV_IN1_PIN, HIGH);
  } else { // Backward <--
    digitalWrite(DRV_IN1_PIN, LOW);
    digitalWrite(DRV_IN2_PIN, HIGH);
  }
}

/**
 * @brief Safely stops all treatment output. Sets current to 0 and disables H-Bridge.
 */
void stop_treatment_output() {
  ledcWrite(CURRENT_CONTROL_PIN, 0);
  digitalWrite(DRV_IN1_PIN, LOW);
  digitalWrite(DRV_IN2_PIN, LOW);
  Serial.println("Treatment Output STOPPED.");
}

/**
 * @brief Produces a series of beeps on the buzzer.
 * @param n Number of beeps.
 * @param t Duration of each beep in milliseconds.
 */
void beep(int n, int t) {
  for (int i = 0; i < n; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(t);
    digitalWrite(BUZZER_PIN, LOW);
    delay(t / 2);
  }
}

// UI and Timers 

void draw_menu() {
  if (!redraw_menu) return;
  u8g2.firstPage();
  do { mui.draw(); } while (u8g2.nextPage());
  redraw_menu = 0;
}

void handle_timer_tick() {
  if (!treatment_active) return;

  // Handle case where timer finishes
  if (elapsed_seconds >= total_seconds_set && total_seconds_set > 0) {
    if (!countdown_finished_logged) {
      Serial.println("Countdown Finished. Returning to menu.");
      countdown_finished_logged = true;
      auto_return_to_menu = true;
      beep(3, 50);
    }
    return;
  }

  // Update timer every second
  if (millis() - last_tick_ms >= 1000) {
    last_tick_ms = millis();
    elapsed_seconds++;

    // Switch direction halfway through the timer
    if (total_seconds_set > 0 && elapsed_seconds == total_seconds_set / 2) {
      direction = (direction == 0) ? 1 : 0; // Toggle direction
      Serial.printf("Direction auto-switched to %s\n", direction_labels[direction]);
    }

    uint32_t remaining_seconds = total_seconds_set - elapsed_seconds;
    display_minutes = remaining_seconds / 60;
    display_seconds = remaining_seconds % 60;
  }
}

void draw_treatment_screen() {
  handle_timer_tick();
  char buf[32];
  u8g2.setFont(u8g2_font_helvR08_tr);
  u8g2.firstPage();
  do {
    snprintf(buf, sizeof(buf), "Time %02u:%02u", display_minutes, display_seconds);
    u8g2.drawStr(0, 12, buf);
    snprintf(buf, sizeof(buf), "Cur  %2u mA", current_mA);
    u8g2.drawStr(0, 28, buf);
    u8g2.drawStr(0, 44, direction_labels[direction]);
  } while (u8g2.nextPage());
}

// SETUP 

void setup() {
  Serial.begin(115200);
  Serial.println("\nIontophoresis Controller Initializing...");

  // Configure Inputs
  pinMode(BUTTON_ENTER_PIN, INPUT_PULLUP);
  pinMode(BUTTON_INCREASE_PIN, INPUT_PULLUP);
  pinMode(BUTTON_DECREASE_PIN, INPUT_PULLUP);

  // Configure Outputs
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(DRV_IN1_PIN, OUTPUT);
  pinMode(DRV_IN2_PIN, OUTPUT);

  // Configure PWM for Current Control
  ledcAttach(CURRENT_CONTROL_PIN, PWM_FREQUENCY, PWM_RESOLUTION_BITS);
  
  // Ensure all outputs are off at startup
  stop_treatment_output();

  // Initialize UI
  u8g2.begin(BUTTON_ENTER_PIN, BUTTON_INCREASE_PIN, BUTTON_DECREASE_PIN, U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);
  mui.begin(u8g2, fds_data, muif_list, sizeof(muif_list) / sizeof(muif_t));
  mui.gotoForm(1, 0);
  current_field_index = 0; // Start at the first field
  Serial.println("Setup complete. Main menu active.");

  beep(1, 10);
}


// MAIN 

void loop() {
  if (mui.isFormActive()) {
    handle_menu_input();
    draw_menu();
  } else {
    if (trigger_treatment_screen) {
      setup_treatment_session();
      trigger_treatment_screen = false;
    }

    if (treatment_active) {
      handle_treatment_input();
      set_h_bridge_direction();
      set_current_level();
      draw_treatment_screen();

      if (auto_return_to_menu) {
        treatment_active = false;
        stop_treatment_output();
      }
    } else {
      return_to_main_menu();
    }
  }
}

// Helper Functions

/**
 * @brief Handles button presses.
 * +/- increases/decreases the value of the current field.
 * Enter confirms and moves to the next field.
 */
void handle_menu_input() {
  uint8_t event = u8g2.getMenuEvent();
  if (event) {
    redraw_menu = 1;
    switch (event) {
      case U8X8_MSG_GPIO_MENU_NEXT: // Increase button (+)
        if (current_field_index == 0 && input_minutes < 20) input_minutes++;
        else if (current_field_index == 1 && input_seconds < 59) input_seconds++;
        else if (current_field_index == 2 && current_mA < 30) current_mA++;
        break;

      case U8X8_MSG_GPIO_MENU_PREV: // Decrease button (-)
        if (current_field_index == 0 && input_minutes > 0) input_minutes--;
        else if (current_field_index == 1 && input_seconds > 0) input_seconds--;
        else if (current_field_index == 2 && current_mA > 0) current_mA--;
        break;

      case U8X8_MSG_GPIO_MENU_SELECT: // Enter button
        if (current_field_index < 3) {
          // Move to the next field
          current_field_index++;
          mui.nextField();
        } else {
          // We are on the "OK" button, so start the treatment
          Serial.println("OK pressed: Starting treatment setup...");
          trigger_treatment_screen = true;
          mui_exit_button_var = 1; // This signals MUI to exit the form
          mui.sendSelect();
          mui.leaveForm();
        }
        break;
    }
  }
}


/**
 * @brief Handles button presses during the active treatment session.
 */
void handle_treatment_input() {
  static unsigned long last_button_time = 0;
  const unsigned long button_debounce = 200;

  if (millis() - last_button_time > button_debounce) {
    if (digitalRead(BUTTON_INCREASE_PIN) == LOW) {
      if (current_mA < 30) current_mA++;
      last_button_time = millis();
    }
    if (digitalRead(BUTTON_DECREASE_PIN) == LOW) {
      if (current_mA > 0) current_mA--;
      last_button_time = millis();
    }
    if (digitalRead(BUTTON_ENTER_PIN) == LOW) {
      Serial.println("Enter button pressed during treatment. Stopping session.");
      auto_return_to_menu = true; // Signal to stop and return
      last_button_time = millis();
    }
  }
}

/**
 * @brief Prepares all state variables for the start of a new treatment session.
 */
void setup_treatment_session() {
  total_seconds_set = (uint32_t)input_minutes * 60 + input_seconds;
  elapsed_seconds = 0;
  last_tick_ms = millis();
  direction = 0; // Always start with the first direction
  display_minutes = input_minutes;
  display_seconds = input_seconds;
  
  auto_return_to_menu = false;
  countdown_finished_logged = false;

  if (total_seconds_set == 0) {
    Serial.println("Timer is 00:00. Treatment will not start.");
    treatment_active = false;
  } else {
    treatment_active = true;
    Serial.printf("=== Treatment Started: %02u:%02u, Current: %u mA ===\n", display_minutes, display_seconds, current_mA);
  }
}

/**
 * @brief Resets state and returns the UI to the main menu form.
 */
void return_to_main_menu() {
  Serial.println("Returning to main menu...");
  stop_treatment_output();

  input_minutes = 0;
  input_seconds = 0;
  current_mA = 0;
  mui_exit_button_var = 0;
  current_field_index = 0; // Reset field index for the next run
  
  mui.gotoForm(1, 0); 
  redraw_menu = 1;
}

