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
  int N = 0, K = 0;
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

// ==========================================
// TÁC NHÂN AI ĐIỀU KHIỂN CHIẾN THUẬT V6.0 (FIX BASE LIMIT, ACTIVE DEFENSE & SMART HQ ASSAULT)
// ==========================================
class BotAgent {
public:
    const int RESERVE_GOLD_ATTACK = 50; 
    
    // Hệ số đe dọa động
    const int SWARM_DANGER_HUGE = 8;
    const int SWARM_DANGER_MED = 4;

    // --- CẤU HÌNH XÂY DỰNG TÙY CHỈNH ---
    int max_bases_to_build = 0;
    int max_bases_to_upgrade = 0;
    int custom_hq_max_level = 3;    
    int custom_base_max_level = 2;  
    bool done_building = false;     

    // --- TRẠNG THÁI ---
    std::map<WarriorId, int> emergency_targets; 
    std::set<WarriorId> attack_squad_ids;
    
    int current_attack_target = -1;
    int current_rally_point = -1; 
    int required_attackers = 0;
    int attack_phase = 0; 
    
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

    // --- CÁC HÀM TIỆN ÍCH ---
    int get_hops(const Paths& P, int u, int v) const {
        if (u == v) return 0;
        int hops = 0; int curr = u;
        while (curr != v) { curr = P.nxt[curr][v]; if (curr == -1) return 999; hops++; }
        return hops;
    }

