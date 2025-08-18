#include <assert.h>
#include <bare.h>
#include <cstdint>
#include <js.h>
#include <jstl.h>
#include <stdlib.h>
// #include <string.h>
// #include <uv.h>

#include <notcurses/notcurses.h>

namespace {
using input_callback_t = js_function_t<void, js_arraybuffer_t>;
using resize_callbac_t = js_function_t<void>;
} // namespace

typedef struct {
  notcurses *handle;

  js_env_t *env;
  uv_poll_t input_poll;
  js_persistent_t<input_callback_t> on_input;
  js_persistent_t<input_callback_t> on_resize;
} bare_notcurses_t;

typedef struct {
  ncinput handle;
} bare_notcurses_input_event_t;

typedef struct {
  ncplane *handle;
} bare_ncplane_t;

namespace {

static void
on_poll(uv_poll_t *handle, int status, int events);

inline static void
rearm_poll(uv_poll_t *poll) {
  int err = uv_poll_start(poll, UV_READABLE, on_poll);
  assert(err == 0);
}

static void
on_poll(uv_poll_t *handle, int status, int events) {
  assert(status == 0 && "poll error");

  auto nc = reinterpret_cast<bare_notcurses_t *>(handle->data);

  if (!(events & UV_READABLE)) {
    rearm_poll(&nc->input_poll);
    return;
  }

  int err;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(nc->env, &scope);
  assert(err == 0);

  input_callback_t callback;
  err = js_get_reference_value(nc->env, nc->on_input, callback);
  assert(err == 0);

  bare_notcurses_input_event_t *event;
  js_arraybuffer_t input_handle;

  while (true) {
    err = js_create_arraybuffer(nc->env, event, input_handle);
    assert(err == 0);

    // $ man 3 notcurses_input
    int res = notcurses_get_nblock(nc->handle, &event->handle);
    assert(res != (uint32_t) -1 && "INPUT ERROR");
    if (res == 0) break;

    err = js_call_function_with_checkpoint(nc->env, callback, input_handle);
    if (err) break;
  }

  err = js_close_handle_scope(nc->env, scope);
  assert(err == 0);

  if (err == 0) {
    rearm_poll(&nc->input_poll);
  }
}

inline static void
stop_poll(bare_notcurses_t &nc) {
  if (nc.on_input.empty()) return;

  int err;
  err = uv_poll_stop(&nc.input_poll);
  assert(err == 0);

  nc.on_input.reset();
}

static inline uint64_t
bnu64 (js_env_t *env, js_bigint_t bn) {
  uint64_t u;

  int err = js_get_value_bigint(env, bn, u);
  assert(err == 0);

  return u;
}
} // namespace

static js_object_t
bare_notcurses_init(js_env_t *env) {
  int err;

  js_arraybuffer_t handle;
  bare_notcurses_t *nc;
  err = js_create_arraybuffer(env, nc, handle);
  assert(err == 0);

  memset(static_cast<void *>(nc), 0, sizeof(*nc));

  FILE *fp = NULL;

  notcurses_options options = {
    // .termtype = "xterm-256"
    .loglevel = NCLOGLEVEL_ERROR,
    .margin_t = 0,
    .margin_r = 0,
    .margin_b = 0,
    .margin_l = 0,
    .flags = 0
  };

  nc->handle = notcurses_core_init(&options, fp);
  nc->env = env;
  nc->on_input.reset();

  ncplane *stdplane = notcurses_stdplane(nc->handle);
  ncplane_set_userptr(stdplane, &*nc);

  return handle;
}

static void
bare_notcurses_destroy(js_env_t *env, js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc) {
  int err = 0;

  stop_poll(*nc);

  err = notcurses_stop(nc->handle);
  assert(err == 0);
}

static int
bare_notcurses_render(js_env_t *env, js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc) {
  int err = notcurses_render(nc->handle);
  assert(err == 0);
  return err;
}

static void
bare_notcurses_input_start(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc,
  input_callback_t callback
) {
  assert(nc->on_input.empty() && "NOT INITIALIZED");

  int err;

  uv_loop_t *loop;
  err = js_get_env_loop(env, &loop);
  assert(err == 0);

  int poll_fd = notcurses_inputready_fd(nc->handle);

  err = uv_poll_init(loop, &nc->input_poll, poll_fd);
  assert(err == 0);

  nc->input_poll.data = nc;

  err = js_create_reference(env, callback, nc->on_input);
  assert(err == 0);

  rearm_poll(&nc->input_poll);
}

