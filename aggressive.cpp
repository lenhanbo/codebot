#include <algorithm>
// Variant: two_base_capture_econ_v5
// Two early bases -> decisive nearest-base raids -> immediate capture of cleared strongholds -> light economy scaling.
#include <cassert>
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

// Variant: two-base nearest-base raid/capture with order-locked ownership.
// Main plan: secure two worked bases, then send complete compact squads into the
// closest enemy base, capture the cleared stronghold, and use it as staging.

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

// Variant: 7-win base + economy-lock scheduling.
// Minimal patch: preserve the proven mission/job system, but assign/fill labor
// jobs before drafting new attack missions so captured bases do not go empty.

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
  int N = 0, K = 0;
  std::vector<long long> x, y; std::vector<int> strongholds; std::vector<std::vector<int>> adj;
  Side my_side = Side::LEFT; int my_hq = 0; int opp_hq = 0;
};

struct GameState {
  int gold = START_GOLD; int opp_gold = START_GOLD; 
  int my_countdown = 5; int opp_countdown = 5; 
  std::vector<Warrior> warriors; std::vector<Building> buildings;
};

struct Actions {
  int train_n = 0; std::vector<std::pair<WarriorId, int>> moves; std::vector<int> upgrades;
};

// --- IO PARSING ---
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
  S = GameState{}; S.gold = START_GOLD; S.opp_gold = START_GOLD;
  Side opp = opposite(M.my_side);
  for (int sfx = 1; sfx <= START_WARRIORS; ++sfx) {
    S.warriors.push_back(Warrior{.id = WarriorId{M.my_side, sfx}, .region = M.my_hq, .hp = HQ_LEVELS[1].warrior_hp});
    S.warriors.push_back(Warrior{.id = WarriorId{opp, sfx}, .region = M.opp_hq, .hp = HQ_LEVELS[1].warrior_hp});
  }
  S.buildings.push_back(Building{hq_of(M, Side::LEFT), Side::LEFT, BType::HQ, 1, HQ_LEVELS[1].hp});
  S.buildings.push_back(Building{hq_of(M, Side::RIGHT), Side::RIGHT, BType::HQ, 1, HQ_LEVELS[1].hp});
  std::cout << "OK" << std::endl;
}

static bool read_turn_start(int &turn_index) {
  std::string line = readln(); if (line == "FINISH") return false;
  auto t = tokens(line); turn_index = std::stoi(t.at(2)); return true;
}

static Building *find_building(GameState &S, int region) {
  for (auto &b : S.buildings) if (b.region == region) return &b; return nullptr;
}
static Warrior *find_warrior(GameState &S, WarriorId id) {
  for (auto &w : S.warriors) if (w.id == id) return &w; return nullptr;
}

static void read_turn_result(GameState &S, const GameMap &M, const Actions &submitted) {
  for (int region : submitted.upgrades) {
    Building *b = find_building(S, region);
    if (b == nullptr) { S.gold -= BASE_LEVELS[1].cost; S.buildings.push_back(make_base(region, M.my_side)); } 
    else {
      if (b->level >= max_level(*b)) { int cost = (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST; S.gold -= cost; b->hp = b->current_hp(); } 
      else { S.gold -= upgrade_cost(*b); apply_upgrade(*b); }
    }
  }
  for (const auto &p : submitted.moves) {
    Building *b = find_building(S, p.second); int cost = (b != nullptr && b->side == M.my_side) ? 0 : MOVE_COST; S.gold -= cost;
    if (Warrior *w = find_warrior(S, p.first)) { w->state = WState::MOVING; w->target = p.second; }
  }
  S.gold -= TRAIN_COST * submitted.train_n;

  { std::string line = readln(); if (line == "FINISH") std::exit(0); }
  { auto t = tokens(readln()); S.my_countdown = std::stoi(t.at(2)); S.opp_countdown = std::stoi(t.at(4)); }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); Side s = parse_side_char(r.at(0)[0]); int region = std::stoi(r.at(1));
      Building *b = find_building(S, region);
      if (b == nullptr) { 
          S.buildings.push_back(make_base(region, s)); 
          if (s != M.my_side) S.opp_gold -= BASE_LEVELS[1].cost;
      } 
      else if (b->side != M.my_side) { 
          if (b->level >= max_level(*b)) { 
              S.opp_gold -= (b->type == BType::HQ) ? HQ_HEAL_COST : BASE_HEAL_COST; 
              b->hp = b->current_hp(); 
          } 
          else { 
              S.opp_gold -= upgrade_cost(*b); 
              apply_upgrade(*b); 
          } 
      }
    }
  }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    if (n > 0) {
      auto ids = tokens(readln());
      for (int i = 0; i < n; ++i) {
        WarriorId id = parse_warrior(ids.at(i)); int hq_region = hq_of(M, id.side); Building *hq_b = find_building(S, hq_region);
        int hq_level = (hq_b != nullptr) ? hq_b->level : 1;
        S.warriors.push_back(Warrior{.id = id, .region = hq_region, .hp = HQ_LEVELS[hq_level].warrior_hp});
        if (id.side != M.my_side) S.opp_gold -= TRAIN_COST;
      }
    }
  }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) {
      auto r = tokens(readln()); WarriorId id = parse_warrior(r.at(0)); int region = std::stoi(r.at(1));
      if (Warrior *w = find_warrior(S, id)) {
        w->region = region; if (id.side == M.my_side && w->state == WState::MOVING && w->region == w->target) { w->state = WState::STATIONARY; }
      }
    }
  }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) { auto r = tokens(readln()); WarriorId id = parse_warrior(r.at(1)); int damage = std::stoi(r.at(2)); if (Warrior *w = find_warrior(S, id)) w->hp -= damage; }
    S.warriors.erase(std::remove_if(S.warriors.begin(), S.warriors.end(), [](const Warrior &w) { return w.hp <= 0; }), S.warriors.end());
  }
  {
    auto t = tokens(readln()); int n = std::stoi(t.at(1));
    for (int i = 0; i < n; ++i) { auto r = tokens(readln()); int region = std::stoi(r.at(1)); int damage = std::stoi(r.at(2)); if (Building *b = find_building(S, region)) b->hp -= damage; }
    S.buildings.erase(std::remove_if(S.buildings.begin(), S.buildings.end(), [](const Building &b) { return b.hp <= 0; }), S.buildings.end());
  }
  (void)readln();

  int my_inc = 0, opp_inc = 0;
  for (const auto &b : S.buildings) {
    int count = 0; 
    for (const auto &w : S.warriors) { if (w.id.side == b.side && w.region == b.region) ++count; }
    int inc = WORK_INCOME * std::min(count, b.work_cap());
    if (b.side == M.my_side) my_inc += inc;
    else opp_inc += inc;
  }
  S.gold += my_inc;
  S.opp_gold += opp_inc;
  
  int my_alive = 0, opp_alive = 0;
  for (const auto &w : S.warriors) {
      if (w.id.side == M.my_side) ++my_alive;
      else ++opp_alive;
  }
  S.gold = std::max(0, S.gold - UPKEEP_PER_WARRIOR * my_alive);
  S.opp_gold = std::max(0, S.opp_gold - UPKEEP_PER_WARRIOR * opp_alive);
}

