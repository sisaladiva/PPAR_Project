#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <getopt.h>
#include <err.h>
#include <mpi.h>
#include <omp.h>
#include <string.h>

typedef uint64_t u64;       /* portable 64-bit integer */
typedef uint32_t u32;       /* portable 32-bit integer */
struct __attribute__ ((packed)) entry { u32 k; u64 v; };  /* hash table entry */

/***************************** global variables ******************************/

u64 n = 0;         /* block size (in bits) */
u64 mask;          /* this is 2**n - 1 */
int nres = 0;

u64 dict_size;     /* number of slots in the hash table */
struct entry *A;   /* the hash table */

/* (P, C) : two plaintext-ciphertext pairs */
u32 P[2][2] = {{0, 0}, {0xffffffff, 0xffffffff}};
u32 C[2][2];

/************************ tools and utility functions *************************/

double wtime()
{
	struct timeval ts;
	gettimeofday(&ts, NULL);
	return (double)ts.tv_sec + ts.tv_usec / 1E6;
}

// murmur64 hash functions, tailorized for 64-bit ints / Cf. Daniel Lemire
u64 murmur64(u64 x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;
    return x;
}

/* represent n in 4 bytes */
void human_format(u64 n, char *target)
{
    if (n < 1000) {
        sprintf(target, "%" PRId64, n);
        return;
    }
    if (n < 1000000) {
        sprintf(target, "%.1fK", n / 1e3);
        return;
    }
    if (n < 1000000000) {
        sprintf(target, "%.1fM", n / 1e6);
        return;
    }
    if (n < 1000000000000ll) {
        sprintf(target, "%.1fG", n / 1e9);
        return;
    }
    if (n < 1000000000000000ll) {
        sprintf(target, "%.1fT", n / 1e12);
        return;
    }
}

/******************************** SPECK block cipher **************************/

#define ROTL32(x,r) (((x)<<(r)) | (x>>(32-(r))))
#define ROTR32(x,r) (((x)>>(r)) | ((x)<<(32-(r))))

#define ER32(x,y,k) (x=ROTR32(x,8), x+=y, x^=k, y=ROTL32(y,3), y^=x)
#define DR32(x,y,k) (y^=x, y=ROTR32(y,3), x^=k, x-=y, x=ROTL32(x,8))

void Speck64128KeySchedule(const u32 K[],u32 rk[])
{
    u32 i,D=K[3],C=K[2],B=K[1],A=K[0];
    for(i=0;i<27;){
        rk[i]=A; ER32(B,A,i++);
        rk[i]=A; ER32(C,A,i++);
        rk[i]=A; ER32(D,A,i++);
    }
}

void Speck64128Encrypt(const u32 Pt[], u32 Ct[], const u32 rk[])
{
    u32 i;
    Ct[0]=Pt[0]; Ct[1]=Pt[1];
    for(i=0;i<27;)
        ER32(Ct[1],Ct[0],rk[i++]);
}

void Speck64128Decrypt(u32 Pt[], const u32 Ct[], u32 const rk[])
{
    int i;
    Pt[0]=Ct[0]; Pt[1]=Ct[1];
    for(i=26;i>=0;)
        DR32(Pt[1],Pt[0],rk[i--]);
}

/******************************** dictionary ********************************/

/*
 * "classic" hash table for 64-bit key-value pairs, with linear probing.  
 * It operates under the assumption that the keys are somewhat random 64-bit integers.
 * The keys are only stored modulo 2**32 - 5 (a prime number), and this can lead 
 * to some false positives.
 */
static const u32 EMPTY = 0xffffffff;
static const u64 PRIME = 0xfffffffb;

/* allocate a hash table with `size` slots (12*size bytes) */
void dict_setup(u64 size)
{
	dict_size = size;
	char hdsize[8];
	human_format(dict_size * sizeof(*A), hdsize);
	printf("Total dictionary size: %sB\n", hdsize);

	A = malloc(sizeof(*A) * dict_size);
	if (A == NULL)
		err(1, "impossible to allocate the dictionnary");
    #pragma omp parallel for
	for (u64 i = 0; i < dict_size; i++)
		A[i].k = EMPTY;
}

