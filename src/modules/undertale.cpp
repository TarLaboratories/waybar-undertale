#include "modules/undertale.hpp"

#include <fcntl.h>
#include <spdlog/spdlog.h>

#include <cctype>
#include <stdexcept>
#include <utility>

#include "cairo.h"
#include "gdkmm/pixbuf.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "glibmm/fileutils.h"
#include "gtk/gtk.h"
#include "gtkmm/widget.h"

namespace waybar::modules::undertale {
Player::Player(GameState* state) : ISizeableTextured(state->textures, "soul.png") {
  width = height = (state->get_width() / 16) - 3;
  hp = 20;
  iframes = 0;
  pos.x = state->arena->get_width() / 2 - width / 2;
  pos.y = state->arena->get_height() / 2 - height / 2;
  this->state = state;
}

IPos Player::get_screen_coords() const {
  if (free) return pos + IPos{state->x, state->y};
  if (state->mode == ARENA) return pos + state->arena->pos;
  return pos;
}

void Player::render(cairo_t* cr) const {
  if (!Input::is_focused()) return;
  if (state->mode == DEATH) {
    if (iframes > 0)
      ISizeableTextured::render(cr);
    else
      return;
  }
  if (iframes % 8 < 4 && state->mode != DIALOGUE) ISizeableTextured::render(cr);
}

void Player::damage(int damage) {
  if (iframes > 0) return;
  iframes = 20;
  hp -= damage;
}

void Player::handle_input() {
  if (free) {
    IPos prev_pos = pos;
    if (state->input->left.held) pos.x -= STEP * 4;
    if (state->input->right.held) pos.x += STEP * 4;
    if (state->input->up.held) pos.y -= STEP * 4;
    if (state->input->down.held) pos.y += STEP * 4;
    if (state->input->z.pressed) system("ydotool click 0x40");
    if (state->input->z.released) system("ydotool click 0x80");
    if (state->input->x.pressed) autoclick = !autoclick;
    if (autoclick) system("ydotool click 0xC0");
    if (prev_pos != pos) {
      IPos pos = get_screen_coords() / 2;
      system(("ydotool mousemove -a -- " + std::to_string(pos.x) + " " + std::to_string(pos.y))
                 .c_str());
    }
    return;
  }
  switch (state->mode) {
    case MODE_SELECT:
      if (state->input->x.pressed) Input::unfocus();
      if (state->input->left.pressed) {
        selected = (selected + 3) % 4;
      }
      if (state->input->right.pressed) selected = (selected + 1) % 4;
      pos = state->buttons[selected]->pos + IPos{2, 4};
      break;
    case ACTION_SELECT:
      if (state->input->x.pressed) {
        state->mode = MODE_SELECT;
      }
      if (state->input->left.pressed) {
        selected = (selected + state->buttons.size() - 5) % (state->buttons.size() - 4);
      }
      if (state->input->right.pressed) selected = (selected + 1) % (state->buttons.size() - 4);
      if (state->input->down.pressed) selected = (selected + 2) % (state->buttons.size() - 4);
      if (state->input->up.pressed) {
        selected = (selected + state->buttons.size() - 6) % (state->buttons.size() - 4);
      }
      pos = state->buttons[selected + 4]->pos +
            IPos{2, 7 - state->buttons[selected + 4]->get_height()};
      break;
    case ARENA:
      if (state->input->left.held) pos.x -= STEP;
      if (state->input->right.held) pos.x += STEP;
      pos.x = std::max(pos.x, 1);
      pos.x = std::min(pos.x, state->arena->get_width() - get_width() - 1);
      if (state->input->up.held) pos.y -= STEP;
      if (state->input->down.held) pos.y += STEP;
      pos.y = std::max(pos.y, 1);
      pos.y = std::min(pos.y, state->arena->get_height() - get_height() - 1);
      break;
    case DIALOGUE:
      break;
    case DEATH:
      break;
  }
}

void Player::tick() {
  if (iframes > 0) {
    iframes--;
  }
  handle_input();
  if (state->mode == DEATH) {
    if (iframes == 0) {
      iframes = -1;
      state->arena->dialogue.emplace("It cannot end like this!\nTAR! Stay determined!");
    }
  }
  if (hp <= 0 && state->mode != DEATH) {
    hp = 0;
    pos = get_screen_coords();
    iframes = 80;
    state->mode = DEATH;
    state->clear_buttons();
  } else if (state->mode != DEATH) {
    texture_name = "soul.png";
  } else if (state->mode == DEATH && iframes == 60) {
    texture_name = "heartbreak.png";
  }
}

Arena::Arena(GameState* state) : state(state) {
  width = state->get_width() / 2;
  height = width / 2;
  pos.x = (state->get_width() - width) / 2;
  pos.y = state->get_height() - height - 34;
}

void Arena::tick() {
  if (state->mode == ACTION_SELECT || state->mode == DIALOGUE) {
    width = state->get_width() * 3 / 4;
  } else if (state->mode == DEATH) {
    width = state->get_width() * 15 / 16;
  } else {
    width = state->get_width() / 2;
  }
  pos.x = (state->get_width() - width) / 2;

  if (state->mode == ARENA) {
    attack_ticks--;
    if (attack_ticks == 0) {
      state->mode = MODE_SELECT;
      return;
    }
    for (Attack* attack : attacks) {
      if (!attack->is_deleted()) attack->tick();
    }
    for (auto it = attacks.begin(); it != attacks.end();) {
      if ((*it)->is_deleted()) {
        it = attacks.erase(it);
      } else
        ++it;
    }
  } else
    attacks.clear();

  if (state->mode == DIALOGUE || state->mode == DEATH) {
    if (cur_text.empty()) cur_text.emplace_back("");
    if (dialogue.empty()) {
      if (state->mode == DEATH) {
        if (!state->player->is_invincible()) {
          if (state->get_height() <= 30) {
            if (state->get_width() == 0) {
              state->set_width(200);
              state->mode = MODE_SELECT;
              state->player->damage(-20);
            }
            state->set_width(state->get_width() - 5);
          } else
            state->set_height(state->get_height() - 1);
        }
        return;
      }
      state->mode = ARENA;
      attack_ticks = 200;
      cur_text.clear();
      cur_char = 0;
    } else {
      if (cur_char < dialogue.front().size()) {
        do {
          if (state->mode == DEATH && state->tick % 3 != 0) break;
          cur_text.back() += dialogue.front()[cur_char++];
          if (cur_text.back().size() == 1 && cur_text.back()[0] == ' ') {
            cur_text.back() = "";
          }
          if ((cur_text.back().size() + 1) * 8 > width && state->mode != DEATH) {
            cur_text.emplace_back("");
          }
          if (!cur_text.back().empty() && cur_text.back().back() == '\n') {
            cur_text.back().pop_back();
            cur_text.emplace_back("");
          }
        } while (state->input->x.pressed && cur_char < dialogue.front().size());
      } else {
        if (state->input->z.pressed) {
          dialogue.pop();
          cur_text.clear();
          cur_char = 0;
        }
      }
    }
  }
}

void Arena::render(cairo_t* cr) const {
  if (state->mode == DEATH && state->player->is_invincible()) return;
  cairo_set_source_rgb(cr, 1, 1, 1);
  if (state->mode != DEATH) {
    cairo_rectangle(cr, pos.x, pos.y, width, height);
    cairo_stroke(cr);
  }

  for (const Attack* attack : attacks) {
    attack->render(cr);
  }

  IPos pos = this->pos + IPos{2, 2};
  if (state->mode == DEATH) pos.x = 2;
  for (const std::string& line : cur_text) {
    state->textures->render_text(cr, line, pos.x + 2, pos.y + 13, 16);
    pos.y += 16;
  }

  if (state->mode != DEATH) {
    state->textures->render_text(cr, "HP", 2, 18, 16);
    state->textures->render_text(cr, std::to_string(state->player->get_hp()), 2, 34, 16);
  }
}

Bullet::Bullet(GameState* state, int radius, IPos pos, IPos vec)
    : ISizeablePositioned(pos, radius, radius), state(state), radius(radius), vec(vec) {}

Button::Button(GameState* state, const std::string& normal_texture, std::string selected_texture,
               IPos pos, int width, int height)
    : ISizeableTextured(state->textures, normal_texture),
      state(state),
      normal_texture(normal_texture),
      selected_texture(std::move(selected_texture)),
      selected(false) {
  this->pos = pos;
  this->width = width;
  this->height = height;
}

void Button::tick() {
  selected = intersects(state->player) && Input::is_focused();
  if (selected) {
    texture_name = selected_texture;
    if (state->input->z.pressed) this->on_select();
  } else {
    texture_name = normal_texture;
  }
}

void Button::render(cairo_t* cr) const {
  if (state->mode != DEATH) ISizeableTextured::render(cr);
}

class TextButton : public Button {
 protected:
  std::string text;

