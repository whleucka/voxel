#include "core/config.hpp"
#include "core/settings.hpp"

#include <cctype>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {

// ─── Small parsing helpers ─────────────────────────────────────────────────

std::string trim(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  size_t b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

bool isInteger(const std::string &s) {
  if (s.empty()) return false;
  size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
  if (i == s.size()) return false;
  for (; i < s.size(); ++i)
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
  return true;
}

// Applies a typed value from the file, warning (and keeping the default) when
// the text can't be parsed as the expected type.
void applyBool(const std::string &k, const std::string &v, bool &out) {
  if (v == "true" || v == "1")       out = true;
  else if (v == "false" || v == "0") out = false;
  else std::cerr << "[config] '" << k << "': expected true/false, got '" << v
                 << "' — keeping default\n";
}

void applyInt(const std::string &k, const std::string &v, int &out) {
  try { out = std::stoi(v); }
  catch (...) {
    std::cerr << "[config] '" << k << "': expected integer, got '" << v
              << "' — keeping default\n";
  }
}

void applyFloat(const std::string &k, const std::string &v, float &out) {
  try { out = std::stof(v); }
  catch (...) {
    std::cerr << "[config] '" << k << "': expected number, got '" << v
              << "' — keeping default\n";
  }
}

// ─── Seed resolution (blank = random, digits = verbatim, text = hashed) ─────
uint32_t resolveSeed(const std::string &raw) {
  std::string v = trim(raw);
  if (v.empty())
    return static_cast<uint32_t>(std::time(nullptr));
  if (isInteger(v))
    return static_cast<uint32_t>(std::stoul(v));
  return static_cast<uint32_t>(std::hash<std::string>{}(v));
}

// ─── Serialization ─────────────────────────────────────────────────────────
// Writes a commented properties file from the given settings. When blank_seed
// is true the level-seed line is left empty (fresh random world each launch);
// otherwise the current world_seed is written so the same world reloads.
std::string serializeSettings(const Settings &s, bool blank_seed) {
  std::ostringstream o;
  o << "# voxel.properties — game/server configuration\n"
    << "# Edit values below, then restart. Delete this file to regenerate it.\n"
    << "# Lines starting with '#' are comments; format is key=value.\n"
    << "\n"
    << "# ─── World ───────────────────────────────────────────────\n"
    << "# Which save to load or create, under saves/<name>/.\n"
    << "world-name=" << s.world_name << "\n"
    << "# Seed for NEW worlds only (an existing save keeps its own seed).\n"
    << "# Blank = new random world; a number is used as-is; any other text\n"
    << "# is hashed into a seed (like Minecraft).\n"
    << "level-seed=";
  if (!blank_seed) o << s.world_seed;
  o << "\n"
    << "# How far (in blocks) chunks stay loaded around the player.\n"
    << "view-distance=" << s.render_distance << "\n"
    << "\n"
    << "# ─── Window ──────────────────────────────────────────────\n"
    << "fullscreen=" << (s.fullscreen ? "true" : "false") << "\n"
    << "vsync=" << (s.vsync ? "true" : "false") << "\n"
    << "# Windowed-mode resolution (ignored while fullscreen).\n"
    << "window-width=" << s.window_width << "\n"
    << "window-height=" << s.window_height << "\n"
    << "\n"
    << "# ─── View ────────────────────────────────────────────────\n"
    << "fov=" << s.fov << "\n"
    << "fog-start=" << s.fog_start << "\n"
    << "fog-end=" << s.fog_end << "\n"
    << "mouse-sensitivity=" << s.mouse_sensitivity << "\n"
    << "\n"
    << "# ─── Atmosphere ──────────────────────────────────────────\n"
    << "# In-game seconds per real second (72 ≈ a 20-minute day).\n"
    << "time-scale=" << s.time_scale << "\n"
    << "water-fog-density=" << s.water_fog_density << "\n"
    << "\n"
    << "# ─── Clouds ──────────────────────────────────────────────\n"
    << "clouds-enabled=" << (s.clouds_enabled ? "true" : "false") << "\n"
    << "cloud-height=" << s.cloud_height << "\n"
    << "cloud-thickness=" << s.cloud_thickness << "\n"
    << "cloud-speed=" << s.cloud_speed << "\n"
    << "cloud-opacity=" << s.cloud_opacity << "\n"
    << "\n"
    << "# ─── Shadows ─────────────────────────────────────────────\n"
    << "shadows-enabled=" << (s.shadows_enabled ? "true" : "false") << "\n"
    << "shadow-distance=" << s.shadow_distance << "\n"
    << "# Depth-map resolution: 1024, 2048, or 4096.\n"
    << "shadow-map-size=" << s.shadow_map_size << "\n";
  return o.str();
}

void writeDefaultFile(const std::string &path) {
  std::ofstream out(path);
  if (!out) {
    std::cerr << "[config] could not write default '" << path
              << "' — using built-in defaults\n";
    return;
  }
  out << serializeSettings(Settings{}, /*blank_seed=*/true);
  std::cout << "[config] wrote default properties to '" << path << "'\n";
}

} // namespace

