#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <set>
#include <map>
#include <tuple>

#ifndef NDEBUG
#define DBG(x) do { std::cerr << x; } while (0)
#else
#define DBG(x) do {} while (0)
#endif

constexpr int MAX_TURN = 200;
constexpr int START_GOLD = 500;
constexpr int START_WARRIORS = 3;
constexpr int MOVE_COST = 10;
constexpr int TRAIN_COST = 120;
constexpr int WORK_INCOME = 15;
constexpr int UPKEEP_PER_WARRIOR = 2;
constexpr int HQ_MAX_LEVEL = 5;
constexpr int BASE_MAX_LEVEL = 3;
constexpr int HQ_HEAL_COST = 1000;
constexpr int BASE_HEAL_COST = 500;

struct HqLevelEntry { int upgrade_cost; int warrior_hp; int hp; int turret; int train_cap; int work_cap; };
struct BaseLevelEntry { int cost; int hp; int turret; int work_cap; };

constexpr HqLevelEntry HQ_LEVELS[HQ_MAX_LEVEL + 1] = {
    {0, 0, 0, 0, 0, 0}, {0, 4, 10, 1, 1, 1}, {600, 5, 15, 2, 1, 2},
    {1200, 6, 20, 2, 2, 3}, {2400, 7, 25, 3, 2, 4}, {3600, 8, 30, 3, 3, 5},
};
constexpr BaseLevelEntry BASE_LEVELS[BASE_MAX_LEVEL + 1] = {
    {0, 0, 0, 0}, {300, 6, 1, 1}, {600, 12, 1, 2}, {1000, 18, 2, 3},
};

enum class Side : int { LEFT = 0, RIGHT = 1 };
enum class BType : int { HQ, BASE };
enum class WState : int { STATIONARY, MOVING };

inline Side opposite(Side s) { return s == Side::LEFT ? Side::RIGHT : Side::LEFT; }
inline char side_char(Side s) { return s == Side::LEFT ? 'A' : 'B'; }
inline Side parse_side_char(char c) { return c == 'A' ? Side::LEFT : Side::RIGHT; }

struct WarriorId {
  Side side = Side::LEFT;
  int num = 0;
  bool operator==(const WarriorId &o) const { return side == o.side && num == o.num; }
  bool operator<(const WarriorId &o) const {
    if (side != o.side) return side < o.side;
    return num < o.num;
  }
};

struct Warrior {
  WarriorId id; int region = 0; int hp = 0; WState state = WState::STATIONARY; int target = 0;
};

struct Building {
  int region = 0; Side side = Side::LEFT; BType type = BType::HQ; int level = 1; int hp = 10;
  int current_hp() const { return type == BType::HQ ? HQ_LEVELS[level].hp : BASE_LEVELS[level].hp; }
  int work_cap() const { return type == BType::HQ ? HQ_LEVELS[level].work_cap : BASE_LEVELS[level].work_cap; }
  int train_cap() const { return type == BType::HQ ? HQ_LEVELS[level].train_cap : 0; }
};

struct GameMap {
  int N = 0, K = 0, center_region = 0;
  std::vector<long long> x, y; std::vector<int> strongholds; std::vector<std::vector<int>> adj;
  Side my_side = Side::LEFT; int my_hq = 0; int opp_hq = 0;
};

struct GameState {
  int gold = START_GOLD; int my_countdown = 5; int opp_countdown = 5;
  std::vector<Warrior> warriors; std::vector<Building> buildings;
  int next_id[2] = {START_WARRIORS + 1, START_WARRIORS + 1};
};

struct Actions {
  int train_n = 0; std::vector<std::pair<WarriorId, int>> moves; std::vector<int> upgrades;
};

static std::string readln() { std::string s; if (!std::getline(std::cin, s)) std::exit(0); return s; }
static std::vector<std::string> tokens(const std::string &s) {
  std::vector<std::string> out; std::istringstream is(s); for (std::string t; is >> t;) out.push_back(t); return out;
}
static WarriorId parse_warrior(const std::string &tok) {
  WarriorId id; id.side = parse_side_char(tok[0]); id.num = std::stoi(tok.substr(1)); return id;
}
static std::string format_warrior(WarriorId id) {
  std::string s; s.push_back(side_char(id.side)); s += std::to_string(id.num); return s;
}
static int hq_of(const GameMap &M, Side s) { return (s == Side::LEFT) ? 0 : M.N - 1; }
static Building make_base(int region, Side s) { return Building{region, s, BType::BASE, 1, BASE_LEVELS[1].hp}; }
static void apply_upgrade(Building &b) { b.level += 1; b.hp = b.current_hp(); }
static int upgrade_cost(const Building &b) { return b.type == BType::HQ ? HQ_LEVELS[b.level + 1].upgrade_cost : BASE_LEVELS[b.level + 1].cost; }
static int max_level(const Building &b) { return b.type == BType::HQ ? HQ_MAX_LEVEL : BASE_MAX_LEVEL; }

static void parse_init(GameMap &M, GameState &S) {
  { auto t = tokens(readln()); M.my_side = (t[1] == "LEFT") ? Side::LEFT : Side::RIGHT; }
  { auto t = tokens(readln()); M.N = std::stoi(t.at(0)); M.K = std::stoi(t.at(1)); }
  M.center_region = (M.N - 1) / 2;
  M.x.assign(M.N, 0); M.y.assign(M.N, 0);
  { auto t = tokens(readln()); for (int i = 0; i < M.N; ++i) M.x[i] = std::stoll(t.at(i)); }
  { auto t = tokens(readln()); for (int i = 0; i < M.N; ++i) M.y[i] = std::stoll(t.at(i)); }
  { auto t = tokens(readln()); M.strongholds.clear(); for (const auto &s : t) M.strongholds.push_back(std::stoi(s)); std::sort(M.strongholds.begin(), M.strongholds.end()); }
  M.adj.assign(M.N, {});
  for (int r = 0; r < M.N; ++r) {
    auto t = tokens(readln()); int deg = std::stoi(t.at(0)); auto &nb = M.adj[r];
    for (int j = 0; j < deg; ++j) nb.push_back(std::stoi(t.at(1 + j))); std::sort(nb.begin(), nb.end());
  }
  M.my_hq = hq_of(M, M.my_side); M.opp_hq = hq_of(M, opposite(M.my_side));
  S = GameState{}; S.gold = START_GOLD; Side opp = opposite(M.my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    S.warriors.push_back(Warrior{.id = WarriorId{M.my_side, sfx}, .region = M.my_hq, .hp = HQ_LEVELS[1].warrior_hp});
    S.warriors.push_back(Warrior{.id = WarriorId{opp, sfx}, .region = M.opp_hq, .hp = HQ_LEVELS[1].warrior_hp});
  }
  S.buildings.push_back(Building{hq_of(M, Side::LEFT), Side::LEFT, BType::HQ, 1, HQ_LEVELS[1].hp});
  S.buildings.push_back(Building{hq_of(M, Side::RIGHT), Side::RIGHT, BType::HQ, 1, HQ_LEVELS[1].hp});
  std::cout << "OK" << std::endl;
}

static std::chrono::steady_clock::time_point g_turn_start;
static inline long long elapsed_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - g_turn_start)
      .count();
}

static bool read_turn_start(int &turn_index) {
  std::string line = readln(); if (line == "FINISH") return false;
  g_turn_start = std::chrono::steady_clock::now();
  auto t = tokens(line); turn_index = std::stoi(t.at(2)); return true;
}

static Building *find_building(GameState &S, int region) {
  for (auto &b : S.buildings) if (b.region == region) return &b; return nullptr;
}
static Warrior *find_warrior(GameState &S, WarriorId id) {
  for (auto &w : S.warriors) if (w.id == id) return &w; return nullptr;
}

struct ObsSide { int train_n = 0; std::vector<std::pair<int,int>> moves; std::vector<int> upgrades;
                 std::vector<std::pair<int,int>> hunger; };
struct ObsTurn { ObsSide side[2]; };

static void read_turn_result(GameState &S, const GameMap &M, const Actions &submitted,
                             ObsTurn *obs = nullptr) {

  std::set<int> allied_before;
  for (const auto &b : S.buildings) if (b.side == M.my_side) allied_before.insert(b.region);
  for (int region : submitted.upgrades) {
    Building *b = find_building(S, region);
    if (b == nullptr) { S.gold -= BASE_LEVELS[1].cost; S.buildings.push_back(make_base(region, M.my_side)); }
    else {
      if (b->level >= max_level(*b)) { int cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST; S.gold -= cost; b->hp = b->current_hp(); }
      else { S.gold -= upgrade_cost(*b); apply_upgrade(*b); }
    }
  }
  for (const auto &p : submitted.moves) {
    int cost = allied_before.count(p.second) ? 0 : MOVE_COST; S.gold -= cost;
    if (Warrior *w = find_warrior(S, p.first)) { w->state = WState::MOVING; w->target = p.second; }
  }
  S.gold -= TRAIN_COST * submitted.train_n;

  { std::string line = readln(); if (line == "FINISH") std::exit(0); }
  { auto t = tokens(readln()); S.my_countdown = std::stoi(t.at(2)); S.opp_countdown = std::stoi(t.at(4)); }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); Side s = parse_side_char(r.at(0)[0]); int region = std::stoi(r.at(1));
      if (obs) obs->side[s == Side::LEFT ? 0 : 1].upgrades.push_back(region);
      Building *b = find_building(S, region);
      if (b == nullptr) { S.buildings.push_back(make_base(region, s)); }
      else if (b->side != M.my_side) { if (b->level >= max_level(*b)) b->hp = b->current_hp(); else apply_upgrade(*b); }
    }
  }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    if (n > 0) {
      auto ids = tokens(readln());
      for (int i = 0; i < n; ++i) {
        WarriorId id = parse_warrior(ids.at(i)); int hq_region = hq_of(M, id.side); Building *hq_b = find_building(S, hq_region);
        S.next_id[id.side == Side::LEFT ? 0 : 1] = id.num + 1;
        if (obs) obs->side[id.side == Side::LEFT ? 0 : 1].train_n++;
        int hq_level = (hq_b != nullptr) ? hq_b->level : 1;
        S.warriors.push_back(Warrior{.id = id, .region = hq_region, .hp = HQ_LEVELS[hq_level].warrior_hp});
      }
    }
  }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); WarriorId id = parse_warrior(r.at(0)); int region = std::stoi(r.at(1));
      if (obs) obs->side[id.side == Side::LEFT ? 0 : 1].moves.push_back({id.num, region});
      if (Warrior *w = find_warrior(S, id)) {
        w->region = region; if (id.side == M.my_side && w->state == WState::MOVING && w->region == w->target) { w->state = WState::STATIONARY; }
      }
    }
  }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) { auto r = tokens(readln()); WarriorId id = parse_warrior(r.at(1)); int damage = std::stoi(r.at(2));
      if (obs && r.at(0) == "HUNGER") obs->side[id.side == Side::LEFT ? 0 : 1].hunger.push_back({id.num, damage});
      if (Warrior *w = find_warrior(S, id)) w->hp -= damage; }
    S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(), [](const Warrior &w) { return w.hp <= 0; }), S.warriors.end());
  }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) { auto r = tokens(readln()); int region = std::stoi(r.at(1)); int damage = std::stoi(r.at(2)); if (Building *b = find_building(S, region)) b->hp -= damage; }
    S.buildings.erase(std::remove_if(S.buildings.begin(), S.buildings.end(), [](const Building &b) { return b.hp <= 0; }), S.buildings.end());
  }
  (void)readln();

  int income = 0;
  for (const auto &b : S.buildings) {
    if (b.side != M.my_side) continue;
    int count = 0; for (const auto &w : S.warriors) { if (w.id.side == M.my_side && w.region == b.region) ++count; }
    income += WORK_INCOME * std::min(count, b.work_cap());
  }
  S.gold += income;
  int alive = 0; for (const auto &w : S.warriors) if (w.id.side == M.my_side) ++alive;
  S.gold = std::max(0, S.gold - UPKEEP_PER_WARRIOR * alive);
}