// --- PATHFINDING ---
struct Paths { std::vector<std::vector<double>> dist; std::vector<std::vector<int>> nxt; };
static double euclid_ceil(const GameMap &M, int u, int v) {
  double dx = (double)(M.x[u] - M.x[v]), dy = (double)(M.y[u] - M.y[v]); return std::ceil(std::sqrt(dx * dx + dy * dy));
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

class BotAgent {
public:
    const int RESERVE_GOLD_ATTACK = 50; 
    const double TUNE_SCORE_REQ_WEIGHT = 10.0;    
    const double TUNE_SCORE_DIST_WEIGHT = 1.0;    
    const double TUNE_BASE_TARGET_BONUS = 60.0;   
    const double TUNE_WEAK_BASE_BONUS = 150.0;    
    const double TUNE_HQ_TARGET_BONUS = 0.0;      
    const double TUNE_MAX_ARMY_RATIO = 0.7;       
    
    int max_bases_to_build = 0;
    int custom_hq_max_level = 1;    // no early HQ tech; spend tempo on raids/captures
    int custom_base_max_level = 2;  // allow light level-2 economy after raid/capture starts
    int current_turn = 0;
    bool done_building = false;     
    bool stop_building = false; 

    std::map<WarriorId, int> emergency_targets; 
    std::map<WarriorId, std::set<WarriorId>> predictive_defenders;
    std::set<WarriorId> core_defenders;
    int current_req_defenders = 0;
    int enemy_min_dist_to_hq = 999;

    struct EnemyAttackGroup {
        std::set<WarriorId> ids;
        std::set<int> potential_targets;
        int current_region = -1;
        int idle_turns = 0;
    };
    std::vector<EnemyAttackGroup> active_enemy_attacks;
    std::map<WarriorId, int> last_enemy_pos;
    
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

    // Strategy knobs for this variant:
    //   1) secure exactly two early bases, with only one worker per base;
    //   2) after that, switch to compact raid squads of ~4;
    //   3) capture destroyed bases lightly, but do not return to heavy economy mode.
    static constexpr int EARLY_BASE_GOAL = 2;
    static constexpr int LIGHT_EXPAND_LIMIT = 7;
    static constexpr int RAID_SQUAD_SIZE = 4;
    static constexpr int BASE_RAID_MAX = 8;
    static constexpr int HQ_MIN_ATTACKERS = 8;
    static constexpr int HQ_MAX_ATTACKERS = 16;
    static constexpr int MAX_ACTIVE_MISSIONS = 4;
    static constexpr int MAX_NEW_MISSIONS_PER_TURN = 2;

    int count_my_units_at(int region) const {
        int cnt = 0;
        for (const auto& w : my_warriors) if (w.region == region) cnt++;
        return cnt;
    }

    bool is_stronghold_region(const GameMap& M, int region) const {
        return std::find(M.strongholds.begin(), M.strongholds.end(), region) != M.strongholds.end();
    }

    bool has_any_building_at(const GameState& S, int region) const {
        for (const auto& b : S.buildings) if (b.region == region) return true;
        return false;
    }

    bool has_enemy_unit_at(const GameState& S, const GameMap& M, int region) const {
        for (const auto& w : S.warriors) if (w.id.side != M.my_side && w.region == region) return true;
        return false;
    }

    bool has_stationary_capturer_at(const GameState& S, const GameMap& M, int region) const {
        if (!is_stronghold_region(M, region)) return false;
        if (has_any_building_at(S, region)) return false;
        if (has_enemy_unit_at(S, M, region)) return false;
        for (const auto& w : my_warriors) {
            if (w.id.side == M.my_side && w.region == region && w.state == WState::STATIONARY &&
                !emergency_targets.count(w.id)) {
                return true;
            }
        }
        return false;
    }

    void release_from_non_emergency_owners(WarriorId id) {
        // Capture has higher priority than staging/job/finished raid bookkeeping.
        // Do not clear emergency ownership here; emergency should still win.
        for (auto& m : active_missions) m.squad_ids.erase(id);
        persistent_jobs.erase(id);
        build_plans.erase(id);
    }

    int count_enemy_bases(const GameState& S, const GameMap& M) const {
        int cnt = 0;
        for (const auto& b : S.buildings) if (b.side != M.my_side && b.type == BType::BASE) cnt++;
        return cnt;
    }

    int planned_new_base_count() const {
        int cnt = 0;
        for (const auto& [wid, plan] : build_plans) {
            bool exists = false;
            for (const auto& b : my_bases) if (b.region == plan.first) { exists = true; break; }
            if (!exists) cnt++;
        }
        return cnt;
    }

    bool has_two_worked_bases() const {
        int worked = 0;
        for (const auto& b : my_bases) {
            if (count_my_units_at(b.region) >= 1) worked++;
        }
        return (int)my_bases.size() >= EARLY_BASE_GOAL && worked >= EARLY_BASE_GOAL;
    }

    bool aggressive_ready() const {
        // Main plan is still two-base first, but do not wait forever if a worker
        // is walking slowly.  Job reservation will still protect the missing worker.
        int worked = 0;
        for (const auto& b : my_bases) if (count_my_units_at(b.region) >= 1) worked++;
        return (int)my_bases.size() >= EARLY_BASE_GOAL &&
               (worked >= EARLY_BASE_GOAL || (int)my_warriors.size() >= EARLY_BASE_GOAL + RAID_SQUAD_SIZE + 1 || current_turn >= 38);
    }

    int light_base_limit() const {
        return aggressive_ready() ? LIGHT_EXPAND_LIMIT : EARLY_BASE_GOAL;
    }

    int frontline_base_region(const GameMap& M, const Paths& P) const {
        int best = M.my_hq;
        int best_d = get_hops(P, M.my_hq, M.opp_hq);
        for (const auto& b : my_bases) {
            int d = get_hops(P, b.region, M.opp_hq);
            if (d < best_d) { best_d = d; best = b.region; }
        }
        return best;
    }

    bool is_in_active_mission(WarriorId id) const {
        for (const auto& m : active_missions) {
            if (m.squad_ids.count(id)) return true;
        }
        return false;
    }

    bool is_forward_committed(const Warrior& w, const GameMap& M, const Paths& P) const {
        if (!aggressive_ready()) return false;
        if (is_in_active_mission(w.id)) return true;
        if (build_plans.count(w.id) || persistent_jobs.count(w.id)) return true;

        int hq_to_opp = get_hops(P, M.my_hq, M.opp_hq);
        int w_to_opp = get_hops(P, w.region, M.opp_hq);
        int front = frontline_base_region(M, P);

        // Units that already reached the front half / staging base should not be
        // pulled back to HQ by weak predictive alarms.  This is the main fix for
        // the 52 <-> 0 / 47 <-> 0 oscillation seen in the logs.
        if (w.region == front && front != M.my_hq) return true;
        if (w_to_opp + 2 <= hq_to_opp) return true;

        for (const auto& b : my_bases) {
            if (w.region == b.region && get_hops(P, b.region, M.opp_hq) + 2 <= hq_to_opp) {
                return true;
            }
        }
        return false;
    }

    void cleanup_finished_missions(const GameState& S, const GameMap& M, int turn) {
        for (auto it = active_missions.begin(); it != active_missions.end(); ) {
            bool target_alive = (it->target == M.opp_hq);
            if (!target_alive) {
                for (const auto& b : S.buildings) {
                    if (b.region == it->target && b.side != M.my_side) { target_alive = true; break; }
                }
            }
            if (!target_alive || (it->is_launched && it->squad_ids.empty()) ||
                (!it->is_launched && turn - it->created_turn > 36)) {
                it = active_missions.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool is_enemy_base_target(const GameState& S, const GameMap& M, int region) const {
        for (const auto& b : S.buildings) {
            if (b.region == region && b.side != M.my_side && b.type == BType::BASE) return true;
        }
        return false;
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
        int hq_lock = 0;
        if (hq_b && opp_hq_b && hq_b->level < opp_hq_b->level && hq_b->level < custom_hq_max_level) {
            bool hq_planned = false;
            for (const auto& p : build_plans) {
                if (p.second.first == hq_b->region) hq_planned = true;
            }
            if (!hq_planned) hq_lock = HQ_LEVELS[hq_b->level + 1].upgrade_cost;
        }
        return virtual_gold - get_locked_gold() - hq_lock - emergency_cost;
    }

    std::vector<Warrior> get_free_units(const GameState& S) {
        std::vector<Warrior> free_units;
        for (const auto& w : my_warriors) {
            // A moving unit already has an implicit order from last turn.  Do not
            // let other systems re-own it before it arrives; otherwise the unit
            // may bounce between HQ defense and frontline staging.
            if (w.state == WState::MOVING) continue;
            bool in_mission = is_in_active_mission(w.id);
            if (!emergency_targets.count(w.id) && !predictive_defenders.count(w.id) && !in_mission &&
                !build_plans.count(w.id) && !persistent_jobs.count(w.id)) {
                free_units.push_back(w);
            }
        }
        return free_units;
    }

    int calculate_req_attackers(int final_target, const GameState& S, const GameMap& M, const Paths& P) const {
        int my_hp = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;

        int b_hp = 0, b_ad = 0;
        for (const auto& b : S.buildings) {
            if (b.region == final_target && b.side != M.my_side) {
                b_hp = b.hp; 
                b_ad = get_b_ad(S, final_target); 
                break;
            }
        }

        int defending_hp = 0;
        for(const auto& ew : enemy_warriors) {
            if (ew.region == final_target) {
                defending_hp += ew.hp;
            }
            else if (final_target == M.opp_hq) {
                int dist_to_hq = get_hops(P, ew.region, final_target);
                if (dist_to_hq <= 4) {
                    defending_hp += ew.hp;
                }
            }
        }

        // Ước tính lính viện trợ đẻ ra nhờ tiền của địch nếu đánh vào HQ địch
        if (final_target == M.opp_hq) {
            int opp_hp = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
            int max_train = S.opp_gold / TRAIN_COST;
            defending_hp += max_train * opp_hp;
        }

        int total_hp_to_clear = defending_hp + b_hp + (b_ad * 3);
        int total_req = (total_hp_to_clear + my_hp - 1) / my_hp;
        return std::max(1, total_req);
    }

    void update_state_and_clean_dead(const GameState &S, const GameMap &M, const Paths &P) {
        my_warriors.clear(); enemy_warriors.clear(); my_bases.clear();
        hq_b = nullptr; opp_hq_b = nullptr;
        
        for (const auto& w : S.warriors) { 
            if (w.id.side == M.my_side) my_warriors.push_back(w); 
            else enemy_warriors.push_back(w); 
        }
        
        int my_labor_income = 0;
        int opp_labor_income = 0;

        for (const auto& b : S.buildings) {
            if (b.side == M.my_side) { 
                if (b.type == BType::BASE) my_bases.push_back(b); 
                else if (b.type == BType::HQ) hq_b = &b; 
                
                int count = 0;
                for (const auto& w : my_warriors) if (w.region == b.region) count++;
                my_labor_income += WORK_INCOME * std::min(count, b.work_cap());
            } else if (b.side != M.my_side) {
                if (b.type == BType::HQ) opp_hq_b = &b;
                
                int count = 0;
                for (const auto& w : enemy_warriors) if (w.region == b.region) count++;
                opp_labor_income += WORK_INCOME * std::min(count, b.work_cap());
            }
        }
        
        // Điều kiện ngập tiền: Lao động phe ta lớn hơn lao động địch + 30
        stop_building = (my_labor_income > opp_labor_income + 30);

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
        // Light economy lock: one worker at HQ and one worker per base.
        // This intentionally frees surplus units for 4-man raids after the first two bases.
        if (hq_b) virtual_job_slots[M.my_hq] += std::min(1, hq_b->work_cap());
        for (const auto& b : my_bases) virtual_job_slots[b.region] += 1;

        max_bases_to_build = light_base_limit();
        
        // The opening objective is considered done only after we own two bases and
        // each of those bases has a worker.  After that, attack/raid mode starts.
        done_building = aggressive_ready();
    }


    void update_enemy_attack_groups(const GameState &S, const GameMap &M, const Paths &P) {
        std::map<std::pair<int, int>, std::set<WarriorId>> moves;
        for (const auto& ew : enemy_warriors) {
            if (last_enemy_pos.count(ew.id)) {
                int from = last_enemy_pos.at(ew.id);
                int to = ew.region;
                if (from != to) moves[{from, to}].insert(ew.id);
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
                        int c = ++next_regions[ew.region];
                        if (c > max_count) { max_count = c; most_common_region = ew.region; }
                        break;
                    }
                }
            }
            it->ids = alive_ids;
            if (it->ids.empty() || most_common_region == -1) { it = active_enemy_attacks.erase(it); continue; }

            if (most_common_region != it->current_region) {
                int from = it->current_region;
                int to = most_common_region;
                it->current_region = to;
                it->idle_turns = 0;
                for (auto tgt_it = it->potential_targets.begin(); tgt_it != it->potential_targets.end(); ) {
                    if (from != *tgt_it && P.nxt[from][*tgt_it] != to) tgt_it = it->potential_targets.erase(tgt_it);
                    else ++tgt_it;
                }
            } else {
                it->idle_turns++;
            }

            if (it->idle_turns > 1 || it->potential_targets.empty()) it = active_enemy_attacks.erase(it);
            else ++it;
        }

        // Backup defense from the 8-win code, but toned down: only treat a moving
        // blob of 4+ enemy units as an attack group. It will only consume free
        // surplus units, never workers/builders/raid squads.
        for (const auto& [path, ids] : moves) {
            if ((int)ids.size() < 4) continue;
            bool in_group = false;
            for (const auto& g : active_enemy_attacks) {
                for (const auto& id : ids) if (g.ids.count(id)) { in_group = true; break; }
                if (in_group) break;
            }
            if (in_group) continue;

            EnemyAttackGroup g;
            g.ids = ids;
            g.current_region = path.second;
            g.idle_turns = 0;
            if (hq_b) g.potential_targets.insert(M.my_hq);
            for (const auto& b : my_bases) {
                // Only defend meaningful bases predictively: worked bases or forward bases.
                bool worked = count_my_units_at(b.region) > 0;
                bool forward = get_hops(P, b.region, M.opp_hq) <= get_hops(P, M.my_hq, M.opp_hq) - 2;
                if (worked || forward) g.potential_targets.insert(b.region);
            }
            for (auto tgt_it = g.potential_targets.begin(); tgt_it != g.potential_targets.end(); ) {
                if (path.first != *tgt_it && P.nxt[path.first][*tgt_it] != path.second) tgt_it = g.potential_targets.erase(tgt_it);
                else ++tgt_it;
            }
            if (!g.potential_targets.empty()) active_enemy_attacks.push_back(g);
        }
    }

    void assign_predictive_defenders(const GameState &S, const GameMap &M, const Paths &P) {
        for (const auto& g : active_enemy_attacks) {
            int best_target = -1, min_dist_enemy = 999, min_dist_my_hq = 999;
            for (int tgt : g.potential_targets) {
                int d_e = get_hops(P, g.current_region, tgt);
                int d_m = get_hops(P, M.my_hq, tgt);
                if (d_e < min_dist_enemy || (d_e == min_dist_enemy && d_m < min_dist_my_hq)) {
                    min_dist_enemy = d_e; min_dist_my_hq = d_m; best_target = tgt;
                }
            }
            if (best_target == -1) continue;
            // Predictive defense is a backup, not an owner that may fight the
            // main raid/staging plan.  Only react when the blob is close.
            if (best_target == M.my_hq) {
                if (min_dist_enemy > 4) continue;
            } else {
                if (min_dist_enemy > 3) continue;
            }

            int e_hp_val = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
            int m_hp_val = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
            int b_hp = 10, b_ad = 1;
            for (const auto& b : S.buildings) if (b.region == best_target) { b_hp = b.hp; b_ad = get_b_ad(S, best_target); break; }
            int req_def = calculate_min_defenders((int)g.ids.size(), e_hp_val, b_hp, b_ad, m_hp_val);
            // Do not recreate the over-aggressive territorial patch. Cap defense
            // requests and only pull true free surplus units.
            if (best_target != M.my_hq) req_def = std::min(req_def, 2);
            else req_def = std::min(req_def, 3);

            int assigned_existing = 0;
            for (auto const& [wid, tgt] : emergency_targets) if (tgt == best_target) assigned_existing++;
            int needed = std::max(0, req_def - assigned_existing);
            if (needed <= 0) continue;

            std::vector<Warrior> free_cands = get_free_units(S);
            std::sort(free_cands.begin(), free_cands.end(), [&](const Warrior& a, const Warrior& b){
                return get_hops(P, a.region, best_target) < get_hops(P, b.region, best_target);
            });

            int assigned = 0;
            for (const auto& w : free_cands) {
                if (assigned >= needed) break;
                if (w.state == WState::MOVING) continue;
                if (is_forward_committed(w, M, P) && best_target == M.my_hq) continue;
                int d_my = get_hops(P, w.region, best_target);
                if (d_my <= min_dist_enemy + 1) {
                    predictive_defenders[w.id] = g.ids;
                    emergency_targets[w.id] = best_target;
                    assigned++;
                }
            }
            if (best_target == M.my_hq && assigned < needed) emergency_train_queue += (needed - assigned);
        }
    }

    void detect_and_handle_emergencies(const GameState &S, const GameMap &M, const Paths &P) {
        core_defenders.clear();
        emergency_targets.clear();
        predictive_defenders.clear();
        if (!hq_b) return;
        update_enemy_attack_groups(S, M, P);

        int enemy_labor_count = 0;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                int count = 0;
                for (const auto& ew : enemy_warriors) {
                    if (ew.region == b.region && get_hops(P, ew.region, M.my_hq) > 3) {
                        count++;
                    }
                }
                enemy_labor_count += std::min(count, b.work_cap());
            }
        }
        int total_e = enemy_warriors.size();
        int invading_enemies = std::max(0, total_e - enemy_labor_count);

        std::vector<int> e_dists;
        for (auto& ew : enemy_warriors) e_dists.push_back(get_hops(P, ew.region, M.my_hq));
        if (!e_dists.empty()) {
            std::sort(e_dists.begin(), e_dists.end());
            enemy_min_dist_to_hq = e_dists[0]; 
        } else {
            enemy_min_dist_to_hq = 999;
        }

        if (invading_enemies <= 0) {
            assign_predictive_defenders(S, M, P);
            return;
        }

        int e_hp = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int m_hp = HQ_LEVELS[hq_b->level].warrior_hp;
        int hq_ad = get_b_ad(S, M.my_hq);

        int req_defenders = calculate_min_defenders(invading_enemies, e_hp, hq_b->hp, hq_ad, m_hp);

        bool dire_hq_threat = (enemy_min_dist_to_hq <= 2 || invading_enemies >= std::max(8, (int)my_warriors.size() / 2));

        std::vector<Warrior> def_candidates;
        for (const auto& w : my_warriors) {
            if (w.state == WState::MOVING) continue;
            if (is_in_active_mission(w.id) && !dire_hq_threat) continue;

            bool is_labor_outside = false;
            if (persistent_jobs.count(w.id) && persistent_jobs.at(w.id) != M.my_hq) is_labor_outside = true;
            if (build_plans.count(w.id) && build_plans.at(w.id).first != M.my_hq) is_labor_outside = true;
            if (is_labor_outside) continue;

            // Do not recall a forward/staging unit to HQ unless HQ is truly in
            // danger.  This keeps captured-base pressure from fighting the
            // defensive fallback every few turns.
            if (!dire_hq_threat && is_forward_committed(w, M, P)) continue;

            def_candidates.push_back(w);
        }

        std::sort(def_candidates.begin(), def_candidates.end(), [&](const Warrior& a, const Warrior& b){
            return get_hops(P, a.region, M.my_hq) < get_hops(P, b.region, M.my_hq);
        });

        for (int i = 0; i < std::min((int)def_candidates.size(), req_defenders); ++i) {
            core_defenders.insert(def_candidates[i].id);
        }
        
        if ((int)def_candidates.size() < req_defenders) {
            emergency_train_queue += (req_defenders - def_candidates.size());
        }

        for (auto wid : core_defenders) {
            int my_dist_to_hq = 0;
            for (auto& w : my_warriors) {
                if (w.id == wid) {
                    my_dist_to_hq = get_hops(P, w.region, M.my_hq);
                    break;
                }
            }
            if (dire_hq_threat && enemy_min_dist_to_hq <= my_dist_to_hq + 1) {
                emergency_targets[wid] = M.my_hq; 
            }
        }

        assign_predictive_defenders(S, M, P);
    }

    void plan_attacks(const GameState &S, const GameMap &M, const Paths &P, int turn) {
        int unfilled_jobs = 0;
        std::map<int, int> temp_jobs = virtual_job_slots;
        for (auto const& [wid, r] : persistent_jobs) if (temp_jobs[r] > 0) temp_jobs[r]--;
        for (auto const& [r, count] : temp_jobs) unfilled_jobs += count;
        
        auto get_attack_free_units = [&](int target_region) {
            std::vector<Warrior> free_for_atk;
            for (const auto& w : my_warriors) {
                if (w.state == WState::MOVING) continue;
                bool in_mission = is_in_active_mission(w.id);
                if (in_mission) continue;
                if (emergency_targets.count(w.id)) continue; 
                
                if (core_defenders.count(w.id)) {
                    int dist_from_target_to_hq = get_hops(P, target_region, M.my_hq);
                    if (dist_from_target_to_hq >= enemy_min_dist_to_hq - 1) {
                        continue; 
                    }
                }

                if (!build_plans.count(w.id) && !persistent_jobs.count(w.id)) {
                    free_for_atk.push_back(w);
                }
            }
            
            std::vector<Warrior> filtered;
            std::set<int> reserved_sh;
            int jobs_to_reserve = unfilled_jobs;
            for (const auto& w : free_for_atk) {
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
                if (d < min_d) {
                    min_d = d;
                    best_rp = b.region;
                }
            }
            return best_rp;
        };

        for (auto it = active_missions.begin(); it != active_missions.end(); ) {
            bool target_alive = (it->target == M.opp_hq);
            if (!target_alive) {
                for (auto& b : S.buildings) if (b.region == it->target && b.side != M.my_side) target_alive = true; 
            }
            if (!target_alive || (it->is_launched && it->squad_ids.empty())) { 
                it = active_missions.erase(it); continue; 
            }
            
            if (it->is_launched) {
                // Once a squad is launched, keep ownership stable.  Do not peel
                // members back to HQ unless the unit was never supposed to be in
                // this mission (dead units are already cleaned in update_state).
                ++it; continue;
            }

            if (turn - it->created_turn > 36) {
                it = active_missions.erase(it); 
                continue;
            }
            
            it->required_attackers = calculate_req_attackers(it->target, S, M, P);
            if (it->target != M.opp_hq) {
                it->required_attackers = std::max(RAID_SQUAD_SIZE, std::min(it->required_attackers, BASE_RAID_MAX));
            } else {
                it->required_attackers = std::max(HQ_MIN_ATTACKERS, std::min(it->required_attackers, HQ_MAX_ATTACKERS));
            }
            
            int current_squad_size = it->squad_ids.size();
            if (current_squad_size < it->required_attackers) {
                std::vector<Warrior> atk_units = get_attack_free_units(it->target);
                std::sort(atk_units.begin(), atk_units.end(), [&](const Warrior& a, const Warrior& b) {
                    return get_hops(P, a.region, it->rally_point) < get_hops(P, b.region, it->rally_point);
                });
                
                for (auto& w : atk_units) {
                    if (current_squad_size >= it->required_attackers) break;
                    it->squad_ids.insert(w.id); 
                    current_squad_size++;
                }
                if (current_squad_size < it->required_attackers) {
                    pending_train_requests += (it->required_attackers - current_squad_size);
                }
            }
            
            int ready_at_rally = 0;
            int rp_dist_to_tgt = get_hops(P, it->rally_point, it->target);
            for(auto wid : it->squad_ids) {
                for(auto& w : my_warriors) {
                    if(w.id == wid) {
                        if (get_hops(P, w.region, it->target) <= rp_dist_to_tgt) {
                            ready_at_rally++;
                        }
                    }
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
        std::vector<Warrior> temp_check_units = get_attack_free_units(M.opp_hq);
        int idle_army_count = temp_check_units.size(); 

        if (aggressive_ready() && (int)active_missions.size() < MAX_ACTIVE_MISSIONS &&
            (done_building || spendable_gold >= 300 || idle_army_count >= RAID_SQUAD_SIZE)) {
            std::set<int> targeted;
            for (const auto& m : active_missions) targeted.insert(m.target);

            int enemy_base_cnt = count_enemy_bases(S, M);
            bool allow_hq_pressure = (enemy_base_cnt <= 1 || turn >= 135 || idle_army_count >= 16);

            std::vector<int> potential_targets;
            // Prefer compact base raids first.  HQ pressure is allowed only after
            // base denial has worked or when we already have a large surplus army.
            for (const auto& b : S.buildings) {
                if (b.side != M.my_side && b.type == BType::BASE && !targeted.count(b.region)) {
                    potential_targets.push_back(b.region); 
                }
            }
            if (allow_hq_pressure && !targeted.count(M.opp_hq)) potential_targets.push_back(M.opp_hq);
            
            struct TargetOption { int tgt; int req; double score; int rp; };
            std::vector<TargetOption> options;

            for (int tgt : potential_targets) {
                int rp = get_rally_point(tgt);
                int current_req = calculate_req_attackers(tgt, S, M, P);
                if (tgt != M.opp_hq) {
                    current_req = std::max(RAID_SQUAD_SIZE, std::min(current_req, BASE_RAID_MAX));
                } else {
                    current_req = std::max(HQ_MIN_ATTACKERS, std::min(current_req, HQ_MAX_ATTACKERS));
                }
                int avg_dist = get_hops(P, rp, tgt); 
                
                double score = 0.0;
                if (tgt == M.opp_hq) {
                    score = current_req * 18.0 + avg_dist * 10.0 - 30.0;
                    if (turn >= 145) score -= 120.0;
                } else {
                    // Nearest-base-first raid.  Earlier versions over-weighted "any base"
                    // and sometimes walked past a close base.  Distance from the current
                    // forward rally point is the main sort key; required attackers is second.
                    score = avg_dist * 22.0 + current_req * 14.0;
                    if (avg_dist <= 3) score -= 90.0;
                    else if (avg_dist <= 5) score -= 45.0;
                    if (current_req <= RAID_SQUAD_SIZE + 1) score -= 25.0;
                }
                options.push_back({tgt, current_req, score, rp});
            }
            
            std::sort(options.begin(), options.end(), [](const TargetOption& a, const TargetOption& b) {
                return a.score < b.score;
            });

            int my_est_inc = WORK_INCOME * (1 + my_bases.size()) - total_upkeep;
            int max_potential_army = get_free_units(S).size() + (virtual_gold + std::max(0, my_est_inc) * 15) / TRAIN_COST;
            for (const auto& m : active_missions) max_potential_army += m.squad_ids.size();

            int new_missions = 0;
            for (const auto& opt : options) {
                if ((int)active_missions.size() >= MAX_ACTIVE_MISSIONS || new_missions >= MAX_NEW_MISSIONS_PER_TURN) break;
                int max_allowed_req = std::max(60, (int)(max_potential_army * TUNE_MAX_ARMY_RATIO));
                if (opt.req > max_allowed_req) continue; 
                
                std::vector<Warrior> atk_units = get_attack_free_units(opt.tgt); 
                int available = atk_units.size();
                if (available == 0 && spendable_gold < TRAIN_COST) break; 
                
                int train_shortage = std::max(0, opt.req - available);
                int budget = (train_shortage * TRAIN_COST);
                
                if (spendable_gold >= budget || available >= opt.req) {
                    AttackMission m;
                    m.target = opt.tgt;
                    m.rally_point = opt.rp;
                    m.required_attackers = opt.req;
                    m.is_launched = false;
                    m.created_turn = turn;
                    
                    std::sort(atk_units.begin(), atk_units.end(), [&](const Warrior& a, const Warrior& b) {
                        return get_hops(P, a.region, opt.rp) < get_hops(P, b.region, opt.rp);
                    });
                    
                    int drafted = 0;
                    for (auto& w : atk_units) {
                        if (drafted >= m.required_attackers) break;
                        m.squad_ids.insert(w.id); 
                        drafted++;
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
                    new_missions++;
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
        bool full_defense_mode = (opp_hq_b && hq_b && opp_hq_b->hp < hq_b->hp);
        int target_hq_level = custom_hq_max_level;
        if (current_turn >= 130 && opp_hq_b && opp_hq_b->level > target_hq_level) {
            target_hq_level = std::min(3, opp_hq_b->level);
        }
        
        bool urgent_hq = (current_turn >= 130 && hq_b && opp_hq_b && hq_b->level < opp_hq_b->level);
        
        if (stop_building && !urgent_hq) {
            target_hq_level = hq_b ? hq_b->level : 1; 
        }

        bool block_new_bases = urgent_hq; 
        std::vector<Warrior> free_units = get_free_units(S);

        // Mô hình Kim tự tháp: Tính toán base levels tĩnh & dự kiến từ build_plans
        int cur_lv1 = 0, cur_lv2 = 0, cur_lv3 = 0;
        for (const auto& b : my_bases) {
            if (b.level == 1) cur_lv1++;
            else if (b.level == 2) cur_lv2++;
            else if (b.level >= 3) cur_lv3++;
        }
        for (auto const& [wid, plan] : build_plans) {
            Building* pb = nullptr;
            for(auto& b : my_bases) if (b.region == plan.first) pb = (Building*)&b;
            if (!pb) { cur_lv1++; }
            else if (pb->level == 1) { cur_lv1--; cur_lv2++; }
            else if (pb->level == 2) { cur_lv2--; cur_lv3++; }
        }

        struct PlanCandidate {
            int region; int cost; bool is_hq; bool is_new_base; bool is_my_half;
            bool is_capture; 
            bool urgent_hq; 
            int dist_to_my_hq;
            bool operator<(const PlanCandidate& o) const {
                if (urgent_hq != o.urgent_hq) return urgent_hq > o.urgent_hq;    
                if (is_capture != o.is_capture) return is_capture > o.is_capture; 
                if (is_new_base != o.is_new_base) return is_new_base > o.is_new_base; 
                if (is_my_half != o.is_my_half) return is_my_half > o.is_my_half;     
                if (cost != o.cost) return cost < o.cost;
                return dist_to_my_hq < o.dist_to_my_hq;
            }
        };

        std::vector<PlanCandidate> cands;
        
        for (int sh : M.strongholds) {
            bool has_b = false; for (const auto& bld : S.buildings) if (bld.region == sh) has_b = true;
            bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == sh) already_planned = true;
            bool is_my_half = get_hops(P, sh, M.my_hq) < get_hops(P, sh, M.opp_hq);
            
            bool unit_standing_here = false;
            for (const auto& w : my_warriors) {
                if (w.region == sh && w.state == WState::STATIONARY && !emergency_targets.count(w.id)) {
                    unit_standing_here = true; break;
                }
            }

            int projected_bases = (int)my_bases.size() + planned_new_base_count();
            bool capture_site = aggressive_ready() && has_stationary_capturer_at(S, M, sh);
            bool under_base_limit = projected_bases < light_base_limit();
            // Before the 2-base opening is done: build normal safe bases.
            // After aggression starts: immediate capture gets first priority.  In
            // addition, allow ONE light side-economy step after the first raid window
            // if we have surplus, so the bot does not stay stuck on 2-base income.
            bool allow_opening_base = !aggressive_ready() && under_base_limit;
            bool allow_forward_capture = capture_site && projected_bases < light_base_limit();
            bool allow_safe_side_econ = aggressive_ready() && !capture_site && is_my_half &&
                                        current_turn >= 55 && projected_bases < std::min(light_base_limit(), 4) &&
                                        get_spendable_gold() >= 420;

            if (!has_b && !already_planned && ((!block_new_bases || capture_site) &&
                (allow_opening_base || allow_forward_capture || allow_safe_side_econ) && !full_defense_mode)) {
                if (is_my_half || is_stronghold_safe(S, M, P, sh) || capture_site) {
                    PlanCandidate c; c.region = sh; c.cost = 300; c.is_hq = false; c.is_new_base = true; c.is_my_half = is_my_half;
                    c.is_capture = capture_site;
                    c.urgent_hq = false; 
                    c.dist_to_my_hq = get_hops(P, sh, M.my_hq); 
                    cands.push_back(c);
                }
            }
        }

        // HQ Upgrade (luôn ưu tiên độc lập, không bị chặn bởi stop_building)
        if (hq_b && hq_b->level < target_hq_level) {
            bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == hq_b->region) enemy_present = true;
            bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == hq_b->region) already_planned = true;
            if (!already_planned && !enemy_present) {
                PlanCandidate c; c.region = hq_b->region; c.cost = HQ_LEVELS[hq_b->level + 1].upgrade_cost;
                c.is_hq = true; c.is_new_base = false; c.is_my_half = true; c.is_capture = false; 
                c.urgent_hq = urgent_hq; 
                c.dist_to_my_hq = 0; cands.push_back(c);
            }
        }

        std::vector<Building> sorted_bases = my_bases;
        std::sort(sorted_bases.begin(), sorted_bases.end(), [&](const Building& a, const Building& b) {
            return get_hops(P, a.region, M.my_hq) < get_hops(P, b.region, M.my_hq);
        });
        
        for (int i = 0; i < (int)sorted_bases.size(); ++i) {
            if (stop_building) break; // Bỏ qua nâng cấp base nếu ngập tiền
            const auto& b = sorted_bases[i];
            bool allow_light_base_upgrade = aggressive_ready() && current_turn >= 65 &&
                                            ((int)my_bases.size() + planned_new_base_count() >= 4) &&
                                            get_spendable_gold() >= 650;
            if (allow_light_base_upgrade && b.level < custom_base_max_level) {
                bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == b.region) enemy_present = true;
                bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == b.region) already_planned = true;
                if (!already_planned && !enemy_present) {
                    PlanCandidate c; c.region = b.region; c.cost = BASE_LEVELS[b.level + 1].cost;
                    c.is_hq = false; c.is_new_base = false; c.is_my_half = true; c.is_capture = false; c.urgent_hq = false;
                    c.dist_to_my_hq = get_hops(P, b.region, M.my_hq); cands.push_back(c);
                }
            }
        }
        
        std::sort(cands.begin(), cands.end());
        int estimated_income = persistent_jobs.size() * WORK_INCOME - total_upkeep;

        for (const auto& cand : cands) {
            // Kiểm tra quy tắc kim tự tháp (không áp dụng cho HQ)
            if (!cand.is_hq) {
                if (!cand.is_new_base) {
                    Building* b = nullptr;
                    for (auto& bld : my_bases) if (bld.region == cand.region) b = (Building*)&bld;
                    if (b) {
                        if (b->level == 1 && cur_lv2 >= (cur_lv1 / 2) + 1) continue; 
                        if (b->level == 2 && cur_lv3 >= (cur_lv2 / 2) + 1) continue; 
                    }
                }
            }

            int base_budget = cand.urgent_hq ? (virtual_gold - get_locked_gold()) : get_spendable_gold();

            WarriorId chosen_id;
            bool has_chosen = false;
            int D = 999;

            // Capture is special: the unit already standing on the cleared
            // stronghold should build immediately.  It may still be present in
            // old mission/job bookkeeping, so explicitly release that ownership.
            if (cand.is_capture) {
                for (const auto& w : my_warriors) {
                    if (w.region == cand.region && w.state == WState::STATIONARY &&
                        !emergency_targets.count(w.id)) {
                        chosen_id = w.id;
                        has_chosen = true;
                        D = 0;
                        break;
                    }
                }
            }

            if (!has_chosen) {
                if (free_units.empty()) {
                    int projected_gold = base_budget + (cand.dist_to_my_hq * estimated_income);
                    int req_gold = cand.cost + TRAIN_COST + (cand.dist_to_my_hq * MOVE_COST);
                    if (projected_gold >= req_gold) pending_train_requests++;
                    break; 
                }

                int best_u = -1; int min_d = 999;
                for (size_t i = 0; i < free_units.size(); i++) {
                    int d = get_hops(P, free_units[i].region, cand.region);
                    if (d < min_d) { min_d = d; best_u = i; }
                }
                chosen_id = free_units[best_u].id;
                has_chosen = true;
                D = min_d;
                free_units.erase(free_units.begin() + best_u);
            }

            int projected_real_gold = base_budget + (D * estimated_income);
            int required_gold = cand.cost + (D * MOVE_COST);
            
            if (has_chosen && projected_real_gold >= required_gold) {
                if (cand.is_capture) release_from_non_emergency_owners(chosen_id);
                build_plans[chosen_id] = {cand.region, cand.cost};
                
                // Cập nhật lại số lượng Base level dự kiến
                if (!cand.is_hq) {
                    if (cand.is_new_base) {
                        cur_lv1++;
                    } else {
                        Building* b = nullptr;
                        for (auto& bld : my_bases) if (bld.region == cand.region) b = (Building*)&bld;
                        if (b) {
                            if (b->level == 1) { cur_lv1--; cur_lv2++; }
                            else if (b->level == 2) { cur_lv2--; cur_lv3++; }
                        }
                    }
                }
            } else {
                break; 
            }
        }
    }

    void process_build_plans(const GameState &S, const GameMap &M, const Paths &P, Actions &a) {
        int target_hq_level = custom_hq_max_level;
        if (current_turn >= 130 && opp_hq_b && opp_hq_b->level > target_hq_level) {
            target_hq_level = std::min(3, opp_hq_b->level);
        }

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
                is_maxed = (b->type == BType::HQ) ? (b->level >= target_hq_level) : (b->level >= custom_base_max_level);
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
        std::vector<int> remaining_jobs;
        for (auto const& [r, count] : virtual_job_slots) {
            for (int i = 0; i < count; ++i) remaining_jobs.push_back(r);
        }

        while (!free_units.empty() && !remaining_jobs.empty()) {
            int best_w = -1, best_j = -1; int min_h = 9999;
            for (size_t i = 0; i < free_units.size(); ++i) {
                for (size_t j = 0; j < remaining_jobs.size(); ++j) {
                    int h = get_hops(P, free_units[i].region, remaining_jobs[j]);
                    if (h < min_h) { min_h = h; best_w = i; best_j = j; }
                }
            }
            persistent_jobs[free_units[best_w].id] = remaining_jobs[best_j];
            free_units.erase(free_units.begin() + best_w);
            remaining_jobs.erase(remaining_jobs.begin() + best_j);
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
            bool is_gathering = false;
            int target_arrival_turn = -1;
            
            if (emergency_targets.count(w.id)) {
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
                            is_gathering = true;
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
                        bool on_empty_stronghold = false;
                        if (aggressive_ready()) {
                            bool is_sh = std::find(M.strongholds.begin(), M.strongholds.end(), w.region) != M.strongholds.end();
                            bool has_b = false;
                            for (const auto& b : S.buildings) if (b.region == w.region) { has_b = true; break; }
                            if (is_sh && !has_b) on_empty_stronghold = true;
                        }

                        bool on_base = false;
                        for (const auto& b : my_bases) if (b.region == w.region) on_base = true;
                        if (hq_b && hq_b->region == w.region) on_base = true;
                        
                        if (on_empty_stronghold) {
                            // Hold the cleared base until we can afford UPGRADE instead
                            // of drifting back and losing the capture window.
                            final_target = w.region;
                        } else if (aggressive_ready()) {
                            int front = frontline_base_region(M, P);
                            // Idle/surplus units should reinforce the most forward owned base,
                            // not drift all the way back to HQ after a raid.
                            final_target = front;
                        } else if (on_base) final_target = w.region; 
                        else {
                            int nearest_base = M.my_hq; int min_d = get_hops(P, w.region, M.my_hq);
                            for (const auto& b : my_bases) {
                                int d = get_hops(P, w.region, b.region);
                                if (d < min_d) { min_d = d; nearest_base = b.region; }
                            }
                            final_target = nearest_base;
                        }
                    }
                }
            }
            
            if(w.region != final_target) {
                if (is_sync_attack) {
                    int my_dist = get_hops(P, w.region, final_target);
                    int time_left = target_arrival_turn - turn;
                    if (time_left > my_dist && my_dist != 999) {
                        continue; 
                    }
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
        current_turn = turn;
        update_state_and_clean_dead(S, M, P); 
        cleanup_finished_missions(S, M, turn);
        
        detect_and_handle_emergencies(S, M, P);
        
        plan_expansion(S, M, P);

        // Economy-lock scheduling:
        // The original 7-win bot planned attacks before assigning free units to
        // newly available labor slots. That is aggressive and often good, but the
        // plus variants over-amplified it: new/captured bases could be left empty
        // while the bot kept drafting and training for missions.  Execute arrived
        // build plans and assign persistent jobs BEFORE creating new missions, so
        // plan_attacks() can only draft true surplus units.  Existing missions are
        // still respected because get_free_units()/assign_jobs exclude squad_ids.
        process_build_plans(S, M, P, a);
        assign_jobs_to_free_units(S, M, P);

        plan_attacks(S, M, P, turn);
        
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

int main() {
  GameMap M; GameState S; parse_init(M, S); Paths P = calculate_paths(M); 
  int turn;
  while (read_turn_start(turn)) {
    Actions a = decide(S, M, P, turn);
    emit_command(); emit_actions(a); emit_end(); read_turn_result(S, M, a);
  }
  return 0;
}