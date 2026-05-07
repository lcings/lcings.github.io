// solver_a1_a2_mt.c
// gcc -O3 -march=native -pthread solver_a1_a2_mt.c -lcrypto -o solver_a1_a2_mt

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

#define INF_X UINT64_MAX
#define INF_Y UINT64_MAX

typedef struct {
    uint64_t x, y;
} Point;
typedef struct {
    uint64_t X, Y, Z;
} JPoint;  // Z=0 => INF

static const uint64_t P_MOD = (uint64_t)-3161559082938421043LL;
static const uint64_t A_CUR = (uint64_t)3964747079513330392ULL;
static const Point G1 = {0xB94A23029F24FB46ULL, 0x04E43124925A6949ULL};
static const Point G2 = {(uint64_t)-8496446684795956753LL, (uint64_t)5705694599655766268ULL};
static const Point INF = {INF_X, INF_Y};

static const char *B58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/* ---------- global state ---------- */
static atomic_int g_found = 0;
static atomic_uint g_s_found = 0;
static atomic_ullong g_attempts = 0;

typedef struct {
    int tid;
    int nthreads;
    int ncpu;
    uint64_t rounds;
    uint8_t d16[16];
    uint8_t d4[4];
    uint64_t k;
    Point Pk;         // k*G1
    Point stepPoint;  // nthreads*G2
} Task;

/* ---------- utils ---------- */
static inline void u64le(uint8_t *o, uint64_t v)
{
    for (int i = 0; i < 8; i++)
        o[i] = (uint8_t)(v >> (8 * i));
}
static inline void u32le(uint8_t *o, uint32_t v)
{
    o[0] = v;
    o[1] = v >> 8;
    o[2] = v >> 16;
    o[3] = v >> 24;
}
static inline uint32_t rd32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint64_t rd64le(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= ((uint64_t)p[i] << (8 * i));
    return v;
}

static void hexprint(const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; i++)
        printf("%02x", p[i]);
}

/* ---------- mod ---------- */
static inline uint64_t add_mod(uint64_t a, uint64_t b)
{
    __uint128_t t = (__uint128_t)a + b;
    return (uint64_t)(t % P_MOD);
}
static inline uint64_t sub_mod(uint64_t a, uint64_t b)
{
    __uint128_t t = (__uint128_t)a + P_MOD - b;
    return (uint64_t)(t % P_MOD);
}
static inline uint64_t mul_mod(uint64_t a, uint64_t b)
{
    __uint128_t t = (__uint128_t)a * b;
    return (uint64_t)(t % P_MOD);
}

static int inv_mod_u64(uint64_t a, uint64_t mod, uint64_t *out)
{
    uint64_t aa = a % mod;
    if (!aa)
        return 0;
    __int128 t = 0, newt = 1;
    uint64_t r = mod, newr = aa;
    while (newr) {
        uint64_t q = r / newr;
        __int128 tt = t - (__int128)q * newt;
        t = newt;
        newt = tt;
        uint64_t rr = (uint64_t)((__uint128_t)r - (__uint128_t)q * newr);
        r = newr;
        newr = rr;
    }
    if (r != 1)
        return 0;
    __int128 mm = (__int128)mod, x = t % mm;
    if (x < 0)
        x += mm;
    *out = (uint64_t)x;
    return 1;
}

/* ---------- affine ecc ---------- */
static Point point_add(Point p1, Point p2)
{
    if (p2.x == INF_X && p2.y == INF_Y)
        return p1;
    if (p1.x == INF_X && p1.y == INF_Y)
        return p2;
    if (p1.x == p2.x && p1.y != p2.y)
        return INF;

    uint64_t lam = 0, inv = 0;
    if (p1.x == p2.x && p1.y == p2.y) {
        uint64_t num = add_mod(mul_mod(3, mul_mod(p1.x, p1.x)), A_CUR);
        uint64_t den = mul_mod(2, p1.y);
        if (!inv_mod_u64(den, P_MOD, &inv))
            return INF;
        lam = mul_mod(num, inv);
    } else {
        uint64_t num = sub_mod(p1.y, p2.y);
        uint64_t den = sub_mod(p1.x, p2.x);
        if (!inv_mod_u64(den, P_MOD, &inv))
            return INF;
        lam = mul_mod(num, inv);
    }

    uint64_t x3 = sub_mod(sub_mod(mul_mod(lam, lam), p1.x), p2.x);
    uint64_t t = add_mod(p1.y, mul_mod(lam, sub_mod(x3, p1.x)));
    uint64_t y3 = (t == 0) ? 0 : (P_MOD - t);
    Point r = {x3, y3};
    return r;
}