static void
bare_notcurses_input_stop(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc
) {
  stop_poll(*nc);
}

static js_arraybuffer_t
bare_notcurses_stdplane(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc
) {
  int err;

  bare_ncplane_t *plane;
  js_arraybuffer_t handle;
  err = js_create_arraybuffer(env, plane, handle);
  assert(err == 0);

  plane->handle = notcurses_stdplane(nc->handle);
  assert(plane->handle != NULL);

  return handle;
}

static js_arraybuffer_t
bare_ncplane_create(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc,
  int y,
  int x,
  uint32_t rows,
  uint32_t cols,
  uint64_t flags,
  uint32_t margin_b,
  uint32_t margin_r,
  std::optional<std::string> name
) {
  int err;

  bare_ncplane_t *plane;
  js_arraybuffer_t handle;
  err = js_create_arraybuffer(env, plane, handle);
  assert(err == 0);

  ncplane_options options = {
    .y = y,
    .x = x,
    .rows = rows,
    .cols = cols,
    .userptr = plane,
    .name = nullptr,
    .flags = flags,
    .margin_b = margin_b,
    .margin_r = margin_r
  };

  if (name) {
    options.name = name->c_str();
  }

  ncplane *stdplane = notcurses_stdplane(nc->handle);
  assert(stdplane != NULL);

  plane->handle = ncplane_create(stdplane, &options);

  return handle;
}

static uint32_t
bare_ncplane_get_y(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_y(plane->handle);
}

static uint32_t
bare_ncplane_get_x(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_x(plane->handle);
}

static uint32_t
bare_ncplane_get_height(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_dim_y(plane->handle);
}

static uint32_t
bare_ncplane_get_width(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_dim_x(plane->handle);
}

static uint32_t
bare_ncplane_get_cursor_y(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_cursor_y(plane->handle);
}

static uint32_t
bare_ncplane_get_cursor_x(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_cursor_x(plane->handle);
}

static uint32_t
bare_ncplane_get_styles(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_styles(plane->handle);
}

static void
bare_ncplane_set_styles(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  uint32_t style_mask
) {
  ncplane_set_styles(plane->handle, style_mask & 0xFFFF);
}

static std::optional<std::string>
bare_ncplane_get_name(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  assert(plane->handle != nullptr);

  char *tmp = ncplane_name(plane->handle);
  if (!tmp) return std::nullopt;

  std::string name(tmp);
  free(tmp);

  return name;
}

void
bare_ncplane_set_name(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  std::string name
) {
  int err = ncplane_set_name(plane->handle, name.c_str());
  assert(err == 0);
}

static void
bare_ncplane_resize_simple(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  uint32_t height,
  uint32_t width
) {
  int err = ncplane_resize_simple(plane->handle, height, width);
  assert(err == 0);
}

static void
bare_ncplane_move_yx(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  uint32_t y,
  uint32_t x
) {
  int err = ncplane_move_yx(plane->handle, y, x);
  assert(err == 0);
}

static void
bare_ncplane_erase(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  ncplane_erase(plane->handle);
}

static void
bare_ncplane_set_base(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  std::string egc,
  uint32_t style_mask,
  uint64_t channels
) {
  assert(style_mask <= 0xFFFF && "uint16_t");

  int err = ncplane_set_base(plane->handle, egc.c_str(), style_mask, channels);
  assert(err >= 0);
}

static int
bare_ncplane_putstr_yx(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  int32_t y,
  int32_t x,
  std::string value
) {
  return ncplane_putstr_yx(plane->handle, y, x, value.c_str());
}

static inline int
str_to_nccell(ncplane *plane, nccell *cell, std::string &str, uint16_t style_mask, uint64_t channels) {
  return nccell_prime(plane, cell, str.c_str(), style_mask, channels);
}

static int
bare_ncplane_vline(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  std::string egc,
  uint32_t len
) {
  nccell c = NCCELL_TRIVIAL_INITIALIZER;
  nccell_prime(plane->handle, &c, egc.c_str(), 0, 0);

  int res = ncplane_vline(plane->handle, &c, len);
  nccell_release(plane->handle, &c);

  return res;
}

static js_bigint_t
bare_ncplane_get_channels(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  uint64_t c = ncplane_channels(plane->handle);
  js_bigint_t res;

  int err = js_create_bigint(env, c, res);
  assert(err == 0);

  return res;
}

static void
bare_ncplane_set_channels(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  js_bigint_t channels
) {
  auto c = bnu64(env, channels);
  ncplane_set_channels(plane->handle, c);
}

static void
bare_ncplane_move_top(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  ncplane_move_top(plane->handle);
}