struct Paths { std::vector<std::vector<double>> dist; std::vector<std::vector<int>> nxt; };
static long long isqrt_ll(long long n) {
  if (n <= 0) return 0;
  long long r = (long long)std::sqrt((double)n);
  while (r > 0 && r * r > n) --r;
  while ((r + 1) * (r + 1) <= n) ++r;
  return r;
}
static double euclid_ceil(const GameMap &M, int u, int v) {
  long long dx = M.x[u] - M.x[v], dy = M.y[u] - M.y[v];
  long long d2 = dx * dx + dy * dy;
  long long r = isqrt_ll(d2);
  if (r * r < d2) ++r;
  return (double)r;
}
static Paths calculate_paths(const GameMap &M) {
  const double INF = std::numeric_limits<double>::infinity(); Paths P;
  P.dist.assign(M.N, std::vector<double>(M.N, INF)); P.nxt.assign(M.N, std::vector<int>(M.N, -1));
  for (int i = 0; i < M.N; ++i) { P.dist[i][i] = 0.0; P.nxt[i][i] = i; }
  for (int u = 0; u < M.N; ++u) { for (int v : M.adj[u]) { double w = euclid_ceil(M, u, v); if (w < P.dist[u][v]) P.dist[u][v] = w; } }
  for (int k = 0; k < M.N; ++k) {
    for (int u = 0; u < M.N; ++u) {
      if (P.dist[u][k] == INF) continue;
      for (int v = 0; v < M.N; ++v) { double cand = P.dist[u][k] + P.dist[k][v]; if (cand < P.dist[u][v]) P.dist[u][v] = cand; }
    }
  }
  for (int u = 0; u < M.N; ++u) {
    for (int v = 0; v < M.N; ++v) {
      if (u == v || P.dist[u][v] == INF) continue;
      double best_score = INF;
      for (int nb : M.adj[u]) {
        if (P.dist[nb][v] == INF) continue;
        double score = euclid_ceil(M, u, nb) + P.dist[nb][v];
        if (score < best_score) { best_score = score; P.nxt[u][v] = nb; }
      }
    }
  }
  return P;
}

static void emit_command() { std::cout << "COMMAND\n"; }
static void emit_actions(const Actions &a) {
  for (const auto &p : a.moves) std::cout << "MOVE " << format_warrior(p.first) << ' ' << p.second << '\n';
  for (int r : a.upgrades) std::cout << "UPGRADE " << r << '\n';
  if (a.train_n > 0) std::cout << "TRAIN " << a.train_n << '\n';
}
static void emit_end() { std::cout << "END" << std::endl; }

namespace fm {

constexpr int MAX_REGIONS = 109;

constexpr int MAX_WARRIORS = 640;

struct SimState {
  int gold[2] = {0, 0};
  int turn = 0;
  int next_num[2] = {1, 1};
  int nwar = 0;
  int8_t  w_side[MAX_WARRIORS];
  int16_t w_num[MAX_WARRIORS];
  int16_t w_region[MAX_WARRIORS];
  int16_t w_hp[MAX_WARRIORS];
  int8_t  w_moving[MAX_WARRIORS];
  int16_t w_target[MAX_WARRIORS];
  int8_t  b_present[MAX_REGIONS];
  int8_t  b_side[MAX_REGIONS];
  int8_t  b_type[MAX_REGIONS];
  int8_t  b_level[MAX_REGIONS];
  int16_t b_hp[MAX_REGIONS];
};

struct TurnPlan {
  int train_n = 0;
  int n_moves = 0; int16_t mv_num[MAX_WARRIORS]; int16_t mv_dest[MAX_WARRIORS];
  int n_upg = 0;   int16_t upg[MAX_REGIONS];
};

inline int b_ad(const SimState &s, int r) {
  return s.b_type[r] == 0 ? HQ_LEVELS[s.b_level[r]].turret : BASE_LEVELS[s.b_level[r]].turret;
}
inline int b_workcap(const SimState &s, int r) {
  return s.b_type[r] == 0 ? HQ_LEVELS[s.b_level[r]].work_cap : BASE_LEVELS[s.b_level[r]].work_cap;
}
inline int b_maxhp(int type, int lvl) { return type == 0 ? HQ_LEVELS[lvl].hp : BASE_LEVELS[lvl].hp; }
inline int b_maxlevel(int type) { return type == 0 ? HQ_MAX_LEVEL : BASE_MAX_LEVEL; }
inline int hq_region(const GameMap &M, int sd) { return sd == 0 ? 0 : M.N - 1; }

static void apply_attacks(SimState &s, int *idx, int n, int K, int build_region) {
  std::sort(idx, idx + n, [&](int x, int y) {
    if (s.w_hp[x] != s.w_hp[y]) return s.w_hp[x] < s.w_hp[y];
    return s.w_num[x] < s.w_num[y];
  });
  for (int i = 0; i < n && K > 0; ++i) {
    int h = s.w_hp[idx[i]];
    if (h <= 0) continue;
    if (K >= h) { K -= h; s.w_hp[idx[i]] = 0; }
    else { s.w_hp[idx[i]] = h - K; K = 0; }
  }
  if (K > 0 && build_region >= 0 && s.b_present[build_region] && s.b_hp[build_region] > 0) {
    s.b_hp[build_region] -= K;
    if (s.b_hp[build_region] < 0) s.b_hp[build_region] = 0;
  }
}

inline int combat_pool_naive(const int *hp, int n, int K, int build_hp, int *out_cnt) {
  static int tmp[MAX_WARRIORS];
  for (int i = 0; i < n; ++i) tmp[i] = hp[i];
  while (K > 0) {
    int best = -1;
    for (int i = 0; i < n; ++i)
      if (tmp[i] > 0 && (best < 0 || tmp[i] < tmp[best])) best = i;
    if (best < 0) break;
    --tmp[best]; --K;
  }
  for (int L = 0; L < 9; ++L) out_cnt[L] = 0;
  for (int i = 0; i < n; ++i) if (tmp[i] >= 1 && tmp[i] <= 8) out_cnt[tmp[i]]++;
  if (K > 0 && build_hp > 0) { build_hp -= K; if (build_hp < 0) build_hp = 0; }
  return build_hp;
}

inline int combat_pool_bucket(const int *hp, int n, int K, int build_hp, int *out_cnt) {
  int cnt[9] = {0};
  for (int i = 0; i < n; ++i) if (hp[i] > 0 && hp[i] <= 8) cnt[hp[i]]++;
  int L = 1;
  while (L <= 8 && K > 0) {
    if (cnt[L] == 0) { ++L; continue; }
    int killable = std::min(cnt[L], K / L);
    if (killable > 0) { cnt[L] -= killable; K -= killable * L; }
    if (cnt[L] > 0 && K > 0 && K < L) { cnt[L]--; cnt[L - K]++; K = 0; }
    else ++L;
  }
  for (int i = 0; i < 9; ++i) out_cnt[i] = cnt[i];
  if (K > 0 && build_hp > 0) { build_hp -= K; if (build_hp < 0) build_hp = 0; }
  return build_hp;
}

[[maybe_unused]] static void apply_turn(SimState &s, const GameMap &M, const Paths &P,
                       const TurnPlan &pa, const TurnPlan &pb) {

  static int8_t pre_present[MAX_REGIONS], pre_side[MAX_REGIONS];
  std::memcpy(pre_present, s.b_present, sizeof(int8_t) * M.N);
  std::memcpy(pre_side, s.b_side, sizeof(int8_t) * M.N);

  auto do_upgrades = [&](const TurnPlan &p, int sd) {
    for (int i = 0; i < p.n_upg; ++i) {
      int r = p.upg[i];
      if (!s.b_present[r]) {
        s.b_present[r] = 1; s.b_side[r] = (int8_t)sd; s.b_type[r] = 1;
        s.b_level[r] = 1; s.b_hp[r] = (int16_t)BASE_LEVELS[1].hp;
        s.gold[sd] -= BASE_LEVELS[1].cost;
      } else if (s.b_level[r] < b_maxlevel(s.b_type[r])) {
        int nl = s.b_level[r] + 1;
        s.gold[sd] -= (s.b_type[r] == 0 ? HQ_LEVELS[nl].upgrade_cost : BASE_LEVELS[nl].cost);
        s.b_level[r] = (int8_t)nl; s.b_hp[r] = (int16_t)b_maxhp(s.b_type[r], nl);
      } else {
        s.gold[sd] -= (s.b_type[r] == 0 ? HQ_HEAL_COST : BASE_HEAL_COST);
        s.b_hp[r] = (int16_t)b_maxhp(s.b_type[r], s.b_level[r]);
      }
    }
  };
  do_upgrades(pa, 0); do_upgrades(pb, 1);

  auto apply_move_cmds = [&](const TurnPlan &p, int sd) {
    for (int i = 0; i < p.n_moves; ++i) {
      int num = p.mv_num[i], dest = p.mv_dest[i];
      int idx = -1;
      for (int w = 0; w < s.nwar; ++w)
        if (s.w_side[w] == sd && s.w_num[w] == num) { idx = w; break; }
      if (idx < 0) continue;
      s.w_target[idx] = (int16_t)dest; s.w_moving[idx] = 1;
      int fee = (pre_present[dest] && pre_side[dest] == sd) ? 0 : MOVE_COST;
      s.gold[sd] -= fee;
    }
  };
  apply_move_cmds(pa, 0); apply_move_cmds(pb, 1);
  for (int w = 0; w < s.nwar; ++w) {
    if (!s.w_moving[w]) continue;
    int cur = s.w_region[w], tgt = s.w_target[w];
    if (cur == tgt) { s.w_moving[w] = 0; continue; }
    int nh = P.nxt[cur][tgt];
    if (nh >= 0) { s.w_region[w] = (int16_t)nh; if (nh == tgt) s.w_moving[w] = 0; }
  }

  auto do_train = [&](const TurnPlan &p, int sd) {
    int hq = hq_region(M, sd);
    int lvl = (s.b_present[hq] && s.b_side[hq] == sd && s.b_type[hq] == 0) ? s.b_level[hq] : 1;
    int hp = HQ_LEVELS[lvl].warrior_hp;
    for (int k = 0; k < p.train_n && s.nwar < MAX_WARRIORS; ++k) {
      int w = s.nwar++;
      s.w_side[w] = (int8_t)sd; s.w_num[w] = (int16_t)s.next_num[sd]++;
      s.w_region[w] = (int16_t)hq; s.w_hp[w] = (int16_t)hp;
      s.w_moving[w] = 0; s.w_target[w] = (int16_t)hq;
      s.gold[sd] -= TRAIN_COST;
    }
  };
  do_train(pa, 0); do_train(pb, 1);

  static int listA[MAX_WARRIORS], listB[MAX_WARRIORS];
  for (int r = 0; r < M.N; ++r) {
    int nA = 0, nB = 0;
    for (int w = 0; w < s.nwar; ++w) {
      if (s.w_region[w] != r) continue;
      if (s.w_side[w] == 0) listA[nA++] = w; else listB[nB++] = w;
    }
    bool hasB = s.b_present[r];
    int bAD = hasB ? b_ad(s, r) : 0;
    int adFromA = (hasB && s.b_side[r] == 0) ? bAD : 0;
    int adFromB = (hasB && s.b_side[r] == 1) ? bAD : 0;
    if (nA == 0 && nB == 0 && !hasB) continue;
    int attacks_on_B = adFromA + nA;
    int attacks_on_A = adFromB + nB;
    int enemyBuildForB = (hasB && s.b_side[r] == 1) ? r : -1;
    int enemyBuildForA = (hasB && s.b_side[r] == 0) ? r : -1;
    if (attacks_on_B > 0) apply_attacks(s, listB, nB, attacks_on_B, enemyBuildForB);
    if (attacks_on_A > 0) apply_attacks(s, listA, nA, attacks_on_A, enemyBuildForA);
  }

  { int j = 0;
    for (int w = 0; w < s.nwar; ++w) {
      if (s.w_hp[w] > 0) {
        if (j != w) { s.w_side[j]=s.w_side[w]; s.w_num[j]=s.w_num[w]; s.w_region[j]=s.w_region[w];
                      s.w_hp[j]=s.w_hp[w]; s.w_moving[j]=s.w_moving[w]; s.w_target[j]=s.w_target[w]; }
        ++j;
      }
    }
    s.nwar = j;
  }
  for (int r = 0; r < M.N; ++r) if (s.b_present[r] && s.b_hp[r] <= 0) s.b_present[r] = 0;

  for (int r = 0; r < M.N; ++r) {
    if (!s.b_present[r]) continue;
    int sd = s.b_side[r], cnt = 0;
    for (int w = 0; w < s.nwar; ++w) if (s.w_side[w] == sd && s.w_region[w] == r) ++cnt;
    s.gold[sd] += WORK_INCOME * std::min(cnt, b_workcap(s, r));
  }

  static int order[MAX_WARRIORS];
  for (int sd = 0; sd < 2; ++sd) {
    int m = 0;
    for (int w = 0; w < s.nwar; ++w) if (s.w_side[w] == sd) order[m++] = w;
    std::sort(order, order + m, [&](int x, int y) { return s.w_num[x] < s.w_num[y]; });
    for (int t = 0; t < m; ++t) {
      int w = order[t];
      if (s.gold[sd] >= UPKEEP_PER_WARRIOR) s.gold[sd] -= UPKEEP_PER_WARRIOR;
      else s.w_hp[w] -= 1;
    }
  }
  { int j = 0;
    for (int w = 0; w < s.nwar; ++w) {
      if (s.w_hp[w] > 0) {
        if (j != w) { s.w_side[j]=s.w_side[w]; s.w_num[j]=s.w_num[w]; s.w_region[j]=s.w_region[w];
                      s.w_hp[j]=s.w_hp[w]; s.w_moving[j]=s.w_moving[w]; s.w_target[j]=s.w_target[w]; }
        ++j;
      }
    }
    s.nwar = j;
  }

  s.turn++;
}

inline SimState from_tracker(const GameState &S, const GameMap &M) {
  SimState s{};
  s.turn = 0; s.nwar = 0; s.next_num[0] = s.next_num[1] = 1;

  s.gold[0] = s.gold[1] = 1 << 29;
  for (const auto &w : S.warriors) {
    if (s.nwar >= MAX_WARRIORS) break;
    int i = s.nwar++; int sd = (w.id.side == Side::LEFT) ? 0 : 1;
    s.w_side[i] = (int8_t)sd; s.w_num[i] = (int16_t)w.id.num; s.w_region[i] = (int16_t)w.region;
    s.w_hp[i] = (int16_t)w.hp; s.w_moving[i] = (w.state == WState::MOVING) ? 1 : 0;
    s.w_target[i] = (int16_t)w.target;
  }
  s.next_num[0] = S.next_id[0]; s.next_num[1] = S.next_id[1];
  for (int r = 0; r < M.N; ++r) s.b_present[r] = 0;
  for (const auto &b : S.buildings) {
    int r = b.region; s.b_present[r] = 1; s.b_side[r] = (b.side == Side::LEFT) ? 0 : 1;
    s.b_type[r] = (b.type == BType::HQ) ? 0 : 1; s.b_level[r] = (int8_t)b.level; s.b_hp[r] = (int16_t)b.hp;
  }
  return s;
}
inline TurnPlan to_plan(const ObsSide &o) {
  TurnPlan p; p.train_n = o.train_n;
  for (int u : o.upgrades) if (p.n_upg < MAX_REGIONS) p.upg[p.n_upg++] = (int16_t)u;
  for (auto &m : o.moves)
    if (p.n_moves < MAX_WARRIORS) { p.mv_num[p.n_moves] = (int16_t)m.first; p.mv_dest[p.n_moves] = (int16_t)m.second; ++p.n_moves; }
  return p;
}
inline void compact_dead(SimState &s) {
  int j = 0;
  for (int i = 0; i < s.nwar; ++i)
    if (s.w_hp[i] > 0) {
      if (j != i) { s.w_side[j]=s.w_side[i]; s.w_num[j]=s.w_num[i]; s.w_region[j]=s.w_region[i];
                    s.w_hp[j]=s.w_hp[i]; s.w_moving[j]=s.w_moving[i]; s.w_target[j]=s.w_target[i]; }
      ++j;
    }
  s.nwar = j;
}
inline int find_w(const SimState &s, int side, int num) {
  for (int i = 0; i < s.nwar; ++i) if (s.w_side[i] == side && s.w_num[i] == num) return i;
  return -1;
}

inline int reconcile_diff(const SimState &pred, const SimState &obs, const GameMap &M, bool verbose) {
  int mm = 0;
  for (int i = 0; i < pred.nwar; ++i) {
    int j = find_w(obs, pred.w_side[i], pred.w_num[i]);
    if (j < 0) { ++mm; if (verbose) std::cerr << "  RECON " << (pred.w_side[i] ? 'B' : 'A') << pred.w_num[i] << " pred-alive obs-gone\n"; continue; }
    if (pred.w_region[i] != obs.w_region[j] || pred.w_hp[i] != obs.w_hp[j]) {
      ++mm; if (verbose) std::cerr << "  RECON " << (pred.w_side[i] ? 'B' : 'A') << pred.w_num[i]
        << " reg " << pred.w_region[i] << "/" << obs.w_region[j] << " hp " << pred.w_hp[i] << "/" << obs.w_hp[j] << " (pred/obs)\n";
    }
  }
  for (int j = 0; j < obs.nwar; ++j)
    if (find_w(pred, obs.w_side[j], obs.w_num[j]) < 0) { ++mm; if (verbose) std::cerr << "  RECON " << (obs.w_side[j] ? 'B' : 'A') << obs.w_num[j] << " obs-alive pred-gone\n"; }
  for (int r = 0; r < M.N; ++r) {
    if (pred.b_present[r] != obs.b_present[r]) { ++mm; if (verbose) std::cerr << "  RECON bldg@" << r << " present " << (int)pred.b_present[r] << "/" << (int)obs.b_present[r] << "\n"; continue; }
    if (!pred.b_present[r]) continue;
    if (pred.b_side[r] != obs.b_side[r] || pred.b_type[r] != obs.b_type[r] || pred.b_level[r] != obs.b_level[r] || pred.b_hp[r] != obs.b_hp[r]) {
      ++mm; if (verbose) std::cerr << "  RECON bldg@" << r << " s/t/l/hp " << (int)pred.b_side[r] << (int)pred.b_type[r] << (int)pred.b_level[r] << ":" << pred.b_hp[r]
        << " / " << (int)obs.b_side[r] << (int)obs.b_type[r] << (int)obs.b_level[r] << ":" << obs.b_hp[r] << "\n";
    }
  }
  return mm;
}

}