 public:
  TextButton(GameState* state, const std::string& text, IPos pos, int font_size)
      : Button(state, "", "", pos, text.size() * font_size / 2, font_size), text(text) {}

  void render(cairo_t* cr) const override {
    if (state->mode == ACTION_SELECT) {
      state->textures->render_text(cr, text, pos.x + state->player->get_width() + 2, pos.y,
                                   get_height());
    }
  }

  void tick() override {
    if (state->mode == ACTION_SELECT) {
      Button::tick();
    }
  }
};

class ActionButton : public TextButton {
 public:
  ActionButton(GameState* state, const std::string& text) : TextButton(state, text, {0, 0}, 16) {}
};

class CheckButton : public ActionButton {
 public:
  CheckButton(GameState* state) : ActionButton(state, "* Check") {}

  void on_select() override {
    std::string pkgs = util::command::exec("yay -Q | wc -l", "").out;
    std::string bat = util::command::exec("acpi | grep -Eo '[0-9]*%'", "").out;
    state->arena->dialogue.emplace("Laptop - SYS artix PKG " + pkgs + " BAT " + bat);
    state->arena->dialogue.emplace("* I use arch btw");
    state->mode = DIALOGUE;
  }
};

class TalkButton : public ActionButton {
 public:
  TalkButton(GameState* state) : ActionButton(state, "* Talk") {}

