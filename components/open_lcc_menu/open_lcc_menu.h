#pragma once

#include <functional>
#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/display/display.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace open_lcc_menu {

// Menu for the two buttons of the Open LCC display.
//
// Home screen: short -/+ and long - (after home_long_minus_time) run the configured automations,
// long + opens the menu.
// Menu: short -/+ move, long + opens/confirms, long - goes back. Numbers are edited with -/+ and
// saved with long +. Without input the menu closes after the timeout, unsaved values are discarded.
class OpenLCCMenu : public Component {
 public:
  enum ItemType { ITEM_NUMBER, ITEM_SWITCH, ITEM_TEXT, ITEM_ACTION };

  struct Item {
    ItemType type;
    std::string label;
    number::Number *number{nullptr};
    float step{0};
    std::string format;
    switch_::Switch *sw{nullptr};
    std::function<std::string()> text;
    Trigger<> *action{nullptr};
    bool confirm{false};
  };

  struct Page {
    std::string title;
    std::vector<Item> items;
  };

  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_buttons(binary_sensor::BinarySensor *minus, binary_sensor::BinarySensor *plus) {
    this->minus_ = minus;
    this->plus_ = plus;
  }
  void set_fonts(display::BaseFont *font, display::BaseFont *small_font, display::BaseFont *value_font) {
    this->font_ = font;
    this->small_font_ = small_font;
    this->value_font_ = value_font;
  }
  // Visible area: the display is larger than the opening in the machine's front panel
  void set_margins(int left, int top, int right, int bottom) {
    this->margin_left_ = left;
    this->margin_top_ = top;
    this->margin_right_ = right;
    this->margin_bottom_ = bottom;
  }
  void set_timeout(uint32_t ms) { this->timeout_ms_ = ms; }
  void set_long_press_time(uint32_t ms) { this->long_press_ms_ = ms; }
  void set_home_long_minus_time(uint32_t ms) { this->home_long_minus_ms_ = ms; }

  Trigger<> *get_home_minus_trigger() { return &this->home_minus_; }
  Trigger<> *get_home_plus_trigger() { return &this->home_plus_; }
  Trigger<> *get_home_long_minus_trigger() { return &this->home_long_minus_; }

  size_t add_page(const std::string &title) {
    this->pages_.push_back(Page{title, {}});
    return this->pages_.size() - 1;
  }
  void add_number_item(size_t page, const std::string &label, number::Number *number, float step,
                       const std::string &format);
  void add_switch_item(size_t page, const std::string &label, switch_::Switch *sw);
  void add_text_item(size_t page, const std::string &label, std::function<std::string()> text);
  void add_action_item(size_t page, const std::string &label, Trigger<> *action, bool confirm);

  // True while the menu (not the home screen) is shown
  bool is_active() const { return this->level_ != LEVEL_HOME; }
  void open();
  void close();
  // Draws the menu over the whole display, call from a display lambda while is_active()
  void draw(display::Display &it);

 protected:
  enum Level { LEVEL_HOME, LEVEL_PAGES, LEVEL_ITEMS, LEVEL_EDIT, LEVEL_CONFIRM };
  enum Button { BUTTON_MINUS = 0, BUTTON_PLUS = 1 };

  struct ButtonState {
    bool pressed{false};
    bool long_fired{false};
    uint32_t pressed_at{0};
  };

  void on_button_(Button button, bool pressed);
  void on_short_(Button button);
  void on_long_(Button button);
  Item *current_item_();
  std::string value_text_(const Item &item, float value) const;
  std::string item_value_text_(const Item &item) const;
  void draw_list_(display::Display &it, int x, int y, int width, int height, const std::vector<std::string> &labels,
                  const std::vector<std::string> &values, int selected);

  binary_sensor::BinarySensor *minus_{nullptr};
  binary_sensor::BinarySensor *plus_{nullptr};
  display::BaseFont *font_{nullptr};
  display::BaseFont *small_font_{nullptr};
  display::BaseFont *value_font_{nullptr};

  int margin_left_{36};
  int margin_top_{20};
  int margin_right_{30};
  int margin_bottom_{16};

  uint32_t timeout_ms_{15000};
  uint32_t long_press_ms_{1000};
  uint32_t home_long_minus_ms_{3000};

  Trigger<> home_minus_;
  Trigger<> home_plus_;
  Trigger<> home_long_minus_;

  std::vector<Page> pages_;
  Level level_{LEVEL_HOME};
  int page_index_{0};
  int item_index_{0};
  float edit_value_{0};
  uint32_t last_input_{0};
  ButtonState buttons_[2];
};

}  // namespace open_lcc_menu
}  // namespace esphome
