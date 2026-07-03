# NEXT NATION — Statement (Tiếng Việt)

> Tài liệu này được dịch và hệ thống lại từ statement gốc trong file `NYPC.html`. Không có nội dung nào được thêm hoặc thay đổi so với bản gốc.

---

## Tổng quan

**NEXT NATION** là trò chơi chiến thuật theo lượt dành cho hai người chơi. Người chơi xây công trình, huấn luyện chiến binh và phá hủy tổng hành dinh của đối thủ.

---

## 1. Luật chơi

### 1.1. Giới thiệu

Trò chơi diễn ra trên **chiến trường** (*battlefield*) gồm `N` khu vực, đánh số từ `0` đến `N-1`, có tính đối xứng tâm qua gốc tọa độ.

- Khu vực `#0` (ngoài cùng bên trái) và khu vực `#N-1` (ngoài cùng bên phải) mỗi khu vực có một **tổng hành dinh** (*headquarter*).
- Trong các khu vực không có tổng hành dinh, có `K` khu vực là **cứ điểm** (*stronghold*), nơi có thể xây **căn cứ** (*base*).

Ràng buộc kích thước:

$$51 \le N \le 10^9$$

$$\lceil 0.15N \rceil \le K \le \lfloor 0.2N \rfloor$$

Cả `N` và `K` đều là số lẻ.

Mỗi người chơi bắt đầu với:

- `3` **chiến binh** (*warriors*) tại tổng hành dinh;
- `500` vàng;
- Tổng hành dinh cấp `1`, HP = `10`;
- Mỗi chiến binh HP = `4`.

Nếu tổng hành dinh bị phá hủy, người sở hữu tổng hành dinh đó thua.

---

### Các lệnh hàng ngày

Mỗi ngày, người chơi có thể gửi các loại lệnh sau:

**Upgrade** — Nâng cấp công trình đồng minh (tổng hành dinh hoặc căn cứ), hoặc xây căn cứ mới trên cứ điểm chưa có công trình. Điều kiện: tại khu vực mục tiêu phải có ít nhất một chiến binh đồng minh và không có chiến binh địch.

**Move** — Ra lệnh cho một chiến binh đang không ở trạng thái di chuyển đi đến khu vực mong muốn. Nếu khu vực đích có công trình đồng minh, không tốn vàng.

**Train** — Dùng `120 × n` vàng để huấn luyện `n` chiến binh tại tổng hành dinh, với `n` không âm và không vượt quá số chiến binh có thể huấn luyện. Không được dùng lệnh `Train` quá một lần mỗi ngày.

---

Sau khi gửi lệnh, ngày mới bắt đầu theo thứ tự:

```
The Morning → The Daytime → The Evening
```

---

### 1.2. Buổi sáng — The Morning

Thứ tự các bước:

```
Building Step → Moving Step → Training Step
```

**Building Step:** Căn cứ mới được xây và các công trình được nâng cấp bằng cách tiêu vàng.

**Moving Step:** Các chiến binh nhận lệnh `Move` trong ngày hiện tại hoặc đang trong trạng thái di chuyển sẽ di chuyển tối đa một khu vực theo Luật di chuyển (Mục 3). Tốn `10` vàng cho mỗi chiến binh nhận lệnh di chuyển hôm nay, trừ khi đích đến có công trình đồng minh.

**Training Step:** Số chiến binh đã được ra lệnh huấn luyện sẽ được tạo ra, tốn `120` vàng mỗi chiến binh. HP của mỗi chiến binh mới được xác định bởi cấp tổng hành dinh. Chiến binh được đánh số duy nhất — huấn luyện sớm hơn có chỉ số nhỏ hơn.

---

### 1.3. Ban ngày — The Daytime

Chỉ có **Battle Step**: các chiến binh tấn công chiến binh hoặc công trình khác theo Luật chiến đấu (Mục 3).

---

### 1.4. Buổi tối — The Evening

Thứ tự các bước:

```
Labor Step → Supply Step
```

