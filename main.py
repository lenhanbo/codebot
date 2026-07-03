#!/usr/bin/env python3
from __future__ import annotations

import math
import sys
import random
from enum import Enum
from typing import NamedTuple, List, Tuple, Dict, Optional, Set

# --- CONSTANTS ---
MAX_TURN = 200
START_GOLD = 500
START_WARRIORS = 3
MOVE_COST = 10
TRAIN_COST = 120
WORK_INCOME = 15
UPKEEP_PER_WARRIOR = 2
HQ_MAX_LEVEL = 5
BASE_MAX_LEVEL = 3
HQ_HEAL_COST = 1000
BASE_HEAL_COST = 500

class HqLevelEntry(NamedTuple):
    upgrade_cost: int
    warrior_hp: int
    hp: int
    turret: int
    train_cap: int
    work_cap: int

class BaseLevelEntry(NamedTuple):
    cost: int
    hp: int
    turret: int
    work_cap: int

HQ_LEVELS: tuple[HqLevelEntry, ...] = (
    HqLevelEntry(0, 0, 0, 0, 0, 0),
    HqLevelEntry(0, 4, 10, 1, 1, 1),
    HqLevelEntry(600, 5, 15, 2, 1, 2),
    HqLevelEntry(1200, 6, 20, 2, 2, 3),
    HqLevelEntry(2400, 7, 25, 3, 2, 4),
    HqLevelEntry(3600, 8, 30, 3, 3, 5),
)

BASE_LEVELS: tuple[BaseLevelEntry, ...] = (
    BaseLevelEntry(0, 0, 0, 0),
    BaseLevelEntry(300, 6, 1, 1),
    BaseLevelEntry(600, 12, 1, 2),
    BaseLevelEntry(1000, 18, 2, 3),
)

class Side(Enum):
    LEFT = "A"
    RIGHT = "B"

    @property
    def opposite(self) -> "Side":
        return Side.RIGHT if self is Side.LEFT else Side.LEFT

class BType(Enum):
    HQ = "HQ"
    BASE = "BASE"

class WState(Enum):
    STATIONARY = 0
    MOVING = 1

class WarriorId(NamedTuple):
    side: Side
    num: int

    def __str__(self) -> str:
        return f"{self.side.value}{self.num}"

    @classmethod
    def parse(cls, tok: str) -> "WarriorId":
        return cls(Side.LEFT if tok[0] == "A" else Side.RIGHT, int(tok[1:]))

class Warrior:
    __slots__ = ['id', 'region', 'hp', 'state', 'target']
    
    def __init__(self, id: WarriorId, region: int, hp: int, state: WState = WState.STATIONARY, target: int = 0):
        self.id = id
        self.region = region
        self.hp = hp
        self.state = state
        self.target = target

class Building:
    __slots__ = ['region', 'side', 'type', 'level', 'hp']
    
    def __init__(self, region: int, side: Side, b_type: BType, level: int = 1, hp: int = 10):
        self.region = region
        self.side = side
        self.type = b_type
        self.level = level
        self.hp = hp

    def current_hp(self) -> int:
        return HQ_LEVELS[self.level].hp if self.type is BType.HQ else BASE_LEVELS[self.level].hp

    def work_cap(self) -> int:
        return HQ_LEVELS[self.level].work_cap if self.type is BType.HQ else BASE_LEVELS[self.level].work_cap

    def train_cap(self) -> int:
        return HQ_LEVELS[self.level].train_cap if self.type is BType.HQ else 0

    def apply_upgrade(self) -> None:
        self.level += 1
        self.hp = self.current_hp()

    def upgrade_cost(self) -> int:
        return HQ_LEVELS[self.level + 1].upgrade_cost if self.type is BType.HQ else BASE_LEVELS[self.level + 1].cost

class GameMap:
    __slots__ = ['N', 'K', 'x', 'y', 'strongholds', 'adj', 'my_side', 'my_hq', 'opp_hq']
    
    def __init__(self):
        self.N: int = 0
        self.K: int = 0
        self.x: List[int] = []
        self.y: List[int] = []
        self.strongholds: List[int] = []
        self.adj: List[List[int]] = []
        self.my_side: Side = Side.LEFT
        self.my_hq: int = 0
        self.opp_hq: int = 0

    def hq_of(self, s: Side) -> int:
        return 0 if s is Side.LEFT else self.N - 1

