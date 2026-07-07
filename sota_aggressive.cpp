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
    // tune parameter
    const int TUNE_MAX_FORWARD_HOLDERS = 2;       // Số lính tối đa đứng giữ chỗ trống bên sân địch
    const int TUNE_MAX_FORWARD_LABOR = 2;         // Giới hạn tổng số labor (work_cap) tối đa bên sân địch
    const int RESERVE_GOLD_ATTACK = 50; 
    const double TUNE_SCORE_REQ_WEIGHT = 10.0;    
    const double TUNE_SCORE_DIST_WEIGHT = 1.0;    
    const double TUNE_BASE_TARGET_BONUS = 200.0;   
    const double TUNE_WEAK_BASE_BONUS = 500.0;    
    const double TUNE_HQ_TARGET_BONUS = 0.0;      
    const double TUNE_MAX_ARMY_RATIO = 0.9;       
    const int TUNE_GOLD_ADVANTAGE = 45;
    const int TUNE_MIN_LABOR_TO_FIGHT = 3; 
    const int TUNE_INCOME_THRESHOLD_TURN = 100;   // Mốc lượt chơi để thay đổi ngưỡng income
    const int TUNE_MIN_INCOME_EARLY = 30;        // Ngưỡng income đầu game
    const int TUNE_MIN_INCOME_LATE =  50;        // Ngưỡng income cần thiết sau mốc TUNE_INCOME_THRESHOLD_TURN
    const int TUNE_MAX_CONCURRENT_BUILDS = 3;
    const int TUNE_MAX_CONCURRENT_ATTACKS = 4;
    const double TUNE_ENEMY_IN_MY_HALF_BONUS = 100000.0;

    // --- Các thông số mới cho Flag thích nghi phòng thủ ---
    const int TUNE_STRONG_DEFENSE_TIMEOUT = 15; // Số lượt bật cờ khi phát hiện địch thủ mạnh
    int strong_defense_turns_left = 0;          // Cờ: > 0 là Địch thủ mạnh, = 0 là Địch thủ yếu
    
    // Snowball / Attack parameters
    const int TUNE_SNOWBALL_INCOME_GAP = 75;      
    const int TUNE_SNOWBALL_BUFFER = 4;           
    const int TUNE_MISSION_TIMEOUT = 30;          // Cơ chế timeout cho nhiệm vụ đánh
    
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
        int reserved_gold = 0; // Quỹ đen xin ứng trước để di chuyển cuối cùng
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

    // THÊM BIẾN MỚI
    bool is_absolute_all_in = false;

    // [Ý 2] HÀM GIẢ LẬP ĐUA LEVEL HQ ĐẾN CUỐI GAME
    bool check_hq_race_to_end(const GameState& S, const GameMap& M, int turn) {
        int sim_my_gold = virtual_gold;
        int sim_opp_gold = opp_gold;
        int my_net = get_my_net_income(S, M, my_warriors.size());
        int opp_net = get_enemy_net_income(S, M, enemy_warriors.size());
        
        int my_lvl = hq_b ? hq_b->level : 1;
        int opp_lvl = opp_hq_b ? opp_hq_b->level : 1;
        
        for (int t = turn; t < 200; ++t) {
            sim_my_gold += my_net;
            sim_opp_gold += opp_net;
            sim_my_gold = std::max(0, sim_my_gold);
            sim_opp_gold = std::max(0, sim_opp_gold);
            
            if (my_lvl < custom_hq_max_level && sim_my_gold >= HQ_LEVELS[my_lvl + 1].upgrade_cost) {
                sim_my_gold -= HQ_LEVELS[my_lvl + 1].upgrade_cost;
                my_lvl++;
            }
            if (opp_lvl < HQ_MAX_LEVEL && sim_opp_gold >= HQ_LEVELS[opp_lvl + 1].upgrade_cost) {
                sim_opp_gold -= HQ_LEVELS[opp_lvl + 1].upgrade_cost;
                opp_lvl++;
            }
        }
        // Trả về true nếu cuối game mình bằng hoặc hơn cấp độ địch (và ít nhất phải có tài chính duy trì)
        return (my_lvl >= opp_lvl && my_lvl > 1); 
    }

    // [Ý 4] HÀM GIẢ LẬP ABSOLUTE ALL-IN
    bool check_absolute_all_in(const GameState& S, const GameMap& M, const Paths& P, int turn) {
        if (turn >= 200) return false;
        
        int opp_hq_hp = opp_hq_b ? opp_hq_b->hp : 10;
        int opp_hq_ad = get_b_ad(S, M.opp_hq);
        int my_hp_val = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        int opp_hp_val = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int opp_train_cap = opp_hq_b ? opp_hq_b->train_cap() : 1;
        int my_train_cap = hq_b ? hq_b->train_cap() : 1;
        
        std::vector<int> init_e_hp_counts(10, 0);
        for (const auto& ew : enemy_warriors) {
            if (ew.region == M.opp_hq && ew.hp > 0 && ew.hp <= 9) {
                init_e_hp_counts[ew.hp]++;
            }
        }
        
        // Thời gian lính hiện tại tới HQ địch (KHÔNG GỌI THỢ MỎ)
        std::map<int, int> my_arrivals;
        for (const auto& w : my_warriors) {
            if (!persistent_jobs.count(w.id)) {
                int d = get_hops(P, w.region, M.opp_hq);
                if (d != 999) my_arrivals[d]++;
            }
        }
        
        int my_gross = get_my_gross_income(S, M);
        int opp_gross = get_enemy_gross_income(S, M);
        int my_hq_to_opp_hq = get_hops(P, M.my_hq, M.opp_hq);
        
        auto calc_T_win = [&]() -> int {
            std::vector<int> cur_e = init_e_hp_counts; 
            std::vector<int> cur_m(10, 0);             
            int cur_base_hp = opp_hq_hp;
            int sim_mg = virtual_gold;
            int sim_og = opp_gold;
            int sim_my_total_units = my_warriors.size();
            int sim_opp_total_units = enemy_warriors.size();
            
            for(int day = 0; day < 200 - turn; ++day) {
                // Ta train lính
                int my_spawn = std::min(my_train_cap, sim_mg / TRAIN_COST);
                sim_mg -= my_spawn * TRAIN_COST;
                sim_my_total_units += my_spawn;
                if (my_hq_to_opp_hq != 999 && my_spawn > 0) my_arrivals[day + my_hq_to_opp_hq] += my_spawn;
                sim_mg = std::max(0, sim_mg + (my_gross - sim_my_total_units * UPKEEP_PER_WARRIOR));
                
                // Địch train lính
                int opp_spawn = std::min(opp_train_cap, sim_og / TRAIN_COST);
                sim_og -= opp_spawn * TRAIN_COST;
                sim_opp_total_units += opp_spawn;
                sim_og = std::max(0, sim_og + (opp_gross - sim_opp_total_units * UPKEEP_PER_WARRIOR));
                cur_e[opp_hp_val] += opp_spawn;
                
                // Lính ta tới nơi
                if (my_arrivals.count(day)) {
                    cur_m[my_hp_val] += my_arrivals[day];
                }
                
                int m_count = 0;
                for(int h = 1; h <= 9; ++h) m_count += cur_m[h];
                
                if (cur_base_hp <= 0) return day; // Nổ HQ địch
                if (day > 50 && m_count == 0) return 999; // Hết lính đẩy
                
                simulate_combat_step(cur_m, cur_e, cur_base_hp, opp_hq_ad);
            }
            return cur_base_hp <= 0 ? (200 - turn) : 999;
        };
        
        int T_us = calc_T_win();
        if (T_us == 999) return false; 
        
        // TÍNH THỜI GIAN ĐỊCH PHÁ HQ TA (WORST CASE)
        std::vector<int> total_e_hps(10, 0);
        int min_d_them = 999;
        for (const auto& ew : enemy_warriors) {
            if (ew.region != M.opp_hq) { 
                if (ew.hp > 0 && ew.hp <= 9) total_e_hps[ew.hp]++;
                int d = get_hops(P, ew.region, M.my_hq);
                if (d < min_d_them) min_d_them = d;
            }
        }
        int total_them_count = 0;
        for(int h=1; h<=9; ++h) total_them_count += total_e_hps[h];
        if (total_them_count == 0 || min_d_them == 999) return true; 
        
        int my_hq_hp_sim = hq_b ? hq_b->hp : 10;
        int my_hq_ad = get_b_ad(S, M.my_hq);
        
        auto calc_T_lose = [&]() -> int {
            std::vector<int> cur_e = total_e_hps; 
            std::vector<int> cur_m(10, 0); 
            int cur_base_hp = my_hq_hp_sim;
            
            for(int day = 0; day < 200 - turn; ++day) {
                if (day >= min_d_them) {
                    int e_count = 0;
                    for(int h = 1; h <= 9; ++h) e_count += cur_e[h];
                    if (cur_base_hp <= 0) return day;
                    if (e_count == 0) return 999;
                    simulate_combat_step(cur_e, cur_m, cur_base_hp, my_hq_ad);
                }
            }
            return cur_base_hp <= 0 ? (200 - turn) : 999;
        };
        
        int T_them = calc_T_lose();
        return T_us < T_them; // Phải kết liễu địch trước khi nhà mình cháy
    }

    int get_dynamic_min_income_to_attack(int turn) const {
        if (turn < TUNE_INCOME_THRESHOLD_TURN) {
            return TUNE_MIN_INCOME_EARLY;
        } else {
            return TUNE_MIN_INCOME_LATE;
        }
    }
    void simulate_combat_step(
        std::vector<int>& atk_counts, // Tần suất HP quân công
        std::vector<int>& def_counts, // Tần suất HP quân thủ
        int& def_base_hp,
        int def_base_ad
    ) const {
        int a_alive = 0, d_alive = 0;
        for (int h = 1; h <= 9; ++h) {
            a_alive += atk_counts[h];
            d_alive += def_counts[h];
        }

        if (a_alive == 0) return; // Nếu quân công chết hết, không có sát thương nào được gây ra

        int dmg_to_def = a_alive;
        // Lấy sát thương của tháp pháo cộng thêm vào nếu Base còn sống
        int dmg_to_atk = d_alive + (def_base_hp > 0 ? def_base_ad : 0);

        // 1. Quân công đánh quân thủ & Base
        for (int i = 0; i < dmg_to_def; ++i) {
            bool hit = false;
            // Tìm quân thủ có HP thấp nhất để đánh
            for (int h = 1; h <= 9; ++h) {
                if (def_counts[h] > 0) {
                    def_counts[h]--;
                    def_counts[h - 1]++; // Unit rớt xuống mức HP thấp hơn (về 0 tức là chết ở cuối ngày)
                    hit = true; break;
                }
            }
            // Nếu không còn lính thủ, đánh vào Base
            if (!hit && def_base_hp > 0) def_base_hp--;
        }

        // 2. Quân thủ & Base đánh quân công
        for (int i = 0; i < dmg_to_atk; ++i) {
            // Tìm quân công có HP thấp nhất để đánh
            for (int h = 1; h <= 9; ++h) {
                if (atk_counts[h] > 0) {
                    atk_counts[h]--;
                    atk_counts[h - 1]++;
                    break;
                }
            }
        }
    }

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

    int calculate_min_defenders(const std::vector<int>& enemy_hps, int base_hp, int base_ad, int my_hp_val) const {
        if (enemy_hps.empty()) return 0;
        
        std::vector<int> init_e_hp_counts(10, 0);
        for(int hp : enemy_hps) if(hp > 0 && hp <= 9) init_e_hp_counts[hp]++;
            
        for (int D = 0; D <= (int)enemy_hps.size() + 5; ++D) {
            std::vector<int> cur_e = init_e_hp_counts; // Lính địch (Quân công)
            std::vector<int> cur_m(10, 0);             // Lính mình (Quân thủ)
            if (D > 0) cur_m[my_hp_val] = D;
            int cur_b_hp = base_hp;

            for(int day = 0; day < 200; ++day) {
                int e_count = 0;
                for(int h = 1; h <= 9; ++h) e_count += cur_e[h];
                
                if (e_count == 0 || cur_b_hp <= 0) break;

                // Kẻ địch là phe Công, Mình là phe Thủ
                simulate_combat_step(cur_e, cur_m, cur_b_hp, base_ad);
            }
            // Nếu căn cứ sống sót, trả về số lính D
            if (cur_b_hp > 0) return D;
        }
        return enemy_hps.size();
    }

    // --- CƠ CHẾ BASE RACE CHUYÊN BIỆT CHO HQ ---
    // Kiểm tra giả định "Nếu ta All-in HQ địch, địch All-in HQ ta, bên nào thắng?"
    bool simulate_base_race(const GameState& S, const GameMap& M, const Paths& P, int A, int rp) const {
        int d_us = get_hops(P, rp, M.opp_hq);
        if (d_us == 999) return false;
        
        // Chỉ số thực tế của HQ địch
        int opp_hq_hp = opp_hq_b ? opp_hq_b->hp : 10;
        int opp_hq_ad = get_b_ad(S, M.opp_hq);
        
        std::vector<int> init_e_hp_counts(10, 0);
        for (const auto& ew : enemy_warriors) {
            if (ew.region == M.opp_hq && ew.hp > 0 && ew.hp <= 9) {
                init_e_hp_counts[ew.hp]++;
            }
        }
        
        int my_hp_val = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        
        // TÍNH SỐ NGÀY TA PHÁ XONG HQ ĐỊCH
        auto calc_T_win = [&]() -> int {
            std::vector<int> cur_e = init_e_hp_counts; // Địch (Quân thủ)
            std::vector<int> cur_m(10, 0);             // Mình (Quân công)
            if (A > 0) cur_m[my_hp_val] = A;
            int cur_base_hp = opp_hq_hp;
            
            for(int day = 0; day < 200; ++day) {
                if (day >= d_us) {
                    int m_count = 0;
                    for(int h = 1; h <= 9; ++h) m_count += cur_m[h];
                    
                    if (cur_base_hp <= 0) return day;
                    if (m_count == 0) return 999; // Lính mình chết sạch trước khi phá xong
                    
                    // Ta là phe Công, Địch là phe Thủ
                    simulate_combat_step(cur_m, cur_e, cur_base_hp, opp_hq_ad);
                }
            }
            return cur_base_hp <= 0 ? 200 : 999;
        };
        
        int T_us = calc_T_win();
        if (T_us == 999) return false; 
        
        // TÍNH SỐ NGÀY ĐỊCH PHÁ XONG HQ TA
        std::vector<int> total_e_hps(10, 0);
        int min_d_them = 999;
        for (const auto& ew : enemy_warriors) {
            if (ew.region != M.opp_hq) { 
                if (ew.hp > 0 && ew.hp <= 9) total_e_hps[ew.hp]++;
                int d = get_hops(P, ew.region, M.my_hq);
                if (d < min_d_them) min_d_them = d;
            }
        }
        
        int total_them_count = 0;
        for(int h=1; h<=9; ++h) total_them_count += total_e_hps[h];
        
        if (total_them_count == 0 || min_d_them == 999) return true;
        
        // Chỉ số thực tế của HQ mình
        int my_hq_hp_sim = hq_b ? hq_b->hp : 10;
        int my_hq_ad = get_b_ad(S, M.my_hq);
        
        auto calc_T_lose = [&]() -> int {
            std::vector<int> cur_e = total_e_hps; // Địch (Quân công)
            std::vector<int> cur_m(10, 0);        // Mình (Quân thủ - giả định ko có lính thủ để tính worst-case)
            int cur_base_hp = my_hq_hp_sim;
            
            for(int day = 0; day < 200; ++day) {
                if (day >= min_d_them) {
                    int e_count = 0;
                    for(int h = 1; h <= 9; ++h) e_count += cur_e[h];
                    
                    if (cur_base_hp <= 0) return day;
                    if (e_count == 0) return 999;
                    
                    // Địch là phe Công, Mình là phe Thủ
                    simulate_combat_step(cur_e, cur_m, cur_base_hp, my_hq_ad);
                }
            }
            return cur_base_hp <= 0 ? 200 : 999;
        };
        
        int T_them = calc_T_lose();
        return T_us <= T_them;
    }

    bool is_safe_against_all_in(const GameState &S, const GameMap &M, const Paths &P, 
                                int target_region, int build_cost, bool is_new_base, int d_build) {
        
        int max_t_wait = 20; 
        
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

            // [TỐI ƯU MỚI] Tính toán trước mảng thời gian lính về tới cứ điểm
            std::vector<int> defender_arrival_times;
            
            // 1. Lính tự do (Free units)
            std::vector<Warrior> free_units = get_free_units(S);
            for (const auto& w : free_units) {
                defender_arrival_times.push_back(get_hops(P, w.region, base_reg));
            }
            
            // 2. Lính đang nằm trong các Mission
            for (const auto& m : active_missions) {
                if (!m.is_launched) {
                    // Mission chưa xuất kích: Lính có thể hủy nhiệm vụ và về thủ ngay lập tức
                    for (auto wid : m.squad_ids) {
                        for (const auto& w : my_warriors) {
                            if (w.id == wid) { 
                                defender_arrival_times.push_back(get_hops(P, w.region, base_reg)); 
                                break; 
                            }
                        }
                    }
                } else {
                    // Mission ĐÃ xuất kích: Phải đập xong Base địch rồi mới quay về
                    int target_base_hp = 18; // Default an toàn nếu không đọc được (Base lv3)
                    for (const auto& b : S.buildings) {
                        if (b.region == m.target && b.side != M.my_side) {
                            target_base_hp = b.hp;
                            break;
                        }
                    }
                    
                    // Ước lượng thời gian đập base = (HP base địch / Số lính tấn công) + 2 lượt buffer
                    int attackers = std::max(1, (int)m.squad_ids.size());
                    int combat_duration = (target_base_hp / attackers) + 2; 

                    for (auto wid : m.squad_ids) {
                        for (const auto& w : my_warriors) {
                            if (w.id == wid) {
                                int t_to_target = get_hops(P, w.region, m.target);
                                int t_target_to_base = get_hops(P, m.target, base_reg);
                                
                                if (t_to_target != 999 && t_target_to_base != 999) {
                                    // Tổng thời gian = Đi tới mục tiêu + Đập nhà + Đi về nhà
                                    defender_arrival_times.push_back(t_to_target + combat_duration + t_target_to_base);
                                }
                                break;
                            }
                        }
                    }
                }
            }

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
                std::vector<int> sim_e_hps(sim_e_pool, e_hp_val);
                int req_def = calculate_min_defenders(sim_e_hps, b_hp, b_ad, m_hp_val);
                if (req_def <= 0) continue; 

                int my_sim_gold = virtual_gold - build_cost; 
                int my_train_cap = hq_b ? hq_b->train_cap() : 1;
                int current_my_gross_inc = get_my_gross_income(S, M); 
                int total_my_units = my_warriors.size();
                
                int max_defenders = 0;
                
                // [TINH CHỈNH MỚI] Kiểm tra lính về kịp lúc T_impact thông qua mảng đã tính trước
                for (int arr_time : defender_arrival_times) {
                    if (arr_time <= T_impact) {
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

    int get_locked_gold() const {
        int locked = 0;
        for (const auto& p : build_plans) locked += p.second.second;
        for (const auto& p : forward_build_plans) locked += p.second.second; 
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
                !in_mission && !build_plans.count(w.id) && !forward_build_plans.count(w.id) && !persistent_jobs.count(w.id)) {
                free_units.push_back(w);
            }
        }
        return free_units;
    }

    bool is_safe_to_dispatch(const GameState& S, const Warrior& w, int tgt, int current_free_count, const GameMap& M, const Paths& P) const {
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

        int e_hp = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int m_hp = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        int hq_ad = get_b_ad(S, M.my_hq);
        int hq_hp = hq_b ? hq_b->hp : 10;
        
        std::vector<int> baseline_e_hps(baseline_invading_enemies, e_hp);
        int dynamic_req_defenders = calculate_min_defenders(baseline_e_hps, hq_hp, hq_ad, m_hp);

        // [TINH CHỈNH] Tính chính xác số lượng lính thủ (bao gồm current_free_count và lính chưa launch)
        int unlaunched_count = 0;
        for (const auto& m : active_missions) {
            if (!m.is_launched) unlaunched_count += m.squad_ids.size();
        }
        
        int free_after = current_free_count + unlaunched_count - 1; 
        int trip_time = 2 * get_hops(P, w.region, tgt);
        int max_dist = get_hops(P, w.region, M.opp_hq) + 1;
        
        return (free_after >= dynamic_req_defenders) || (trip_time <= max_dist);
    }

    int calculate_req_attackers(int final_target, int enemy_reaction_window, const GameState& S, const GameMap& M, const Paths& P) const {
        bool is_hq = (final_target == M.opp_hq);

        int my_net_income = get_my_net_income(S, M, my_warriors.size());
        int opp_net_income = get_enemy_net_income(S, M, enemy_warriors.size());

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
            int required = defending_enemies + (b_hp + my_hp_val - 1) / my_hp_val + TUNE_SNOWBALL_BUFFER;
            return required;
        }

        int my_hp = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;
        int b_hp = 0, b_ad = 0;

        for (const auto& b : S.buildings) {
            if (b.region == final_target && b.side != M.my_side) {
                b_hp = b.hp; b_ad = get_b_ad(S, final_target); break;
            }
        }

        std::set<WarriorId> busy_enemies;
        for (const auto& g : active_enemy_attacks) {
            if (g.type == ThreatType::GATHERING && g.current_region == final_target) continue; 
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
        std::vector<int> init_def_hps(10, 0);
        std::map<int, int> local_enemy_counts;
        
        for(auto& ew : enemy_warriors) {
            if (busy_enemies.count(ew.id)) continue;
            if (ew.region == final_target) {
                if(ew.hp > 0 && ew.hp <= 9) init_def_hps[ew.hp]++;
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
            std::vector<int> cur_e = init_def_hps;
            int cur_base_hp = b_hp;
            int total_sim_e_units = enemy_warriors.size(); 
            std::vector<int> cur_m(10, 0);
            if(A > 0) cur_m[my_hp] = A;
            
            std::map<int, int> sim_reinforcements = reinforcements_by_day;

            // [TINH CHỈNH] Thay dist_from_us bằng enemy_reaction_window
            for(int day = 0; day < 50 + enemy_reaction_window; ++day) {
                bool enemy_pulls_reinforcements = true;
                bool enemy_trains_defenders = true;
                
                if (strong_defense_turns_left == 0) { 
                    if (!is_hq) {
                        enemy_pulls_reinforcements = false; 
                        enemy_trains_defenders = false;
                    } else {
                        enemy_pulls_reinforcements = (day >= enemy_reaction_window - 1); 
                        enemy_trains_defenders = (day >= enemy_reaction_window - 1); 
                    }
                }

                if (sim_reinforcements.count(day)) {
                    int r = sim_reinforcements[day];
                    if (enemy_pulls_reinforcements) cur_e[opp_hp_per_unit] += r;
                }

                if (opp_hq_b && cur_base_hp > 0) {
                    int spawns = std::min(opp_train_cap, sim_gold / TRAIN_COST);
                    if (enemy_trains_defenders) { 
                        sim_gold -= spawns * TRAIN_COST;
                        total_sim_e_units += spawns;
                        if (is_hq) {
                            if (day >= 1) cur_e[opp_hp_per_unit] += spawns;
                        } else {
                            if (day >= 1) { 
                                int d_hq = get_hops(P, M.opp_hq, final_target);
                                sim_reinforcements[day + d_hq] += spawns;
                            }
                        }
                    }
                }
                
                if (day >= enemy_reaction_window) {
                    int e_count = 0, m_count = 0;
                    for(int h=1; h<=9; ++h) { e_count += cur_e[h]; m_count += cur_m[h]; }
                    
                    if (cur_base_hp <= 0) return true; 
                    if (m_count == 0) return false; 
                    
                    simulate_combat_step(cur_m, cur_e, cur_base_hp, b_ad);
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
        
        // [SỬA LỖI Ở ĐÂY] Tách bạch Quỹ chung và Quỹ đen ngay từ đầu lượt
        virtual_gold = S.gold; 
        for (const auto& m : active_missions) {
            virtual_gold -= m.reserved_gold;
        }
        if (virtual_gold < 0) virtual_gold = 0; // Đề phòng bug lạ

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

        // ========================================================
        // [CẬP NHẬT] GIẢI PHÓNG QUÂN GIỮ CHỖ THỪA (FORWARD HOLDERS)
        // ========================================================
        int forward_holders_count = 0;
        std::vector<WarriorId> excess_holders;

        auto check_forward_holders = [&](const std::map<WarriorId, std::pair<int, int>>& plans) {
            for (auto const& [wid, plan] : plans) {
                int reg = plan.first;
                // Nếu mục tiêu nằm bên sân địch
                if (get_hops(P, reg, M.my_hq) > get_hops(P, reg, M.opp_hq)) {
                    bool has_b = false;
                    for (const auto& b : S.buildings) if (b.region == reg) has_b = true;
                    
                    if (!has_b) { // Là cứ điểm trống
                        forward_holders_count++;
                        // Nếu vượt giới hạn, tước lệnh của lính này
                        if (forward_holders_count > TUNE_MAX_FORWARD_HOLDERS) {
                            excess_holders.push_back(wid);
                        }
                    }
                }
            }
        };

        check_forward_holders(forward_build_plans);
        check_forward_holders(build_plans);

        // Xóa lệnh, lính sẽ tự động biến thành Free Units trong lượt này
        for (auto wid : excess_holders) {
            forward_build_plans.erase(wid);
            build_plans.erase(wid);
        }
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
            
            if (count <= labor_cap) {
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
                        std::vector<int> excess_e_hps(excess, e_hp_val);
                        int min_def_needed = calculate_min_defenders(excess_e_hps, weakest_base_hp, weakest_base_ad, HQ_LEVELS[hq_b ? hq_b->level : 1].warrior_hp);
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

    void assign_predictive_defenders(const GameState &S, const GameMap &M, const Paths &P,
                                     const std::map<int, int>& req_per_base,
                                     const std::map<int, std::set<WarriorId>>& e_ids_per_base,
                                     const std::map<int, int>& min_dist_per_base) {
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

        for (auto const& [tgt, req] : req_per_base) {
            if (req > 0) {
                int b_level = 1;
                for (const auto& b : S.buildings) {
                    if (b.region == tgt) { b_level = b.level; break; }
                }
                int priority = (tgt == M.my_hq) ? 1000 : (b_level * 10);
                int max_cap = (tgt == M.my_hq) ? 9999 : (b_level * 5 + 5); 
                active_threats.push_back({tgt, min_dist_per_base.at(tgt), e_ids_per_base.at(tgt), req, priority, max_cap});
            }
        }

        std::sort(active_threats.begin(), active_threats.end(), [&](const Threat& a, const Threat& b) {
            if(a.enemy_dist == b.enemy_dist) return a.priority > b.priority;
            return a.enemy_dist < b.enemy_dist;
        });

        std::vector<Warrior> available_defenders = get_free_units(S);
        int train_cap_per_turn = hq_b ? hq_b->train_cap() : 1;
        
        int sim_gold = get_spendable_gold(); 
        int my_inc = std::max(0, get_my_gross_income(S, M) - total_upkeep); 

        for (const auto& threat : active_threats) {
            int existing = 0;
            // 1. Lính chuyên thủ đã được gán nhãn từ trước
            for (auto const& [my_id, my_tgt] : emergency_targets) {
                if (my_tgt == threat.target) existing++;
            }
            
            // 2. [THÊM MỚI] Đếm luôn lính Labor (Farm/Build) đang ở hoặc sẽ về kịp Base này
            for (const auto& w : my_warriors) {
                if (!emergency_targets.count(w.id)) {
                    int job_tgt = -1;
                    if (persistent_jobs.count(w.id)) job_tgt = persistent_jobs.at(w.id);
                    else if (build_plans.count(w.id)) job_tgt = build_plans.at(w.id).first;
                    else if (forward_build_plans.count(w.id)) job_tgt = forward_build_plans.at(w.id).first;
                    
                    if (job_tgt == threat.target) {
                        // Kịp tham chiến thì đếm luôn vào lực lượng phòng thủ
                        if (get_hops(P, w.region, threat.target) <= threat.enemy_dist + 1) {
                            existing++;
                        }
                    }
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

            int total_available = (int)can_arrive_in_time.size() + future_trainable;

            if (total_available < needed) {
                int shortage = needed - total_available;

                for (auto& m : active_missions) {
                    if (shortage <= 0) break;
                    if (m.target == M.opp_hq && simulate_base_race(S, M, P, m.squad_ids.size(), m.rally_point)) {
                        continue;
                    }
                    if (m.is_launched) continue;
                    for (auto it_id = m.squad_ids.begin(); it_id != m.squad_ids.end(); ) {
                        if (shortage <= 0) break;

                        WarriorId wid = *it_id;
                        const Warrior* w_ptr = nullptr;
                        for (const auto& w : my_warriors) {
                            if (w.id == wid) { w_ptr = &w; break; }
                        }

                        if (w_ptr && get_hops(P, w_ptr->region, threat.target) <= threat.enemy_dist + 1) {
                            it_id = m.squad_ids.erase(it_id);
                            
                            if (m.reserved_gold >= MOVE_COST) {
                                m.reserved_gold -= MOVE_COST;
                                virtual_gold += MOVE_COST;
                                sim_gold += MOVE_COST; 
                            }

                            can_arrive_in_time.push_back(*w_ptr);
                            shortage--;
                            total_available++;
                        } else {
                            ++it_id;
                        }
                    }
                }
            }

            int train_needed = std::max(0, needed - (int)can_arrive_in_time.size());
            sim_gold -= train_needed * TRAIN_COST; 

            if (total_available < needed && threat.target != M.my_hq) {
                doomed_bases.insert(threat.target);
                continue; 
            }

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

        // --- BƯỚC 1: LẤY BASE LÀM TRUNG TÂM & TÍNH TỔNG UY HIẾP (Aggregated Threat) ---
        // --- BƯỚC 1: LẤY BASE LÀM TRUNG TÂM & TÍNH TỔNG UY HIẾP (Aggregated Threat) ---
        std::map<int, int> current_req_per_base;
        std::map<int, std::set<WarriorId>> current_enemy_ids_per_base;
        std::map<int, int> min_dist_per_base;

        int e_hp_val = opp_hq_b ? HQ_LEVELS[opp_hq_b->level].warrior_hp : 4;
        int m_hp_val = hq_b ? HQ_LEVELS[hq_b->level].warrior_hp : 4;

        std::vector<int> bases_to_check;
        bases_to_check.push_back(M.my_hq);
        for (const auto& b : my_bases) bases_to_check.push_back(b.region);

        // [Ý 1] Lọc ra mục tiêu gần nhất cho mỗi nhóm tấn công của địch
        std::map<int, std::vector<EnemyAttackGroup>> closest_groups_per_base;
        
        for (const auto& g : active_enemy_attacks) {
            int best_tgt = -1;
            int min_dist_to_tgt = 999;
            int best_priority = -1; // Ưu tiên HQ > Base level cao > Base level thấp
            
            for (int tgt : bases_to_check) {
                if (g.potential_targets.count(tgt)) {
                    int d = get_hops(P, g.current_region, tgt);
                    int b_level = 1;
                    for (const auto& b : S.buildings) if (b.region == tgt) { b_level = b.level; break; }
                    int priority = (tgt == M.my_hq) ? 1000 : (b_level * 10);
                    
                    if (d < min_dist_to_tgt || (d == min_dist_to_tgt && priority > best_priority)) {
                        best_tgt = tgt;
                        min_dist_to_tgt = d;
                        best_priority = priority;
                    }
                }
            }
            if (best_tgt != -1) {
                closest_groups_per_base[best_tgt].push_back(g);
            }
        }

        for (int tgt : bases_to_check) {
            std::vector<int> combined_e_hps;
            std::set<WarriorId> combined_e_ids;
            int min_d = 999;

            if (closest_groups_per_base.count(tgt)) {
                for (const auto& g : closest_groups_per_base[tgt]) {
                    for (auto id : g.ids) {
                        if (!combined_e_ids.count(id)) { 
                            combined_e_ids.insert(id);
                            for (const auto& ew : enemy_warriors) {
                                if (ew.id == id) { combined_e_hps.push_back(ew.hp); break; }
                            }
                        }
                    }
                    int d = get_hops(P, g.current_region, tgt);
                    if (d < min_d) min_d = d;
                }
            }

            if (combined_e_ids.size() > 0 && (tgt == M.my_hq || combined_e_ids.size() > 2)) {
                int b_hp = 0, b_ad = 0;
                for (const auto& b : S.buildings) {
                    if (b.region == tgt) { b_hp = b.hp; b_ad = get_b_ad(S, tgt); break; }
                }
                int req_def = calculate_min_defenders(combined_e_hps, b_hp, b_ad, m_hp_val);
                current_req_per_base[tgt] = req_def;
                current_enemy_ids_per_base[tgt] = combined_e_ids;
                min_dist_per_base[tgt] = min_d;
            } else {
                current_req_per_base[tgt] = 0;
            }
        }

        // --- BƯỚC 2: GIẢI PHÓNG QUÂN THỪA KHI ĐỊCH RẼ HƯỚNG/TÁCH NHÓM (Early Release) ---
        std::map<int, std::vector<WarriorId>> my_defenders_by_target;
        for (auto const& [my_id, tgt] : emergency_targets) {
            my_defenders_by_target[tgt].push_back(my_id);
        }

        for (auto const& [tgt, defenders] : my_defenders_by_target) {
            int req = current_req_per_base[tgt];
            
            int labor_existing = 0;
            for (const auto& w : my_warriors) {
                if (!emergency_targets.count(w.id)) {
                    int job_tgt = -1;
                    if (persistent_jobs.count(w.id)) job_tgt = persistent_jobs.at(w.id);
                    else if (build_plans.count(w.id)) job_tgt = build_plans.at(w.id).first;
                    else if (forward_build_plans.count(w.id)) job_tgt = forward_build_plans.at(w.id).first;
                    
                    if (job_tgt == tgt) labor_existing++;
                }
            }
            int total_existing = (int)defenders.size() + labor_existing;

            if (total_existing > req) {
                std::vector<std::pair<int, WarriorId>> dist_id_pairs;
                for (auto id : defenders) {
                    int d = 999;
                    for (const auto& w : my_warriors) {
                        if (w.id == id) { d = get_hops(P, w.region, tgt); break; }
                    }
                    dist_id_pairs.push_back({d, id});
                }
                std::sort(dist_id_pairs.rbegin(), dist_id_pairs.rend());
                
                int excess = std::min((int)defenders.size(), total_existing - req);
                for (int i = 0; i < excess; ++i) {
                    WarriorId release_id = dist_id_pairs[i].second;
                    emergency_targets.erase(release_id); 
                    predictive_defenders.erase(release_id);
                }
            }
            
            for (auto const& [my_id, my_tgt] : emergency_targets) {
                if (my_tgt == tgt) {
                    predictive_defenders[my_id] = current_enemy_ids_per_base[tgt];
                }
            }
        }
        
        for (auto it = predictive_defenders.begin(); it != predictive_defenders.end(); ) {
            if (!emergency_targets.count(it->first)) it = predictive_defenders.erase(it);
            else ++it;
        }

        // --- BƯỚC 3: XỬ LÝ DỰ BÁO LƯỢNG QUÂN ALL-IN BẰNG HP THỰC TẾ ---
        
        // 3.1 Tập hợp lính địch ĐÃ có trong nhóm Threat
        std::set<WarriorId> tracked_enemy_ids;
        for (const auto& g : active_enemy_attacks) {
            for (auto id : g.ids) tracked_enemy_ids.insert(id);
        }

        // 3.2 Lọc ra lính địch đang bận làm lao động (Căn cứ theo work_cap của công trình địch)
        std::set<WarriorId> laboring_enemy_ids;
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side) {
                int cap = b.work_cap();
                int assigned = 0;
                for (const auto& ew : enemy_warriors) {
                    if (ew.region == b.region && !tracked_enemy_ids.count(ew.id)) {
                        if (assigned < cap) {
                            laboring_enemy_ids.insert(ew.id);
                            assigned++;
                        }
                    }
                }
            }
        }

        // 3.3 Phần còn lại chính xác là lực lượng All-in (lấy HP thực tế)
        std::vector<int> actual_all_in_hps;
        for (const auto& ew : enemy_warriors) {
            if (!tracked_enemy_ids.count(ew.id) && !laboring_enemy_ids.count(ew.id)) {
                if (ew.hp > 0 && ew.hp <= 9) {
                    actual_all_in_hps.push_back(ew.hp);
                }
            }
        }
        
        int my_net_inc = get_my_net_income(S, M, my_warriors.size());
        int opp_net_inc = get_enemy_net_income(S, M, enemy_warriors.size());
        
        bool defend_all_bases = 1;

        std::vector<int> bases_to_defend;
        bases_to_defend.push_back(M.my_hq); 
        
        if (defend_all_bases) {
            for (const auto& b : my_bases) {
                bases_to_defend.push_back(b.region);
            }
        }
        
        current_req_defenders = 0;
        for (int reg : bases_to_defend) {
            int b_hp = 0, b_ad = 0;
            for (const auto& b : S.buildings) {
                if (b.region == reg) {
                    b_hp = b.hp;
                    b_ad = get_b_ad(S, reg);
                    break;
                }
            }
            
            // [CẬP NHẬT] Truyền thẳng mảng HP thực tế thay vì mảng baseline
            int req_def = calculate_min_defenders(actual_all_in_hps, b_hp, b_ad, m_hp_val);
            
            int labor_at_reg = 0;
            for (const auto& w : my_warriors) {
                int job_tgt = -1;
                if (persistent_jobs.count(w.id)) job_tgt = persistent_jobs.at(w.id);
                else if (build_plans.count(w.id)) job_tgt = build_plans.at(w.id).first;
                else if (forward_build_plans.count(w.id)) job_tgt = forward_build_plans.at(w.id).first;
                if (job_tgt == reg) labor_at_reg++;
            }
            
            int actual_shortage = std::max(0, req_def - labor_at_reg);
            current_req_defenders = std::max(current_req_defenders, actual_shortage);
        }

        int free_count = get_free_units(S).size();

        if (free_count < current_req_defenders) {
            emergency_train_queue += (current_req_defenders - free_count);
        }

        // --- BƯỚC 4: PHÂN BỔ PHÒNG THỦ VÀ TRIỆU HỒI KHẨN CẤP THEO THỨ TỰ ƯU TIÊN ---
        assign_predictive_defenders(S, M, P, current_req_per_base, current_enemy_ids_per_base, min_dist_per_base);

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
            
            if (min_d >= 2) { best_rp_stealth = M.my_hq; min_d_stealth = min_d; }

            for (const auto& b : my_bases) {
                int d = get_hops(P, b.region, tgt);
                if ( d < min_d_stealth ) { min_d_stealth = d; best_rp_stealth = b.region; }
                if (d < min_d) { min_d = d; best_rp = b.region; }
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

        // ============================================
        // [MACRO CHECKS] ĐIỀU KIỆN VĨ MÔ
        // ============================================
        bool can_attack_struct = done_building || my_bases.size() >= 2 || is_rushing_hq;
        int min_income_req = get_dynamic_min_income_to_attack(turn);
        bool can_attack_econ = (current_net_income >= min_income_req);

        // ============================================
        // 1. CẬP NHẬT CÁC NHIỆM VỤ HIỆN CÓ
        // ============================================
        for (auto it = active_missions.begin(); it != active_missions.end(); ) {
            bool target_alive = (it->target == M.opp_hq);
            if (!target_alive) {
                for (auto& b : S.buildings) if (b.region == it->target && b.side != M.my_side) target_alive = true; 
            }
            
            if (!target_alive || (it->is_launched && it->squad_ids.empty())) { 
                if (target_alive && it->is_launched && it->squad_ids.empty()) {
                    strong_defense_turns_left = TUNE_STRONG_DEFENSE_TIMEOUT; 
                }
                virtual_gold += it->reserved_gold;
                it = active_missions.erase(it); 
                continue; 
            }
            
            if (it->is_launched) { ++it; continue; }

            if (turn - it->created_turn > TUNE_MISSION_TIMEOUT) {
                virtual_gold += it->reserved_gold;
                it = active_missions.erase(it); 
                continue;
            }
            
            std::vector<WarriorId> ready_wids;
            for(auto wid : it->squad_ids) {
                for(auto& w : my_warriors) {
                    if(w.id == wid && w.state == WState::STATIONARY && w.region == it->rally_point) {
                        ready_wids.push_back(wid); break;
                    }
                }
            }
            int ready_count = ready_wids.size();

            // Cân đối Quỹ đen theo lượng lính đã tới Rally
            int target_reserve = ready_count * MOVE_COST;
            if (it->reserved_gold < target_reserve) {
                int diff = target_reserve - it->reserved_gold;
                int transfer = std::min(virtual_gold, diff);
                it->reserved_gold += transfer;
                virtual_gold -= transfer;
            } else if (it->reserved_gold > target_reserve) {
                virtual_gold += (it->reserved_gold - target_reserve);
                it->reserved_gold = target_reserve;
            }

            int train_cap = hq_b ? hq_b->train_cap() : 1;
            
            // Tính T_first_arrival thực tế để kích hoạt thời gian phản ứng
            int t_first_arrival = 999;
            for(auto wid : it->squad_ids) {
                for(auto& w : my_warriors) {
                    if(w.id == wid) {
                        int d = get_hops(P, w.region, it->rally_point);
                        if (d < t_first_arrival) t_first_arrival = d;
                    }
                }
            }
            if (t_first_arrival == 999) t_first_arrival = 0;

            // =========================================================
            // [GIỮ LẠI] TÌM KIẾM MỤC TIÊU TỐT HƠN (CHUYỂN MỤC TIÊU)
            // =========================================================
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
                    
                    int deficit_c = std::max(0, it->required_attackers - ready_count);
                    int t_gather_c = (deficit_c > 0) ? (deficit_c / train_cap) : 0;
                    int er_window =0;
                    if(strong_defense_turns_left >0) er_window = std::max(0, t_gather_c - t_first_arrival - 3) + dist - 1;
                    else er_window =  1;
                    int req = calculate_req_attackers(tgt, er_window, S, M, P);

                    if (ready_count >= req && req > 0) {
                        double score = 0.0;
                        if (tgt == M.opp_hq) {
                            score += 10000000.0;
                        } else {
                            int b_lvl = 1;
                            for (const auto& b : S.buildings) if (b.region == tgt) { b_lvl = b.level; break; }
                            score += 500.0 + b_lvl * 100.0;
                        }
                        score -= dist * 50.0;
                        score -= (ready_count - req) * 10.0;
                        
                        if (score > best_cand.score) {
                            best_cand = {tgt, req, score};
                        }
                    }
                }
            }

            // Thực thi việc chuyển mục tiêu hoặc cập nhật lượng quân cần thiết cho mục tiêu cũ
            if (best_cand.tgt != -1 && it->reserved_gold >= best_cand.req * MOVE_COST) {
                if (it->is_launched) locked_targets.erase(it->target);
                else gathering_targets.erase(it->target);

                it->target = best_cand.tgt;
                it->required_attackers = best_cand.req;
            } else {
                int dist_rp_tgt = get_hops(P, it->rally_point, it->target);
                int deficit = std::max(0, it->required_attackers - (int)it->squad_ids.size());
                int t_gather = (deficit > 0) ? (deficit / train_cap) : 0;
                int enemy_reaction_window = std::max(0, t_gather - t_first_arrival) + dist_rp_tgt;
                
                if(strong_defense_turns_left >0) enemy_reaction_window = std::max(0, t_gather - t_first_arrival - 3) + dist_rp_tgt - 1;
                else enemy_reaction_window = 1;

                it->required_attackers = calculate_req_attackers(it->target, enemy_reaction_window, S, M, P);
            }

            // =========================================================
            // CHỐT CHẶN AN TOÀN TRƯỚC KHI XUẤT KÍCH (STRICT LAUNCH CHECK)
            // =========================================================
            bool can_launch = false;
            if (ready_count >= it->required_attackers && it->required_attackers > 0 && it->reserved_gold >= it->required_attackers * MOVE_COST) {
                if (it->target == M.opp_hq) {
                    can_launch = simulate_base_race(S, M, P, it->required_attackers, it->rally_point);
                } else {
                    can_launch = true;
                    std::vector<int> bases_to_check = {M.my_hq};
                    for (auto& b : my_bases) bases_to_check.push_back(b.region);
                    for (int reg : bases_to_check) {
                        if (!is_safe_against_all_in(S, M, P, reg, 0, false, 0)) {
                            can_launch = false; break;
                        }
                    }
                }
            }

            if (can_launch) {
                gathering_targets.erase(it->target);
                locked_targets.insert(it->target);
                it->is_launched = true;
                
                int old_size = it->squad_ids.size();
                it->squad_ids.clear();
                for (int i = 0; i < it->required_attackers; i++) {
                    it->squad_ids.insert(ready_wids[i]);
                }
                current_free_count += (old_size - it->required_attackers);
                
                int needed_gold = it->required_attackers * MOVE_COST;
                if (it->reserved_gold > needed_gold) {
                    virtual_gold += (it->reserved_gold - needed_gold);
                    it->reserved_gold = needed_gold;
                }
            } else {
                int current_squad_size = it->squad_ids.size();
                // Bổ sung lính nếu thiếu (Và kinh tế cho phép)
                if (current_squad_size < it->required_attackers && can_attack_econ) {
                    std::vector<Warrior> atk_units = get_attack_free_units();
                    std::sort(atk_units.begin(), atk_units.end(), [&](const Warrior& a, const Warrior& b) {
                        return get_hops(P, a.region, it->rally_point) < get_hops(P, b.region, it->rally_point);
                    });
                    
                    for (auto& w : atk_units) {
                        if (current_squad_size >= it->required_attackers) break;
                        if (!is_safe_to_dispatch(S, w, it->rally_point, current_free_count, M, P)) continue;
                        
                        it->squad_ids.insert(w.id); 
                        current_squad_size++;
                        current_free_count--; 
                    }
                    if (current_squad_size < it->required_attackers) {
                        // KHI ĐANG RUSH HQ -> KHÔNG PHÁT SINH NHU CẦU MUA LÍNH ĐÁNH LẺ
                        if (!is_rushing_hq) {
                            pending_train_requests += (it->required_attackers - current_squad_size);
                        }
                    }
                } 
                // Cắt giảm nếu thừa lính
                else if (current_squad_size > it->required_attackers) {
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
            ++it;
        }

        // ============================================
        // 2. KHỞI TẠO CÁC NHIỆM VỤ MỚI
        // ============================================
        int my_net_income = get_my_net_income(S, M, my_warriors.size());
        int opp_net_income = get_enemy_net_income(S, M, enemy_warriors.size());
        if (!can_attack_struct || !can_attack_econ ) return; 

        std::vector<int> potential_targets;
        if (!locked_targets.count(M.opp_hq)) potential_targets.push_back(M.opp_hq);
        for (const auto& b : S.buildings) {
            if (b.side != M.my_side && b.type == BType::BASE && !locked_targets.count(b.region)) {
                potential_targets.push_back(b.region); 
            }
        }
        
        struct TargetOption { int tgt; int req; double score; int rp; };
        std::vector<TargetOption> options;
        std::vector<Warrior> atk_units = get_attack_free_units(); 
        
        for (int tgt : potential_targets) {
            if (gathering_targets.count(tgt)) continue;
            int rp = get_rally_point(tgt);
            int dist_rp_tgt = get_hops(P, rp, tgt);
            
            int t_first_arrival = 999;
            for (auto& w : atk_units) {
                int d = get_hops(P, w.region, rp);
                if (d < t_first_arrival) t_first_arrival = d;
            }
            if (t_first_arrival == 999) t_first_arrival = 0;
            
            int enemy_reaction_window = 1;
            if(strong_defense_turns_left >0) enemy_reaction_window = dist_rp_tgt;
            int current_req = calculate_req_attackers(tgt, enemy_reaction_window, S, M, P);
            
            int train_cap = hq_b ? hq_b->train_cap() : 1;
            int deficit = std::max(0, current_req - (int)atk_units.size());
            int t_gather = (deficit > 0) ? (deficit / train_cap) : 0;
            
            enemy_reaction_window = 1;
            if(strong_defense_turns_left >0) enemy_reaction_window = std::max(0, t_gather - t_first_arrival - 3) + dist_rp_tgt - 1;
                else enemy_reaction_window = 1;
            current_req = calculate_req_attackers(tgt, enemy_reaction_window, S, M, P); 
            
            double score = (current_req * TUNE_SCORE_REQ_WEIGHT) + (dist_rp_tgt * TUNE_SCORE_DIST_WEIGHT);
            
            if (tgt == M.center_region) score -= 999999.0; 
            else if (tgt == M.opp_hq) score -= TUNE_HQ_TARGET_BONUS;
            else {
                score -= TUNE_BASE_TARGET_BONUS;
                if (current_req <= 10) score -= TUNE_WEAK_BASE_BONUS; 
            }
            
            int dist_to_my_hq = get_hops(P, tgt, M.my_hq);
            int dist_to_opp_hq = get_hops(P, tgt, M.opp_hq);
            if (tgt != M.opp_hq && dist_to_my_hq < dist_to_opp_hq) score -= TUNE_ENEMY_IN_MY_HALF_BONUS; 
            
            options.push_back({tgt, current_req, score, rp});
        }
        
        std::sort(options.begin(), options.end(), [](const TargetOption& a, const TargetOption& b) {
            return a.score < b.score;
        });
        
        int my_est_inc = get_my_net_income(S, M, my_warriors.size());
        int max_potential_army = get_free_units(S).size() + (virtual_gold + std::max(0, my_est_inc) * 15) / TRAIN_COST;
        for (const auto& m : active_missions) max_potential_army += m.squad_ids.size();
        
        for (const auto& opt : options) {
            if (active_missions.size() >= TUNE_MAX_CONCURRENT_ATTACKS) break;
            
            int max_allowed_req = std::max(50, (int)(max_potential_army * TUNE_MAX_ARMY_RATIO));
            if (opt.req > max_allowed_req) continue; 
            
            int available = atk_units.size();
            if (is_rushing_hq && available < opt.req) continue;
            
            int train_shortage = std::max(0, opt.req - available);
            
            AttackMission m;
            m.target = opt.tgt; m.rally_point = opt.rp; m.required_attackers = opt.req;
            m.is_launched = false; m.created_turn = turn;
            
            std::sort(atk_units.begin(), atk_units.end(), [&](const Warrior& a, const Warrior& b) {
                return get_hops(P, a.region, opt.rp) < get_hops(P, b.region, opt.rp);
            });
            
            int drafted = 0;
            for (auto it = atk_units.begin(); it != atk_units.end(); ) {
                if (drafted >= m.required_attackers) break;
                if (!is_safe_to_dispatch(S, *it, opt.rp, current_free_count, M, P)) { ++it; continue; }
                
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

            int transfer = std::min(virtual_gold, ready_at_rally * MOVE_COST);
            m.reserved_gold += transfer;
            virtual_gold -= transfer;

            bool can_launch = false;
            if (ready_at_rally >= m.required_attackers && m.required_attackers > 0 && m.reserved_gold >= m.required_attackers * MOVE_COST) {
                if (opt.tgt == M.opp_hq) {
                    can_launch = simulate_base_race(S, M, P, m.required_attackers, m.rally_point);
                } else {
                    can_launch = true;
                    std::vector<int> bases_to_check = {M.my_hq};
                    for (auto& b : my_bases) bases_to_check.push_back(b.region);
                    for (int reg : bases_to_check) {
                        if (!is_safe_against_all_in(S, M, P, reg, 0, false, 0)) {
                            can_launch = false; break;
                        }
                    }
                }
            }

            if (can_launch) {
                m.is_launched = true;
                locked_targets.insert(opt.tgt); 
            } else {
                // KHI ĐANG RUSH HQ -> KHÔNG PHÁT SINH YÊU CẦU MUA LÍNH
                if (!is_rushing_hq) {
                    pending_train_requests += train_shortage;
                }
                gathering_targets.insert(opt.tgt); 
            }

            active_missions.push_back(m);
        }
    }
    
    void plan_forward_expansion(const GameState &S, const GameMap &M, const Paths &P) {
        if (is_rushing_hq) return; 

        int current_builds = build_plans.size() + forward_build_plans.size();
        if (current_builds >= TUNE_MAX_CONCURRENT_BUILDS) return; 
        
        // [CẬP NHẬT] Tính tổng labor đang có và sẽ có bên sân đối phương
        int current_forward_labor = 0;
        for (const auto& b : my_bases) {
            if (get_hops(P, b.region, M.my_hq) > get_hops(P, b.region, M.opp_hq)) {
                current_forward_labor += b.work_cap();
            }
        }
        for (auto const& [wid, plan] : build_plans) {
            if (get_hops(P, plan.first, M.my_hq) > get_hops(P, plan.first, M.opp_hq)) current_forward_labor += BASE_LEVELS[1].work_cap;
        }
        for (auto const& [wid, plan] : forward_build_plans) {
            if (get_hops(P, plan.first, M.my_hq) > get_hops(P, plan.first, M.opp_hq)) current_forward_labor += BASE_LEVELS[1].work_cap;
        }

        // Chặn nếu đã đạt giới hạn labor bên sân địch
        if (current_forward_labor >= TUNE_MAX_FORWARD_LABOR) return;

        std::vector<Warrior> free_units = get_free_units(S);
        
        struct FwdCand { int region; int dist_opp; WarriorId wid; };
        std::vector<FwdCand> cands;
        
        // [CẬP NHẬT] Đã xóa vòng lặp if (!has_money_advantage)
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
            if (current_forward_labor >= TUNE_MAX_FORWARD_LABOR) break; // Giới hạn labor
            
            int base_budget = get_spendable_gold();
            if (base_budget >= BASE_LEVELS[1].cost) {
                
                if (!is_safe_against_all_in(S, M, P, cand.region, BASE_LEVELS[1].cost, true, 0)) {
                    continue; 
                }

                forward_build_plans[cand.wid] = {cand.region, BASE_LEVELS[1].cost};
                current_builds++;
                current_forward_labor += BASE_LEVELS[1].work_cap; // Cập nhật labor để check cho lặp sau
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

            if (w_copy.region == target_region && w_copy.state == WState::STATIONARY) {
                bool already_upgrading = std::find(a.upgrades.begin(), a.upgrades.end(), target_region) != a.upgrades.end();
                
                if (!already_upgrading && virtual_gold >= actual_cost + total_upkeep) {
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
        int estimated_income = (persistent_jobs.size() * WORK_INCOME) - (my_warriors.size() * UPKEEP_PER_WARRIOR);

        // ==========================================
        // GOM NHÓM LÍNH RẢNH THEO KHU VỰC ĐỂ TỐI ƯU
        // ==========================================
        std::map<int, std::vector<Warrior>> free_by_region;
        for (const auto& w : free_units) {
            free_by_region[w.region].push_back(w);
        }

        // Tạo danh sách các Hub (Base & HQ của mình)
        std::vector<int> my_hubs;
        my_hubs.push_back(M.my_hq);
        for (const auto& b : my_bases) my_hubs.push_back(b.region);

        // ==========================================
        // PHA 0: XỬ LÝ UPGRADE (HQ & Căn cứ hiện tại)
        // ==========================================
        struct UpgradeCandidate {
            int region; int cost; bool is_hq; int priority;
            bool operator<(const UpgradeCandidate& o) const {
                if (priority != o.priority) return priority < o.priority;
                return cost < o.cost;
            }
        };
        std::vector<UpgradeCandidate> up_cands;

        int lv1_count = 0, lv2_count = 0;
        for (const auto& b : my_bases) {
            if (b.level == 1) lv1_count++;
            if (b.level == 2) lv2_count++;
        }
        int max_to_lv2 = lv1_count / 2 + 1;
        int max_to_lv3 = lv2_count / 2 + 1;

        int planned_to_lv2 = 0, planned_to_lv3 = 0;
        auto count_planned = [&](int region) {
            for (const auto& b : my_bases) {
                if (b.region == region) {
                    if (b.level == 1) planned_to_lv2++;
                    if (b.level == 2) planned_to_lv3++;
                }
            }
        };
        for (const auto& plan : build_plans) count_planned(plan.second.first);
        for (const auto& plan : forward_build_plans) count_planned(plan.second.first);

        // Upgrade HQ
        if (hq_b && hq_b->level < custom_hq_max_level && !hq_fast_tracked) {
            bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == hq_b->region) enemy_present = true;
            bool already_planned = false; 
            for(auto const& [wid, plan] : build_plans) if (plan.first == hq_b->region) already_planned = true;
            for(auto const& [wid, plan] : forward_build_plans) if (plan.first == hq_b->region) already_planned = true;
            
            if (!already_planned && !enemy_present) {
                up_cands.push_back({hq_b->region, HQ_LEVELS[hq_b->level + 1].upgrade_cost, true, 1});
            }
        }

        // Upgrade Base
        if (!has_money_advantage) {
            for (const auto& b : my_bases) {
                bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == b.region) enemy_present = true;
                bool already_planned = false; 
                for(auto const& [wid, plan] : build_plans) if (plan.first == b.region) already_planned = true;
                for(auto const& [wid, plan] : forward_build_plans) if (plan.first == b.region) already_planned = true;
                
                if (!already_planned && !enemy_present) {
                    if (b.level == 1 && planned_to_lv2 < max_to_lv2) {
                        up_cands.push_back({b.region, BASE_LEVELS[2].cost, false, (b.region == M.center_region) ? 2 : 3});
                        planned_to_lv2++; 
                    } else if (b.level == 2 && planned_to_lv3 < max_to_lv3) {
                        up_cands.push_back({b.region, BASE_LEVELS[3].cost, false, (b.region == M.center_region) ? 2 : 3});
                        planned_to_lv3++;
                    }
                }
            }
        }
        
        std::sort(up_cands.begin(), up_cands.end());

        // Phân bổ ngân sách cho Upgrade
        for (const auto& cand : up_cands) {
            int base_budget = get_spendable_gold();
            if (current_free_count == 0) {
                int D = get_hops(P, M.my_hq, cand.region);
                int projected_gold = base_budget + (D * estimated_income);
                int req_gold = cand.cost + TRAIN_COST + MOVE_COST;
                if (projected_gold >= req_gold) pending_train_requests++;
                break; 
            }
            
            int best_hub = -1;
            int min_d = 999;

            for (int hub : my_hubs) {
                if (free_by_region[hub].empty()) continue;
                int d = get_hops(P, hub, cand.region);
                if (d < min_d) { min_d = d; best_hub = hub; }
            }

            if (best_hub != -1) {
                int D = min_d; 
                if (!is_safe_against_all_in(S, M, P, cand.region, cand.cost, false, D)) continue; 

                int projected_real_gold = base_budget + (D * estimated_income);
                int required_gold = cand.cost + (D * MOVE_COST);
                
                if (projected_real_gold >= required_gold) {
                    int r_idx = std::rand() % free_by_region[best_hub].size();
                    Warrior chosen_w = free_by_region[best_hub][r_idx];

                    if (is_safe_to_dispatch(S, chosen_w, cand.region, current_free_count, M, P)) {
                        build_plans[chosen_w.id] = {cand.region, cand.cost};
                        current_free_count--;
                        current_builds++; 
                        
                        free_by_region[best_hub].erase(free_by_region[best_hub].begin() + r_idx);
                        if (current_builds >= TUNE_MAX_CONCURRENT_BUILDS) return; 
                    }
                }
            }
        }

        // ==========================================
        // LỌC DANH SÁCH STRONGHOLD TRỐNG
        // ==========================================
        // ==========================================
        // LỌC DANH SÁCH STRONGHOLD TRỐNG VÀ KIỂM SOÁT LÍNH GIỮ CHỖ
        // ==========================================
        std::vector<int> available_sh;
        int forward_holders = 0; 
        
        // 1. Đếm số lính đang thực sự ĐỨNG SẴN trên Stronghold trống ở sân địch
        for (const auto& w : my_warriors) {
            bool in_enemy_half = get_hops(P, w.region, M.my_hq) > get_hops(P, w.region, M.opp_hq);
            bool is_sh = std::find(M.strongholds.begin(), M.strongholds.end(), w.region) != M.strongholds.end();
            
            if (in_enemy_half && is_sh) {
                bool has_b = false;
                for (const auto& b : S.buildings) if (b.region == w.region) has_b = true;
                if (!has_b) forward_holders++; // Cộng 1 lính đang giữ chỗ
            }
        }

        for (int sh : M.strongholds) {
            bool has_b = false; 
            for (const auto& bld : S.buildings) if (bld.region == sh) has_b = true;
            
            bool already_planned = false; 
            for(auto const& [wid, plan] : build_plans) {
                if (plan.first == sh) {
                    already_planned = true;
                    // 2. Đếm thêm những lính ĐANG CHẠY sang sân địch để giữ chỗ/xây nhà
                    bool is_my_half = get_hops(P, sh, M.my_hq) <= get_hops(P, sh, M.opp_hq);
                    if (!is_my_half) {
                        // Tránh đếm đúp nếu lính đó đã tới nơi và được đếm ở bước 1
                        bool already_at_destination = false;
                        for (const auto& w : my_warriors) if (w.id == wid && w.region == sh) already_at_destination = true;
                        if (!already_at_destination) forward_holders++;
                    }
                }
            }
            for(auto const& [wid, plan] : forward_build_plans) if (plan.first == sh) already_planned = true;
            
            if (!has_b && !already_planned) {
                available_sh.push_back(sh);
            }
        }

        // ==========================================
        // PHA 1: MỞ RỘNG (CHỈ DUYỆT CÁC HUB CÓ LÍNH)
        // ==========================================
        
        // 1. Chống lỗi lính ma: Xử lý trước lính ĐÃ ĐỨNG SẴN trên Stronghold trống
        for (auto it = free_by_region.begin(); it != free_by_region.end(); ++it) {
            int reg = it->first;
            auto sh_it = std::find(available_sh.begin(), available_sh.end(), reg);
            if (sh_it != available_sh.end()) {
                while (!it->second.empty() && current_builds < TUNE_MAX_CONCURRENT_BUILDS) {
                    int base_budget = get_spendable_gold();
                    
                    if (base_budget >= 300 && is_safe_against_all_in(S, M, P, reg, 300, true, 0)) {
                        int r_idx = std::rand() % it->second.size();
                        Warrior chosen_w = it->second[r_idx];

                        build_plans[chosen_w.id] = {reg, 300};
                        current_free_count--;
                        current_builds++;
                        
                        bool is_my_half = get_hops(P, reg, M.my_hq) <= get_hops(P, reg, M.opp_hq);
                        if (!is_my_half) forward_holders++;

                        it->second.erase(it->second.begin() + r_idx);
                        available_sh.erase(sh_it);
                        break; 
                    } else {
                        break; 
                    }
                }
            }
        }

        // [CẬP NHẬT] ƯU TIÊN XÂY CENTER BASE TỪ BASE THỨ 2 TRỞ ĐI
        int total_planned_bases = my_bases.size() + build_plans.size() + forward_build_plans.size();
        if (total_planned_bases >= 1 && current_builds < TUNE_MAX_CONCURRENT_BUILDS && !has_money_advantage) {
            auto center_it = std::find(available_sh.begin(), available_sh.end(), M.center_region);
            if (center_it != available_sh.end()) {
                int base_budget = get_spendable_gold();
                int best_hub = -1;
                int min_dist_to_center = 999;
                
                // Tìm Hub có lính rảnh và gần Center nhất
                for (int hub : my_hubs) {
                    if (free_by_region[hub].empty()) continue;
                    int d = get_hops(P, hub, M.center_region);
                    if (d < min_dist_to_center) {
                        if (is_safe_against_all_in(S, M, P, M.center_region, 300, true, d)) {
                            min_dist_to_center = d;
                            best_hub = hub;
                        }
                    }
                }

                if (best_hub != -1) {
                    int D = min_dist_to_center;
                    int projected_real_gold = base_budget + (D * estimated_income);
                    int required_gold = 300 + (D * MOVE_COST);

                    if (projected_real_gold >= required_gold) {
                        int r_idx = std::rand() % free_by_region[best_hub].size();
                        Warrior chosen_w = free_by_region[best_hub][r_idx];

                        if (is_safe_to_dispatch(S, chosen_w, M.center_region, current_free_count, M, P)) {
                            build_plans[chosen_w.id] = {M.center_region, 300};
                            current_free_count--;
                            current_builds++;
                            
                            bool is_my_half = get_hops(P, M.center_region, M.my_hq) <= get_hops(P, M.center_region, M.opp_hq);
                            if (!is_my_half) forward_holders++;

                            free_by_region[best_hub].erase(free_by_region[best_hub].begin() + r_idx);
                            available_sh.erase(center_it);
                        }
                    }
                }
            }
        }

        // 2. Chuyển qua từng Base/HQ có lính rảnh (Theo thuật toán tham lam như cũ)
        if (!has_money_advantage) {
            for (int hub : my_hubs) {
                while (!free_by_region[hub].empty() && current_builds < TUNE_MAX_CONCURRENT_BUILDS) {
                    int base_budget = get_spendable_gold();
                    int best_sh = -1;
                    int min_dist_to_hub = 999;

                    for (int sh : available_sh) {
                        bool is_my_half = get_hops(P, sh, M.my_hq) <= get_hops(P, sh, M.opp_hq);

                        if (!is_my_half && forward_holders >= TUNE_MAX_FORWARD_HOLDERS) continue;

                        if (sh == M.center_region || is_my_half || is_stronghold_safe(S, M, P, sh)) {
                            int d = get_hops(P, hub, sh);
                            if (d < min_dist_to_hub) {
                                if (is_safe_against_all_in(S, M, P, sh, 300, true, d)) {
                                    min_dist_to_hub = d;
                                    best_sh = sh;
                                }
                            }
                        }
                    }

                    bool assigned = false;
                    if (best_sh != -1) {
                        int D = min_dist_to_hub;
                        int projected_real_gold = base_budget + (D * estimated_income);
                        int required_gold = 300 + (D * MOVE_COST);

                        if (projected_real_gold >= required_gold) {
                            int r_idx = std::rand() % free_by_region[hub].size();
                            Warrior chosen_w = free_by_region[hub][r_idx];

                            if (is_safe_to_dispatch(S, chosen_w, best_sh, current_free_count, M, P)) {
                                build_plans[chosen_w.id] = {best_sh, 300};
                                current_free_count--;
                                current_builds++;
                                
                                bool is_my_half = get_hops(P, best_sh, M.my_hq) <= get_hops(P, best_sh, M.opp_hq);
                                if (!is_my_half) forward_holders++;

                                free_by_region[hub].erase(free_by_region[hub].begin() + r_idx);
                                available_sh.erase(std::remove(available_sh.begin(), available_sh.end(), best_sh), available_sh.end());
                                assigned = true;
                            }
                        }
                    }
                    
                    if (!assigned) break; 
                }
            }
        }

        // ==========================================
        // PHA 2: SINH LÍNH (TÌM BÃI GẦN HQ NHẤT)
        // ==========================================
        if (current_builds < TUNE_MAX_CONCURRENT_BUILDS && !has_money_advantage) {
            int base_budget = get_spendable_gold();

            std::vector<int> hq_cands = available_sh;
            std::sort(hq_cands.begin(), hq_cands.end(), [&](int a, int b) {
                return get_hops(P, M.my_hq, a) < get_hops(P, M.my_hq, b);
            });

            for (int sh : hq_cands) {
                if (current_builds >= TUNE_MAX_CONCURRENT_BUILDS) break;

                bool is_my_half = get_hops(P, sh, M.my_hq) <= get_hops(P, sh, M.opp_hq);
                if (!is_my_half && forward_holders >= TUNE_MAX_FORWARD_HOLDERS) continue;

                if (sh == M.center_region || is_my_half || is_stronghold_safe(S, M, P, sh)) {
                    int D = get_hops(P, M.my_hq, sh);
                    int projected_gold = base_budget + (D * estimated_income);
                    int req_gold = 300 + TRAIN_COST + (D * MOVE_COST);

                    if (projected_gold >= req_gold) {
                        pending_train_requests++;
                        current_builds++; 
                        if (!is_my_half) forward_holders++;
                    }
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
                bool already_upgrading = std::find(a.upgrades.begin(), a.upgrades.end(), target_region) != a.upgrades.end();
                
                if (!already_upgrading && virtual_gold >= actual_cost + total_upkeep) {
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
        int spendable_gold = get_spendable_gold();
        
        // NẾU VÀO TRẠNG THÁI ABSOLUTE ALL-IN -> XẢ MAX TIỀN MUA LÍNH LIÊN TỤC
        if (is_absolute_all_in) {
            if (virtual_gold >= TRAIN_COST) {
                int act = std::min(cap, virtual_gold / TRAIN_COST);
                if (act > 0) { a.train_n = act; virtual_gold -= act * TRAIN_COST; has_trained = true; }
            }
            return;
        }

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
            
            // ĐIỀU LỆNH TỐI CAO: NẾU ALL-IN, TOÀN BỘ LÍNH CHƯA LÀM THỢ (TRỐNG JOB) SẼ LÀM CẢM TỬ QUÂN
            if (is_absolute_all_in && !persistent_jobs.count(w.id)) {
                final_target = M.opp_hq;
            }
            else if (predictive_defenders.count(w.id) || emergency_targets.count(w.id)) {
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
                    if (target_arrival_turn != -1 && time_left > my_dist && my_dist != 999) continue; 

                    int cost = 0;
                    bool final_is_my_base = false;
                    for (const auto& b : S.buildings) if (b.region == final_target && b.side == M.my_side) final_is_my_base = true;
                    if (!final_is_my_base) cost = MOVE_COST;

                    if (cost > 0) {
                        for (auto& m : active_missions) {
                            if (m.squad_ids.count(w.id)) {
                                if (m.reserved_gold >= cost) {
                                    m.reserved_gold -= cost;
                                    a.moves.push_back({w.id, final_target});
                                }
                                break; 
                            }
                        }
                    } else {
                        a.moves.push_back({w.id, final_target});
                    }
                } else {
                    int cost = 0;
                    bool final_is_my_base = false;
                    for (const auto& b : S.buildings) if (b.region == final_target && b.side == M.my_side) final_is_my_base = true;
                    if (!final_is_my_base) cost = MOVE_COST;

                    // Absolute All-In có quỹ đặc quyền (Tiêu sạch tiền hiện có để đẩy quân nhanh nhất)
                    if((is_emergency && virtual_gold >= cost) || (!is_emergency && virtual_gold >= cost + total_upkeep) || (is_absolute_all_in && virtual_gold >= cost)) { 
                        a.moves.push_back({w.id, final_target}); 
                        virtual_gold -= cost; 
                    }
                }
            }
        }
    }

    Actions decide(const GameState &S, const GameMap &M, const Paths &P, int turn) {
        Actions a;
        if (strong_defense_turns_left > 0) strong_defense_turns_left--;
        update_state_and_clean_dead(S, M, P); 
        
        int my_net_income = get_my_net_income(S, M, my_warriors.size());
        int opp_net_income = get_enemy_net_income(S, M, enemy_warriors.size());
        has_money_advantage = (my_net_income >= opp_net_income + TUNE_GOLD_ADVANTAGE);
        
        // [Ý 2] Tinh chỉnh điều kiện is_rushing_hq
        is_rushing_hq = false;
        if (opp_hq_b && hq_b && has_money_advantage) {
            is_rushing_hq = check_hq_race_to_end(S, M, turn);
        }

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

        // [Ý 4] Kiểm tra chốt hạ
        is_absolute_all_in = check_absolute_all_in(S, M, P, turn);

        if (is_absolute_all_in) {
            // Hủy toàn bộ mission, thủ, mở rộng để tiết kiệm tính toán và tiền bạc. Thợ mỏ tự giữ vị trí sinh tiền.
            active_missions.clear();
            predictive_defenders.clear();
            emergency_targets.clear();
            build_plans.clear();
            forward_build_plans.clear();
        } else {
            plan_forward_expansion(S, M, P);
            process_forward_build_plans(S, M, P, a);
            plan_attacks(S, M, P, turn);
            detect_and_handle_emergencies(S, M, P); 
            plan_expansion(S, M, P, hq_fast_tracked);
            process_build_plans(S, M, P, a);
            assign_jobs_to_free_units(S, M, P);
        }
        
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