//------------------------------------------------------------------------------------------------
//! One row of the /leaderboard answer. Member names are the JSON keys, kept to one letter
//! because a page repeats them for every row: r rank, n name, k kills, d deaths, h HVT kills,
//! g HVT guard kills, o objective score, s score, t transport rating, i insertions, p players.
//------------------------------------------------------------------------------------------------
class IA_ApiBoardRow
{
	int r;
	string n;
	int k;
	int d;
	int h;
	int g;
	int o;
	int s;
	int t;
	int i;
	int p;
}