namespace ev {

struct EvalWeights {
  double hq_hp    = 12.0;
  double material = 1.0;
  double income   = 0.8;
  double structv  = 0.4;
  double tech     = 0.3;
  double control  = 0.4;
  double dist     = 0.3;

};
constexpr EvalWeights WEIGHTS{};
constexpr double INF_SCORE = 1e15;

inline int terminal_winner(const fm::SimState &s, const GameMap &M, int me) {
  int opp = me ^ 1;
  int myhq = fm::hq_region(M, me), ophq = fm::hq_region(M, opp);
  bool myAlive = s.b_present[myhq] && s.b_side[myhq] == me  && s.b_type[myhq] == 0;
  bool opAlive = s.b_present[ophq] && s.b_side[ophq] == opp && s.b_type[ophq] == 0;
  if (!myAlive && !opAlive) return 0;
  if (!myAlive) return -1;
  if (!opAlive) return +1;
  if (s.turn >= MAX_TURN) {
    int mh = s.b_hp[myhq], oh = s.b_hp[ophq];
    if (mh > oh) return +1;
    if (mh < oh) return -1;
    return 0;
  }
  return 2;
}

inline double feat(const fm::SimState &s, const GameMap &M, const Paths &P, int sd) {
  int opp = sd ^ 1;
  int hq = fm::hq_region(M, sd), ehq = fm::hq_region(M, opp);
  int wcnt[fm::MAX_REGIONS];
  for (int r = 0; r < M.N; ++r) wcnt[r] = 0;
  double material = 0.0, mindist = 1e18;
  for (int w = 0; w < s.nwar; ++w) {
    if (s.w_side[w] != sd) continue;
    material += s.w_hp[w];
    int r = s.w_region[w];
    if (r >= 0 && r < M.N) {
      ++wcnt[r];
      double d = P.dist[r][ehq];
      if (d < mindist) mindist = d;
    }
  }
  bool hqOwn = s.b_present[hq] && s.b_side[hq] == sd && s.b_type[hq] == 0;
  double hq_hp = hqOwn ? (double)s.b_hp[hq] : 0.0;
  double tech  = hqOwn ? (double)s.b_level[hq] : 0.0;
  double income = 0.0, structv = 0.0, control = 0.0;
  for (int r = 0; r < M.N; ++r) {
    if (!s.b_present[r] || s.b_side[r] != sd) continue;
    int cap = fm::b_workcap(s, r);
    int workers = wcnt[r] < cap ? wcnt[r] : cap;
    income += (double)WORK_INCOME * workers;
    if (s.b_type[r] == 1) structv += s.b_level[r] * 4.0 + s.b_hp[r];
  }
  for (int r : M.strongholds)
    if (s.b_present[r] && s.b_side[r] == sd) control += 1.0;

  double distterm = 0.0;
  double scale = P.dist[hq][ehq];
  if (mindist < 1e17 && std::isfinite(scale) && scale > 0.0)
    distterm = -(mindist / scale);
  const EvalWeights &W = WEIGHTS;
  return W.hq_hp * hq_hp + W.material * material + W.income * income
       + W.structv * structv + W.tech * tech + W.control * control + W.dist * distterm;
}

inline double eval(const fm::SimState &s, const GameMap &M, const Paths &P, int me) {
  int t = terminal_winner(s, M, me);
  if (t == +1) return  INF_SCORE - s.turn;
  if (t == -1) return -INF_SCORE + s.turn;
  if (t == 0)  return 0.0;
  return feat(s, M, P, me) - feat(s, M, P, me ^ 1);
}

}

namespace cg {

struct Rng {
  unsigned long long s;
  explicit Rng(unsigned long long seed = 0x9E3779B97F4A7C15ull) : s(seed ? seed : 1) {}
  inline unsigned long long next() {
    s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
    return s * 0x2545F4914F6CDD1Dull;
  }
  inline int below(int n) { return n <= 0 ? 0 : (int)(next() % (unsigned)n); }
};

inline int find_slot(const fm::SimState &s, int sd, int num) {
  for (int w = 0; w < s.nwar; ++w) if (s.w_side[w] == sd && s.w_num[w] == num) return w;
  return -1;
}
inline int hq_level_pre(const fm::SimState &s, const GameMap &M, int sd) {
  int hq = fm::hq_region(M, sd);
  return (s.b_present[hq] && s.b_side[hq] == sd && s.b_type[hq] == 0) ? s.b_level[hq] : 1;
}

inline int move_fee(const fm::SimState &s, int sd, int dest) {
  return (s.b_present[dest] && s.b_side[dest] == sd) ? 0 : MOVE_COST;
}

inline void repair_plan(const fm::SimState &s, const GameMap &M, int sd,
                        const fm::TurnPlan &in, fm::TurnPlan &out) {
  out.train_n = 0; out.n_moves = 0; out.n_upg = 0;
  const int gold = s.gold[sd];
  int spent = 0;

  int mine[fm::MAX_REGIONS], enemy[fm::MAX_REGIONS];
  for (int r = 0; r < M.N; ++r) { mine[r] = 0; enemy[r] = 0; }
  for (int w = 0; w < s.nwar; ++w) {
    int r = s.w_region[w];
    if (r < 0 || r >= M.N) continue;
    if (s.w_side[w] == sd) ++mine[r]; else ++enemy[r];
  }

  static bool moved[fm::MAX_WARRIORS];
  for (int w = 0; w < s.nwar; ++w) moved[w] = false;
  for (int i = 0; i < in.n_moves; ++i) {
    int num = in.mv_num[i], dest = in.mv_dest[i];
    if (dest < 0 || dest >= M.N) continue;
    int w = find_slot(s, sd, num);
    if (w < 0) continue;
    if (s.w_moving[w]) continue;
    if (dest == s.w_region[w]) continue;
    if (moved[w]) continue;
    int fee = move_fee(s, sd, dest);
    if (spent + fee > gold) continue;
    spent += fee; moved[w] = true;
    out.mv_num[out.n_moves] = (int16_t)num; out.mv_dest[out.n_moves] = (int16_t)dest;
    ++out.n_moves;
  }

  static bool upped[fm::MAX_REGIONS];
  for (int r = 0; r < M.N; ++r) upped[r] = false;
  for (int i = 0; i < in.n_upg; ++i) {
    int r = in.upg[i];
    if (r < 0 || r >= M.N) continue;
    if (upped[r]) continue;
    if (mine[r] < 1 || enemy[r] > 0) continue;
    int cost;
    if (!s.b_present[r]) {
      if (!std::binary_search(M.strongholds.begin(), M.strongholds.end(), r)) continue;
      cost = BASE_LEVELS[1].cost;
    } else {
      if (s.b_side[r] != sd) continue;
      int type = s.b_type[r], lvl = s.b_level[r];
      if (lvl >= fm::b_maxlevel(type))
        cost = (type == 0) ? HQ_HEAL_COST : BASE_HEAL_COST;
      else
        cost = (type == 0) ? HQ_LEVELS[lvl + 1].upgrade_cost : BASE_LEVELS[lvl + 1].cost;
    }
    if (spent + cost > gold) continue;
    spent += cost; upped[r] = true;
    out.upg[out.n_upg++] = (int16_t)r;
  }

  if (in.train_n >= 1) {
    int cap = HQ_LEVELS[hq_level_pre(s, M, sd)].train_cap;
    int n = in.train_n < cap ? in.train_n : cap;
    while (n >= 1 && spent + TRAIN_COST * n > gold) --n;
    if (n >= 1) { out.train_n = n; spent += TRAIN_COST * n; }
  }
}

inline void random_plan(const fm::SimState &s, const GameMap &M, int sd, Rng &rng,
                        fm::TurnPlan &out) {
  fm::TurnPlan raw; raw.train_n = 0; raw.n_moves = 0; raw.n_upg = 0;
  for (int w = 0; w < s.nwar && raw.n_moves < fm::MAX_WARRIORS; ++w) {
    if (s.w_side[w] != sd || s.w_moving[w]) continue;
    if ((rng.next() & 1u) == 0) continue;
    int dest = rng.below(M.N);
    raw.mv_num[raw.n_moves] = s.w_num[w]; raw.mv_dest[raw.n_moves] = (int16_t)dest;
    ++raw.n_moves;
  }

  if (s.nwar > 0 && (rng.below(3) == 0)) {
    for (int tries = 0; tries < 3 && raw.n_upg < fm::MAX_REGIONS; ++tries) {
      int w = rng.below(s.nwar);
      if (s.w_side[w] == sd) { raw.upg[raw.n_upg++] = s.w_region[w]; break; }
    }
  }
  int cap = HQ_LEVELS[hq_level_pre(s, M, sd)].train_cap;
  raw.train_n = rng.below(cap + 1);
  repair_plan(s, M, sd, raw, out);
}

}