bool saveServerProperties(const std::string &path) {
  std::ofstream out(path);
  if (!out) {
    std::cerr << "[config] could not save settings to '" << path << "'\n";
    return false;
  }
  out << serializeSettings(g_settings, /*blank_seed=*/false);
  std::cout << "[config] saved settings to '" << path << "'\n";
  return true;
}

void loadServerProperties(const std::string &path) {
  // Create a default file on first run so there is always something to edit.
  {
    std::ifstream probe(path);
    if (!probe.good()) writeDefaultFile(path);
  }

  std::ifstream in(path);
  if (!in) {
    std::cerr << "[config] could not open '" << path
              << "' — using built-in defaults\n";
    return;
  }

  std::unordered_map<std::string, std::string> kv;
  std::string line;
  while (std::getline(in, line)) {
    std::string s = trim(line);
    if (s.empty() || s[0] == '#') continue;
    size_t eq = s.find('=');
    if (eq == std::string::npos) {
      std::cerr << "[config] ignoring malformed line: '" << s << "'\n";
      continue;
    }
    kv[trim(s.substr(0, eq))] = trim(s.substr(eq + 1));
  }

  // Seed is special: absent key still means "random".
  auto seedIt = kv.find("level-seed");
  g_settings.world_seed = resolveSeed(seedIt == kv.end() ? "" : seedIt->second);
  kv.erase("level-seed");

  for (const auto &[k, v] : kv) {
    if      (k == "world-name")        { if (!v.empty()) g_settings.world_name = v; }
    else if (k == "view-distance")     applyInt(k, v, g_settings.render_distance);
    else if (k == "fullscreen")        applyBool(k, v, g_settings.fullscreen);
    else if (k == "vsync")             applyBool(k, v, g_settings.vsync);
    else if (k == "window-width")      applyInt(k, v, g_settings.window_width);
    else if (k == "window-height")     applyInt(k, v, g_settings.window_height);
    else if (k == "fov")               applyFloat(k, v, g_settings.fov);
    else if (k == "fog-start")         applyFloat(k, v, g_settings.fog_start);
    else if (k == "fog-end")           applyFloat(k, v, g_settings.fog_end);
    else if (k == "mouse-sensitivity") applyFloat(k, v, g_settings.mouse_sensitivity);
    else if (k == "time-scale")        applyFloat(k, v, g_settings.time_scale);
    else if (k == "water-fog-density") applyFloat(k, v, g_settings.water_fog_density);
    else if (k == "clouds-enabled")    applyBool(k, v, g_settings.clouds_enabled);
    else if (k == "cloud-height")      applyFloat(k, v, g_settings.cloud_height);
    else if (k == "cloud-thickness")   applyInt(k, v, g_settings.cloud_thickness);
    else if (k == "cloud-speed")       applyFloat(k, v, g_settings.cloud_speed);
    else if (k == "cloud-opacity")     applyFloat(k, v, g_settings.cloud_opacity);
    else if (k == "shadows-enabled")   applyBool(k, v, g_settings.shadows_enabled);
    else if (k == "shadow-distance")   applyFloat(k, v, g_settings.shadow_distance);
    else if (k == "shadow-map-size")   applyInt(k, v, g_settings.shadow_map_size);
    else std::cerr << "[config] unknown key '" << k << "' — ignored\n";
  }
}
