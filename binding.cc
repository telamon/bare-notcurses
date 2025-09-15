#include <assert.h>
#include <bare.h>
#include <cstdint>
#include <js.h>
#include <jstl.h>
#include <stdlib.h>

#include <notcurses/notcurses.h>

namespace {
using input_callback_t = js_function_t<void, js_arraybuffer_t>;
using resize_callback_t = js_function_t<void>;
} // namespace

typedef struct {
  notcurses *handle;

  js_env_t *env;
  uv_poll_t input_poll;
  js_persistent_t<input_callback_t> on_input;
} bare_notcurses_t;

typedef struct {
  ncinput handle;
} bare_notcurses_input_event_t;

typedef struct {
  ncplane *handle;
  js_persistent_t<resize_callback_t> on_resize;
} bare_ncplane_t;

typedef struct {
  ncvisual *handle;
  uint32_t width;
  uint32_t height;
  uint8_t bpp;

  js_persistent_t<js_arraybuffer_t> data;
  uint32_t offset;
  uint32_t len;
} bare_ncvisual_t;

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

static int
on_plane_resize (ncplane *ncp) {
  auto plane = reinterpret_cast<bare_ncplane_t *>(ncplane_userptr(ncp));
  assert(plane->handle == ncp);

  auto ctx = ncplane_notcurses(plane->handle);
  auto stdplane = notcurses_stdplane(ctx);
  auto nc = reinterpret_cast<bare_notcurses_t *>(ncplane_userptr(stdplane));

  int err;

  js_handle_scope_t *scope;
  err = js_open_handle_scope(nc->env, &scope);
  assert(err == 0);

  resize_callback_t callback;
  err = js_get_reference_value(nc->env, plane->on_resize, callback);
  assert(err == 0);

  int res = js_call_function_with_checkpoint(nc->env, callback);

  err = js_close_handle_scope(nc->env, scope);
  assert(err == 0);

  return res;
}

} // namespace