namespace rhea {

struct SearchCfg {
  int pop           = 20;
  long long deadline_ms = 50;
  int tournament_k  = 2;
  int explorers     = 3;
  int local_muts    = 2;
  int max_gens      = 4000;
  int endgame_turn  = 160;
  unsigned long long seed = 0xD1CE05EEDull;
};
constexpr SearchCfg CFG{};
constexpr int MAXP    = 32;
constexpr int MAX_NUM = 704;

static fm::TurnPlan g_pop[MAXP];
static fm::TurnPlan g_child[MAXP];
static double       g_fit[MAXP];
static double       g_cf[MAXP];

static int    g_last_gens = 0;
static double g_last_best = 0.0;
static double g_last_seed = 0.0;
static int    g_last_override = 0;

inline void actions_to_plan(const Actions &a, const GameMap &M, fm::TurnPlan &p) {
  (void)M;
  p.train_n = a.train_n; p.n_moves = 0; p.n_upg = 0;
  for (const auto &mv : a.moves)
    if (p.n_moves < fm::MAX_WARRIORS) { p.mv_num[p.n_moves] = (int16_t)mv.first.num; p.mv_dest[p.n_moves] = (int16_t)mv.second; ++p.n_moves; }
  for (int u : a.upgrades) if (p.n_upg < fm::MAX_REGIONS) p.upg[p.n_upg++] = (int16_t)u;
}
inline Actions plan_to_actions(const fm::TurnPlan &p, const GameMap &M) {
  Actions a; a.train_n = p.train_n;
  for (int i = 0; i < p.n_moves; ++i) a.moves.push_back({WarriorId{M.my_side, (int)p.mv_num[i]}, p.mv_dest[i]});
  for (int i = 0; i < p.n_upg; ++i) a.upgrades.push_back(p.upg[i]);
  return a;
}

inline fm::SimState build_state(const GameState &S, const GameMap &M, int turn) {
  fm::SimState s = fm::from_tracker(S, M);
  int me = (M.my_side == Side::LEFT) ? 0 : 1, opp = me ^ 1;
  s.gold[me]  = S.gold;
  s.gold[opp] = S.gold;
  s.turn = turn;
  return s;
}

inline void opp_policy(const fm::SimState &s, const GameMap &M, const Paths &P, int opp, fm::TurnPlan &out) {
  fm::TurnPlan raw; raw.train_n = 1; raw.n_moves = 0; raw.n_upg = 0;
  int myhq = fm::hq_region(M, opp ^ 1);
  for (int w = 0; w < s.nwar && raw.n_moves < fm::MAX_WARRIORS; ++w) {
    if (s.w_side[w] != opp || s.w_moving[w]) continue;
    int cur = s.w_region[w];
    if (cur == myhq) continue;
    int nh = P.nxt[cur][myhq];
    if (nh < 0 || nh == cur) continue;
    raw.mv_num[raw.n_moves] = s.w_num[w]; raw.mv_dest[raw.n_moves] = (int16_t)myhq; ++raw.n_moves;
  }
  cg::repair_plan(s, M, opp, raw, out);
}

inline double fitness(const fm::SimState &s0, const GameMap &M, const Paths &P, int me,
                      const fm::TurnPlan &my, const fm::TurnPlan &opp) {
  fm::SimState s = s0;
  if (me == 0) fm::apply_turn(s, M, P, my, opp);
  else         fm::apply_turn(s, M, P, opp, my);
  return ev::eval(s, M, P, me);
}

inline int plan_terminal(const fm::SimState &s0, const GameMap &M, const Paths &P, int me,
                         const fm::TurnPlan &my, const fm::TurnPlan &opp) {
  fm::SimState s = s0;
  if (me == 0) fm::apply_turn(s, M, P, my, opp);
  else         fm::apply_turn(s, M, P, opp, my);
  return ev::terminal_winner(s, M, me);
}

inline bool terminal_override(int seed_term, int best_term) {
  if (best_term == +1 && seed_term != +1) return true;
  if (seed_term == -1 && best_term != -1) return true;
  return false;
}

inline fm::SimState apply_plan(const fm::SimState &s0, const GameMap &M, const Paths &P, int me,
                               const fm::TurnPlan &my, const fm::TurnPlan &opp) {
  fm::SimState s = s0;
  if (me == 0) fm::apply_turn(s, M, P, my, opp);
  else         fm::apply_turn(s, M, P, opp, my);
  return s;
}

struct EndgameCfg {
  int def_radius = 2;
  int gold_slack = 100;
  long long wHQ = 10;
  long long wEHQ = 6;
  long long wEST = 1;
};
constexpr EndgameCfg ECFG{};

inline int eg_hq_hp(const fm::SimState &s, const GameMap &M, int sd) {
  int hq = fm::hq_region(M, sd);
  return (s.b_present[hq] && s.b_side[hq] == sd && s.b_type[hq] == 0) ? s.b_hp[hq] : 0;
}
inline int eg_struct(const fm::SimState &s, const GameMap &M, int sd) {
  int v = 0;
  for (int r = 0; r < M.N; ++r)
    if (s.b_present[r] && s.b_side[r] == sd && s.b_type[r] == 1) v += s.b_level[r] * 4 + s.b_hp[r];
  return v;
}
inline int eg_defenders(const fm::SimState &s, const GameMap &M, const Paths &P, int sd, int R) {
  int hq = fm::hq_region(M, sd), c = 0;
  for (int w = 0; w < s.nwar; ++w)
    if (s.w_side[w] == sd) { int r = s.w_region[w]; if (r >= 0 && r < M.N && P.dist[r][hq] <= (double)R) ++c; }
  return c;
}
inline long long eg_tiebreak(const fm::SimState &s, const GameMap &M, int me) {
  int opp = me ^ 1;
  return ECFG.wHQ  * eg_hq_hp(s, M, me)
       - ECFG.wEHQ * eg_hq_hp(s, M, opp)
       - ECFG.wEST * eg_struct(s, M, opp);
}

inline bool endgame_safe(const fm::SimState &ss, const fm::SimState &sb,
                         const GameMap &M, const Paths &P, int me) {
  if (eg_hq_hp(sb, M, me) < eg_hq_hp(ss, M, me)) return false;
  if (eg_defenders(sb, M, P, me, ECFG.def_radius)
        < eg_defenders(ss, M, P, me, ECFG.def_radius)) return false;
  if (sb.gold[me] < ss.gold[me] - ECFG.gold_slack) return false;
  return true;
}

inline bool endgame_override(const fm::SimState &ss, const fm::SimState &sb,
                             const GameMap &M, const Paths &P, int me) {
  return endgame_safe(ss, sb, M, P, me) && eg_tiebreak(sb, M, me) > eg_tiebreak(ss, M, me);
}

inline int tournament(cg::Rng &rng, int Pn) {
  int best = rng.below(Pn);
  for (int k = 1; k < CFG.tournament_k; ++k) { int c = rng.below(Pn); if (g_fit[c] > g_fit[best]) best = c; }
  return best;
}

inline void crossover(const fm::TurnPlan &A, const fm::TurnPlan &B, cg::Rng &rng, fm::TurnPlan &out) {
  out.n_moves = 0; out.n_upg = 0;
  out.train_n = (rng.next() & 1u) ? A.train_n : B.train_n;
  static bool usedn[MAX_NUM]; std::memset(usedn, 0, sizeof(usedn));
  auto take_moves = [&](const fm::TurnPlan &X) {
    for (int i = 0; i < X.n_moves; ++i) {
      int num = X.mv_num[i];
      if (num >= 0 && num < MAX_NUM && !usedn[num] && (rng.next() & 1u) && out.n_moves < fm::MAX_WARRIORS) {
        usedn[num] = true; out.mv_num[out.n_moves] = (int16_t)num; out.mv_dest[out.n_moves] = X.mv_dest[i]; ++out.n_moves;
      }
    }
  };
  take_moves(A); take_moves(B);
  static bool usedr[fm::MAX_REGIONS]; std::memset(usedr, 0, sizeof(usedr));
  auto take_upg = [&](const fm::TurnPlan &X) {
    for (int i = 0; i < X.n_upg; ++i) {
      int r = X.upg[i];
      if (r >= 0 && r < fm::MAX_REGIONS && !usedr[r] && (rng.next() & 1u) && out.n_upg < fm::MAX_REGIONS) {
        usedr[r] = true; out.upg[out.n_upg++] = (int16_t)r;
      }
    }
  };
  take_upg(A); take_upg(B);
}

inline void mutate(const fm::SimState &s, const GameMap &M, fm::TurnPlan &p, cg::Rng &rng, int sd) {
  switch (rng.below(6)) {
    case 0:
      if (p.n_moves > 0) { int i = rng.below(p.n_moves); p.mv_dest[i] = (int16_t)rng.below(M.N); }
      break;
    case 1:
      for (int t = 0; t < 4; ++t) { int w = rng.below(s.nwar);
        if (s.w_side[w] == sd && !s.w_moving[w]) {
          if (p.n_moves < fm::MAX_WARRIORS) { p.mv_num[p.n_moves] = s.w_num[w]; p.mv_dest[p.n_moves] = (int16_t)rng.below(M.N); ++p.n_moves; }
          break; }
      }
      break;
    case 2:
      if (p.n_moves > 0) { int i = rng.below(p.n_moves); p.mv_num[i] = p.mv_num[p.n_moves - 1]; p.mv_dest[i] = p.mv_dest[p.n_moves - 1]; --p.n_moves; }
      break;
    case 3: {
      int cap = HQ_LEVELS[cg::hq_level_pre(s, M, sd)].train_cap;
      int d = (rng.next() & 1u) ? 1 : -1; int nv = p.train_n + d;
      p.train_n = nv < 0 ? 0 : (nv > cap ? cap : nv);
      break; }
    case 4:
      for (int t = 0; t < 4; ++t) { int w = rng.below(s.nwar);
        if (s.w_side[w] == sd) { if (p.n_upg < fm::MAX_REGIONS) p.upg[p.n_upg++] = s.w_region[w]; break; }
      }
      break;
    case 5:
      if (p.n_upg > 0) { int i = rng.below(p.n_upg); p.upg[i] = p.upg[p.n_upg - 1]; --p.n_upg; }
      break;
  }
}

inline Actions search_turn(const GameState &S, const GameMap &M, const Paths &P, int turn, const Actions &heuristic) {
  int me = (M.my_side == Side::LEFT) ? 0 : 1, opp = me ^ 1;
  fm::SimState s0 = build_state(S, M, turn);
  fm::TurnPlan oppPlan; opp_policy(s0, M, P, opp, oppPlan);

  cg::Rng rng(CFG.seed ^ (0x9E3779B97F4A7C15ull * (unsigned long long)(turn + 1)));
  int Pn = CFG.pop < MAXP ? CFG.pop : MAXP;

  fm::TurnPlan seed0; actions_to_plan(heuristic, M, seed0);
  cg::repair_plan(s0, M, me, seed0, g_pop[0]);
  fm::TurnPlan seedPlan = g_pop[0];
  int explorers = CFG.explorers < Pn - 1 ? CFG.explorers : (Pn > 1 ? Pn - 1 : 0);
  int local_end = Pn - explorers;
  for (int i = 1; i < Pn; ++i) {
    if (i < local_end) {
      fm::TurnPlan tmp = seedPlan;
      int km = 1 + rng.below(CFG.local_muts);
      for (int k = 0; k < km; ++k) mutate(s0, M, tmp, rng, me);
      cg::repair_plan(s0, M, me, tmp, g_pop[i]);
    } else {
      cg::random_plan(s0, M, me, rng, g_pop[i]);
    }
  }

  if (elapsed_ms() >= CFG.deadline_ms) {
    g_last_gens = 0; g_last_seed = g_last_best = 0.0; g_last_override = 0;
    return plan_to_actions(seedPlan, M);
  }

  long long t_score0 = elapsed_ms();
  long long eval_ms = 0;
  int scored = 0;
  for (int i = 0; i < Pn; ++i) {
    long long e0 = elapsed_ms();
    g_fit[i] = fitness(s0, M, P, me, g_pop[i], oppPlan);
    long long de = elapsed_ms() - e0; if (de > eval_ms) eval_ms = de;
    ++scored;
    if (elapsed_ms() >= CFG.deadline_ms) break;
  }
  g_last_seed = g_fit[0];
  if (scored < Pn || elapsed_ms() >= CFG.deadline_ms) {
    g_last_gens = 0; g_last_best = g_fit[0]; g_last_override = 0;
    return plan_to_actions(seedPlan, M);
  }
  int best = 0; for (int i = 1; i < Pn; ++i) if (g_fit[i] > g_fit[best]) best = i;

  long long gen_ms = (elapsed_ms() - t_score0) * 3 / 2 + 1;
  int gens = 0;
  while (gens < CFG.max_gens && elapsed_ms() + 2 * gen_ms < CFG.deadline_ms) {
    long long g0 = elapsed_ms();
    double prev_best = g_fit[best];
    g_child[0] = g_pop[best]; g_cf[0] = g_fit[best];
    for (int i = 1; i < Pn; ++i) {
      int pa = tournament(rng, Pn), pb = tournament(rng, Pn);
      fm::TurnPlan c; crossover(g_pop[pa], g_pop[pb], rng, c);
      mutate(s0, M, c, rng, me);
      cg::repair_plan(s0, M, me, c, g_child[i]);
    }
    for (int i = 1; i < Pn; ++i) g_cf[i] = fitness(s0, M, P, me, g_child[i], oppPlan);
    for (int i = 0; i < Pn; ++i) { g_pop[i] = g_child[i]; g_fit[i] = g_cf[i]; }
    best = 0; for (int i = 1; i < Pn; ++i) if (g_fit[i] > g_fit[best]) best = i;
    assert(g_fit[best] >= prev_best - 1e-6);
    (void)prev_best;
    long long dt = elapsed_ms() - g0;
    if (dt > gen_ms) gen_ms = dt;
    ++gens;
  }
  g_last_gens = gens; g_last_best = g_fit[best];

  int seed_term = 2, best_term = 2;
  const fm::TurnPlan *chosen = &seedPlan;
  int eg_idx = -1;
  if (elapsed_ms() + 2 * eval_ms + 1 < CFG.deadline_ms) {
    fm::SimState ss = apply_plan(s0, M, P, me, seedPlan,    oppPlan);
    fm::SimState sb = apply_plan(s0, M, P, me, g_pop[best], oppPlan);
    seed_term = ev::terminal_winner(ss, M, me);
    best_term = ev::terminal_winner(sb, M, me);
    if (terminal_override(seed_term, best_term)) {
      chosen = &g_pop[best];
    } else if (turn >= CFG.endgame_turn) {

      long long best_tb = eg_tiebreak(ss, M, me);
      for (int i = 0; i < Pn; ++i) {
        if (elapsed_ms() + 2 * eval_ms + 1 >= CFG.deadline_ms) break;
        fm::SimState si = (i == best) ? sb : apply_plan(s0, M, P, me, g_pop[i], oppPlan);
        if (endgame_safe(ss, si, M, P, me)) {
          long long tb = eg_tiebreak(si, M, me);
          if (tb > best_tb) { best_tb = tb; eg_idx = i; }
        }
      }
      if (eg_idx >= 0) chosen = &g_pop[eg_idx];
    }
  }
  g_last_override = (chosen != &seedPlan) ? 1 : 0;

  DBG("[t" << turn << "] rhea gens=" << gens << " best=" << g_fit[best]
      << " seed=" << g_last_seed << " st=" << seed_term << " bt=" << best_term
      << " override=" << g_last_override << " eg=" << eg_idx << " el=" << elapsed_ms() << "\n");
  return plan_to_actions(*chosen, M);
}

}