/* Insert the binding key |----> value in the dictionnary */
void dict_insert(u64 key, u64 value)
{
    u64 h = murmur64(key) % dict_size;
    for (;;) {
        if (A[h].k == EMPTY)
            break;
        h += 1;
        if (h == dict_size)
            h = 0;
    }
    assert(A[h].k == EMPTY);
    A[h].k = key % PRIME;
    A[h].v = value;
}


/* Query the dictionnary with this `key`.  Write values (potentially) 
 *  matching the key in `values` and return their number. The `values`
 *  array must be preallocated of size (at least) `maxval`.
 *  The function returns -1 if there are more than `maxval` results.
 */
int dict_probe(u64 key, int maxval, u64 values[])
{
    u32 k = key % PRIME;
    u64 h = murmur64(key) % dict_size;
    int nval = 0;
    for (;;) {
        if (A[h].k == EMPTY)
            return nval;
        if (A[h].k == k) {
        	if (nval == maxval)
        		return -1;
            values[nval] = A[h].v;
            nval += 1;
        }
        h += 1;
        if (h == dict_size)
            h = 0;
   	}
}
#include <math.h>

double log2(double x) {
    return log(x) / log(2);
}

/*
EN FONCTION DES BITS DE POIDS FAIBLES
int dict_probe_local(u64 key, int maxval, u64 values[], int rank, int num_processes) {

    int number_bits= log2(num_processes);
    u64 mask = (1ULL << number_bits) - 1;
    // On prend les 2 derniers bits pour déterminer le processus cible
    int target_process = (key & mask);  // Les 2 derniers bits de la clé

    // Si la clé appartient au processus courant (d'après ses 2 derniers bits), on la recherche localement
    if (target_process == rank) {
        return dict_probe(key, maxval, values);
    }
    return 0; // La clé ne correspond pas à ce processus
}*/

/***************************** MITM problem ***********************************/

/* f : {0, 1}^n --> {0, 1}^n.  Speck64-128 encryption of P[0], using k */
u64 f(u64 k)
{
    assert((k & mask) == k);
    u32 K[4] = {k & 0xffffffff, k >> 32, 0, 0};
    u32 rk[27];
    Speck64128KeySchedule(K, rk);
    u32 Ct[2];
    Speck64128Encrypt(P[0], Ct, rk);
    return ((u64) Ct[0] ^ ((u64) Ct[1] << 32)) & mask;
}


/* g : {0, 1}^n --> {0, 1}^n.  speck64-128 decryption of C[0], using k */
u64 g(u64 k)
{
    assert((k & mask) == k);
    u32 K[4] = {k & 0xffffffff, k >> 32, 0, 0};
    u32 rk[27];
    Speck64128KeySchedule(K, rk);
    u32 Pt[2];
    Speck64128Decrypt(Pt, C[0], rk);
    return ((u64) Pt[0] ^ ((u64) Pt[1] << 32)) & mask;
}

bool is_good_pair(u64 k1, u64 k2)
{
    u32 Ka[4] = {k1 & 0xffffffff, k1 >> 32, 0, 0};
    u32 Kb[4] = {k2 & 0xffffffff, k2 >> 32, 0, 0};
    u32 rka[27];
    u32 rkb[27];
    Speck64128KeySchedule(Ka, rka);
    Speck64128KeySchedule(Kb, rkb);
    u32 mid[2];
    u32 Ct[2];
    Speck64128Encrypt(P[1], mid, rka);
    Speck64128Encrypt(mid, Ct, rkb);
    return (Ct[0] == C[1][0]) && (Ct[1] == C[1][1]);
}



void dict_clear() {
    // Vérifie si la table de hachage existe
    if (A != NULL) {
        free(A); // Libère la mémoire allouée pour la table
        A = NULL; // Réinitialise le pointeur pour éviter les accès invalides
    }
    dict_size = 0; // Réinitialise la taille de la table
}



