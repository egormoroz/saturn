#include "selfplay.hpp"

#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <algorithm>
#include <random>
#include <list>
#include <fstream>
#include <thread>

#include "search/search.hpp"
#include "mininnue/nnue.hpp"
#include "pack.hpp"
#include "primitives/utility.hpp"


static std::mt19937& get_thread_local_rng() {
    struct Rng {
        Rng() 
            : gen(uint32_t(timer::now()))
        {
        }

        std::mt19937 gen;
    };
    static thread_local Rng rng;
    return rng.gen;
}

constexpr float lerp(float t, float a, float b) {
    return (1 - t) * a + t * b;
}

template<typename T>
class Queue {
public:
    Queue() = default;

    void push(T &&t) {
        std::unique_lock<std::mutex> lck(m_);

        q_.push(std::forward<T>(t));
        lck.unlock();

        non_empty_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lck(m_);
        non_empty_.wait(lck, [this]{ return !q_.empty(); });

        T val = std::move(q_.front());
        q_.pop();

        return val;
    }

    bool try_pop(T &t) {
        std::lock_guard<std::mutex> lck(m_);
        if (q_.empty())
            return false;
        t = std::move(q_.front());
        q_.pop();
        return true;
    }

private:
    std::condition_variable non_empty_;
    std::mutex m_;
    std::queue<T> q_;
};

struct Judge {
    enum Reason {
        NO_REASON = 0,
        SCORE_THRESHOLD,
        CHECKMATE,
        STALEMATE,
        REPEATED_DRAW_SCORE,
        FIFTY_MOVE_RULE,
        THREEFOLD_REP,
        MATERIAL_DRAW,
        MAX_PLY_REACHED,
    };

    void adjudicate(const Board &b, const Stack &st, 
            Move m, int16_t score, int ply) 
    {
        Color stm = b.side_to_move();

        if (abs(score) > 10)
            draw_score_plies = 0;
        else if (ply >= 100)
            ++draw_score_plies;

        if (!is_ok(m)) {
            if (b.checkers()) {
                result = ~stm;
                reason = CHECKMATE;
            } else {
                result = GameOutcome::DRAW;
                reason = STALEMATE;
            }
            return;
        }

        // the score is reversed, since the move m is already applied
        score = -score;

        if (abs(score) > 10000) {
            result = score > 0 ? stm : ~stm;
            reason = SCORE_THRESHOLD;
            return;
        }

        if (b.half_moves() >= 100) {
            result = GameOutcome::DRAW;
            reason = FIFTY_MOVE_RULE;
            return;
        }

        if (st.is_repetition(b) && score == 0) {
            result = GameOutcome::DRAW;
            reason = THREEFOLD_REP;
            return;
        }

        if (!b.checkers() && b.is_material_draw()) {
            result = GameOutcome::DRAW;
            reason = MATERIAL_DRAW;
            return;
        }

        if (draw_score_plies >= 8) {
            result = GameOutcome::DRAW;
            reason = REPEATED_DRAW_SCORE;
            return;
        }

        if (ply >= MAX_PLIES - MAX_DEPTH * 2) {
            result = GameOutcome::DRAW;
            reason = MAX_PLY_REACHED;
            return;
        }
    }

    int draw_score_plies = 0;
    int result = -1;
    Reason reason = NO_REASON;
};

static const char* reason_to_str(Judge::Reason r) {
    if (r < 0 || r > Judge::MAX_PLY_REACHED)
        return "res_inv";

    const char* strs[Judge::MAX_PLY_REACHED + 1] = {
        "NO_REASON",
        "score",
        "checkmate",
        "stalemate",
        "rep_lowscore",
        "50",
        "3-fold",
        "mat_draw",
        "max_ply",
    };

    return strs[r];
}

struct Entry {
    PosChain pc;
    uint64_t hash;
    Judge::Reason reason;
};

using PosQueue = Queue<Entry>;

class Session {
public:
    Session(PosQueue &q, const SearchLimits &limits, int n_pvs, int max_ld_moves)
        : board_(si_stack_), limits_(limits), num_pvs_(n_pvs), 
          max_ld_moves_(max_ld_moves), q_(q), keep_going_(true),
          th_([this] { thread_routine(); })
    {
    }

    void stop() {
        keep_going_ = false;
        if (th_.joinable())
            th_.join();
    }

    ~Session() {
        stop();
    }


private:
    void thread_routine() {
        search_.set_silent(true);

        PosChain pc;
        while (keep_going_) {
            SearchLimits limits = limits_;
            Judge judge;

            stack_.reset();
            board_ = Board::start_pos(si_stack_);
            int start_ply = setup_board();

            pc.n_moves = 0;
            pc.start = pack_board(board_);
            uint64_t hash = board_.key();

            for (int ply = start_ply; judge.result < 0; ++ply) {
                if (!keep_going_) return;

                limits.start = timer::now();

                stack_.set_start(stack_.total_height());
                search_.setup(board_, limits, &stack_, false, num_pvs_);

                search_.iterative_deepening();

                int n_pvs = search_.num_pvs();
                // no legal moves
                if (n_pvs == 0) {
                    judge.adjudicate(board_, stack_, MOVE_NONE, 0, 0);
                    break;
                }

                // record the position score, but take possibly not the best move
                int16_t score = search_.get_pv_start(0).score;
                int pv_idx = choose_pv(100);
                Move move = search_.get_pv_start(pv_idx).move;
                assert(board_.is_valid_move(move));

                pc.seq[pc.n_moves++] = { move, score };

                Color stm = board_.side_to_move();
                limits.time[stm] += limits.inc[stm];
                stack_.push(board_.key(), move);
                board_ = board_.do_move(move, &si_stack_[ply+1]);
                hash ^= board_.key();

                judge.adjudicate(board_, stack_, move, score, ply);
            }

            assert(judge.result != -1);
            pc.result = judge.result;

            if (!pc.n_moves) {
                printf("[WARN] selfplay worker: empty sequence???\n");
                continue;
            }
            q_.push({ pc, hash, judge.reason });
        }
    }