class BotAgent {
public:
    const int RESERVE_GOLD_ATTACK = 50;
    const double TUNE_SCORE_REQ_WEIGHT = 10.0;
    const double TUNE_SCORE_DIST_WEIGHT = 1.0;
    const double TUNE_BASE_TARGET_BONUS = 60.0;
    const double TUNE_WEAK_BASE_BONUS = 150.0;
    const double TUNE_HQ_TARGET_BONUS = 0.0;
    const double TUNE_MAX_ARMY_RATIO = 0.7;
    const int TUNE_LABOR_ADVANTAGE_THRESHOLD = 3;
    const int TUNE_MIN_LABOR_TO_FIGHT = 3;
    const int TUNE_MAX_CONCURRENT_BUILDS = 2;

    int max_bases_to_build = 0;
    int max_bases_to_upgrade = 0;
    int custom_hq_max_level = HQ_MAX_LEVEL;
    int custom_base_max_level = BASE_MAX_LEVEL;
    bool done_building = false;

    std::map<WarriorId, int> emergency_targets;
    std::map<WarriorId, std::set<WarriorId>> predictive_defenders;
    int current_req_defenders = 0;
    int enemy_min_dist_to_hq = 999;

    struct AttackMission {
        int target = -1;
        int rally_point = -1;
        int required_attackers = 0;
        int target_arrival_turn = -1;
        int created_turn = 0;
        bool is_launched = false;
        std::set<WarriorId> squad_ids;
    };
    std::vector<AttackMission> active_missions;

    struct EnemyAttackGroup {
        std::set<WarriorId> ids;
        std::set<int> potential_targets;
        int current_region;
        int idle_turns;
    };
    std::vector<EnemyAttackGroup> active_enemy_attacks;
    std::map<WarriorId, int> last_enemy_pos;

    std::map<WarriorId, int> persistent_jobs;
    std::map<WarriorId, std::pair<int, int>> build_plans;
    std::map<int, int> virtual_job_slots;

    int virtual_gold = 0;
    int total_upkeep = 0;
    bool has_trained = false;
    int emergency_train_queue = 0;
    int pending_train_requests = 0;

    std::vector<Warrior> my_warriors, enemy_warriors;
    std::vector<Building> my_bases;
    const Building* hq_b = nullptr;
    const Building* opp_hq_b = nullptr;
    int enemy_bases_count = 0;