static js_object_t
bare_notcurses_init(js_env_t *env, uint64_t flags) {
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
    .flags = flags
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

static int
bare_notcurses_check_pixel_support (
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc
) {
  return notcurses_check_pixel_support(nc->handle);
}

static void
bare_notcurses_input_start(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc,
  input_callback_t callback,
  uint32_t miceEnable
) {
  assert(nc->on_input.empty() && "NOT INITIALIZED");

  int err;

  uv_loop_t *loop;
  err = js_get_env_loop(env, &loop);
  assert(err == 0);

  int poll_fd = notcurses_inputready_fd(nc->handle);

  if (miceEnable != NCMICE_NO_EVENTS) {
    int res = notcurses_mice_enable(nc->handle, miceEnable);
    assert(res == 0 && "MOUSE FAILED"); // TODO: silent fail
  }

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
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> parent,
  int y,
  int x,
  uint32_t rows,
  uint32_t cols,
  uint64_t flags,
  uint32_t margin_b,
  uint32_t margin_r,
  std::optional<std::string> name,
  std::optional<resize_callback_t> onresize
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

  if (margin_r || margin_b) {
    options.flags |= NCPLANE_OPTION_MARGINALIZED;
  }

  if (name) {
    options.name = name->c_str();
  }

  if (onresize) {
    err = js_create_reference(env, *onresize, plane->on_resize);
    assert(err == 0);
    options.resizecb = on_plane_resize;
  }

  plane->handle = ncplane_create(parent->handle, &options);

  return handle;
}

static int
bare_ncplane_destroy(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  int err = ncplane_destroy(plane->handle);
  assert(err == 0);

  plane->handle = nullptr;
  plane->on_resize.reset();

  return err;
}

static int
bare_ncplane_family_destroy(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  int err = ncplane_family_destroy(plane->handle);
  assert(err == 0);

  plane->handle = nullptr;
  plane->on_resize.reset();

  return err;
}

#if 0
static js_arraybuffer_t
bare_ncplane_get_notcurses(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  auto nc = ncplane_notcurses(plane->handle);
  auto stdplane = notcurses_stdplane(nc);
  auto bare_nc = reinterpret_cast<bare_notcurses_t *>(ncplane_userptr(stdplane));
}
#endif

static js_object_t
bare_ncplane_pixel_geom(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  int err;

  uint32_t pxy, pxx;
  uint32_t celldimy, celldimx;
  uint32_t maxbmapy, maxbmapx;

  ncplane_pixel_geom(plane->handle, &pxy, &pxx, &celldimy, &celldimx, &maxbmapy, &maxbmapx);

  js_object_t res;
  err = js_create_object(env, res);
  assert(err == 0);

#define V(sym) \
  err = js_set_property(env, res, #sym, sym); \
  assert(err == 0);

  V(pxy)
  V(pxx)
  V(celldimy)
  V(celldimx)
  V(maxbmapy)
  V(maxbmapx)

#undef V

  return res;
}

static int32_t
bare_ncplane_get_y(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_y(plane->handle);
}

static int32_t
bare_ncplane_get_x(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_x(plane->handle);
}

static uint32_t
bare_ncplane_get_dim_y(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  return ncplane_dim_y(plane->handle);
}

static uint32_t
bare_ncplane_get_dim_x(
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
  char *tmp = ncplane_name(plane->handle);
  if (!tmp) return std::nullopt;

  std::string name(tmp);
  free(tmp);

  return name;
}

static void
bare_ncplane_set_name(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  std::string name
) {
  int err = ncplane_set_name(plane->handle, name.c_str());
  assert(err == 0);
}

static int
bare_ncplane_resize_simple(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  uint32_t height,
  uint32_t width
) {
  int err = ncplane_resize_simple(plane->handle, height, width);
  assert(err == 0);

  return err;
}

static int
bare_ncplane_move_yx(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  int32_t y,
  int32_t x
) {
  int err = ncplane_move_yx(plane->handle, y, x);
  assert(err == 0);

  return err;
}

static void
bare_ncplane_erase(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane
) {
  ncplane_erase(plane->handle);
}

static int
bare_ncplane_set_base(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  std::string egc,
  uint32_t style_mask,
  js_bigint_t channels
) {
  assert(style_mask <= 0xFFFF && "uint16_t");

  auto c = bnu64(env, channels);

  int err = ncplane_set_base(plane->handle, egc.c_str(), style_mask, c);
  assert(err >= 0);

  return err;
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

static int
bare_ncplane_mergedown_simple(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> dst
) {
 int err = ncplane_mergedown_simple(plane->handle, dst->handle);
 assert(err == 0);
 return err;
}

static int
bare_ncplane_perimeter_simple(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  int type,
  uint32_t style_mask,
  js_bigint_t channels,
  uint32_t ctlword
) {
  auto c = bnu64(env, channels);
  int err;
  switch (type) {
    default:
    case 0:
      err = ncplane_perimeter_rounded(plane->handle, style_mask, c, ctlword);
      break;
    case 1:
      err = ncplane_perimeter_double(plane->handle, style_mask, c, ctlword);
      break;
  }
  assert(err == 0);
  return err;
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

static bool
bare_ncplane_reparent_family(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> new_parent
) {
  auto res = ncplane_reparent_family(plane->handle, new_parent->handle);
  return res != nullptr;
}

static int
bare_ncplane_cursor_move_yx(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  int32_t y,
  int32_t x
) {
  return ncplane_cursor_move_yx(plane->handle, y, x);
}

static std::string
bare_ncplane_contents(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncplane_t, 1> plane,
  int32_t begy,
  int32_t begx,
  int32_t leny,
  int32_t lenx
) {
  auto str = ncplane_contents(plane->handle, begy, begx, leny, lenx);
  assert(str != nullptr);

  std::string text(str);
  free(str);

  return text;
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

static bool
bare_ncpinput_get_mouse(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_input_event_t, 1> event
) {
  return nckey_mouse_p(event->handle.id);
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

static js_arraybuffer_t
bare_ncvisual_create(
  js_env_t *env,
  js_arraybuffer_t data,
  uint32_t offset,
  uint32_t len,
  uint32_t width,
  uint32_t height,
  uint32_t bpp // bytes per pixel
) {
  int err;

  js_arraybuffer_t handle;
  bare_ncvisual_t *visual;

  err = js_create_arraybuffer(env, visual, handle);
  assert(err == 0);

  // save ref to redraw on resize
  err = js_create_reference(env, data, visual->data);
  assert(err == 0);

  visual->offset = offset;
  visual->len = len;
  visual->width = width;
  visual->height = height;
  visual->bpp = bpp;

  std::span<uint8_t> rgba;
  err = js_get_arraybuffer_info(env, data, rgba);
  assert(err == 0);
  assert(rgba.size() <= offset + len && "BUFFER SLICE");
  assert(height * width * 4 == len && "PIXELS");

  visual->handle = ncvisual_from_rgba(&rgba[offset], height, width * bpp, width);

  return handle;
}

static void
bare_ncvisual_destroy(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_ncvisual_t, 1> visual
) {
  visual->data.reset();
  ncvisual_destroy(visual->handle);
}

static std::optional<js_arraybuffer_t>
bare_ncvisual_blit(
  js_env_t *env,
  js_arraybuffer_span_of_t<bare_notcurses_t, 1> nc,
  js_arraybuffer_span_of_t<bare_ncvisual_t, 1> visual,
  std::optional<js_arraybuffer_span_of_t<bare_ncplane_t, 1>> dst,
  int y,
  int x,
  int scaling,
  int blitter,
  uint64_t flags
) {
  ncvisual_options opts = {
    .y = y,
    .x = x,
    .blitter = static_cast<ncblitter_e>(blitter),
    .flags = flags,
  };

  //  .scaling = NCSCALE_STRETCH,
  //  .blitter = NCBLIT_3x2, // Seems default

  if (dst) {
    opts.n = (*dst)->handle;
    opts.scaling = static_cast<ncscale_e>(scaling);

    ncplane *cplane = ncvisual_blit(nc->handle, visual->handle, &opts);
    assert(cplane == (*dst)->handle);

    return std::nullopt;
  } else {
    js_arraybuffer_t handle;
    bare_ncplane_t *plane;

    int err = js_create_arraybuffer(env, plane, handle);
    assert(err == 0);

    plane->handle = ncvisual_blit(nc->handle, visual->handle, &opts);
    assert(plane->handle != nullptr);

    return handle;
  }
}

int32_t
bare_notcurses_ncstrwidth(js_env_t *env, std::string text, bool ignoreInvalid) {
  int bytes, width;

  int res = ncstrwidth(text.c_str(), &bytes, &width);

  if (res < 0 && ignoreInvalid) return width;
  return res;
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
  V("pixelSupport", bare_notcurses_check_pixel_support)

  // ncplane

  V("planeCreate", bare_ncplane_create)
  V("planeDestroy", bare_ncplane_destroy)
  V("planeFamilyDestroy", bare_ncplane_family_destroy)
  V("planePixelGeom", bare_ncplane_pixel_geom)
  V("planeMoveYX", bare_ncplane_move_yx)
  V("planeResizeSimple", bare_ncplane_resize_simple)
  V("planeErase", bare_ncplane_erase)
  V("planeSetBase", bare_ncplane_set_base)
  V("planePutstrYX", bare_ncplane_putstr_yx)
  V("planeVLine", bare_ncplane_vline)
  V("planeMergedown", bare_ncplane_mergedown_simple)
  V("planePerimeter", bare_ncplane_perimeter_simple)
  // V("planeHLine", bare_ncplane_hline)
  // V("planeVAlign", bare_ncplane_valign)
  // V("planeHAlign", bare_ncplane_halign)
  V("planeCursorMoveYX", bare_ncplane_cursor_move_yx)
  // V("planeMoveBelow", bare_ncplane_move_below)
  // V("planeMoveAbove", bare_ncplane_move_above)
  V("planeMoveBottom", bare_ncplane_move_bottom)
  V("planeMoveTop", bare_ncplane_move_top)
  V("planeReparentFamily", bare_ncplane_reparent_family)
  V("planeContents", bare_ncplane_contents)
  // ncplane_box()

  V("getPlaneY", bare_ncplane_get_y)
  V("getPlaneX", bare_ncplane_get_x)
  V("getPlaneDimY", bare_ncplane_get_dim_y)
  V("getPlaneDimX", bare_ncplane_get_dim_x)
  V("getPlaneName", bare_ncplane_get_name)
  V("setPlaneName", bare_ncplane_set_name)
  V("getPlaneCursorY", bare_ncplane_get_cursor_y)
  V("getPlaneCursorX", bare_ncplane_get_cursor_x)
  V("getPlaneStyles", bare_ncplane_get_styles)
  V("setPlaneStyles", bare_ncplane_set_styles)
  V("getPlaneChannels", bare_ncplane_get_channels)
  V("setPlaneChannels", bare_ncplane_set_channels)

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
  V("getEventMouse", bare_ncpinput_get_mouse)

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

  // ncvisual

  V("visualCreate", bare_ncvisual_create);
  V("visualDestroy", bare_ncvisual_destroy);
  V("visualBlit", bare_ncvisual_blit);

  // util

  V("ncstrwidth", bare_notcurses_ncstrwidth);
#undef V

  // constants
#define V(constant) \
  err = js_set_property(env, exports, #constant, static_cast<uint64_t>(constant)); \
  assert(err == 0);

  V(NCOPTION_INHIBIT_SETLOCALE)
  V(NCOPTION_NO_CLEAR_BITMAPS)
  V(NCOPTION_NO_WINCH_SIGHANDLER)
  V(NCOPTION_NO_QUIT_SIGHANDLERS)
  V(NCOPTION_PRESERVE_CURSOR)
  V(NCOPTION_SUPPRESS_BANNERS)
  V(NCOPTION_NO_ALTERNATE_SCREEN)
  V(NCOPTION_NO_FONT_CHANGES)
  V(NCOPTION_DRAIN_INPUT)
  V(NCOPTION_SCROLLING)
  V(NCOPTION_CLI_MODE)

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

  V(NCTYPE_UNKNOWN)
  V(NCTYPE_PRESS)
  V(NCTYPE_REPEAT)
  V(NCTYPE_RELEASE)

  V(NCMICE_NO_EVENTS)
  V(NCMICE_MOVE_EVENT)
  V(NCMICE_BUTTON_EVENT)
  V(NCMICE_DRAG_EVENT)
  V(NCMICE_ALL_EVENTS)

  V(NCPIXEL_NONE)
  V(NCPIXEL_SIXEL)
  V(NCPIXEL_LINUXFB)
  V(NCPIXEL_ITERM2)
  V(NCPIXEL_KITTY_STATIC)
  V(NCPIXEL_KITTY_ANIMATED)
  V(NCPIXEL_KITTY_SELFREF)

  V(NCBLIT_DEFAULT)
  V(NCBLIT_1x1)
  V(NCBLIT_2x1)
  V(NCBLIT_2x2)
  V(NCBLIT_3x2)
  V(NCBLIT_4x2)
  V(NCBLIT_BRAILLE)
  V(NCBLIT_PIXEL)
  V(NCBLIT_4x1)
  V(NCBLIT_8x1)

  V(NCSCALE_NONE)
  V(NCSCALE_SCALE)
  V(NCSCALE_STRETCH)
  V(NCSCALE_NONE_HIRES)
  V(NCSCALE_SCALE_HIRES)

  V(NCVISUAL_OPTION_NODEGRADE)
  V(NCVISUAL_OPTION_BLEND)
  V(NCVISUAL_OPTION_HORALIGNED)
  V(NCVISUAL_OPTION_VERALIGNED)
  V(NCVISUAL_OPTION_ADDALPHA)
  V(NCVISUAL_OPTION_CHILDPLANE)
  V(NCVISUAL_OPTION_NOINTERPOLATE)

  V(NCKEY_INVALID)
  V(NCKEY_RESIZE)
  V(NCKEY_UP)
  V(NCKEY_RIGHT)
  V(NCKEY_DOWN)
  V(NCKEY_LEFT)
  V(NCKEY_INS)
  V(NCKEY_DEL)
  V(NCKEY_BACKSPACE)
  V(NCKEY_PGDOWN)
  V(NCKEY_PGUP)
  V(NCKEY_HOME)
  V(NCKEY_END)
  V(NCKEY_F00)
  V(NCKEY_F01)
  V(NCKEY_F02)
  V(NCKEY_F03)
  V(NCKEY_F04)
  V(NCKEY_F05)
  V(NCKEY_F06)
  V(NCKEY_F07)
  V(NCKEY_F08)
  V(NCKEY_F09)
  V(NCKEY_F10)
  V(NCKEY_F11)
  V(NCKEY_F12)
  V(NCKEY_F13)
  V(NCKEY_F14)
  V(NCKEY_F15)
  V(NCKEY_F16)
  V(NCKEY_F17)
  V(NCKEY_F18)
  V(NCKEY_F19)
  V(NCKEY_F20)
  V(NCKEY_F21)
  V(NCKEY_F22)
  V(NCKEY_F23)
  V(NCKEY_F24)
  V(NCKEY_F25)
  V(NCKEY_F26)
  V(NCKEY_F27)
  V(NCKEY_F28)
  V(NCKEY_F29)
  V(NCKEY_F30)
  V(NCKEY_F31)
  V(NCKEY_F32)
  V(NCKEY_F33)
  V(NCKEY_F34)
  V(NCKEY_F35)
  V(NCKEY_F36)
  V(NCKEY_F37)
  V(NCKEY_F38)
  V(NCKEY_F39)
  V(NCKEY_F40)
  V(NCKEY_F41)
  V(NCKEY_F42)
  V(NCKEY_F43)
  V(NCKEY_F44)
  V(NCKEY_F45)
  V(NCKEY_F46)
  V(NCKEY_F47)
  V(NCKEY_F48)
  V(NCKEY_F49)
  V(NCKEY_F50)
  V(NCKEY_F51)
  V(NCKEY_F52)
  V(NCKEY_F53)
  V(NCKEY_F54)
  V(NCKEY_F55)
  V(NCKEY_F56)
  V(NCKEY_F57)
  V(NCKEY_F58)
  V(NCKEY_F59)
  V(NCKEY_F60)
  V(NCKEY_ENTER)
  V(NCKEY_CLS)
  V(NCKEY_DLEFT)
  V(NCKEY_DRIGHT)
  V(NCKEY_ULEFT)
  V(NCKEY_URIGHT)
  V(NCKEY_CENTER)
  V(NCKEY_BEGIN)
  V(NCKEY_CANCEL)
  V(NCKEY_CLOSE)
  V(NCKEY_COMMAND)
  V(NCKEY_COPY)
  V(NCKEY_EXIT)
  V(NCKEY_PRINT)
  V(NCKEY_REFRESH)
  V(NCKEY_SEPARATOR)
  V(NCKEY_CAPS_LOCK)
  V(NCKEY_SCROLL_LOCK)
  V(NCKEY_NUM_LOCK)
  V(NCKEY_PRINT_SCREEN)
  V(NCKEY_PAUSE)
  V(NCKEY_MENU)
  V(NCKEY_MEDIA_PLAY)
  V(NCKEY_MEDIA_PAUSE)
  V(NCKEY_MEDIA_PPAUSE)
  V(NCKEY_MEDIA_REV)
  V(NCKEY_MEDIA_STOP)
  V(NCKEY_MEDIA_FF)
  V(NCKEY_MEDIA_REWIND)
  V(NCKEY_MEDIA_NEXT)
  V(NCKEY_MEDIA_PREV)
  V(NCKEY_MEDIA_RECORD)
  V(NCKEY_MEDIA_LVOL)
  V(NCKEY_MEDIA_RVOL)
  V(NCKEY_MEDIA_MUTE)
  V(NCKEY_LSHIFT)
  V(NCKEY_LCTRL)
  V(NCKEY_LALT)
  V(NCKEY_LSUPER)
  V(NCKEY_LHYPER)
  V(NCKEY_LMETA)
  V(NCKEY_RSHIFT)
  V(NCKEY_RCTRL)
  V(NCKEY_RALT)
  V(NCKEY_RSUPER)
  V(NCKEY_RHYPER)
  V(NCKEY_RMETA)
  V(NCKEY_L3SHIFT)
  V(NCKEY_L5SHIFT)
  V(NCKEY_MOTION)
  V(NCKEY_BUTTON1)
  V(NCKEY_BUTTON2)
  V(NCKEY_BUTTON3)
  V(NCKEY_BUTTON4)
  V(NCKEY_BUTTON5)
  V(NCKEY_BUTTON6)
  V(NCKEY_BUTTON7)
  V(NCKEY_BUTTON8)
  V(NCKEY_BUTTON9)
  V(NCKEY_BUTTON10)
  V(NCKEY_BUTTON11)
  V(NCKEY_SIGNAL)
  V(NCKEY_EOF)
  V(NCKEY_MOD_SHIFT)
  V(NCKEY_MOD_ALT)
  V(NCKEY_MOD_CTRL)
  V(NCKEY_MOD_SUPER)
  V(NCKEY_MOD_HYPER)
  V(NCKEY_MOD_META)
  V(NCKEY_MOD_CAPSLOCK)
  V(NCKEY_MOD_NUMLOCK)
  V(NCKEY_SCROLL_UP)
  V(NCKEY_SCROLL_DOWN)
  V(NCKEY_RETURN)
  V(NCKEY_TAB)
  V(NCKEY_ESC)
  V(NCKEY_SPACE)

#undef V

  // forward string constants

#define V(name, value) \
  err = js_set_property(env, exports, name, value); \
  assert(err == 0);

  V("NOTCURSES_VERSION", notcurses_version())

  char *tmp;

  tmp = notcurses_hostname();
  V("NOTCURSES_HOSTNAME", tmp)
  free(tmp);

  tmp = notcurses_osversion();
  V("NOTCURSES_OSVERSION", tmp)
  free(tmp);

  tmp = notcurses_accountname();
  V("NOTCURSES_ACCOUNTNAME", tmp)
  free(tmp);

#undef V
  return exports;
}

BARE_MODULE(bare_native, bare_notcurses_exports)