  void on_select() override {
    state->arena->dialogue.emplace("You said you use  arch btw");
    state->arena->dialogue.emplace("Laptop's pipewire broke, so it can't speak");
    state->arena->dialogue.emplace("This is awkward...");
    state->mode = DIALOGUE;
  }
};

class CommandItemButton : public ActionButton {
 protected:
  std::string name;
  std::string command;

 public:
  CommandItemButton(GameState* state, const std::string& name, std::string command)
      : ActionButton(state, "* " + name), name(name), command(std::move(command)) {}

  void on_select() override {
    system(command.c_str());
    state->mode = DIALOGUE;
    std::string uppercase = "";
    for (char i : name) uppercase += std::toupper(i);
    state->arena->dialogue.emplace(fmt::format("You used {}!", uppercase));
  }
};

class TerminalButton : public CommandItemButton {
 public:
  TerminalButton(GameState* state)
      : CommandItemButton(state, "Cat", "kitty --class=float --detach") {}

  void on_select() override {
    int heal = 5 + (rand() % 5);
    CommandItemButton::on_select();
    state->arena->dialogue.emplace(fmt::format("The kitty healed you {} HP!", heal));
    state->player->damage(-heal);
  }
};

class BrowserButton : public CommandItemButton {
 public:
  BrowserButton(GameState* state) : CommandItemButton(state, "Wolf", "librewolf --browser") {}

  void on_select() override {
    int heal = (rand() % 10) + 5;
    int dmg = rand() % 20;
    CommandItemButton::on_select();
    state->arena->dialogue.emplace(
        fmt::format("You summoned a (libre) wolf! It healed you {} HP!", heal));
    state->arena->dialogue.emplace(fmt::format("And then bit you! (-{} HP)", dmg));
    state->player->damage(dmg - heal);
  }
};

class IdeaButton : public CommandItemButton {
 public:
  IdeaButton(GameState* state)
      : CommandItemButton(state, "Idea", "/opt/intellij-idea-community-edition/bin/idea") {}

