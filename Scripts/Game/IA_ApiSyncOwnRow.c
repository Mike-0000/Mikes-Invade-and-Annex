//------------------------------------------------------------------------------------------------
//! One entry of "own" in a /sync answer, read by JsonLoadContext: where a player stands on a
//! board by score, or this server on the board of servers. A board row (IA_ApiBoardRow) with
//! the board and the player it belongs to. The member names are the JSON keys.
//------------------------------------------------------------------------------------------------
class IA_ApiSyncOwnRow
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
	string id;			// the player's identity; empty on the board of servers
	string board;		// server, global or servers
}
