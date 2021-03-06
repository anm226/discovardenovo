///////////////////////////////////////////////////////////////////////////////
//                   SOFTWARE COPYRIGHT NOTICE AGREEMENT                     //
//       This software and its documentation are copyright (2014) by the     //
//   Broad Institute.  All rights are reserved.  This software is supplied   //
//   without any warranty or guaranteed support whatsoever. The Broad        //
//   Institute is not responsible for its use, misuse, or functionality.     //
///////////////////////////////////////////////////////////////////////////////

// MakeDepend: library OMP
// MakeDepend: cflags OMP_FLAGS

#include "CoreTools.h"
#include "ParallelVecUtilities.h"
#include "Qualvector.h"
#include "paths/HyperBasevector.h"
#include "paths/long/ReadPath.h"
#include "paths/long/large/Clean200.h"
#include "paths/long/large/GapToyTools.h"

void Clean200( HyperBasevector& hb, vec<int>& inv, ReadPathVec& paths,
     const vecbasevector& bases, const VecPQVec& quals, const int verbosity,
     const int version, const Bool REMOVE_TINY )
{
     // Start.

     double clock = WallClockTime( );

     // Heuristics.

     const int max_exts = 10;
     const int npasses = 2;

     // Run two passes.

     for ( int zpass = 1; zpass <= npasses; zpass++ )
     {
     
     // ---->

     // Set up.

     vec<int> to_right;
     hb.ToRight(to_right);
     HyperBasevectorX hbx(hb);
     VecULongVec paths_index;
     invert( paths, paths_index, hb.EdgeObjectCount( ) );

     // Look for weak branches.

     cout << Date( ) << ": start walking" << endl;
     cout << "memory in use = " << ToStringAddCommas( MemUsageBytes( ) ) << endl;
     const int max_rl = 250;
     vec<int> to_delete;
     #pragma omp parallel for
     for ( int e = 0; e < hb.EdgeObjectCount( ); e++ )
     {    int v = to_right[e];
          if ( !hb.To(v).solo( ) || hb.From(v).size( ) <= 1 ) continue;

          // Find extensions of e.

          int n = hb.From(v).size( );
          int depth = max_rl;
          vec< vec<int> > exts;
          GetExtensions( hbx, v, max_exts, exts, depth );
          if ( exts.isize( ) > max_exts ) continue;
          int N = exts.size( );
          vec<int> ei(N);
          for ( int i = 0; i < N; i++ )
          for ( int j = 0; j < n; j++ )
               if ( exts[i][0] == hb.IFrom( v, j ) ) ei[i] = j;

          // Convert to basevectors.

          vec<basevector> bexts, rbexts;
          for ( int i = 0; i < N; i++ )
          {    const vec<int>& x = exts[i];
               basevector b = hb.Cat(x);
               bexts.push_back(b);
               b.ReverseComplement( );
               rbexts.push_back(b);    }

          // Score each read path containing e or a successor of it.

          vec<vec<int>> scores(n);
          vec< pair<int64_t,int> > pi; // {(id,start)}
          for ( int i = 0; i < (int) paths_index[e].size( ); i++ )
          {    int64_t id = paths_index[e][i];
               const ReadPath& p = paths[id];
               for ( int j = 0; j < (int) p.size( ); j++ )
               {    if ( p[j] == e ) 
                    {    int start = p.getOffset( );
                         for ( int l = 0; l <= j; l++ )
                              start -= hb.Kmers( p[l] );
                         pi.push( id, start );    }    }    }
          for ( int m = 0; m < n; m++ )
          {    int ep = hb.IFrom( v, m );
               for ( int i = 0; i < (int) paths_index[ep].size( ); i++ )
               {    int64_t id = paths_index[ep][i];
                    const ReadPath& p = paths[id];
                    for ( int j = 0; j < (int) p.size( ); j++ )
                    {    if ( p[j] == ep ) 
                         {    if ( j > 0 && p[j-1] == e ) continue;
                              int start = p.getOffset( );
                              for ( int l = 0; l < j; l++ )
                                   start -= hb.Kmers( p[l] );
                              pi.push( id, start );    }    }    }    }
          qvec qv;
          for ( int i = 0; i < pi.isize( ); i++ )
          {    int64_t id = pi[i].first; 
               int start = pi[i].second;
               quals[id].unpack(&qv);
               const ReadPath& p = paths[id];
               vec<int> q( N, 0 );
               for ( int pos = 0; pos < depth + hb.K( ) - 1; pos++ )
               {    int rpos = pos - start;
                    if ( rpos < 0 || rpos >= bases[id].isize( ) ) continue;
                    for ( int l = 0; l < N; l++ )
                    {    if ( bexts[l][pos] != bases[id][rpos] )
                              q[l] += qv[rpos];    }    }
               vec<int> qq( n, 1000000000 );
               for ( int l = 0; l < N; l++ )
                    qq[ ei[l] ] = Min( qq[ ei[l] ], q[l] );
               vec<int> idx( n, vec<int>::IDENTITY );
               SortSync( qq, idx );
               if ( qq[0] < qq[1] ) scores[ idx[0] ].push_back( qq[1] - qq[0] );    }

          // Score each read path containing rc of e or its successor.

          vec< pair<int64_t,int> > rpi; // {(id, start of read rel edge re)}
          int re = inv[e];
          for ( int i = 0; i < (int) paths_index[re].size( ); i++ )
          {    int64_t id = paths_index[re][i];
               const ReadPath& p = paths[id];
               for ( int j = 0; j < (int) p.size( ); j++ )
               {    if ( p[j] == re ) 
                    {    int start = p.getOffset( );
                         for ( int l = 0; l < j; l++ )
                              start -= hb.Kmers( p[l] );
                         rpi.push( id, start );    }    }    }
          for ( int m = 0; m < n; m++ )
          {    int rep = inv[ hb.IFrom( v, m ) ];
               for ( int i = 0; i < (int) paths_index[rep].size( ); i++ )
               {    int64_t id = paths_index[rep][i];
                    const ReadPath& p = paths[id];
                    for ( int j = 0; j < (int) p.size( ); j++ )
                    {    if ( p[j] == rep ) 
                         {    if ( j < (int) p.size( ) - 1 && p[j+1] == re ) 
                                   continue;
                              int start = p.getOffset( );
                              for ( int l = 0; l <= j; l++ )
                                   start -= hb.Kmers( p[l] );
                              rpi.push( id, start );    }    }    }    }
          for ( int i = 0; i < rpi.isize( ); i++ )
          {    int64_t id = rpi[i].first; 
               int start = rpi[i].second;
               quals[id].unpack(&qv);
               const ReadPath& p = paths[id];
               vec<int> q( N, 0 );
               for ( int pos = 0; pos < depth + hb.K( ) - 1; pos++ )
               {    int rpos = hb.K( ) - 2 - pos - start;
                    if ( rpos < 0 || rpos >= bases[id].isize( ) ) continue;
                    for ( int l = 0; l < N; l++ )
                    {    int s = bexts[l].size( );
                         if ( rbexts[l][s-pos-1] != bases[id][rpos] )
                              q[l] += qv[rpos];    }    }
               vec<int> qq( n, 1000000000 );
               for ( int l = 0; l < N; l++ )
                    qq[ ei[l] ] = Min( qq[ ei[l] ], q[l] );
               vec<int> idx( n, vec<int>::IDENTITY );
               SortSync( qq, idx );
               if ( qq[0] < qq[1] ) scores[ idx[0] ].push_back( qq[1] - qq[0] );    }

          // Analyze scores.

          for ( int j = 0; j < n; j++ )
               ReverseSort( scores[j] );
          AnalyzeScores( hbx, inv, v, scores, to_delete, zpass, verbosity,
               version );    }

     // Remove tiny standalone edges

     if (REMOVE_TINY)
     {    for ( int v = 0; v < hb.N( ); v++ )
          {    if ( hb.To(v).nonempty( ) ) continue;
               if ( !hb.From(v).solo( ) ) continue;
               int w = hb.From(v)[0];
               if ( v == w ) continue;
               if ( !hb.To(w).solo( ) ) continue;
               if ( hb.From(w).nonempty( ) ) continue;
               int e = hb.IFrom( v, 0 );
               if ( hb.EdgeLengthKmers(e) > 50 ) continue;
               to_delete.push_back(e);    }    }

     // Clean up.

     hb.DeleteEdges(to_delete);
     Cleanup( hb, inv, paths );    }
     TestInvolution( hb, inv );
     Validate( hb, inv, paths );    
     cout << TimeSince(clock) << " used cleaning 200-mer graph" << endl;    }

