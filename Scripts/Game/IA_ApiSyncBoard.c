//------------------------------------------------------------------------------------------------
//! One entry of "boards" in a /sync answer, read by JsonLoadContext: what the stats service
//! holds of a board the exchange asked about. The member names are the JSON keys.
//------------------------------------------------------------------------------------------------
class IA_ApiSyncBoard
{
	string board;		// server, global or servers
	string etag;		// names the rows the stats service holds now
	int total;			// rows on the whole board
	int unchanged;		// 1 when the rows are those of the etag that was sent, and so did not come again
}