static void
bare_ncplane_move_bottom(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  ncplane_move_bottom(plane->handle);
}

static int
bare_ncplane_cursor_move_yx(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  uint32_t y,
  uint32_t x
) {
  return ncplane_cursor_move_yx(plane->handle, y, x);
}

static uint32_t
bare_ncinput_get_id(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return event->handle.id;
}

static int
bare_ncinput_get_type(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return event->handle.evtype;
}

static int
bare_ncinput_get_y(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return event->handle.y;
}

static int
bare_ncinput_get_x(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return event->handle.x;
}

static std::string
bare_ncinput_get_utf8(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return std::string(event->handle.utf8);
}

static int
bare_ncinput_get_modifiers(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return event->handle.modifiers;
}

static int
bare_ncinput_get_ypx(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return event->handle.ypx;
}

static int
bare_ncinput_get_xpx(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return event->handle.xpx;
}

static js_arraybuffer_t
bare_ncinput_get_eff_text(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  auto view = std::span<uint32_t>(event->handle.eff_text);

  js_arraybuffer_t buffer;
  int err = js_create_arraybuffer(env, view, buffer);
  assert(err == 0);

  return buffer;
}

static uint32_t
bare_ncchannels_get_fchannel(
  js_env_t *env,
  js_bigint_t channels
) {
  auto c = bnu64(env, channels);
  return ncchannels_fchannel(c);
}

static uint32_t
bare_ncchannels_get_bchannel(
  js_env_t *env,
  js_bigint_t channels
) {
  auto c = bnu64(env, channels);
  return ncchannels_bchannel(c);
}

static js_bigint_t
bare_ncchannels_combine(
  js_env_t *env,
  uint32_t fchan,
  uint32_t bchan
) {
  auto c = ncchannels_combine(fchan, bchan);

  js_bigint_t res;
  int err = js_create_bigint(env, c, res);
  assert(err == 0);

  return res;
}


static js_bigint_t
bare_ncchannels_reverse(
  js_env_t *env,
  js_bigint_t channels
) {
  auto c = bnu64(env, channels);

  auto r = ncchannels_reverse(c);
  js_bigint_t res;

  int err = js_create_bigint(env, r, res);
  assert(err == 0);

  return res;
}

static uint32_t
bare_ncchannel_get_rgb(
  js_env_t *env,
  uint32_t channel
) {
  return ncchannel_rgb(channel);
}

static uint32_t
bare_ncchannel_set_rgb(
  js_env_t *env,
  uint32_t channel,
  uint32_t rgb
) {
  ncchannel_set(&channel, rgb);
  return channel;
}

static uint32_t
bare_ncchannel_get_alpha(
  js_env_t *env,
  uint32_t channel
) {
  return ncchannel_alpha(channel);
}

static uint32_t
bare_ncchannel_set_alpha(
  js_env_t *env,
  uint32_t channel,
  uint32_t alpha
) {
  ncchannel_set_alpha(&channel, alpha);
  return channel;
}

static uint32_t
bare_ncchannel_get_palindex(
  js_env_t *env,
  uint32_t channel
) {
  return ncchannel_palindex(channel);
}

static uint32_t
bare_ncchannel_set_palindex(
  js_env_t *env,
  uint32_t channel,
  uint32_t index
) {
  ncchannel_set_palindex(&channel, index);
  return channel;
}

static bool
bare_ncchannel_is_default(
  js_env_t *env,
  uint32_t channel
) {
  return ncchannel_default_p(channel);
}

static bool
bare_ncchannel_is_rgb(
  js_env_t *env,
  uint32_t channel
) {
  return ncchannel_rgb_p(channel);
}

static bool
bare_ncchannel_is_indexed(
  js_env_t *env,
  uint32_t channel
) {
  return ncchannel_palindex_p(channel);
}