class GameState:
    __slots__ = ['gold', 'my_countdown', 'opp_countdown', 'warriors', 'buildings']
    
    def __init__(self):
        self.gold: int = START_GOLD
        self.my_countdown: int = 5
        self.opp_countdown: int = 5
        self.warriors: Dict[WarriorId, Warrior] = {}
        self.buildings: Dict[int, Building] = {}

    def get_building(self, region: int) -> Optional[Building]:
        return self.buildings.get(region)

    def get_warrior(self, wid: WarriorId) -> Optional[Warrior]:
        return self.warriors.get(wid)

class Actions:
    __slots__ = ['train_n', 'moves', 'upgrades']
    def __init__(self):
        self.train_n: int = 0
        self.moves: List[Tuple[WarriorId, int]] = []
        self.upgrades: List[int] = []

# --- PATHFINDING ---

class Paths:
    __slots__ = ['dist', 'nxt']
    def __init__(self, dist: List[List[float]], nxt: List[List[int]]):
        self.dist = dist
        self.nxt = nxt

def calculate_paths(M: GameMap) -> Paths:
    INF = math.inf
    N = M.N
    dist = [[INF] * N for _ in range(N)]
    nxt = [[-1] * N for _ in range(N)]

    for i in range(N):
        dist[i][i] = 0.0
        nxt[i][i] = i
        
    for u in range(N):
        for v in M.adj[u]:
            dist[u][v] = math.ceil(math.hypot(M.x[u] - M.x[v], M.y[u] - M.y[v]))

    for k in range(N):
        dk = dist[k]
        for u in range(N):
            du = dist[u]
            if du[k] == INF: continue
            for v in range(N):
                cand = du[k] + dk[v]
                if cand < du[v]:
                    du[v] = cand

    for u in range(N):
        du = dist[u]
        for v in range(N):
            if u == v or du[v] == INF: continue
            best_score = INF
            for nb in M.adj[u]:
                if dist[nb][v] == INF: continue
                score = math.ceil(math.hypot(M.x[u] - M.x[nb], M.y[u] - M.y[nb])) + dist[nb][v]
                if score < best_score:
                    best_score = score
                    nxt[u][v] = nb
                    
    return Paths(dist, nxt)