void Clean200x( HyperBasevector& hb, vec<int>& inv, ReadPathVec& paths,
     const vecbasevector& bases, const VecPQVec& quals, const int verbosity,
     const int version, const Bool REMOVE_TINY )
{
     // Start.

     double clock = WallClockTime( );

     // Heuristics.

     const int max_exts = 10;
     const int npasses = 2;

     // Run two passes.

     for ( int zpass = 1; zpass <= npasses; zpass++ )
     {
     
     // ---->

     // Set up.

     vec<int> to_right;
     hb.ToRight(to_right);
     HyperBasevectorX hbx(hb);
     VecULongVec paths_index;
     invert( paths, paths_index, hb.EdgeObjectCount( ) );

     // Look for weak branches.

     cout << Date( ) << ": start walking" << endl;
     cout << "memory in use = " << ToStringAddCommas( MemUsageBytes( ) ) << endl;
     const int max_rl = 250;
     vec<int> to_delete;
     #pragma omp parallel for
     for ( int v = 0; v < hb.N( ); v++ )
     {    if ( hb.To(v).empty( ) || hb.From(v).size( ) <= 1 ) continue;

          // Find extensions of e.

          int n = hb.From(v).size( );
          int depth = max_rl;
          vec< vec<int> > exts;
          GetExtensions( hbx, v, max_exts, exts, depth );
          if ( exts.isize( ) > max_exts ) continue;
          int N = exts.size( );
          vec<int> ei(N);
          for ( int i = 0; i < N; i++ )
          for ( int j = 0; j < n; j++ )
               if ( exts[i][0] == hb.IFrom( v, j ) ) ei[i] = j;

          // Convert to basevectors.

          vec<basevector> bexts, rbexts;
          for ( int i = 0; i < N; i++ )
          {    const vec<int>& x = exts[i];
               basevector b = hb.Cat(x);
               bexts.push_back(b);
               b.ReverseComplement( );
               rbexts.push_back(b);    }

          // Score each read path containing a predecessor or successor of e.

          vec<vec<int>> scores(n);
          vec< pair<int64_t,int> > pi; // {(id,start)}
          for ( int u = 0; u < hb.To(v).isize( ); u++ )
          {    int e = hb.ITo( v, u );
               for ( int i = 0; i < (int) paths_index[e].size( ); i++ )
               {    int64_t id = paths_index[e][i];
                    const ReadPath& p = paths[id];
                    for ( int j = 0; j < (int) p.size( ); j++ )
                    {    if ( p[j] == e ) 
                         {    int start = p.getOffset( );
                              for ( int l = 0; l <= j; l++ )
                                   start -= hb.Kmers( p[l] );
                              pi.push( id, start );    }    }    }    }
          for ( int m = 0; m < n; m++ )
          {    int ep = hb.IFrom( v, m );
               for ( int i = 0; i < (int) paths_index[ep].size( ); i++ )
               {    int64_t id = paths_index[ep][i];
                    const ReadPath& p = paths[id];
                    for ( int j = 0; j < (int) p.size( ); j++ )
                    {    if ( p[j] == ep ) 
                         {    if ( j > 0 && Member( hb.ToEdgeObj(v), p[j-1] ) )
                                   continue;
                              int start = p.getOffset( );
                              for ( int l = 0; l < j; l++ )
                                   start -= hb.Kmers( p[l] );
                              pi.push( id, start );    }    }    }    }
          qvec qv;
          for ( int i = 0; i < pi.isize( ); i++ )
          {    int64_t id = pi[i].first; 
               int start = pi[i].second;
               quals[id].unpack(&qv);
               const ReadPath& p = paths[id];
               vec<int> q( N, 0 );
               for ( int pos = 0; pos < depth + hb.K( ) - 1; pos++ )
               {    int rpos = pos - start;
                    if ( rpos < 0 || rpos >= bases[id].isize( ) ) continue;
                    for ( int l = 0; l < N; l++ )
                    {    if ( bexts[l][pos] != bases[id][rpos] )
                              q[l] += qv[rpos];    }    }
               vec<int> qq( n, 1000000000 );
               for ( int l = 0; l < N; l++ )
                    qq[ ei[l] ] = Min( qq[ ei[l] ], q[l] );
               vec<int> idx( n, vec<int>::IDENTITY );
               SortSync( qq, idx );
               if ( qq[0] < qq[1] ) scores[ idx[0] ].push_back( qq[1] - qq[0] );    }

          // Score each read path containing rc of e or its successor.

          vec< pair<int64_t,int> > rpi; // {(id, start of read rel edge re)}
          vec<int> res;
          for ( int u = 0; u < hb.To(v).isize( ); u++ )
          {    int e = hb.ITo( v, u );
               int re = inv[e];
               res.push_back(re);
               for ( int i = 0; i < (int) paths_index[re].size( ); i++ )
               {    int64_t id = paths_index[re][i];
                    const ReadPath& p = paths[id];
                    for ( int j = 0; j < (int) p.size( ); j++ )
                    {    if ( p[j] == re ) 
                         {    int start = p.getOffset( );
                              for ( int l = 0; l < j; l++ )
                                   start -= hb.Kmers( p[l] );
                              rpi.push( id, start );    }    }    }    }
          for ( int m = 0; m < n; m++ )
          {    int rep = inv[ hb.IFrom( v, m ) ];
               for ( int i = 0; i < (int) paths_index[rep].size( ); i++ )
               {    int64_t id = paths_index[rep][i];
                    const ReadPath& p = paths[id];
                    for ( int j = 0; j < (int) p.size( ); j++ )
                    {    if ( p[j] == rep ) 
                         {    if ( j < (int) p.size( ) - 1 && Member( res, p[j+1] ) )
                                   continue;     
                              int start = p.getOffset( );
                              for ( int l = 0; l <= j; l++ )
                                   start -= hb.Kmers( p[l] );
                              rpi.push( id, start );    }    }    }    }
          for ( int i = 0; i < rpi.isize( ); i++ )
          {    int64_t id = rpi[i].first; 
               int start = rpi[i].second;
               quals[id].unpack(&qv);
               const ReadPath& p = paths[id];
               vec<int> q( N, 0 );
               for ( int pos = 0; pos < depth + hb.K( ) - 1; pos++ )
               {    int rpos = hb.K( ) - 2 - pos - start;
                    if ( rpos < 0 || rpos >= bases[id].isize( ) ) continue;
                    for ( int l = 0; l < N; l++ )
                    {    int s = bexts[l].size( );
                         if ( rbexts[l][s-pos-1] != bases[id][rpos] )
                              q[l] += qv[rpos];    }    }
               vec<int> qq( n, 1000000000 );
               for ( int l = 0; l < N; l++ )
                    qq[ ei[l] ] = Min( qq[ ei[l] ], q[l] );
               vec<int> idx( n, vec<int>::IDENTITY );
               SortSync( qq, idx );
               if ( qq[0] < qq[1] ) scores[ idx[0] ].push_back( qq[1] - qq[0] );    }

          // Analyze scores.

          for ( int j = 0; j < n; j++ )
               ReverseSort( scores[j] );
          AnalyzeScores( hbx, inv, v, scores, to_delete, zpass, verbosity,
               version );    }

     // Remove tiny standalone edges

     if (REMOVE_TINY)
     {    for ( int v = 0; v < hb.N( ); v++ )
          {    if ( hb.To(v).nonempty( ) ) continue;
               if ( !hb.From(v).solo( ) ) continue;
               int w = hb.From(v)[0];
               if ( v == w ) continue;
               if ( !hb.To(w).solo( ) ) continue;
               if ( hb.From(w).nonempty( ) ) continue;
               int e = hb.IFrom( v, 0 );
               if ( hb.EdgeLengthKmers(e) > 50 ) continue;
               to_delete.push_back(e);    }    }

     // Clean up.

     hb.DeleteEdges(to_delete);
     Cleanup( hb, inv, paths );    }
     TestInvolution( hb, inv );
     Validate( hb, inv, paths );    
     cout << TimeSince(clock) << " used cleaning 200-mer graph" << endl;    }