**Labor Step:** Mỗi công trình tạo ra `15 × x` vàng, trong đó `x` là giá trị nhỏ hơn giữa số chiến binh đồng minh trong khu vực và số ô lao động (*# of labor slots*).

**Supply Step:** Mỗi chiến binh tiêu thụ `2` vàng theo thứ tự tăng dần của số hiệu. Nếu không đủ vàng, chiến binh đó mất `1` HP. Nếu HP về `0`, chiến binh rút lui và rời khỏi chiến trường.

---

## 2. Công trình

Công trình là nguồn tạo thu nhập. Sau khi xây, nâng cấp hoặc sửa chữa, HP công trình trở về HP tối đa.

Khi bắt đầu mỗi **Battle Step**, tháp pháo (*turret*) trong công trình thực hiện số lần tấn công trong cùng khu vực bằng với chỉ số sát thương tấn công (*AD*) của tháp pháo.

Lệnh `Upgrade` chỉ được gửi tối đa một lần mỗi ngày cho mỗi khu vực. Một công trình không thể được nâng cấp hai lần trong cùng một ngày, và cũng không thể được nâng cấp trong cùng ngày mà nó được xây.

---

### 2.1. Tổng hành dinh — Headquarter (HQ)

Mỗi người chơi bắt đầu với tổng hành dinh cấp `1`. Mỗi người chơi chỉ được có một tổng hành dinh.

| Cấp | Chi phí nâng cấp | HP tối đa chiến binh | HP tối đa HQ | AD tháp pháo | Chiến binh có thể huấn luyện | Ô lao động |
|:---:|---:|:---:|:---:|:---:|:---:|:---:|
| `1` | — | `4` | `10` | `1` | `1` | `1` |
| `2` | `600` vàng | `5` | `15` | `2` | `1` | `2` |
| `3` | `1,200` vàng | `6` | `20` | `2` | `2` | `3` |
| `4` | `2,400` vàng | `7` | `25` | `3` | `2` | `4` |
| `5` | `3,600` vàng | `8` | `30` | `3` | `3` | `5` |
| `5+` (Sửa chữa) | `1,000` vàng | — | — | — | — | — |

---

### 2.2. Căn cứ — Base

Mỗi người chơi có thể xây bao nhiêu căn cứ tùy ý. Khác với tổng hành dinh, căn cứ không thể huấn luyện chiến binh.

| Cấp | Chi phí xây/nâng cấp | HP tối đa | AD tháp pháo | Ô lao động |
|:---:|---:|:---:|:---:|:---:|
| `1` | `300` vàng | `6` | `1` | `1` |
| `2` | `600` vàng | `12` | `1` | `2` |
| `3` | `1,000` vàng | `18` | `2` | `3` |
| `3+` (Sửa chữa) | `500` vàng | — | — | — |

---

## 3. Di chuyển và Chiến đấu

### 3.1. Luật di chuyển — Movement Rule

Nếu không có chiến binh địch trong cùng khu vực với chiến binh đang thực hiện lệnh `Move`, chiến binh di chuyển một khu vực theo đường đi có tổng khoảng cách đường thẳng giữa các khu vực liền kề là nhỏ nhất.

**Khoảng cách đường thẳng** giữa hai điểm $(x_1, y_1)$ và $(x_2, y_2)$ được định nghĩa là:

$$\left\lceil \sqrt{(x_1 - x_2)^2 + (y_1 - y_2)^2} \right\rceil$$

tức là phần nguyên làm tròn lên của khoảng cách Euclid.

Mỗi chiến binh tìm đường đến mục tiêu bằng cách chỉ di chuyển qua các khu vực liền kề, tối thiểu hóa tổng khoảng cách, rồi di chuyển đến khu vực tiếp theo trên đường đi đó. Nếu có nhiều đường đi như nhau, chiến binh di chuyển đến khu vực có chỉ số nhỏ hơn.

Sau khi nhận lệnh `Move`, chiến binh vào trạng thái di chuyển và không thể nhận lệnh `Move` khác trước khi đến đích. Trạng thái di chuyển kết thúc ngay khi chiến binh đến khu vực mục tiêu.

- Nếu đã gửi lệnh `Move` có tốn vàng, số vàng đó **không được hoàn lại** dù sau đó công trình được xây ở khu vực mục tiêu.
- Nếu đã gửi lệnh `Move` không tốn vàng, việc di chuyển **không bị hủy** dù công trình ở khu vực mục tiêu bị phá hủy.

---

### 3.2. Luật chiến đấu — Battle Rule

Trong mỗi **Battle Step**, các bước diễn ra theo thứ tự:

1. Trong mỗi khu vực có công trình, công trình thực hiện số lần tấn công bằng với AD của tháp pháo.
2. Trong mỗi khu vực, giả sử số chiến binh của ta là `a` và đối thủ là `b`; đối thủ bị tấn công `a` lần và ta bị tấn công `b` lần. Chiến binh có HP = `0` chưa rút lui cho đến khi trận chiến kết thúc, vì vậy vẫn được tính trong `a` và `b`.
3. Sau khi trận chiến kết thúc, chiến binh có HP = `0` rút lui. Công trình có HP = `0` bị phá hủy.

**Luật cho mỗi lần tấn công:**

- Trong số các chiến binh đối thủ còn HP dương, giảm HP của chiến binh có HP **nhỏ nhất** đi `1`. Nếu có nhiều chiến binh như vậy, chiến binh có **số hiệu nhỏ nhất** bị chọn.
- Nếu không có chiến binh đối thủ nào còn HP dương và công trình đối thủ còn HP dương, HP của công trình đó bị giảm `1`.

---

## 4. Kết thúc trò chơi

Trong vòng `200` ngày:

- Nếu một tổng hành dinh bị phá hủy → người còn lại **thắng**.
- Nếu cả hai bị phá hủy trong cùng một ngày, hoặc cả hai còn nguyên sau `200` ngày → bên có tổng hành dinh **nhiều HP hơn** thắng.
- Nếu HP bằng nhau → **hòa**.

---

## 5. Phương pháp sinh chiến trường (chi tiết)

> Vì cấu trúc chiến trường được cung cấp trong input, người chơi **không cần biết** luật sinh để giải bài.

### 5.1. Chiến trường

Chiến trường biểu diễn trên mặt phẳng Euclid vô hạn, gồm đúng $N = 2N' + 1$ khu vực đa giác.

- Mỗi khu vực có một **tâm** (*center*) nằm bên trong đa giác, không trên biên.
- Hai khu vực **liền kề** nếu có chung ít nhất một cạnh.
- Tổng hành dinh đặt tại khu vực `#0` và `#N-1`.
- Có $K = 2K' + 1$ khu vực là cứ điểm.

### 5.2. Quy trình sinh chiến trường

Các hằng số: $L = 10^4$, $D = 100$, $A = 24$. Mỗi lần chọn là độc lập và đều xác suất.

**Bước 1 — Xác định $N'$ và $K'$**

$N'$ chọn ngẫu nhiên từ $[25, 54]$. Sau đó $K'$ chọn ngẫu nhiên từ:

$$\left\lceil \frac{0.15(2N' + 1) - 1}{2} \right\rceil \le K' \le \left\lfloor \frac{0.2(2N' + 1) - 1}{2} \right\rfloor$$

**Bước 2 — Tạo đường tròn $S$**

$S$ là đường tròn tâm gốc tọa độ, bán kính $L$.

**Bước 3 — Sinh tập điểm $P$**

Ban đầu $P = \{(0, 0)\}$. Lặp $N'$ lần:

1. Chọn điểm lưới $u = (x_u, y_u)$ trên hoặc trong $S$.
2. Nếu tồn tại $v = (x_v, y_v) \in P$ thỏa $x_u = x_v$ hoặc $\mathrm{dist}(u, v) < D$, chọn lại.
3. Thêm $u$ và $u'$ (đối xứng của $u$ qua gốc $O$) vào $P$.

Sau khi lặp: $|P| = 2N' + 1$. Đánh số các điểm từ $0$ đến $N-1$ theo thứ tự tăng dần của tọa độ $x$.

**Bước 4 — Tạo tập điểm $Q$**

Vẽ đường tròn $C$ tâm $O$, bán kính $1.5L$, chia thành $A$ phần bằng nhau. Cụ thể: lấy hai giao điểm $t_1$, $t_2$ của $C$ với trục $x$, chia cung trên và cung dưới nối $t_1$ với $t_2$ thành $A/2$ phần bằng nhau. Các điểm chia đó là $Q$.

**Bước 5 — Tính Voronoi và Delaunay, rồi tính lại**

Tính biểu đồ Voronoi và tam giác hóa Delaunay trên $P \cup Q$. Thu thập các trọng tâm đa giác Voronoi đối ngẫu với từng điểm trong $P$, gọi tập đó là $P'$, rồi gán lại số thứ tự. Sau đó tính lại Voronoi và Delaunay trên $P' \cup Q$.

**Bước 6 — Xác định khu vực và quan hệ liền kề**

Đa giác Voronoi đối ngẫu với điểm $t$ trở thành khu vực $\#t$; điểm $t$ là tâm khu vực đó. Mỗi khu vực liền kề với các khu vực được nối bởi tam giác hóa Delaunay (không được hình thành từ $Q$).

**Bước 7 — Chọn các cứ điểm**

Ban đầu $R = \{N'\}$. Lặp $K'$ lần:

1. Chọn ngẫu nhiên $t \in [0, N-1]$, $t \notin R$.
2. Nếu khu vực $\#t$ hoặc $\#(N-1-t)$ liền kề với bất kỳ $\#r$ nào với $r \in R$, chọn lại.
3. Nếu khu vực $\#t$ và $\#(N-1-t)$ liền kề nhau, chọn lại.
4. Thêm $t$ và $N-1-t$ vào $R$.

Sau khi lặp: $|R| = 2K' + 1$. Mỗi $r \in R$ là một cứ điểm.

---

## 6. Tương tác — Interaction

Chương trình đọc input từ `stdin` và tuân theo protocol của từng lệnh.

### 6.1. READY

```
READY (LEFT | RIGHT)
N K
x_0 ... x_{N-1}
y_0 ... y_{N-1}
p_0 ... p_{K-1}
a_0 b_{0,0} ... b_{0,a_0-1}
...
a_{N-1} b_{N-1,0} ... b_{N-1,a_{N-1}-1}
```

Lệnh này thông báo cho người chơi biết phía của mình và bản đồ chiến trường:

- Dòng 1: phía tổng hành dinh — `LEFT` hoặc `RIGHT`.
- Dòng 2: giá trị `N` và `K`.
- `x_i`, `y_i`: tọa độ tâm khu vực `#i`.
- `p_i`: chỉ số các cứ điểm.
- Khu vực `#i` liền kề với `a_i` khu vực; chỉ số các khu vực liền kề là `b_{i,j}` với $0 \le j < a_i$.

Người chơi phải output `OK` trong vòng `1,000` ms. Nếu không, bị **TLE** và thua.

---

### 6.2. Bắt đầu lượt — START

```
START TURN T
```

Đầu mỗi lượt, lệnh `START` được đưa ra. `T` là chỉ số ngày.

---

### 6.3. Gửi lệnh — COMMAND

```
COMMAND
...
MOVE a d
TRAIN n
UPGRADE d
...
END
```

Người chơi bắt đầu pha gửi lệnh bằng `COMMAND`, gửi số lệnh tùy ý theo bất kỳ thứ tự, rồi kết thúc bằng `END`.

Chiến binh ban đầu được đánh số: `A1`, `A2`, `A3` (người chơi trái); `B1`, `B2`, `B3` (người chơi phải).

Các loại lệnh:

- `MOVE a d` — di chuyển chiến binh `#a` đến khu vực `#d`.
- `TRAIN n` — huấn luyện `n` chiến binh tại tổng hành dinh. Tối đa 1 lần mỗi ngày.
- `UPGRADE d` — nâng cấp hoặc sửa chữa công trình đồng minh ở khu vực `#d`. Nếu không có công trình, xây căn cứ mới.

**Giới hạn thời gian:** Phải output `END` trong vòng `100` ms sau khi nhận `START`. Nếu vượt quá $t$ ms, tiêu thụ $\lceil t / 100 \rceil$ countdown token. Hết token mà vẫn vượt giờ → thua **TLE**. Mỗi người chơi bắt đầu với `5` countdown token.

Gửi lệnh không hợp lệ → thua ngay **WA**. Cả hai cùng gửi lệnh không hợp lệ → **hòa**.

---

### 6.4. Xem kết quả — VIEW RESULT

```
TURN T
TIME T_x R_x T_y R_y
UPGRADE N
t_1 d_1
...
TRAIN N
a_1 ... a_N
MOVE N
a_1 d_1
...
DAMAGE N
C_1 a_1 h_1
...
SIEGE N
t_1 d_1 h_1
...
```

Nếu cả hai người chơi gửi lệnh bình thường, kết quả được trả về. Nếu `N = 0` trong phần `TRAIN`, `UPGRADE`, `MOVE`, `DAMAGE` hoặc `SIEGE`, không có dòng tiếp theo cho phần đó.

**TURN T** — Thông tin của ngày `T`.

**TIME T_x R_x T_y R_y** — `T_x`, `T_y`: thời gian bạn/đối thủ dùng trong lượt (ms); `R_x`, `R_y`: countdown token còn lại của bạn/đối thủ.

**UPGRADE N** — `N` công trình được xây/nâng cấp/sửa chữa. Mỗi dòng tiếp theo: `t_i` (chủ sở hữu: `A` hoặc `B`), `d_i` (chỉ số khu vực).

**TRAIN N** — `N` chiến binh được huấn luyện. Dòng tiếp theo: chỉ số các chiến binh mới theo thứ tự. Chỉ số dạng `A`/`B` + số nguyên (ví dụ `A12`, `B7`), tăng tuần tự từ `1` cho mỗi người chơi.

**MOVE N** — `N` chiến binh đã di chuyển. Mỗi dòng: `a_i` (chỉ số chiến binh), `d_i` (khu vực đã đến).

**DAMAGE N** — `N` bản ghi sát thương lên chiến binh. Mỗi dòng: `C_i` (nguyên nhân), `a_i` (chiến binh bị sát thương), `h_i` (lượng sát thương). `C_i` là một trong:
  - `TURRET` — sát thương từ tháp pháo;
  - `COMBAT` — sát thương từ chiến đấu giữa chiến binh;
  - `HUNGER` — sát thương do thiếu vàng ở **Supply Step**.

**SIEGE N** — `N` bản ghi sát thương lên công trình. Mỗi dòng: `t_i` (chủ sở hữu: `A`/`B`), `d_i` (khu vực), `h_i` (lượng sát thương).

Kết quả mỗi lượt kết thúc bằng `END`.

---

### 6.5. Kết thúc trò chơi — FINISH

```
FINISH
```

Khi trò chơi kết thúc vì bất kỳ lý do nào, lệnh `FINISH` được đưa ra. Chương trình phải **lập tức kết thúc bình thường** — nếu `FINISH` xuất hiện thay vì kết quả sau một lượt, chương trình phải dừng ngay, không đọc thêm.

---

## 7. Game Simulator

Link simulator: [https://d3a0kd35ch7dmv.cloudfront.net/](https://d3a0kd35ch7dmv.cloudfront.net/)

Có thể dán log vào simulator để phân tích quá trình chơi.

---

## 8. CLI Tool

Link tải: [https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/nation-providing.zip](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/nation-providing.zip)

Dùng CLI tool để cho các AI đấu với nhau trên máy cục bộ.

---

## 9. Sample AI

| Battle Number | Mô tả |
|:---:|---|
| `1` | Không đưa ra bất kỳ lệnh nào. |
| `2` | Thực hiện cùng các thao tác như sample code. |
| `3` | Dùng chiến thuật tấn công cơ bản. |
| `4` | Dùng chiến thuật phát triển cơ bản. |
| `5` | Dùng chiến thuật tấn công sơ cấp. |
| `6` | Dùng chiến thuật phát triển sơ cấp. |
| `7` | Dùng chiến thuật tấn công thích ứng. |
| `8` | Dùng chiến thuật tấn công nâng cao. |

---

## 10. Ngôn ngữ được hỗ trợ

| Ngôn ngữ | Giới hạn bộ nhớ | Sample code |
|---|:---:|---|
| C20 (`gcc 14.2.0`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.c) |
| C++20 (`g++ 14.2.0`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.cpp) |
| Python3 (`Python 3.12.3`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.py) |
| PyPy3 (`Python 3.9.18`, `PyPy 7.3.15 with GCC 13.2.0`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.py) |
| OpenJDK Java 25 (`openjdk 25.0.2 2026-01-20`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/Main.java) |
| Rust 2024 (`cargo 1.93.1`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.rs) |
| Node.js (`v24.13.1`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.js) |
| Typescript 5.9.3 (`nodejs v24.13.1`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.ts) |
| C# 10 (`dotnet 10.0.103`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.cs) |
| Kotlin JVM 2.3 (`kotlinc-jvm 2.3.10`, `JRE 25.0.2+10-Ubuntu-124.04`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.kt) |
| Go (`go 1.26.0`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.go) |
| Scala JVM 3.8 (`Scala 3.8.1`, `JRE 25.0.2+10-Ubuntu-124.04`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.scala) |
| Lua 5.4 (`Lua 5.4.6`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.lua) |
| Lua 5.1 (`LuaJIT 2.1.1748459687`) | `1,024 MiB` | [Download](https://nypc-static.s3.ap-northeast-2.amazonaws.com/2026/nextnation-61bnx71k/sample-code/sample-code.lua) |
| C++26 (`gcc 15`) | `1,024 MiB` | *(không có sample code)* |
| Rust 2024 Nightly (`cargo >= 1.95.0`) | `1,024 MiB` | *(không có sample code)* |
