#include "open_lcc_menu.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace open_lcc_menu {

static const char *const TAG = "open_lcc_menu";

static const Color COLOR_BACKGROUND(0, 0, 0);
static const Color COLOR_TEXT(255, 255, 255);
static const Color COLOR_DIM(128, 128, 128);
static const Color COLOR_BAR(40, 40, 40);
static const Color COLOR_SELECTED_BG(255, 255, 255);
static const Color COLOR_SELECTED_TEXT(0, 0, 0);
static const Color COLOR_WARNING(255, 200, 0);

static constexpr int TITLE_HEIGHT = 20;
static constexpr int FOOTER_HEIGHT = 16;
static constexpr int MIN_ROW_HEIGHT = 28;
static constexpr int PADDING = 4;

void OpenLCCMenu::setup() {
  if (this->minus_ != nullptr)
    this->minus_->add_on_state_callback([this](bool state) { this->on_button_(BUTTON_MINUS, state); });
  if (this->plus_ != nullptr)
    this->plus_->add_on_state_callback([this](bool state) { this->on_button_(BUTTON_PLUS, state); });
}

void OpenLCCMenu::add_number_item(size_t page, const std::string &label, number::Number *number, float step,
                                  const std::string &format) {
  Item item{};
  item.type = ITEM_NUMBER;
  item.label = label;
  item.number = number;
  item.step = step;
  item.format = format;
  this->pages_[page].items.push_back(item);
}

void OpenLCCMenu::add_switch_item(size_t page, const std::string &label, switch_::Switch *sw) {
  Item item{};
  item.type = ITEM_SWITCH;
  item.label = label;
  item.sw = sw;
  this->pages_[page].items.push_back(item);
}

void OpenLCCMenu::add_text_item(size_t page, const std::string &label, std::function<std::string()> text) {
  Item item{};
  item.type = ITEM_TEXT;
  item.label = label;
  item.text = std::move(text);
  this->pages_[page].items.push_back(item);
}

void OpenLCCMenu::add_action_item(size_t page, const std::string &label, Trigger<> *action, bool confirm) {
  Item item{};
  item.type = ITEM_ACTION;
  item.label = label;
  item.action = action;
  item.confirm = confirm;
  this->pages_[page].items.push_back(item);
}

void OpenLCCMenu::open() {
  if (this->pages_.empty())
    return;
  this->level_ = LEVEL_PAGES;
  this->page_index_ = 0;
  this->item_index_ = 0;
  this->last_input_ = millis();
}

void OpenLCCMenu::close() { this->level_ = LEVEL_HOME; }

void OpenLCCMenu::loop() {
  uint32_t now = millis();

  for (int b = 0; b < 2; b++) {
    ButtonState &state = this->buttons_[b];
    if (!state.pressed || state.long_fired)
      continue;
    uint32_t threshold = this->long_press_ms_;
    if (this->level_ == LEVEL_HOME && b == BUTTON_MINUS)
      threshold = this->home_long_minus_ms_;
    if (now - state.pressed_at >= threshold) {
      state.long_fired = true;
      this->last_input_ = now;
      this->on_long_(static_cast<Button>(b));
    }
  }

  if (this->level_ != LEVEL_HOME && now - this->last_input_ > this->timeout_ms_) {
    ESP_LOGD(TAG, "Menu timeout");
    this->close();
  }
}

void OpenLCCMenu::on_button_(Button button, bool pressed) {
  ButtonState &state = this->buttons_[button];
  uint32_t now = millis();
  this->last_input_ = now;

  if (pressed) {
    state.pressed = true;
    state.long_fired = false;
    state.pressed_at = now;
    return;
  }

  if (state.pressed && !state.long_fired)
    this->on_short_(button);
  state.pressed = false;
}

OpenLCCMenu::Item *OpenLCCMenu::current_item_() {
  if (this->page_index_ >= (int) this->pages_.size())
    return nullptr;
  auto &items = this->pages_[this->page_index_].items;
  if (this->item_index_ >= (int) items.size())
    return nullptr;
  return &items[this->item_index_];
}

static int wrap(int value, int count) {
  if (count <= 0)
    return 0;
  return ((value % count) + count) % count;
}