static Point point_mul(Point p, uint64_t n)
{
    Point r = INF;
    while (n) {
        if (n & 1)
            r = point_add(r, p);
        p = point_add(p, p);
        n >>= 1;
    }
    return r;
}

/* ---------- jacobian ---------- */
static inline int j_is_inf(JPoint p)
{
    return p.Z == 0;
}
static inline JPoint j_inf(void)
{
    JPoint r = {0, 1, 0};
    return r;
}
static inline JPoint j_from_aff(Point p)
{
    if (p.x == INF_X && p.y == INF_Y)
        return j_inf();
    JPoint r = {p.x, p.y, 1};
    return r;
}
static JPoint j_double(JPoint P)
{
    if (j_is_inf(P) || P.Y == 0)
        return j_inf();
    uint64_t XX = mul_mod(P.X, P.X), YY = mul_mod(P.Y, P.Y), YYYY = mul_mod(YY, YY);
    uint64_t ZZ = mul_mod(P.Z, P.Z), Z4 = mul_mod(ZZ, ZZ);
    uint64_t S = mul_mod(4, mul_mod(P.X, YY));
    uint64_t M = add_mod(mul_mod(3, XX), mul_mod(A_CUR, Z4));
    uint64_t X3 = sub_mod(mul_mod(M, M), mul_mod(2, S));
    uint64_t Y3 = sub_mod(mul_mod(M, sub_mod(S, X3)), mul_mod(8, YYYY));
    uint64_t Z3 = mul_mod(2, mul_mod(P.Y, P.Z));
    JPoint R = {X3, Y3, Z3};
    return R;
}
static JPoint j_add_mixed(JPoint P, Point Q)
{
    if (j_is_inf(P))
        return j_from_aff(Q);
    if (Q.x == INF_X && Q.y == INF_Y)
        return P;

    uint64_t Z1Z1 = mul_mod(P.Z, P.Z);
    uint64_t U2 = mul_mod(Q.x, Z1Z1);
    uint64_t S2 = mul_mod(Q.y, mul_mod(Z1Z1, P.Z));

    uint64_t H = sub_mod(U2, P.X);
    uint64_t R = sub_mod(S2, P.Y);

    if (H == 0) {
        if (R == 0)
            return j_double(P);
        return j_inf();
    }

    uint64_t HH = mul_mod(H, H), HHH = mul_mod(HH, H), V = mul_mod(P.X, HH);
    uint64_t X3 = sub_mod(sub_mod(mul_mod(R, R), HHH), mul_mod(2, V));
    uint64_t Y3 = sub_mod(mul_mod(R, sub_mod(V, X3)), mul_mod(P.Y, HHH));
    uint64_t Z3 = mul_mod(P.Z, H);
    JPoint T = {X3, Y3, Z3};
    return T;
}

static void batch_inv_u64(const uint64_t *z, uint64_t *zinv, int n)
{
    uint64_t *pref = (uint64_t *)malloc((size_t)n * sizeof(uint64_t));
    if (!pref)
        exit(2);
    pref[0] = z[0];
    for (int i = 1; i < n; i++)
        pref[i] = mul_mod(pref[i - 1], z[i]);
    uint64_t inv_all = 0;
    if (!inv_mod_u64(pref[n - 1], P_MOD, &inv_all)) {
        memset(zinv, 0, (size_t)n * sizeof(uint64_t));
        free(pref);
        return;
    }
    for (int i = n - 1; i >= 1; i--) {
        zinv[i] = mul_mod(inv_all, pref[i - 1]);
        inv_all = mul_mod(inv_all, z[i]);
    }
    zinv[0] = inv_all;
    free(pref);
}

