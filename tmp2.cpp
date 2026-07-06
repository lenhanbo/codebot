#include <algorithm>
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
        int hq_level = (hq_b != nullptr) ? hq_b->level : 1;
        S.warriors.push_back(Warrior{.id = id, .region = hq_region, .hp = HQ_LEVELS[hq_level].warrior_hp});
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
    const double TUNE_BASE_TARGET_BONUS = 500.0;   
    const double TUNE_WEAK_BASE_BONUS = 150.0;    
    const double TUNE_HQ_TARGET_BONUS = 0.0;      
    const double TUNE_MAX_ARMY_RATIO = 0.9;       
    const int TUNE_GOLD_ADVANTAGE = 45;
    const int TUNE_MIN_LABOR_TO_FIGHT = 3; 
    const int TUNE_MIN_NET_INCOME_TO_ATTACK = 50; 
    const int TUNE_MAX_CONCURRENT_BUILDS = 2;
    const int TUNE_MAX_CONCURRENT_ATTACKS = 4;
    const double TUNE_ENEMY_IN_MY_HALF_BONUS = 100000.0;
    
    // [THÊM MỚI] Hai hằng số cho cơ chế Snowball
    const int TUNE_SNOWBALL_INCOME_GAP = 75;      // Chênh lệch Net Income để bật mode "Lấy thịt đè người"
    const int TUNE_SNOWBALL_BUFFER = 5;           // Lượng lính cộng thêm cho chắc cú khi snowball
    
    int max_bases_to_build = 0;
    int max_bases_to_upgrade = 0;
    int custom_hq_max_level = HQ_MAX_LEVEL;    
    int custom_base_max_level = BASE_MAX_LEVEL;  
    bool done_building = false;     
    std::set<int> doomed_bases; 
    bool is_rushing_hq = false;
    bool has_money_advantage = false;

    int opp_gold = START_GOLD;
    std::set<WarriorId> prev_enemy_ids;
    std::map<int, int> prev_enemy_bases;

    std::map<WarriorId, int> emergency_targets; 
    std::map<WarriorId, std::set<WarriorId>> predictive_defenders; 
    int current_req_defenders = 0;
    int enemy_min_dist_to_hq = 999;
    std::map<int, int> enemy_concentration_turns;
    
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
    
    enum class ThreatType { GATHERING, MOVING };
    struct EnemyAttackGroup {
        std::set<WarriorId> ids;
        std::set<int> potential_targets;
        int current_region;
        int idle_turns;
        ThreatType type = ThreatType::GATHERING; 
    };
    std::vector<EnemyAttackGroup> active_enemy_attacks;
    std::map<WarriorId, int> last_enemy_pos;

    std::map<WarriorId, int> persistent_jobs; 
    std::map<WarriorId, std::pair<int, int>> build_plans; 
    std::map<WarriorId, std::pair<int, int>> forward_build_plans; 
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

    int get_my_gross_income(const GameState& S, const GameMap& M) const {
        int income = 0;
        for (const auto& b : S.buildings) {
            if (b.side == M.my_side) {
                int count = 0;
                for (const auto& w : my_warriors) if (w.region == b.region) ++count;
                income += WORK_INCOME * std::min(count, b.work_cap());
            }
        }
        return income;
    }

    int get_my_net_income(const GameState& S, const GameMap& M, int simulated_warrior_count) const {
        return get_my_gross_income(S, M) - (simulated_warrior_count * UPKEEP_PER_WARRIOR);
    }

    int get_enemy_gross_income(const GameState& S, const GameMap& M) const {
        int income = 0;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                int count = 0;
                for (const auto& ew : enemy_warriors) if (ew.region == b.region) ++count;
                income += WORK_INCOME * std::min(count, b.work_cap());
            }
        }
        return income;
    }

    int get_enemy_net_income(const GameState& S, const GameMap& M, int simulated_enemy_warrior_count) const {
        return get_enemy_gross_income(S, M) - (simulated_enemy_warrior_count * UPKEEP_PER_WARRIOR);
    }

    void update_enemy_economy(const GameState& S, const GameMap& M) {
        bool is_first_turn = prev_enemy_ids.empty() && prev_enemy_bases.empty();
        
        int income = 0;
        std::set<WarriorId> curr_enemy_ids;
        std::map<int, int> curr_enemy_bases;

        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                curr_enemy_bases[b.region] = b.level;
                int count = 0;
                for (const auto& ew : S.warriors) {
                    if (ew.id.side != M.my_side && ew.region == b.region) ++count;
                }
                income += WORK_INCOME * std::min(count, b.work_cap());
            }
        }
        
        int upkeep = 0;
        int new_units = 0;
        int move_cost_est = 0;

        for (const auto& ew : S.warriors) {
            if (ew.id.side != M.my_side) {
                curr_enemy_ids.insert(ew.id);
                upkeep += UPKEEP_PER_WARRIOR;
                if (!is_first_turn && !prev_enemy_ids.count(ew.id)) new_units++;
                
                if (!is_first_turn && last_enemy_pos.count(ew.id) && last_enemy_pos[ew.id] != ew.region) {
                    bool target_is_base = false;
                    for (const auto& b : S.buildings) {
                        if (b.side != M.my_side && b.region == ew.region) { target_is_base = true; break; }
                    }
                    if (!target_is_base) move_cost_est += MOVE_COST;
                }
            }
        }

        int build_upgrade_cost = 0;
        if (!is_first_turn) {
            for (const auto& [reg, lvl] : curr_enemy_bases) {
                if (!prev_enemy_bases.count(reg)) build_upgrade_cost += BASE_LEVELS[1].cost;
                else if (lvl > prev_enemy_bases[reg]) {
                    if (reg == M.opp_hq) build_upgrade_cost += HQ_LEVELS[lvl].upgrade_cost;
                    else build_upgrade_cost += BASE_LEVELS[lvl].cost;
                }
            }
        }

        if (is_first_turn) {
            opp_gold = START_GOLD; 
        } else {
            int net_income = income - upkeep;
            opp_gold += net_income;
            opp_gold = std::max(0, opp_gold);
            opp_gold -= (new_units * TRAIN_COST);
            opp_gold -= move_cost_est;
            opp_gold -= build_upgrade_cost;
            opp_gold = std::max(0, opp_gold); 
        }

        prev_enemy_ids = curr_enemy_ids;
        prev_enemy_bases = curr_enemy_bases;
    }

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

    bool is_safe_against_all_in(const GameState &S, const GameMap &M, const Paths &P, 
                                int target_region, int build_cost, bool is_new_base, int d_build) {
        
        int max_t_wait = 35; 
        
        int total_enemy = enemy_warriors.size();
        int enemy_labor = 0;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                int count = 0;
                for (const auto& ew : enemy_warriors) if (ew.region == b.region) count++;
                enemy_labor += std::min(count, b.work_cap());
            }
        }

        int attacking_bases_count = 0;
        for (const auto& g : active_enemy_attacks) {
            if (g.potential_targets.find(M.my_hq) == g.potential_targets.end()) {
                attacking_bases_count += g.ids.size();
            }
        }

        int estimated_all_in = std::max(0, total_enemy - enemy_labor - attacking_bases_count - 1);
        int opp_hq_labor_cap = opp_hq_b ? opp_hq_b->work_cap() : 1;
        int opp_hq_troop_count = 0;
        for (const auto& ew : enemy_warriors) if (ew.region == M.opp_hq) opp_hq_troop_count++;
        int old_baseline = std::max(0, opp_hq_troop_count - opp_hq_labor_cap);
        
        int baseline_invading_enemies = std::max(estimated_all_in, old_baseline);
        
        int opp_train_cap = opp_hq_b ? opp_hq_b->train_cap() : 1;
        int opp_gross_inc = get_enemy_gross_income(S, M);

        int added_labor = 0;
        if (is_new_base) added_labor = BASE_LEVELS[1].work_cap;
        else {
            for (auto& b : S.buildings) if (b.region == target_region && b.side == M.my_side) {
                if (b.type == BType::HQ) added_labor = HQ_LEVELS[b.level + 1].work_cap - HQ_LEVELS[b.level].work_cap;
                else added_labor = BASE_LEVELS[b.level + 1].work_cap - BASE_LEVELS[b.level].work_cap;
                break;
            }
        }

        std::vector<int> bases_to_check;
        bases_to_check.push_back(M.my_hq);
        for (auto& b : my_bases) bases_to_check.push_back(b.region);
        if (std::find(bases_to_check.begin(), bases_to_check.end(), target_region) == bases_to_check.end()) {
            bases_to_check.push_back(target_region);
        }

        int e_hp_val = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int m_hp_val = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;

        for (int base_reg : bases_to_check) {
            int d_e = get_hops(P, M.opp_hq, base_reg);
            if (d_e == 999) continue;

            for (int t = 0; t <= max_t_wait; ++t) {
                int T_impact = t + d_e; 

                int sim_opp_gold = opp_gold;
                int sim_e_pool = baseline_invading_enemies; 
                int sim_total_enemy = total_enemy;     

                for (int i = 0; i < T_impact; ++i) {
                    if (i < t) {
                        int spawn = std::min(sim_opp_gold / TRAIN_COST, opp_train_cap);
                        sim_e_pool += spawn;
                        sim_total_enemy += spawn;
                        sim_opp_gold -= spawn * TRAIN_COST;
                    }

                    int sim_net_inc = opp_gross_inc - sim_total_enemy * UPKEEP_PER_WARRIOR;
                    if (sim_opp_gold + sim_net_inc >= 0) {
                        sim_opp_gold += sim_net_inc;
                    } else {
                        int deficit = -(sim_opp_gold + sim_net_inc);
                        sim_opp_gold = 0;
                        if (i >= t && sim_e_pool > 0) {
                            int starved_units = std::min(sim_e_pool, (deficit + 1) / 2); 
                            sim_e_pool = std::max(0, sim_e_pool - starved_units);
                            sim_total_enemy = std::max(0, sim_total_enemy - starved_units);
                        }
                    }
                }

                int b_hp = 10, b_ad = 0;
                if (base_reg == target_region) {
                    if (is_new_base) {
                        b_hp = BASE_LEVELS[1].hp; b_ad = BASE_LEVELS[1].turret;
                    } else {
                        for (auto& b : S.buildings) if (b.region == base_reg) {
                            b_hp = (b.type == BType::HQ) ? HQ_LEVELS[b.level + 1].hp : BASE_LEVELS[b.level + 1].hp;
                            b_ad = (b.type == BType::HQ) ? HQ_LEVELS[b.level + 1].turret : BASE_LEVELS[b.level + 1].turret;
                        }
                    }
                } else {
                    for (auto& b : S.buildings) if (b.region == base_reg) {
                        b_hp = b.hp; b_ad = get_b_ad(S, base_reg); break;
                    }
                }

                int req_def = calculate_min_defenders(sim_e_pool, e_hp_val, b_hp, b_ad, m_hp_val);
                if (req_def <= 0) continue; 

                int my_sim_gold = virtual_gold - build_cost; 
                int my_train_cap = hq_b ? hq_b->train_cap() : 1;
                int current_my_gross_inc = get_my_gross_income(S, M); 
                int total_my_units = my_warriors.size();
                
                int max_defenders = 0;
                std::vector<Warrior> free_units = get_free_units(S);
                for (const auto& w : free_units) {
                    if (get_hops(P, w.region, base_reg) <= T_impact) {
                        max_defenders++;
                    }
                }

                int hq_to_base_dist = get_hops(P, M.my_hq, base_reg);

                for (int i = 0; i < T_impact; ++i) {
                    if (i == d_build) {
                        current_my_gross_inc += added_labor * WORK_INCOME;
                    }

                    if (i >= 1) {
                        int spawn = std::min(my_sim_gold / TRAIN_COST, my_train_cap);
                        total_my_units += spawn;
                        my_sim_gold -= spawn * TRAIN_COST;
                        
                        if (i + 1 + hq_to_base_dist <= T_impact) {
                            max_defenders += spawn;
                        }
                    }
                    
                    int sim_my_net_inc = current_my_gross_inc - total_my_units * UPKEEP_PER_WARRIOR;
                    my_sim_gold += sim_my_net_inc;
                    my_sim_gold = std::max(0, my_sim_gold);
                }

                if (max_defenders < req_def) return false;
            }
        }
        return true; 
    }

    int get_locked_move_gold(const GameState& S, const GameMap& M) const {
        int locked = 0;
        for (const auto& m : active_missions) {
            for (auto wid : m.squad_ids) {
                bool found = false;
                int w_region = -1;
                for (const auto& w : my_warriors) {
                    if (w.id == wid) { w_region = w.region; found = true; break; }
                }
                // Nếu lính chưa tới đích, dự trữ sẵn 10 vàng MOVE_COST
                if (found && w_region != m.target) {
                    bool target_is_my_base = false;
                    for (const auto& b : S.buildings) {
                        if (b.region == m.target && b.side == M.my_side) { target_is_my_base = true; break; }
                    }
                    if (!target_is_my_base) {
                        locked += MOVE_COST;
                    }
                }
            }
        }
        return locked;
    }
    int get_locked_build_gold() const {
        int locked = 0;
        for (const auto& p : build_plans) locked += p.second.second;
        for (const auto& p : forward_build_plans) locked += p.second.second; 
        return locked;
    }
    int get_locked_gold(const GameState& S, const GameMap& M) const {
        return get_locked_build_gold() + get_locked_move_gold(S, M);
    }
    
    // [SỬA LẠI] Truyền thêm S, M vào hàm này
    int get_spendable_gold(const GameState& S, const GameMap& M) const {
        int emergency_cost = emergency_train_queue * TRAIN_COST;
        return virtual_gold - get_locked_gold(S, M) - emergency_cost;
    }

    std::vector<Warrior> get_free_units(const GameState& S) const {
        std::vector<Warrior> free_units;
        for (const auto& w : my_warriors) {
            bool in_mission = false;
            for (const auto& m : active_missions) {
                if (m.squad_ids.count(w.id)) { in_mission = true; break; }
            }
            if (!predictive_defenders.count(w.id) && !emergency_targets.count(w.id) && 
                !in_mission && !build_plans.count(w.id) && !forward_build_plans.count(w.id) && !persistent_jobs.count(w.id)) {
                free_units.push_back(w);
            }
        }
        return free_units;
    }

    // [SỬA LẠI] Thêm tham số GameState& S và cập nhật logic lọc quân địch rảnh rỗi
    bool is_safe_to_dispatch(const GameState& S, const Warrior& w, int tgt, int current_free_count, const GameMap& M, const Paths& P) const {
        // --- LOGIC MỚI: Tính toán quân địch rảnh rỗi (giống is_safe_against_all_in) ---
        int total_enemy = enemy_warriors.size();
        int enemy_labor = 0;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                int count = 0;
                for (const auto& ew : enemy_warriors) if (ew.region == b.region) count++;
                enemy_labor += std::min(count, b.work_cap());
            }
        }

        int attacking_bases_count = 0;
        for (const auto& g : active_enemy_attacks) {
            // Những quân địch đang đi tấn công base khác (không nhắm vào HQ ta) thì không tính
            if (g.potential_targets.find(M.my_hq) == g.potential_targets.end()) {
                attacking_bases_count += g.ids.size();
            }
        }

        int estimated_all_in = std::max(0, total_enemy - enemy_labor - attacking_bases_count - 1);
        int opp_hq_labor_cap = opp_hq_b ? opp_hq_b->work_cap() : 1;
        int opp_hq_troop_count = 0;
        for (const auto& ew : enemy_warriors) if (ew.region == M.opp_hq) opp_hq_troop_count++;
        int old_baseline = std::max(0, opp_hq_troop_count - opp_hq_labor_cap);
        
        int baseline_invading_enemies = std::max(estimated_all_in, old_baseline);

        int e_hp = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int m_hp = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        int hq_ad = get_b_ad(S, M.my_hq);
        int hq_hp = hq_b ? hq_b->hp : 10;
        
        // Tính ra lượng quân thực tế cần để thủ HQ hiện tại
        int dynamic_req_defenders = calculate_min_defenders(baseline_invading_enemies, e_hp, hq_hp, hq_ad, m_hp);

        // --- ĐIỀU KIỆN CŨ ---
        int free_after = current_free_count - 1;
        int trip_time = 2 * get_hops(P, w.region, tgt);
        int max_dist = get_hops(P, M.center_region, M.opp_hq) + 1;
        
        return (free_after >= dynamic_req_defenders) || (trip_time <= max_dist);
    }

    // [SỬA LẠI] Cập nhật: Chỉ skip tính toán phòng thủ đối với Base, không skip đối với HQ.
    int calculate_req_attackers(int final_target, int dist_from_us, const GameState& S, const GameMap& M, const Paths& P) const {
        bool is_hq = (final_target == M.opp_hq);

        // ========================================================
        // LOGIC MỚI: KIỂM TRA SNOWBALL - KINH TẾ VƯỢT TRỘI (CHỈ CHO BASE)
        // ========================================================
        int my_net_income = get_my_net_income(S, M, my_warriors.size());
        int opp_net_income = get_enemy_net_income(S, M, enemy_warriors.size());

        // CHỈ bỏ qua mô phỏng 50 turn nếu mục tiêu KHÔNG PHẢI là HQ
        if (!is_hq && my_net_income - opp_net_income >= TUNE_SNOWBALL_INCOME_GAP) {
            int b_hp = 0;
            for (const auto& b : S.buildings) {
                if (b.region == final_target && b.side != M.my_side) {
                    b_hp = b.hp;
                    break;
                }
            }
            
            int defending_enemies = 0;
            for (const auto& ew : enemy_warriors) {
                if (ew.region == final_target) defending_enemies++;
            }
            
            int my_hp_val = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
            
            // Heuristic chớp nhoáng: Lính địch tại chỗ + (Máu base / máu lính mình) + Buffer
            int required = defending_enemies + (b_hp + my_hp_val - 1) / my_hp_val + TUNE_SNOWBALL_BUFFER;
            
            return required;
        }

        // ========================================================
        // PHẦN MÔ PHỎNG 50 TURN CŨ (Bắt buộc dùng cho HQ hoặc khi chưa Snowball)
        // ========================================================
        int my_hp = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        int b_hp = 0, b_ad = 0;

        for (const auto& b : S.buildings) {
            if (b.region == final_target && b.side != M.my_side) {
                b_hp = b.hp; b_ad = get_b_ad(S, final_target); break;
            }
        }

        std::set<WarriorId> busy_enemies;
        for (const auto& g : active_enemy_attacks) {
            if (g.type == ThreatType::GATHERING && g.current_region == final_target) {
                continue; 
            }
            for (auto id : g.ids) busy_enemies.insert(id);
        }

        std::map<int, int> working_enemies;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                int count = 0;
                for (const auto& ew : enemy_warriors) if (ew.region == b.region) count++;
                working_enemies[b.region] = std::min(count, b.work_cap());
            }
        }

        std::map<int, int> reinforcements_by_day;
        int initial_defending_hp = 0;
        int initial_defending_units = 0;

        std::map<int, int> local_enemy_counts;
        for(auto& ew : enemy_warriors) {
            if (busy_enemies.count(ew.id)) continue;

            if (ew.region == final_target) {
                initial_defending_hp += ew.hp;
                initial_defending_units++;
            } else {
                local_enemy_counts[ew.region]++;
            }
        }

        for (auto const& [reg, count] : local_enemy_counts) {
            int working = working_enemies.count(reg) ? working_enemies[reg] : 0;
            int free_count = std::max(0, count - working);
            if (free_count > 0) {
                int d = get_hops(P, reg, final_target);
                if (d < 999) {
                    int arrival_day = d + 1; 
                    reinforcements_by_day[arrival_day] += free_count;
                }
            }
        }

        int opp_train_cap = opp_hq_b ? opp_hq_b->train_cap() : 1;
        int opp_hp_per_unit = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int opp_gross_inc = get_enemy_gross_income(S, M);
        
        auto can_win = [&](int A) {
            int sim_gold = opp_gold;
            int e_hp_pool = initial_defending_hp;
            int e_units = initial_defending_units;
            int cur_base_hp = b_hp;
            
            int total_sim_e_units = enemy_warriors.size(); 
            int my_hp_pool = A * my_hp;
            int my_units = A;
            
            std::map<int, int> sim_reinforcements = reinforcements_by_day;

            for(int day = 0; day < 50 + dist_from_us; ++day) {
                if (sim_reinforcements.count(day)) {
                    int r = sim_reinforcements[day];
                    e_units += r;
                    e_hp_pool += r * opp_hp_per_unit;
                }

                if (opp_hq_b && cur_base_hp > 0) {
                    int spawns = std::min(opp_train_cap, sim_gold / TRAIN_COST);
                    sim_gold -= spawns * TRAIN_COST;
                    total_sim_e_units += spawns;
                    
                    if (is_hq) {
                        if (day >= 1) { 
                            e_units += spawns;
                            e_hp_pool += spawns * opp_hp_per_unit;
                        }
                    } else {
                        if (day >= 1) { 
                            int d_hq = get_hops(P, M.opp_hq, final_target);
                            sim_reinforcements[day + d_hq] += spawns;
                        }
                    }
                }
                
                if (day >= dist_from_us) {
                    int dmg_to_us = (cur_base_hp > 0 ? b_ad : 0) + e_units;
                    int dmg_to_them = my_units;
                    
                    e_hp_pool -= dmg_to_them;
                    if (e_hp_pool < 0) {
                        cur_base_hp += e_hp_pool;
                        e_hp_pool = 0;
                    }
                    e_units = (e_hp_pool + opp_hp_per_unit - 1) / opp_hp_per_unit;
                    
                    my_hp_pool -= dmg_to_us;
                    my_units = my_hp_pool > 0 ? (my_hp_pool + my_hp - 1) / my_hp : 0;
                    
                    if (cur_base_hp <= 0) return true; 
                    if (my_hp_pool <= 0) return false; 
                }
                
                int sim_net_inc = opp_gross_inc - total_sim_e_units * UPKEEP_PER_WARRIOR;
                sim_gold += sim_net_inc;
                sim_gold = std::max(0, sim_gold);
            }
            return false; 
        };

        int low = 1, high = 1000;
        int ans = high;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            if (can_win(mid)) {
                ans = mid;       
                high = mid - 1;
            } else {
                low = mid + 1;   
            }
        }
        
        return ans;
    }

    void update_state_and_clean_dead(const GameState &S, const GameMap &M, const Paths &P) {
        update_enemy_economy(S, M); 

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
        for (auto it = forward_build_plans.begin(); it != forward_build_plans.end(); ) {
            if (!alive_ids.count(it->first)) it = forward_build_plans.erase(it); else ++it;
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
        std::vector<EnemyAttackGroup> new_split_groups;

        for (auto it = active_enemy_attacks.begin(); it != active_enemy_attacks.end(); ) {
            std::map<int, std::set<WarriorId>> region_to_ids;
            for (const auto& id : it->ids) {
                for (const auto& ew : enemy_warriors) {
                    if (ew.id == id) {
                        region_to_ids[ew.region].insert(id);
                        break;
                    }
                }
            }

            if (region_to_ids.empty()) {
                it = active_enemy_attacks.erase(it);
                continue;
            }

            auto region_it = region_to_ids.begin();
            it->ids = region_it->second;
            int main_new_reg = region_it->first;
            int old_reg = it->current_region;

            auto process_group = [&](EnemyAttackGroup& g, int new_reg) {
                if (new_reg != old_reg) {
                    g.type = ThreatType::MOVING; 
                    g.idle_turns = 0;
                    for (auto tgt_it = g.potential_targets.begin(); tgt_it != g.potential_targets.end(); ) {
                        if (old_reg != *tgt_it && P.nxt[old_reg][*tgt_it] != new_reg) {
                            tgt_it = g.potential_targets.erase(tgt_it);
                        } else {
                            ++tgt_it;
                        }
                    }
                    g.current_region = new_reg;
                } else {
                    g.idle_turns++;
                }
                return !g.potential_targets.empty();
            };

            bool keep_main = process_group(*it, main_new_reg);

            auto advance_it = region_it; ++advance_it;
            for (; advance_it != region_to_ids.end(); ++advance_it) {
                EnemyAttackGroup split_g = *it;
                split_g.ids = advance_it->second;
                split_g.current_region = old_reg; 
                if (process_group(split_g, advance_it->first)) {
                    new_split_groups.push_back(split_g);
                }
            }

            if (!keep_main ) {
                it = active_enemy_attacks.erase(it);
            } else {
                ++it;
            }
        }
        
        for (const auto& ng : new_split_groups) {
            active_enemy_attacks.push_back(ng);
        }

        std::map<int, int> enemy_count_per_region;
        std::map<int, std::set<WarriorId>> enemy_ids_per_region;
        for (const auto& ew : enemy_warriors) {
            enemy_count_per_region[ew.region]++;
            enemy_ids_per_region[ew.region].insert(ew.id);
        }

        std::map<int, int> enemy_labor_cap_per_region;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                enemy_labor_cap_per_region[b.region] = b.work_cap();
            }
        }

        for (auto it = enemy_concentration_turns.begin(); it != enemy_concentration_turns.end(); ) {
            int reg = it->first;
            int count = enemy_count_per_region[reg]; 
            int labor_cap = enemy_labor_cap_per_region.count(reg) ? enemy_labor_cap_per_region[reg] : 0;
            
            if (reg == M.opp_hq || count <= labor_cap) {
                it = enemy_concentration_turns.erase(it); 
            } else {
                ++it;
            }
        }

        int weakest_base_hp = HQ_LEVELS[hq_b ? hq_b->level : 1].hp;
        int weakest_base_ad = get_b_ad(S, M.my_hq);
        for (const auto& b : my_bases) {
            if (b.hp < weakest_base_hp) { weakest_base_hp = b.hp; weakest_base_ad = get_b_ad(S, b.region); }
        }
        int e_hp_val = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;

        for (auto const& [reg, count] : enemy_count_per_region) {
            if (reg == M.opp_hq) continue; 

            int labor_cap = enemy_labor_cap_per_region.count(reg) ? enemy_labor_cap_per_region[reg] : 0;

            if (count > labor_cap) { 
                enemy_concentration_turns[reg]++; 
                
                if (enemy_concentration_turns[reg] > 1) { 
                    int excess = count - labor_cap; 
                    
                    if (excess > 0) {
                        int min_def_needed = calculate_min_defenders(excess, e_hp_val, weakest_base_hp, weakest_base_ad, HQ_LEVELS[hq_b ? hq_b->level : 1].warrior_hp);
                        
                        if (min_def_needed > 0) {
                            std::set<WarriorId> gathered_ids;
                            for (auto id : enemy_ids_per_region[reg]) {
                                bool is_moving = false;
                                for (const auto& g : active_enemy_attacks) {
                                    if (g.type == ThreatType::MOVING && g.ids.count(id)) {
                                        is_moving = true; break;
                                    }
                                }
                                if (!is_moving) gathered_ids.insert(id);
                            }

                            if (!gathered_ids.empty()) {
                                bool existing_group_updated = false;
                                for (auto& g : active_enemy_attacks) {
                                    if (g.current_region == reg && g.type == ThreatType::GATHERING) {
                                        g.ids = gathered_ids; 
                                        g.idle_turns = enemy_concentration_turns[reg];
                                        existing_group_updated = true;
                                        break;
                                    }
                                }

                                if (!existing_group_updated) {
                                    EnemyAttackGroup g;
                                    g.ids = gathered_ids;
                                    g.current_region = reg;
                                    g.idle_turns = enemy_concentration_turns[reg];
                                    g.type = ThreatType::GATHERING; 
                                    
                                    g.potential_targets.insert(M.my_hq);
                                    for (const auto& b : my_bases) g.potential_targets.insert(b.region);
                                    
                                    active_enemy_attacks.push_back(g);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void assign_predictive_defenders(const GameState &S, const GameMap &M, const Paths &P) {
        doomed_bases.clear(); 
        
        struct Threat {
            int target;
            int enemy_dist;
            std::set<WarriorId> enemy_ids;
            int req_def;
            int priority;
            int max_cap;
        };
        std::vector<Threat> active_threats;

        for (const auto& g : active_enemy_attacks) {
            int best_target = -1;
            int min_dist_enemy = 999;
            
            for (int tgt : g.potential_targets) {
                int d_e = get_hops(P, g.current_region, tgt);
                if (d_e < min_dist_enemy) {
                    min_dist_enemy = d_e;
                    best_target = tgt;
                }
            }

            if (best_target != -1) {
                int e_hp_val = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
                int m_hp_val = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
                int b_hp = 0, b_ad = 0, b_level = 1;
                for (const auto& b : S.buildings) {
                    if (b.region == best_target) {
                        b_hp = b.hp;
                        b_ad = get_b_ad(S, best_target);
                        b_level = b.level;
                        break;
                    }
                }
                int req_def = calculate_min_defenders(g.ids.size(), e_hp_val, b_hp, b_ad, m_hp_val);
                if (req_def > 0) {
                    int priority = (best_target == M.my_hq) ? 1000 : (b_level * 10);
                    int max_cap = (best_target == M.my_hq) ? 9999 : (b_level * 5 + 5); 
                    active_threats.push_back({best_target, min_dist_enemy, g.ids, req_def, priority, max_cap});
                }
            }
        }

        std::sort(active_threats.begin(), active_threats.end(), [&](const Threat& a, const Threat& b) {
            if(a.enemy_dist == b.enemy_dist) return a.priority > b.priority;
            return a.enemy_dist < b.enemy_dist;
        });

        std::vector<Warrior> available_defenders = get_free_units(S);
        int train_cap_per_turn = hq_b ? hq_b->train_cap() : 1;
        
        int sim_gold = get_spendable_gold(S, M); 
        int my_inc = std::max(0, get_my_gross_income(S, M) - total_upkeep); 

        for (const auto& threat : active_threats) {
            int existing = 0;
            for (auto const& [my_id, e_ids] : predictive_defenders) {
                if (emergency_targets.count(my_id) && emergency_targets.at(my_id) == threat.target) {
                    existing++;
                }
            }

            int needed = threat.req_def - existing;
            if (needed <= 0) continue;

            if (threat.req_def > threat.max_cap && threat.target != M.my_hq) {
                doomed_bases.insert(threat.target);
                continue; 
            }

            std::vector<Warrior> can_arrive_in_time;
            for (const auto& w : available_defenders) {
                if (get_hops(P, w.region, threat.target) <= threat.enemy_dist + 1) { 
                    can_arrive_in_time.push_back(w);
                }
            }

            int dist_hq_to_threat = get_hops(P, M.my_hq, threat.target);
            int turns_to_train = std::max(0, threat.enemy_dist + 1 - dist_hq_to_threat);
            int future_trainable = 0;
            int temp_gold = sim_gold; 

            for(int t = 0; t < turns_to_train; ++t) {
                int spawn = std::min(train_cap_per_turn, temp_gold / TRAIN_COST);
                future_trainable += spawn;
                temp_gold -= spawn * TRAIN_COST;
                temp_gold += my_inc; 
            }

            int potential_defenders = can_arrive_in_time.size() + future_trainable;

            if (potential_defenders < needed && threat.target != M.my_hq) {
                doomed_bases.insert(threat.target);
                continue; 
            }

            int train_needed = std::max(0, needed - (int)can_arrive_in_time.size());
            sim_gold -= train_needed * TRAIN_COST; 

            std::sort(can_arrive_in_time.begin(), can_arrive_in_time.end(), [&](const Warrior& a, const Warrior& b){
                return get_hops(P, a.region, threat.target) < get_hops(P, b.region, threat.target);
            });

            int assigned = 0;
            for (const auto& w : can_arrive_in_time) {
                if (assigned >= needed) break;
                
                predictive_defenders[w.id] = threat.enemy_ids;
                emergency_targets[w.id] = threat.target;
                
                auto it = std::find_if(available_defenders.begin(), available_defenders.end(), [&](const Warrior& av){ return av.id == w.id; });
                if (it != available_defenders.end()) available_defenders.erase(it);
                
                assigned++;
            }

            if (train_needed > 0) {
                emergency_train_queue += train_needed;
            }
        }
    }

    void detect_and_handle_emergencies(const GameState &S, const GameMap &M, const Paths &P) {
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

        int total_enemy = enemy_warriors.size();

        int enemy_labor = 0;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                int count = 0;
                for (const auto& ew : enemy_warriors) {
                    if (ew.region == b.region) count++;
                }
                enemy_labor += std::min(count, b.work_cap());
            }
        }

        int tracked_in_groups = 0;
        for (const auto& g : active_enemy_attacks) {
            tracked_in_groups += g.ids.size();
        }

        int estimated_all_in = std::max(0, total_enemy - enemy_labor - tracked_in_groups);

        int opp_hq_labor_cap = opp_hq_b ? opp_hq_b->work_cap() : 1;
        int opp_hq_troop_count = 0;
        for (const auto& ew : enemy_warriors) {
            if (ew.region == M.opp_hq) opp_hq_troop_count++;
        }
        int old_baseline = std::max(0, opp_hq_troop_count - opp_hq_labor_cap);

        int baseline_invading_enemies = std::max(estimated_all_in, old_baseline);

        int e_hp = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int m_hp = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        int hq_ad = get_b_ad(S, M.my_hq);
        int hq_hp = hq_b ? hq_b->hp : 10;
        current_req_defenders = calculate_min_defenders(baseline_invading_enemies, e_hp, hq_hp, hq_ad, m_hp);

        int free_count = get_free_units(S).size();

        if (free_count < current_req_defenders) {
            emergency_train_queue += (current_req_defenders - free_count);
        }

        assign_predictive_defenders(S, M, P);

        for (int doomed_reg : doomed_bases) {
            bool enemy_close = false;
            for (const auto& ew : enemy_warriors) {
                if (get_hops(P, ew.region, doomed_reg) <= 1) { 
                    enemy_close = true; break;
                }
            }
            
            if (enemy_close) {
                virtual_job_slots[doomed_reg] = 0; 
                for (auto it = persistent_jobs.begin(); it != persistent_jobs.end(); ) {
                    if (it->second == doomed_reg) {
                        it = persistent_jobs.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }

    void plan_attacks(const GameState &S, const GameMap &M, const Paths &P, int turn) {
        int current_net_income = get_my_net_income(S, M, my_warriors.size()); 
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
            
            int best_rp_stealth = -1;
            int min_d_stealth = 999;
            
            if (min_d >= 2) {
                best_rp_stealth = M.my_hq;
                min_d_stealth = min_d;
            }

            for (const auto& b : my_bases) {
                int d = get_hops(P, b.region, tgt);
                if ( d < min_d_stealth ) {
                    min_d_stealth = d;
                    best_rp_stealth = b.region;
                }
                if (d < min_d) { 
                    min_d = d; 
                    best_rp = b.region; 
                }
            }
            
            return (best_rp_stealth != -1) ? best_rp_stealth : best_rp;
        };

        int current_free_count = get_free_units(S).size();

        std::set<int> locked_targets;
        std::set<int> gathering_targets;
        for (const auto& m : active_missions) {
            if (m.is_launched) locked_targets.insert(m.target);
            else gathering_targets.insert(m.target);
        }

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
            
            int rp_dist_to_original_tgt = get_hops(P, it->rally_point, it->target);
            
            std::vector<WarriorId> ready_wids;
            for(auto wid : it->squad_ids) {
                for(auto& w : my_warriors) {
                    if(w.id == wid && w.state == WState::STATIONARY && w.region == it->rally_point) {
                        ready_wids.push_back(wid); break;
                    }
                }
            }
            int ready_count = ready_wids.size();

            struct TargetCand { int tgt; int req; double score; };
            TargetCand best_cand = {-1, 99999, -9999999.0};

            if (ready_count > 0) {
                std::vector<int> potential_pivot_targets;
                bool opp_hq_alive = false;
                for (const auto& b : S.buildings) if (b.region == M.opp_hq && b.side != M.my_side) opp_hq_alive = true;
                if (opp_hq_alive) potential_pivot_targets.push_back(M.opp_hq);
                
                for (const auto& b : S.buildings) {
                    if (b.side != M.my_side && b.type == BType::BASE) potential_pivot_targets.push_back(b.region);
                }

                for (int tgt : potential_pivot_targets) {
                    int dist = get_hops(P, it->rally_point, tgt);
                    if (dist == 999) continue;
                    
                    if (tgt != it->target && locked_targets.count(tgt)) continue; 
                    
                    int req = calculate_req_attackers(tgt, dist, S, M, P);
                    if (ready_count >= req && req > 0) {
                        double score = 0.0;
                        if (tgt == M.opp_hq) {
                            score += 10000.0;
                        } else {
                            int b_lvl = 1;
                            for (const auto& b : S.buildings) if (b.region == tgt) { b_lvl = b.level; break; }
                            score += 500.0 + b_lvl * 100.0;
                        }
                        score -= dist * 50.0;
                        score -= (ready_count - req) * 10.0;
                        if (tgt == it->target) score += 200.0;
                        
                        if (score > best_cand.score) {
                            best_cand = {tgt, req, score};
                        }
                    }
                }
            }

            if (best_cand.tgt != -1) {
                if (it->is_launched) locked_targets.erase(it->target);
                else gathering_targets.erase(it->target);

                it->target = best_cand.tgt;
                it->required_attackers = best_cand.req;
                it->is_launched = true; 
                locked_targets.insert(best_cand.tgt); 

                int old_size = it->squad_ids.size();
                it->squad_ids.clear();
                
                for (int i = 0; i < best_cand.req; i++) {
                    it->squad_ids.insert(ready_wids[i]);
                }
                
                current_free_count += (old_size - best_cand.req);

            } else {
                it->required_attackers = calculate_req_attackers(it->target, rp_dist_to_original_tgt, S, M, P);
                
                if (ready_count >= it->required_attackers && it->required_attackers > 0) {
                    if (!it->is_launched) {
                        gathering_targets.erase(it->target);
                        locked_targets.insert(it->target);
                    }
                    it->is_launched = true;
                    
                    int old_size = it->squad_ids.size();
                    it->squad_ids.clear();
                    
                    for (int i = 0; i < it->required_attackers; i++) {
                        it->squad_ids.insert(ready_wids[i]);
                    }
                    
                    current_free_count += (old_size - it->required_attackers);
                } else {
                    int current_squad_size = it->squad_ids.size();
                    if (current_squad_size < it->required_attackers && !is_rushing_hq) {
                        std::vector<Warrior> atk_units = get_attack_free_units();
                        std::sort(atk_units.begin(), atk_units.end(), [&](const Warrior& a, const Warrior& b) {
                            return get_hops(P, a.region, it->rally_point) < get_hops(P, b.region, it->rally_point);
                        });
                        
                        for (auto& w : atk_units) {
                            if (current_squad_size >= it->required_attackers) break;
                            // [SỬA LẠI] Gọi hàm có thêm biến S
                            if (!is_safe_to_dispatch(S, w, it->target, current_free_count, M, P)) continue;
                            it->squad_ids.insert(w.id); 
                            current_squad_size++;
                            current_free_count--; 
                        }
                        if (current_squad_size < it->required_attackers) {
                            if (current_net_income >= TUNE_MIN_NET_INCOME_TO_ATTACK) {
                                pending_train_requests += (it->required_attackers - current_squad_size);
                            }
                        }
                    } else if (current_squad_size > it->required_attackers) {
                        std::vector<Warrior> squad_units;
                        for(auto wid : it->squad_ids) {
                            for(auto& w : my_warriors) if(w.id == wid) squad_units.push_back(w);
                        }
                        std::sort(squad_units.begin(), squad_units.end(), [&](const Warrior& a, const Warrior& b){
                            return get_hops(P, a.region, it->rally_point) > get_hops(P, b.region, it->rally_point);
                        });
                        
                        int to_remove = current_squad_size - it->required_attackers;
                        for(int i = 0; i < to_remove; i++) {
                            it->squad_ids.erase(squad_units[i].id);
                            current_free_count++;
                        }
                    }
                }
            }
            ++it;
        }

        int spendable_gold = get_spendable_gold(S, M); 
        std::vector<Warrior> temp_check_units = get_attack_free_units();
        int idle_army_count = temp_check_units.size(); 

        bool can_attack = (get_total_labor(S, M) >= TUNE_MIN_LABOR_TO_FIGHT);
        if (can_attack && (done_building || spendable_gold >= 300 || idle_army_count >= 3)) {
            
            std::vector<int> potential_targets;
            if (!locked_targets.count(M.opp_hq)) potential_targets.push_back(M.opp_hq);
            for (const auto& b : S.buildings) {
                if (b.side != M.my_side && b.type == BType::BASE && !locked_targets.count(b.region)) {
                    potential_targets.push_back(b.region); 
                }
            }

            std::vector<Warrior> atk_units_fast = get_attack_free_units();
            std::map<int, std::vector<Warrior>> free_by_region;
            for (auto& w : atk_units_fast) {
                // [SỬA LẠI] Gọi hàm có thêm biến S
                if (is_safe_to_dispatch(S, w, M.opp_hq, current_free_count, M, P)) {
                    free_by_region[w.region].push_back(w);
                }
            }
            
            for (int tgt : potential_targets) {
                if (active_missions.size() >= TUNE_MAX_CONCURRENT_ATTACKS) break;
                
                for (auto& [reg, group] : free_by_region) {
                    if (group.empty()) continue;
                    
                    int dist = get_hops(P, reg, tgt);
                    if (dist == 999) continue;
                    
                    int req = calculate_req_attackers(tgt, dist, S, M, P);
                    
                    if (group.size() >= req && req <= 100) {
                        AttackMission m;
                        m.target = tgt;
                        m.rally_point = reg;
                        m.required_attackers = req;
                        m.is_launched = true; 
                        m.created_turn = turn;
                        
                        for (int i = 0; i < req; ++i) {
                            m.squad_ids.insert(group[i].id);
                            current_free_count--;
                        }
                        group.erase(group.begin(), group.begin() + req);
                        active_missions.push_back(m);
                        locked_targets.insert(tgt); 
                        break; 
                    }
                }
            }

            std::vector<int> remaining_targets;
            for(int tgt : potential_targets) {
                if (!locked_targets.count(tgt) && !gathering_targets.count(tgt)) {
                    remaining_targets.push_back(tgt);
                }
            }
            potential_targets = remaining_targets;

            struct TargetOption { int tgt; int req; double score; int rp; };
            std::vector<TargetOption> options;

            for (int tgt : potential_targets) {
                int rp = get_rally_point(tgt);
                int avg_dist = get_hops(P, rp, tgt); 
                int current_req = calculate_req_attackers(tgt, avg_dist, S, M, P);
                
                double score = (current_req * TUNE_SCORE_REQ_WEIGHT) + (avg_dist * TUNE_SCORE_DIST_WEIGHT);
                
                if (tgt == M.center_region) score -= 999999.0; 
                else if (tgt == M.opp_hq) score -= TUNE_HQ_TARGET_BONUS;
                else {
                    score -= TUNE_BASE_TARGET_BONUS;
                    if (current_req <= 10) score -= TUNE_WEAK_BASE_BONUS; 
                }

                bool has_gathering = false;
                for (const auto& g : active_enemy_attacks) {
                    if (g.type == ThreatType::GATHERING && g.current_region == tgt) {
                        has_gathering = true; break;
                    }
                }
                if (has_gathering) {
                    score -= 50000.0;
                }

                int dist_to_my_hq = get_hops(P, tgt, M.my_hq);
                int dist_to_opp_hq = get_hops(P, tgt, M.opp_hq);
                if (tgt != M.opp_hq && dist_to_my_hq < dist_to_opp_hq) {
                    score -= TUNE_ENEMY_IN_MY_HALF_BONUS; 
                }

                options.push_back({tgt, current_req, score, rp});
            }
            
            std::sort(options.begin(), options.end(), [](const TargetOption& a, const TargetOption& b) {
                return a.score < b.score;
            });

            int my_est_inc = get_my_net_income(S, M, my_warriors.size());
            int max_potential_army = get_free_units(S).size() + (virtual_gold + std::max(0, my_est_inc) * 15) / TRAIN_COST;
            for (const auto& m : active_missions) max_potential_army += m.squad_ids.size();

            std::vector<Warrior> atk_units = get_attack_free_units(); 

            for (const auto& opt : options) {
                if (active_missions.size() >= TUNE_MAX_CONCURRENT_ATTACKS) break;
                
                int max_allowed_req = std::max(50, (int)(max_potential_army * TUNE_MAX_ARMY_RATIO));
                if (opt.req > max_allowed_req) continue; 
                
                int available = atk_units.size();
                if (is_rushing_hq && available < opt.req) continue;
                if (available == 0 && spendable_gold < TRAIN_COST) break; 
                
                int train_shortage = std::max(0, opt.req - available);
                
                if (train_shortage > 0 && current_net_income < TUNE_MIN_NET_INCOME_TO_ATTACK) {
                    continue; 
                }
                
                int budget = (train_shortage * TRAIN_COST) + (done_building ? 0 : RESERVE_GOLD_ATTACK);
                
                if (spendable_gold >= budget || available >= opt.req) {
                    AttackMission m;
                    m.target = opt.tgt; m.rally_point = opt.rp; m.required_attackers = opt.req;
                    m.is_launched = false; m.created_turn = turn;
                    
                    std::sort(atk_units.begin(), atk_units.end(), [&](const Warrior& a, const Warrior& b) {
                        return get_hops(P, a.region, opt.rp) < get_hops(P, b.region, opt.rp);
                    });
                    
                    int drafted = 0;
                    for (auto it = atk_units.begin(); it != atk_units.end(); ) {
                        if (drafted >= m.required_attackers) break;
                        // [SỬA LẠI] Gọi hàm có thêm biến S
                        if (!is_safe_to_dispatch(S, *it, opt.tgt, current_free_count, M, P)) { ++it; continue; }
                        
                        m.squad_ids.insert(it->id); 
                        drafted++;
                        current_free_count--;
                        it = atk_units.erase(it);
                    }
                    
                    int ready_at_rally = 0;
                    for(auto wid : m.squad_ids) {
                        for(auto& w : my_warriors) {
                            if(w.id == wid && w.state == WState::STATIONARY && w.region == m.rally_point) ready_at_rally++;
                        }
                    }

                    if (ready_at_rally >= m.required_attackers && m.required_attackers > 0) {
                        m.is_launched = true;
                        locked_targets.insert(opt.tgt); 
                    } else {
                        pending_train_requests += train_shortage;
                        gathering_targets.insert(opt.tgt); 
                    }
                    spendable_gold -= budget; 
                    spendable_gold -= (drafted * MOVE_COST);
                    active_missions.push_back(m);
                }
            }
        }
    }
    
    void plan_forward_expansion(const GameState &S, const GameMap &M, const Paths &P) {
        if (is_rushing_hq) return; 

        int current_builds = build_plans.size() + forward_build_plans.size();
        if (current_builds >= TUNE_MAX_CONCURRENT_BUILDS) return; 
        
        std::vector<Warrior> free_units = get_free_units(S);
        
        struct FwdCand { int region; int dist_opp; WarriorId wid; };
        std::vector<FwdCand> cands;

        for (int sh : M.strongholds) {
            int dist_opp = get_hops(P, sh, M.opp_hq);
            int dist_my = get_hops(P, sh, M.my_hq);
            if (dist_opp < dist_my) { 
                bool has_b = false; 
                for (const auto& bld : S.buildings) if (bld.region == sh) has_b = true;
                bool already_planned = false; 
                for(auto const& [wid, plan] : build_plans) if (plan.first == sh) already_planned = true;
                for(auto const& [wid, plan] : forward_build_plans) if (plan.first == sh) already_planned = true;
                
                if (!has_b && !already_planned) {
                    for (const auto& w : free_units) {
                        if (w.region == sh) { 
                            cands.push_back({sh, dist_opp, w.id});
                            break; 
                        }
                    }
                }
            }
        }

        std::sort(cands.begin(), cands.end(), [](const FwdCand& a, const FwdCand& b){
            return a.dist_opp < b.dist_opp;
        });

        for (const auto& cand : cands) {
            if (current_builds >= TUNE_MAX_CONCURRENT_BUILDS) break;
            
            int base_budget = get_spendable_gold(S, M);
            if (base_budget >= BASE_LEVELS[1].cost) {
                    forward_build_plans[cand.wid] = {cand.region, BASE_LEVELS[1].cost};
                    current_builds++;
            }
        }
    }

    void process_forward_build_plans(const GameState &S, const GameMap &M, const Paths &P, Actions &a) {
        for (auto it = forward_build_plans.begin(); it != forward_build_plans.end(); ) {
            Warrior w_copy; bool found_w = false;
            for (const auto& w : my_warriors) if (w.id == it->first) { w_copy = w; found_w = true; break; }
            if (!found_w) { it = forward_build_plans.erase(it); continue; }

            int target_region = it->second.first;
            bool enemy_present = false;
            for (const auto& ew : enemy_warriors) {
                if (ew.region == target_region) { enemy_present = true; break; }
            }
            if (enemy_present) { it = forward_build_plans.erase(it); continue; }

            Building* b = nullptr;
            for(auto& bld : S.buildings) if (bld.region == target_region) b = (Building*)&bld;

            if (b) { it = forward_build_plans.erase(it); continue; } 

            int actual_cost = BASE_LEVELS[1].cost;
            int reserved_move = get_locked_move_gold(S, M); // [THÊM MỚI] Lấy quỹ tiền di chuyển
            if (w_copy.region == target_region && w_copy.state == WState::STATIONARY) {
                bool already_upgrading = std::find(a.upgrades.begin(), a.upgrades.end(), target_region) != a.upgrades.end();
                
                if (!already_upgrading && (virtual_gold - reserved_move) >= actual_cost + total_upkeep) {
                    a.upgrades.push_back(target_region);
                    virtual_gold -= actual_cost;
                    virtual_job_slots[target_region] += BASE_LEVELS[1].work_cap;
                    persistent_jobs[w_copy.id] = target_region;
                    it = forward_build_plans.erase(it); 
                } else ++it;
            } else ++it;
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

    void plan_expansion(const GameState &S, const GameMap &M, const Paths &P, bool hq_fast_tracked) {
        int current_builds = build_plans.size() + forward_build_plans.size();
        if (current_builds >= TUNE_MAX_CONCURRENT_BUILDS) return; 
        
        std::vector<Warrior> free_units = get_free_units(S);
        int current_free_count = free_units.size();

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
        for (const auto& plan : forward_build_plans) {
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
            for (const auto& b : my_bases) { if (b.region == plan.second.first) { is_new = false; break; } }
            if (is_new) future_base_count++;
        }
        for (const auto& plan : forward_build_plans) {
            bool is_new = true;
            for (const auto& b : my_bases) { if (b.region == plan.second.first) { is_new = false; break; } }
            if (is_new) future_base_count++;
        }

        if (!is_rushing_hq) {
            for (int sh : M.strongholds) {
                bool has_b = false; for (const auto& bld : S.buildings) if (bld.region == sh) has_b = true;
                bool already_planned = false; 
                for(auto const& [wid, plan] : build_plans) if (plan.first == sh) already_planned = true;
                for(auto const& [wid, plan] : forward_build_plans) if (plan.first == sh) already_planned = true;
                
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
        }

        if (hq_b && hq_b->level < custom_hq_max_level && !hq_fast_tracked) {
            bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == hq_b->region) enemy_present = true;
            bool already_planned = false; 
            for(auto const& [wid, plan] : build_plans) if (plan.first == hq_b->region) already_planned = true;
            for(auto const& [wid, plan] : forward_build_plans) if (plan.first == hq_b->region) already_planned = true;
            
            if (!already_planned && !enemy_present) {
                PlanCandidate c; c.region = hq_b->region; c.cost = HQ_LEVELS[hq_b->level + 1].upgrade_cost;
                c.is_hq = true; c.is_new_base = false; c.dist_to_my_hq = 0; c.priority = 1;
                cands.push_back(c);
            }
        }

        if (!is_rushing_hq) {
            for (const auto& b : my_bases) {
                bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == b.region) enemy_present = true;
                bool already_planned = false; 
                for(auto const& [wid, plan] : build_plans) if (plan.first == b.region) already_planned = true;
                for(auto const& [wid, plan] : forward_build_plans) if (plan.first == b.region) already_planned = true;
                
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
        }
        
        std::sort(cands.begin(), cands.end());
        int estimated_income = (persistent_jobs.size() * WORK_INCOME) - (my_warriors.size() * UPKEEP_PER_WARRIOR);

        for (const auto& cand : cands) {
            int base_budget = get_spendable_gold(S, M);
            
            if (free_units.empty()) {
                int D = cand.dist_to_my_hq;
                int projected_gold = base_budget + (D * estimated_income);
                int req_gold = cand.cost + TRAIN_COST + (MOVE_COST);
                if (projected_gold >= req_gold) pending_train_requests++;
                break; 
            }
            
            int best_u = -1; int min_d = 999;
            for (size_t i = 0; i < free_units.size(); i++) {
                // [SỬA LẠI] Gọi hàm có thêm biến S
                if (!is_safe_to_dispatch(S, free_units[i], cand.region, current_free_count, M, P)) continue;
                
                int d = get_hops(P, free_units[i].region, cand.region);
                if (d < min_d) { min_d = d; best_u = i; }
            }

            if (best_u != -1) {
                int D = min_d; 

                if (!is_safe_against_all_in(S, M, P, cand.region, cand.cost, cand.is_new_base, D)) {
                    continue; 
                }

                int projected_real_gold = base_budget + (D * estimated_income);
                int required_gold = cand.cost + (D * MOVE_COST);
                
                if (projected_real_gold >= required_gold) {
                    build_plans[free_units[best_u].id] = {cand.region, cand.cost};
                    current_free_count--;
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
            int reserved_move = get_locked_move_gold(S, M); // [THÊM MỚI] Lấy quỹ tiền di chuyển
            if (w_copy.region == target_region && w_copy.state == WState::STATIONARY) {
                bool already_upgrading = std::find(a.upgrades.begin(), a.upgrades.end(), target_region) != a.upgrades.end();
                
                if (!already_upgrading && (virtual_gold - reserved_move) >= actual_cost + total_upkeep) {
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

        if (!has_money_advantage) {
            std::vector<int> bases_to_check;
            if (hq_b) bases_to_check.push_back(hq_b->region);
            for (const auto& b : my_bases) bases_to_check.push_back(b.region);

            for (int reg : bases_to_check) {
                int work_cap = 0;
                for (const auto& b : S.buildings) {
                    if (b.region == reg && b.side == M.my_side) {
                        work_cap = b.work_cap(); break;
                    }
                }

                int current_workers = 0;
                for (const auto& w : my_warriors) {
                    if (w.region == reg || (persistent_jobs.count(w.id) && persistent_jobs[w.id] == reg)) {
                        current_workers++;
                    }
                }

                while (current_workers < work_cap) {
                    std::vector<Warrior> current_free = get_free_units(S);
                    if (current_free.empty()) break;

                    int best_w = -1; int min_h = 9999;
                    int free_count = current_free.size();
                    for (size_t i = 0; i < current_free.size(); ++i) {
                        // [SỬA LẠI] Gọi hàm có thêm biến S
                        if (!is_safe_to_dispatch(S, current_free[i], reg, free_count, M, P)) continue;
                        int h = get_hops(P, current_free[i].region, reg);
                        if (h < min_h) { min_h = h; best_w = i; }
                    }

                    if (best_w != -1) {
                        persistent_jobs[current_free[best_w].id] = reg;
                        current_workers++;
                        if (virtual_job_slots.count(reg) && virtual_job_slots[reg] > 0) {
                            virtual_job_slots[reg]--;
                        }
                    } else {
                        break;
                    }
                }
            }
        }

        std::vector<Warrior> free_units = get_free_units(S);
        int current_free_count = free_units.size();

        std::vector<int> remaining_jobs;
        for (auto const& [r, count] : virtual_job_slots) {
            for (int i = 0; i < count; ++i) remaining_jobs.push_back(r);
        }

        while (!free_units.empty() && !remaining_jobs.empty()) {
            int best_w = -1, best_j = -1; int min_h = 9999;
            for (size_t i = 0; i < free_units.size(); ++i) {
                for (size_t j = 0; j < remaining_jobs.size(); ++j) {
                    // [SỬA LẠI] Gọi hàm có thêm biến S
                    if (!is_safe_to_dispatch(S, free_units[i], remaining_jobs[j], current_free_count, M, P)) continue;

                    int h = get_hops(P, free_units[i].region, remaining_jobs[j]);
                    if (h < min_h) { min_h = h; best_w = i; best_j = j; }
                }
            }
            if (best_w != -1) {
                persistent_jobs[free_units[best_w].id] = remaining_jobs[best_j];
                current_free_count--;
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
        int spendable_gold = get_spendable_gold(S, M);
        
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

        int best_fwd_base = -1;
        int min_d_opp = 999;
        bool has_center = false;

        for (const auto& b : my_bases) {
            if (b.region == M.center_region) has_center = true;
            int d_opp = get_hops(P, b.region, M.opp_hq);
            int d_my = get_hops(P, b.region, M.my_hq);
            if (d_opp < d_my) { 
                if (d_opp < min_d_opp) {
                    min_d_opp = d_opp;
                    best_fwd_base = b.region;
                }
            }
        }

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
                    if (forward_build_plans.count(w.id)) {
                        final_target = forward_build_plans[w.id].first;
                    } else if (build_plans.count(w.id)) {
                        final_target = build_plans[w.id].first;
                    } else if (persistent_jobs.count(w.id)) {
                        final_target = persistent_jobs[w.id]; 
                    } else {
                        {
                            int d_to_opp = get_hops(P, w.region, M.opp_hq);
                            int d_to_my = get_hops(P, w.region, M.my_hq);
                            if (d_to_opp < d_to_my) {
                                final_target = w.region;
                            } else {
                                if (has_center) {
                                    final_target = M.center_region;
                                } else {
                                    final_target = M.my_hq;
                                }
                            }
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
        
        int my_net_income = get_my_net_income(S, M, my_warriors.size());
        int opp_net_income = get_enemy_net_income(S, M, enemy_warriors.size());
        has_money_advantage = (my_net_income > opp_net_income + TUNE_GOLD_ADVANTAGE );
        
        bool cond1 = (opp_hq_b && hq_b && opp_hq_b->level > hq_b->level && my_net_income >= opp_net_income);
        is_rushing_hq = (cond1 || has_money_advantage);

        bool hq_fast_tracked = false;
        if (is_rushing_hq && hq_b && hq_b->level < custom_hq_max_level) {
            bool has_unit_at_hq = false;
            for (const auto& w : my_warriors) {
                if (w.region == M.my_hq) { has_unit_at_hq = true; break; }
            }
            if (has_unit_at_hq) {
                int cost = HQ_LEVELS[hq_b->level + 1].upgrade_cost;
                if (virtual_gold >= cost + total_upkeep) {
                    a.upgrades.push_back(M.my_hq);
                    virtual_gold -= cost;
                    hq_fast_tracked = true;
                }
            }
        }

        plan_forward_expansion(S, M, P);
        process_forward_build_plans(S, M, P, a);

        plan_attacks(S, M, P, turn);
        detect_and_handle_emergencies(S, M, P); 
        
        plan_expansion(S, M, P, hq_fast_tracked);
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

int main() {
  GameMap M; GameState S; parse_init(M, S); Paths P = calculate_paths(M); 
  int turn;
  while (read_turn_start(turn)) {
    Actions a = decide(S, M, P, turn);
    emit_command(); emit_actions(a); emit_end(); read_turn_result(S, M, a);
  }
  return 0;
}