int golden_claw_search_mpi(int maxres, u64 **k1, u64 **k2, int rank, int num_processes) {
    double start = wtime();
    u64 N = 1ull << n;
    u64 local_dict_size = (1.125 * N) / (num_processes*2);  // Ajusté pour chaque processus
    
    // Check if the local dictionary size is sufficient
    if (local_dict_size < 2) {
        fprintf(stderr, "Dictionary size is too small for the number of processes\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        return 0;
    }

   // Calculate the number of bits needed for masking
    int number_bits = log2(num_processes);
    u64 mask = (1ULL << number_bits) - 1;

	
    char hdsize[8];
    human_format(local_dict_size * sizeof(*A), hdsize);
    printf("Process %d: Local dictionary size: %sB\n", rank, hdsize);

    // Initialize the number of local keys found
    int local_nkey = 0;

    // Allocate memory for k1 and k2 arrays to store results
    *k1 = malloc(maxres * sizeof(u64));
    *k2 = malloc(maxres * sizeof(u64));
    if (*k1 == NULL || *k2 == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        return 0;
    }

    // Initialize the allocated memory to zero
    memset(*k1, 0, maxres * sizeof(u64));
    memset(*k2, 0, maxres * sizeof(u64));

    // Loop over two parity values (0 and 1)
    for (int parity = 0; parity < 2; parity++) {

        // Set up the dictionary for the current process
        dict_setup(local_dict_size);

        #pragma omp parallel 
        {
            // Allocate local entries for each thread
            struct entry *local_entries = malloc(sizeof(struct entry) * local_dict_size / omp_get_num_threads());
            if (local_entries == NULL) {
                fprintf(stderr, "Memory allocation failed\n");
                MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
            }

            u64 local_count = 0;    // Counter for local entries

            // Initialize the first entry explicitly
            local_entries[local_count].k = 0; 
            local_entries[local_count].v = 0; 

            // Each thread processes a portion of the data
            #pragma omp for
            for (u64 x = 0; x < N; x += 1) {
                u64 z = f(x);

                // Check if the parity matches
                if (((z & 1) == parity) && ((x & mask) == rank)) {
                    if (local_count >= (local_dict_size / omp_get_num_threads())) {
                        fprintf(stderr, "Local entries array out of bounds\n");
                        free(local_entries);
                        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
                    }
                    local_entries[local_count].k = z % PRIME;
                    local_entries[local_count].v = x;
                    local_count++;
                }
            }

            #pragma omp critical
            {
                for (u64 i = 0; i < local_count; i++) {
                    dict_insert(local_entries[i].k, local_entries[i].v);
                }
            }

            free(local_entries);
        }

        u64 ncandidates = 0;

        #pragma omp parallel reduction(+:ncandidates)
        {
            
            u64 local_x[256];
            u64 local_k1[1024], local_k2[1024];
            int local_nres = 0;
memset(local_x, 0, sizeof(local_x));
memset(local_k1, 0, sizeof(local_k1));
memset(local_k2, 0, sizeof(local_k2));

            #pragma omp for schedule(dynamic, 1024)
            for (u64 z = 0; z < N; z++) {
                u64 y = g(z);
                if ((y & 1) == parity) {
                    int nx = dict_probe(y, 256, local_x);
                    ncandidates += nx;

                    for (int j = 0; j < nx; j++) {
                        if (is_good_pair(local_x[j], z)) {
                            if (local_nres < 1024) {
                                local_k1[local_nres] = local_x[j];
                                local_k2[local_nres] = z;
                                local_nres++;
                            }
                        }
                    }
                }
            }

            #pragma omp critical
            {
                for (int i = 0; i < local_nres && local_nkey < maxres; i++) {
                    (*k1)[local_nkey] = local_k1[i];
                    (*k2)[local_nkey] = local_k2[i];
                    local_nkey++;
                }
            }
        }

        dict_clear();
    }

    double end_time = wtime();
    printf("Process %d: Execution time: %.5f seconds\n", rank, end_time - start);

    return local_nkey;
}

 
/************************** command-line options ****************************/

void usage(char **argv)
{
        printf("%s [OPTIONS]\n\n", argv[0]);
        printf("Options:\n");
        printf("--n N                       block size [default 24]\n");
        printf("--C0 N                      1st ciphertext (in hex)\n");
        printf("--C1 N                      2nd ciphertext (in hex)\n");
        printf("\n");
        printf("All arguments are required\n");
        exit(0);
}

void process_command_line_options(int argc, char ** argv)
{
        struct option longopts[4] = {
                {"n", required_argument, NULL, 'n'},
                {"C0", required_argument, NULL, '0'},
                {"C1", required_argument, NULL, '1'},
                {NULL, 0, NULL, 0}
        };
        char ch;
        int set = 0;
        while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
                switch (ch) {
                case 'n':
                        n = atoi(optarg);
                        mask = (1ull << n) - 1;
                        break;
                case '0':
                        set |= 1;
                        u64 c0 = strtoull(optarg, NULL, 16);
                        C[0][0] = c0 & 0xffffffff;
                        C[0][1] = c0 >> 32;
                        break;
                case '1':
                        set |= 2;
                        u64 c1 = strtoull(optarg, NULL, 16);
                        C[1][0] = c1 & 0xffffffff;
                        C[1][1] = c1 >> 32;
                        break;
                default:
                        errx(1, "Unknown option\n");
                }
        }
        if (n == 0 || set != 3) {
        	usage(argv);
        	exit(1);
        }
}