    int setup_board() {
        int ply = make_random_moves(2, 0.5f);

        auto rng = get_thread_local_rng();
        std::uniform_int_distribution<int> dist(0, max_ld_moves_);
        int n_ld_moves = dist(rng);

        SearchLimits limits;
        limits.depth = 2;
        limits.type = limits.DEPTH;

        constexpr int stop_score_thresh = 500;
        constexpr int max_pv_diff = 50;
        constexpr int ld_multipv = 20;

        int multipv = std::max(ld_multipv, num_pvs_);

        // Make several low depth moves. This results in okay-ish positions 
        // and (hopefully) fixes the issue with not generating *any* endgame positions.
        for (int i = 0; i < n_ld_moves; ++i) {
            limits.start = timer::now();
            search_.setup(board_, limits, nullptr, false, multipv);
            search_.iterative_deepening();

            int n_pvs = search_.num_pvs();
            if (!n_pvs)
                break;
            int score = search_.get_pv_start(0).score;
            if (abs(score) > stop_score_thresh)
                break;

            Move m = search_.get_pv_start(choose_pv(max_pv_diff)).move;
            stack_.push(board_.key(), m);
            board_ = board_.do_move(m, &si_stack_[++ply]);
        }

        return ply;
    }

    int make_random_moves(int n_moves, float temp=1.f) {
        ExtMove moves[MAX_MOVES];
        float weights[MAX_MOVES];
        int total_moves = 0;

        auto rng = get_thread_local_rng();
        int ply = 0;

        auto assign_weights = [&]() {
            float min_weight = 1;
            for (int i = 0; i < total_moves; ++i) {
                Board b = board_.do_move(moves[i], &si_stack_[ply+1]);
                weights[i] = static_cast<float>(-mini::evaluate(b));
                min_weight = weights[i] < min_weight ? weights[i] : min_weight;
            }

            if (min_weight <= 0) {
                for (int i = 0; i < total_moves; ++i)
                    weights[i] -= min_weight - 1;
            }

            for (int i = 0; i < total_moves; ++i)
                weights[i] = std::pow(weights[i], 1/temp);
        };

        for (int i = 0; i < n_moves; ++i) {
            ExtMove *end = generate<LEGAL>(board_, moves);
            total_moves = int(end - moves);
            assign_weights();
            std::discrete_distribution<unsigned> dist(
                    weights, weights+total_moves);

            Move m = moves[dist(rng)];
            stack_.push(board_.key(), m);
            board_ = board_.do_move(m, &si_stack_[++ply]);
        }

        return ply;
    }

    int choose_pv(int max_diff) const {
        auto rng = get_thread_local_rng();

        int candidate_ics[MAX_MOVES] = { 0 };
        int n_candidates = 1;
        const int best_score = search_.get_pv_start(0).score;

        for (int i = 1; i < search_.num_pvs(); ++i)
            if (abs(search_.get_pv_start(i).score - best_score) <= max_diff)
                candidate_ics[n_candidates++] = i;
        std::uniform_int_distribution<> dist(0, n_candidates-1);
        return candidate_ics[dist(rng)];
    }

    Search search_;

    Board board_;
    StateInfo si_stack_[MAX_PLIES];
    Stack stack_;
    SearchLimits limits_;
    int num_pvs_;
    int max_ld_moves_;

    PosQueue &q_;
    std::atomic_bool keep_going_;
    std::thread th_;
};

void selfplay(const char *out_name, int num_pos, int nodes, 
        int n_pvs, int max_ld_moves, int n_threads) 
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s.bin", out_name);

    std::ofstream fout(buf, std::ios::binary);
    if (!fout) {
        printf("[ERROR] selfplay: could not create bin file %s\n", buf);
        return;
    }
    ChainWriter writer(fout);

    SearchLimits limits;
    limits.nodes = nodes;
    limits.type = limits.NODES;

    PosQueue q;

    std::list<Session> sessions;
    for (int i = 0; i < n_threads; ++i)
        sessions.emplace_back(q, limits, n_pvs, max_ld_moves);

    TimePoint start = timer::now();

    int outcome_int[3] = { 1, -1, 0 };
    int pos_cnt = 0;

    while (pos_cnt < num_pos) {
        Entry e = q.pop();
        writer.write(e.pc);
        pos_cnt += e.pc.n_moves;

        auto delta = std::max<TimePoint>(1, timer::now() - start);
        int pos_per_sec = int(pos_cnt * 1000ll / delta);
        float eta = float(num_pos - pos_cnt) / pos_per_sec;

        printf("[%d / %d] % 2d % 6d %d pos/s ", pos_cnt, num_pos,
                outcome_int[e.pc.result], e.pc.seq[e.pc.n_moves-1].score, 
                pos_per_sec);
        if (eta > 3600.f)
            printf("%.2f eta hr ", eta / 3600);
        else if (eta > 60.f)
            printf("%.2f eta min ", eta / 60);
        else
            printf("%.2f eta sec ", eta);

        printf("%s\n", reason_to_str(e.reason));
    }

    for (Session &s: sessions)
        s.stop();
}

