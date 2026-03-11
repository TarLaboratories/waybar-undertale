#pragma once

#include <fmt/format.h>
#include <gtkmm/image.h>

#include <map>
#include <string>
#include <utility>

#include "AModule.hpp"
#include "cairo-ft.h"
#include "cairo.h"
#include "freetype/freetype.h"
#include "glib.h"
#include "gtkmm/box.h"
#include "util/command.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules::undertale {

enum MovementMode : std::uint8_t { MODE_SELECT, ACTION_SELECT, ARENA, DIALOGUE, DEATH };

enum Action : std::uint8_t { FIGHT, ACT, ITEM, MERCY };

struct KeyState {
  bool pressed = false;
  bool held = false;

  void handle(bool press) {
    held = press;
    if (press) pressed = true;
  }
};

class Input {
 public:
  KeyState left, right, up, down, z, x;

 private:
  std::string filename;
  std::string submap = "undertale";
  int fd;
  char *in = new char[2];

 public:
  Input(const std::string &filename, std::string submap = "undertale")
      : filename(filename), submap(std::move(submap)) {
    mkfifo(filename.c_str(), S_IWUSR | S_IRUSR | O_NONBLOCK);
    fd = open(filename.c_str(), O_NONBLOCK);
    bind();
  }

  ~Input() {
    unbind();
    close(fd);
    remove(filename.c_str());
  }

  static bool is_focused() {
    return util::command::exec("hyprctl submap", "").out == "undertale\n";
  }

 private:
  void bind() {
    system(("hyprctl keyword submap " + submap).c_str());
    system(("hyprctl keyword 'bindt , left, exec, echo -n lp > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindt , right, exec, echo -n rp > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindt , up, exec, echo -n up > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindt , down, exec, echo -n dp > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindt , Z, exec, echo -n zp > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindt , X, exec, echo -n xp > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindtr , left, exec, echo -n lr > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindtr , right, exec, echo -n rr > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindtr , up, exec, echo -n ur > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindtr , down, exec, echo -n dr > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindtr , Z, exec, echo -n zr > " + filename + "'").c_str());
    system(("hyprctl keyword 'bindtr , X, exec, echo -n xr > " + filename + "'").c_str());
    system("hyprctl keyword submap reset");
  }

  static void unbind() {
    system("hyprctl keyword unbind ,left");
    system("hyprctl keyword unbind ,right");
    system("hyprctl keyword unbind ,down");
    system("hyprctl keyword unbind ,up");
    system("hyprctl keyword unbind ,Z");
    system("hyprctl keyword unbind ,X");
  }

 public:
  void read() {
    int nread = ::read(fd, in, 2);
    left.pressed = false;
    right.pressed = false;
    up.pressed = false;
    down.pressed = false;
    z.pressed = false;
    x.pressed = false;
    if (nread > 0) {
      bool press = in[1] == 'p';
      switch (in[0]) {
        case 'l':
          left.handle(press);
          break;
        case 'r':
          right.handle(press);
          break;
        case 'u':
          up.handle(press);
          break;
        case 'd':
          down.handle(press);
          break;
        case 'z':
          z.handle(press);
          break;
        case 'x':
          x.handle(press);
          break;
      }
    }
  }
};

class Textures {
 private:
  std::map<std::string, GdkPixbuf *> loadedTextures;

 public:
  std::string asset_path;
  FT_Face ft_font;
  cairo_font_face_t *font;

  Textures() {
    FT_Library ft;
    FT_Init_FreeType(&ft);
    FT_New_Face(ft, "/home/taranov/.fonts/Determination.ttf", 0, &ft_font);
    font = cairo_ft_font_face_create_for_ft_face(ft_font, 0);
  }

  void render(cairo_t *cr, const std::string &name, double x, double y, int max_width,
              int max_height);

  void render_text(cairo_t *cr, const std::string &text, double x, double y, int font_size) const;
};

class IRenderable {
 public:
  virtual void render(cairo_t *cr) const {}
};

class IPos {
 public:
  int x, y;

  IPos() = default;

  IPos(int x_, int y_) : x(x_), y(y_) {}

  int sq_dist() const { return (x * x) + (y * y); }

  IPos operator+(const IPos &pos) const { return {x + pos.x, y + pos.y}; }

  IPos operator-(const IPos &pos) const { return {x - pos.x, y - pos.y}; }
};

class ISizeable {
 protected:
  int width;
  int height;

  ISizeable() = default;

  ISizeable(int width, int height) : width(width), height(height) {}

 public:
  int get_width() const { return width; }

  int get_height() const { return height; }

  IPos get_size() const { return {width, height}; }
};

class IPositioned {
 protected:
  IPositioned() = default;

  IPositioned(IPos pos) : pos(pos) {}

 public:
  IPos pos = {0, 0};
};

struct GameState;

class Arena;

class ISizeablePositioned : public ISizeable, public IPositioned {
 protected:
  ISizeablePositioned() = default;

  ISizeablePositioned(IPos pos, int width, int height)
      : ISizeable(width, height), IPositioned(pos) {}