js_value_t *
bare_notcurses_exports(js_env_t *env, js_value_t *exports) {
  int err;

  // functions
#define V(name, fn) \
  err = js_set_property<fn>(env, exports, name); \
  assert(err == 0);

  // notcurses

  V("init", bare_notcurses_init)
  V("destroy", bare_notcurses_destroy)
  V("stdplane", bare_notcurses_stdplane)
  V("inputStart", bare_notcurses_input_start)
  V("inputStop", bare_notcurses_input_stop)
  V("render", bare_notcurses_render)

  // ncplane

  V("createPlane", bare_ncplane_create)
  V("planeMoveYX", bare_ncplane_move_yx)
  V("planeResizeSimple", bare_ncplane_resize_simple)
  V("planeErase", bare_ncplane_erase)
  V("planeSetBase", bare_ncplane_set_base)
  V("planePutstrYX", bare_ncplane_putstr_yx)
  V("planeVLine", bare_ncplane_vline)
  V("getPlaneChannels", bare_ncplane_get_channels)
  V("setPlaneChannels", bare_ncplane_set_channels)

  // V("planeHLine", bare_ncplane_hline)
  // V("planeVAlign", bare_ncplane_valign)
  // V("planeHAlign", bare_ncplane_halign)
  V("planeCursorMoveYX", bare_ncplane_cursor_move_yx)
  // V("planeMoveBelow", bare_ncplane_move_below)
  // V("planeMoveAbove", bare_ncplane_move_above)
  V("planeMoveBottom", bare_ncplane_move_bottom)
  V("planeMoveTop", bare_ncplane_move_top)
  // ncplane_box()

  V("getPlaneY", bare_ncplane_get_y)
  V("getPlaneX", bare_ncplane_get_x)
  V("getPlaneHeight", bare_ncplane_get_height)
  V("getPlaneWidth", bare_ncplane_get_width)
  V("getPlaneName", bare_ncplane_get_name)
  V("setPlaneName", bare_ncplane_set_name)
  V("getPlaneCursorY", bare_ncplane_get_cursor_y)
  V("getPlaneCursorX", bare_ncplane_get_cursor_x)
  V("getPlaneStyles", bare_ncplane_get_styles)
  V("setPlaneStyles", bare_ncplane_set_styles)

  // ncinput

  V("getEventId", bare_ncinput_get_id)
  V("getEventType", bare_ncinput_get_type)
  V("getEventY", bare_ncinput_get_y)
  V("getEventX", bare_ncinput_get_x)
  V("getEventYpx", bare_ncinput_get_ypx)
  V("getEventXpx", bare_ncinput_get_xpx)
  V("getEventUtf8", bare_ncinput_get_utf8)
  V("getEventModifiers", bare_ncinput_get_modifiers)
  V("getEventEffText", bare_ncinput_get_eff_text)

  // ncchannels u64-ops
  V("getChannelFg", bare_ncchannels_get_fchannel)
  V("getChannelBg", bare_ncchannels_get_bchannel)
  V("combineChannels", bare_ncchannels_combine)
  V("reverseChannels", bare_ncchannels_reverse)

  // ncchannel u32-ops
  V("getChannelRGB", bare_ncchannel_get_rgb)
  V("setChannelRGB", bare_ncchannel_set_rgb)
  V("getChannelAlpha", bare_ncchannel_get_alpha)
  V("setChannelAlpha", bare_ncchannel_set_alpha)
  V("getChannelPalindex", bare_ncchannel_get_palindex)
  V("setChannelPalindex", bare_ncchannel_set_palindex)
  V("isChannelDefault", bare_ncchannel_is_default)
  V("isChannelRGB", bare_ncchannel_is_rgb)
  V("isChannelIndexed", bare_ncchannel_is_indexed)

#undef V

  // constants
#define V(constant) \
  err = js_set_property(env, exports, #constant, static_cast<uint64_t>(constant)); \
  assert(err == 0);

  V(NCPLANE_OPTION_HORALIGNED)
  V(NCPLANE_OPTION_VERALIGNED)
  V(NCPLANE_OPTION_MARGINALIZED)
  V(NCPLANE_OPTION_FIXED)
  V(NCPLANE_OPTION_AUTOGROW)
  V(NCPLANE_OPTION_VSCROLL)

  V(NCSTYLE_MASK)
  V(NCSTYLE_ITALIC)
  V(NCSTYLE_UNDERLINE)
  V(NCSTYLE_UNDERCURL)
  V(NCSTYLE_BOLD)
  V(NCSTYLE_STRUCK)
  V(NCSTYLE_NONE)

  V(NCKEY_MOD_SHIFT)
  V(NCKEY_MOD_ALT)
  V(NCKEY_MOD_CTRL)
  V(NCKEY_MOD_SUPER)
  V(NCKEY_MOD_HYPER)
  V(NCKEY_MOD_META)
  V(NCKEY_MOD_CAPSLOCK)
  V(NCKEY_MOD_NUMLOCK)

  V(NCTYPE_UNKNOWN)
  V(NCTYPE_PRESS)
  V(NCTYPE_REPEAT)
  V(NCTYPE_RELEASE)

#undef V

  return exports;
}

BARE_MODULE(bare_native, bare_notcurses_exports)
