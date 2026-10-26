#include "core/session.hpp"
#include "core/settings.hpp"
#include "net/wire.hpp"
#include <iostream>

std::unique_ptr<Session> Session::local(Mode mode, const Options &opts, std::string &error) {
  std::unique_ptr<Session> s(new Session(mode));
  s->opts = opts;
  s->game_ = std::make_unique<Game>();
  if (s->game_->load(opts.save_dir))
    std::cout << "Loaded world '" << opts.save_dir << "' (seed " << g_settings.world_seed << ")\n";
  else
    std::cout << "New world '" << opts.save_dir << "' (seed " << g_settings.world_seed << ")\n";

  s->game_->addClient(s->local_link, Game::Role::Host);
  if (mode == Mode::Host) {
    s->server_ = std::make_unique<NetServer>(*s->game_);
    if (!s->server_->listen(opts.port, error)) {
      error = "Can't host on port " + std::to_string(opts.port) + ": " + error;
      return nullptr;
    }
    std::string beacon_error;
    if (opts.beacon && !s->beacon.start(beacon_error))
      std::cerr << "[net] no LAN beacon (" << beacon_error << "); guests can type the address\n";
  }
  s->local_link.send(Join{opts.name});
  s->game_->pump();
  return s;
}

std::unique_ptr<Session> Session::join(const std::string &address, const Options &opts,
                                       std::string &error) {
  std::unique_ptr<Session> s(new Session(Mode::Guest));
  s->opts = opts;
  s->remote = std::make_unique<NetConnection>();
  if (!s->remote->connect(address, opts.port, error)) return nullptr;
  s->saved_seed = g_settings.world_seed;
  s->saved_offset = g_settings.noise_offset;
  s->client_world = std::make_unique<World>();
  s->remote->send(Join{opts.name});
  return s;
}

Session::~Session() {
  end();
  if (mode_ == Mode::Guest) {
    // The world's generator threads read the seed: stop them first.
    client_world.reset();
    g_settings.world_seed = saved_seed;
    g_settings.noise_offset = saved_offset;
  }
}

Connection &Session::connection() {
  if (remote) return *remote;
  return local_link;
}

void Session::frame(float dt, double now) {
  if (mode_ == Mode::Guest) {
    client_clock.update(dt);
    return;
  }
  if (server_) server_->poll();
  game_->pump();
  game_->advance(dt);
  if (server_) {
    server_->flush();
    beacon.tick(now, {wire::kProtocolVersion, opts.world_name, opts.name,
                      static_cast<uint8_t>(game_->playerCount()),
                      static_cast<uint8_t>(Game::kMaxPlayers), server_->port()});
  }
}

void Session::follow(glm::vec3 position) {
  if (client_world) client_world->setInterestPoints(std::span(&position, 1));
}

void Session::poll(std::vector<Event> &out) {
  size_t first = out.size();
  connection().poll(out);
  if (mode_ != Mode::Guest) return;
  for (size_t i = first; i < out.size(); ++i) {
    const Event &ev = out[i];
    if (const auto *w = std::get_if<Welcome>(&ev)) {
      // Before any chunk exists: chunks generate from these as they stream in.
      g_settings.world_seed = w->seed;
      g_settings.noise_offset = w->noise_offset;
      client_world->setLoadedEdits(std::span<const BlockEdit>(w->edits));
      client_clock.time_of_day = w->time_of_day;
      client_clock.scale = w->time_scale;
    } else if (const auto *t = std::get_if<TimeSync>(&ev)) {
      client_clock.time_of_day = t->time_of_day;
      client_clock.scale = t->time_scale;
    } else if (const auto *b = std::get_if<BlockChanged>(&ev)) {
      client_world->applyEdit(b->pos, b->type);
    }
  }
}

void Session::end() {
  if (ended) return;
  ended = true;
  if (game_) game_->save();
  if (server_) server_->shutdown("Host left the game");
}