    int get_b_hp(const GameState& S, int region) const {
        for (const auto& b : S.buildings) if (b.region == region) return b.hp;
        return 0;
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

    int get_locked_gold() const {
        int locked = 0;
        for (const auto& p : build_plans) locked += p.second.second;
        return locked;
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
    
    std::vector<Warrior> get_free_units(const GameState& S) {
        std::vector<Warrior> free_units;
        for (const auto& w : my_warriors) {
            if (!emergency_targets.count(w.id) && !attack_squad_ids.count(w.id) &&
                !build_plans.count(w.id) && !persistent_jobs.count(w.id)) {
                free_units.push_back(w);
            }
        }
        return free_units;
    }

    // MỚI: Hàm kiểm tra xem đường đi có đi xuyên qua base địch hay không
    bool path_has_enemy_base(int start, int target, const Paths& P, const GameState& S, const GameMap& M) const {
        int curr = start;
        while (curr != target && curr != -1) {
            curr = P.nxt[curr][target];
            if (curr == -1) break;
            if (curr != target) {
                for (const auto& b : S.buildings) {
                    if (b.region == curr && b.side != M.my_side && b.type == BType::BASE) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // ==========================================
    // MODULE LOGIC
    // ==========================================

    void update_state_and_clean_dead(const GameState &S, const GameMap &M, const Paths &P) {
        my_warriors.clear(); enemy_warriors.clear(); my_bases.clear();
        hq_b = nullptr; opp_hq_b = nullptr;
        
        for (const auto& w : S.warriors) { 
            if (w.id.side == M.my_side) my_warriors.push_back(w); 
            else enemy_warriors.push_back(w); 
        }
        for (const auto& b : S.buildings) {
            if (b.side == M.my_side) { 
                if (b.type == BType::BASE) my_bases.push_back(b); 
                else if (b.type == BType::HQ) hq_b = &b; 
            } else if (b.side != M.my_side && b.type == BType::HQ) opp_hq_b = &b;
        }

        total_upkeep = my_warriors.size() * UPKEEP_PER_WARRIOR;
        virtual_gold = S.gold; 
        has_trained = false;
        emergency_train_queue = 0;
        pending_train_requests = 0;

        std::set<WarriorId> alive_ids;
        for (auto& w : my_warriors) alive_ids.insert(w.id);

        for (auto it = attack_squad_ids.begin(); it != attack_squad_ids.end(); ) {
            if (!alive_ids.count(*it)) it = attack_squad_ids.erase(it); else ++it;
        }
        for (auto it = emergency_targets.begin(); it != emergency_targets.end(); ) {
            if (!alive_ids.count(it->first)) it = emergency_targets.erase(it); else ++it;
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

        // SỬA: GIỚI HẠN SỐ LƯỢNG BASE ĐỂ TRÁNH THIẾU TIỀN VÀ MẮC KẸT
        int total_sh = M.strongholds.size();
        max_bases_to_build =  (total_sh / 2) + 1; // Không xây quá 3 base
        max_bases_to_upgrade = max_bases_to_build/2+1; 

        // KIỂM TRA ĐÃ HẾT MỤC TIÊU NÂNG CẤP/XÂY MỚI CHƯA
        done_building = true;
        if (my_bases.size() < max_bases_to_build) done_building = false;
        if (hq_b && hq_b->level < custom_hq_max_level) done_building = false;
        
        std::vector<Building> sorted_bases = my_bases;
        std::sort(sorted_bases.begin(), sorted_bases.end(), [&](const Building& a, const Building& b) {
            return get_hops(P, a.region, M.my_hq) < get_hops(P, b.region, M.my_hq);
        });
        
        int upgrade_limit = std::min((int)sorted_bases.size(), max_bases_to_upgrade);
        for (int i = 0; i < upgrade_limit; ++i) {
            if (sorted_bases[i].level < custom_base_max_level) done_building = false;
        }
    }

    void detect_and_handle_emergencies(const GameState &S, const GameMap &M, const Paths &P) {
        emergency_targets.clear();
        bool emergency_triggered = false;

        struct Swarm { int center; int size; };
        std::vector<Swarm> swarms;
        for (const auto& ew : enemy_warriors) {
            bool added = false;
            for (auto& swarm : swarms) {
                if (get_hops(P, ew.region, swarm.center) <= 2) {
                    swarm.size++; added = true; break; 
                }
            }
            if (!added) swarms.push_back({ew.region, 1});
        }

        std::vector<Swarm> active_threats;
        for (const auto& swarm : swarms) {
            int dist_to_my_hq = get_hops(P, swarm.center, M.my_hq);
            int dist_to_opp_hq = get_hops(P, swarm.center, M.opp_hq);
            
            // SỬA: Tìm khoảng cách từ địch đến bất kỳ công trình nào của mình
            int min_dist_to_building = dist_to_my_hq;
            for (const auto& b : my_bases) {
                min_dist_to_building = std::min(min_dist_to_building, get_hops(P, swarm.center, b.region));
            }

            bool is_threat = false;
            if (min_dist_to_building <= 3) is_threat = true; // SỬA: Bắt buộc phòng thủ khi địch vào vùng nguy hiểm
            else if (swarm.size >= SWARM_DANGER_HUGE) is_threat = true; 
            else if (swarm.size >= SWARM_DANGER_MED && dist_to_my_hq <= dist_to_opp_hq + 2) is_threat = true;
            else if (dist_to_my_hq < dist_to_opp_hq) is_threat = true;

            if (is_threat) active_threats.push_back(swarm);
        }

        for (const auto& swarm : active_threats) {
            int target_region = M.my_hq;
            int eta = get_hops(P, swarm.center, M.my_hq);
            
            // Xác định chính xác địch đang lao vào mục tiêu nào
            for (const auto& b : my_bases) {
                int dist = get_hops(P, swarm.center, b.region);
                if (dist < eta) { eta = dist; target_region = b.region; }
            }

            int defenders_in_time = 0;
            for (const auto& w : my_warriors) if (get_hops(P, w.region, target_region) <= eta) defenders_in_time++;

            int req_defenders = std::max(1, swarm.size + 1); // Cần số lính lớn hơn để đảm bảo an toàn
            
            if (defenders_in_time < req_defenders) {
                emergency_triggered = true;
                
                int train_cap = hq_b ? hq_b->train_cap() : 1;
                // SỬA: Tính toán chính xác dù địch đã vào sát nhà (eta = 0)
                int max_trainable = std::max(1, eta) * train_cap; 
                int deficit = req_defenders - defenders_in_time;
                
                // SỬA: Luôn xếp toàn bộ lính bị thiếu vào queue (nếu dư vàng hệ thống tự sản xuất)
                emergency_train_queue += deficit; 

                int pull_from_field = std::max(0, deficit - max_trainable); 
                
                if (pull_from_field > 0) {
                    std::vector<Warrior> candidates = my_warriors;
                    std::sort(candidates.begin(), candidates.end(), [&](const Warrior& a, const Warrior& b) {
                        return get_hops(P, a.region, target_region) < get_hops(P, b.region, target_region);
                    });
                    
                    int pulled = 0;
                    for (auto& w : candidates) {
                        if (pulled >= pull_from_field) break;
                        if (!emergency_targets.count(w.id) && get_hops(P, w.region, target_region) > eta) {
                            emergency_targets[w.id] = target_region;
                            persistent_jobs.erase(w.id); attack_squad_ids.erase(w.id); build_plans.erase(w.id);
                            pulled++;
                        }
                    }
                }
            }
        }

        if (emergency_triggered) { 
            build_plans.clear(); attack_phase = 0; attack_squad_ids.clear(); 
        }
    }

    void plan_attacks(const GameState &S, const GameMap &M, const Paths &P) {
        if (emergency_targets.size() > 0) return; 

        if (attack_phase > 0) {
            bool target_alive = false;
            for (auto& b : S.buildings) if (b.region == current_attack_target && b.side != M.my_side) target_alive = true;
            if (!target_alive && current_attack_target != M.opp_hq) { attack_phase = 0; attack_squad_ids.clear(); return; }
            
            std::vector<Warrior> free_units = get_free_units(S);
            int current_squad_size = attack_squad_ids.size();
            
            if (current_squad_size < required_attackers) {
                for (auto& w : free_units) {
                    attack_squad_ids.insert(w.id);
                    current_squad_size++;
                    if (current_squad_size >= required_attackers) break;
                }
            }
            
            if (current_squad_size < required_attackers) {
                pending_train_requests += (required_attackers - current_squad_size);
            }

            if (attack_phase == 1) {
                int units_at_rally = 0; 
                for (auto& w : my_warriors) {
                    if (attack_squad_ids.count(w.id) && w.region == current_rally_point) {
                        units_at_rally++;
                    }
                }
                
                int threshold = std::max(1, (required_attackers * 4) / 5);
                if (units_at_rally >= threshold) {
                    attack_phase = 2; 
                }
            }
            return;
        }

        if (attack_phase == 0 && (done_building || virtual_gold > 1000)) {
            std::vector<Warrior> free_units = get_free_units(S);
            std::vector<int> targets = {M.opp_hq};
            for(auto& b: S.buildings) if (b.side != M.my_side && b.type == BType::BASE) targets.push_back(b.region);
            
            int best_target = -1; int best_rally = -1; double best_score = 1e9; int chosen_req = 0;
            
            for (int t : targets) {
                int rally_pt = M.my_hq; int min_my_dist = 999;
                for (const auto& b : my_bases) {
                    int d = get_hops(P, b.region, t);
                    if (d < min_my_dist) { min_my_dist = d; rally_pt = b.region; }
                }
                
                int local_enemy_hp = 0; 
                for (const auto& ew : enemy_warriors) {
                    if (get_hops(P, ew.region, t) <= 3) local_enemy_hp += ew.hp;
                }
                
                int req = local_enemy_hp + 3; 
                if (req < 3) req = 3;
                if (req > 30) req = 30; 
                
                double score = (min_my_dist * 2.0) + (local_enemy_hp * 2.0) + get_b_ad(S, t);
                
                // SỬA: Áp dụng chiến lược đánh thốc HQ không qua base
                if (t == M.opp_hq) {
                    if (!path_has_enemy_base(rally_pt, t, P, S, M)) {
                        score -= 1000000.0; // Ưu tiên cực độ nếu có đường đi không gặp base địch
                    } else {
                        score -= 10000.0;   // Vẫn ưu tiên cao nếu bắt buộc phải phá base dọc đường
                    }
                }
                
                if (score < best_score) { best_score = score; best_target = t; best_rally = rally_pt; chosen_req = req; }
            }
            
            if (best_target != -1) {
                int available = free_units.size();
                int train_shortage = std::max(0, chosen_req - available);
                
                int budget = (train_shortage * TRAIN_COST) + (done_building ? 0 : RESERVE_GOLD_ATTACK);
                int spendable_gold = virtual_gold - get_locked_gold();
                
                if (spendable_gold >= budget) {
                    attack_phase = 1; current_attack_target = best_target;
                    current_rally_point = best_rally; required_attackers = chosen_req;
                    attack_squad_ids.clear();
                    
                    int drafted = 0;
                    for (auto& w : free_units) {
                        if (drafted >= chosen_req) break;
                        attack_squad_ids.insert(w.id); drafted++;
                    }
                    pending_train_requests += train_shortage;
                }
            }
        }
    }

    void plan_expansion(const GameState &S, const GameMap &M, const Paths &P) {
        if (emergency_targets.size() > 0 || attack_phase != 0) return;

        std::vector<Warrior> free_units = get_free_units(S);

        struct PlanCandidate {
            int region; int cost; bool is_hq; bool is_new_base; bool is_my_half;
            int dist_to_my_hq;
            bool operator<(const PlanCandidate& o) const {
                if (is_new_base != o.is_new_base) return is_new_base > o.is_new_base; 
                if (is_my_half != o.is_my_half) return is_my_half > o.is_my_half;     
                if (cost != o.cost) return cost < o.cost;
                return dist_to_my_hq < o.dist_to_my_hq;
            }
        };

        std::vector<PlanCandidate> cands;
        
        if (my_bases.size() < max_bases_to_build) {
            for (int sh : M.strongholds) {
                bool has_b = false; for (const auto& bld : S.buildings) if (bld.region == sh) has_b = true;
                bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == sh) already_planned = true;
                bool is_my_half = get_hops(P, sh, M.my_hq) < get_hops(P, sh, M.opp_hq);
                
                if (!has_b && !already_planned && (is_my_half || is_stronghold_safe(S, M, P, sh))) {
                    PlanCandidate c; c.region = sh; c.cost = 300; c.is_hq = false; c.is_new_base = true; c.is_my_half = is_my_half;
                    c.dist_to_my_hq = get_hops(P, sh, M.my_hq); cands.push_back(c);
                }
            }
        }

        if (hq_b && hq_b->level < custom_hq_max_level) {
            bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == hq_b->region) enemy_present = true;
            bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == hq_b->region) already_planned = true;
            if (!already_planned && !enemy_present) {
                PlanCandidate c; c.region = hq_b->region; c.cost = HQ_LEVELS[hq_b->level + 1].upgrade_cost;
                c.is_hq = true; c.is_new_base = false; c.is_my_half = true; c.dist_to_my_hq = 0; cands.push_back(c);
            }
        }

        std::vector<Building> sorted_bases = my_bases;
        std::sort(sorted_bases.begin(), sorted_bases.end(), [&](const Building& a, const Building& b) {
            return get_hops(P, a.region, M.my_hq) < get_hops(P, b.region, M.my_hq);
        });

        int limit = std::min((int)sorted_bases.size(), max_bases_to_upgrade);
        for (int i = 0; i < limit; ++i) {
            const auto& b = sorted_bases[i];
            if (b.level < custom_base_max_level) {
                bool enemy_present = false; for (const auto& ew : enemy_warriors) if (ew.region == b.region) enemy_present = true;
                bool already_planned = false; for(auto const& [wid, plan] : build_plans) if (plan.first == b.region) already_planned = true;
                if (!already_planned && !enemy_present) {
                    PlanCandidate c; c.region = b.region; c.cost = BASE_LEVELS[b.level + 1].cost;
                    c.is_hq = false; c.is_new_base = false; c.is_my_half = true;
                    c.dist_to_my_hq = get_hops(P, b.region, M.my_hq); cands.push_back(c);
                }
            }
        }
        
        std::sort(cands.begin(), cands.end());
        
        int locked_so_far = get_locked_gold();
        int estimated_income = persistent_jobs.size() * WORK_INCOME - total_upkeep;

        for (const auto& cand : cands) {
            if (free_units.empty()) {
                int D = cand.dist_to_my_hq;
                int projected_gold = S.gold - locked_so_far + (D * estimated_income);
                int req_gold = cand.cost + TRAIN_COST + (D * MOVE_COST);
                
                if (projected_gold >= req_gold) pending_train_requests++;
                break; 
            }
            
            int best_u = -1; int min_d = 999;
            for (size_t i = 0; i < free_units.size(); i++) {
                int d = get_hops(P, free_units[i].region, cand.region);
                if (d < min_d) { min_d = d; best_u = i; }
            }

            int D = min_d;
            
            int projected_real_gold = S.gold - locked_so_far + (D * estimated_income);
            int required_gold = cand.cost + (D * MOVE_COST);
            
            if (projected_real_gold >= required_gold) {
                build_plans[free_units[best_u].id] = {cand.region, cand.cost};
                locked_so_far += cand.cost; 
                free_units.erase(free_units.begin() + best_u);
            } else {
                break; 
            }
        }
    }

    void process_build_plans(const GameState &S, const GameMap &M, const Paths &P, Actions &a) {
        for (auto it = build_plans.begin(); it != build_plans.end(); ) {
            Warrior w_copy; bool found_w = false;
            for (const auto& w : my_warriors) if (w.id == it->first) { w_copy = w; found_w = true; break; }
            if (!found_w) { it = build_plans.erase(it); continue; }

            int target_region = it->second.first;
            int locked_cost = it->second.second;

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
                } else {
                    ++it;
                }
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
        int spendable_gold = virtual_gold - get_locked_gold();
        
        if (!has_trained && emergency_train_queue > 0 && virtual_gold >= TRAIN_COST) {
            int act = std::min({emergency_train_queue, cap, virtual_gold / TRAIN_COST});
            if(act > 0) { a.train_n = act; virtual_gold -= act * TRAIN_COST; has_trained = true; }
        }
        else if (!has_trained && spendable_gold >= TRAIN_COST + total_upkeep) {
            int act = std::min({pending_train_requests, cap, (spendable_gold - total_upkeep) / TRAIN_COST});
            if(act > 0) { a.train_n = act; virtual_gold -= act * TRAIN_COST; has_trained = true; }
        }
    }

    void execute_movement(const GameState &S, const GameMap &M, const Paths &P, Actions &a) {
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
            
            if (emergency_targets.count(w.id)) {
                final_target = emergency_targets[w.id];
                is_emergency = true;
            } else if (attack_phase > 0 && attack_squad_ids.count(w.id)) {
                if (attack_phase == 1) final_target = current_rally_point;      
                else if (attack_phase == 2) final_target = current_attack_target; 
            } else if (build_plans.count(w.id)) {
                final_target = build_plans[w.id].first;
            } else if (persistent_jobs.count(w.id)) {
                final_target = persistent_jobs[w.id]; 
            } else {
                bool on_base = false;
                for (const auto& b : my_bases) if (b.region == w.region) on_base = true;
                if (hq_b && hq_b->region == w.region) on_base = true;
                
                if (on_base) final_target = w.region; 
                else {
                    int nearest_base = M.my_hq; int min_d = get_hops(P, w.region, M.my_hq);
                    for (const auto& b : my_bases) {
                        int d = get_hops(P, w.region, b.region);
                        if (d < min_d) { min_d = d; nearest_base = b.region; }
                    }
                    final_target = nearest_base;
                }
            }
            
            if(w.region != final_target) {
                int actual_step_target = final_target;
                
                if (!is_emergency) {
                    bool final_is_base = false;
                    for (const auto& b : S.buildings) if (b.side == M.my_side && b.region == final_target) final_is_base = true;
                    if (!final_is_base) {
                        int nxt = P.nxt[w.region][final_target];
                        bool nxt_is_base = false;
                        for (const auto& b : S.buildings) if (b.side == M.my_side && b.region == nxt) nxt_is_base = true;
                        if (nxt_is_base) actual_step_target = nxt;
                    }
                } else {
                    actual_step_target = P.nxt[w.region][final_target];
                }
                
                int cost = 0;
                bool moving_to_my_base = false;
                for (const auto& b : S.buildings) if (b.region == actual_step_target && b.side == M.my_side) moving_to_my_base = true;
                if (!moving_to_my_base) cost = MOVE_COST;

                if((is_emergency && virtual_gold >= cost) || (!is_emergency && virtual_gold >= cost + total_upkeep)) { 
                    a.moves.push_back({w.id, actual_step_target});
                    virtual_gold -= cost; 
                }
            }
        }
    }

    Actions decide(const GameState &S, const GameMap &M, const Paths &P, int turn) {
        Actions a;
        update_state_and_clean_dead(S, M, P); 
        
        detect_and_handle_emergencies(S, M, P);
        plan_attacks(S, M, P);
        plan_expansion(S, M, P);
        process_build_plans(S, M, P, a);
        assign_jobs_to_free_units(S, M, P);
        execute_training(S, M, P, a);
        execute_movement(S, M, P, a);
        
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