    int get_total_labor(const GameState& S, const GameMap& M) const {
        int labor = 0;
        for (const auto& b : S.buildings) {
            if (b.side == M.my_side) labor += b.work_cap();
        }
        return labor;
    }
    int get_enemy_total_labor(const GameState& S, const GameMap& M) const {
        int labor = 0;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) labor += b.work_cap();
        }
        return labor;
    }

    int get_hops(const Paths& P, int u, int v) const {
        if (u == v) return 0;
        int hops = 0; int curr = u;
        while (curr != v) { curr = P.nxt[curr][v]; if (curr == -1) return 999; hops++; }
        return hops;
    }

    int get_b_ad(const GameState& S, int region) const {
        for (const auto& b : S.buildings) {
            if (b.region == region) {
                if (b.type == BType::HQ) return HQ_LEVELS[b.level].turret;
                else return BASE_LEVELS[b.level].turret;
            }
        }
        return 0;
    }

    int calculate_min_defenders(int E, int enemy_troop_hp, int base_hp, int base_ad, int my_troop_hp) const {
        if (E <= 0) return 0;
        for (int D = 0; D <= E + 2; ++D) {
            int e_hp = E * enemy_troop_hp;
            int m_hp = D * my_troop_hp;
            int b_hp = base_hp;
            while (e_hp > 0 && b_hp > 0) {
                e_hp -= base_ad;
                if (e_hp <= 0) break;
                int cur_e = (e_hp + enemy_troop_hp - 1) / enemy_troop_hp;
                int cur_d = (m_hp > 0) ? ((m_hp + my_troop_hp - 1) / my_troop_hp) : 0;
                e_hp -= cur_d;
                int dmg_to_us = cur_e;
                if (m_hp >= dmg_to_us) {
                    m_hp -= dmg_to_us;
                } else {
                    int remaining = dmg_to_us - m_hp;
                    m_hp = 0;
                    b_hp -= remaining;
                }
            }
            if (b_hp > 0) return D;
        }
        return E;
    }

    int get_locked_gold() const {
        int locked = 0;
        for (const auto& p : build_plans) locked += p.second.second;
        return locked;
    }

    int get_spendable_gold() const {
        int emergency_cost = emergency_train_queue * TRAIN_COST;
        return virtual_gold - get_locked_gold() - emergency_cost;
    }

    std::vector<Warrior> get_free_units(const GameState& S) const {
        std::vector<Warrior> free_units;
        for (const auto& w : my_warriors) {
            bool in_mission = false;
            for (const auto& m : active_missions) {
                if (m.squad_ids.count(w.id)) { in_mission = true; break; }
            }
            if (!predictive_defenders.count(w.id) && !emergency_targets.count(w.id) &&
                !in_mission && !build_plans.count(w.id) && !persistent_jobs.count(w.id)) {
                free_units.push_back(w);
            }
        }
        return free_units;
    }

    bool is_safe_to_dispatch(const Warrior& w, int tgt, int current_center_free, const GameMap& M, const Paths& P) const {
        int center_after = current_center_free - ((w.region == M.center_region) ? 1 : 0);
        int trip_time = 2 * get_hops(P, w.region, tgt);
        int max_dist = get_hops(P, M.center_region, M.opp_hq);
        return (center_after >= current_req_defenders) || (trip_time < max_dist);
    }

    int calculate_req_attackers(int final_target, int dist_from_rp, const GameState& S, const GameMap& M) const {
        int my_hp = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        int b_hp = 0, b_ad = 0;
        int b_level = 1;
        bool is_hq = (final_target == M.opp_hq);

        for (const auto& b : S.buildings) {
            if (b.region == final_target && b.side != M.my_side) {
                b_hp = b.hp; b_ad = get_b_ad(S, final_target); b_level = b.level; break;
            }
        }

        int defending_hp = 0;
        for(const auto& ew : enemy_warriors) {
            if (ew.region == final_target) defending_hp += ew.hp;
        }

        int enemy_spawn_hp = 0;
        if (is_hq) {
            int spawn_cap = HQ_LEVELS[b_level].train_cap;
            int enemy_hp_per_unit = HQ_LEVELS[b_level].warrior_hp;
            int predicted_turns = dist_from_rp + 3;
            enemy_spawn_hp = predicted_turns * spawn_cap * enemy_hp_per_unit;
        }

        int total_hp_to_clear = defending_hp + b_hp + (b_ad * 3) + enemy_spawn_hp;
        int total_req = (total_hp_to_clear + my_hp - 1) / my_hp;
        return std::max(1, total_req);
    }

    void update_state_and_clean_dead(const GameState &S, const GameMap &M, const Paths &P) {
        my_warriors.clear(); enemy_warriors.clear(); my_bases.clear();
        hq_b = nullptr; opp_hq_b = nullptr;
        enemy_bases_count = 0;
        for (const auto& w : S.warriors) {
            if (w.id.side == M.my_side) my_warriors.push_back(w);
            else enemy_warriors.push_back(w);
        }
        for (const auto& b : S.buildings) {
            if (b.side == M.my_side) {
                if (b.type == BType::BASE) my_bases.push_back(b);
                else if (b.type == BType::HQ) hq_b = &b;
            } else if (b.side != M.my_side) {
                if (b.type == BType::HQ) opp_hq_b = &b;
                else if (b.type == BType::BASE) enemy_bases_count++;
            }
        }

        total_upkeep = my_warriors.size() * UPKEEP_PER_WARRIOR;
        virtual_gold = S.gold;
        has_trained = false;
        emergency_train_queue = 0;
        pending_train_requests = 0;

        std::set<WarriorId> alive_ids;
        for (auto& w : my_warriors) alive_ids.insert(w.id);

        for (auto& m : active_missions) {
            for (auto it = m.squad_ids.begin(); it != m.squad_ids.end(); ) {
                if (!alive_ids.count(*it)) it = m.squad_ids.erase(it); else ++it;
            }
        }
        for (auto it = emergency_targets.begin(); it != emergency_targets.end(); ) {
            if (!alive_ids.count(it->first)) it = emergency_targets.erase(it); else ++it;
        }
        for (auto it = predictive_defenders.begin(); it != predictive_defenders.end(); ) {
            if (!alive_ids.count(it->first)) it = predictive_defenders.erase(it); else ++it;
        }
        for (auto it = build_plans.begin(); it != build_plans.end(); ) {
            if (!alive_ids.count(it->first)) it = build_plans.erase(it); else ++it;
        }
        for (auto it = persistent_jobs.begin(); it != persistent_jobs.end(); ) {
            if (!alive_ids.count(it->first)) it = persistent_jobs.erase(it); else ++it;
        }

        virtual_job_slots.clear();
        if (hq_b) virtual_job_slots[M.my_hq] += hq_b->work_cap();
        for (const auto& b : my_bases) virtual_job_slots[b.region] += b.work_cap();

        done_building = true;
        if (my_bases.size() < 3) done_building = false;
        if (hq_b && hq_b->level < custom_hq_max_level) done_building = false;
    }

    void update_enemy_attack_groups(const GameState &S, const GameMap &M, const Paths &P) {
        std::map<std::pair<int, int>, std::set<WarriorId>> moves;
        for (const auto& ew : enemy_warriors) {
            if (last_enemy_pos.count(ew.id)) {
                int from = last_enemy_pos.at(ew.id);
                int to = ew.region;
                if (from != to) {
                    moves[{from, to}].insert(ew.id);
                }
            }
        }

        for (auto it = active_enemy_attacks.begin(); it != active_enemy_attacks.end(); ) {
            std::set<WarriorId> alive_ids;
            int most_common_region = -1;
            std::map<int, int> next_regions;
            int max_count = 0;

            for (const auto& id : it->ids) {
                for (const auto& ew : enemy_warriors) {
                    if (ew.id == id) {
                        alive_ids.insert(id);
                        next_regions[ew.region]++;
                        if (next_regions[ew.region] > max_count) {
                            max_count = next_regions[ew.region];
                            most_common_region = ew.region;
                        }
                        break;
                    }
                }
            }
            it->ids = alive_ids;

            if (it->ids.empty() || most_common_region == -1) {
                it = active_enemy_attacks.erase(it);
                continue;
            }

            if (most_common_region != it->current_region) {
                int from = it->current_region;
                int to = most_common_region;
                it->current_region = to;
                it->idle_turns = 0;

                for (auto tgt_it = it->potential_targets.begin(); tgt_it != it->potential_targets.end(); ) {
                    if (P.nxt[from][*tgt_it] != to && from != *tgt_it) {
                        tgt_it = it->potential_targets.erase(tgt_it);
                    } else {
                        ++tgt_it;
                    }
                }
            } else {
                it->idle_turns++;
            }

            if (it->idle_turns > 0 || it->potential_targets.empty()) {
                it = active_enemy_attacks.erase(it);
            } else {
                ++it;
            }
        }

        for (const auto& [path, ids] : moves) {
            if (ids.size() >= 4) {
                bool in_group = false;
                for (const auto& g : active_enemy_attacks) {
                    if (g.ids.count(*ids.begin())) { in_group = true; break; }
                }
                if (!in_group) {
                    EnemyAttackGroup g;
                    g.ids = ids;
                    g.current_region = path.second;
                    g.idle_turns = 0;
                    if (hq_b) g.potential_targets.insert(M.my_hq);
                    for (const auto& b : my_bases) g.potential_targets.insert(b.region);

                    for (auto tgt_it = g.potential_targets.begin(); tgt_it != g.potential_targets.end(); ) {
                        if (P.nxt[path.first][*tgt_it] != path.second && path.first != *tgt_it) {
                            tgt_it = g.potential_targets.erase(tgt_it);
                        } else {
                            ++tgt_it;
                        }
                    }
                    if (!g.potential_targets.empty()) {
                        active_enemy_attacks.push_back(g);
                    }
                }
            }
        }
    }

    void assign_predictive_defenders(const GameState &S, const GameMap &M, const Paths &P) {
        for (const auto& g : active_enemy_attacks) {
            int best_target = -1;
            int min_dist_enemy = 999;
            int min_dist_my_hq = 999;

            for (int tgt : g.potential_targets) {
                int d_e = get_hops(P, g.current_region, tgt);
                int d_m = get_hops(P, M.my_hq, tgt);
                if (d_e < min_dist_enemy || (d_e == min_dist_enemy && d_m < min_dist_my_hq)) {
                    min_dist_enemy = d_e;
                    min_dist_my_hq = d_m;
                    best_target = tgt;
                }
            }

            if (best_target != -1) {
                int e_hp_val = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
                int m_hp_val = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
                int b_hp = 0, b_ad = 0;
                for (const auto& b : S.buildings) {
                    if (b.region == best_target) {
                        b_hp = b.hp;
                        b_ad = get_b_ad(S, best_target);
                        break;
                    }
                }
                int req_def = calculate_min_defenders(g.ids.size(), e_hp_val, b_hp, b_ad, m_hp_val);

                int existing = 0;
                for (auto const& [my_id, e_ids] : predictive_defenders) {
                    if (emergency_targets.count(my_id) && emergency_targets.at(my_id) == best_target) {
                        for(auto id: e_ids) if(g.ids.count(id)) { existing++; break; }
                    }
                }

                int needed = req_def - existing;
                if (needed > 0) {
                    std::vector<Warrior> free_cands = get_free_units(S);
                    std::sort(free_cands.begin(), free_cands.end(), [&](const Warrior& a, const Warrior& b){
                        return get_hops(P, a.region, best_target) < get_hops(P, b.region, best_target);
                    });

                    int assigned = 0;
                    for (const auto& w : free_cands) {
                        if (assigned >= needed) break;
                        int d_my = get_hops(P, w.region, best_target);
                        if (d_my <= min_dist_enemy + 2) {
                            predictive_defenders[w.id] = g.ids;
                            emergency_targets[w.id] = best_target;
                            assigned++;
                        }
                    }
                    if (assigned < needed && best_target == M.my_hq) {
                        emergency_train_queue += (needed - assigned);
                    }
                }
            }
        }
    }

    void detect_and_handle_emergencies(const GameState &S, const GameMap &M, const Paths &P) {
        if (get_total_labor(S, M) < TUNE_MIN_LABOR_TO_FIGHT) return;

        std::vector<int> e_dists;
        for (auto& ew : enemy_warriors) e_dists.push_back(get_hops(P, ew.region, M.my_hq));
        if (!e_dists.empty()) {
            std::sort(e_dists.begin(), e_dists.end());
            enemy_min_dist_to_hq = e_dists[0];
        } else {
            enemy_min_dist_to_hq = 999;
        }

        update_enemy_attack_groups(S, M, P);

        for (auto it = predictive_defenders.begin(); it != predictive_defenders.end(); ) {
            WarriorId my_id = it->first;
            int my_target = emergency_targets.count(my_id) ? emergency_targets[my_id] : -1;
            bool still_valid = false;

            for (WarriorId e_id : it->second) {
                for (const auto& g : active_enemy_attacks) {
                    if (g.ids.count(e_id) && g.potential_targets.count(my_target)) {
                        still_valid = true; break;
                    }
                }
                if (still_valid) break;
            }
            if (!still_valid) {
                emergency_targets.erase(my_id);
                it = predictive_defenders.erase(it);
            } else {
                ++it;
            }
        }

        int opp_hq_labor_cap = opp_hq_b ? opp_hq_b->work_cap() : 1;
        int opp_hq_troop_count = 0;
        for (const auto& ew : enemy_warriors) {
            if (ew.region == M.opp_hq) opp_hq_troop_count++;
        }
        int baseline_invading_enemies = std::max(0, opp_hq_troop_count - opp_hq_labor_cap);

        int e_hp = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int m_hp = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        int hq_ad = get_b_ad(S, M.my_hq);
        int hq_hp = hq_b ? hq_b->hp : 10;
        current_req_defenders = calculate_min_defenders(baseline_invading_enemies, e_hp, hq_hp, hq_ad, m_hp);

        int center_free_count = 0;
        for (const auto& w : get_free_units(S)) {
            if (w.region == M.center_region) center_free_count++;
        }

        if (center_free_count < current_req_defenders) {
            emergency_train_queue += (current_req_defenders - center_free_count);
        }

        assign_predictive_defenders(S, M, P);
    }

    void plan_attacks(const GameState &S, const GameMap &M, const Paths &P, int turn) {
        int unfilled_jobs = 0;
        std::map<int, int> temp_jobs = virtual_job_slots;
        for (auto const& [wid, r] : persistent_jobs) if (temp_jobs[r] > 0) temp_jobs[r]--;
        for (auto const& [r, count] : temp_jobs) unfilled_jobs += count;

        auto get_attack_free_units = [&]() {
            std::vector<Warrior> current_free = get_free_units(S);
            std::vector<Warrior> filtered;
            std::set<int> reserved_sh;
            int jobs_to_reserve = unfilled_jobs;

            for (const auto& w : current_free) {
                bool reserve_this = false;
                for (int sh : M.strongholds) {
                    if (w.region == sh && !reserved_sh.count(sh)) {
                        bool has_b = false;
                        for(auto& b: S.buildings) if (b.region == sh) has_b = true;
                        if (!has_b) { reserved_sh.insert(sh); reserve_this = true; break; }
                    }
                }
                if (!reserve_this && jobs_to_reserve > 0) { reserve_this = true; jobs_to_reserve--; }
                if (!reserve_this) filtered.push_back(w);
            }
            return filtered;
        };

        auto get_rally_point = [&](int tgt) {
            int best_rp = M.my_hq;
            int min_d = get_hops(P, M.my_hq, tgt);
            for (const auto& b : my_bases) {
                int d = get_hops(P, b.region, tgt);
                if (d < min_d) { min_d = d; best_rp = b.region; }
            }
            return best_rp;
        };

        int current_center_free = 0;
        for (const auto& w : get_free_units(S)) if (w.region == M.center_region) current_center_free++;

        for (auto it = active_missions.begin(); it != active_missions.end(); ) {
            bool target_alive = (it->target == M.opp_hq);
            if (!target_alive) {
                for (auto& b : S.buildings) if (b.region == it->target && b.side != M.my_side) target_alive = true;
            }
            if (!target_alive || (it->is_launched && it->squad_ids.empty())) {
                it = active_missions.erase(it); continue;
            }

            if (it->is_launched) {
                ++it; continue;
            }

            if (!it->is_launched && turn - it->created_turn > 30) {
                it = active_missions.erase(it); continue;
            }

            int rp_dist_to_tgt = get_hops(P, it->rally_point, it->target);
            it->required_attackers = calculate_req_attackers(it->target, rp_dist_to_tgt, S, M);

            int current_squad_size = it->squad_ids.size();
            if (current_squad_size < it->required_attackers) {
                std::vector<Warrior> atk_units = get_attack_free_units();
                std::sort(atk_units.begin(), atk_units.end(), [&](const Warrior& a, const Warrior& b) {
                    return get_hops(P, a.region, it->rally_point) < get_hops(P, b.region, it->rally_point);
                });

                for (auto& w : atk_units) {
                    if (current_squad_size >= it->required_attackers) break;
                    if (!is_safe_to_dispatch(w, it->target, current_center_free, M, P)) continue;

                    it->squad_ids.insert(w.id);
                    current_squad_size++;
                    if (w.region == M.center_region) current_center_free--;
                }
                if (current_squad_size < it->required_attackers) {
                    pending_train_requests += (it->required_attackers - current_squad_size);
                }
            }

            int ready_at_rally = 0;
            for(auto wid : it->squad_ids) {
                for(auto& w : my_warriors) {
                    if(w.id == wid && get_hops(P, w.region, it->target) <= rp_dist_to_tgt) ready_at_rally++;
                }
            }

            if (ready_at_rally >= it->required_attackers) {
                it->is_launched = true;
                int max_d = 0;
                for(auto wid : it->squad_ids) {
                    for(auto& w : my_warriors) {
                        if(w.id == wid) max_d = std::max(max_d, get_hops(P, w.region, it->target));
                    }
                }
                it->target_arrival_turn = turn + max_d;
            }
            ++it;
        }

        int spendable_gold = get_spendable_gold();
        std::vector<Warrior> temp_check_units = get_attack_free_units();
        int idle_army_count = temp_check_units.size();

        bool can_attack = (get_total_labor(S, M) >= TUNE_MIN_LABOR_TO_FIGHT);
        if (can_attack && (done_building || spendable_gold >= 300 || idle_army_count >= 10)) {
            std::set<int> targeted;
            for (const auto& m : active_missions) targeted.insert(m.target);

            std::vector<int> potential_targets;
            if (!targeted.count(M.opp_hq)) potential_targets.push_back(M.opp_hq);
            for (const auto& b : S.buildings) {
                if (b.side != M.my_side && b.type == BType::BASE && !targeted.count(b.region)) {
                    potential_targets.push_back(b.region);
                }
            }

            struct TargetOption { int tgt; int req; double score; int rp; };
            std::vector<TargetOption> options;

            for (int tgt : potential_targets) {
                int rp = get_rally_point(tgt);
                int avg_dist = get_hops(P, rp, tgt);
                int current_req = calculate_req_attackers(tgt, avg_dist, S, M);

                double score = (current_req * TUNE_SCORE_REQ_WEIGHT) + (avg_dist * TUNE_SCORE_DIST_WEIGHT);

                if (tgt == M.center_region) score -= 999999.0;
                else if (tgt == M.opp_hq) score -= TUNE_HQ_TARGET_BONUS;
                else {
                    score -= TUNE_BASE_TARGET_BONUS;
                    if (current_req <= 10) score -= TUNE_WEAK_BASE_BONUS;
                }
                options.push_back({tgt, current_req, score, rp});
            }

            std::sort(options.begin(), options.end(), [](const TargetOption& a, const TargetOption& b) {
                return a.score < b.score;
            });

            int my_est_inc = WORK_INCOME * (1 + my_bases.size()) - total_upkeep;
            int max_potential_army = get_free_units(S).size() + (virtual_gold + std::max(0, my_est_inc) * 15) / TRAIN_COST;
            for (const auto& m : active_missions) max_potential_army += m.squad_ids.size();

            for (const auto& opt : options) {
                int max_allowed_req = std::max(60, (int)(max_potential_army * TUNE_MAX_ARMY_RATIO));
                if (opt.req > max_allowed_req) continue;

                std::vector<Warrior> atk_units = get_attack_free_units();
                int available = atk_units.size();
                if (available == 0 && spendable_gold < TRAIN_COST) break;

                int train_shortage = std::max(0, opt.req - available);
                int budget = (train_shortage * TRAIN_COST) + (done_building ? 0 : RESERVE_GOLD_ATTACK);

                if (spendable_gold >= budget || available >= opt.req) {
                    AttackMission m;
                    m.target = opt.tgt; m.rally_point = opt.rp; m.required_attackers = opt.req;
                    m.is_launched = false; m.created_turn = turn;

                    std::sort(atk_units.begin(), atk_units.end(), [&](const Warrior& a, const Warrior& b) {
                        return get_hops(P, a.region, opt.rp) < get_hops(P, b.region, opt.rp);
                    });

                    int drafted = 0;
                    for (auto& w : atk_units) {
                        if (drafted >= m.required_attackers) break;
                        if (!is_safe_to_dispatch(w, opt.tgt, current_center_free, M, P)) continue;

                        m.squad_ids.insert(w.id);
                        drafted++;
                        if (w.region == M.center_region) current_center_free--;
                    }

                    int ready_at_rally = 0;
                    int rp_dist_to_tgt = get_hops(P, m.rally_point, m.target);
                    for(auto wid : m.squad_ids) {
                        for(auto& w : my_warriors) {
                            if(w.id == wid && get_hops(P, w.region, m.target) <= rp_dist_to_tgt) ready_at_rally++;
                        }
                    }

                    if (ready_at_rally >= m.required_attackers) {
                        m.is_launched = true;
                        int max_d = 0;
                        for(auto wid : m.squad_ids) {
                            for(auto& w : my_warriors) {
                                if(w.id == wid) max_d = std::max(max_d, get_hops(P, w.region, m.target));
                            }
                        }
                        m.target_arrival_turn = turn + max_d;
                    } else {
                        pending_train_requests += train_shortage;
                    }
                    spendable_gold -= budget;
                    active_missions.push_back(m);
                }
            }
        }
    }

    bool is_stronghold_safe(const GameState& S, const GameMap& M, const Paths& P, int sh_region) const {
        int min_my = get_hops(P, M.my_hq, sh_region);
        for (auto& w : my_warriors) min_my = std::min(min_my, get_hops(P, w.region, sh_region));
        for (auto& b : my_bases) min_my = std::min(min_my, get_hops(P, b.region, sh_region));
        for (auto& w : enemy_warriors) {
            int dist_e = get_hops(P, w.region, sh_region);
            if (dist_e <= 2) return false;
            if (dist_e < min_my - 1) return false;
        }
        return true;
    }

    void plan_expansion(const GameState &S, const GameMap &M, const Paths &P) {
        if (get_total_labor(S, M) > get_enemy_total_labor(S, M) + TUNE_LABOR_ADVANTAGE_THRESHOLD) return;

        int current_builds = build_plans.size();
        if (current_builds >= TUNE_MAX_CONCURRENT_BUILDS) return;

        std::vector<Warrior> free_units = get_free_units(S);
        int current_center_free = 0;
        for (const auto& w : free_units) if (w.region == M.center_region) current_center_free++;

        struct PlanCandidate {
            int region; int cost; bool is_hq; bool is_new_base;
            int dist_to_my_hq; int priority;
            bool operator<(const PlanCandidate& o) const {
                if (is_new_base != o.is_new_base) return is_new_base > o.is_new_base;
                if (cost != o.cost) return cost < o.cost;
                if (priority != o.priority) return priority < o.priority;
                return dist_to_my_hq < o.dist_to_my_hq;
            }
        };

        std::vector<PlanCandidate> cands;

        int lv1_count = 0, lv2_count = 0;
        for (const auto& b : my_bases) {
            if (b.level == 1) lv1_count++;
            if (b.level == 2) lv2_count++;
        }
        int max_to_lv2 = lv1_count / 2 + 1;
        int max_to_lv3 = lv2_count / 2 + 1;

        int planned_to_lv2 = 0, planned_to_lv3 = 0;
        for (const auto& plan : build_plans) {
            for (const auto& b : my_bases) {
                if (b.region == plan.second.first) {
                    if (b.level == 1) planned_to_lv2++;
                    if (b.level == 2) planned_to_lv3++;
                }
            }
        }

        int future_base_count = my_bases.size();
        for (const auto& plan : build_plans) {
            bool is_new = true;
            for (const auto& b : my_bases) {
                if (b.region == plan.second.first) { is_new = false; break; }
            }
            if (is_new) future_base_count++;
        }

        for (int sh : M.strongholds) {
            bool has_b = false; for (const auto& bld : S.buildings) if (bld.region == sh) has_b = true;
            bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == sh) already_planned = true;
            bool unit_standing_here = false;
            for (const auto& w : free_units) { if (w.region == sh) { unit_standing_here = true; break; } }

            if (!has_b && !already_planned) {
                bool is_my_half = get_hops(P, sh, M.my_hq) < get_hops(P, sh, M.opp_hq);
                if (sh == M.center_region || is_my_half || is_stronghold_safe(S, M, P, sh) || unit_standing_here) {
                    PlanCandidate c; c.region = sh; c.cost = 300; c.is_hq = false; c.is_new_base = true;
                    c.dist_to_my_hq = get_hops(P, sh, M.my_hq);
                    if (future_base_count == 0) c.priority = 1; else c.priority = (sh == M.center_region) ? 1 : 2;
                    cands.push_back(c);
                }
            }
        }

        if (hq_b && hq_b->level < custom_hq_max_level) {
            bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == hq_b->region) enemy_present = true;
            bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == hq_b->region) already_planned = true;
            if (!already_planned && !enemy_present) {
                PlanCandidate c; c.region = hq_b->region; c.cost = HQ_LEVELS[hq_b->level + 1].upgrade_cost;
                c.is_hq = true; c.is_new_base = false; c.dist_to_my_hq = 0; c.priority = 1;
                cands.push_back(c);
            }
        }

        for (const auto& b : my_bases) {
            bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == b.region) enemy_present = true;
            bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == b.region) already_planned = true;

            if (!already_planned && !enemy_present) {
                if (b.level == 1 && planned_to_lv2 < max_to_lv2) {
                    PlanCandidate c; c.region = b.region; c.cost = BASE_LEVELS[2].cost;
                    c.is_hq = false; c.is_new_base = false; c.dist_to_my_hq = get_hops(P, b.region, M.my_hq);
                    c.priority = (b.region == M.center_region) ? 2 : 3;
                    cands.push_back(c); planned_to_lv2++;
                } else if (b.level == 2 && planned_to_lv3 < max_to_lv3) {
                    PlanCandidate c; c.region = b.region; c.cost = BASE_LEVELS[3].cost;
                    c.is_hq = false; c.is_new_base = false; c.dist_to_my_hq = get_hops(P, b.region, M.my_hq);
                    c.priority = (b.region == M.center_region) ? 2 : 3;
                    cands.push_back(c); planned_to_lv3++;
                }
            }
        }

        std::sort(cands.begin(), cands.end());
        int estimated_income = persistent_jobs.size() * WORK_INCOME - total_upkeep;

        for (const auto& cand : cands) {
            int base_budget = get_spendable_gold();
            if (free_units.empty()) {
                int D = cand.dist_to_my_hq;
                int projected_gold = base_budget + (D * estimated_income);
                int req_gold = cand.cost + TRAIN_COST + (D * MOVE_COST);
                if (projected_gold >= req_gold) pending_train_requests++;
                break;
            }

            int best_u = -1; int min_d = 999;
            for (size_t i = 0; i < free_units.size(); i++) {
                if (!is_safe_to_dispatch(free_units[i], cand.region, current_center_free, M, P)) continue;

                int d = get_hops(P, free_units[i].region, cand.region);
                if (d < min_d) { min_d = d; best_u = i; }
            }

            if (best_u != -1) {
                int D = min_d;
                int projected_real_gold = base_budget + (D * estimated_income);
                int required_gold = cand.cost + (D * MOVE_COST);

                if (projected_real_gold >= required_gold) {
                    build_plans[free_units[best_u].id] = {cand.region, cand.cost};
                    if (free_units[best_u].region == M.center_region) current_center_free--;
                    free_units.erase(free_units.begin() + best_u);

                    current_builds++;
                    if (current_builds >= TUNE_MAX_CONCURRENT_BUILDS) break;
                } else {
                    break;
                }
            }
        }
    }

    void process_build_plans(const GameState &S, const GameMap &M, const Paths &P, Actions &a) {
        for (auto it = build_plans.begin(); it != build_plans.end(); ) {
            Warrior w_copy; bool found_w = false;
            for (const auto& w : my_warriors) if (w.id == it->first) { w_copy = w; found_w = true; break; }
            if (!found_w) { it = build_plans.erase(it); continue; }

            int target_region = it->second.first;
            bool enemy_present = false;
            for (const auto& ew : enemy_warriors) {
                if (ew.region == target_region) { enemy_present = true; break; }
            }
            if (enemy_present) { it = build_plans.erase(it); continue; }

            Building* b = nullptr;
            for(auto& bld : S.buildings) if (bld.region == target_region) b = (Building*)&bld;

            bool is_my_base = (b && b->side == M.my_side);
            bool is_opp_base = (b && b->side != M.my_side);
            bool is_maxed = false;
            int actual_cost = 300;

            if (is_my_base) {
                is_maxed = (b->type == BType::HQ) ? (b->level >= custom_hq_max_level) : (b->level >= custom_base_max_level);
                if (!is_maxed) actual_cost = (b->type == BType::HQ) ? HQ_LEVELS[b->level + 1].upgrade_cost : BASE_LEVELS[b->level + 1].cost;
            }

            if (is_opp_base || is_maxed) { it = build_plans.erase(it); continue; }

            if (w_copy.region == target_region && w_copy.state == WState::STATIONARY) {
                if (virtual_gold >= actual_cost + total_upkeep) {
                    a.upgrades.push_back(target_region);
                    virtual_gold -= actual_cost;
                    int added_slots = (!is_my_base) ? BASE_LEVELS[1].work_cap :
                        (b->type == BType::HQ ? (HQ_LEVELS[b->level + 1].work_cap - HQ_LEVELS[b->level].work_cap) : (BASE_LEVELS[b->level + 1].work_cap - BASE_LEVELS[b->level].work_cap));

                    virtual_job_slots[target_region] += added_slots;
                    persistent_jobs[w_copy.id] = target_region;
                    it = build_plans.erase(it);
                } else ++it;
            } else ++it;
        }
    }

    void assign_jobs_to_free_units(const GameState &S, const GameMap &M, const Paths &P) {
        for (auto it = persistent_jobs.begin(); it != persistent_jobs.end(); ) {
            int job_region = it->second;
            if (virtual_job_slots.count(job_region) && virtual_job_slots[job_region] > 0) {
                virtual_job_slots[job_region]--; ++it;
            } else it = persistent_jobs.erase(it);
        }

        std::vector<Warrior> free_units = get_free_units(S);
        int current_center_free = 0;
        for (const auto& w : free_units) if (w.region == M.center_region) current_center_free++;

        std::vector<int> remaining_jobs;
        for (auto const& [r, count] : virtual_job_slots) {
            for (int i = 0; i < count; ++i) remaining_jobs.push_back(r);
        }

        while (!free_units.empty() && !remaining_jobs.empty()) {
            int best_w = -1, best_j = -1; int min_h = 9999;
            for (size_t i = 0; i < free_units.size(); ++i) {
                for (size_t j = 0; j < remaining_jobs.size(); ++j) {
                    if (!is_safe_to_dispatch(free_units[i], remaining_jobs[j], current_center_free, M, P)) continue;

                    int h = get_hops(P, free_units[i].region, remaining_jobs[j]);
                    if (h < min_h) { min_h = h; best_w = i; best_j = j; }
                }
            }
            if (best_w != -1) {
                persistent_jobs[free_units[best_w].id] = remaining_jobs[best_j];
                if (free_units[best_w].region == M.center_region) current_center_free--;
                free_units.erase(free_units.begin() + best_w);
                remaining_jobs.erase(remaining_jobs.begin() + best_j);
            } else {
                break;
            }
        }
        if (remaining_jobs.size() > 0) pending_train_requests += remaining_jobs.size();
    }

    void execute_training(const GameState &S, const GameMap &M, const Paths &P, Actions &a) {
        int cap = hq_b ? hq_b->train_cap() : 1;
        int spendable_gold = get_spendable_gold();

        if (!has_trained && emergency_train_queue > 0 && virtual_gold >= TRAIN_COST) {
            int act = std::min({emergency_train_queue, cap, virtual_gold / TRAIN_COST});
            if(act > 0) { a.train_n = act; virtual_gold -= act * TRAIN_COST; has_trained = true; }
        }
        else if (!has_trained && spendable_gold >= TRAIN_COST + total_upkeep) {
            int act = std::min({pending_train_requests, cap, (spendable_gold - total_upkeep) / TRAIN_COST});
            if(act > 0) { a.train_n = act; virtual_gold -= act * TRAIN_COST; has_trained = true; }
        }
    }

    void execute_movement(const GameState &S, const GameMap &M, const Paths &P, Actions &a, int turn) {
        std::vector<Warrior*> w_ptrs;
        for(auto& w : my_warriors) w_ptrs.push_back(&w);
        std::sort(w_ptrs.begin(), w_ptrs.end(), [&](Warrior* w1, Warrior* w2) {
            return get_hops(P, w1->region, M.opp_hq) < get_hops(P, w2->region, M.opp_hq);
        });

        for(auto& w_ptr : w_ptrs) {
            Warrior& w = *w_ptr;
            if(w.state == WState::MOVING) continue;

            int final_target = w.region;
            bool is_emergency = false;
            bool is_sync_attack = false;
            int target_arrival_turn = -1;

            if (predictive_defenders.count(w.id) || emergency_targets.count(w.id)) {
                final_target = emergency_targets[w.id];
                is_emergency = true;
            } else {
                bool in_mission = false;
                for (const auto& m : active_missions) {
                    if (m.squad_ids.count(w.id)) {
                        in_mission = true;
                        if (m.is_launched) {
                            final_target = m.target;
                            is_sync_attack = true;
                            target_arrival_turn = m.target_arrival_turn;
                        } else {
                            final_target = m.rally_point;
                        }
                        break;
                    }
                }
                if (!in_mission) {
                    if (build_plans.count(w.id)) {
                        final_target = build_plans[w.id].first;
                    } else if (persistent_jobs.count(w.id)) {
                        final_target = persistent_jobs[w.id];
                    } else {
                        bool center_is_mine = false;
                        for (const auto& b : my_bases) {
                            if (b.region == M.center_region) center_is_mine = true;
                        }

                        if (center_is_mine) {
                            final_target = M.center_region;
                        } else {
                            final_target = w.region;
                        }
                    }
                }
            }

            if(w.region != final_target) {
                if (is_sync_attack) {
                    int my_dist = get_hops(P, w.region, final_target);
                    int time_left = target_arrival_turn - turn;
                    if (time_left > my_dist && my_dist != 999) continue;
                }

                int cost = 0;
                bool final_is_my_base = false;
                for (const auto& b : S.buildings) if (b.region == final_target && b.side == M.my_side) final_is_my_base = true;
                if (!final_is_my_base) cost = MOVE_COST;

                if((is_emergency && virtual_gold >= cost) || (!is_emergency && virtual_gold >= cost + total_upkeep)) {
                    a.moves.push_back({w.id, final_target});
                    virtual_gold -= cost;
                }
            }
        }
    }

    Actions decide(const GameState &S, const GameMap &M, const Paths &P, int turn) {
        Actions a;
        update_state_and_clean_dead(S, M, P);

        plan_attacks(S, M, P, turn);
        detect_and_handle_emergencies(S, M, P);
        plan_expansion(S, M, P);

        process_build_plans(S, M, P, a);
        assign_jobs_to_free_units(S, M, P);
        execute_training(S, M, P, a);
        execute_movement(S, M, P, a, turn);

        last_enemy_pos.clear();
        for (const auto& ew : enemy_warriors) last_enemy_pos[ew.id] = ew.region;

        return a;
    }
};