void OpenLCCMenu::on_short_(Button button) {
  int direction = button == BUTTON_PLUS ? 1 : -1;

  switch (this->level_) {
    case LEVEL_HOME:
      if (button == BUTTON_PLUS)
        this->home_plus_.trigger();
      else
        this->home_minus_.trigger();
      break;
    case LEVEL_PAGES:
      this->page_index_ = wrap(this->page_index_ + direction, this->pages_.size());
      break;
    case LEVEL_ITEMS:
      this->item_index_ = wrap(this->item_index_ + direction, this->pages_[this->page_index_].items.size());
      break;
    case LEVEL_EDIT: {
      Item *item = this->current_item_();
      if (item == nullptr || item->number == nullptr)
        break;
      auto &traits = item->number->traits;
      float step = item->step > 0 ? item->step : traits.get_step();
      float value = this->edit_value_ + direction * step;
      // Round to the step to avoid float drift
      if (step > 0)
        value = std::round(value / step) * step;
      this->edit_value_ = clamp(value, traits.get_min_value(), traits.get_max_value());
      break;
    }
    case LEVEL_CONFIRM:
      break;
  }
}

void OpenLCCMenu::on_long_(Button button) {
  switch (this->level_) {
    case LEVEL_HOME:
      if (button == BUTTON_PLUS)
        this->open();
      else
        this->home_long_minus_.trigger();
      break;

    case LEVEL_PAGES:
      if (button == BUTTON_MINUS) {
        this->close();
      } else if (!this->pages_[this->page_index_].items.empty()) {
        this->level_ = LEVEL_ITEMS;
        this->item_index_ = 0;
      }
      break;

    case LEVEL_ITEMS: {
      if (button == BUTTON_MINUS) {
        this->level_ = LEVEL_PAGES;
        break;
      }
      Item *item = this->current_item_();
      if (item == nullptr)
        break;
      switch (item->type) {
        case ITEM_NUMBER:
          this->edit_value_ = item->number->has_state() && !std::isnan(item->number->state)
                                  ? item->number->state
                                  : item->number->traits.get_min_value();
          this->level_ = LEVEL_EDIT;
          break;
        case ITEM_SWITCH:
          item->sw->toggle();
          break;
        case ITEM_ACTION:
          if (item->confirm) {
            this->level_ = LEVEL_CONFIRM;
          } else {
            item->action->trigger();
          }
          break;
        case ITEM_TEXT:
          break;
      }
      break;
    }

    case LEVEL_EDIT: {
      Item *item = this->current_item_();
      if (button == BUTTON_PLUS && item != nullptr && item->number != nullptr) {
        auto call = item->number->make_call();
        call.set_value(this->edit_value_);
        call.perform();
      }
      this->level_ = LEVEL_ITEMS;
      break;
    }

    case LEVEL_CONFIRM: {
      Item *item = this->current_item_();
      this->level_ = LEVEL_ITEMS;
      if (button == BUTTON_PLUS && item != nullptr && item->action != nullptr)
        item->action->trigger();
      break;
    }
  }
}

std::string OpenLCCMenu::value_text_(const Item &item, float value) const {
  if (std::isnan(value))
    return "-";
  std::string format = item.format.empty() ? "%.1f" : item.format;
  char buf[32];
  snprintf(buf, sizeof(buf), format.c_str(), value);
  return buf;
}

std::string OpenLCCMenu::item_value_text_(const Item &item) const {
  switch (item.type) {
    case ITEM_NUMBER:
      return this->value_text_(item, item.number->has_state() ? item.number->state : NAN);
    case ITEM_SWITCH:
      return item.sw->state ? "an" : "aus";
    case ITEM_TEXT:
      return item.text ? item.text() : "";
    case ITEM_ACTION:
      return ">";
  }
  return "";
}

void OpenLCCMenu::draw_list_(display::Display &it, int x, int y, int width, int height,
                             const std::vector<std::string> &labels, const std::vector<std::string> &values,
                             int selected) {
  int visible = height / MIN_ROW_HEIGHT;
  if (visible < 1)
    visible = 1;
  int row_height = height / visible;

  int first = 0;
  if (selected >= visible)
    first = selected - visible + 1;

  for (int row = 0; row < visible && first + row < (int) labels.size(); row++) {
    int index = first + row;
    int row_y = y + row * row_height;
    bool is_selected = index == selected;
    Color fg = is_selected ? COLOR_SELECTED_TEXT : COLOR_TEXT;
    if (is_selected)
      it.filled_rectangle(x, row_y, width, row_height, COLOR_SELECTED_BG);
    int center_y = row_y + row_height / 2;
    it.print(x + PADDING, center_y, this->font_, fg, display::TextAlign::CENTER_LEFT, labels[index].c_str());
    if (index < (int) values.size() && !values[index].empty())
      it.print(x + width - PADDING, center_y, this->font_, is_selected ? fg : COLOR_DIM,
               display::TextAlign::CENTER_RIGHT, values[index].c_str());
  }

  // Scroll indicators at the right edge
  int arrow_x = x + width - 6;
  if (first > 0)
    it.filled_triangle(arrow_x - 4, y + 5, arrow_x + 4, y + 5, arrow_x, y + 1, COLOR_DIM);
  if (first + visible < (int) labels.size())
    it.filled_triangle(arrow_x - 4, y + height - 5, arrow_x + 4, y + height - 5, arrow_x, y + height - 1, COLOR_DIM);
}