  void on_select() override {
    CommandItemButton::on_select();
    state->arena->dialogue.pop();
    state->arena->dialogue.emplace("You had an idea!");
    state->arena->dialogue.emplace(
        "And decided that you should actually do something important...");
  }
};

class SteamButton : public CommandItemButton {
 public:
  SteamButton(GameState* state) : CommandItemButton(state, "Steam", "steam -console") {}

  void on_select() override {
    CommandItemButton::on_select();
    state->arena->dialogue.emplace("It was very effective...");
    state->arena->dialogue.emplace("...in hogging all of the system resources.");
  }
};

class PowerButton : public CommandItemButton {
 public:
  PowerButton(GameState* state) : CommandItemButton(state, "Sleep", "suspend") {}

  void on_select() override { state->arena->dialogue.emplace("Oh, you woke up!"); }
};

class AppsButton : public ActionButton {
 public:
  AppsButton(GameState* state) : ActionButton(state, "* ???") {}

  void on_select() override { system("rofi -show drun"); }
};

class DevicesButton : public ActionButton {
 public:
  DevicesButton(GameState* state) : ActionButton(state, "* PCIe") {}

  void on_select() override {
    system(
        "sudo disable-pci $(lspci | rofi -p 'Disable device' -dmenu -case-smart | awk '{print "
        "$1}')");
    state->mode = DIALOGUE;
    state->arena->dialogue.emplace("PCIe device has   been disabled");
  }
};

class RescanButton : public ActionButton {
 public:
  RescanButton(GameState* state) : ActionButton(state, "* PCIe") {}

  void on_select() override {
    system("sudo rescan-pci");
    state->mode = DIALOGUE;
    state->arena->dialogue.emplace("PCIe devices have been restored");
  }
};

class SpareButton : public ActionButton {
 public:
  SpareButton(GameState* state) : ActionButton(state, "* Spare") {}

  void on_select() override {
    state->mode = DIALOGUE;
    state->arena->dialogue.emplace("You tried to spare Laptop...");
    state->arena->dialogue.emplace("That doesn't make sense though...");
    state->arena->dialogue.emplace("You were using it for like 5 years...");
    state->arena->dialogue.emplace("If you want to spare it, maybe try shutting it down.");
  }
};

class FleeButton : public ActionButton {
 private:
  int count = 0;

 public:
  FleeButton(GameState* state) : ActionButton(state, "* Flee") {}

  void on_select() override {
    state->mode = MODE_SELECT;
    if (state->player->free) {
      text = "* Re";
      for (int i = 1; i < count; i++) {
        text += "re";
      }
      text += "flee";
    } else {
      text = "* Unflee";
      count++;
    }
    state->player->free = !state->player->free;
  }
};

class FightButton : public Button {
 public:
  FightButton(GameState* state, IPos pos, int width, int height)
      : Button(state, "fight.png", "fight_selected.png", pos, width, height) {}

  void on_select() override {
    state->clear_buttons();
    state->buttons.push_back(new DevicesButton(state));
    state->mode = ACTION_SELECT;
    state->player->selected = 0;
  }
};

class ActButton : public Button {
 public:
  ActButton(GameState* state, IPos pos, int width, int height)
      : Button(state, "act.png", "act_selected.png", pos, width, height) {}

  void on_select() override {
    state->clear_buttons();
    state->buttons.push_back(new CheckButton(state));
    state->buttons.push_back(new TalkButton(state));
    state->mode = ACTION_SELECT;
    state->player->selected = 0;
  }
};

class ItemButton : public Button {
 public:
  ItemButton(GameState* state, IPos pos, int width, int height)
      : Button(state, "item.png", "item_selected.png", pos, width, height) {}

  void on_select() override {
    state->clear_buttons();
    state->buttons.push_back(new TerminalButton(state));
    state->buttons.push_back(new BrowserButton(state));
    state->buttons.push_back(new IdeaButton(state));
    state->buttons.push_back(new SteamButton(state));
    state->buttons.push_back(new PowerButton(state));
    state->buttons.push_back(new AppsButton(state));
    state->mode = ACTION_SELECT;
    state->player->selected = 0;
  }
};

class MercyButton : public Button {
 public:
  MercyButton(GameState* state, IPos pos, int width, int height)
      : Button(state, "mercy.png", "mercy_selected.png", pos, width, height) {}