/* ---------- base58 ---------- */
static int b58_index(char c)
{
    const char *p = strchr(B58, c);
    return p ? (int)(p - B58) : -1;
}

static char *base58_encode(const uint8_t *data, size_t len)
{
    size_t zeros = 0;
    while (zeros < len && data[zeros] == 0)
        zeros++;
    size_t size = len * 138 / 100 + 2;
    uint8_t *buf = (uint8_t *)calloc(size, 1);
    if (!buf)
        return NULL;
    size_t buflen = 1;
    for (size_t i = zeros; i < len; i++) {
        uint32_t carry = data[i];
        for (size_t j = 0; j < buflen; j++) {
            uint32_t v = (uint32_t)buf[j] * 256 + carry;
            buf[j] = (uint8_t)(v % 58);
            carry = v / 58;
        }
        while (carry) {
            buf[buflen++] = (uint8_t)(carry % 58);
            carry /= 58;
        }
    }
    size_t outlen = zeros + buflen;
    char *out = (char *)malloc(outlen + 1);
    if (!out) {
        free(buf);
        return NULL;
    }
    size_t k = 0;
    for (size_t i = 0; i < zeros; i++)
        out[k++] = '1';
    for (size_t i = 0; i < buflen; i++)
        out[k++] = B58[buf[buflen - 1 - i]];
    out[k] = '\0';
    free(buf);
    return out;
}

static int base58_decode(const char *s, uint8_t *out, size_t *outlen)
{
    size_t slen = strlen(s), zeros = 0;
    while (zeros < slen && s[zeros] == '1')
        zeros++;
    size_t size = slen * 733 / 1000 + 1;
    uint8_t *b256 = (uint8_t *)calloc(size, 1);
    if (!b256)
        return 0;

    for (size_t i = zeros; i < slen; i++) {
        int val = b58_index(s[i]);
        if (val < 0) {
            free(b256);
            return 0;
        }
        uint32_t carry = (uint32_t)val;
        for (long j = (long)size - 1; j >= 0; j--) {
            uint32_t x = (uint32_t)b256[j] * 58 + carry;
            b256[j] = (uint8_t)(x & 0xFF);
            carry = x >> 8;
        }
        if (carry) {
            free(b256);
            return 0;
        }
    }

    size_t i = 0;
    while (i < size && b256[i] == 0)
        i++;
    size_t need = zeros + (size - i);
    if (*outlen < need) {
        free(b256);
        return 0;
    }

    size_t k = 0;
    for (size_t z = 0; z < zeros; z++)
        out[k++] = 0;
    while (i < size)
        out[k++] = b256[i++];
    *outlen = k;
    free(b256);
    return 1;
}

/* ---------- check/test ---------- */
static int checkValid_raw32(const uint8_t raw[32])
{
    uint32_t s = rd32le(raw + 20);
    uint64_t k = rd64le(raw + 24);

    Point p_s = point_mul(G2, s);
    Point p_k = point_mul(G1, k);
    Point r = point_add(p_k, p_s);
    if (r.x == INF_X && r.y == INF_Y)
        return -1;

    uint8_t msg[36], md[16];
    memcpy(msg, raw, 20);
    u64le(msg + 20, r.x);
    u64le(msg + 28, r.y);
    MD5(msg, 36, md);

    return (memcmp(md, raw + 20, 4) == 0) ? 0 : -1;
}

static int checkValid_b58(const char *s, uint8_t out32[32])
{
    uint8_t tmp[64] = {0};
    size_t n = sizeof(tmp);
    if (!base58_decode(s, tmp, &n))
        return -1;
    if (n != 32)
        return -1;
    memcpy(out32, tmp, 32);
    return checkValid_raw32(out32);
}

static int test_func(const char *a1, const char *a2, int a3)
{
    uint8_t d1[32] = {0}, d2[32] = {0}, md[16];
    if (checkValid_b58(a1, d1) != 0)
        return 28673;
    if (checkValid_b58(a2, d2) != 0)
        return 28674;
    MD5(d1, 32, md);
    if (memcmp(md, d2, 16) != 0)
        return 28675;
    if (rd32le(d2 + 16) != (uint32_t)a3)
        return 28676;
    return 0;
}

