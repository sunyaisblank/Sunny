/**
 * @file NRPL001A.cpp
 * @brief Neo-Riemannian transformations implementation
 *
 * Component: NRPL001A
 */

#include "NRPL001A.h"

#include <algorithm>
#include <queue>

namespace Sunny::Core {

Result<Triad> make_triad(PitchClass root, TriadQuality quality) {
    if (root >= 12) {
        return std::unexpected(ErrorCode::InvalidTriad);
    }
    return Triad{root, quality};
}

std::array<PitchClass, 3> triad_pitch_classes(const Triad& t) {
    if (t.quality == TriadQuality::Major) {
        return {t.root, transpose(t.root, 4), transpose(t.root, 7)};
    }
    return {t.root, transpose(t.root, 3), transpose(t.root, 7)};
}

Result<Triad> triad_from_pitch_classes(PitchClass a, PitchClass b, PitchClass c) {
    std::array<PitchClass, 3> pcs = {
        static_cast<PitchClass>(a % 12),
        static_cast<PitchClass>(b % 12),
        static_cast<PitchClass>(c % 12)
    };
    std::sort(pcs.begin(), pcs.end());

    // Try each pitch class as root
    for (auto root : pcs) {
        // Check major: root, root+4, root+7
        PitchClass third_maj = transpose(root, 4);
        PitchClass fifth_maj = transpose(root, 7);
        std::array<PitchClass, 3> maj = {root, third_maj, fifth_maj};
        std::sort(maj.begin(), maj.end());
        if (maj == pcs) {
            return Triad{root, TriadQuality::Major};
        }

        // Check minor: root, root+3, root+7
        PitchClass third_min = transpose(root, 3);
        PitchClass fifth_min = transpose(root, 7);
        std::array<PitchClass, 3> min = {root, third_min, fifth_min};
        std::sort(min.begin(), min.end());
        if (min == pcs) {
            return Triad{root, TriadQuality::Minor};
        }
    }

    return std::unexpected(ErrorCode::InvalidTriad);
}

Triad apply_plr(const Triad& t, NROperation op) {
    switch (op) {
        case NROperation::P:
            return Triad{
                t.root,
                t.quality == TriadQuality::Major ? TriadQuality::Minor : TriadQuality::Major
            };
        case NROperation::R:
            if (t.quality == TriadQuality::Major) {
                return Triad{transpose(t.root, 9), TriadQuality::Minor};
            }
            return Triad{transpose(t.root, 3), TriadQuality::Major};
        case NROperation::L:
            if (t.quality == TriadQuality::Major) {
                return Triad{transpose(t.root, 4), TriadQuality::Minor};
            }
            return Triad{transpose(t.root, 8), TriadQuality::Major};
    }
    return t;  // unreachable
}

Triad apply_plr_sequence(const Triad& t, std::span<const NROperation> ops) {
    Triad current = t;
    for (auto op : ops) {
        current = apply_plr(current, op);
    }
    return current;
}

namespace {

// Node index: root * 2 + (quality == Minor ? 1 : 0)
int triad_index(const Triad& t) {
    return static_cast<int>(t.root) * 2 + (t.quality == TriadQuality::Minor ? 1 : 0);
}

Triad triad_from_index(int idx) {
    return Triad{
        static_cast<PitchClass>(idx / 2),
        (idx % 2 == 1) ? TriadQuality::Minor : TriadQuality::Major
    };
}

}  // namespace

int plr_distance(const Triad& from, const Triad& to) {
    if (from == to) return 0;

    int start = triad_index(from);
    int goal = triad_index(to);

    std::array<int, 24> dist;
    dist.fill(-1);
    dist[start] = 0;

    std::queue<int> q;
    q.push(start);

    while (!q.empty()) {
        int cur = q.front();
        q.pop();

        Triad cur_triad = triad_from_index(cur);

        for (auto op : {NROperation::P, NROperation::R, NROperation::L}) {
            Triad next = apply_plr(cur_triad, op);
            int ni = triad_index(next);

            if (dist[ni] == -1) {
                dist[ni] = dist[cur] + 1;
                if (ni == goal) return dist[ni];
                q.push(ni);
            }
        }
    }

    return -1;  // unreachable on a connected graph
}

std::vector<NROperation> plr_path(const Triad& from, const Triad& to) {
    if (from == to) return {};

    int start = triad_index(from);
    int goal = triad_index(to);

    struct Entry {
        int parent;
        NROperation op;
    };

    std::array<int, 24> visited;
    visited.fill(-1);
    visited[start] = 0;

    std::array<Entry, 24> parent;
    parent[start] = {-1, NROperation::P};

    std::queue<int> q;
    q.push(start);

    while (!q.empty()) {
        int cur = q.front();
        q.pop();

        Triad cur_triad = triad_from_index(cur);

        for (auto op : {NROperation::P, NROperation::R, NROperation::L}) {
            Triad next = apply_plr(cur_triad, op);
            int ni = triad_index(next);

            if (visited[ni] == -1) {
                visited[ni] = visited[cur] + 1;
                parent[ni] = {cur, op};

                if (ni == goal) {
                    // Reconstruct path
                    std::vector<NROperation> path;
                    int node = goal;
                    while (node != start) {
                        path.push_back(parent[node].op);
                        node = parent[node].parent;
                    }
                    std::reverse(path.begin(), path.end());
                    return path;
                }

                q.push(ni);
            }
        }
    }

    return {};  // unreachable
}

int common_tone_count(const Triad& a, const Triad& b) {
    auto pcs_a = triad_pitch_classes(a);
    auto pcs_b = triad_pitch_classes(b);

    int count = 0;
    for (auto pa : pcs_a) {
        for (auto pb : pcs_b) {
            if (pa == pb) {
                ++count;
                break;
            }
        }
    }
    return count;
}

}  // namespace Sunny::Core