static BotAgent bot_agent;
static Actions decide(const GameState &S, const GameMap &M, const Paths &P, int turn) {
    return bot_agent.decide(S, M, P, turn);
}

static Actions validate_actions(const Actions &in, const GameState &S, const GameMap &M) {
  Actions out;
  const int gold = S.gold;
  int spent = 0;

  std::set<WarriorId> moved;
  for (const auto &mv : in.moves) {
    const WarriorId id = mv.first;
    const int d = mv.second;
    const Warrior *w = nullptr;
    for (const auto &ww : S.warriors) if (ww.id == id) { w = &ww; break; }
    if (w == nullptr) continue;
    if (id.side != M.my_side) continue;
    if (w->state == WState::MOVING) continue;
    if (d < 0 || d >= M.N) continue;
    if (d == w->region) continue;
    if (moved.count(id)) continue;

    bool free_move = false;
    for (const auto &b : S.buildings)
      if (b.region == d && b.side == M.my_side) { free_move = true; break; }
    const int cost = free_move ? 0 : MOVE_COST;
    if (spent + cost > gold) continue;
    spent += cost; moved.insert(id); out.moves.push_back(mv);
  }

  std::set<int> upped;
  for (const int d : in.upgrades) {
    if (d < 0 || d >= M.N) continue;
    if (upped.count(d)) continue;
    int mine = 0, enemy = 0;
    for (const auto &w : S.warriors) {
      if (w.region != d) continue;
      if (w.id.side == M.my_side) ++mine; else ++enemy;
    }
    if (mine < 1 || enemy > 0) continue;
    const Building *b = nullptr;
    for (const auto &bb : S.buildings) if (bb.region == d) { b = &bb; break; }
    int cost;
    if (b == nullptr) {
      if (!std::binary_search(M.strongholds.begin(), M.strongholds.end(), d)) continue;
      cost = BASE_LEVELS[1].cost;
    } else {
      if (b->side != M.my_side) continue;
      if (b->level >= max_level(*b))
        cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST;
      else
        cost = upgrade_cost(*b);
    }
    if (spent + cost > gold) continue;
    spent += cost; upped.insert(d); out.upgrades.push_back(d);
  }

  if (in.train_n >= 1) {
    const Building *hq = nullptr;
    for (const auto &bb : S.buildings)
      if (bb.region == M.my_hq && bb.side == M.my_side) { hq = &bb; break; }
    const int cap = (hq != nullptr) ? HQ_LEVELS[hq->level].train_cap : HQ_LEVELS[1].train_cap;
    int n = std::min(in.train_n, cap);
    while (n >= 1 && spent + TRAIN_COST * n > gold) --n;
    if (n >= 1) { out.train_n = n; spent += TRAIN_COST * n; }
  }

  return out;
}