  void on_select() override {
    state->clear_buttons();
    state->buttons.push_back(new SpareButton(state));
    state->buttons.push_back(new FleeButton(state));
    state->buttons.push_back(new RescanButton(state));
    state->mode = ACTION_SELECT;
    state->player->selected = 0;
  }
};

void Textures::render_text(cairo_t* cr, const std::string& text, double x, double y,
                           int font_size) const {
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_move_to(cr, x, y);
  cairo_set_font_face(cr, font);
  cairo_set_font_size(cr, font_size);
  cairo_show_text(cr, text.c_str());
  cairo_move_to(cr, 0, 0);
}
}  // namespace waybar::modules::undertale

waybar::modules::undertale::Undertale::Undertale(const std::string& id, const Json::Value& config)
    : AModule(config, "undertale", id), window(), box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.pack_start(area_);
  // window.get_window()->input_shape_combine_region(Cairo::Region::create(), 0, 0);
  window.set_size_request(1920, 1080);
  window.set_title("waybar-undertale");
  window.set_name("undertale-overlay");
  GtkCssProvider* css_provider = gtk_css_provider_new();
  GError* err;
  gtk_css_provider_load_from_path(css_provider, "~/.config/waybar/style.css", &err);

  gtk_style_context_add_provider_for_screen(window.get_screen()->gobj(),
                                            (GtkStyleProvider*)css_provider,
                                            GTK_STYLE_PROVIDER_PRIORITY_USER);
  window.add(state_.overlay);
  window.show_all();
  box_.set_name("undertale");
  state_.input = new Input("/tmp/undertale_input");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);
  state_.set_width(config["width"].asInt());
  if (state_.get_width() == 0) {
    state_.set_width(200);
  }
  state_.set_height(state_.get_width() / 2);
  state_.textures = new Textures(config["path"].asString());
  state_.overlay.set_size_request(1920, 1080);
  state_.overlay.set_opacity(1);
  g_signal_connect(area_.gobj(), "draw", G_CALLBACK(this->draw_callback), G_OBJECT(&state_));
  g_signal_connect(state_.overlay.gobj(), "draw", G_CALLBACK(this->draw_overlay_callback),
                   G_OBJECT(&state_));

  state_.arena = new Arena(&state_);
  state_.player = new Player(&state_);

  IPos button_pos = {4, state_.arena->pos.y + state_.arena->get_height() + 8};
  int button_width = (state_.get_width() / 4) - 8;
  int button_height = button_width / 2;

  state_.buttons[0] = new FightButton(&state_, button_pos, button_width, button_height);
  button_pos = button_pos + IPos{state_.get_width() / 4, 0};
  state_.buttons[1] = new ActButton(&state_, button_pos, button_width, button_height);
  button_pos = button_pos + IPos{state_.get_width() / 4, 0};
  state_.buttons[2] = new ItemButton(&state_, button_pos, button_width, button_height);
  button_pos = button_pos + IPos{state_.get_width() / 4, 0};
  state_.buttons[3] = new MercyButton(&state_, button_pos, button_width, button_height);

  dp.emit();

  const auto once = std::chrono::milliseconds::max();
  if (!config_.isMember("interval") || config_["interval"].isNull() ||
      config_["interval"] == "once") {
    interval_ = once;
  } else if (config_["interval"].isNumeric()) {
    const auto interval_seconds = config_["interval"].asDouble();
    if (interval_seconds <= 0) {
      interval_ = once;
    } else {
      interval_ =
          std::chrono::milliseconds(std::max(1L,  // Minimum 1ms due to millisecond precision
                                             static_cast<long>(interval_seconds * 1000)));
    }
  } else {
    interval_ = once;
  }

  delayWorker();
}

waybar::modules::undertale::Undertale::~Undertale() { delete state_.input; };