void OpenLCCMenu::draw(display::Display &it) {
  it.fill(COLOR_BACKGROUND);
  if (this->level_ == LEVEL_HOME)
    return;

  // Only draw inside the part of the display that is visible through the front panel
  int x = this->margin_left_;
  int y = this->margin_top_;
  int width = it.get_width() - this->margin_left_ - this->margin_right_;
  int height = it.get_height() - this->margin_top_ - this->margin_bottom_;
  int body_y = y + TITLE_HEIGHT;
  int body_height = height - TITLE_HEIGHT - FOOTER_HEIGHT;
  int body_center_y = body_y + body_height / 2;

  std::string title = "MENÜ";
  std::string position;
  std::string footer;

  switch (this->level_) {
    case LEVEL_HOME:
      return;

    case LEVEL_PAGES: {
      std::vector<std::string> labels;
      for (auto &page : this->pages_)
        labels.push_back(page.title);
      position = str_sprintf("%d/%d", this->page_index_ + 1, (int) this->pages_.size());
      footer = "-/+ wählen  L+ öffnen  L- zurück";
      this->draw_list_(it, x, body_y, width, body_height, labels, {}, this->page_index_);
      break;
    }

    case LEVEL_ITEMS: {
      Page &page = this->pages_[this->page_index_];
      title = page.title;
      std::vector<std::string> labels, values;
      for (auto &item : page.items) {
        labels.push_back(item.label);
        values.push_back(this->item_value_text_(item));
      }
      position = str_sprintf("%d/%d", this->item_index_ + 1, (int) page.items.size());
      Item *item = this->current_item_();
      if (item != nullptr && item->type == ITEM_TEXT) {
        footer = "-/+ wählen  L- zurück";
      } else if (item != nullptr && item->type == ITEM_SWITCH) {
        footer = "-/+ wählen  L+ umschalten";
      } else {
        footer = "-/+ wählen  L+ öffnen  L- zurück";
      }
      this->draw_list_(it, x, body_y, width, body_height, labels, values, this->item_index_);
      break;
    }

    case LEVEL_EDIT: {
      Item *item = this->current_item_();
      if (item == nullptr)
        return;
      title = item->label;
      it.print(x + width / 2, body_center_y, this->value_font_, COLOR_TEXT, display::TextAlign::CENTER,
               this->value_text_(*item, this->edit_value_).c_str());
      footer = "-/+ ändern  L+ OK  L- abbrechen";
      break;
    }

    case LEVEL_CONFIRM: {
      Item *item = this->current_item_();
      if (item == nullptr)
        return;
      title = "Bestätigen";
      it.print(x + width / 2, body_center_y, this->font_, COLOR_WARNING, display::TextAlign::CENTER,
               (item->label + "?").c_str());
      footer = "L+ ja   L- nein";
      break;
    }
  }

  // Title bar
  it.filled_rectangle(x, y, width, TITLE_HEIGHT, COLOR_BAR);
  it.print(x + PADDING, y + TITLE_HEIGHT / 2, this->small_font_, COLOR_TEXT, display::TextAlign::CENTER_LEFT,
           title.c_str());
  if (!position.empty())
    it.print(x + width - PADDING, y + TITLE_HEIGHT / 2, this->small_font_, COLOR_DIM,
             display::TextAlign::CENTER_RIGHT, position.c_str());

  // Footer
  it.print(x + width / 2, y + height - FOOTER_HEIGHT / 2, this->small_font_, COLOR_DIM, display::TextAlign::CENTER,
           footer.c_str());
}

}  // namespace open_lcc_menu
}  // namespace esphome
