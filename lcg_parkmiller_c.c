static unsigned random_seed = 0xdf2583ab;

#define RAND_MAX ((1u << 31u) - 1u)
unsigned lcg_parkmiller(unsigned *state)
{
	const unsigned N = 0x7fffffff;
	const unsigned G = 48271u;

	/*
		Indirectly compute state*G%N.

		Let:
			div = state/(N/G)
			rem = state%(N/G)

		Then:
			rem + div*(N/G) == state
			rem*G + div*(N/G)*G == state*G

		Now:
			div*(N/G)*G == div*(N - N%G) === -div*(N%G)  (mod N)

		Therefore:
			rem*G - div*(N%G) === state*G  (mod N)

		Add N if necessary so that the result is between 1 and N-1.
	*/
	unsigned div = *state / (N / G);  /* max : 2,147,483,646 / 44,488 = 48,271 */
	unsigned rem = *state % (N / G);  /* max : 2,147,483,646 % 44,488 = 44,487 */

	unsigned a = rem * G;        /* max : 44,487 * 48,271 = 2,147,431,977 */
	unsigned b = div * (N % G);  /* max : 48,271 * 3,399 = 164,073,129 */

	return *state = (a > b) ? (a - b) : (a + (N - b));
}

unsigned next_random() {
	return lcg_parkmiller(&random_seed);
}

//https://stackoverflow.com/questions/2509679/how-to-generate-a-random-integer-number-from-within-a-range#6852396
// Assumes 0 <= max <= RAND_MAX
// Returns in the closed interval [0, max]
unsigned random_at_most(unsigned max) {
	unsigned long
		// max <= RAND_MAX < ULONG_MAX, so this is okay.
		num_bins = (unsigned long) max + 1,
		num_rand = (unsigned long) RAND_MAX + 1,
		bin_size = num_rand / num_bins,
		defect   = num_rand % num_bins;

	unsigned x;
	do {
	x = next_random();
	}
	// This is carefully written not to overflow
	while (num_rand - defect <= (unsigned long)x);

	// Truncated division is intentional
	return (unsigned) x/bin_size;
}