# --- AGENT (THE BRAIN) ---
class BotAgent:
    __slots__ = ['M', 'P', 'turn', 'opp_strongholds', 'my_strongholds', 'attack_squad_ids']
    
    def __init__(self, M: GameMap, P: Paths):
        self.M = M
        self.P = P
        self.turn = 0
        self.attack_squad_ids = set() # Danh sách biên chế "Biệt đội cảm tử"
        
        self.my_strongholds = sorted(M.strongholds, key=lambda s: P.dist[s][M.my_hq])
        self.opp_strongholds = sorted(M.strongholds, key=lambda s: P.dist[s][M.opp_hq])

    # --- HÀM TÌNH BÁO ---
    def count_warriors_in_radius(self, state: GameState, center: int, radius: float) -> Tuple[int, int]:
        allied, enemy = 0, 0
        for w in state.warriors.values():
            if self.P.dist[w.region][center] <= radius:
                if w.id.side == self.M.my_side:
                    allied += 1
                else:
                    enemy += 1
        return allied, enemy

    def can_move_warrior(self, state: GameState, w: Warrior) -> bool:
        if w.state == WState.MOVING: return False
        _, enemy = self.count_warriors_in_radius(state, w.region, 0)
        return enemy == 0 # Bị kẹt nếu có địch đứng chung ô

    def get_move_cost(self, state: GameState, target_region: int) -> int:
        b = state.get_building(target_region)
        return 0 if (b and b.side == self.M.my_side) else MOVE_COST

    def is_stronghold_safe(self, state: GameState, sh_region: int) -> bool:
        """Đảm bảo Ta tới nhanh hơn Địch và không có mai phục."""
        _, enemy_at_sh = self.count_warriors_in_radius(state, sh_region, 0)
        if enemy_at_sh > 0: return False
        
        min_enemy_dist = math.inf
        for w in state.warriors.values():
            if w.id.side != self.M.my_side:
                d = self.P.dist[w.region][sh_region]
                if d < min_enemy_dist: min_enemy_dist = d
        return self.P.dist[self.M.my_hq][sh_region] <= min_enemy_dist

    # --- BỘ NÃO CHIẾN THUẬT ---
    def decide(self, state: GameState, turn: int) -> Actions:
        self.turn = turn
        a = Actions()
        local_gold = state.gold
        has_trained = False
        upgraded_this_turn = set()
        
        DANGER_RADIUS = 250.0 # Khoảng cách quét báo động (Bạn có thể tinh chỉnh)

        my_warriors = [w for w in state.warriors.values() if w.id.side == self.M.my_side]
        enemy_warriors = [w for w in state.warriors.values() if w.id.side != self.M.my_side]
        my_bases = [b for b in state.buildings.values() if b.side == self.M.my_side and b.type == BType.BASE]
        hq_b = state.get_building(self.M.my_hq)
        
        def try_train(amount: int):
            nonlocal local_gold, has_trained
            if not has_trained and local_gold >= TRAIN_COST:
                cap = hq_b.train_cap() if hq_b else 1
                act = min(amount, cap, local_gold // TRAIN_COST)
                if act > 0:
                    a.train_n = act
                    local_gold -= act * TRAIN_COST
                    has_trained = True

        # ==========================================
        # 1. BÀNH TRƯỚNG BASE: Lấy nhiều nhất có thể
        # ==========================================
        for w in my_warriors:
            if w.state == WState.STATIONARY and w.region in self.M.strongholds and w.region not in state.buildings:
                if local_gold >= BASE_LEVELS[1].cost and w.region not in upgraded_this_turn:
                    _, enemy = self.count_warriors_in_radius(state, w.region, 0)
                    if enemy == 0:
                        a.upgrades.append(w.region)
                        upgraded_this_turn.add(w.region)
                        local_gold -= BASE_LEVELS[1].cost

        safe_empty_sh = [sh for sh in self.M.strongholds if sh not in state.buildings and self.is_stronghold_safe(state, sh)]

        # ==========================================
        # 2. NÂNG CẤP: Chừa tiền để tạo Base, dư mới nâng
        # ==========================================
        upg_cands = []
        if hq_b:
            c = hq_b.upgrade_cost() if hq_b.level < HQ_MAX_LEVEL else (HQ_HEAL_COST if hq_b.hp < hq_b.current_hp() else -1)
            if c > 0: upg_cands.append((c, 0, 0, self.M.my_hq))
        for b in my_bases:
            c = b.upgrade_cost() if b.level < BASE_MAX_LEVEL else (BASE_HEAL_COST if b.hp < b.current_hp() else -1)
            if c > 0: 
                _, enemy_near = self.count_warriors_in_radius(state, b.region, DANGER_RADIUS)
                upg_cands.append((c, 1, -enemy_near, b.region))
                
        upg_cands.sort(key=lambda x: (x[0], x[1], x[2]))
        
        for c, _, _, region in upg_cands:
            # Nếu còn Base trống để chiếm, phải giữ lại 430 vàng (300 xây + 120 lính + 10 move)
            reserved_gold = 430 if safe_empty_sh else 0
            if local_gold - c >= reserved_gold and region not in upgraded_this_turn:
                _, e_in_base = self.count_warriors_in_radius(state, region, 0)
                if e_in_base == 0:
                    a.upgrades.append(region)
                    upgraded_this_turn.add(region)
                    local_gold -= c

        # ==========================================
        # 3. KÍCH HOẠT TẤN CÔNG (1 Hit = 1 Sát thương)
        # ==========================================
        alive_ids = {w.id for w in my_warriors}
        self.attack_squad_ids = {wid for wid in self.attack_squad_ids if wid in alive_ids}

        # Số lính tối thiểu để gây >= 1 sát thương lên HQ
        enemy_hq_hp_sum = sum(w.hp for w in enemy_warriors if w.region == self.M.opp_hq)
        required_attackers = enemy_hq_hp_sum + 1

        if len(self.attack_squad_ids) < required_attackers:
            shortage = required_attackers - len(self.attack_squad_ids)
            available_w = [w for w in my_warriors if w.id not in self.attack_squad_ids]
            
            if len(available_w) >= shortage:
                # ĐỦ ĐIỀU KIỆN! Trưng dụng ngay lính gần địch nhất vào biên chế Tấn Công
                available_w.sort(key=lambda w: self.P.dist[w.region][self.M.opp_hq])
                for w in available_w[:shortage]:
                    self.attack_squad_ids.add(w.id)
            else:
                # Không đủ lực lượng -> Hủy đợt tấn công để bảo toàn sinh mạng
                self.attack_squad_ids.clear()

        # ==========================================
        # 4. HỆ THỐNG GIAO VIỆC & SINH LÍNH (STRICT LABOR)
        # ==========================================
        jobs = []
        if hq_b: jobs.extend([self.M.my_hq] * hq_b.work_cap())
        for b in my_bases:
            jobs.extend([b.region] * b.work_cap())
        jobs.extend(safe_empty_sh)

        # Lính không đi đánh nhau sẽ là Thợ (Workers)
        workers = [w for w in my_warriors if w.id not in self.attack_squad_ids]
        
        # Chỉ sinh lính NẾU CẦN để lấp đầy chỗ trống công việc!
        if len(workers) < len(jobs):
            try_train(1)

        worker_assignments = {}
        unassigned_workers = list(workers)

        for job_region in jobs:
            if not unassigned_workers: break
            best_w = min(unassigned_workers, key=lambda w: self.P.dist[w.region][job_region])
            worker_assignments[best_w.id] = job_region
            unassigned_workers.remove(best_w)

        for w in unassigned_workers:
            worker_assignments[w.id] = self.M.my_hq # Thất nghiệp thì ngoan ngoãn ở HQ

        # ==========================================
        # 5. THỰC THI DI CHUYỂN
        # ==========================================
        ordered_moves = set()

        # --- A. Lệnh di chuyển cho Biệt đội Tấn công (Synchronized Movement) ---
        if self.attack_squad_ids:
            squad_w = [w for w in my_warriors if w.id in self.attack_squad_ids]
            if squad_w:
                # CHÌA KHÓA: Tìm khoảng cách xa nhất. Chỉ những lính ở khoảng cách này mới được phép bước lên.
                d_max = max(self.P.dist[w.region][self.M.opp_hq] for w in squad_w)
                
                for w in squad_w:
                    if not self.can_move_warrior(state, w): continue
                    
                    if self.P.dist[w.region][self.M.opp_hq] == d_max and d_max > 0:
                        target = self.P.nxt[w.region][self.M.opp_hq]
                        c = self.get_move_cost(state, target)
                        if local_gold >= c:
                            a.moves.append((w.id, target))
                            ordered_moves.add(w.id)
                            local_gold -= c

        # --- B. Lệnh di chuyển cho Thợ & Báo Động Lùi ---
        for w in workers:
            if w.id in ordered_moves or not self.can_move_warrior(state, w): continue

            # PHÒNG VỆ: Lùi lại dần nếu địch đông hơn/bằng phe ta trong khu vực X
            allied_near, enemy_near = self.count_warriors_in_radius(state, w.region, DANGER_RADIUS)
            if enemy_near >= allied_near and enemy_near > 0:
                target = self.P.nxt[w.region][self.M.my_hq]
                if target != w.region:
                    c = self.get_move_cost(state, target)
                    if local_gold >= c:
                        a.moves.append((w.id, target))
                        ordered_moves.add(w.id)
                        local_gold -= c
                continue 

            # LÀM VIỆC: Nếu không có địch, chạy tới nơi làm việc được phân công
            target_job = worker_assignments.get(w.id, self.M.my_hq)
            if w.region != target_job:
                target = self.P.nxt[w.region][target_job]
                c = self.get_move_cost(state, target)
                if local_gold >= c:
                    a.moves.append((w.id, target))
                    ordered_moves.add(w.id)
                    local_gold -= c

        return a


# --- IO & GAME LOOP ---

def readln() -> str:
    line = sys.stdin.readline()
    if not line:
        sys.exit(0)
    return line.rstrip("\n")

def read_tokens() -> List[str]:
    return readln().split()

def parse_init() -> Tuple[GameMap, GameState]:
    M = GameMap()
    t = read_tokens()
    M.my_side = Side.LEFT if t[1] == "LEFT" else Side.RIGHT

    t = read_tokens()
    M.N, M.K = int(t[0]), int(t[1])
    M.x = [int(v) for v in read_tokens()]
    M.y = [int(v) for v in read_tokens()]
    M.strongholds = sorted(int(v) for v in read_tokens())

    M.adj = [[] for _ in range(M.N)]
    for r in range(M.N):
        t = read_tokens()
        M.adj[r] = sorted(int(v) for v in t[1:1 + int(t[0])])

    M.my_hq = M.hq_of(M.my_side)
    M.opp_hq = M.hq_of(M.my_side.opposite)

    S = GameState()
    opp = M.my_side.opposite
    
    for sfx in range(1, START_WARRIORS + 1):
        w_my = WarriorId(M.my_side, sfx)
        w_opp = WarriorId(opp, sfx)
        S.warriors[w_my] = Warrior(w_my, M.my_hq, HQ_LEVELS[1].warrior_hp)
        S.warriors[w_opp] = Warrior(w_opp, M.opp_hq, HQ_LEVELS[1].warrior_hp)

    S.buildings[0] = Building(0, Side.LEFT, BType.HQ, 1, HQ_LEVELS[1].hp)
    S.buildings[M.N - 1] = Building(M.N - 1, Side.RIGHT, BType.HQ, 1, HQ_LEVELS[1].hp)

    print("OK", flush=True)
    return M, S

def read_turn_start() -> Optional[int]:
    line = readln()
    if line == "FINISH":
        return None
    return int(line.split()[2])

def read_turn_result(S: GameState, M: GameMap, submitted: Actions) -> None:
    for region in submitted.upgrades:
        b = S.get_building(region)
        if b is None:
            S.gold -= BASE_LEVELS[1].cost
            S.buildings[region] = Building(region, M.my_side, BType.BASE, 1, BASE_LEVELS[1].hp)
        else:
            max_lvl = HQ_MAX_LEVEL if b.type is BType.HQ else BASE_MAX_LEVEL
            if b.level >= max_lvl:
                S.gold -= HQ_HEAL_COST if b.type is BType.HQ else BASE_HEAL_COST
                b.hp = b.current_hp()
            else:
                S.gold -= b.upgrade_cost()
                b.apply_upgrade()

    for wid, target in submitted.moves:
        b = S.get_building(target)
        S.gold -= 0 if (b and b.side is M.my_side) else MOVE_COST
        w = S.get_warrior(wid)
        if w:
            w.state = WState.MOVING
            w.target = target

    S.gold -= TRAIN_COST * submitted.train_n

    if readln() == "FINISH":
        sys.exit(0)

    t = read_tokens()
    S.my_countdown, S.opp_countdown = int(t[2]), int(t[4])

    t = read_tokens()
    for _ in range(int(t[1])):
        r = read_tokens()
        s = Side.LEFT if r[0][0] == 'A' else Side.RIGHT
        region = int(r[1])
        b = S.get_building(region)
        if b is None:
            S.buildings[region] = Building(region, s, BType.BASE, 1, BASE_LEVELS[1].hp)
        elif b.side is not M.my_side:
            max_lvl = HQ_MAX_LEVEL if b.type is BType.HQ else BASE_MAX_LEVEL
            if b.level >= max_lvl:
                b.hp = b.current_hp()
            else:
                b.apply_upgrade()

    t = read_tokens()
    n = int(t[1])
    if n > 0:
        ids = read_tokens()
        for i in range(n):
            wid = WarriorId.parse(ids[i])
            hq_region = M.hq_of(wid.side)
            hq_b = S.get_building(hq_region)
            hq_lvl = hq_b.level if hq_b else 1
            S.warriors[wid] = Warrior(wid, hq_region, HQ_LEVELS[hq_lvl].warrior_hp)

    t = read_tokens()
    for _ in range(int(t[1])):
        r = read_tokens()
        wid = WarriorId.parse(r[0])
        w = S.get_warrior(wid)
        if w:
            w.region = int(r[1])
            if wid.side is M.my_side and w.state is WState.MOVING and w.region == w.target:
                w.state = WState.STATIONARY

    t = read_tokens()
    for _ in range(int(t[1])):
        r = read_tokens()
        wid = WarriorId.parse(r[1])
        w = S.get_warrior(wid)
        if w:
            w.hp -= int(r[2])
    
    S.warriors = {k: v for k, v in S.warriors.items() if v.hp > 0}

    t = read_tokens()
    for _ in range(int(t[1])):
        r = read_tokens()
        b = S.get_building(int(r[1]))
        if b:
            b.hp -= int(r[2])
            
    S.buildings = {k: v for k, v in S.buildings.items() if v.hp > 0}

    readln()  

    income = 0
    for b in S.buildings.values():
        if b.side is M.my_side:
            count = sum(1 for w in S.warriors.values() if w.id.side is M.my_side and w.region == b.region)
            income += WORK_INCOME * min(count, b.work_cap())
    S.gold += income

    alive = sum(1 for w in S.warriors.values() if w.id.side is M.my_side)
    S.gold = max(0, S.gold - UPKEEP_PER_WARRIOR * alive)

def emit(a: Actions) -> None:
    out: List[str] = ["COMMAND"]
    for wid, target in a.moves:
        out.append(f"MOVE {wid} {target}")
    for r in a.upgrades:
        out.append(f"UPGRADE {r}")
    if a.train_n > 0:
        out.append(f"TRAIN {a.train_n}")
    out.append("END")
    sys.stdout.write("\n".join(out) + "\n")
    sys.stdout.flush()

def main() -> None:
    M, S = parse_init()
    P = calculate_paths(M)
    agent = BotAgent(M, P)

    while (turn := read_turn_start()) is not None:
        a = agent.decide(S, turn)
        emit(a)
        read_turn_result(S, M, a)

if __name__ == "__main__":
    sys.setrecursionlimit(2000)
    main()