constexpr long long DEADLINE_MS = 80;

#ifndef FM_TEST_BUILD
int main() {
  GameMap M; GameState S; parse_init(M, S); Paths P = calculate_paths(M);
  int turn;
  [[maybe_unused]] int recon_reports = 0;
  while (read_turn_start(turn)) {

    Actions a, a_heur;
    try { a_heur = decide(S, M, P, turn); } catch (...) { a_heur = Actions{}; }
    try { a = rhea::search_turn(S, M, P, turn, a_heur); } catch (...) { a = a_heur; }
    a = validate_actions(a, S, M);
    emit_command(); emit_actions(a); emit_end();
    [[maybe_unused]] const std::size_t ncmd =
        a.moves.size() + a.upgrades.size() + (a.train_n > 0 ? 1u : 0u);
    DBG("[t" << turn << "] ms=" << elapsed_ms() << " ncmd=" << ncmd << "\n");
    if (elapsed_ms() > DEADLINE_MS)
      DBG("[t" << turn << "] WARN over budget (" << elapsed_ms() << " ms)\n");
#ifndef NDEBUG

    fm::SimState pre = fm::from_tracker(S, M);
    for (int i = 0; i < pre.nwar; ++i) pre.w_moving[i] = 0;
    ObsTurn obs;
    read_turn_result(S, M, a, &obs);
    fm::SimState post = fm::from_tracker(S, M);
    fm::TurnPlan pa = fm::to_plan(obs.side[0]), pb = fm::to_plan(obs.side[1]);
    fm::apply_turn(pre, M, P, pa, pb);

    for (int sd = 0; sd < 2; ++sd)
      for (auto &h : obs.side[sd].hunger) { int i = fm::find_w(pre, sd, h.first); if (i >= 0) pre.w_hp[i] -= h.second; }
    fm::compact_dead(pre);
    bool verbose = (recon_reports < 5);
    int mm = fm::reconcile_diff(pre, post, M, verbose);
    if (mm > 0) ++recon_reports;
    DBG("[t" << turn << "] reconcile mismatch=" << mm << "\n");
#else
    read_turn_result(S, M, a);
#endif
  }
  return 0;
}
#endif