/******************************************************************************/
int main(int argc, char **argv)
{
    int rank, num_processes;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_processes);

    process_command_line_options(argc, argv);
    if (rank == 0) {
        printf("Running with n=%d, C0=(%08x, %08x) and C1=(%08x, %08x)\n", 
            (int)n, C[0][0], C[0][1], C[1][0], C[1][1]);
    }

    u64 *k1 = NULL, *k2 = NULL;
    int local_nkey = 0;

    // Allouer et initialiser k1 et k2
    k1 = malloc(16 * sizeof(u64));  // ou une taille appropriée
    k2 = malloc(16 * sizeof(u64));  // ou une taille appropriée
    if (k1 == NULL || k2 == NULL) {
        fprintf(stderr, "Memory allocation failed for k1 or k2\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    memset(k1, 0, 16 * sizeof(u64));  // Initialisation à zéro
    memset(k2, 0, 16 * sizeof(u64));  // Initialisation à zéro

    local_nkey = golden_claw_search_mpi(16, &k1, &k2, rank, num_processes);

    int *all_nkeys = NULL;
    int *displacements = NULL;
    u64 *all_k1 = NULL, *all_k2 = NULL;
    int total_nkey = 0;

    if (rank == 0) {
        all_nkeys = malloc(num_processes * sizeof(int));
        displacements = malloc(num_processes * sizeof(int));
    }

    MPI_Gather(&local_nkey, 1, MPI_INT, all_nkeys, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        displacements[0] = 0;
        total_nkey = all_nkeys[0];
        for (int i = 1; i < num_processes; i++) {
            displacements[i] = displacements[i-1] + all_nkeys[i-1];
            total_nkey += all_nkeys[i];
        }
        
        // Allouer et initialiser all_k1 et all_k2
        all_k1 = malloc(total_nkey * sizeof(u64));
        all_k2 = malloc(total_nkey * sizeof(u64));
        if (all_k1 == NULL || all_k2 == NULL) {
            fprintf(stderr, "Memory allocation failed for all_k1 or all_k2\n");
            MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
        }
        memset(all_k1, 0, total_nkey * sizeof(u64));  // Initialisation à zéro
        memset(all_k2, 0, total_nkey * sizeof(u64));  // Initialisation à zéro
    }

    MPI_Gatherv(k1, local_nkey, MPI_UNSIGNED_LONG_LONG, all_k1, all_nkeys, displacements, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);
    MPI_Gatherv(k2, local_nkey, MPI_UNSIGNED_LONG_LONG, all_k2, all_nkeys, displacements, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Total solutions found: %d\n", total_nkey);
        for (int i = 0; i < total_nkey; i++) {
            assert(f(all_k1[i]) == g(all_k2[i]));
            assert(is_good_pair(all_k1[i], all_k2[i]));		
            printf("Solution found: (%" PRIx64 ", %" PRIx64 ") [checked OK]\n", all_k1[i], all_k2[i]);
        }

        free(all_nkeys);
        free(displacements);
        free(all_k1);
        free(all_k2);
    }

    free(k1);
    free(k2);

    MPI_Finalize();
    
    return 0;
}