void waybar::modules::undertale::Undertale::delayWorker() {
  thread_ = [this] {
    if (!pause) {
      dp.emit();
      thread_.sleep_for(interval_);
    }
    pause = !Input::is_focused();
    if (pause) thread_.sleep();
  };
}

void waybar::modules::undertale::Undertale::refresh(int sig) {
  if (config_["signal"].isInt() && sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
}

void waybar::modules::undertale::Textures::render(cairo_t* cr, const std::string& name, double x,
                                                  double y, int max_width = -1,
                                                  int max_height = -1) {
  GdkPixbuf* pix;
  if (loadedTextures.contains(name)) {
    pix = loadedTextures.at(name);
  } else {
    GError* err = nullptr;
    char* path = new char[asset_path.length() + name.length() + 1];
    std::strcpy(path, (asset_path + name).c_str());
    pix = gdk_pixbuf_new_from_file_at_size(path, max_width, max_height, &err);
    if (err != nullptr) {
      auto error =
          std::runtime_error("Error loading asset '" + asset_path + name + "': " + err->message);
      g_error_free(err);
      throw error;
    }
    loadedTextures[name] = pix;
  }
  gdk_cairo_set_source_pixbuf(cr, pix, x, y);
  cairo_rectangle(cr, x, y, x + gdk_pixbuf_get_width(pix), y + gdk_pixbuf_get_height(pix));
  cairo_fill(cr);
}

gboolean waybar::modules::undertale::Undertale::draw_callback(GtkWidget* widget, cairo_t* cr,
                                                              gpointer data) {
  auto* state = (GameState*)data;
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_rectangle(cr, 0, 0, state->get_width(), state->get_height());
  cairo_fill(cr);
  if (state->mode != MODE_SELECT) {
    state->arena->render(cr);
  }
  for (Button* button : state->buttons) {
    button->render(cr);
  }
  if (!state->player->free) state->player->render(cr);
  return FALSE;
}

gboolean waybar::modules::undertale::Undertale::draw_overlay_callback(GtkWidget* widget,
                                                                      cairo_t* cr, gpointer data) {
  auto* state = (GameState*)data;
  Player* player = state->player;
  if (player->free) {
    player->render(cr);
  }
  return FALSE;
}

auto waybar::modules::undertale::Undertale::update() -> void {
  state_.tick++;
  state_.input->read();
  gtk_widget_translate_coordinates(&area_.gobj()->widget,
                                   gtk_widget_get_toplevel(&area_.gobj()->widget), 0, 0, &state_.x,
                                   &state_.y);
  if (state_.mode != DEATH) {
    if (state_.mode == MODE_SELECT) {
      state_.set_height(28);
    } else {
      state_.set_height(state_.get_width() / 2);
    }
  }
  area_.set_size_request(state_.get_width(), state_.get_height());
  if (rand() % 4 == 0 && state_.mode == ARENA) {
    state_.arena->attacks.push_back(
        new Bullet(&state_, 5, {50, 50}, {(rand() % 6) - 3, -(rand() % 3)}));
  }
  state_.player->tick();
  int ind = 0;
  for (Button* button : state_.buttons) {
    if (ind < 4) {
      if (state_.mode != MODE_SELECT)
        button->pos.y = state_.arena->pos.y + state_.arena->get_height() + 8;
      else
        button->pos.y = (state_.get_height() - button->get_height()) / 2;
    } else {
      button->pos.y =
          state_.arena->pos.y + 2 + (ind - 4) / 2 * button->get_height() + button->get_height();
      if (ind % 2 == 0)
        button->pos.x = state_.arena->pos.x + 2;
      else
        button->pos.x = state_.arena->pos.x + state_.arena->get_width() / 2;
    }
    button->tick();
    ind++;
  }
  if (state_.mode != MODE_SELECT) state_.arena->tick();
  if (state_.mode != DEATH) {
    if (state_.mode == MODE_SELECT) {
      state_.set_height(28);
    } else {
      state_.set_height(state_.get_width() / 2);
    }
  }
  area_.set_size_request(state_.get_width(), state_.get_height());
  area_.queue_draw();
  state_.overlay.queue_draw();
  box_.get_style_context()->remove_class("empty");

  AModule::update();
}