 public:
  IPos get_center() const { return pos + IPos{get_width() / 2, get_height() / 2}; }

  virtual bool contains(ISizeablePositioned *obj) {
    IPos rel = obj->pos - pos;
    if (rel.x < 0 || rel.y < 0) return false;
    if (rel.x + obj->get_width() > get_width()) return false;
    if (rel.y + obj->get_height() > get_height()) return false;
    return true;
  }

  virtual bool contains(IPos pos) {
    IPos rel = pos - this->pos;
    if (rel.x < 0 || rel.y < 0) return false;
    if (rel.x > get_width() || rel.y > get_height()) return false;
    return true;
  }

  virtual bool intersects(ISizeablePositioned *obj) {
    if (contains(obj->pos)) return true;
    if (contains(obj->pos + IPos{obj->get_width(), 0})) return true;
    if (contains(obj->pos + IPos{0, obj->get_height()})) return true;
    if (contains(obj->pos + obj->get_size())) return true;
    return false;
  }
};

class IUIElement {
 public:
  virtual void tick() {}
};

class ISizeableTextured : public IRenderable, public ISizeablePositioned {
 private:
  Textures *textures;

 protected:
  std::string texture_name;

  ISizeableTextured(Textures *textures, const std::string &texture_name) {
    this->textures = textures;
    this->texture_name = texture_name;
  }

  virtual IPos get_screen_coords() const { return pos; }

 public:
  void render(cairo_t *cr) const override {
    IPos coords = get_screen_coords();
    textures->render(cr, texture_name, coords.x, coords.y, width, height);
  }
};

class Attack : public IUIElement, public IRenderable {
 private:
  bool deleted = false;

 public:
  void remove() { deleted = true; }

  bool is_deleted() const { return deleted; }

  virtual ~Attack() = default;
};

class Player : public ISizeableTextured, public IUIElement {
 private:
  int hp;
  int iframes;
  GameState *state;

 public:
  int selected = ACT;

  static const int STEP = 4;

  Player(GameState *state);

  IPos get_screen_coords() const override;

  void render(cairo_t *cr) const override;

  void damage(int damage);

  void tick() override;

  int get_hp() const { return hp; }

  bool is_invincible() const { return iframes > 0; }
};

class Arena : public ISizeablePositioned, public IRenderable, public IUIElement {
 private:
  GameState *state;

 public:
  std::vector<Attack *> attacks;
  std::queue<std::string> dialogue;
  std::vector<std::string> cur_text;
  int cur_char = 0;
  int attack_ticks = 0;

  Arena(GameState *state);

  void tick() override;

  void render(cairo_t *cr) const override;
};

class Button : public ISizeableTextured, public IUIElement {
 protected:
  GameState *state;
  std::string normal_texture;
  std::string selected_texture;
  bool selected;

 public:
  Button(GameState *state, const std::string &normal_texture, std::string selected_texture,
         IPos pos, int width, int height);

  void tick() override;

  void render(cairo_t *cr) const override;

  virtual void on_select() {}
};

struct GameState : public ISizeable {
  GameState() = default;

  Input *input;
  MovementMode mode = MODE_SELECT;
  Textures *textures = new Textures();
  Arena *arena;
  Player *player;
  std::vector<Button *> buttons = std::vector<Button *>(4, nullptr);
  long tick = 0;

  void set_width(int width) { this->width = width; }

  void set_height(int height) { this->height = height; }

  void clear_buttons() {
    while (buttons.size() > 4) buttons.pop_back();
  }
};

class Bullet : public Attack, private ISizeablePositioned {
 private:
  GameState *state;
  int radius;
  IPos vec;

 public:
  Bullet(GameState *state, int radius, IPos pos, IPos vec);

  void tick() override {
    if (intersects(state->player)) {
      bool should_remove = !state->player->is_invincible();
      state->player->damage((rand() % 4) + 3);
      if (should_remove) {
        remove();
        return;
      }
    }
    pos = pos + vec;
    IPos c = pos + state->arena->pos;
    if (c.x < -radius || c.y < -radius || c.x > state->get_width() + radius ||
        c.y > state->get_height() + radius)
      remove();
  }

  bool contains(IPos p) override { return (p - pos).sq_dist() <= radius * radius; }

  void render(cairo_t *cr) const override {
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_arc(cr, pos.x + state->arena->pos.x, pos.y + state->arena->pos.y, radius, 0, 360);
    cairo_stroke(cr);
  }
};

class Undertale : public AModule {
 public:
  Undertale(const std::string &, const Json::Value &);
  ~Undertale() override;
  auto update() -> void override;
  void refresh(int /*signal*/) override;

 private:
  void delayWorker();
  void handleEvent();
  void check_asset_path();
  static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer data);

  Gtk::Box box_;
  Gtk::DrawingArea area_;
  std::string asset_path_;
  std::chrono::milliseconds interval_;
  util::command::res output_;

  util::SleeperThread thread_;

  GameState state_;

  bool left_held = false;
  bool right_held = false;
  bool up_held = false;
  bool down_held = false;
};

}  // namespace waybar::modules::undertale