void AnalyzeScores( const HyperBasevectorX& hb, const vec<int>& inv, const int v,
     const vec<vec<int>>& scores, vec<int>& to_delete, const int zpass, 
     const int verbosity, const int version )
{
     // Heuristics.

     const int max_del = 15;
     const int min_win = 100;
     const int max_lose = 50;
     const int min_ratio = 5;

     // Analyze scores.

     int n = hb.From(v).size( );
     for ( int d = 0; d <= max_del; d++ )
     {    vec<int> qsum(n);
          for ( int j = 0; j < n; j++ )
          {    for ( int i = 0; i < scores[j].isize( ); i++ )
               {    if ( scores[j][i] <= d ) break;
                    qsum[j] += scores[j][i];    }    }
          vec<int> ids( n, vec<int>::IDENTITY );
          ReverseSortSync( qsum, ids );
          Bool done = False;
          int lx = ( version == 1 ? 1 : n - 1 );
          for ( int r = 1; r <= lx; r++ )
          {    if ( qsum[0] >= min_win && qsum[r] <= max_lose
                    && qsum[0] >= min_ratio * qsum[r] )
               {    done = True;
                    #pragma omp critical
                    {    for ( int j = r; j < n; j++ )
                         {    int e2 = hb.IFrom( v, ids[j] );
                              to_delete.push_back( e2, inv[e2] );    }
                         if ( verbosity >= 1 )
                         {    cout << "\n--> ";
                              for ( int j = 0; j < n; j++ )
                              {    int e2 = hb.IFrom( v, ids[j] );
                                   if ( j > 0 ) cout << ",";
                                   cout << e2;    }
                              cout << "\n";
                              for ( int j = 0; j < n; j++ )
                              {    cout << "e" << j+1 << ": ";
                                   cout << printSeq( scores[ ids[j] ] ) << endl;    }
                              cout << "deleting e" << r+1;
                              if ( n > r+1 ) cout << "-" << n << endl;
                              cout << endl;    }
                         if ( verbosity >= 2 )
                         {    for ( int j = r; j < n; j++ )
                              {    int e2 = hb.IFrom( v, ids[j] );
                                   hb.EdgeObject(e2).Print( cout,
                                       ToString(zpass) + "." + ToString(e2) );
                                   cout << "-----" << endl;    }    }    }    }
               if (done) break;    }
          if (done) break;    }    }

void GetExtensions( const HyperBasevectorX& hb, const int v,
     const int max_exts, vec<vec<int>>& exts, int& depth )
{    int n = hb.From(v).size( );
     exts.clear( );
     for ( int pass = 1; pass <= 2; pass++ )
     {    exts.clear( );
          for ( int j = 0; j < n; j++ )
          {    vec<int> x = { hb.IFrom( v, j ) };
               exts.push_back(x);    }
          for ( int i = 0; i < exts.isize( ); i++ )
          {    if ( i >= max_exts ) break;
               int len = 0;
               for ( int l = 0; l < exts[i].isize( ); l++ )
                    len += hb.Kmers( exts[i][l] );
               if ( len >= depth ) continue;
               int w = hb.ToRight( exts[i].back( ) );
               if ( hb.From(w).empty( ) ) 
               {    depth = Min( depth, len );
                    continue;    }
               vec<int> p = exts[i];
               for ( int m = 0; m < (int) hb.From(w).size( ); m++ )
               {    vec<int> q(p);
                    q.push_back( hb.IFrom( w, m ) );
                    if ( m == 0 ) exts[i] = q;
                    else exts.push_back(q);    }
               i--;    }    }    }