/* ---------- worker ---------- */
static void bind_thread_to_cpu(int tid, int ncpu)
{
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(tid % ncpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#else
    (void)tid;
    (void)ncpu;
#endif
}

static void *worker(void *arg)
{
    Task *t = (Task *)arg;
    bind_thread_to_cpu(t->tid, t->ncpu);

    enum { BATCH = 256 };

    uint32_t s = (uint32_t)t->tid;
    Point p2 = point_mul(G2, s);
    Point start = point_add(t->Pk, p2);
    JPoint cur = j_from_aff(start);

    uint8_t msg[36], md[16];
    memcpy(msg, t->d16, 16);
    memcpy(msg + 16, t->d4, 4);

    JPoint jp[BATCH];
    uint32_t sv[BATCH];
    int idx[BATCH];
    uint64_t z[BATCH], zinv[BATCH];

    uint64_t local_attempts = 0, i = 0;

    while (i < t->rounds) {
        if ((i & 1023ULL) == 0 && atomic_load_explicit(&g_found, memory_order_relaxed))
            break;
        int n = (t->rounds - i >= BATCH) ? BATCH : (int)(t->rounds - i);

        for (int j = 0; j < n; j++) {
            jp[j] = cur;
            sv[j] = s;
            s += (uint32_t)t->nthreads;
            cur = j_add_mixed(cur, t->stepPoint);
        }

        int vc = 0;
        for (int j = 0; j < n; j++) {
            if (!j_is_inf(jp[j])) {
                idx[vc] = j;
                z[vc] = jp[j].Z;
                vc++;
            }
        }

        if (vc > 0) {
            batch_inv_u64(z, zinv, vc);
            for (int q = 0; q < vc; q++) {
                int j = idx[q];
                uint64_t zi = zinv[q];
                uint64_t z2 = mul_mod(zi, zi);
                uint64_t z3 = mul_mod(z2, zi);
                uint64_t ax = mul_mod(jp[j].X, z2);
                uint64_t ay = mul_mod(jp[j].Y, z3);

                u64le(msg + 20, ax);
                u64le(msg + 28, ay);
                MD5(msg, 36, md);

                if (rd32le(md) == sv[j]) {
                    if (!atomic_exchange_explicit(&g_found, 1, memory_order_relaxed))
                        atomic_store_explicit(&g_s_found, sv[j], memory_order_relaxed);
                    local_attempts += (uint64_t)(j + 1);
                    atomic_fetch_add_explicit(&g_attempts, local_attempts, memory_order_relaxed);
                    return NULL;
                }
            }
        }

        i += (uint64_t)n;
        local_attempts += (uint64_t)n;
    }

    atomic_fetch_add_explicit(&g_attempts, local_attempts, memory_order_relaxed);
    return NULL;
}

/* ---------- search one token ---------- */
static int search_token_mt(int nthreads, uint64_t rounds_per_thread, const uint8_t d16[16], const uint8_t d4[4],
                           uint8_t out_raw32[32], int verbose)
{
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    if (ncpu <= 0)
        ncpu = 1;

    pthread_t *ths = (pthread_t *)malloc(sizeof(pthread_t) * (size_t)nthreads);
    Task *tasks = (Task *)malloc(sizeof(Task) * (size_t)nthreads);
    if (!ths || !tasks) {
        free(ths);
        free(tasks);
        return 0;
    }

    uint64_t job = 0;
    time_t t0 = time(NULL), last = t0;
    unsigned long long last_attempts = atomic_load_explicit(&g_attempts, memory_order_relaxed);

    for (;;) {
        job++;
        atomic_store_explicit(&g_found, 0, memory_order_relaxed);

        uint8_t kb[8];
        if (RAND_bytes(kb, 8) != 1) {
            free(ths);
            free(tasks);
            return 0;
        }
        uint64_t k = rd64le(kb);
        Point Pk = point_mul(G1, k);
        Point step = point_mul(G2, (uint64_t)nthreads);

        for (int i = 0; i < nthreads; i++) {
            tasks[i].tid = i;
            tasks[i].nthreads = nthreads;
            tasks[i].ncpu = (int)ncpu;
            tasks[i].rounds = rounds_per_thread;
            memcpy(tasks[i].d16, d16, 16);
            memcpy(tasks[i].d4, d4, 4);
            tasks[i].k = k;
            tasks[i].Pk = Pk;
            tasks[i].stepPoint = step;
            pthread_create(&ths[i], NULL, worker, &tasks[i]);
        }
        for (int i = 0; i < nthreads; i++)
            pthread_join(ths[i], NULL);

        time_t now = time(NULL);
        if (verbose && now != last) {
            unsigned long long at = atomic_load_explicit(&g_attempts, memory_order_relaxed);
            double dt = (double)(now - last);
            double mps = (dt > 0) ? ((double)(at - last_attempts) / dt / 1e6) : 0.0;
            printf("[job=%llu] attempts=%llu speed=%.2f M/s elapsed=%llds\n", (unsigned long long)job, at, mps,
                   (long long)(now - t0));
            last_attempts = at;
            last = now;
        }

        if (atomic_load_explicit(&g_found, memory_order_relaxed)) {
            uint32_t s = atomic_load_explicit(&g_s_found, memory_order_relaxed);
            memcpy(out_raw32, d16, 16);
            memcpy(out_raw32 + 16, d4, 4);
            u32le(out_raw32 + 20, s);
            u64le(out_raw32 + 24, k);
            free(ths);
            free(tasks);
            return 1;
        }
    }
}

/* ---------- main ---------- */
int main(int argc, char **argv)
{
    uint32_t a3 = 131232;
    if (argc == 2) {
        printf("gen for windows\n");
        a3 = 132512;
    }

    int nthreads = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (nthreads <= 0)
        nthreads = 8;
    uint64_t rounds = (1ULL << 23);  // 默认每线程每job约4M

    printf("threads=%d rounds/thread/job=%llu\n", nthreads, (unsigned long long)rounds);

    /* ---- 1) 生成 a1 ---- */
    uint8_t d16_a1[16], d4_a1[4], raw1[32];
    if (RAND_bytes(d16_a1, 16) != 1 || RAND_bytes(d4_a1, 4) != 1) {
        fprintf(stderr, "RAND_bytes failed\n");
        return 1;
    }

    printf("[*] searching a1...\n");
    if (!search_token_mt(nthreads, rounds, d16_a1, d4_a1, raw1, 1)) {
        fprintf(stderr, "search a1 failed\n");
        return 1;
    }

    char *a1 = base58_encode(raw1, 32);
    if (!a1) {
        fprintf(stderr, "base58 a1 failed\n");
        return 1;
    }

    printf("[+] a1 raw = ");
    hexprint(raw1, 32);
    printf("\n");
    printf("[+] checkValid(a1) = %s\n", checkValid_raw32(raw1) == 0 ? "OK" : "FAIL");

    /* ---- 2) 生成 a2 ----
       条件：
       a2[0..15] = MD5(a1_raw)[0..15]
       a2[16..19] = a3 (LE)
    */
    uint8_t md[16], d16_a2[16], d4_a2[4], raw2[32];
    MD5(raw1, 32, md);
    memcpy(d16_a2, md, 16);
    u32le(d4_a2, a3);

    printf("[*] searching a2...\n");
    if (!search_token_mt(nthreads, rounds, d16_a2, d4_a2, raw2, 1)) {
        fprintf(stderr, "search a2 failed\n");
        free(a1);
        return 1;
    }

    char *a2 = base58_encode(raw2, 32);
    if (!a2) {
        fprintf(stderr, "base58 a2 failed\n");
        free(a1);
        return 1;
    }

    printf("[+] a2 raw = ");
    hexprint(raw2, 32);
    printf("\n");
    printf("[+] checkValid(a2) = %s\n", checkValid_raw32(raw2) == 0 ? "OK" : "FAIL");

    /* ---- 3) 最终 test ---- */
    int rc = test_func(a1, a2, (int)a3);
    printf("[+] test(a1,a2,%u) = %d\n", a3, rc);
    printf("[+] a1 = %s\n", a1);
    printf("[+] a2 = %s\n", a2);
    free(a1);
    free(a2);
    return 0;
}
